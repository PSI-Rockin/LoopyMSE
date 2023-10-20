#include <fstream>
#include <string>
#include "core/cart.h"
#include "core/memory.h"

namespace Cart
{

struct State
{
	std::vector<uint8_t> rom;
	std::vector<uint8_t> sram;
	std::string sram_file_path;
};

static State state;

static void commit_sram()
{
	std::ofstream file(state.sram_file_path, std::ios::binary);
	file.write((char*)state.sram.data(), state.sram.size());
}

void initialize(Config::CartInfo& info)
{
	state = {};

	state.rom = info.rom;
	state.sram = info.sram;
	state.sram_file_path = info.sram_file_path;

	Memory::map_sh2_pagetable(state.rom.data(), ROM_START, state.rom.size());
	Memory::map_sh2_pagetable(state.sram.data(), SRAM_START, state.sram.size());
}

void shutdown()
{
	commit_sram();
}

void sram_commit_check()
{
	//Force a save every 60 frames
	//TODO: is there a better way of doing this?
	static int frame_count = 0;
	frame_count++;

	if (frame_count < 60)
	{
		return;
	}

	frame_count = 0;
	commit_sram();
}

}