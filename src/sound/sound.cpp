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
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include <SDL.h>

#include <core/timing.h>
#include <sound/sound.h>
#include <sound/loopysound.h>

namespace Sound
{

static Timing::FuncHandle timeref_func;
static Timing::EventHandle timeref_ev;

static std::unique_ptr<LoopySound::LoopySound> sound_engine;

static int sample_rate;
static int buffer_size;

static bool mute = false;
static float volume_level; // Automatically managed by mute

static void buffer_callback(float* buffer, uint32_t count);

/* SDL-specific code start */

static SDL_AudioDeviceID audio_device;

static void sdl_audio_callback(void* userdata, uint8_t* raw_buffer, int len)
{
	float* sample_buffer = (float*)raw_buffer;
	int sample_count = len / sizeof(float);
	buffer_callback(sample_buffer, sample_count);
}

static bool sdl_audio_initialize()
{
	// Initialize SDL audio subsystem if available
	if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		printf("[Sound] SDL audio unavailable: %s\n", SDL_GetError());
		return false;
	}

	// Set up desired audio format
	SDL_AudioSpec format_desired;
	SDL_zero(format_desired);
	format_desired.freq = TARGET_SAMPLE_RATE;
	format_desired.format = AUDIO_F32SYS;
	format_desired.channels = 2;
	format_desired.samples = TARGET_BUFFER_SIZE;
	format_desired.callback = sdl_audio_callback;
	format_desired.userdata = NULL;
	assert(sizeof(float) == 4);

	// Try to open a device using this format
	SDL_AudioSpec format_obtained;
	audio_device = SDL_OpenAudioDevice(NULL, 0, &format_desired, &format_obtained, 0);
	if(!audio_device)
	{
		printf("[Sound] No audio device available\n");
		return false;
	}

	// Set parameters from the actual format
	sample_rate = format_obtained.freq;
	buffer_size = format_obtained.samples;

	// Finally enable output
	SDL_PauseAudioDevice(audio_device, 0);
	printf("[Sound] Using audio device %s\n", SDL_GetAudioDeviceName(audio_device, 0));
	return true;
}

static void sdl_audio_shutdown()
{
	// Close audio device
	if(audio_device) SDL_CloseAudioDevice(audio_device);
}

/* SDL-specific code end */

static void timeref(uint64_t param, int cycles_late);

void initialize(std::vector<uint8_t>& sound_rom)
{
	if(!sound_rom.empty())
	{
		if(!sdl_audio_initialize())
		{
			return;
		}

		sound_engine = std::make_unique<LoopySound::LoopySound>(sound_rom, (float)sample_rate, buffer_size);

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
	sdl_audio_shutdown();
	sound_engine = nullptr;
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
	if(sound_engine)
	{
		sound_engine->set_control_register(value);
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
	if(sound_engine)
	{
		sound_engine->midi_in((char)value);
	}
}

void set_mute(bool mute_in)
{
	mute = mute_in;
	printf("[Sound] %s output\n", mute_in ? "Muted" : "Unmuted");
}

static void timeref(uint64_t param, int cycles_late)
{
	constexpr static int cycles_per_timeref = Timing::F_CPU / TIMEREF_FREQUENCY;
	Timing::UnitCycle timeref_cycles = Timing::convert_cpu(cycles_per_timeref - cycles_late);
	timeref_ev = Timing::add_event(timeref_func, timeref_cycles, 0, Timing::CPU_TIMER);

	constexpr static float timeref_period = 1.f / TIMEREF_FREQUENCY;
	sound_engine->time_reference(timeref_period);
}

static void update_volume_level()
{
	if(MUTE_FADE_MS > 0)
	{
		float delta = 1000.f / (sample_rate * MUTE_FADE_MS);
		if(mute) delta = -delta;
		volume_level += delta;
		volume_level = std::clamp(volume_level, 0.f, 1.f);
	}
	else
	{
		volume_level = mute ? 0.f : 1.f;
	}
}

static void buffer_callback(float* sample_buffer, uint32_t sample_count)
{
	if(sound_engine)
	{
		// Generate samples if we can, updating the mute level every sample
		float tmp[2];
		int p = 0;
		for(uint32_t i = 0; i < sample_count/2; i++)
		{
			update_volume_level();
			sound_engine->gen_sample(tmp);
			sample_buffer[p++] = tmp[0] * volume_level;
			sample_buffer[p++] = tmp[1] * volume_level;
		}
	}
	else
	{
		// If for some reason we can't generate samples, zero the buffer
		for(uint32_t i = 0; i < sample_count; i++)
		{
			sample_buffer[i] = 0.f;
		}
	}
}

}
