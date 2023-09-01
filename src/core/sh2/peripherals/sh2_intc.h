#pragma once
#include <cstdint>

namespace SH2::OCPM::INTC
{

enum class IRQ
{
	NMI,
	UserBreak,

	IRQ0,
	IRQ1,
	IRQ2,
	IRQ3,
	IRQ4,
	IRQ5,
	IRQ6,
	IRQ7,

	DMAC0,
	DMAC1,
	DMAC2,
	DMAC3,

	ITU0,
	ITU1,
	ITU2,
	ITU3,
	ITU4,

	SCI0,
	SCI1,

	PRT,

	WDT,

	REF,

	NumIrq
};

void initialize();

uint16_t read16(uint32_t addr);
void write16(uint32_t addr, uint16_t value);

void assert_irq(IRQ irq, int info);

}