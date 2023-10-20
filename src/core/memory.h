#pragma once
#include <cstdint>
#include <vector>

namespace Memory
{

constexpr static int BIOS_START = 0x00000000;
constexpr static int BIOS_SIZE = 0x8000;

constexpr static int RAM_START = 0x01000000;
constexpr static int RAM_SIZE = 0x80000;

constexpr static int MMIO_START = 0x05000000;

void initialize(std::vector<uint8_t>& bios_rom);
void shutdown();

void map_sh2_pagetable(uint8_t* data, uint32_t start, uint32_t size);
uint8_t** get_sh2_pagetable();

}