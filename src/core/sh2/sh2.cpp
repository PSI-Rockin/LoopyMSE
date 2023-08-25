#include <cassert>
#include <common/bswp.h>
#include "core/sh2/peripherals/sh2_dmac.h"
#include "core/sh2/sh2.h"
#include "core/sh2/sh2_bus.h"
#include "core/sh2/sh2_interpreter.h"
#include "core/sh2/sh2_local.h"
#include "core/memory.h"

namespace SH2
{

CPU sh2;

void initialize()
{
	sh2 = {};

	sh2.pagetable = Memory::get_sh2_pagetable();

	//TODO: set this to a reset vector
	//Add 4 to account for pipelining
	sh2.pc = 0x0E000480 + 4;

	//Set up on-chip peripheral modules after CPU is done
	OCPM::DMAC::initialize();
}

void shutdown()
{
	//nop
}

void run()
{
	uint16_t instr = Bus::read16(sh2.pc - 4);
	SH2::Interpreter::run(instr);
	sh2.pc += 2;
}

}