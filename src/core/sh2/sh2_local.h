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

	int32_t cycles_left;

	uint8_t** pagetable;
};

extern CPU sh2;

}