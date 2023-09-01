#include <cassert>
#include "core/sh2/peripherals/sh2_intc.h"
#include "core/sh2/sh2.h"

namespace SH2::OCPM::INTC
{

struct State
{
	int prios[(int)IRQ::NumIrq];
};

static State state;

static int get_vector_from_irq(IRQ irq, int info)
{
	if (irq >= IRQ::ITU0 && irq <= IRQ::ITU4)
	{
		int irq_index = (int)irq - (int)IRQ::ITU0;
		return 80 + (irq_index * 4) + (info & 0x3);
	}

	assert(0);
	return 0;
}

void initialize()
{
	state = {};
}

uint16_t read16(uint32_t addr)
{
	assert(0);
	return 0;
}

void write16(uint32_t addr, uint16_t value)
{
	addr &= 0xF;

	switch (addr)
	{
	case 0x08:
		state.prios[(int)IRQ::ITU1] = value & 0x0F;
		state.prios[(int)IRQ::ITU0] = (value >> 4) & 0x0F;
		state.prios[(int)IRQ::DMAC2] = state.prios[(int)IRQ::DMAC3] = (value >> 8) & 0x0F;
		state.prios[(int)IRQ::DMAC0] = state.prios[(int)IRQ::DMAC1] = value >> 12;
		break;
	default:
		assert(0);
	}
}

void assert_irq(IRQ irq, int info)
{
	//TODO: if multiple IRQs have been asserted, the INTC should store the IRQs and attempt to send the highest priority one to the CPU
	int vector_id = get_vector_from_irq(irq, info);
	int prio = state.prios[(int)irq];

	SH2::raise_irq(vector_id, prio);
}

}