#pragma once
#include <cstdint>

namespace SH2::OCPM
{

constexpr static int IO_BASE_ADDR = 0x05000000;
constexpr static int IO_END_ADDR = 0x06000000;

constexpr static int ORAM_BASE_ADDR = 0x0F000000;
constexpr static int ORAM_END_ADDR = 0x0F000400;

uint8_t io_read8(uint32_t addr);
uint16_t io_read16(uint32_t addr);
uint32_t io_read32(uint32_t addr);

void io_write8(uint32_t addr, uint8_t value);
void io_write16(uint32_t addr, uint16_t value);
void io_write32(uint32_t addr, uint32_t value);

uint8_t oram_read8(uint32_t addr);
uint16_t oram_read16(uint32_t addr);
uint32_t oram_read32(uint32_t addr);

void oram_write8(uint32_t addr, uint8_t value);
void oram_write16(uint32_t addr, uint16_t value);
void oram_write32(uint32_t addr, uint32_t value);

}