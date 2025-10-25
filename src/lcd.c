#include <stdlib.h>
#include <stdio.h>
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/regs/dreq.h"
#include <math.h>

#include "lcd.h"

#pragma region Images

image_t image_create(u16 width, u16 height)
{
    image_t temp_img;
    temp_img.data = malloc((u32)width * (u32)height * 2);
    if(temp_img.data)
    {
        temp_img.width = width;
        temp_img.height = height;
    }
    else
    {
        temp_img.width = 0;
        temp_img.height = 0;
    }
    
    temp_img.is_frame_buffer = false;
    temp_img.parent_lcd = NULL;

    return temp_img;
}

void image_destroy(image_t* img)
{
    if(img->data) free(img->data);
    img->data = NULL;
    img->width = 0;
    img->height = 0;
    img->is_frame_buffer = false;
    img->parent_lcd = NULL;
}

void image_clear(image_t* img, colour_t colour)
{
    // int dma_chan = 0;
    // dma_channel_wait_for_finish_blocking(dma_chan);
    // dma_channel_config c = dma_channel_get_default_config(dma_chan);

    // channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    // channel_config_set_read_increment(&c, false);
    // channel_config_set_write_increment(&c, true);

    // dma_channel_configure(
    //     dma_chan,
    //     &c,
    //     img->data,
    //     &colour,
    //     img->width * img->height,
    //     true
    // );
    for(u32 i = 0; i < img->width * img->height; i++)
    {
        img->data[i] = colour;
    }
}

void image_draw_rectangle(image_t* img, struct rect bounds, colour_t colour)
{
    if(bounds.x > img->width || bounds.y > img->height) return;
    if(bounds.w + bounds.x > img->width) bounds.w = img->width - bounds.x;
    if(bounds.h + bounds.y > img->height) bounds.h = img->height - bounds.y;

    for(int i = 0; i < bounds.w; i++)
    {
        img->data[(bounds.y * img->width) + bounds.x + i] = colour;
    }

    for(int i = 1; i < bounds.h; i++)
    {
        __builtin_memcpy(img->data + ((bounds.y + i) * img->width) + bounds.x, 
                        img->data + (bounds.y * img->width) + bounds.x,
                        bounds.w * 2);
    }
}

static colour_t colour_blend(const colour_t c1, const colour_t c2, const uint8_t mix)
{
    colour_t out;

    if (mix == 0)  return c1;
    if (mix == 255) return c2;

    // Extract channels (RGB565)
    uint8_t r1 = (c1.rrrrrggg >> 3) & 0x1F;
    uint8_t g1 = ((c1.rrrrrggg & 0x07) << 3) | ((c1.gggbbbbb >> 5) & 0x07);
    uint8_t b1 = c1.gggbbbbb & 0x1F;

    uint8_t r2 = (c2.rrrrrggg >> 3) & 0x1F;
    uint8_t g2 = ((c2.rrrrrggg & 0x07) << 3) | ((c2.gggbbbbb >> 5) & 0x07);
    uint8_t b2 = c2.gggbbbbb & 0x1F;

    // Blend per channel
    uint8_t r = (r1 * (255 - mix) + r2 * mix) / 255;
    uint8_t g = (g1 * (255 - mix) + g2 * mix) / 255;
    uint8_t b = (b1 * (255 - mix) + b2 * mix) / 255;

    // Repack into RGB565 struct
    out.rrrrrggg = (r << 3) | (g >> 3);
    out.gggbbbbb = ((g & 0x07) << 5) | b;

    return out;
}

void image_draw_text_bg(image_t* img, font_t* font, const char* text, u16 x, u16 y, colour_t bg_colour, colour_t text_colour)
{
    int str_index = 0;
    int advance_x = 0;
    int advance_y = 0;
    
    colour_t* blend_map = malloc(sizeof(colour_t) * 256);
    for(u16 i = 0; i < 256; i++)
    {
        blend_map[i] = colour_blend(bg_colour, text_colour, i);
    }

    while(text[str_index] != '\0')
    {
        char c = text[str_index];
        
        const u8* glyph_data = font->data + font->glyph_index_map[(int)c];
        u8 char_width = *glyph_data;
        if(c == ' ')
        {
            advance_x += char_width;
            str_index++;
            continue;
        }

        for(int iy = 0; iy < font->font_height; iy++)
        {
            const u8* row_data = (glyph_data + 1) + char_width * iy;
            for(int ix = 0; ix < char_width; ix++)
            {
                img->data[(y + advance_y + iy) * img->width + (x + advance_x + ix)] = blend_map[*(row_data + ix)];
            }
        }

        advance_x += char_width;
        str_index++;
    }

    free(blend_map);
}

void image_draw_image(int x, int y, image_t* dst_img, const image_t* src_img)
{
    printf("x: %d y: %d sw: %d sh: %d\n", x, y, src_img->width, src_img->height);
    // Image completely off screen.
    if(x > dst_img->width || y > dst_img->height) return;
    if((int)x < -(int)src_img->width || (int)y < -(int)dst_img->height) return;

    // Cull dimensions
    int width = src_img->width;
    if(x + width > dst_img->width) width = dst_img->width - x;
    int height = src_img->height;
    if(y + height > dst_img->height) height = dst_img->height - y;

    // Calculate onscreen area
    int x_start = 0;
    int y_start = 0;
    if(x < 0) x_start = -x;
    if(y < 0) y_start = -y;

    for(int i = y_start; i < height; i++)
    {
        //  Calculate row
        colour_t* dst_row = dst_img->data + (dst_img->width * (i + y));
        colour_t* src_row = src_img->data + (src_img->width * i);

        for(int j = x_start; j < width; j++)
        {
            dst_row[x + j] = src_row[j];
        }
    }
}

#pragma endregion

#pragma region Fonts

u32 font_measure_text_width(font_t* font, const char* text)
{
    u32 width = 0;
    u32 str_index = 0;
    while(text[str_index] != '\0')
    {
        char c = text[str_index];
        width += *(font->data + font->glyph_index_map[c]);
        str_index++;
    }

    return width;
}

#pragma region Lcd Device

enum lcd_madctrl_bits
{
    LCD_MADCTL_MY = 0x80,  ///< Bottom to top (Page Address Order)
    LCD_MADCTL_MX = 0x40,  ///< Right to left (Column Address Order)
    LCD_MADCTL_MV = 0x20,  ///< Reverse Mode (Page/Column Order)
    LCD_MADCTL_ML = 0x10,  ///< LCD refresh Bottom to top (Line Address Order)
    LCD_MADCTL_RGB = 0x00, ///< Red-Green-Blue pixel order
    LCD_MADCTL_BGR = 0x08, ///< Blue-Green-Red pixel order
};

enum lcd_spi_cmds
{
    LCD_SPI_NOP                     = 0x00,
    LCD_SPI_RESET                   = 0x01,
    LCD_SPI_SLEEP_IN                = 0x10,
    LCD_SPI_SLEEP_OUT               = 0x11,
    LCD_SPI_INVERT_OFF              = 0x20,
    LCD_SPI_INVERT_ON               = 0x21,
    LCD_SPI_DISPLAY_OFF             = 0x28,
    LCD_SPI_DISPLAY_ON              = 0x29,
    LCD_SPI_COLUMN_ADDRESS          = 0x2A,
    LCD_SPI_ROW_ADDRESS             = 0x2B,
    LCD_SPI_MEMORY_WRITE            = 0x2C,
    LCD_SPI_MADCTL                  = 0x36,
    LCD_SPI_VERTICAL_SCROLL_START   = 0x37,
    LCD_SPI_IDLE_MODE_OFF           = 0x38,
    LCD_SPI_IDLE_MODE_ON            = 0x39,
    LCD_SPI_PIXEL_FORMAT            = 0x3A
};

struct lcd_device
{
    image_t frame_buffer;
    spi_instance_t* spi_dev;
    u8 dc_pin;
    u8 reset_pin;
    u8 backlight_pin;
};

static void lcd_send_byte(lcd_device_t* dev, u8 data)
{
    gpio_put(dev->dc_pin, true);
    spi_instance_transmit(dev->spi_dev, &data, 1);
}

static void lcd_send_data(lcd_device_t* dev, u8* data, u32 length)
{
    gpio_put(dev->dc_pin, true);
    spi_instance_transmit(dev->spi_dev, data, length);
}

static void lcd_send_cmd(lcd_device_t* dev, u8 data)
{
    gpio_put(dev->dc_pin, false);
    spi_instance_transmit_byte(dev->spi_dev, data);
}

static void lcd_set_address_window(lcd_device_t* dev, u16 x0, u16 y0, u16 x1, u16 y1)
{
    u8 column_pack[4] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF};
    u8 row_pack[4] = {y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF};

    lcd_send_cmd(dev, LCD_SPI_COLUMN_ADDRESS);
    lcd_send_data(dev, column_pack, 4);

    lcd_send_cmd(dev, LCD_SPI_ROW_ADDRESS);
    lcd_send_data(dev, row_pack, 4);

    //  Ready to write colour data.
    lcd_send_cmd(dev, LCD_SPI_MEMORY_WRITE);
}

lcd_device_t* lcd_init_fb(image_t* frame_buffer, spi_instance_t* spi_dev, uint8_t dc, uint8_t rst, uint8_t bl)
{
    lcd_device_t* dev = malloc(sizeof(lcd_device_t));
    dev->frame_buffer = *frame_buffer;
    dev->spi_dev = spi_dev;
    dev->dc_pin = dc;
    dev->reset_pin = rst;
    dev->backlight_pin = bl;

    dev->frame_buffer.is_frame_buffer = true;
    dev->frame_buffer.parent_lcd = dev;

    gpio_init(dc);
    gpio_init(rst);
    gpio_init(bl);

    gpio_set_dir(dc, GPIO_OUT);
    gpio_set_dir(rst, GPIO_OUT);
    gpio_set_dir(bl, GPIO_OUT);

    gpio_put(dc, true);
    gpio_put(rst, true);
    gpio_put(bl, true);
    return dev;
}

lcd_device_t* lcd_init(u16 width, u16 height, spi_instance_t* spi_dev, uint8_t dc, uint8_t rst, uint8_t bl)
{
    image_t fb = image_create(width, height);
    return lcd_init_fb(&fb, spi_dev, dc, rst, bl);
}

void lcd_destroy(lcd_device_t* dev)
{
    if(!dev) return;
    image_destroy(&dev->frame_buffer);

    free(dev);
}

void lcd_reset(lcd_device_t* dev)
{
    gpio_put(dev->reset_pin, false);
    sleep_ms(100);
    gpio_put(dev->reset_pin, true);
    sleep_ms(120);
    gpio_put(25, false);

    spi_instance_chip_enable(dev->spi_dev);

    lcd_send_cmd(dev, LCD_SPI_RESET);
    sleep_ms(10);

    //  MADCTL
    u8 madctl = LCD_MADCTL_RGB;
    lcd_send_cmd(dev, LCD_SPI_MADCTL);
    lcd_send_data(dev, &madctl, 1);

    u8 COLMOD = 0x05;
    lcd_send_cmd(dev, LCD_SPI_PIXEL_FORMAT);
    lcd_send_data(dev, &COLMOD, 1);

    lcd_send_cmd(dev, LCD_SPI_INVERT_ON);

    lcd_send_cmd(dev, LCD_SPI_SLEEP_OUT);
    sleep_ms(10);

    lcd_send_cmd(dev, LCD_SPI_DISPLAY_ON);
    spi_instance_chip_disable(dev->spi_dev);

}

image_t lcd_get_framebuffer(lcd_device_t* dev)
{
    return dev->frame_buffer;
}

void lcd_update_display(lcd_device_t* dev)
{
    spi_instance_chip_enable(dev->spi_dev);
    lcd_set_address_window(dev, 0, 20, dev->frame_buffer.width - 1, dev->frame_buffer.height +19);
    lcd_send_data(dev, (u8*)dev->frame_buffer.data, dev->frame_buffer.width * dev->frame_buffer.height * sizeof(colour_t));
    spi_instance_chip_disable(dev->spi_dev);
}

#pragma endregion