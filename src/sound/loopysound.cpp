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
#include <vector>

#include <sound/loopysound.h>

namespace LoopySound {

UPD937_Core::UPD937_Core(std::vector<uint8_t>& romIn, float synthesisRate) {
	// Pad ROM to a power of 2
	int romsize = 1;
	while(romsize < romIn.size()) romsize <<= 1;
	rom = new uint8_t[romsize]{0};
	memcpy(rom, romIn.data(), romIn.size());
	rommask = romsize-1;

	// Set up global state
	ptr_partials = readRom16(0) * 32;
	ptr_pitchenv = readRom16(2) * 32;
	ptr_volenv   = readRom16(4) * 32;
	ptr_sampdesc = readRom16(6) * 32;
	ptr_demosong = readRom16(8) * 32;

	// Setup voice state
	for(int i = 0; i < 32; i++) {
		voices[i] = {};
	}

	// Setup channel state
	for(int i = 0; i < 4; i++) {
		channels[i] = {};
		progCh(i, 0);
	}
	setChannelConfiguration(false, false);

	this->synthesisRate = synthesisRate;

	midiRunningStatus = 0;
	midiParamCount = 0;
	midiInSysex = false;

	volumeSlider[0] = volumeSlider[1] = 4;
}

void UPD937_Core::genSample(int out[]) {
	updateSample();
	for(int lr = 0; lr <= 1; lr++) {
		int accum = 0;
		for(int i = 0; i < 16; i ++) {
			UPD937_VoiceState *vo = &voices[i+i+lr];
			UPD937_ChannelState *ch = &channels[vo->channel];
			if(vo->volume == 0) continue;
			if(ch->mute) continue;
			int s = vo->sampLastVal;
			int sb = (readRom16(vo->sampPtr*2)>>4) - 0x800;
			int sd = ((sb - s) * vo->sampFract) / 0x8000;
			s += sd;
			s = (s * vo->volume) / 65536;
			if(vo->channel > 0) {
				s = (s * VOLUME_SLIDER_LEVELS[volumeSlider[vo->channel==3 ? 1 : 0]]) / 4096;
			}
			accum += s;
		}
		accum = std::clamp(accum, -32767, 32767);
		out[lr] = accum;
	}
}

void UPD937_Core::setChannelConfiguration(bool multi, bool all) {
	if(multi) {
		channels[0].firstVoice = 2*0;
		channels[0].voiceCount = 2*6;
		channels[1].firstVoice = 2*6;
		channels[1].voiceCount = 2*4;
		channels[2].firstVoice = 2*10;
		channels[2].voiceCount = 2*2;
		channels[3].firstVoice = 2*12;
		channels[3].voiceCount = 2*4;
		channels[0].midiEnabled = true;
		channels[1].midiEnabled = true;
		channels[2].midiEnabled = true;
		channels[3].midiEnabled = all;
	} else {
		channels[0].firstVoice = 2*0;
		channels[0].voiceCount = 2*12;
		channels[0].midiEnabled = true;
		channels[1].midiEnabled = false;
		channels[2].midiEnabled = false;
		channels[3].midiEnabled = false;
		channels[1].voiceCount = 0;
		channels[2].voiceCount = 0;
		channels[3].voiceCount = 0;
	}
	for(int v = 0; v < 32; v++) voices[v].channel = 0;
	for(int c = 1; c < 4; c++) {
		for(int v = 0; v < channels[c].voiceCount; v++) {
			voices[channels[c].firstVoice+v].channel = c;
		}
	}
	// TODO reset state?
}

void UPD937_Core::setVolumeSlider(int group, int slider) {
	group = std::clamp(group, 0, 1);
	slider = std::clamp(slider, 0, 4);
	volumeSlider[group] = slider;
}

void UPD937_Core::setChannelMuted(int channel, bool mute) {
	channels[channel].mute = mute;
}

void UPD937_Core::resetChannels(bool clearProgram) {
	int p = clearProgram ? 0 : 128;
	progCh(0, p);
	progCh(1, p);
	progCh(2, p);
	progCh(3, p);
}

void UPD937_Core::processMidiNow(char midiByte) {
	// This function must be called from the audio thread!
	// TODO: make thread safe?
	int m = midiByte & 0xFF;
	if(m >= 0x80) {
		// Status byte
		if(m == 0xF0 && !midiInSysex) midiInSysex = true;
		if(m == 0xF7 && midiInSysex) {
			midiInSysex = false;
			// process sysex?
		}
		if(m < 0xF8) {
			midiStatus = m;
			midiRunningStatus = (m < 0xF0) ? m : 0;
			midiParamCount = 0;
		}
	} else {
		if(midiParamCount >= sizeof(midiParamBytes) || midiStatus == 0) return;
		midiParamBytes[midiParamCount++] = (char)(m&0x7F);
		if(midiInSysex) return;
		int statusHi = midiStatus>>4;
		if(statusHi == 0xF) {
			// Process F0-F7 statuses
		} else {
			int channel = midiStatus&0x0F;
			int msize = (statusHi==0xC || statusHi==0xD) ? 1 : 2;
			if(midiParamCount >= msize && !midiInSysex) {
				if(channels[channel].midiEnabled) {
					switch(statusHi) {
					case 0x8:
						noteOff(channel, midiParamBytes[0]);
						break;
					case 0x9:
						if(midiParamBytes[1] > 0) noteOn(channel, midiParamBytes[0]);
						else noteOff(channel, midiParamBytes[0]);
						break;
					case 0xA:
						printf("[Sound] unhandled message KEY PRESSURE\n");
						break;
					case 0xB:
						if(midiParamBytes[0] == 0x40) {
							controlChgSustain(channel, (midiParamBytes[1] >= 0x40));
						} else {
							printf("[Sound] unhandled message CONTROL CHANGE %02X %02X\n", midiParamBytes[0], midiParamBytes[1]);
						}
						break;
					case 0xC:
						progCh(channel, midiParamBytes[0]);
						break;
					case 0xD:
						printf("[Sound] unhandled message CHANNEL PRESSURE\n");
						break;
					case 0xE:
						pitchBend(channel, (midiParamBytes[1]<<1) | (midiParamBytes[1]>>6));
						break;
					case 0xF:
					default:
						break;
					}
				}
				midiParamCount = 0;
				midiStatus = midiRunningStatus;
			}
		}
	}
}

int UPD937_Core::readRom8(int offset) {
	return rom[offset&rommask]&0xFF;
}

int UPD937_Core::readRom16(int offset) {
	return ((rom[(offset+1)&rommask]&0xFF)<<8)|(rom[offset&rommask]&0xFF);
}

int UPD937_Core::readRom24(int offset) {
	return ((rom[(offset+2)&rommask]&0xFF)<<16)|((rom[(offset+1)&rommask]&0xFF)<<8)|(rom[offset&rommask]&0xFF);
}

void UPD937_Core::updateSample() {
	// Clock the volume & pitch envelope generators
	if((sampleCount%384) == 0) updateVolumeEnvelopes();
	int clk2div = (int) round(CLK2_DIVP * synthesisRate);
	clk2Counter += CLK2_MUL;
	if(clk2Counter >= clk2div) {
		//printf("[Sound] CLK2 %08X %d %d %d %.2f\n", clk2Counter, CLK2_MUL, clk2div, CLK2_DIVP, synthesisRate);
		updatePitchEnvelopes();
		clk2Counter -= clk2div;
	}

	// Update volume/pitch ramps
	for(int i = 0; i < 32; i++) {
		UPD937_VoiceState *vo = &voices[i];
		vo->volumeRateCounter++;
		if(vo->volumeRateCounter >= vo->volumeRateDiv) {
			vo->volumeRateCounter = 0;
			if(vo->volumeDown) {
				vo->volume = std::clamp(std::max(vo->volumeTarget, vo->volume - vo->volumeRateMul), 0, 65535);
			} else {
				vo->volume = std::clamp(std::min(vo->volumeTarget, vo->volume + vo->volumeRateMul), 0, 65535);
			}
		}
		if(vo->volume > 0) {
			int pitchRelative = vo->pitch;
			pitchRelative += vo->pitchEnvValue/16;
			pitchRelative += channels[vo->channel].bendOffset;
			//if(pitchRelative < 0) pitchRelative = 0;
			//if(pitchRelative > 0x5FF) pitchRelative = 0x5FF;
			vo->sampFract += readRom16(ptr_pitchtable + pitchRelative*2);
			if(vo->sampFract >= 0x8000) {
				vo->sampFract -= 0x8000;
				vo->sampLastVal = (readRom16(vo->sampPtr*2)>>4) - 0x800;
				vo->sampPtr++;
			}
			if(vo->sampPtr > vo->sampEnd) vo->sampPtr = vo->sampLoop;
		}
	}

	sampleCount++;
}

void UPD937_Core::updateVolumeEnvelopes() {
	delayUpdatePhase = (delayUpdatePhase+1)&1;
	// Do all at once for now
	for(int i = 0; i < 32; i++) {
		UPD937_VoiceState *vo = &voices[i];
		bool changed = false;
		if(vo->volEnvDelay > 0) {
			// Update delay
			if(delayUpdatePhase == 0) vo->volEnvDelay--;
			if(vo->volEnvDelay > 0) continue;
			else if(vo->active) changed = true;
		}
		if(vo->volEnvStep < 16 && vo->volume > 0 && !vo->active) {
			// If key released, enter release phase at same step
			vo->volEnvStep |= 16;
			changed = true;
		} else {
			// If reached target and not ended, advance to next step
			if((vo->volume <= vo->volumeTarget && vo->volumeDown) || (vo->volume >= vo->volumeTarget && !vo->volumeDown)) {
				if(vo->volumeTarget > 0 && vo->volumeRateMul != 0) {
					vo->volEnvStep = ((vo->volEnvStep+1)&15) + (vo->volEnvStep&16); // Wrap after 16 steps, stay in same phase
					changed = true;
				}
			}
		}
		bool alreadyReset = false;
		while(changed) {
			changed = false;
			int envRate = readRom8(ptr_volenv + vo->volEnv*64 + vo->volEnvStep*2 + 0);
			int envTarget = readRom8(ptr_volenv + vo->volEnv*64 + vo->volEnvStep*2 + 1);
			bool envDown = (envRate>=128);
			envRate &= 127;
			int envTargetV = readRom16(ptr_voltable + envTarget*2);
			// Always process as regular envelope step
			vo->volumeDown = envDown;
			if(envRate == 127) {
				// Instant apply
				vo->volumeRateMul = 0xFFFF;
				vo->volumeRateDiv = 1;
			} else if(envRate == 0 && envDown) { // TODO proper check for condition where target decreased by 1?
				// Hold condition
				vo->volumeRateMul = 0;
				vo->volumeRateDiv = 1;
			//} else if(((envTargetV < vo->volumeTarget) && !envDown) || ((envTargetV > vo->volumeTarget) && envDown) && !alreadyReset) { // TODO check old target before mapping
			} else if(envTargetV == 0 && !envDown && !alreadyReset) {
				// Sign mismatch, invalid, reset/loop
				// Real firmware gets stuck in infinite loop if first step is invalid, here we avoid that
				// This is used intentionally by some envelopes for looping on "00 00"
				vo->volEnvStep &= 16;
				alreadyReset = true;
				changed = true;
			} else {
				// Regular ramp
				envRate = (envRate*2) + 2;
				vo->volumeRateMul = readRom16(ptr_ratetable + envRate*4 + 0);
				vo->volumeRateDiv = readRom8(ptr_ratetable + envRate*4 + 2)+1;
			}
			vo->volumeTarget = envTargetV;
		}
	}
}

void UPD937_Core::updatePitchEnvelopes() {
	// Do all at once for now
	for(int i = 0; i < 32; i++) {
		UPD937_VoiceState *vo = &voices[i];
		if(vo->volume == 0) continue; // TODO is this a valid check for this?
		bool changed = false;
		// Update delay
		if(vo->pitchEnvDelay > 0) {
			vo->pitchEnvDelay--;
			if(vo->pitchEnvDelay > 0) continue;
			else changed = true;
		}

		// Update pitch ramp
		if(vo->pitchEnvRate != 0) {
			vo->pitchEnvValue += vo->pitchEnvRate;
			bool reachedTarget = false;
			if(vo->pitchEnvRate > 0) reachedTarget = (vo->pitchEnvValue >= vo->pitchEnvTarget);
			else reachedTarget = (vo->pitchEnvValue <= vo->pitchEnvTarget);
			if(reachedTarget) {
				vo->pitchEnvValue = vo->pitchEnvTarget;
				vo->pitchEnvStep++;
				if(vo->pitchEnvStep >= 8) vo->pitchEnvStep = 1; // Should it loop like this?
				changed = true;
			}
		}

		bool alreadyLooped = false;
		while(changed && vo->pitchEnvStep < 8) {
			changed = false;
			int envRate = readRom16(ptr_pitchenv + vo->pitchEnv*32 + vo->pitchEnvStep*4 + 0);
			int envTarget = readRom16(ptr_pitchenv + vo->pitchEnv*32 + vo->pitchEnvStep*4 + 2);
			bool loopFlag = (envRate&0x2000) > 0;
			bool envDown = (envRate&0x1000) > 0;
			envRate &= 0xFFF;
			if(loopFlag) {
				vo->pitchEnvStep = envRate&7;
				changed = !alreadyLooped;
				alreadyLooped = true;
			} else {
				vo->pitchEnvRate = envRate * (envDown ? -1 : 1);
				vo->pitchEnvTarget += envTarget * (envDown ? -16 : 16);
			}
		}
	}
}

int UPD937_Core::getFreeVoice(int c) {
	// TODO make this operate on allocation pairs not real voices
	UPD937_ChannelState *ch = &channels[c];

	// Find first inactive from current position
	int ret = ch->firstVoice + ch->allocateNext;
	for(int i = 0; i < ch->voiceCount; i++) {
		if(!(voices[ret].active)) break;
		ch->allocateNext++;
		if(ch->allocateNext >= ch->voiceCount) ch->allocateNext = 0;
		ret = ch->firstVoice + ch->allocateNext;
	}

	// Start from next voice next time
	ch->allocateNext++;
	if(ch->allocateNext >= ch->voiceCount) ch->allocateNext = 0;

	return ret;
}

void UPD937_Core::noteOn(int channel, int note) {
	if(channel < 0 || channel > 3) return;
	UPD937_ChannelState *ch = &channels[channel];
	note &= 127;
	int noteRanged = note;
	while(noteRanged < 36) noteRanged += 12;
	while(noteRanged > 96) noteRanged -= 12;

	// Get instrument descriptor
	int partialAddr = ch->partialsOffset;

	// Get keymap and update partial address
	int kmByte = (noteRanged-36)/2;
	int kmShift = ((noteRanged-36)&1)*4;
	int kmVal = (readRom8(ptr_keymaps + ch->keymapNo*32 + kmByte) >> kmShift) & 0xF;
	partialAddr += kmVal * (ch->layered?12:6);

	// Get partial
	// TODO: From here layering needs to be implemented with allocating extra voices
	partialAddr *= 2;

	for(int vn = 0; vn < (ch->layered?4:2); vn++) {
		UPD937_VoiceState *vo = &voices[getFreeVoice(channel)];

		// Set basic parameters from the partial
		vo->pitchEnv = readRom16(ptr_partials + partialAddr + 0);
		vo->volEnv = readRom16(ptr_partials + partialAddr + 2);
		int samp = readRom16(ptr_partials + partialAddr + 4);

		// Get sample data
		vo->sampStart = readRom24(ptr_sampdesc + samp*10 + 1);
		vo->sampEnd = readRom24(ptr_sampdesc + samp*10 + 4);
		vo->sampLoop = readRom24(ptr_sampdesc + samp*10 + 7);

		// Initialize sampler
		vo->sampPtr = vo->sampStart;
		vo->sampFract = 0;
		vo->sampLastVal = 0; // Hardware might not do this

		// Set note
		vo->note = note;
		int sampleNote = readRom8(ptr_sampdesc + samp*10);
		if(sampleNote > 0) {
			vo->pitch = (noteRanged - sampleNote) * 32;
		} else {
			vo->pitch = 0x200; // Default for unpitched notes
		}

		// Setup envelope
		vo->volume = 0;
		vo->volumeTarget = 0;
		vo->volumeRateMul = 0;
		vo->volumeRateDiv = 1;
		vo->volumeDown = false;
		vo->volEnvDelay = 0;
		vo->volEnvStep = 0;

		// Read first step of envelope
		int envRate = readRom8(ptr_volenv + vo->volEnv*64 + 0);
		int envTarget = readRom8(ptr_volenv + vo->volEnv*64 + 1);
		if(envTarget == 0) {
			// This is a delay step
			vo->volEnvDelay = envRate+1;
			vo->volEnvStep = 1;
		} else {
			// Regular envelope step
			vo->volumeDown = (envRate>=128);
			envRate &= 127;
			vo->volumeTarget = readRom16(ptr_voltable + envTarget*2);
			if(envRate == 127) {
				vo->volumeRateMul = 0xFFFF;
				vo->volumeRateDiv = 1;
			} else {
				envRate = (envRate*2) + 2;
				vo->volumeRateMul = readRom16(ptr_ratetable + envRate*4 + 0);
				vo->volumeRateDiv = readRom8(ptr_ratetable + envRate*4 + 2)+1;
			}
		}

		// Set pitch envelope
		int pitchInitial = readRom16(ptr_pitchenv + vo->pitchEnv*32 + 0);
		pitchInitial = (pitchInitial&0xFFF) * ((pitchInitial>=0x1000) ? -1 : 1);
		vo->pitchEnvValue = vo->pitchEnvTarget = pitchInitial*16;
		vo->pitchEnvRate = 0;
		vo->pitchEnvDelay = readRom16(ptr_pitchenv + vo->pitchEnv*32 + 2) + 1;
		vo->pitchEnvStep = 1;

		vo->active = true;
		vo->sustained = false;

		partialAddr += 6;
	}
}

void UPD937_Core::noteOff(int channel, int note) {
	if(channel < 0 || channel > 3) return;
	UPD937_ChannelState *ch = &channels[channel];
	note &= 127;
	int voicesPerNote = ch->layered ? 4 : 2;
	for(int i = ch->firstVoice; i < ch->firstVoice+ch->voiceCount; i += voicesPerNote) {
		UPD937_VoiceState *vo = &voices[i];
		if(vo->note == note && vo->active && !vo->sustained) {
			for(int j = 0; j < voicesPerNote; j++) {
				if(ch->sustain) voices[i+j].sustained = true;
				else voices[i+j].active = false;
			}
			break;
		}
	}
}

void UPD937_Core::progCh(int channel, int prog) {
	if(channel < 0 || channel > 3) return;
	UPD937_ChannelState *ch = &channels[channel];
	// Silence all notes on this channel by decaying over a 512 sample period
	for(int i = ch->firstVoice; i < ch->firstVoice+ch->voiceCount; i++) {
		voices[i].active = false;
		voices[i].sustained = false;
		voices[i].volumeRateMul = (voices[i].volume+511)/512;
		voices[i].volumeRateDiv = 1;
		voices[i].volumeTarget = 0;
		voices[i].volumeDown = true;
		voices[i].volEnvStep = 16; // hack to make it think it's in release phase
	}
	ch->allocateNext = 0;
	// Check if new program is valid AFTER silencing notes
	if(prog < 0 || prog > 109) return;
	prog = midiProgToBank(prog, 0);
	// Update channel's instrument parameters
	ch->instrument = prog;
	ch->partialsOffset = readRom16(ptr_instdesc + prog*4 + 0);
	ch->keymapNo = readRom8(ptr_instdesc + prog*4 + 2);
	int flags = readRom8(ptr_instdesc + prog*4 + 3);
	ch->layered = (flags & 0x10) > 0;
}

void UPD937_Core::pitchBend(int channel, int bendByte) {
	if(channel < 0 || channel > 3) return;
	UPD937_ChannelState *ch = &channels[channel];
	ch->bendValue = bendByte-128;
	ch->bendOffset = readRom8(ptr_ratetable + bendByte*4 + 3) - 128;
}

void UPD937_Core::controlChgSustain(int channel, bool sustain) {
	if(channel < 0 || channel > 3) return;
	UPD937_ChannelState *ch = &channels[channel];
	ch->sustain = sustain;
	if(!sustain) {
		for(int i = ch->firstVoice; i < ch->firstVoice+ch->voiceCount; i++) {
			if(voices[i].sustained) voices[i].sustained = voices[i].active = false;
		}
	}
}

int UPD937_Core::midiProgToBank(int prog, int bankSelect) {
	if(prog < 10) return prog+(bankSelect*10);
	return prog-10 + bankSelect*100 + HC_NUM_BANKS*10;
}

LoopySound::LoopySound(std::vector<uint8_t>& romIn, float outRate, int bufferSize) {
	this->outRate = outRate;
	this->synthRate = TUNING * 192;
	this->mixLevel = MIX_LEVEL;
	this->bufferSize = bufferSize;
	printf("[Sound] Init uPD937 core: synth rate %.01f, out rate %.01f, buffer size %d\n", synthRate, outRate, bufferSize);
	synth = new UPD937_Core(romIn, synthRate);
	if(FILTER_ENABLE) {
		printf("[Sound] Init filters\n");
		filterTone = new BiquadStereoFilter(synthRate, FILTER_CUTOFF, FILTER_RESONANCE, false);
		filterBlockDC = new BiquadStereoFilter(outRate, 20, 0.7, true);
	} else {
		filterTone = NULL;
		filterBlockDC = NULL;
	}
}

void LoopySound::genSample(float out[]) {
	// Process midi events every 64 samples
	if((outSampleCount & 63) == 0) {
		handleMidiEvent();
	}
	interpolationStep += synthRate / outRate;
	while(interpolationStep >= 1.f) {
		lastSample[0] = currentSample[0];
		lastSample[1] = currentSample[1];
		synth->genSample(rawSamples);
		// Get synth sample and filter it at synth rate
		currentSample[0] = rawSamples[0] / 32768.f;
		currentSample[1] = rawSamples[1] / 32768.f;
		if(filterTone) filterTone->process(currentSample);
		interpolationStep--;
	}
	// Resample and mix at out rate
	mixSample[0] = (lastSample[0] + (currentSample[0]-lastSample[0]) * interpolationStep) * 6.8f * mixLevel;
	mixSample[1] = (lastSample[1] + (currentSample[1]-lastSample[1]) * interpolationStep) * 6.8f * mixLevel;
	if(filterBlockDC) filterBlockDC->process(mixSample);
	// Write output
	out[0] = std::clamp(mixSample[0], -1.f, 1.f);
	out[1] = std::clamp(mixSample[1], -1.f, 1.f);
	outSampleCount++;
}

void LoopySound::setChannelMuted(int channel, bool mute) {
	synth->setChannelMuted(channel, mute);
}

void LoopySound::timeReference(float delta) {
	hasTimeReference = true;
	if(delta > 0) {
		int deltaSamples = (int)floor(delta * outRate);
		timeReferenceSamples += deltaSamples;
	}

	// Hard correction, keep within sane distance of local time
	if(timeReferenceSamples < outSampleCount) {
		timeReferenceSamples = outSampleCount;
	} else if(timeReferenceSamples > outSampleCount + (2 * bufferSize)) {
		timeReferenceSamples = outSampleCount + (2 * bufferSize);
	}

	// Soft correction, slowly drift towards local time (middle of hard range)
	// This introduces some relative error but biases it to hit hard limits less often
	timeReferenceSamples += (outSampleCount + bufferSize - timeReferenceSamples + 32) >> 6;
}

void LoopySound::setControlRegister(int creg) {
	creg &= 0xFFF;
	// Handle volume sliders
	int volSw0 = (creg>>6)&7;
	int volSw1 = (creg>>9)&7;
	if((volSw0&1) > 0) synth->setVolumeSlider(0, 2);
	else if((volSw0&2) > 0) synth->setVolumeSlider(0, 3);
	else if((volSw0&4) > 0) synth->setVolumeSlider(0, 4);
	if((volSw1&1) > 0) synth->setVolumeSlider(1, 2);
	else if((volSw1&2) > 0) synth->setVolumeSlider(1, 3);
	else if((volSw1&4) > 0) synth->setVolumeSlider(1, 4);
	// Handle buttons
	int buttons = creg&63;
	int buttonsPushed = buttons & (~buttonsLast);
	buttonsLast = buttons;
	// Check button pushes with priority order
	if((buttonsPushed&16) > 0) { // ON
		channelConfigState = 0;
		synth->setChannelConfiguration(false, false);
		synth->resetChannels(true);
	}
	if((buttonsPushed&1) > 0) { // DEMO
		// temporarily just silence channels when entering demo mode
		inDemo = !inDemo;
		if(inDemo) synth->resetChannels(false);
	}
	if((buttonsPushed&32) > 0 && (channelConfigState == 0)) { // MIDI
		channelConfigState = 1;
		synth->setChannelConfiguration(false, false);
		synth->resetChannels(true);
	}
	if((buttonsPushed&8) > 0) { // EXT
		// Do nothing for now as rhythm not implemented
	}
	if((buttonsPushed&4) > 0 && (channelConfigState == 1 || channelConfigState == 3)) { // CH4
		synth->setChannelConfiguration(true, true);
		synth->resetChannels(false);
		channelConfigState = 4;
	}
	if((buttonsPushed&2) > 0 && channelConfigState == 1) { // CH3
		synth->setChannelConfiguration(true, false);
		synth->resetChannels(false);
		channelConfigState = 3;
	}
}

bool LoopySound::midiIn(char b) {
	// temporarily ignore midi here when in demo or keyboard mode
	if(inDemo || (channelConfigState == 0)) return true;
	return enqueueMidiByte(b, timeReferenceSamples);
}

bool LoopySound::enqueueMidiByte(char midiByte, int timestamp) {
	if((queueWrite + 1) % MIDI_QUEUE_CAPACITY == queueRead) {
		if(!midiOverflowed) printf("[Sound] MIDI queue overflow, increase queue capacity or send smaller groups more often.\n");
		midiOverflowed = true;
		return false;
	}
	midiOverflowed = false;
	midiQueueBytes[queueWrite] = midiByte;
	midiQueueTimestamps[queueWrite] = timestamp;
	queueWrite = (queueWrite + 1) % MIDI_QUEUE_CAPACITY;
	return true;
}

void LoopySound::handleMidiEvent() {
	while(queueWrite != queueRead) {
		int etime = midiQueueTimestamps[queueRead];
		int timeDiff = (etime - outSampleCount); // wraparound taken care of here
		if(hasTimeReference && timeDiff > 0) break;
		char ebyte = midiQueueBytes[queueRead];
		queueRead = (queueRead + 1) % MIDI_QUEUE_CAPACITY;
		synth->processMidiNow(ebyte);
	}
}

BiquadStereoFilter::BiquadStereoFilter(float fs, float fc, float q, bool hp) {
	reset();
	setParameters(fs, fc, q, hp);
}

void BiquadStereoFilter::setFs(float fs) {
	this->fs = fs;
	updateCoefficients();
}

void BiquadStereoFilter::setFc(float fc) {
	this->fc = fc;
	updateCoefficients();
}

void BiquadStereoFilter::setQ(float q) {
	this->q = q;
	updateCoefficients();
}

void BiquadStereoFilter::setHp(bool hp) {
	this->hp = hp;
	updateCoefficients();
}

void BiquadStereoFilter::setParameters(float fs, float fc, float q, bool hp) {
	this->fs = fs;
	this->fc = fc;
	this->q = q;
	this->hp = hp;
	updateCoefficients();
}

void BiquadStereoFilter::reset() {
	for(int c = 0; c < 2; c++) {
		x1[c] = x2[c] = y1[c] = y2[c] = 0;
	}
}

void BiquadStereoFilter::process(float sample[]) {
	for(int c = 0; c < 2; c++) {
		float x0 = sample[c];
		float y0 = b0*x0 + b1*x1[c] + b2*x2[c] - a1*y1[c] - a2*y2[c];
		x2[c] = x1[c];
		x1[c] = x0;
		y2[c] = y1[c];
		y1[c] = y0;
		sample[c] = y0;
	}
}

void BiquadStereoFilter::updateCoefficients() {
	// Second order shared
	constexpr static float PI = 3.14159265358979323846;
	float K = (float)tan(PI*fc/fs);
	float W = K*K;
	float alpha = 1 + (K / q) + W;
	a1 = 2 * (W - 1) / alpha;
	a2 = (1 - (K / q) + W) / alpha;
	if(hp) {
		// Second-order high pass
		b0 = b2 = 1 / alpha;
		b1 = -2 * b0;
	} else {
		// Second-order low pass
		b0 = b2 = W / alpha;
		b1 = 2 * b0;
	}
}


}