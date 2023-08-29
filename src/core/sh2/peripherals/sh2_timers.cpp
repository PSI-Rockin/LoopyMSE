#include <cassert>
#include <cstdio>
#include <tuple>
#include "core/sh2/peripherals/sh2_timers.h"

namespace SH2::OCPM::Timer
{

constexpr static int TIMER_COUNT = 5;

struct Timer
{
	int id;

	struct Ctrl
	{
		int clock;
		int edge_mode;
		int clear_mode;
	};

	Ctrl ctrl;

	int intr_enable;
	int intr_flag;

	uint16_t counter;
	uint16_t gen_reg[2];
};

struct State
{
	int timer_enable;
	int sync_ctrl;
	int mode;

	Timer timers[TIMER_COUNT];
};

typedef std::tuple<Timer*, int> TimerDev;

static State state;

static TimerDev get_dev_from_addr(uint32_t addr)
{
	addr &= 0x3F;

	//Timers 3 and 4 have extra registers and are also spaced oddly
	if (addr >= 0x32)
	{
		return TimerDev(&state.timers[4], addr - 0x32);
	}

	if (addr >= 0x22 && addr < 0x30)
	{
		return TimerDev(&state.timers[3], addr - 0x22);
	}

	//The remaining timers have predictable spacing
	if (addr >= 0x04 && addr < 0x22)
	{
		addr -= 0x04;
		int id = addr / 0xA;
		int reg = addr % 0xA;
		return TimerDev(&state.timers[id], reg);
	}

	//Shared registers don't have a timer pointer
	return TimerDev(nullptr, addr);
}

void initialize()
{
	state = {};

	for (int i = 0; i < TIMER_COUNT; i++)
	{
		state.timers[i].id = i;
	}
}

uint8_t read8(uint32_t addr)
{
	TimerDev dev = get_dev_from_addr(addr);

	Timer* timer = std::get<Timer*>(dev);
	int reg = std::get<int>(dev);

	if (timer)
	{
		switch (reg)
		{
		case 0x03:
			return timer->intr_flag | 0x78;
		default:
			assert(0);
			return 0;
		}
	}

	switch (reg)
	{
	case 0x00:
		return state.timer_enable | 0x60;
	case 0x01:
		return state.sync_ctrl | 0x60;
	case 0x02:
		return state.mode;
	default:
		assert(0);
		return 0;
	}
}

uint16_t read16(uint32_t addr)
{
	assert(0);
	return 0;
}

void write8(uint32_t addr, uint8_t value)
{
	TimerDev dev = get_dev_from_addr(addr);

	Timer* timer = std::get<Timer*>(dev);
	int reg = std::get<int>(dev);

	if (timer)
	{
		switch (reg)
		{
		case 0x00:
			printf("[Timer] write timer%d ctrl: %02X\n", timer->id, value);
			timer->ctrl.clock = value & 0x7;
			timer->ctrl.edge_mode = (value >> 3) & 0x3;
			timer->ctrl.clear_mode = (value >> 5) & 0x3;

			assert(!timer->ctrl.edge_mode);
			assert(timer->ctrl.clear_mode != 3);
			break;
		case 0x01:
			printf("[Timer] write timer%d io ctrl: %02X\n", timer->id, value);
			assert(!value);
			break;
		case 0x02:
			printf("[Timer] write timer%d intr enable: %02X\n", timer->id, value);
			timer->intr_enable = value;
			break;
		case 0x03:
			printf("[Timer] write timer%d intr flag: %02X\n", timer->id, value);
			timer->intr_flag &= value;
			break;
		case 0x04:
			printf("[Timer] write timer%d counter: %02X\n", timer->id, value);

			//The BIOS writes 0 to here under the assumption that it resets the whole counter...
			assert(!value);
			timer->counter = 0;
			break;
		default:
			assert(0);
		}

		return;
	}

	switch (reg)
	{
	case 0x00:
		printf("[Timer] write master enable: %02X\n", value);
		state.timer_enable = value & 0x1F;
		break;
	case 0x01:
		printf("[Timer] write sync ctrl: %02X\n", value);
		state.sync_ctrl = value & 0x1F;
		assert(!state.sync_ctrl);
		break;
	case 0x02:
		printf("[Timer] write mode: %02X\n", value);
		state.mode = value & 0x7F;
		assert(!state.mode);
		break;
	default:
		assert(0);
	}
}

void write16(uint32_t addr, uint16_t value)
{
	TimerDev dev = get_dev_from_addr(addr);

	Timer* timer = std::get<Timer*>(dev);
	int reg = std::get<int>(dev);

	if (timer)
	{
		switch (reg)
		{
		case 0x06:
		case 0x08:
			reg = (reg - 0x06) >> 1;
			printf("[Timer] write timer%d general reg%d: %04X\n", timer->id, reg, value);
			timer->gen_reg[reg] = value;
			break;
		default:
			assert(0);
		}

		return;
	}

	switch (reg)
	{
	default:
		assert(0);
	}
}

}