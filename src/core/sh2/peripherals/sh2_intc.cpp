#include <cassert>
#include "core/sh2/peripherals/sh2_intc.h"
#include "core/sh2/sh2_local.h"

namespace SH2::OCPM::INTC
{

struct State
{
	uint32_t vectors[(int)IRQ::NumIrq];
	int prios[(int)IRQ::NumIrq];

	int pending_irqs[(int)IRQ::NumIrq];
	int irq_offs[(int)IRQ::NumIrq];
};

static State state;

static void send_irq_signal()
{
	int vector = 0;
	int highest_prio = 0;

	for (int id = 0; id < (int)IRQ::NumIrq; id++)
	{
		if (state.pending_irqs[id])
		{
			if (state.prios[id] > highest_prio)
			{
				highest_prio = state.prios[id];
				vector = state.vectors[id] + state.irq_offs[id];
			}
		}
	}

	SH2::assert_irq(vector, highest_prio);
}

void initialize()
{
	state = {};

	//NMI and UserBreak have fixed priorities, everything else is configurable
	state.prios[(int)IRQ::NMI] = 16;
	state.prios[(int)IRQ::UserBreak] = 15;

	state.vectors[(int)IRQ::NMI] = 11;
	state.vectors[(int)IRQ::UserBreak] = 12;

	for (int i = 0; i < 8; i++)
	{
		int id = (int)IRQ::IRQ0;
		state.vectors[id + i] = 64 + i;
	}

	for (int i = 0; i < 4; i++)
	{
		int id = (int)IRQ::DMAC0;
		state.vectors[id + i] = 72 + (i * 4);
	}

	for (int i = 0; i < 5; i++)
	{
		int id = (int)IRQ::ITU0;
		state.vectors[id + i] = 80 + (i * 4);
	}

	for (int i = 0; i < 2; i++)
	{
		int id = (int)IRQ::SCI0;
		state.vectors[id + i] = 100 + (i * 4);
	}

	//TODO: remaining interrupts
}

uint16_t read16(uint32_t addr)
{
	addr &= 0xF;

	switch (addr)
	{
	case 0x04:
	{
		// IPRA
		uint16_t result = state.prios[(int)IRQ::IRQ0] << 12;
		result |= state.prios[(int)IRQ::IRQ1] << 8;
		result |= state.prios[(int)IRQ::IRQ2] << 4;
		result |= state.prios[(int)IRQ::IRQ3];
		return result;
	}
	case 0x06:
	{
		// IPRB
		uint16_t result = state.prios[(int)IRQ::IRQ4] << 12;
		result |= state.prios[(int)IRQ::IRQ5] << 8;
		result |= state.prios[(int)IRQ::IRQ6] << 4;
		result |= state.prios[(int)IRQ::IRQ7];
		return result;
	}
	case 0x08:
	{
		// IPRC
		uint16_t result =  state.prios[(int)IRQ::DMAC0] << 12;
		result |= state.prios[(int)IRQ::DMAC2] << 8;
		result |= state.prios[(int)IRQ::ITU0] << 4;
		result |= state.prios[(int)IRQ::ITU1];
		return result;
	}
	case 0x0A:
	{
		// IPRD
		uint16_t result = state.prios[(int)IRQ::ITU2] << 12;
		result |= state.prios[(int)IRQ::ITU3] << 8;
		result |= state.prios[(int)IRQ::ITU4] << 4;
		result |= state.prios[(int)IRQ::SCI0];
		return result;
	}
	case 0x0C:
	{
		// IPRE
		uint16_t result = state.prios[(int)IRQ::SCI1] << 12;
		result |= state.prios[(int)IRQ::PRT] << 8;
		result |= state.prios[(int)IRQ::WDT] << 4;
		return result;
	}
	default:
		assert(0);
		return 0;
	}
}

uint8_t read8(uint32_t addr)
{
	uint8_t result;
	if ((addr & 1) == 0)
	{
		// Read the first (high) byte
		result = read16(addr) >> 8;
	}
	else
	{
		// Read the second (low) byte
		result = read16(addr - 1) & 0xFF;
	}
	return result;
}

void write16(uint32_t addr, uint16_t value)
{
	addr &= 0xF;

	switch (addr)
	{
	case 0x04:
		// IPRA
		state.prios[(int)IRQ::IRQ0] = value >> 12;
		state.prios[(int)IRQ::IRQ1] = (value >> 8) & 0x0F;
		state.prios[(int)IRQ::IRQ2] = (value >> 4) & 0x0F;
		state.prios[(int)IRQ::IRQ3] = value & 0x0F;
		break;
	case 0x06:
		// IPRB
		state.prios[(int)IRQ::IRQ4] = value >> 12;
		state.prios[(int)IRQ::IRQ5] = (value >> 8) & 0x0F;
		state.prios[(int)IRQ::IRQ6] = (value >> 4) & 0x0F;
		state.prios[(int)IRQ::IRQ7] = value & 0x0F;
		break;
	case 0x08:
		// IPRC
		state.prios[(int)IRQ::DMAC0] = state.prios[(int)IRQ::DMAC1] = value >> 12;
		state.prios[(int)IRQ::DMAC2] = state.prios[(int)IRQ::DMAC3] = (value >> 8) & 0x0F;
		state.prios[(int)IRQ::ITU0] = (value >> 4) & 0x0F;
		state.prios[(int)IRQ::ITU1] = value & 0x0F;
		break;
	case 0x0A:
		// IPRD
		state.prios[(int)IRQ::ITU2] = value >> 12;
		state.prios[(int)IRQ::ITU3] = (value >> 8) & 0x0F;
		state.prios[(int)IRQ::ITU4] = (value >> 4) & 0x0F;
		state.prios[(int)IRQ::SCI0] = value & 0x0F;
		break;
	case 0x0C:
		// IPRE
		state.prios[(int)IRQ::SCI1] = value >> 12;
		state.prios[(int)IRQ::PRT] = (value >> 8) & 0x0F;
		state.prios[(int)IRQ::WDT] = state.prios[(int)IRQ::REF] = (value >> 4) & 0x0F;
		break;
	default:
		assert(0);
	}
}

void write8(uint32_t addr, uint8_t value)
{
	uint16_t tmp;
	if ((addr & 1) == 0)
	{
		// Write the first (high) byte by masking
		tmp = read16(addr) & 0x00FF;
		tmp |= value << 8;
		write16(addr, tmp);
	}
	else
	{
		// Write the second (low) byte by masking
		tmp = read16(addr - 1) & 0xFF00;
		tmp |= value;
		write16(addr - 1, tmp);
	}
}

void assert_irq(IRQ irq, int vector_offs)
{
	state.pending_irqs[(int)irq] = true;
	state.irq_offs[(int)irq] = vector_offs;
	send_irq_signal();
}

void deassert_irq(IRQ irq)
{
	state.pending_irqs[(int)irq] = false;
	send_irq_signal();
}

}