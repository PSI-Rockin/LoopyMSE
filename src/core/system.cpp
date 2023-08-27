#include <video/video.h>
#include "core/sh2/sh2.h"
#include "core/memory.h"
#include "core/system.h"

namespace System
{

void initialize(Config::SystemInfo& config)
{
	//Memory must initialize first
	Memory::initialize(config.bios_rom, config.cart_rom);

	SH2::initialize();

	//Initialize subprojects after everything else
	Video::initialize();
}

void shutdown()
{
	//Shutdown all components in the reverse order they were initialized
	Video::shutdown();

	SH2::shutdown();

	Memory::shutdown();
}

void run()
{
	while (true)
	{
		SH2::run();
	}
}

}