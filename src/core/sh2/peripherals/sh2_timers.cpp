#include <cassert>
#include <cstdio>
#include <tuple>
#include "core/sh2/peripherals/sh2_intc.h"
#include "core/sh2/peripherals/sh2_timers.h"
#include "core/timing.h"

namespace SH2::OCPM::Timer
{

constexpr static int TIMER_COUNT = 5;

static Timing::FuncHandle ev_func;

struct Timer
{
	Timing::EventHandle ev;
	INTC::IRQ irq;
	int enabled;
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

	uint32_t counter;
	uint32_t counter_when_started;
	uint32_t gen_reg[2];

	int64_t time_when_started;

	void update_counter()
	{
		if (!ev.is_valid())
		{
			return;
		}

		assert(!(ctrl.clock & ~0x3));

		int64_t time_elapsed = Timing::get_timestamp(Timing::CPU_TIMER) - time_when_started;
		counter = counter_when_started + (time_elapsed >> ctrl.clock);
		counter &= 0xFFFF;
	}

	void set_enable(bool new_enable)
	{
		enabled = new_enable;

		if (!ev.is_valid() && enabled)
		{
			start();
		}
		else if (ev.is_valid() && !enabled)
		{
			Timing::cancel_event(ev);
		}
	}

	void start()
	{
		assert(!(ctrl.clock & ~0x3));
		assert(!ctrl.edge_mode);
		assert(ctrl.clear_mode != 3);

		//Calculate the target which will take the smallest amount of time to reach
		constexpr static uint32_t OVERFLOW_TARGET = 0x10000;
		uint32_t nearest_target = OVERFLOW_TARGET;
		for (int i = 0; i < 2; i++)
		{
			if (counter < gen_reg[i])
			{
				nearest_target = std::min(nearest_target, gen_reg[i]);
			}
		}

		uint32_t cycles = (nearest_target - counter) << ctrl.clock;
		Timing::UnitCycle sched_cycles = Timing::convert_cpu(cycles);
		ev = Timing::add_event(ev_func, sched_cycles, (uint64_t)this, Timing::CPU_TIMER);

		time_when_started = Timing::get_timestamp(Timing::CPU_TIMER);
		counter_when_started = counter;
	}
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

static void update_timer_irq(Timer* timer)
{
	int subirq = -1;
	for (int i = 0; i < 3; i++)
	{
		if (timer->intr_enable & timer->intr_flag & (1 << i))
		{
			subirq = i;
			break;
		}
	}

	if (subirq >= 0)
	{
		INTC::assert_irq(timer->irq, subirq);
	}
	else
	{
		INTC::deassert_irq(timer->irq);
	}
}

static void update_timer_target(Timer* timer)
{
	// Disable and re-enable to force new timing to take effect
	if (timer->enabled)
	{
		timer->set_enable(false);
		timer->set_enable(true);
	}
}

static void intr_event(uint64_t param, int cycles_late)
{
	assert(!cycles_late);
	Timer* timer = (Timer*)param;

	timer->update_counter();

	bool clear_counter = false;

	//Compare 1
	if (timer->counter == timer->gen_reg[0])
	{
		timer->intr_flag |= 0x1;
		if (timer->ctrl.clear_mode == 0x1)
		{
			clear_counter = true;
		}
	}

	//Compare 2
	if (timer->counter == timer->gen_reg[1])
	{
		assert(0);
	}

	//Overflow
	if (timer->counter == 0)
	{
		assert(0);
	}

	if (clear_counter)
	{
		timer->counter = 0;
	}

	update_timer_irq(timer);

	//Restart the timer
	timer->start();
}

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
	ev_func = {};

	for (int i = 0; i < TIMER_COUNT; i++)
	{
		state.timers[i].id = i;
	}

	state.timers[0].irq = INTC::IRQ::ITU0;
	state.timers[1].irq = INTC::IRQ::ITU1;
	state.timers[2].irq = INTC::IRQ::ITU2;
	state.timers[3].irq = INTC::IRQ::ITU3;
	state.timers[4].irq = INTC::IRQ::ITU4;

	ev_func = Timing::register_func("Timer::intr_event", intr_event);
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
			timer->update_counter();
			timer->ctrl.clock = value & 0x7;
			timer->ctrl.edge_mode = (value >> 3) & 0x3;
			timer->ctrl.clear_mode = (value >> 5) & 0x3;
			update_timer_target(timer);
			break;
		case 0x01:
			printf("[Timer] write timer%d io ctrl: %02X\n", timer->id, value);
			assert(!value);
			break;
		case 0x02:
			printf("[Timer] write timer%d intr enable: %02X\n", timer->id, value);
			timer->intr_enable = value;
			update_timer_irq(timer);
			break;
		case 0x03:
			printf("[Timer] write timer%d intr flag: %02X\n", timer->id, value);
			timer->intr_flag &= value;
			update_timer_irq(timer);
			break;
		case 0x04:
			printf("[Timer] write timer%d counter: %02X**\n", timer->id, value);
			//The BIOS writes 0 to here under the assumption that it resets the whole counter...
			timer->update_counter();
			timer->counter &= 0x00FF;
			timer->counter |= value << 8;
			update_timer_target(timer);
			break;
		case 0x05:
			printf("[Timer] write timer%d counter: **%02X\n", timer->id, value);
			timer->update_counter();
			timer->counter &= 0xFF00;
			timer->counter |= value;
			update_timer_target(timer);
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

		for (int i = 0; i < TIMER_COUNT; i++)
		{
			state.timers[i].set_enable((value >> i) & 0x1);
		}
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
		case 0x04:
			printf("[Timer] write timer%d counter: %04X\n", timer->id, value);
			timer->counter = value;
			update_timer_target(timer);
			break;
		case 0x06:
		case 0x08:
			reg = (reg - 0x06) >> 1;
			printf("[Timer] write timer%d general reg%d: %04X\n", timer->id, reg, value);
			timer->update_counter();
			timer->gen_reg[reg] = value;
			update_timer_target(timer);
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