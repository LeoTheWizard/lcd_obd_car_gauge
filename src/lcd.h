///
/// @file lcd.h
/// @brief Drives the NV3030B lcd controller with the rp2350 microcontroller.
/// @author Leo Walker
///

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "spi.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef struct lcd_device lcd_device_t;

struct font 
{
    const u8* data;
    const u32* glyph_index_map;   //  Access with char and recieve pointer to glyph
    u16 font_height;
};
typedef struct font font_t;
u32 font_measure_text_width(font_t* font, const char* text);

struct rgb16
{
    u8 rrrrrggg;
    u8 gggbbbbb;
};
typedef struct rgb16 colour_t;

#define COLOUR_RED (colour_t){0xF8, 0x00}
#define COLOUR_PURPLE (colour_t){0xF8, 0x1F}
#define COLOUR_WHITE (colour_t){0xFF, 0xFF}
#define COLOUR_BLACK (colour_t){0x00, 0x00}

struct rect
{
    u16 x;
    u16 y;
    u16 w;
    u16 h;
};

struct image
{
    colour_t* data;
    u32 width;
    u32 height;

    bool is_frame_buffer;
    lcd_device_t* parent_lcd;
};
typedef struct image image_t;

image_t image_create(u16 width, u16 height);
void image_destroy(image_t* img);

void image_clear(image_t* img, colour_t colour);
void image_draw_rectangle(image_t* img, struct rect bounds, colour_t colour);
void image_draw_text_bg(image_t* img, font_t* font, const char* text, u16 x, u16 y, colour_t bg_colour, colour_t text_colour);
void image_draw_image(int x, int y, image_t* dst_img, const image_t* src_img);

lcd_device_t* lcd_init_fb(image_t* frame_buffer, spi_instance_t* spi_dev, uint8_t dc, uint8_t rst, uint8_t bl);
lcd_device_t* lcd_init(u16 width, u16 height, spi_instance_t* spi_dev, uint8_t dc, uint8_t rst, uint8_t bl);
void lcd_destroy(lcd_device_t* dev);
void lcd_reset(lcd_device_t* dev);
void lcd_set_brightness(lcd_device_t* dev, u8 percentage);
image_t lcd_get_framebuffer(lcd_device_t* dev);
void lcd_update_display(lcd_device_t* dev);

