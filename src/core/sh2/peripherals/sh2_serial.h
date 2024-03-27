#pragma once
#include <cstdint>

namespace SH2::OCPM::Serial
{

void initialize();

uint8_t read8(uint32_t addr);

void write8(uint32_t addr, uint8_t value);

void set_tx_callback(int port, void (*callback)(uint8_t));

}