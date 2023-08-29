#include <cstdio>
#include "core/sh2/peripherals/sh2_dmac.h"
#include "core/sh2/peripherals/sh2_ocpm.h"
#include "core/sh2/peripherals/sh2_timers.h"

namespace SH2::OCPM
{

constexpr static int TIMER_START = 0xF00;
constexpr static int TIMER_END = 0xF40;

constexpr static int DMAC_START = 0xF40;
constexpr static int DMAC_END = 0xF80;

uint8_t read8(uint32_t addr)
{
	addr = (addr & 0x1FF) + 0xE00;

	if (addr >= TIMER_START && addr < TIMER_END)
	{
		return Timer::read8(addr);
	}

	switch (addr)
	{
	default:
		printf("[OCPM] read8 %08X\n", addr);
		return 0;
	}
}

uint16_t read16(uint32_t addr)
{
	addr = (addr & 0x1FF) + 0xE00;

	if (addr >= TIMER_START && addr < TIMER_END)
	{
		return Timer::read16(addr);
	}

	if (addr >= DMAC_START && addr < DMAC_END)
	{
		return DMAC::read16(addr);
	}

	switch (addr)
	{
	default:
		printf("[OCPM] read16 %08X\n", addr);
		return 0;
	}
}

uint32_t read32(uint32_t addr)
{
	addr = (addr & 0x1FF) + 0xE00;
	switch (addr)
	{
	default:
		printf("[OCPM] read32 %08X\n", addr);
		return 0;
	}
}

void write8(uint32_t addr, uint8_t value)
{
	addr = (addr & 0x1FF) + 0xE00;
	if (addr >= TIMER_START && addr < TIMER_END)
	{
		Timer::write8(addr, value);
		return;
	}

	switch (addr)
	{
	default:
		printf("[OCPM] write8 %08X: %02X\n", addr, value);
	}
}

void write16(uint32_t addr, uint16_t value)
{
	addr = (addr & 0x1FF) + 0xE00;
	if (addr >= TIMER_START && addr < TIMER_END)
	{
		Timer::write16(addr, value);
		return;
	}

	if (addr >= DMAC_START && addr < DMAC_END)
	{
		DMAC::write16(addr, value);
		return;
	}

	switch (addr)
	{
	default:
		printf("[OCPM] write16 %08X: %04X\n", addr, value);
	}
}

void write32(uint32_t addr, uint32_t value)
{
	addr = (addr & 0x1FF) + 0xE00;
	if (addr >= DMAC_START && addr < DMAC_END)
	{
		DMAC::write32(addr, value);
		return;
	}
	switch (addr)
	{
	default:
		printf("[OCPM] write32 %08X: %08X\n", addr, value);
	}
}

}