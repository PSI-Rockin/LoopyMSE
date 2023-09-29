#include <sdl/sdl.h>
#include <video/video.h>
#include "core/sh2/sh2.h"
#include "core/memory.h"
#include "core/system.h"
#include "core/timing.h"

namespace System
{

void initialize(Config::SystemInfo& config)
{
	//Memory must initialize first
	Memory::initialize(config.bios_rom, config.cart_rom);

	//Ensure that timing initializes before any CPUs
	Timing::initialize();

	SH2::initialize();

	//Initialize subprojects after everything else
	SDL::initialize();
	Video::initialize();
}

void shutdown()
{
	//Shutdown all components in the reverse order they were initialized
	Video::shutdown();
	SDL::shutdown();

	SH2::shutdown();

	Timing::shutdown();
	Memory::shutdown();
}

void run()
{
	while (true)
	{
		//TODO: if multiple cores are added, ensure that they are relatively synced

		//Calculate the smallest timeslice between all cores
		int64_t slice_length = (std::numeric_limits<int64_t>::max)();
		for (int i = 0; i < Timing::NUM_TIMERS; i++)
		{
			slice_length = std::min(slice_length, Timing::calc_slice_length(i));
		}

		//Run all cores, processing any scheduler events that happen for them
		for (int i = 0; i < Timing::NUM_TIMERS; i++)
		{
			Timing::process_slice(i, slice_length);
		}
	}
}

}