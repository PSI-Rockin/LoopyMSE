#pragma once
#include <cstdint>

namespace SH2::OCPM
{

constexpr static int BASE_ADDR = 0x05000000;
constexpr static int END_ADDR = 0x06000000;

uint8_t io_read8(uint32_t addr);
uint16_t io_read16(uint32_t addr);
uint32_t io_read32(uint32_t addr);

void io_write8(uint32_t addr, uint8_t value);
void io_write16(uint32_t addr, uint16_t value);
void io_write32(uint32_t addr, uint32_t value);

}