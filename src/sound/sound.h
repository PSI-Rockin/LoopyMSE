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

namespace Sound
{

// Target output format. 44100-48000Hz provides good quality.
// SDL converts unsupported formats internally.
// Smaller buffer gives lower latency, larger buffer allows smoother timing with time reference.
// A good compromise buffer size is around 50ms.
constexpr static int TARGET_SAMPLE_RATE = 48000;
constexpr static int TARGET_BUFFER_SIZE = 2048;

// Time reference to smooth out audio timing at larger buffer sizes. Assumes consistent CPU timing.
constexpr static int TIMEREF_FREQUENCY = 100;
constexpr static bool TIMEREF_ENABLE = TIMEREF_FREQUENCY > (TARGET_SAMPLE_RATE / TARGET_BUFFER_SIZE);

// Fade up/down time in milliseconds when sound is muted e.g. by minimizing the window.
constexpr static int MUTE_FADE_MS = 20;

// Audio synthesis parameters in loopysound.h.


void initialize(std::vector<uint8_t>& soundRom);
void shutdown();

constexpr static int CTRL_START = 0x04080000;
constexpr static int CTRL_END = 0x040A0000;

uint8_t ctrl_read8(uint32_t addr);
uint16_t ctrl_read16(uint32_t addr);
uint32_t ctrl_read32(uint32_t addr);
void ctrl_write8(uint32_t addr, uint8_t value);
void ctrl_write16(uint32_t addr, uint16_t value);
void ctrl_write32(uint32_t addr, uint32_t value);

void midi_byte_in(uint8_t value);
void set_mute(bool mute_in);

static void timeref(uint64_t param, int cycles_late);
static void buffer_callback(float* buffer, uint32_t count);

}
