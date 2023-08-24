#pragma once
#include <cstdint>
#include <vector>

namespace Config
{

struct SystemInfo
{
	std::vector<uint8_t> bios_rom;
	std::vector<uint8_t> cart_rom;
};

}