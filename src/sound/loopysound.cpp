/*
Casio Loopy sound implementation by kasami, 2023-2024.
Features a reverse-engineered uPD937 synth engine, MIDI retiming, EQ filtering and resampling.

This implementation is INCOMPLETE, but mostly sufficient for Loopy emulation running original game
software. It is missing playback of the internal demo tune (used by some games) and rhythm presets
(not used) as the formats are currently unknown, and the synth core also lacks some small details.

The code is messy and will probably stay that way until a more complete implementation (standalone
uPD937 library?) replaces it in the future. It was ported from a Java prototype, and may have some
inefficiencies and things that aren't structured well for C++.

Game support notes:
- PC Collection title screen goes a bit fast and some sounds get stuck (timing issue?)
- Wanwan has no PCM sample support, and seems to crackle on dialog sfx (same timing issue?)
*/

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include <sound/loopysound.h>

namespace LoopySound
{

UPD937_Core::UPD937_Core(std::vector<uint8_t>& rom_in, float synthesis_rate)
{
	// Pad ROM to a power of 2
	int rom_size = 1;
	while(rom_size < rom_in.size()) rom_size <<= 1;
	rom = std::make_unique<uint8_t[]>(rom_size);
	memcpy(rom.get(), rom_in.data(), rom_in.size());
	rom_mask = rom_size-1;

	// Set up global state
	ptr_partials = read_rom_16(0) * 32;
	ptr_pitchenv = read_rom_16(2) * 32;
	ptr_volenv   = read_rom_16(4) * 32;
	ptr_sampdesc = read_rom_16(6) * 32;
	ptr_demosong = read_rom_16(8) * 32;

	// Setup voice state
	for(int v = 0; v < 32; v++)
	{
		voices[v] = {};
	}

	// Setup channel state
	for(int c = 0; c < 4; c++)
	{
		channels[c] = {};
		prog_chg(c, 0);
	}
	set_channel_configuration(false, false);

	this->synthesis_rate = synthesis_rate;

	midi_running_status = 0;
	midi_param_count = 0;
	midi_in_sysex = false;

	volume_slider[0] = volume_slider[1] = 4;
}

void UPD937_Core::gen_sample(int out[])
{
	update_sample();
	for(int lr = 0; lr <= 1; lr++)
	{
		int accum = 0;
		for(int v = 0; v < 32; v += 2)
		{
			UPD937_VoiceState *vo = &voices[v+lr];
			UPD937_ChannelState *ch = &channels[vo->channel];
			if(vo->volume == 0) continue;
			if(ch->mute) continue;
			int s = vo->sample_last_val;
			int sb = (read_rom_16(vo->sample_ptr*2)>>4) - 0x800;
			int sd = ((sb - s) * vo->sample_fract) / 0x8000;
			s += sd;
			s = (s * vo->volume) / 65536;
			if(vo->channel > 0)
			{
				s = (s * VOLUME_SLIDER_LEVELS[volume_slider[vo->channel==3 ? 1 : 0]]) / 4096;
			}
			accum += s;
		}
		accum = std::clamp(accum, -32767, 32767);
		out[lr] = accum;
	}
}

void UPD937_Core::set_channel_configuration(bool multi, bool all)
{
	if(multi)
	{
		channels[0].first_voice = 2*0;
		channels[0].voice_count = 2*6;
		channels[1].first_voice = 2*6;
		channels[1].voice_count = 2*4;
		channels[2].first_voice = 2*10;
		channels[2].voice_count = 2*2;
		channels[3].first_voice = 2*12;
		channels[3].voice_count = 2*4;
		channels[0].midi_enabled = true;
		channels[1].midi_enabled = true;
		channels[2].midi_enabled = true;
		channels[3].midi_enabled = all;
	}
	else
	{
		channels[0].first_voice = 2*0;
		channels[0].voice_count = 2*12;
		channels[0].midi_enabled = true;
		channels[1].midi_enabled = false;
		channels[2].midi_enabled = false;
		channels[3].midi_enabled = false;
		channels[1].voice_count = 0;
		channels[2].voice_count = 0;
		channels[3].voice_count = 0;
	}
	for(int v = 0; v < 32; v++) voices[v].channel = 0;
	for(int c = 1; c < 4; c++)
	{
		for(int v = 0; v < channels[c].voice_count; v++)
		{
			voices[channels[c].first_voice+v].channel = c;
		}
	}
	// TODO reset state?
}

void UPD937_Core::set_volume_slider(int group, int slider)
{
	group = std::clamp(group, 0, 1);
	slider = std::clamp(slider, 0, 4);
	volume_slider[group] = slider;
}

void UPD937_Core::set_channel_muted(int channel, bool mute)
{
	channels[channel].mute = mute;
}

void UPD937_Core::reset_channels(bool clear_program)
{
	int p = clear_program ? 0 : 128;
	prog_chg(0, p);
	prog_chg(1, p);
	prog_chg(2, p);
	prog_chg(3, p);
}

void UPD937_Core::process_midi_now(char midi_byte)
{
	// This function must be called from the audio thread!
	// TODO: make thread safe?
	int m = midi_byte & 0xFF;
	if(m >= 0x80)
	{
		// Status byte
		if(m == 0xF0 && !midi_in_sysex) midi_in_sysex = true;
		if(m == 0xF7 && midi_in_sysex)
		{
			midi_in_sysex = false;
			// process sysex?
		}
		if(m < 0xF8)
		{
			midi_status = m;
			midi_running_status = (m < 0xF0) ? m : 0;
			midi_param_count = 0;
		}
	}
	else
	{
		if(midi_param_count >= sizeof(midi_param_bytes) || midi_status == 0) return;
		midi_param_bytes[midi_param_count++] = (char)(m&0x7F);
		if(midi_in_sysex) return;
		int status_hi = midi_status>>4;
		if(status_hi == 0xF)
		{
			// Process F0-F7 statuses
		}
		else
		{
			int channel = midi_status&0x0F;
			int message_size = (status_hi==0xC || status_hi==0xD) ? 1 : 2;
			if(midi_param_count >= message_size && !midi_in_sysex)
			{
				if(channels[channel].midi_enabled)
				{
					switch(status_hi)
					{
					case 0x8:
						note_off(channel, midi_param_bytes[0]);
						break;
					case 0x9:
						if(midi_param_bytes[1] > 0) note_on(channel, midi_param_bytes[0]);
						else note_off(channel, midi_param_bytes[0]);
						break;
					case 0xA:
						printf("[Sound] unhandled message KEY PRESSURE\n");
						break;
					case 0xB:
						if(midi_param_bytes[0] == 0x40)
						{
							control_chg_sustain(channel, (midi_param_bytes[1] >= 0x40));
						}
						else
						{
							printf("[Sound] unhandled message CONTROL CHANGE %02X %02X\n", midi_param_bytes[0], midi_param_bytes[1]);
						}
						break;
					case 0xC:
						prog_chg(channel, midi_param_bytes[0]);
						break;
					case 0xD:
						printf("[Sound] unhandled message CHANNEL PRESSURE\n");
						break;
					case 0xE:
						pitch_bend(channel, (midi_param_bytes[1]<<1) | (midi_param_bytes[1]>>6));
						break;
					case 0xF:
					default:
						break;
					}
				}
				midi_param_count = 0;
				midi_status = midi_running_status;
			}
		}
	}
}

int UPD937_Core::read_rom_8(int offset)
{
	return rom[offset&rom_mask]&0xFF;
}

int UPD937_Core::read_rom_16(int offset)
{
	return ((rom[(offset+1)&rom_mask]&0xFF)<<8)|(rom[offset&rom_mask]&0xFF);
}

int UPD937_Core::read_rom_24(int offset)
{
	return ((rom[(offset+2)&rom_mask]&0xFF)<<16)|((rom[(offset+1)&rom_mask]&0xFF)<<8)|(rom[offset&rom_mask]&0xFF);
}

void UPD937_Core::update_sample()
{
	// Clock the volume & pitch envelope generators
	if((sample_count%384) == 0) update_volume_envelopes();
	int clk2_div = (int) round(CLK2_DIVP * synthesis_rate);
	clk2_counter += CLK2_MUL;
	if(clk2_counter >= clk2_div)
	{
		update_pitch_envelopes();
		clk2_counter -= clk2_div;
	}

	// Update volume/pitch ramps
	for(int v = 0; v < 32; v++)
	{
		UPD937_VoiceState *vo = &voices[v];
		vo->volume_rate_counter++;
		if(vo->volume_rate_counter >= vo->volume_rate_div)
		{
			vo->volume_rate_counter = 0;
			if(vo->volume_down)
			{
				vo->volume = std::clamp(std::max(vo->volume_target, vo->volume - vo->volume_rate_mul), 0, 65535);
			}
			else
			{
				vo->volume = std::clamp(std::min(vo->volume_target, vo->volume + vo->volume_rate_mul), 0, 65535);
			}
		}
		if(vo->volume > 0)
		{
			int pitch_relative = vo->pitch;
			pitch_relative += vo->pitch_env_value/16;
			pitch_relative += channels[vo->channel].bend_offset;
			//if(pitch_relative < 0) pitch_relative = 0;
			//if(pitch_relative > 0x5FF) pitch_relative = 0x5FF;
			vo->sample_fract += read_rom_16(ptr_pitchtable + pitch_relative*2);
			if(vo->sample_fract >= 0x8000)
			{
				vo->sample_fract -= 0x8000;
				vo->sample_last_val = (read_rom_16(vo->sample_ptr*2)>>4) - 0x800;
				vo->sample_ptr++;
			}
			if(vo->sample_ptr > vo->sample_end) vo->sample_ptr = vo->sample_loop;
		}
	}

	sample_count++;
}

void UPD937_Core::update_volume_envelopes()
{
	delay_update_phase = (delay_update_phase+1)&1;
	// Do all at once for now
	for(int v = 0; v < 32; v++)
	{
		UPD937_VoiceState *vo = &voices[v];
		bool changed = false;
		if(vo->volume_env_delay > 0)
		{
			// Update delay
			if(delay_update_phase == 0) vo->volume_env_delay--;
			if(vo->volume_env_delay > 0) continue;
			else if(vo->active) changed = true;
		}
		if(vo->volume_env_step < 16 && vo->volume > 0 && !vo->active)
		{
			// If key released, enter release phase at same step
			vo->volume_env_step |= 16;
			changed = true;
		}
		else
		{
			// If reached target and not ended, advance to next step
			if((vo->volume <= vo->volume_target && vo->volume_down) || (vo->volume >= vo->volume_target && !vo->volume_down))
			{
				if(vo->volume_target > 0 && vo->volume_rate_mul != 0)
				{
					vo->volume_env_step = ((vo->volume_env_step+1)&15) + (vo->volume_env_step&16); // Wrap after 16 steps, stay in same phase
					changed = true;
				}
			}
		}
		bool already_reset = false;
		while(changed)
		{
			changed = false;
			int env_rate = read_rom_8(ptr_volenv + vo->volume_env*64 + vo->volume_env_step*2 + 0);
			int env_target = read_rom_8(ptr_volenv + vo->volume_env*64 + vo->volume_env_step*2 + 1);
			bool env_down = (env_rate>=128);
			env_rate &= 127;
			int env_volume_target = read_rom_16(ptr_voltable + env_target*2);
			// Always process as regular envelope step
			vo->volume_down = env_down;
			if(env_rate == 127)
			{
				// Instant apply
				vo->volume_rate_mul = 0xFFFF;
				vo->volume_rate_div = 1;
			}
			else if(env_rate == 0 && env_down)
			{
				// TODO proper check for condition where target decreased by 1?
				// Hold condition
				vo->volume_rate_mul = 0;
				vo->volume_rate_div = 1;
			}
			// TODO check old target before mapping
			//else if(((env_volume_target < vo->volume_target) && !env_down) || ((env_volume_target > vo->volume_target) && env_down) && !already_reset)
			else if(env_volume_target == 0 && !env_down && !already_reset)
			{
				// Sign mismatch, invalid, reset/loop
				// Real firmware gets stuck in infinite loop if first step is invalid, here we avoid that
				// This is used intentionally by some envelopes for looping on "00 00"
				vo->volume_env_step &= 16;
				already_reset = true;
				changed = true;
			}
			else
			{
				// Regular ramp
				env_rate = (env_rate*2) + 2;
				vo->volume_rate_mul = read_rom_16(ptr_ratetable + env_rate*4 + 0);
				vo->volume_rate_div = read_rom_8(ptr_ratetable + env_rate*4 + 2)+1;
			}
			vo->volume_target = env_volume_target;
		}
	}
}

void UPD937_Core::update_pitch_envelopes()
{
	// Do all at once for now
	for(int v = 0; v < 32; v++)
	{
		UPD937_VoiceState *vo = &voices[v];
		if(vo->volume == 0) continue; // TODO is this a valid check for this?
		bool changed = false;
		// Update delay
		if(vo->pitch_env_delay > 0)
		{
			vo->pitch_env_delay--;
			if(vo->pitch_env_delay > 0) continue;
			else changed = true;
		}

		// Update pitch ramp
		if(vo->pitch_env_rate != 0)
		{
			vo->pitch_env_value += vo->pitch_env_rate;
			bool reached_target = false;
			if(vo->pitch_env_rate > 0) reached_target = (vo->pitch_env_value >= vo->pitch_env_target);
			else reached_target = (vo->pitch_env_value <= vo->pitch_env_target);
			if(reached_target)
			{
				vo->pitch_env_value = vo->pitch_env_target;
				vo->pitch_env_step++;
				if(vo->pitch_env_step >= 8) vo->pitch_env_step = 1; // Should it loop like this?
				changed = true;
			}
		}

		bool already_looped = false;
		while(changed && vo->pitch_env_step < 8)
		{
			changed = false;
			int env_rate = read_rom_16(ptr_pitchenv + vo->pitch_env*32 + vo->pitch_env_step*4 + 0);
			int env_target = read_rom_16(ptr_pitchenv + vo->pitch_env*32 + vo->pitch_env_step*4 + 2);
			bool loop_flag = (env_rate&0x2000) > 0;
			bool env_down = (env_rate&0x1000) > 0;
			env_rate &= 0xFFF;
			if(loop_flag)
			{
				vo->pitch_env_step = env_rate&7;
				changed = !already_looped;
				already_looped = true;
			}
			else
			{
				vo->pitch_env_rate = env_rate * (env_down ? -1 : 1);
				vo->pitch_env_target += env_target * (env_down ? -16 : 16);
			}
		}
	}
}

int UPD937_Core::get_free_voice(int c)
{
	// TODO make this operate on allocation pairs not real voices
	UPD937_ChannelState *ch = &channels[c];

	// Find first inactive from current position
	int ret = ch->first_voice + ch->allocate_next;
	for(int i = 0; i < ch->voice_count; i++)
	{
		if(!(voices[ret].active)) break;
		ch->allocate_next++;
		if(ch->allocate_next >= ch->voice_count) ch->allocate_next = 0;
		ret = ch->first_voice + ch->allocate_next;
	}

	// Start from next voice next time
	ch->allocate_next++;
	if(ch->allocate_next >= ch->voice_count) ch->allocate_next = 0;

	return ret;
}

void UPD937_Core::note_on(int channel, int note)
{
	if(channel < 0 || channel > 3) return;
	UPD937_ChannelState *ch = &channels[channel];
	note &= 127;
	int note_ranged = note;
	while(note_ranged < 36) note_ranged += 12;
	while(note_ranged > 96) note_ranged -= 12;

	// Get instrument descriptor
	int partial_addr = ch->partials_offset;
	int voices_per_note = ch->layered ? 4 : 2;

	// Get keymap and update partial address
	int keymap_byte = (note_ranged-36)/2;
	int keymap_shift = ((note_ranged-36)&1)*4;
	int keymap_val = (read_rom_8(ptr_keymaps + ch->keymap_no*32 + keymap_byte) >> keymap_shift) & 0xF;

	// Get partial
	partial_addr += keymap_val * voices_per_note * 3;
	// TODO: From here layering needs to be implemented with allocating extra voices
	partial_addr *= 2;

	for(int vn = 0; vn < voices_per_note; vn++)
	{
		UPD937_VoiceState *vo = &voices[get_free_voice(channel)];

		// Set basic parameters from the partial
		vo->pitch_env = read_rom_16(ptr_partials + partial_addr + 0);
		vo->volume_env = read_rom_16(ptr_partials + partial_addr + 2);
		int sample_descriptor = read_rom_16(ptr_partials + partial_addr + 4);

		// Get sample data
		vo->sample_start = read_rom_24(ptr_sampdesc + sample_descriptor*10 + 1);
		vo->sample_end = read_rom_24(ptr_sampdesc + sample_descriptor*10 + 4);
		vo->sample_loop = read_rom_24(ptr_sampdesc + sample_descriptor*10 + 7);

		// Initialize sampler
		vo->sample_ptr = vo->sample_start;
		vo->sample_fract = 0;
		vo->sample_last_val = 0; // Hardware might not do this

		// Set note
		vo->note = note;
		int sample_note = read_rom_8(ptr_sampdesc + sample_descriptor*10);
		if(sample_note > 0)
		{
			vo->pitch = (note_ranged - sample_note) * 32;
		}
		else
		{
			vo->pitch = 0x200; // Default for unpitched notes
		}

		// Setup envelope
		vo->volume = 0;
		vo->volume_target = 0;
		vo->volume_rate_mul = 0;
		vo->volume_rate_div = 1;
		vo->volume_down = false;
		vo->volume_env_delay = 0;
		vo->volume_env_step = 0;

		// Read first step of envelope
		int env_rate = read_rom_8(ptr_volenv + vo->volume_env*64 + 0);
		int env_target = read_rom_8(ptr_volenv + vo->volume_env*64 + 1);
		if(env_target == 0)
		{
			// This is a delay step
			vo->volume_env_delay = env_rate+1;
			vo->volume_env_step = 1;
		}
		else
		{
			// Regular envelope step
			vo->volume_down = (env_rate>=128);
			env_rate &= 127;
			vo->volume_target = read_rom_16(ptr_voltable + env_target*2);
			if(env_rate == 127)
			{
				vo->volume_rate_mul = 0xFFFF;
				vo->volume_rate_div = 1;
			}
			else
			{
				env_rate = (env_rate*2) + 2;
				vo->volume_rate_mul = read_rom_16(ptr_ratetable + env_rate*4 + 0);
				vo->volume_rate_div = read_rom_8(ptr_ratetable + env_rate*4 + 2)+1;
			}
		}

		// Set pitch envelope
		int pitch_initial = read_rom_16(ptr_pitchenv + vo->pitch_env*32 + 0);
		pitch_initial = (pitch_initial&0xFFF) * ((pitch_initial>=0x1000) ? -1 : 1);
		vo->pitch_env_value = vo->pitch_env_target = pitch_initial*16;
		vo->pitch_env_rate = 0;
		vo->pitch_env_delay = read_rom_16(ptr_pitchenv + vo->pitch_env*32 + 2) + 1;
		vo->pitch_env_step = 1;

		vo->active = true;
		vo->sustained = false;

		partial_addr += 6;
	}
}

void UPD937_Core::note_off(int channel, int note)
{
	if(channel < 0 || channel > 3) return;
	UPD937_ChannelState *ch = &channels[channel];
	note &= 127;
	int voices_per_note = ch->layered ? 4 : 2;
	for(int v = ch->first_voice; v < ch->first_voice+ch->voice_count; v += voices_per_note)
	{
		UPD937_VoiceState *vo = &voices[v];
		if(vo->note == note && vo->active && !vo->sustained)
		{
			for(int i = 0; i < voices_per_note; i++)
			{
				if(ch->sustain) voices[v+i].sustained = true;
				else voices[v+i].active = false;
			}
			break;
		}
	}
}

void UPD937_Core::prog_chg(int channel, int prog)
{
	if(channel < 0 || channel > 3) return;
	UPD937_ChannelState *ch = &channels[channel];
	// Silence all notes on this channel by decaying over a 512 sample period
	for(int v = ch->first_voice; v < ch->first_voice+ch->voice_count; v++)
	{
		voices[v].active = false;
		voices[v].sustained = false;
		voices[v].volume_rate_mul = (voices[v].volume+511)/512;
		voices[v].volume_rate_div = 1;
		voices[v].volume_target = 0;
		voices[v].volume_down = true;
		voices[v].volume_env_step = 16; // hack to make it think it's in release phase
	}
	ch->allocate_next = 0;
	// Check if new program is valid AFTER silencing notes
	if(prog < 0 || prog > 109) return;
	prog = midi_prog_to_bank(prog, 0);
	// Update channel's instrument parameters
	ch->instrument = prog;
	ch->partials_offset = read_rom_16(ptr_instdesc + prog*4 + 0);
	ch->keymap_no = read_rom_8(ptr_instdesc + prog*4 + 2);
	int flags = read_rom_8(ptr_instdesc + prog*4 + 3);
	ch->layered = (flags & 0x10) > 0;
}

void UPD937_Core::pitch_bend(int channel, int bend_byte)
{
	if(channel < 0 || channel > 3) return;
	UPD937_ChannelState *ch = &channels[channel];
	ch->bend_value = bend_byte-128;
	ch->bend_offset = read_rom_8(ptr_ratetable + bend_byte*4 + 3) - 128;
}

void UPD937_Core::control_chg_sustain(int channel, bool sustain)
{
	if(channel < 0 || channel > 3) return;
	UPD937_ChannelState *ch = &channels[channel];
	ch->sustain = sustain;
	if(!sustain)
	{
		for(int i = ch->first_voice; i < ch->first_voice+ch->voice_count; i++)
		{
			if(voices[i].sustained) voices[i].sustained = voices[i].active = false;
		}
	}
}

int UPD937_Core::midi_prog_to_bank(int prog, int bank_select)
{
	if(prog < 10) return prog+(bank_select*10);
	return prog-10 + bank_select*100 + HC_NUM_BANKS*10;
}

LoopySound::LoopySound(std::vector<uint8_t>& rom_in, float out_rate, int buffer_size)
{
	this->out_rate = out_rate;
	this->synth_rate = TUNING * 192;
	this->mix_level = MIX_LEVEL;
	this->buffer_size = buffer_size;
	printf("[Sound] Init uPD937 core: synth rate %.01f, out rate %.01f, buffer size %d\n", synth_rate, out_rate, buffer_size);
	synth = std::make_unique<UPD937_Core>(rom_in, synth_rate);
	if(FILTER_ENABLE)
	{
		printf("[Sound] Init filters\n");
		filter_tone = std::make_unique<BiquadStereoFilter>(synth_rate, FILTER_CUTOFF, FILTER_RESONANCE, false);
		filter_block_dc = std::make_unique<BiquadStereoFilter>(out_rate, 20.f, 0.7f, true);
	}
	else
	{
		filter_tone = nullptr;
		filter_block_dc = nullptr;
	}
}

void LoopySound::gen_sample(float out[])
{
	// Process midi events every 64 samples
	if((out_sample_count & 63) == 0)
	{
		handle_midi_event();
	}
	interpolation_step += synth_rate / out_rate;
	while(interpolation_step >= 1.f)
	{
		last_sample[0] = current_sample[0];
		last_sample[1] = current_sample[1];
		synth->gen_sample(raw_samples);
		// Get synth sample and filter it at synth rate
		current_sample[0] = raw_samples[0] / 32768.f;
		current_sample[1] = raw_samples[1] / 32768.f;
		if(filter_tone) filter_tone->process(current_sample);
		interpolation_step--;
	}
	// Resample and mix at out rate
	mix_sample[0] = (last_sample[0] + (current_sample[0]-last_sample[0]) * interpolation_step) * 6.8f * mix_level;
	mix_sample[1] = (last_sample[1] + (current_sample[1]-last_sample[1]) * interpolation_step) * 6.8f * mix_level;
	if(filter_block_dc) filter_block_dc->process(mix_sample);
	// Write output
	out[0] = std::clamp(mix_sample[0], -1.f, 1.f);
	out[1] = std::clamp(mix_sample[1], -1.f, 1.f);
	out_sample_count++;
}

void LoopySound::set_channel_muted(int channel, bool mute)
{
	synth->set_channel_muted(channel, mute);
}

void LoopySound::time_reference(float delta)
{
	has_time_reference = true;
	if(delta > 0)
	{
		int delta_samples = (int)floor(delta * out_rate);
		time_reference_samples += delta_samples;
	}

	// Hard correction, keep within sane distance of local time
	if(time_reference_samples < out_sample_count)
	{
		time_reference_samples = out_sample_count;
	}
	else if(time_reference_samples > out_sample_count + (2 * buffer_size))
	{
		time_reference_samples = out_sample_count + (2 * buffer_size);
	}

	// Soft correction, slowly drift towards local time (middle of hard range)
	// This introduces some relative error but biases it to hit hard limits less often
	time_reference_samples += (out_sample_count + buffer_size - time_reference_samples + 32) >> 6;
}

void LoopySound::set_control_register(int creg)
{
	creg &= 0xFFF;
	// Handle volume sliders
	int vol_sw_0 = (creg>>6)&7;
	int vol_sw_1 = (creg>>9)&7;
	if((vol_sw_0&1) > 0) synth->set_volume_slider(0, 2);
	else if((vol_sw_0&2) > 0) synth->set_volume_slider(0, 3);
	else if((vol_sw_0&4) > 0) synth->set_volume_slider(0, 4);
	if((vol_sw_1&1) > 0) synth->set_volume_slider(1, 2);
	else if((vol_sw_1&2) > 0) synth->set_volume_slider(1, 3);
	else if((vol_sw_1&4) > 0) synth->set_volume_slider(1, 4);
	// Handle buttons
	int buttons = creg&63;
	int buttons_pushed = buttons & (~buttons_last);
	buttons_last = buttons;
	// Check button pushes with priority order
	if((buttons_pushed&16) > 0)
	{
		// ON
		channel_config_state = 0;
		synth->set_channel_configuration(false, false);
		synth->reset_channels(true);
	}
	if((buttons_pushed&1) > 0)
	{
		// DEMO
		// temporarily just silence channels when entering demo mode
		in_demo = !in_demo;
		if(in_demo) synth->reset_channels(false);
	}
	if((buttons_pushed&32) > 0 && (channel_config_state == 0))
	{
		// MIDI
		channel_config_state = 1;
		synth->set_channel_configuration(false, false);
		synth->reset_channels(true);
	}
	if((buttons_pushed&8) > 0)
	{
		// EXT
		// Do nothing for now as rhythm not implemented
	}
	if((buttons_pushed&4) > 0 && (channel_config_state == 1 || channel_config_state == 3))
	{
		// CH4
		synth->set_channel_configuration(true, true);
		synth->reset_channels(false);
		channel_config_state = 4;
	}
	if((buttons_pushed&2) > 0 && channel_config_state == 1)
	{
		// CH3
		synth->set_channel_configuration(true, false);
		synth->reset_channels(false);
		channel_config_state = 3;
	}
}

bool LoopySound::midi_in(char b)
{
	// temporarily ignore midi here when in demo or keyboard mode
	if(in_demo || (channel_config_state == 0)) return true;
	return enqueue_midi_byte(b, time_reference_samples);
}

bool LoopySound::enqueue_midi_byte(char midi_byte, int timestamp)
{
	if((queue_write + 1) % MIDI_QUEUE_CAPACITY == queue_read)
	{
		if(!midi_overflowed) printf("[Sound] MIDI queue overflow, increase queue capacity or send smaller groups more often.\n");
		midi_overflowed = true;
		return false;
	}
	midi_overflowed = false;
	midi_queue_bytes[queue_write] = midi_byte;
	midi_queue_timestamps[queue_write] = timestamp;
	queue_write = (queue_write + 1) % MIDI_QUEUE_CAPACITY;
	return true;
}

void LoopySound::handle_midi_event()
{
	while(queue_write != queue_read)
	{
		int event_time = midi_queue_timestamps[queue_read];
		int time_diff = (event_time - out_sample_count); // wraparound taken care of here
		if(has_time_reference && time_diff > 0) break;
		char event_byte = midi_queue_bytes[queue_read];
		queue_read = (queue_read + 1) % MIDI_QUEUE_CAPACITY;
		synth->process_midi_now(event_byte);
	}
}

BiquadStereoFilter::BiquadStereoFilter(float fs, float fc, float q, bool hp)
{
	reset();
	set_parameters(fs, fc, q, hp);
}

void BiquadStereoFilter::set_fs(float fs)
{
	this->fs = fs;
	update_coefficients();
}

void BiquadStereoFilter::set_fc(float fc)
{
	this->fc = fc;
	update_coefficients();
}

void BiquadStereoFilter::set_q(float q)
{
	this->q = q;
	update_coefficients();
}

void BiquadStereoFilter::set_hp(bool hp)
{
	this->hp = hp;
	update_coefficients();
}

void BiquadStereoFilter::set_parameters(float fs, float fc, float q, bool hp)
{
	this->fs = fs;
	this->fc = fc;
	this->q = q;
	this->hp = hp;
	update_coefficients();
}

void BiquadStereoFilter::reset()
{
	for(int c = 0; c < 2; c++)
	{
		x1[c] = x2[c] = y1[c] = y2[c] = 0;
	}
}

void BiquadStereoFilter::process(float sample[])
{
	for(int c = 0; c < 2; c++)
	{
		float x0 = sample[c];
		float y0 = b0*x0 + b1*x1[c] + b2*x2[c] - a1*y1[c] - a2*y2[c];
		x2[c] = x1[c];
		x1[c] = x0;
		y2[c] = y1[c];
		y1[c] = y0;
		sample[c] = y0;
	}
}

void BiquadStereoFilter::update_coefficients()
{
	// Second order shared
	constexpr static float PI = 3.14159265358979323846f;
	float K = (float)tan(PI*fc/fs);
	float W = K*K;
	float alpha = 1 + (K / q) + W;
	a1 = 2 * (W - 1) / alpha;
	a2 = (1 - (K / q) + W) / alpha;
	if(hp)
	{
		// Second-order high pass
		b0 = b2 = 1 / alpha;
		b1 = -2 * b0;
	}
	else
	{
		// Second-order low pass
		b0 = b2 = W / alpha;
		b1 = 2 * b0;
	}
}

}
