#include <cassert>
#include <cstdio>
#include <cstring>
#include "video/vdp_local.h"
#include "video/video.h"

namespace Video
{

VDP vdp;

static void inc_vcount()
{
	vdp.vcount++;
	
	//Once we go past the visible region, enter VSYNC
	//TODO: is 0x1D8 the correct starting value?
	if (vdp.vcount == 0x0E0)
	{
		vdp.vcount = 0x1D8;
	}

	//At the end of VSYNC, wrap around to the start of the visible region
	if (vdp.vcount == 0x200)
	{
		vdp.vcount = 0;
	}
}

void initialize()
{
	vdp = {};
}

void shutdown()
{
	// nop
}

uint8_t palette_read8(uint32_t addr)
{
	return vdp.palette[addr & 0x1FF];
}

uint16_t palette_read16(uint32_t addr)
{
	uint16_t value;
	memcpy(&value, &vdp.palette[addr & 0x1FF], 2);
	return value;
}

uint32_t palette_read32(uint32_t addr)
{
	uint32_t value;
	memcpy(&value, &vdp.palette[addr & 0x1FF], 4);
	return value;
}

void palette_write8(uint32_t addr, uint8_t value)
{
	vdp.palette[addr & 0x1FF] = value;
}

void palette_write16(uint32_t addr, uint16_t value)
{
	memcpy(&vdp.palette[addr & 0x1FF], &value, 2);
}

void palette_write32(uint32_t addr, uint32_t value)
{
	memcpy(&vdp.palette[addr & 0x1FF], &value, 4);
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
	return value;
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
		return layer->x;
	case 0x008:
		return layer->y;
	case 0x010:
		return layer->unk1;
	case 0x018:
		return layer->unk2;
	case 0x020:
		return layer->w | (layer->unk3 << 8);
	case 0x028:
		return layer->h;
	case 0x030:
		return vdp.bitmap_030;
	case 0x040:
		return vdp.bitmap_040;
	case 0x050:
		return layer->unk4;
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
		printf("[VDP] write bitmap%d x: %04X\n", index, value);
		layer->x = value;
		break;
	case 0x008:
		printf("[VDP] write bitmap%d y: %04X\n", index, value);
		layer->y = value;
		break;
	case 0x010:
		printf("[VDP] write bitmap%d 010: %04X\n", index, value);
		layer->unk1 = value;
		break;
	case 0x018:
		printf("[VDP] write bitmap%d 018: %04X\n", index, value);
		layer->unk2 = value;
		break;
	case 0x020:
		printf("[VDP] write bitmap%d w+unk: %04X\n", index, value);
		layer->w = value & 0xFF;
		layer->unk3 = value >> 8;
		break;
	case 0x028:
		printf("[VDP] write bitmap%d h: %04X\n", index, value);
		layer->h = value;
		break;
	case 0x030:
		printf("[VDP] write bitmap 030: %04X\n", value);
		vdp.bitmap_030 = value;
		break;
	case 0x040:
		printf("[VDP] write bitmap 040: %04X\n", value);
		vdp.bitmap_040 = value;
		break;
	case 0x050:
		printf("[VDP] write bitmap%d 050: %04X\n", index, value);
		layer->unk4 = value;
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
	{
		uint16_t vcount = vdp.vcount;

		//FIXME: Dirty hack, should be part of a future scheduler
		inc_vcount();
		return vcount;
	}
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
			//TODO: display capture
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

}