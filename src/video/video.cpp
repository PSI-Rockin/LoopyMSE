#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <common/bswp.h>
#include <core/sh2/peripherals/sh2_intc.h>
#include <core/memory.h>
#include <core/timing.h>
#include "video/render.h"
#include "video/vdp_local.h"
#include "video/video.h"

namespace Video
{

static Timing::FuncHandle vcount_func, hsync_func;
static Timing::EventHandle vcount_ev, hsync_ev;

VDP vdp;

constexpr static int LINES_PER_FRAME = 263;

struct DumpHeader
{
	uint32_t addr;
	uint32_t length;
	uint32_t data_width;
};

static void dump_bmp(std::string name, std::unique_ptr<uint16_t[]>& data)
{
	std::ofstream bmp_file(name + ".bmp", std::ios::binary);

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
		int flipped_y = DISPLAY_HEIGHT - y - 1;
		bmp_file.write((char*)(data.get() + flipped_y * DISPLAY_WIDTH), DISPLAY_WIDTH * 2);
	}
}

static void dump_all_bmps()
{
	for (int i = 0; i < 4; i++)
	{
		char num = '0' + i;

		std::string bitmap_name = "output_bitmap";
		dump_bmp(bitmap_name + num, vdp.bitmap_output[i]);
	}

	for (int i = 0; i < 2; i++)
	{
		char num = '0' + i;

		std::string bg_name = "output_bg";
		dump_bmp(bg_name + num, vdp.bg_output[i]);

		std::string screen_name = "output_screen_";
		screen_name += (i == 1) ? 'B' : 'A';
		dump_bmp(screen_name, vdp.screen_output[i]);

		std::string obj_name = "output_obj";
		dump_bmp(obj_name + num, vdp.obj_output[i]);
	}

	dump_bmp("output_display", vdp.display_output);
}

static void start_hsync(uint64_t param, int cycles_late)
{
	vdp.hcount |= 0x100;
}

static void vsync_start()
{
	printf("[Video] VSYNC start\n");

	//This is kinda weird, but when the VDP enters VSYNC, the total number of scanlines is subtracted from VCOUNT
	//Think of the VSYNC lines as being negative
	vdp.vcount = (vdp.vcount - LINES_PER_FRAME) & 0x1FF;
	vdp.frame_ended = true;

	//NMI is triggered on VSYNC
	if (vdp.cmp_irq_ctrl.nmi_enable)
	{
		//TODO: is there a cleaner way to do this?
		auto irq_id = SH2::OCPM::INTC::IRQ::NMI;
		SH2::OCPM::INTC::assert_irq(irq_id, 0);
		SH2::OCPM::INTC::deassert_irq(irq_id);
	}

	dump_bmp("output_display", vdp.display_output);
	//dump_all_bmps();
	//dump_for_serial();
}

static void inc_vcount(uint64_t param, int cycles_late)
{
	//Leave HSYNC
	vdp.hcount &= ~0x100;
	if (vdp.vcount < DISPLAY_HEIGHT)
	{
		Renderer::draw_scanline(vdp.vcount);
	}

	vdp.vcount++;
	
	//Once we go past the visible region, enter VSYNC
	if (vdp.vcount == DISPLAY_HEIGHT)
	{
		vsync_start();
	}

	//At the end of VSYNC, wrap around to the start of the visible region
	constexpr static int VSYNC_END = 0x200;
	if (vdp.vcount == VSYNC_END)
	{
		printf("[Video] VSYNC end\n");
		vdp.vcount = 0;
	}

	constexpr static int CYCLES_PER_FRAME = Timing::F_CPU / 60;
	constexpr static int CYCLES_PER_LINE = CYCLES_PER_FRAME / LINES_PER_FRAME;
	constexpr static int CYCLES_UNTIL_HSYNC = (CYCLES_PER_LINE * 256.0f) / 341.25f;

	Timing::UnitCycle scanline_cycles = Timing::convert_cpu(CYCLES_PER_LINE - cycles_late);
	vcount_ev = Timing::add_event(vcount_func, scanline_cycles, 0, Timing::CPU_TIMER);

	Timing::UnitCycle hsync_cycles = Timing::convert_cpu(CYCLES_UNTIL_HSYNC - cycles_late);
	hsync_ev = Timing::add_event(hsync_func, hsync_cycles, 0, Timing::CPU_TIMER);
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

	//Initialize output buffers
	for (int i = 0; i < 2; i++)
	{
		vdp.bg_output[i] = std::make_unique<uint16_t[]>(DISPLAY_WIDTH * DISPLAY_HEIGHT);
		vdp.obj_output[i] = std::make_unique<uint16_t[]>(DISPLAY_WIDTH * DISPLAY_HEIGHT);
		vdp.screen_output[i] = std::make_unique<uint16_t[]>(DISPLAY_WIDTH * DISPLAY_HEIGHT);
	}

	//Set all OBJs to invisible
	for (int i = 0; i < OAM_SIZE; i += 4)
	{
		oam_write32(i, 0x200);
	}

	for (int i = 0; i < 4; i++)
	{
		vdp.bitmap_output[i] = std::make_unique<uint16_t[]>(DISPLAY_WIDTH * DISPLAY_HEIGHT);
	}

	vdp.display_output = std::make_unique<uint16_t[]>(DISPLAY_WIDTH * DISPLAY_HEIGHT);

	//Map VRAM to the CPU
	//Bitmap VRAM is mirrored
	Memory::map_sh2_pagetable(vdp.bitmap, BITMAP_VRAM_START, BITMAP_VRAM_SIZE);
	Memory::map_sh2_pagetable(vdp.bitmap, BITMAP_VRAM_START + BITMAP_VRAM_SIZE, BITMAP_VRAM_SIZE);
	Memory::map_sh2_pagetable(vdp.tile, TILE_VRAM_START, TILE_VRAM_SIZE);

	vcount_func = Timing::register_func("Video::inc_vcount", inc_vcount);
	hsync_func = Timing::register_func("Video::start_hsync", start_hsync);

	//Kickstart the VCOUNT event
	inc_vcount(0, 0);
}

void shutdown()
{
	// nop
}

void start_frame()
{
	vdp.frame_ended = false;
}

bool check_frame_end()
{
	return vdp.frame_ended;
}

uint16_t* get_display_output()
{
	return vdp.display_output.get();
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
		return vdp.bitmap_ctrl;
	case 0x040:
		return vdp.bitmap_palsel;
	case 0x050:
		return layer->buffer_ctrl;
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
		layer->scrollx = value & 0x1FF;
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
		printf("[Video] write BM_CTRL: %04X\n", value);
		vdp.bitmap_ctrl = value;
		break;
	case 0x040:
		printf("[Video] write BM_PALSEL: %04X\n", value);
		vdp.bitmap_palsel = value;
		break;
	case 0x050:
		printf("[Video] write BM%d_BUFFER_CTRL: %04X\n", index, value);
		layer->buffer_ctrl = value;
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
	case 0x002:
		//FIXME: This only reflects HSYNC status, it doesn't actually return the horizontal counter
		return vdp.hcount;
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
	addr &= 0xFFF;
	switch (addr)
	{
	case 0x20:
		return vdp.tilebase;
	default:
		assert(0);
		return 0;
	}
}

uint16_t bgobj_read16(uint32_t addr)
{
	addr &= 0xFFF;
	switch (addr)
	{
	case 0x000:
	{
		uint16_t result = vdp.bg_ctrl.shared_maps;
		result |= vdp.bg_ctrl.map_size << 1;
		result |= vdp.bg_ctrl.bg0_8bit << 3;
		result |= vdp.bg_ctrl.tile_size1 << 4;
		result |= vdp.bg_ctrl.tile_size0 << 6;
		return result;
	}
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
	case 0x010:
	{
		uint16_t result = vdp.obj_ctrl.id_offs;
		result |= vdp.obj_ctrl.tile_index_offs[1] << 8;
		result |= vdp.obj_ctrl.tile_index_offs[0] << 11;
		result |= vdp.obj_ctrl.is_8bit << 14;
		return result;
	}
	case 0x012:
		return vdp.obj_palsel[0];
	case 0x014:
		return vdp.obj_palsel[1];
	case 0x020:
		return vdp.tilebase;
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
		printf("[Video] write BG_CTRL: %04X\n", value);
		vdp.bg_ctrl.shared_maps = value & 0x1;
		vdp.bg_ctrl.map_size = (value >> 1) & 0x3;
		vdp.bg_ctrl.bg0_8bit = (value >> 3) & 0x1;

		//Note the reversed order!
		vdp.bg_ctrl.tile_size1 = (value >> 4) & 0x3;
		vdp.bg_ctrl.tile_size0 = (value >> 6) & 0x3;
		break;
	case 0x002:
	case 0x006:
	{
		int index = (addr - 0x002) >> 2;
		printf("[Video] write BG%d_SCROLLX: %04X\n", index, value);
		vdp.bg_scrollx[index] = value & 0xFFF;
		break;
	}
	case 0x004:
	case 0x008:
	{
		int index = (addr - 0x004) >> 2;
		printf("[Video] write BG%d_SCROLLY: %04X\n", index, value);
		vdp.bg_scrolly[index] = value & 0xFFF;
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
	case 0x010:
		printf("[Video] write OBJ_CTRL: %04X\n", value);
		vdp.obj_ctrl.id_offs = value & 0xFF;

		//Note the reversed order!
		vdp.obj_ctrl.tile_index_offs[1] = (value >> 8) & 0x7;
		vdp.obj_ctrl.tile_index_offs[0] = (value >> 11) & 0x7;
		vdp.obj_ctrl.is_8bit = (value >> 14) & 0x1;
		break;
	case 0x012:
	case 0x014:
	{
		int index = (addr - 0x012) >> 1;
		printf("[Video] write OBJ%d_PALSEL: %04X\n", index, value);
		vdp.obj_palsel[index] = value;
		break;
	}
	case 0x020:
		printf("[Video] write TILEBASE: %04X\n", value);
		vdp.tilebase = value & 0xFF;
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
		return vdp.dispmode;
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

		result |= vdp.layer_ctrl.bitmap_screen_mode[0] << 8;
		result |= vdp.layer_ctrl.bitmap_screen_mode[1] << 10;
		result |= vdp.layer_ctrl.obj_screen_mode[0] << 12;
		result |= vdp.layer_ctrl.obj_screen_mode[1] << 14;
		return result;
	}
	case 0x004:
	{
		uint16_t result = vdp.color_prio.prio_mode;
		result |= vdp.color_prio.screen_b_backdrop_only << 4;
		result |= vdp.color_prio.output_screen_b << 5;
		result |= vdp.color_prio.output_screen_a << 6;
		result |= vdp.color_prio.blend_mode << 7;
		return result;
	}
	case 0x006:
		//Note the reversed order!
		return vdp.backdrops[1];
	case 0x008:
		return vdp.backdrops[0];
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
		vdp.dispmode = value & 0x7;
		printf("[Video] write DISPMODE: %04X\n", value);
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

		vdp.layer_ctrl.bitmap_screen_mode[0] = (value >> 8) & 0x3;
		vdp.layer_ctrl.bitmap_screen_mode[1] = (value >> 10) & 0x3;
		vdp.layer_ctrl.obj_screen_mode[0] = (value >> 12) & 0x3;
		vdp.layer_ctrl.obj_screen_mode[1] = value >> 14;
		printf("[Video] write LAYER_CTRL: %04X\n", value);
		break;
	case 0x004:
		vdp.color_prio.prio_mode = value & 0xF;
		vdp.color_prio.screen_b_backdrop_only = (value >> 4) & 0x1;
		vdp.color_prio.output_screen_b = (value >> 5) & 0x1;
		vdp.color_prio.output_screen_a = (value >> 6) & 0x1;
		vdp.color_prio.blend_mode = (value >> 7) & 0x1;
		printf("[Video] write COLORPRIO: %04X\n", value);
		break;
	case 0x006:
		//Note the reversed order!
		vdp.backdrops[1] = value;
		break;
	case 0x008:
		vdp.backdrops[0] = value;
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

uint8_t irq_read8(uint32_t addr)
{
	assert(0);
	return 0;
}

uint16_t irq_read16(uint32_t addr)
{
	addr &= 0xFFF;
	
	switch (addr)
	{
	case 0x002:
		return vdp.irq0_hcmp;
	case 0x004:
		return vdp.irq0_vcmp;
	default:
		assert(0);
		return 0;
	}
}

uint32_t irq_read32(uint32_t addr)
{
	assert(0);
	return 0;
}

void irq_write8(uint32_t addr, uint8_t value)
{
	assert(0);
}

void irq_write16(uint32_t addr, uint16_t value)
{
	addr &= 0xFFF;

	switch (addr)
	{
	case 0x000:
		//TODO: emulate IRQ0, a screen position compare interrupt (no game uses it, but homebrew might)
		vdp.cmp_irq_ctrl.irq0_enable = (value >> 1) & 0x1;
		vdp.cmp_irq_ctrl.nmi_enable = (value >> 2) & 0x1;
		vdp.cmp_irq_ctrl.use_vcmp = (value >> 5) & 0x1;
		vdp.cmp_irq_ctrl.irq0_enable2 = (value >> 7) & 0x1;
		printf("[VDP] write CMP_IRQ_CTRL: %04X\n", value);
		break;
	case 0x002:
		vdp.irq0_hcmp = value & 0x1FF;
		break;
	case 0x004:
		vdp.irq0_vcmp = value & 0x1FF;
		break;
	}
}

void irq_write32(uint32_t addr, uint32_t value)
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