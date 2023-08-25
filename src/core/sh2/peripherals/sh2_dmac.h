#pragma once
#include <cstdint>

namespace SH2::OCPM::DMAC
{

void initialize();

uint16_t read16(uint32_t addr);

void write16(uint32_t addr, uint16_t value);
void write32(uint32_t addr, uint32_t value);

}