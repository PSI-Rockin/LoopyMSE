#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <common/bswp.h>
#include <core/memory.h>
#include <core/timing.h>
#include "video/render.h"
#include "video/vdp_local.h"
#include "video/video.h"

namespace Video
{

static Timing::FuncHandle vcount_func;
static Timing::EventHandle vcount_ev;

VDP vdp;

struct DumpHeader
{
	uint32_t addr;
	uint32_t length;
	uint32_t data_width;
};

static void output_bmp()
{
	std::ofstream bmp_file("emu_output.bmp", std::ios::binary);

	const char* SIGNATURE = "BM";
	bmp_file.write(SIGNATURE, 2);

	constexpr static int DATA_SIZE = (DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
	uint32_t file_size = DATA_SIZE + 0x36;
	bmp_file.write((char*)&file_size, 4);

	uint32_t reserved = 0;
	bmp_file.write((char*)&reserved, 4);

	uint32_t data_offs = 0x36;
	bmp_file.write((char*)&data_offs, 4);

	uint32_t info_size = 0x28;
	bmp_file.write((char*)&info_size, 4);

	bmp_file.write((char*)&DISPLAY_WIDTH, 4);
	bmp_file.write((char*)&DISPLAY_HEIGHT, 4);

	uint16_t planes = 1;
	bmp_file.write((char*)&planes, 2);

	uint16_t bpp = 16;
	bmp_file.write((char*)&bpp, 2);

	uint32_t compression = 0;
	bmp_file.write((char*)&compression, 4);
	bmp_file.write((char*)&compression, 4);
	bmp_file.write((char*)&compression, 4);
	bmp_file.write((char*)&compression, 4);
	bmp_file.write((char*)&compression, 4);
	bmp_file.write((char*)&compression, 4);

	for (int y = 0; y < DISPLAY_HEIGHT; y++)
	{
		bmp_file.write((char*)vdp.display_output[DISPLAY_HEIGHT - y - 1], DISPLAY_WIDTH * 2);
	}
}

static void inc_vcount(uint64_t param, int cycles_late)
{
	if (vdp.vcount < DISPLAY_HEIGHT)
	{
		Renderer::draw_scanline(vdp.vcount);
	}

	vdp.vcount++;
	
	//Once we go past the visible region, enter VSYNC
	constexpr static int VSYNC_START = 0x1D9;
	if (vdp.vcount == DISPLAY_HEIGHT)
	{
		printf("[Video] VSYNC start\n");
		vdp.vcount = VSYNC_START;
		output_bmp();
		dump_for_serial();
	}

	//At the end of VSYNC, wrap around to the start of the visible region
	constexpr static int VSYNC_END = 0x200;
	if (vdp.vcount == VSYNC_END)
	{
		printf("[Video] VSYNC end\n");
		vdp.vcount = 0;
	}

	constexpr static int LINES_PER_FRAME = DISPLAY_HEIGHT + (VSYNC_END - VSYNC_START);

	constexpr static int CYCLES_PER_FRAME = Timing::F_CPU / 60;
	constexpr static int CYCLES_PER_LINE = CYCLES_PER_FRAME / LINES_PER_FRAME;

	Timing::UnitCycle sched_cycles = Timing::convert_cpu(CYCLES_PER_LINE - cycles_late);
	vcount_ev = Timing::add_event(vcount_func, sched_cycles, 0, Timing::CPU_TIMER);
}

static void dump_serial_region(std::ofstream& dump, uint8_t* mem, uint32_t addr, uint32_t length)
{
	DumpHeader header;
	header.addr = Common::bswp32(addr | (1 << 27)); //Make sure the address is 16-bit for the CPU
	header.length = Common::bswp32(length);
	header.data_width = Common::bswp32(2);

	dump.write((char*)&header, sizeof(header));
	dump.write((char*)mem, length);
}

void initialize()
{
	vdp = {};

	vcount_func = Timing::register_func("Video::inc_vcount", inc_vcount);

	//Kickstart the VCOUNT event
	inc_vcount(0, 0);

	//Map VRAM to the CPU
	Memory::map_sh2_pagetable(vdp.bitmap, BITMAP_VRAM_START, BITMAP_VRAM_SIZE);
	Memory::map_sh2_pagetable(vdp.tile, TILE_VRAM_START, TILE_VRAM_SIZE);
}

void shutdown()
{
	// nop
}

void dump_for_serial()
{
	std::ofstream dump("emudump.bin", std::ios::binary);
	const char* MAGIC = "LPSTATE\0";

	dump.write(MAGIC, 8);

	dump_serial_region(dump, vdp.bitmap, BITMAP_VRAM_START, BITMAP_VRAM_SIZE);
	dump_serial_region(dump, vdp.tile, TILE_VRAM_START, TILE_VRAM_SIZE);
	dump_serial_region(dump, vdp.palette, PALETTE_START, PALETTE_SIZE);
	dump_serial_region(dump, vdp.oam, OAM_START, OAM_SIZE);

	//TODO: dump MMIO
}

uint8_t palette_read8(uint32_t addr)
{
	return vdp.palette[addr & 0x1FF];
}

uint16_t palette_read16(uint32_t addr)
{
	uint16_t value;
	memcpy(&value, &vdp.palette[addr & 0x1FF], 2);
	return Common::bswp16(value);
}

uint32_t palette_read32(uint32_t addr)
{
	uint32_t value;
	memcpy(&value, &vdp.palette[addr & 0x1FF], 4);
	return Common::bswp32(value);
}

void palette_write8(uint32_t addr, uint8_t value)
{
	vdp.palette[addr & 0x1FF] = value;
}

void palette_write16(uint32_t addr, uint16_t value)
{
	value = Common::bswp16(value);
	memcpy(&vdp.palette[addr & 0x1FF], &value, 2);
}

void palette_write32(uint32_t addr, uint32_t value)
{
	value = Common::bswp32(value);
	memcpy(&vdp.palette[addr & 0x1FF], &value, 4);
}

uint8_t oam_read8(uint32_t addr)
{
	assert(0);
	return 0;
}

uint16_t oam_read16(uint32_t addr)
{
	uint16_t value;
	memcpy(&value, &vdp.oam[addr & 0x1FF], 2);
	return Common::bswp16(value);
}

uint32_t oam_read32(uint32_t addr)
{
	uint32_t value;
	memcpy(&value, &vdp.oam[addr & 0x1FF], 4);
	return Common::bswp32(value);
}

void oam_write8(uint32_t addr, uint8_t value)
{
	assert(0);
}

void oam_write16(uint32_t addr, uint16_t value)
{
	value = Common::bswp16(value);
	memcpy(&vdp.oam[addr & 0x1FF], &value, 2);
}

void oam_write32(uint32_t addr, uint32_t value)
{
	value = Common::bswp32(value);
	memcpy(&vdp.oam[addr & 0x1FF], &value, 4);
}

uint8_t capture_read8(uint32_t addr)
{
	assert(0);
	return 0;
}

uint16_t capture_read16(uint32_t addr)
{
	addr &= 0x1FF;
	uint16_t value;
	memcpy(&value, &vdp.capture_buffer[addr], 2);
	return Common::bswp16(value);
}

uint32_t capture_read32(uint32_t addr)
{
	assert(0);
	return 0;
}

void capture_write8(uint32_t addr, uint8_t value)
{
	assert(0);
}

void capture_write16(uint32_t addr, uint16_t value)
{
	assert(0);
}

void capture_write32(uint32_t addr, uint32_t value)
{
	assert(0);
}

uint8_t bitmap_reg_read8(uint32_t addr)
{
	assert(0);
	return 0;
}

uint16_t bitmap_reg_read16(uint32_t addr)
{
	addr &= 0xFFF;
	
	int index = (addr >> 1) & 0x3;
	auto layer = &vdp.bitmap_regs[index];
	int reg = addr & ~0x7;

	switch (reg)
	{
	case 0x000:
		return layer->scrollx;
	case 0x008:
		return layer->scrolly;
	case 0x010:
		return layer->screenx;
	case 0x018:
		return layer->screeny;
	case 0x020:
		return layer->w | (layer->clipx << 8);
	case 0x028:
		return layer->h;
	case 0x030:
		return vdp.bitmap_030;
	case 0x040:
		return vdp.bitmap_040;
	case 0x050:
		return layer->unk;
	default:
		assert(0);
		return 0;
	}
}

uint32_t bitmap_reg_read32(uint32_t addr)
{
	assert(0);
	return 0;
}

void bitmap_reg_write8(uint32_t addr, uint8_t value)
{
	assert(0);
}

void bitmap_reg_write16(uint32_t addr, uint16_t value)
{
	addr &= 0xFFF;

	int index = (addr >> 1) & 0x3;
	auto layer = &vdp.bitmap_regs[index];
	int reg = addr & ~0x7;

	switch (reg)
	{
	case 0x000:
		printf("[Video] write BM%d_SCROLLX: %04X\n", index, value);
		layer->scrollx = value & 0xFF;
		break;
	case 0x008:
		printf("[Video] write BM%d_SCROLLY: %04X\n", index, value);
		layer->scrolly = value & 0x1FF;
		break;
	case 0x010:
		printf("[Video] write BM%d_SCREENX: %04X\n", index, value);
		layer->screenx = value & 0x1FF;
		break;
	case 0x018:
		printf("[Video] write BM%d_SCREENY: %04X\n", index, value);
		layer->screeny = value & 0x1FF;
		break;
	case 0x020:
		printf("[Video] write BM%d_CLIPWIDTH: %04X\n", index, value);
		layer->w = value & 0xFF;
		layer->clipx = value >> 8;
		break;
	case 0x028:
		printf("[Video] write BM%d_HEIGHT: %04X\n", index, value);
		layer->h = value & 0xFF;
		break;
	case 0x030:
		printf("[Video] write bitmap 030: %04X\n", value);
		vdp.bitmap_030 = value;
		break;
	case 0x040:
		printf("[Video] write bitmap 040: %04X\n", value);
		vdp.bitmap_040 = value;
		break;
	case 0x050:
		printf("[Video] write bitmap%d 050: %04X\n", index, value);
		layer->unk = value;
		break;
	default:
		assert(0);
	}
}

void bitmap_reg_write32(uint32_t addr, uint32_t value)
{
	assert(0);
}

uint8_t ctrl_read8(uint32_t addr)
{
	assert(0);
	return 0;
}

uint16_t ctrl_read16(uint32_t addr)
{
	addr &= 0xFFF;
	switch (addr)
	{
	case 0x000:
		printf("[Video] read ctrl 000\n");
		return 0;
	case 0x004:
		return vdp.vcount;
	default:
		assert(0);
		return 0;
	}
}

uint32_t ctrl_read32(uint32_t addr)
{
	assert(0);
	return 0;
}

void ctrl_write8(uint32_t addr, uint8_t value)
{
	assert(0);
}

void ctrl_write16(uint32_t addr, uint16_t value)
{
	addr &= 0xFFF;
	switch (addr)
	{
	case 0x000:
		printf("[Video] write ctrl 000: %04X\n", value);
		break;
	case 0x006:
		if (value & 0x01)
		{
			vdp.capture_enable = true;
		}

		//Bit 0 turns on display capture, only log writes to other bits for now
		if (value != 0x01)
		{
			printf("[Video] write ctrl 006: %04X\n", value);
		}
		break;
	default:
		assert(0);
	}
}

void ctrl_write32(uint32_t addr, uint32_t value)
{
	assert(0);
}

uint8_t bgobj_read8(uint32_t addr)
{
	assert(0);
	return 0;
}

uint16_t bgobj_read16(uint32_t addr)
{
	addr &= 0xFFF;
	switch (addr)
	{
	case 0x000:
		return vdp.bg_format;
	case 0x002:
		return vdp.bg_scrollx[0];
	case 0x004:
		return vdp.bg_scrolly[0];
	case 0x006:
		return vdp.bg_scrollx[1];
	case 0x008:
		return vdp.bg_scrolly[1];
	case 0x00A:
		return vdp.bg_palsel[0];
	case 0x00C:
		return vdp.bg_palsel[1];
	case 0x020:
		return vdp.bg_tileoffs;
	default:
		assert(0);
		return 0;
	}
}

uint32_t bgobj_read32(uint32_t addr)
{
	assert(0);
	return 0;
}

void bgobj_write8(uint32_t addr, uint8_t value)
{
	assert(0);
}

void bgobj_write16(uint32_t addr, uint16_t value)
{
	addr &= 0xFFF;
	switch (addr)
	{
	case 0x000:
		printf("[Video] write BG_FORMAT: %04X\n", value);
		vdp.bg_format = value;
		break;
	case 0x002:
	case 0x006:
	{
		int index = (addr - 0x002) >> 2;
		printf("[Video] write BG%d_SCROLLX: %04X\n", index, value);
		vdp.bg_scrollx[index] = value & 0xFF;
		break;
	}
	case 0x004:
	case 0x008:
	{
		int index = (addr - 0x004) >> 2;
		printf("[Video] write BG%d_SCROLLY: %04X\n", index, value);
		vdp.bg_scrolly[index] = value & 0x1FF;
		break;
	}
	case 0x00A:
	case 0x00C:
	{
		int index = (addr - 0x00A) >> 1;
		printf("[Video] write BG%d_PALSEL: %04X\n", index, value);
		vdp.bg_palsel[index] = value;
		break;
	}
	case 0x020:
		printf("[Video] write BG_TILEOFFS: %04X\n", value);
		vdp.bg_tileoffs = value;
		break;
	default:
		assert(0);
	}
}

void bgobj_write32(uint32_t addr, uint32_t value)
{
	assert(0);
}

uint8_t display_read8(uint32_t addr)
{
	assert(0);
	return 0;
}

uint16_t display_read16(uint32_t addr)
{
	addr &= 0xFFF;
	switch (addr)
	{
	case 0x000:
		return vdp.display_000;
	case 0x002:
	{
		uint16_t result = 0;

		for (int i = 0; i < 2; i++)
		{
			result |= vdp.layer_ctrl.bg_enable[i] << i;
			result |= vdp.layer_ctrl.obj_enable[i] << (i + 6);
		}

		for (int i = 0; i < 4; i++)
		{
			result |= vdp.layer_ctrl.bitmap_enable[i] << (i + 2);
		}

		result |= vdp.layer_ctrl.unk << 8;
		return result;
	}
	case 0x004:
		return vdp.display_004;
	case 0x006:
		return vdp.master_brightness;
	case 0x008:
		return vdp.display_008;
	default:
		assert(0);
		return 0;
	}
}

uint32_t display_read32(uint32_t addr)
{
	assert(0);
	return 0;
}

void display_write8(uint32_t addr, uint8_t value)
{
	assert(0);
}

void display_write16(uint32_t addr, uint16_t value)
{
	addr &= 0xFFF;
	switch (addr)
	{
	case 0x000:
		vdp.display_000 = value;
		printf("[Video] write display 000: %04X\n", value);
		break;
	case 0x002:
		for (int i = 0; i < 2; i++)
		{
			vdp.layer_ctrl.bg_enable[i] = (value >> i) & 0x1;
			vdp.layer_ctrl.obj_enable[i] = (value >> (i + 6)) & 0x1;
		}

		for (int i = 0; i < 4; i++)
		{
			vdp.layer_ctrl.bitmap_enable[i] = (value >> (i + 2)) & 0x1;
		}

		vdp.layer_ctrl.unk = value >> 8;
		printf("[Video] write layer ctrl: %04X\n", value);
		break;
	case 0x004:
		vdp.display_004 = value;
		printf("[Video] write display 004: %04X\n", value);
		break;
	case 0x006:
		vdp.master_brightness = value;
		printf("[Video] write master brightness: %04X\n", value);
		break;
	case 0x008:
		vdp.display_008 = value;
		printf("[Video] write display 008: %04X\n", value);
		break;
	case 0x00A:
		vdp.capture_ctrl.scanline = value & 0xFF;
		vdp.capture_ctrl.format = (value >> 8) & 0x3;
		break;
	default:
		assert(0);
	}
}

void display_write32(uint32_t addr, uint32_t value)
{
	assert(0);
}

uint8_t dma_ctrl_read8(uint32_t addr)
{
	assert(0);
	return 0;
}

uint16_t dma_ctrl_read16(uint32_t addr)
{
	addr &= 0xFFF;
	switch (addr)
	{
	case 0x002:
		return vdp.dma_mask;
	case 0x004:
		return vdp.dma_value;
	default:
		assert(0);
		return 0;
	}
}

uint32_t dma_ctrl_read32(uint32_t addr)
{
	assert(0);
	return 0;
}

void dma_ctrl_write8(uint32_t addr, uint8_t value)
{
	assert(0);
}

void dma_ctrl_write16(uint32_t addr, uint16_t value)
{
	addr &= 0xFFF;
	switch (addr)
	{
	case 0x000:
		printf("[Video] write dma ctrl 000: %04X\n", value);
		break;
	case 0x002:
		//TODO: what does bit 8 do? Seems to have no effect in HW tests at this time
		vdp.dma_mask = value & 0x1FF;
		break;
	case 0x004:
		vdp.dma_value = value & 0xFF;
		break;
	default:
		assert(0);
	}
}

void dma_ctrl_write32(uint32_t addr, uint32_t value)
{
	assert(0);
}

uint8_t dma_read8(uint32_t addr)
{
	assert(0);
	return 0;
}

uint16_t dma_read16(uint32_t addr)
{
	assert(0);
	return 0;
}

uint32_t dma_read32(uint32_t addr)
{
	assert(0);
	return 0;
}

void dma_write8(uint32_t addr, uint8_t value)
{
	assert(0);
}

void dma_write16(uint32_t addr, uint16_t value)
{
	//Value written doesn't matter, it always triggers this
	//TODO: how long does this take? Is the CPU stalled?
	addr &= 0x3FF;

	int y = addr >> 1;
	for (int x = 0; x < DISPLAY_WIDTH; x++)
	{
		uint32_t addr = x + (y * DISPLAY_WIDTH);
		uint8_t data = vdp.bitmap[addr];
		data &= ~vdp.dma_mask;
		data |= vdp.dma_value & vdp.dma_mask;
		vdp.bitmap[addr] = data;
	}
}

void dma_write32(uint32_t addr, uint32_t value)
{
	assert(0);
}

}