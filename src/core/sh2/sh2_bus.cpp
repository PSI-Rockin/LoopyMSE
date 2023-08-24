#include <cstdio>
#include "core/sh2/sh2_bus.h"

namespace SH2::Bus
{

//TODO: Get rid of this FILTHY hack
static bool is_vblank = false;

#define HANDLE_ACCESS(access, ...)										\
	return unmapped_##access(__VA_ARGS__);

uint8_t unmapped_read8(uint32_t addr)
{
	printf("[SH2] unmapped read8 %08X\n", addr);
	return 0;
}

uint16_t unmapped_read16(uint32_t addr)
{
	if (addr == 0x04058004)
	{
		is_vblank = !is_vblank;
		return is_vblank << 8;
	}
	printf("[SH2] unmapped read16 %08X\n", addr);
	return 0;
}

uint32_t unmapped_read32(uint32_t addr)
{
	printf("[SH2] unmapped read32 %08X\n", addr);
	return 0;
}

void unmapped_write8(uint32_t addr, uint8_t value)
{
	printf("[SH2] unmapped write8 %08X: %02X\n", addr, value);
}

void unmapped_write16(uint32_t addr, uint16_t value)
{
	printf("[SH2] unmapped write16 %08X: %04X\n", addr, value);
}

void unmapped_write32(uint32_t addr, uint32_t value)
{
	printf("[SH2] unmapped write32 %08X: %08X\n", addr, value);
}

uint8_t read8(uint32_t addr)
{
	HANDLE_ACCESS(read8, addr);
}

uint16_t read16(uint32_t addr)
{
	HANDLE_ACCESS(read16, addr);
}

uint32_t read32(uint32_t addr)
{
	HANDLE_ACCESS(read32, addr);
}

void write8(uint32_t addr, uint8_t value)
{
	HANDLE_ACCESS(write8, addr, value);
}

void write16(uint32_t addr, uint16_t value)
{
	HANDLE_ACCESS(write16, addr, value);
}

void write32(uint32_t addr, uint32_t value)
{
	HANDLE_ACCESS(write32, addr, value);
}

}