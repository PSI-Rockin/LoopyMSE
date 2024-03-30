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

#pragma once
#include <cstdint>
#include <memory>
#include <vector>

namespace LoopySound
{

/* Audio synthesis parameters start */

// Tuning of A4 note, affects internal sample rate.
// Standard is 442Hz (internal sample rate 84864Hz).
constexpr static float TUNING = 442.f;

// Final mix level after amplification circuit.
// Comfortable listening level is around 0.7 to 0.8, typical hardware level 0.62.
constexpr static float MIX_LEVEL = 0.7f;

// Filters affects both high and low frequencies to approximate the hardware's resonant LPF.
// Cutoff and resonance derived from theoretical circuit analysis.
constexpr static bool FILTER_ENABLE = true;
constexpr static float FILTER_CUTOFF = 8247.f;
constexpr static float FILTER_RESONANCE = 1.67f;

/* Audio synthesis parameters end*/


// Temporary hardcoded stuff
constexpr static int HC_RATETABLE = 0x1000;
constexpr static int HC_VOLTABLE = 0x1400;
constexpr static int HC_PITCHTABLE = 0x1600;
constexpr static int HC_INSTDESC = 0x2200;
constexpr static int HC_KEYMAPS = 0x3DA0;
constexpr static int HC_NUM_BANKS = 1;

// Pitch envelopes update at MIDICLK(4M)/32768 instead of using main clock
// So we need to approximate 4M/32768 clock from sample rate
constexpr static int CLK2_MUL = 15625;
constexpr static int CLK2_DIVP = 128;

// Big enough midi retiming queue for >250ms audio buffer.
// Could be a lot lower for realtime midi. Must be a power of 2.
constexpr static int MIDI_QUEUE_CAPACITY = 2048;

struct UPD937_VoiceState
{
	int channel, note;
	bool active, sustained;
	int pitch;
	int volume, volume_target, volume_rate_mul, volume_rate_div, volume_rate_counter;
	bool volume_down;
	int volume_env, volume_env_step, volume_env_delay;
	int pitch_env, pitch_env_step, pitch_env_delay, pitch_env_value, pitch_env_rate, pitch_env_target;
	int sample_start, sample_end, sample_loop, sample_ptr, sample_fract, sample_last_val;
	bool sample_new;
};

struct UPD937_ChannelState
{
	bool midi_enabled;
	bool mute; // External
	int first_voice, voice_count;
	bool sustain;
	// Current instrument parameters
	int instrument;
	int partials_offset;
	int keymap_no;
	bool layered;
	int bend_offset, bend_value;
	// Voice allocator
	int allocate_next;
};

class UPD937_Core
{
private:
	// Volume sliders arbitrarily scaled to 4096.
	// Values for 0,2,3,4 approximated, 1 guessed.
	const int VOLUME_SLIDER_LEVELS[5] = {0, 2048, 2580, 3251, 4096};

	std::unique_ptr<uint8_t[]> rom;
	int rom_mask;

	// Global state
	uint32_t ptr_partials;
	uint32_t ptr_pitchenv;
	uint32_t ptr_volenv;
	uint32_t ptr_sampdesc;
	uint32_t ptr_demosong;
	uint32_t ptr_pitchtable = HC_PITCHTABLE;
	uint32_t ptr_instdesc = HC_INSTDESC;
	uint32_t ptr_keymaps = HC_KEYMAPS;
	uint32_t ptr_ratetable = HC_RATETABLE;
	uint32_t ptr_voltable = HC_VOLTABLE;

	// Sound synthesis state
	UPD937_VoiceState voices[32];
	UPD937_ChannelState channels[32];
	int volume_slider[2] = {};

	// Timer state
	int clk2_counter = 0;
	int delay_update_phase = 0;
	uint32_t sample_count = 0;

	// Audio output state
	float synthesis_rate;

	// Midi parsing
	int midi_status = 0;
	int midi_running_status = 0;
	char midi_param_bytes[8] = {};
	int midi_param_count = 0;
	bool midi_in_sysex = false;

public:
	UPD937_Core(std::vector<uint8_t>& rom_in, float synthesis_rate);
	void gen_sample(int out[]);
	void set_channel_configuration(bool multi, bool all);
	void set_volume_slider(int group, int slider);
	void set_channel_muted(int channel, bool mute);
	void reset_channels(bool clear_program);
	void process_midi_now(char midi_byte);
private:
	int read_rom_8(int offset);
	int read_rom_16(int offset);
	int read_rom_24(int offset);
	void update_sample();
	void update_volume_envelopes();
	void update_pitch_envelopes();
	int get_free_voice(int c);
	void note_on(int channel, int note);
	void note_off(int channel, int note);
	void prog_chg(int channel, int prog);
	void pitch_bend(int channel, int bend_byte);
	void control_chg_sustain(int channel, bool sustain);
	int midi_prog_to_bank(int prog, int bank_select);
};

class BiquadStereoFilter
{
private:
	// Filter parameters
	float fs, fc, q;
	bool hp = false;
	// Filter coefficients
	float a1, a2, b0, b1, b2;
	// Filter state (stereo)
	float x1[2];
	float x2[2];
	float y1[2];
	float y2[2];

public:
	BiquadStereoFilter(float fs, float fc, float q, bool hp);
	void set_fs(float fs);
	void set_fc(float fc);
	void set_q(float q);
	void set_hp(bool hp);
	void set_parameters(float fs, float fc, float q, bool hp);
	void reset();
	void process(float sample[]);
private:
	void update_coefficients();
};

class LoopySound
{
private:
	std::unique_ptr<UPD937_Core> synth;
	std::unique_ptr<BiquadStereoFilter> filter_tone;
	std::unique_ptr<BiquadStereoFilter> filter_block_dc;

	// Audio parameters
	float mix_level;
	float out_rate;
	float synth_rate;
	int buffer_size;

	// Interpolation state
	int raw_samples[2] = {};
	float current_sample[2] = {};
	float last_sample[2] = {};
	float mix_sample[2] = {};
	float interpolation_step = 0;

	// Timing correction
	int out_sample_count = 0;
	int time_reference_samples = 0;
	bool has_time_reference = false;

	// Interface state
	int buttons_last = 0;
	int channel_config_state = 0;
	bool in_demo = false;

	// MIDI retiming queue
	char midi_queue_bytes[MIDI_QUEUE_CAPACITY];
	int midi_queue_timestamps[MIDI_QUEUE_CAPACITY];
	int queue_write = 0, queue_read = 0;
	bool midi_overflowed = false;

public:
	LoopySound(std::vector<uint8_t>& rom_in, float out_rate, int buffer_size);
	void gen_sample(float out[]);
	void set_channel_muted(int channel, bool mute);
	void time_reference(float delta);
	void set_control_register(int creg);
	bool midi_in(char b);
private:
	bool enqueue_midi_byte(char midi_byte, int timestamp);
	void handle_midi_event();
};

}
