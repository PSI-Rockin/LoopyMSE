#include <cassert>
#include <cstdio>
#include <sound/sound.h>
#include "core/sh2/peripherals/sh2_dmac.h"
#include "core/sh2/peripherals/sh2_serial.h"
#include "core/timing.h"

namespace SH2::OCPM::Serial
{

constexpr static int PORT_COUNT = 2;

static Timing::FuncHandle tx_ev_func;

struct Port
{
	Timing::EventHandle tx_ev;
	DMAC::DREQ rx_dreq_id, tx_dreq_id;

	int id;
	int bit_factor;
	int cycles_per_bit;

	struct Mode
	{
		int clock_factor;
		int mp_enable;
		int stop_bit_length;
		int parity_mode;
		int parity_enable;
		int seven_bit_mode;
		int sync_mode;
	};

	Mode mode;

	struct Ctrl
	{
		int clock_mode;
		int tx_end_intr_enable;
		int mp_intr_enable;
		int rx_enable;
		int tx_enable;
		int rx_intr_enable;
		int tx_intr_enable;
	};

	Ctrl ctrl;

	struct Status
	{
		int tx_empty;
	};

	Status status;

	int tx_bits_left;
	uint8_t tx_shift_reg;
	uint8_t tx_buffer;
	uint8_t tx_prepared_data;

	void (*tx_callback)(uint8_t);

	void calc_cycles_per_bit()
	{
		assert(!mode.sync_mode);
		cycles_per_bit = (32 << (mode.clock_factor * 2)) * (bit_factor + 1);
	}

	void tx_start(uint8_t value)
	{
		tx_bits_left = 8;
		tx_shift_reg = value;
		status.tx_empty = true;
		sched_tx_ev();
	}

	void sched_tx_ev()
	{
		Timing::UnitCycle sched_cycles = Timing::convert_cpu(cycles_per_bit);
		tx_ev = Timing::add_event(tx_ev_func, sched_cycles, (uint64_t)this, Timing::CPU_TIMER);
	}
};

struct State
{
	Port ports[PORT_COUNT];
};

static State state;

static void check_tx_dreqs()
{
	for (auto& port : state.ports)
	{
		if (port.status.tx_empty && port.ctrl.tx_enable)
		{
			DMAC::send_dreq(port.tx_dreq_id);
		}
	}
}

static void tx_event(uint64_t param, int cycles_late)
{
	assert(!cycles_late);
	Port* port = (Port*)param;

	bool bit = port->tx_shift_reg & 0x1;
	port->tx_shift_reg >>= 1;
	port->tx_prepared_data >>= 1;
	port->tx_prepared_data |= bit << 7;
	port->tx_bits_left--;

	if (!port->tx_bits_left)
	{
		printf("[Serial] port%d tx %02X\n", port->id, port->tx_prepared_data);

		if (port->tx_callback != NULL) {
			port->tx_callback(port->tx_prepared_data);
		}

		if (!port->status.tx_empty)
		{
			port->tx_start(port->tx_buffer);
			check_tx_dreqs();
		}
		else
		{
			//TODO: can this trigger an interrupt?
			printf("[Serial] port%d finished tx\n", port->id);
		}
	}
	else
	{
		port->sched_tx_ev();
	}
}

void initialize()
{
	state = {};

	tx_ev_func = Timing::register_func("Serial::tx_event", tx_event);

	for (int i = 0; i < PORT_COUNT; i++)
	{
		state.ports[i].id = i;
		state.ports[i].status.tx_empty = true;
		state.ports[i].calc_cycles_per_bit();
	}

	state.ports[0].rx_dreq_id = DMAC::DREQ::RXI0;
	state.ports[1].rx_dreq_id = DMAC::DREQ::RXI1;
	state.ports[0].tx_dreq_id = DMAC::DREQ::TXI0;
	state.ports[1].tx_dreq_id = DMAC::DREQ::TXI1;
}

uint8_t read8(uint32_t addr)
{
	addr &= 0xF;
	Port* port = &state.ports[addr >> 3];
	int reg = addr & 0x7;

	printf("[Serial] read port%d reg%d\n", port->id, reg);
	return 0;
}

void write8(uint32_t addr, uint8_t value)
{
	addr &= 0xF;
	Port* port = &state.ports[addr >> 3];
	int reg = addr & 0x7;

	switch (reg)
	{
	case 0x00:
		printf("[Serial] write port%d mode: %02X\n", port->id, value);
		port->mode.clock_factor = value & 0x3;
		port->mode.mp_enable = (value >> 2) & 0x1;
		port->mode.stop_bit_length = (value >> 3) & 0x1;
		port->mode.parity_mode = (value >> 4) & 0x1;
		port->mode.parity_enable = (value >> 5) & 0x1;
		port->mode.seven_bit_mode = (value >> 6) & 0x1;
		port->mode.sync_mode = (value >> 7) & 0x1;
		assert(!(value & ~0x3));
		break;
	case 0x01:
		printf("[Serial] write port%d bitrate factor: %02X\n", port->id, value);
		port->bit_factor = value;
		port->calc_cycles_per_bit();
		printf("[Serial] set port%d baudrate: %d bit/s\n", port->id, Timing::F_CPU / port->cycles_per_bit);
		break;
	case 0x02:
		printf("[Serial] write port%d ctrl: %02X\n", port->id, value);
		port->ctrl.clock_mode = value & 0x3;
		port->ctrl.tx_end_intr_enable = (value >> 2) & 0x1;
		port->ctrl.mp_intr_enable = (value >> 3) & 0x1;
		port->ctrl.rx_enable = (value >> 4) & 0x1;
		port->ctrl.tx_enable = (value >> 5) & 0x1;
		port->ctrl.rx_intr_enable = (value >> 6) & 0x1;
		port->ctrl.tx_intr_enable = (value >> 7) & 0x1;

		if (!port->ctrl.tx_enable)
		{
			port->status.tx_empty = true;
		}

		check_tx_dreqs();
		break;
	case 0x03:
		assert(port->status.tx_empty && port->ctrl.tx_enable);

		if (!port->tx_bits_left)
		{
			//Space is available, move the data to the buffer register and start the timed transfer
			port->tx_start(value);
		}
		else
		{
			//Byte transfer is in progress, clear DREQ and store data in the buffer
			port->tx_buffer = value;
			port->status.tx_empty = false;
			DMAC::clear_dreq(port->tx_dreq_id);
		}
		break;
	case 0x04:
		//TODO
		printf("[Serial write port%d status: %02X\n", port->id, value);
		break;
	default:
		assert(0);
	}
}

void set_tx_callback(int port, void (*callback)(uint8_t))
{
	assert(port >= 0 && port < PORT_COUNT);
	state.ports[port].tx_callback = callback;
}

}