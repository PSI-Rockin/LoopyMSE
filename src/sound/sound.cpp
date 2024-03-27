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

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <core/timing.h>
#include <sound/sound.h>
#include <sound/loopysound.h>

namespace Sound
{

static Timing::FuncHandle timeref_func;
static Timing::EventHandle timeref_ev;

static LoopySound::LoopySound* soundEngine;

static void timeref(uint64_t param, int cycles_late)
{
	constexpr static int CYCLES_PER_TIMEREF = Timing::F_CPU / TIMEREF_FREQUENCY;
	Timing::UnitCycle timeref_cycles = Timing::convert_cpu(CYCLES_PER_TIMEREF - cycles_late);
	timeref_ev = Timing::add_event(timeref_func, timeref_cycles, 0, Timing::CPU_TIMER);

	soundEngine->timeReference(1.f / TIMEREF_FREQUENCY);
}

void initialize(std::vector<uint8_t>& soundRom, int sampleRate, int bufferSize)
{
	if(!soundRom.empty())
	{
		soundEngine = new LoopySound::LoopySound(soundRom, (float)sampleRate, TUNING, MIX_LEVEL, (float)sampleRate / (float)bufferSize, FILTER_ENABLE);
		if(TIMEREF_ENABLE)
		{
			printf("[Sound] Schedule timeref %d Hz\n", TIMEREF_FREQUENCY);
			timeref_func = Timing::register_func("Sound::timeref", timeref);
			timeref(0, 0);
		}
	}
}

void shutdown()
{
	// nop
}


uint8_t ctrl_read8(uint32_t addr)
{
	assert(0);
	return 0;
}
uint16_t ctrl_read16(uint32_t addr)
{
	assert(0);
	return 0;
}
uint32_t ctrl_read32(uint32_t addr)
{
	assert(0);
	return 0;
}
void ctrl_write8(uint32_t addr, uint8_t value)
{
	assert(0);
}
void ctrl_write16(uint32_t addr, uint16_t value)
{
	value &= 0xFFF;
	//printf("[Sound] Control register %03X\n", value);
	//fflush(stdout);
	if(soundEngine) {
		soundEngine->setControlRegister(value);
	}
}
void ctrl_write32(uint32_t addr, uint32_t value)
{
	assert(0);
}

void midi_byte_in(uint8_t value)
{
	//printf("[Sound] MIDI byte %02X\n", value);
	//fflush(stdout);
	if(soundEngine) {
		soundEngine->midiIn((char)value);
	}
}

void buffer_callback(int16_t* buffer, uint32_t count, bool mute)
{
	// Soft mute the audio by outputting zeros here,
	// but keep running the synth so nothing bad happens when unmuting
	
	if(mute || !soundEngine) memset(buffer, 0, count*2);
	
	if(soundEngine) {
		float tmp[2];
		int p = 0;
		for(int i = 0; i < count/2; i++) {
			soundEngine->genSample(tmp);
			if(!mute) {
				buffer[p++] = (int16_t) round(tmp[0] * 32767.f);
				buffer[p++] = (int16_t) round(tmp[1] * 32767.f);
			}
		}
	}
}

}
