#include <fstream>
#include <core/config.h>
#include <core/system.h>

int main()
{
	Config::SystemInfo config = {};

	std::ifstream cart_file("D:/nigaoe_artist_be.bin", std::ios::binary);
	if (!cart_file.is_open())
	{
		return 1;
	}

	config.cart_rom.assign(std::istreambuf_iterator<char>(cart_file), {});
	cart_file.close();

	std::ifstream bios_file("D:/loopy_bios.bin", std::ios::binary);
	if (!bios_file.is_open())
	{
		return 1;
	}

	config.bios_rom.assign(std::istreambuf_iterator<char>(bios_file), {});
	bios_file.close();

	System::initialize(config);
	System::run();
	System::shutdown();

	return 0;
}
