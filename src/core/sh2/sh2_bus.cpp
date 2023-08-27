#include <cstdio>
#include <cstring>
#include <fstream>
#include <common/bswp.h>
#include <video/video.h>
#include "core/sh2/peripherals/sh2_ocpm.h"
#include "core/sh2/sh2_bus.h"
#include "core/sh2/sh2_local.h"

namespace SH2::Bus
{

static uint32_t translate_addr(uint32_t addr)
{
	//Bits 28-31 are always ignored
	//The on-chip region (bits 24-27 == 0xF) is NOT mirrored - all other regions are mirrored
	if ((addr & 0x0F000000) != 0x0F000000)
	{
		return addr & ~0xF8000000;
	}

	return addr & ~0xF0000000;
}

#define MMIO_ACCESS(access, ...)											\
	if (addr >= Video::PALETTE_START && addr < Video::PALETTE_END)			\
		return Video::palette_##access(__VA_ARGS__);						\
	if (addr >= Video::CAPTURE_START && addr < Video::CAPTURE_END)			\
		return Video::capture_##access(__VA_ARGS__);						\
	if (addr >= Video::CTRL_REG_START && addr < Video::CTRL_REG_END)		\
		return Video::ctrl_##access(__VA_ARGS__);							\
	if (addr >= Video::BITMAP_REG_START && addr < Video::BITMAP_REG_END)	\
		return Video::bitmap_reg_##access(__VA_ARGS__);						\
	if (addr >= Video::DISPLAY_REG_START && addr < Video::DISPLAY_REG_END)	\
		return Video::display_##access(__VA_ARGS__);						\
	if (addr >= OCPM::BASE_ADDR && addr < OCPM::END_ADDR)					\
		return OCPM::##access(__VA_ARGS__);									\
	return unmapped_##access(__VA_ARGS__);

uint8_t unmapped_read8(uint32_t addr)
{
	printf("[SH2] unmapped read8 %08X\n", addr);
	return 0;
}

uint16_t unmapped_read16(uint32_t addr)
{
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
	addr = translate_addr(addr);
	uint8_t* mem = sh2.pagetable[addr >> 12];
	if (mem)
	{
		return mem[addr & 0xFFF];
	}
	
	MMIO_ACCESS(read8, addr);
}

uint16_t read16(uint32_t addr)
{
	addr = translate_addr(addr);
	uint8_t* mem = sh2.pagetable[addr >> 12];
	if (mem)
	{
		uint16_t value;
		memcpy(&value, mem + (addr & 0xFFF), 2);
		return Common::bswp16(value);
	}

	MMIO_ACCESS(read16, addr);
}

uint32_t read32(uint32_t addr)
{
	addr = translate_addr(addr);
	uint8_t* mem = sh2.pagetable[addr >> 12];
	if (mem)
	{
		uint32_t value;
		memcpy(&value, mem + (addr & 0xFFF), 4);
		return Common::bswp32(value);
	}

	MMIO_ACCESS(read32, addr);
}

void write8(uint32_t addr, uint8_t value)
{
	addr = translate_addr(addr);
	uint8_t* mem = sh2.pagetable[addr >> 12];
	if (mem)
	{
		mem[addr & 0xFFF] = value;
		return;
	}

	MMIO_ACCESS(write8, addr, value);
}

void write16(uint32_t addr, uint16_t value)
{
	addr = translate_addr(addr);
	uint8_t* mem = sh2.pagetable[addr >> 12];
	if (mem)
	{
		value = Common::bswp16(value);
		memcpy(mem + (addr & 0xFFF), &value, 2);
		return;
	}
	MMIO_ACCESS(write16, addr, value);
}

void write32(uint32_t addr, uint32_t value)
{
	addr = translate_addr(addr);
	uint8_t* mem = sh2.pagetable[addr >> 12];
	if (mem)
	{
		value = Common::bswp32(value);
		memcpy(mem + (addr & 0xFFF), &value, 4);
		return;
	}
	MMIO_ACCESS(write32, addr, value);
}

}