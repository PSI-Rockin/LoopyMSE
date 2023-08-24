#include <memory>
#include "core/memory.h"

namespace Memory
{

//SH2 ignores bits 28-31, so the pagetable size can be reduced
//TODO: instead of reducing size, maybe make the pagetable more granular?
constexpr static int SH2_PAGETABLE_SIZE = (1 << 28) / 4096;

struct State
{
	std::vector<uint8_t*> sh2_pagetable;

	std::vector<uint8_t> cart;

	uint8_t bios[BIOS_SIZE];
	uint8_t ram[RAM_SIZE];
};

std::unique_ptr<State> state;

static void map_pagetable(std::vector<uint8_t*>& table, uint8_t* data, uint32_t start, uint32_t size)
{
	start >>= 12;
	size >>= 12;

	for (unsigned int i = 0; i < size; i++)
	{
		table[start + i] = data + (i << 12);
	}
}

void initialize(std::vector<uint8_t>& bios_rom, std::vector<uint8_t>& cart_rom)
{
	state = std::make_unique<State>();

	memcpy(state->bios, bios_rom.data(), BIOS_SIZE);
	state->cart = cart_rom;

	state->sh2_pagetable.resize(SH2_PAGETABLE_SIZE);
	std::fill(state->sh2_pagetable.begin(), state->sh2_pagetable.end(), nullptr);

	map_pagetable(state->sh2_pagetable, state->bios, BIOS_START, BIOS_SIZE);
	map_pagetable(state->sh2_pagetable, state->ram, RAM_START, RAM_SIZE);
	map_pagetable(state->sh2_pagetable, state->cart.data(), CART_START, state->cart.size());
}

void shutdown()
{
	state = nullptr;
}

uint8_t** get_sh2_pagetable()
{
	return state->sh2_pagetable.data();
}

}