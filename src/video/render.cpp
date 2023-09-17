#include <cassert>
#include <cstring>
#include <common/bswp.h>
#include "video/render.h"
#include "video/vdp_local.h"

namespace Video::Renderer
{

static void write_screen(int index, int x, uint8_t value)
{
	if (x < 0x100)
	{
		vdp.screens[index][x] = value;
	}
}

static void draw_bg(int index, int screen_y)
{
	//TODO: how does the tile format register work?
	assert(screen_y < 0xE0);
	uint8_t* tile_map = vdp.tile;

	for (int screen_x = 0; screen_x < 0x100; screen_x++)
	{
		int x = (screen_x + vdp.bg_scrollx[index]) & 0xFF;
		int y = (screen_y + vdp.bg_scrolly[index]) & 0x1FF;

		uint16_t* tile_map = (uint16_t*)vdp.tile;
		tile_map += (x >> 3) + ((y >> 3) * 0x20);

		uint16_t descriptor;
		memcpy(&descriptor, tile_map, 2);
		descriptor = Common::bswp16(descriptor);

		uint16_t tile_index = descriptor & 0x7FF;
		int screen_index = (descriptor >> 11) & 0x1;
		int pal_descriptor = (descriptor >> 12) & 0x3;
		bool x_flip = (descriptor >> 14) & 0x1;
		bool y_flip = descriptor >> 15;

		int tile_x = x & 0x7;
		if (x_flip)
		{
			tile_x = 7 - tile_x;
		}

		int tile_y = y & 0x7;
		if (y_flip)
		{
			tile_y = 7 - tile_y;
		}

		uint8_t* tile_ptr = vdp.tile + 0x1000 + (tile_index << 5);
		tile_ptr += tile_x >> 1;
		tile_ptr += tile_y << 2;

		uint8_t tile_data;
		if (tile_x & 0x1)
		{
			tile_data = (*tile_ptr) & 0xF;
		}
		else
		{
			tile_data = (*tile_ptr) >> 4;
		}

		//0 is transparent
		if (!tile_data)
		{
			continue;
		}

		uint16_t palsel = vdp.bg_palsel[index];
		int pal = (palsel >> (pal_descriptor * 4)) & 0xF;

		uint8_t output = tile_data + (pal << 4);
		write_screen(screen_index, screen_x, output);
	}
}

static void draw_bitmap(int index, int y)
{
	VDP::BitmapRegs* regs = &vdp.bitmap_regs[index];

	if (y < regs->screeny || y > regs->screeny + regs->h)
	{
		return;
	}

	y = y - regs->screeny;

	int start_x = regs->screenx + regs->clipx;
	int end_x = regs->screenx + regs->w;

	for (int x = start_x; x <= end_x; x++)
	{
		int data_x = (regs->scrollx + x) & 0xFF;
		int data_y = (regs->scrolly + y) & 0x1FF;

		uint32_t addr = data_x + (data_y * (regs->w + 1));
		uint8_t data = vdp.bitmap[addr & 0x1FFFF];

		if (data)
		{
			write_screen(0, x, data);
		}
	}
}

static void display_capture(int y)
{
	switch (vdp.capture_ctrl.format)
	{
	case 0x03:
		//Capture screen A before applying the palette
		memcpy(vdp.capture_buffer, vdp.screens[0], 0x100);
		break;
	default:
		assert(0);
	}
}

void draw_scanline(int y)
{
	//Set both screens to the backdrop color
	memset(vdp.screens, 0, sizeof(vdp.screens));

	for (int i = 0; i < 4; i++)
	{
		if (vdp.layer_ctrl.bitmap_enable[i])
		{
			draw_bitmap(i, y);
		}
	}

	if (vdp.layer_ctrl.bg_enable[0])
	{
		draw_bg(0, y);
	}

	if (vdp.capture_enable && y == vdp.capture_ctrl.scanline)
	{
		display_capture(y);
		vdp.capture_enable = false;
	}
}

}