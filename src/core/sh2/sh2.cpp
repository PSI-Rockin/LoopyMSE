#include <cassert>
#include "core/sh2/sh2.h"
#include "core/sh2/sh2_bus.h"
#include "core/sh2/sh2_interpreter.h"
#include "core/sh2/sh2_local.h"
#include "core/memory.h"

namespace SH2
{

CPU sh2;

//TODO: move these functions to a common module
static uint16_t bswp16(uint16_t value)
{
	return (value >> 8) | (value << 8);
}

uint32_t bswp32(uint32_t value)
{
	return (value >> 24) |
		(((value >> 16) & 0xFF) << 8) |
		(((value >> 8) & 0xFF) << 16) |
		(value << 24);
}

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

void initialize()
{
	sh2 = {};

	sh2.pagetable = Memory::get_sh2_pagetable();

	//TODO: set this to a reset vector
	//Add 4 to account for pipelining
	sh2.pc = 0x0E000480 + 4;
}

void shutdown()
{
	//nop
}

void run()
{
	uint16_t instr = read16(sh2.pc - 4);
	SH2::Interpreter::run(instr);
	sh2.pc += 2;
}

uint8_t read8(uint32_t addr)
{
	addr = translate_addr(addr);
	uint8_t* mem = sh2.pagetable[addr >> 12];
	if (!mem)
	{
		return Bus::read8(addr);
	}

	return mem[addr & 0xFFF];
}

uint16_t read16(uint32_t addr)
{
	addr = translate_addr(addr);
	uint8_t* mem = sh2.pagetable[addr >> 12];
	if (!mem)
	{
		return Bus::read16(addr);
	}

	uint16_t value;
	memcpy(&value, mem + (addr & 0xFFF), 2);
	return bswp16(value);
}

uint32_t read32(uint32_t addr)
{
	addr = translate_addr(addr);
	uint8_t* mem = sh2.pagetable[addr >> 12];
	if (!mem)
	{
		return Bus::read32(addr);
	}

	uint32_t value;
	memcpy(&value, mem + (addr & 0xFFF), 4);
	return bswp32(value);
}

void write8(uint32_t addr, uint8_t value)
{
	addr = translate_addr(addr);
	uint8_t* mem = sh2.pagetable[addr >> 12];
	if (!mem)
	{
		Bus::write8(addr, value);
		return;
	}

	mem[addr & 0xFFF] = value;
}

void write16(uint32_t addr, uint16_t value)
{
	addr = translate_addr(addr);
	uint8_t* mem = sh2.pagetable[addr >> 12];
	if (!mem)
	{
		Bus::write16(addr, value);
		return;
	}

	value = bswp16(value);
	memcpy(mem + (addr & 0xFFF), &value, 2);
}

void write32(uint32_t addr, uint32_t value)
{
	addr = translate_addr(addr);
	uint8_t* mem = sh2.pagetable[addr >> 12];
	if (!mem)
	{
		Bus::write32(addr, value);
		return;
	}

	value = bswp32(value);
	memcpy(mem + (addr & 0xFFF), &value, 4);
}

}