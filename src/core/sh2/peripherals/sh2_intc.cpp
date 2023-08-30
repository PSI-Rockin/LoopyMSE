#include <cassert>
#include "core/sh2/peripherals/sh2_intc.h"
#include "core/sh2/sh2.h"

namespace SH2::OCPM::INTC
{

struct State
{

};

static State state;

void initialize()
{
	state = {};
}

void assert_irq(IRQ irq, int info)
{
	assert(irq == IRQ::ITU0);

	SH2::raise_exception(80 + info);
}

}