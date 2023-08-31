#pragma once
#include <cstdint>

namespace SH2::OCPM::DMAC
{

enum class DREQ
{
	External,
	Reserved,
	External2,
	External3,

	RXI0,
	TXI0,
	RXI1,
	TXI1,

	IMIA0,
	IMIA1,
	IMIA2,
	IMIA3,

	Auto,
	Reserved2,
	Reserved3,
	Reserved4,

	NumDreq
};

void initialize();

void send_dreq(DREQ dreq);
void clear_dreq(DREQ dreq);

uint16_t read16(uint32_t addr);

void write16(uint32_t addr, uint16_t value);
void write32(uint32_t addr, uint32_t value);

}