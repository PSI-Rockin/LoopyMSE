#pragma once
#include <memory>
#include "video/video.h"

namespace Video
{

struct VDP
{
	//16-bit color output of the layers, screens, and final image to be displayed
	std::unique_ptr<uint16_t[]> bg_output[2];
	std::unique_ptr<uint16_t[]> bitmap_output[4];
	std::unique_ptr<uint16_t[]> obj_output[2];
	std::unique_ptr<uint16_t[]> screen_output[2];
	std::unique_ptr<uint16_t[]> display_output;

	int frame_ended;
	int visible_scanlines; //Configured by VDP_MODE

	//Screen A is 0, Screen B is 1
	uint8_t screens[2][DISPLAY_WIDTH];

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

	struct Mode
	{
		int use_pal;
		int extra_scanlines;
		int unk;
		int mouse_scan;
		int pad_scan;
		int unk2;
	};

	Mode mode;

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
		uint16_t buffer_ctrl;

		uint8_t buffered_color;
	};

	BitmapRegs bitmap_regs[4];
	uint16_t bitmap_ctrl;
	uint16_t bitmap_palsel;

	//BG/OBJ registers - 0x0C05Axxx
	struct BgCtrl
	{
		int shared_maps;
		int map_size;
		int bg0_8bit;
		int tile_size0;
		int tile_size1;
	};

	BgCtrl bg_ctrl;
	uint16_t bg_scrollx[2];
	uint16_t bg_scrolly[2];
	uint16_t bg_palsel[2];
	uint16_t tilebase;

	struct ObjCtrl
	{
		int id_offs;
		int tile_index_offs[2];
		int is_8bit;
	};

	ObjCtrl obj_ctrl;
	uint16_t obj_palsel[2];

	//Display registers - 0x0C05Bxxx

	uint16_t dispmode;
	
	struct LayerCtrl
	{
		int bg_enable[2];
		int bitmap_enable[4];
		int obj_enable[2];
		int bitmap_screen_mode[2];
		int obj_screen_mode[2];
	};

	LayerCtrl layer_ctrl;

	struct ColorPrio
	{
		int prio_mode;
		int screen_b_backdrop_only;
		int output_screen_b;
		int output_screen_a;
		int blend_mode;
	};

	ColorPrio color_prio;
	uint16_t backdrops[2];

	struct CaptureCtrl
	{
		int scanline;
		int format;
	};

	CaptureCtrl capture_ctrl;

	//IRQ control registers (not to be confused with 58008) - 0x0C05Cxxx
	struct CmpIrqCtrl
	{
		int irq0_enable;
		int nmi_enable;
		int use_vcmp;
		int irq0_enable2;
	};

	CmpIrqCtrl cmp_irq_ctrl;
	uint16_t irq0_hcmp;
	uint16_t irq0_vcmp;

	//DMA ctrl registers - 0x0C05Exxx
	uint16_t dma_mask;
	uint16_t dma_value;
};

extern VDP vdp;

}