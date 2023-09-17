#pragma once
#include "video/video.h"

namespace Video
{

struct VDP
{
	//Screen A is 0, Screen B is 1
	uint8_t screens[2][0x100];

	//Bitmap VRAM - 0x0C000000
	uint8_t bitmap[BITMAP_VRAM_SIZE];

	//Tile VRAM - 0x0C040000
	uint8_t tile[TILE_VRAM_SIZE];

	//OAM - 0x0C050000
	uint8_t oam[OAM_SIZE];

	//Palette - 0x0C051000
	uint8_t palette[PALETTE_SIZE];

	//Display capture buffer - 0x0C052000
	uint8_t capture_buffer[CAPTURE_SIZE];

	//Control registers - 0x0C058xxx

	uint16_t unk_58000;
	uint16_t hcount;
	uint16_t vcount;
	
	int capture_enable;

	//Bitmap registers - 0x0C059xxx
	struct BitmapRegs
	{
		uint16_t scrollx;
		uint16_t scrolly;
		uint16_t screenx;
		uint16_t screeny;
		uint16_t w;
		uint16_t clipx;
		uint16_t h;
		uint16_t unk;
	};

	BitmapRegs bitmap_regs[4];
	uint16_t bitmap_030, bitmap_040;

	//BG/OBJ registers - 0x0C05Axxx
	uint16_t bg_format;
	uint16_t bg_scrollx[2];
	uint16_t bg_scrolly[2];
	uint16_t bg_palsel[2];
	uint16_t bg_tileoffs;

	//Display registers - 0x0C05Bxxx

	uint16_t display_000;
	
	struct LayerCtrl
	{
		int bg_enable[2];
		int bitmap_enable[4];
		int obj_enable[2];
		int unk;
	};

	LayerCtrl layer_ctrl;
	uint16_t display_004;
	uint16_t master_brightness;
	uint16_t display_008;

	struct CaptureCtrl
	{
		int scanline;
		int format;
	};

	CaptureCtrl capture_ctrl;
};

extern VDP vdp;

}