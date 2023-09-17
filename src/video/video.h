#pragma once
#include <cstdint>

namespace Video
{

constexpr static int BITMAP_VRAM_START = 0x04000000;
constexpr static int BITMAP_VRAM_SIZE = 0x40000;
constexpr static int BITMAP_VRAM_END = BITMAP_VRAM_START + BITMAP_VRAM_SIZE;

constexpr static int TILE_VRAM_START = 0x04040000;
constexpr static int TILE_VRAM_SIZE = 0x10000;
constexpr static int TILE_VRAM_END = TILE_VRAM_START + TILE_VRAM_SIZE;

constexpr static int OAM_START = 0x04050000;
constexpr static int OAM_SIZE = 0x200;
constexpr static int OAM_END = OAM_START + OAM_SIZE;

constexpr static int PALETTE_START = 0x04051000;
constexpr static int PALETTE_SIZE = 0x200;
constexpr static int PALETTE_END = PALETTE_START + PALETTE_SIZE;

constexpr static int CAPTURE_START = 0x04052000;
constexpr static int CAPTURE_SIZE = 0x200;
constexpr static int CAPTURE_END = CAPTURE_START + CAPTURE_SIZE;

constexpr static int CTRL_REG_START = 0x04058000;
constexpr static int CTRL_REG_END = 0x04059000;

constexpr static int BITMAP_REG_START = 0x04059000;
constexpr static int BITMAP_REG_END = 0x0405A000;

constexpr static int BGOBJ_REG_START = 0x0405A000;
constexpr static int BGOBJ_REG_END = 0x0405B000;

constexpr static int DISPLAY_REG_START = 0x0405B000;
constexpr static int DISPLAY_REG_END = 0x0405C000;

void initialize();
void shutdown();
void dump_for_serial();

//TODO: should these MMIO accessors be moved to a different file?
uint8_t palette_read8(uint32_t addr);
uint16_t palette_read16(uint32_t addr);
uint32_t palette_read32(uint32_t addr);

void palette_write8(uint32_t addr, uint8_t value);
void palette_write16(uint32_t addr, uint16_t value);
void palette_write32(uint32_t addr, uint32_t value);

uint8_t oam_read8(uint32_t addr);
uint16_t oam_read16(uint32_t addr);
uint32_t oam_read32(uint32_t addr);

void oam_write8(uint32_t addr, uint8_t value);
void oam_write16(uint32_t addr, uint16_t value);
void oam_write32(uint32_t addr, uint32_t value);

uint8_t capture_read8(uint32_t addr);
uint16_t capture_read16(uint32_t addr);
uint32_t capture_read32(uint32_t addr);

void capture_write8(uint32_t addr, uint8_t value);
void capture_write16(uint32_t addr, uint16_t value);
void capture_write32(uint32_t addr, uint32_t value);

uint8_t ctrl_read8(uint32_t addr);
uint16_t ctrl_read16(uint32_t addr);
uint32_t ctrl_read32(uint32_t addr);

void ctrl_write8(uint32_t addr, uint8_t value);
void ctrl_write16(uint32_t addr, uint16_t value);
void ctrl_write32(uint32_t addr, uint32_t value);

uint8_t bitmap_reg_read8(uint32_t addr);
uint16_t bitmap_reg_read16(uint32_t addr);
uint32_t bitmap_reg_read32(uint32_t addr);

void bitmap_reg_write8(uint32_t addr, uint8_t value);
void bitmap_reg_write16(uint32_t addr, uint16_t value);
void bitmap_reg_write32(uint32_t addr, uint32_t value);

uint8_t bgobj_read8(uint32_t addr);
uint16_t bgobj_read16(uint32_t addr);
uint32_t bgobj_read32(uint32_t addr);

void bgobj_write8(uint32_t addr, uint8_t value);
void bgobj_write16(uint32_t addr, uint16_t value);
void bgobj_write32(uint32_t addr, uint32_t value);

uint8_t display_read8(uint32_t addr);
uint16_t display_read16(uint32_t addr);
uint32_t display_read32(uint32_t addr);

void display_write8(uint32_t addr, uint8_t value);
void display_write16(uint32_t addr, uint16_t value);
void display_write32(uint32_t addr, uint32_t value);

}