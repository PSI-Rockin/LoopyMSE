#include <algorithm>
#include <cassert>
#include <cstring>
#include <common/bswp.h>
#include "video/render.h"
#include "video/vdp_local.h"

namespace Video::Renderer
{

static uint16_t read_palette(uint8_t value)
{
	uint16_t color;
	memcpy(&color, vdp.palette + (value * 2), 2);
	return Common::bswp16(color);
}

static uint16_t read_screen(int index, int x)
{
	uint8_t pal_color = vdp.screens[index][x];
	if (!pal_color || (index == 1 && vdp.color_prio.screen_b_backdrop_only))
	{
		return vdp.backdrops[index];
	}

	return read_palette(pal_color);
}

static void write_screen(int index, int x, uint8_t value)
{
	if (x < DISPLAY_WIDTH)
	{
		vdp.screens[index][x] = value;
	}
}

static void write_color(std::unique_ptr<uint16_t[]>& buffer, int x, int y, uint16_t value)
{
	buffer[x + (y * DISPLAY_WIDTH)] = value;
}

static void write_pal_color(std::unique_ptr<uint16_t[]>& buffer, int x, int y, uint8_t pal_index)
{
	uint16_t color = read_palette(pal_index);
	write_color(buffer, x, y, color);
}

static void draw_bg(int index, int screen_y)
{
	//TODO: how does the tile format register work?
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
		write_pal_color(vdp.bg_output[index], screen_x, screen_y, output);
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

		int pair_index = index >> 1;
		int output_mode = vdp.layer_ctrl.bitmap_screen_mode[pair_index];

		if (!data)
		{
			continue;
		}

		write_pal_color(vdp.bitmap_output[index], x, y + regs->screeny, data);

		if (output_mode & 0x1)
		{
			write_screen(1, x, data);
		}

		if (output_mode & 0x2)
		{
			write_screen(0, x, data);
		}
	}
}

static void process_color_math(int y)
{
	for (int x = 0; x < DISPLAY_WIDTH; x++)
	{
		uint16_t input_a = 0, input_b = 0;
		if (vdp.color_prio.output_screen_a)
		{
			input_a = read_screen(0, x);
			write_color(vdp.screen_output[0], x, y, input_a);
		}

		if (vdp.color_prio.output_screen_b)
		{
			input_b = read_screen(1, x);
			write_color(vdp.screen_output[1], x, y, input_b);
		}

		int a_r = (input_a >> 10) & 0x1F;
		int a_g = (input_a >> 5) & 0x1F;
		int a_b = input_a & 0x1F;

		int b_r = (input_b >> 10) & 0x1F;
		int b_g = (input_b >> 5) & 0x1F;
		int b_b = input_b & 0x1F;

		int out_r, out_g, out_b;

		if (vdp.color_prio.blend_mode)
		{
			//Subtractive blending
			out_r = a_r - b_r;
			out_g = a_g - b_g;
			out_b = a_b - b_b;
		}
		else
		{
			//Additive blending
			out_r = a_r + b_r;
			out_g = a_g + b_g;
			out_b = a_b + b_b;
		}

		out_r = std::clamp(out_r, 0, 0x1F);
		out_g = std::clamp(out_g, 0, 0x1F);
		out_b = std::clamp(out_b, 0, 0x1F);

		uint16_t output = (out_r << 10) | (out_g << 5) | out_b;
		write_color(vdp.display_output, x, y, output);
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
	//Clear the output buffers
	constexpr static int LINE_SIZE = DISPLAY_WIDTH * 2;
	int offs = y * DISPLAY_WIDTH;
	for (int i = 0; i < 2; i++)
	{
		memset(vdp.bg_output[i].get() + offs, 0, LINE_SIZE);
		memset(vdp.obj_output[i].get() + offs, 0, LINE_SIZE);
		memset(vdp.bitmap_output[i].get() + offs, 0, LINE_SIZE);
		memset(vdp.bitmap_output[i + 2].get() + offs, 0, LINE_SIZE);
		memset(vdp.screen_output[i].get() + offs, 0, LINE_SIZE);
	}

	memset(vdp.display_output.get() + offs, 0, LINE_SIZE);

	//Set both screens to the backdrop color
	memset(vdp.screens, 0, sizeof(vdp.screens));

	for (int i = 3; i >= 0; i--)
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

	process_color_math(y);

	if (vdp.capture_enable && y == vdp.capture_ctrl.scanline)
	{
		display_capture(y);
		vdp.capture_enable = false;
	}
}

}