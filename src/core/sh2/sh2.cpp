#include <algorithm>
#include <cassert>
#include <common/bswp.h>
#include "core/sh2/peripherals/sh2_dmac.h"
#include "core/sh2/peripherals/sh2_intc.h"
#include "core/sh2/peripherals/sh2_serial.h"
#include "core/sh2/peripherals/sh2_timers.h"
#include "core/sh2/sh2.h"
#include "core/sh2/sh2_bus.h"
#include "core/sh2/sh2_interpreter.h"
#include "core/sh2/sh2_local.h"
#include "core/memory.h"
#include "core/timing.h"

namespace SH2
{

CPU sh2;

void initialize()
{
	sh2 = {};

	sh2.pagetable = Memory::get_sh2_pagetable();

	//TODO: set this to a reset vector
	set_pc(0x0E000480);

	Timing::register_timer(Timing::CPU_TIMER, &sh2.cycles_left, run);

	//Set up on-chip peripheral modules after CPU is done
	OCPM::DMAC::initialize();
	OCPM::INTC::initialize();
	OCPM::Serial::initialize();
	OCPM::Timer::initialize();
}

void shutdown()
{
	//nop
}

void run()
{
	while (sh2.cycles_left)
	{
		uint16_t instr = Bus::read16(sh2.pc - 4);
		SH2::Interpreter::run(instr);
		sh2.cycles_left--;
		sh2.pc += 2;
	}
}

void raise_irq(int vector_id, int prio)
{
	int imask = (sh2.sr >> 4) & 0xF;
	assert(prio > imask);

	raise_exception(vector_id);

	//Interrupt mask should only be modified after the above function so that the original value can be pushed onto the stack
	prio = std::clamp(prio, 0, 15);
	sh2.sr &= ~0xF0;
	sh2.sr |= prio << 4;
}

void raise_exception(int vector_id)
{
	assert(vector_id >= 0x40 && vector_id < 0x100);

	//Push SR and PC onto the stack
	sh2.gpr[15] -= 4;
	Bus::write32(sh2.gpr[15], sh2.sr);
	sh2.gpr[15] -= 4;
	Bus::write32(sh2.gpr[15], sh2.pc - 4);

	uint32_t vector_addr = sh2.vbr + (vector_id * 4);
	uint32_t new_pc = Bus::read32(vector_addr);

	set_pc(new_pc);
}

void set_pc(uint32_t new_pc)
{
	//Needs to be + 4 to account for pipelining
	sh2.pc = new_pc + 4;
}

}