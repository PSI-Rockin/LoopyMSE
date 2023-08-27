#pragma once
#include <cstdint>

namespace SH2::OCPM
{

constexpr static int BASE_ADDR = 0x05000000;
constexpr static int END_ADDR = 0x06000000;

uint8_t read8(uint32_t addr);
uint16_t read16(uint32_t addr);
uint32_t read32(uint32_t addr);

void write8(uint32_t addr, uint8_t value);
void write16(uint32_t addr, uint16_t value);
void write32(uint32_t addr, uint32_t value);

}