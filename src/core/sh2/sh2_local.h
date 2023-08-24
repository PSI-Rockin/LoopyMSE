#pragma once
#include <cstdint>

namespace SH2
{

struct CPU
{
	uint32_t gpr[16];
	uint32_t pc;
	uint32_t pr;
	uint32_t macl, mach;
	uint32_t gbr, vbr;
	uint32_t sr;

	uint8_t** pagetable;
};

extern CPU sh2;

uint8_t read8(uint32_t addr);
uint16_t read16(uint32_t addr);
uint32_t read32(uint32_t addr);

void write8(uint32_t addr, uint8_t value);
void write16(uint32_t addr, uint16_t value);
void write32(uint32_t addr, uint32_t value);

}