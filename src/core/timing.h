#pragma once
#include <functional>
#include <string>
#include <limits>
#include <cstdint>

namespace Timing
{

enum TimerId
{
	CPU_TIMER,
	NUM_TIMERS,
	INVALID_TIMER
};

typedef std::function<void()> TimerFunc;
typedef std::function<void(uint64_t, int)> EventFunc;

/* Represents a registered function with a name. */
struct FuncHandle
{
	int value;

	FuncHandle() { value = -1; }
	bool is_valid() { return value >= 0; }
};

/* Represents a scheduled event for a particular core. */
struct EventHandle
{
	int64_t value;

	EventHandle() { value = -1; }
	bool is_valid() { return value >= 0; }

	int get_timer_id() { return value & 0xFF; }
	int64_t get_ev_id() { return value >> 8; }
};

//The clockrate of the CPU is exactly 16 MHz
constexpr static int F_CPU = 16 * 1000 * 1000;

//Maximum amount of time alloted to a slice
//TODO: make this bigger?
constexpr static int64_t MAX_SLICE_LENGTH = 512;

constexpr static int64_t MAX_TIMESTAMP = (std::numeric_limits<int64_t>::max)();

/* A scheduler cycle - a unit cycle is in units of the CPU's clockrate. */
enum class UnitCycle : int64_t;

void initialize();
void shutdown();

void register_timer(TimerId id, int32_t* cycle_count, TimerFunc func);

FuncHandle register_func(std::string name, EventFunc func);

EventHandle add_event(FuncHandle func, UnitCycle cycles, uint64_t param = 0, int core = -1);
void cancel_event(EventHandle& handle);

void process_slice(int id, int32_t slice);
int64_t calc_slice_length(int id);

int64_t get_timestamp(int id = -1);

UnitCycle convert_cpu(int64_t cycles);

template <int FREQ> UnitCycle convert(int64_t num)
{
	/* Check for overflow */
	int64_t max_value = MAX_TIMESTAMP / FREQ;

	if (num / FREQ > max_value)
	{
		/* Multiplication not possible, return largest possible value */
		return (UnitCycle)MAX_TIMESTAMP;
	}

	if (num > max_value)
	{
		/* Round down to prevent overflow */
		return (UnitCycle)((num / FREQ) * F_CPU);
	}

	return (UnitCycle)(num * F_CPU / FREQ);
}

}