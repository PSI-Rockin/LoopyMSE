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
#include <vector>

namespace LoopySound {

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

// Big enough midi retiming queue for 16ms audio buffer > 195 bytes
// Could be a lot lower for realtime midi. Must be a power of 2.
constexpr static int MIDI_QUEUE_CAPACITY = 256;

struct UPD937_VoiceState {
	int channel, note;
	bool active, sustained;
	int pitch;
	int volume, volumeTarget, volumeRateMul, volumeRateDiv, volumeRateCounter;
	bool volumeDown;
	int volEnv, volEnvStep, volEnvDelay;
	int pitchEnv, pitchEnvStep, pitchEnvDelay, pitchEnvValue, pitchEnvRate, pitchEnvTarget;
	int sampStart, sampEnd, sampLoop, sampPtr, sampFract, sampLastVal;
	bool sampNew;
};

struct UPD937_ChannelState {
	bool midiEnabled;
	bool mute; // External
	int firstVoice, voiceCount;
	bool sustain;
	// Current instrument parameters
	int instrument;
	int partialsOffset;
	int keymapNo;
	bool layered;
	int bendOffset, bendValue;
	// Voice allocator
	int allocateNext;
};

class UPD937_Core {
private:
	// Volume sliders arbitrarily scaled to 4096.
	// Values for 0,2,3,4 approximated, 1 guessed.
	const int VOLUME_SLIDER_LEVELS[5] = {0, 2048, 2580, 3251, 4096};

	uint8_t* rom;
	int rommask;

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
	int volumeSlider[2] = {};

	// Timer state
	int clk2Counter = 0;
	int delayUpdatePhase = 0;
	uint32_t sampleCount = 0;

	// Audio output state
	float synthesisRate;

	// Midi parsing
	int midiStatus = 0;
	int midiRunningStatus = 0;
	char midiParamBytes[8] = {};
	int midiParamCount = 0;
	bool midiInSysex = false;

public:
	UPD937_Core(std::vector<uint8_t>& romIn, float synthesisRate);
	void genSample(int out[]);
	void setChannelConfiguration(bool multi, bool all);
	void setVolumeSlider(int group, int slider);
	void setChannelMuted(int channel, bool mute);
	void resetChannels(bool clearProgram);
	void processMidiNow(char midiByte);
private:
	int readRom8(int offset);
	int readRom16(int offset);
	int readRom24(int offset);
	void updateSample();
	void updateVolumeEnvelopes();
	void updatePitchEnvelopes();
	int getFreeVoice(int c);
	void noteOn(int channel, int note);
	void noteOff(int channel, int note);
	void progCh(int channel, int prog);
	void pitchBend(int channel, int bendByte);
	void controlChgSustain(int channel, bool sustain);
	int midiProgToBank(int prog, int bankSelect);
};

class BiquadStereoFilter {
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
	void setFs(float fs);
	void setFc(float fc);
	void setQ(float q);
	void setHp(bool hp);
	void setParameters(float fs, float fc, float q, bool hp);
	void reset();
	void process(float sample[]);
private:
	void updateCoefficients();
};

class LoopySound {
private:
	UPD937_Core *synth;
	BiquadStereoFilter* filterTone;
	BiquadStereoFilter* filterBlockDC;

	// Audio parameters
	float mixLevel;
	float outRate;
	float synthRate;

	// Interpolation state
	int rawSamples[2] = {};
	float currentSample[2] = {};
	float lastSample[2] = {};
	float mixSample[2] = {};
	float interpolationStep = 0;

	// Timing correction
	float buffersPerSecond;
	int sampleCount = 0;
	int timeReferenceT = 0;
	bool hasTimeReference = false;

	// Interface state
	int buttonsLast = 0;
	int channelConfigState = 0;
	bool inDemo = false;

	// MIDI retiming queue
	char midiQueueBytes[MIDI_QUEUE_CAPACITY];
	int midiQueueTimestamps[MIDI_QUEUE_CAPACITY];
	int queueWrite = 0, queueRead = 0;
	int maxQueuedBytes = 0;
	bool midiOverflowed = false;

public:
	LoopySound(std::vector<uint8_t>& romIn, float outRate, float tuning, float mixLevel, float buffersPerSecond, bool filterEnable);
	void genSample(float out[]);
	void setChannelMuted(int channel, bool mute);
	void timeReference(float delta);
	void setControlRegister(int creg);
	bool midiIn(char b);
private:
	bool enqueueMidiByte(char midiByte, int timestamp);
	void handleMidiEvent();
};

}