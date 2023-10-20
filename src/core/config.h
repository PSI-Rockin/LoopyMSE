#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Config
{

struct CartInfo
{
	std::vector<uint8_t> rom;
	std::vector<uint8_t> sram;
	std::string sram_file_path;
};

struct SystemInfo
{
	CartInfo cart;
	std::vector<uint8_t> bios_rom;
};

}