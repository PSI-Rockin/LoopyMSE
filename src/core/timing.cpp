#include <algorithm>
#include <cassert>
#include "core/timing.h"

namespace Timing
{

struct RegisteredFunc
{
	std::string name;
	EventFunc func;
};

struct Event
{
	int64_t exec_time;
	uint64_t param;
	EventFunc func;
	int64_t id;

	friend bool operator>(const Event& l, const Event& r);
};

struct Timer
{
	int64_t timestamp;
	int64_t next_event_id;
	int32_t slice_length;
	int32_t* cycles_left;
	std::vector<Event> events;
	TimerFunc func;
	int id;
	bool in_slice;

	int64_t get_timestamp()
	{
		int64_t result = timestamp;
		if (in_slice)
		{
			result += slice_length - (int64_t)get_cycles_left();
		}
		return result;
	}

	int32_t get_cycles_left()
	{
		return *cycles_left;
	}

	void set_cycles_left(int32_t sched_cycles)
	{
		*cycles_left = sched_cycles;
	}
};

struct State
{
	Timer* cur_timer;
	std::vector<RegisteredFunc> funcs;
	std::vector<Timer> timers;
};

static State state;

static bool operator>(const Event& l, const Event& r)
{
	return l.exec_time > r.exec_time;
}

static Timer* get_timer(int id)
{
	if (id < 0)
	{
		return state.cur_timer;
	}

	assert(id < state.timers.size());
	return &state.timers[id];
}

static void process_events()
{
	Timer* timer = state.cur_timer;

	int32_t cycles_executed = timer->slice_length - timer->get_cycles_left();
	timer->timestamp += cycles_executed;
	timer->slice_length = 0;
	timer->set_cycles_left(0);

	timer->in_slice = false;

	while (!timer->events.empty() && timer->events.front().exec_time <= timer->get_timestamp())
	{
		Event ev = std::move(timer->events.front());
		std::pop_heap(timer->events.begin(), timer->events.end(), std::greater<>());
		timer->events.pop_back();

		int cycles_late = timer->timestamp - ev.exec_time;
		ev.func(ev.param, cycles_late);
	}
}

static void set_cur_timer(int id, int32_t slice)
{
	assert(id >= 0);
	Timer* timer = get_timer(id);

	timer->slice_length = slice;
	timer->set_cycles_left(slice);
	timer->in_slice = true;

	state.cur_timer = timer;
}

void initialize()
{
	state = {};

	state.timers = std::vector<Timer>(NUM_TIMERS);
}

void shutdown()
{
	state = {};
}

void register_timer(TimerId id, int32_t* cycle_count, TimerFunc func)
{
	//Ensure new timers are registered only during initialization
	assert(!state.cur_timer);
	assert(cycle_count);
	assert(id < NUM_TIMERS);

	state.timers[id].cycles_left = cycle_count;
	state.timers[id].id = id;
	state.timers[id].func = func;
}

FuncHandle register_func(std::string name, EventFunc func)
{
	RegisteredFunc reg = { name, func };
	state.funcs.push_back(reg);

	FuncHandle handle;
	handle.value = state.funcs.size() - 1;

	return handle;
}

EventHandle add_event(FuncHandle func, UnitCycle cycles, uint64_t param, int core)
{
	assert(func.is_valid());

	Timer* timer = get_timer(core);

	RegisteredFunc* reg_func = &state.funcs[func.value];
	Event ev;
	ev.func = reg_func->func;
	ev.param = param;
	ev.id = (timer->next_event_id << 8) | timer->id;
	timer->next_event_id++;

	int64_t raw_cycles = (int64_t)cycles;
	ev.exec_time = timer->get_timestamp() + raw_cycles;

	int32_t raw_cycles_left = timer->get_cycles_left();
	if (timer->in_slice && raw_cycles < raw_cycles_left && timer == state.cur_timer)
	{
		//If the event is scheduled during a slice and should occur before the slice ends, adjust the slice length
		timer->slice_length -= raw_cycles_left - raw_cycles;
		timer->set_cycles_left(raw_cycles);
	}

	timer->events.push_back(ev);
	std::push_heap(timer->events.begin(), timer->events.end(), std::greater<>());

	EventHandle handle;
	handle.value = ev.id;
	return handle;
}

void cancel_event(EventHandle& ev)
{
	assert(ev.is_valid());

	Timer* timer = get_timer(ev.get_timer_id());

	bool event_found = false;
	for (auto it = timer->events.begin(); it != timer->events.end(); it++)
	{
		if (it->id == ev.value)
		{
			event_found = true;
			timer->events.erase(it);
			std::make_heap(timer->events.begin(), timer->events.end(), std::greater<>());
			break;
		}
	}

	assert(event_found);

	//Indicate that the handle is now invalid
	ev.value = -1;
}

void process_slice(int id, int32_t slice)
{
	set_cur_timer(id, slice);
	state.cur_timer->func();
	process_events();
}

int64_t calc_slice_length(int id)
{
	Timer* timer = get_timer(id);

	if (!timer->events.size())
	{
		return MAX_SLICE_LENGTH;
	}

	int64_t next_event_delta = timer->events.front().exec_time - timer->get_timestamp();
	int64_t slice_length = std::min(MAX_SLICE_LENGTH, next_event_delta);

	return slice_length;
}

int64_t get_timestamp(int id)
{
	Timer* timer = get_timer(id);

	return timer->get_timestamp();
}

UnitCycle convert_cpu(int64_t cycles)
{
	return convert<F_CPU>(cycles);
}

}