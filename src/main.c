///
/// @file main.c
/// @brief Program entry point for Noah's Car Digital Gauge.
/// @copyright Leo Walker (c) 2025, All Rights Reserved.
/// @author Leo Walker
/// 
///

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "lcd.h"

#include "font.h"
#include "bignumbers.h"

#define UART_ID uart0
#define BAUD_RATE 500000

// Pins for UART0 on the Pico
#define UART_TX_PIN 0
#define UART_RX_PIN 1
enum text_alignment
{
    ALIGN_LEFT,
    ALIGN_MIDDLE,
    ALIGN_RIGHT
};

static u32 measure_big_number_width(const char* number_string)
{
    u32 width = 0;
    u8 numstrlen = strlen(number_string);
    for(int i = 0; i < numstrlen; i++)
    {
        switch(number_string[i])
        {
            case '0':
            width += gauge_zero_img.width;
            break;

            case '1':
            width += gauge_one_img.width;
            break;

            case '2':
            width += gauge_two_img.width;
            break;

            case '3':
            width += gauge_three_img.width;
            break;

            case '4':
            width += gauge_four_img.width;
            break;

            case '5':
            width += gauge_five_img.width;
            break;

            case '6':
            width += gauge_six_img.width;
            break;

            case '7':
            width += gauge_seven_img.width;
            break;

            case '8':
            width += gauge_eight_img.width;
            break;

            case '9':
            width += gauge_nine_img.width;
            break;

            case '.':
            width += gauge_period_img.width;
            break;
        }
    }
    return width;
}

static u32 image_draw_big_numbers(image_t* img, u16 x, u16 y, const char* number_string, enum text_alignment alignment)
{
    u16 total_width = measure_big_number_width(number_string);
    int x_advance = (float)x + ((float)total_width * (alignment * -0.5f));
    u8 number_string_length = strlen(number_string);

    for(int i = 0; i < number_string_length; i++)
    {
        switch(number_string[i])
        {
            case '0':
            image_draw_image(x_advance, y, img, &gauge_zero_img);
            x_advance += gauge_zero_img.width;
            break;

            case '1':
            image_draw_image(x_advance, y, img, &gauge_one_img);
            x_advance += gauge_one_img.width;
            break;

            case '2':
            image_draw_image(x_advance, y, img, &gauge_two_img);
            x_advance += gauge_two_img.width;
            break;

            case '3':
            image_draw_image(x_advance, y, img, &gauge_three_img);
            x_advance += gauge_three_img.width;
            break;

            case '4':
            image_draw_image(x_advance, y, img, &gauge_four_img);
            x_advance += gauge_four_img.width;
            break;

            case '5':
            image_draw_image(x_advance, y, img, &gauge_five_img);
            x_advance += gauge_five_img.width;
            break;

            case '6':
            image_draw_image(x_advance, y, img, &gauge_six_img);
            x_advance += gauge_six_img.width;
            break;

            case '7':
            image_draw_image(x_advance, y, img, &gauge_seven_img);
            x_advance += gauge_seven_img.width;
            break;

            case '8':
            image_draw_image(x_advance, y, img, &gauge_eight_img);
            x_advance += gauge_eight_img.width;
            break;

            case '9':
            image_draw_image(x_advance, y, img, &gauge_nine_img);
            x_advance += gauge_nine_img.width;
            break;

            case '.':
            image_draw_image(x_advance, y, img, &gauge_period_img);
            x_advance += gauge_period_img.width;
            break;
        }
    }
    return total_width;
}

int main()
{
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    stdio_uart_init_full(UART_ID, BAUD_RATE, UART_TX_PIN, UART_RX_PIN);

    printf("abc\n");
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    gpio_put(25, false);

    colour_t frame_buffer_data[240 * 280];
    image_t frame_buffer;
    frame_buffer.data = (colour_t*)frame_buffer_data;
    frame_buffer.width = 240;
    frame_buffer.height = 280;

    spi_instance_t* spi_device = spi_instance_init(SPI_HW_0, 2, 3, 4, 5, false, 80000000);
    lcd_device_t* lcd_device = lcd_init_fb(&frame_buffer, spi_device, 6, 7, 8);

    lcd_reset(lcd_device);
    image_clear(&frame_buffer, COLOUR_BLACK);
    image_draw_text_bg(&frame_buffer, &gauge_font, "Miles Per Gallon", 15, 180, COLOUR_BLACK, COLOUR_WHITE);
    lcd_update_display(lcd_device);

    float value = 0.0f;
    char value_string[16];

    int last_width = 0;
    while(1)
    {
        snprintf(value_string, 16, "%0.1f", value);

        
        int new_width = image_draw_big_numbers(&frame_buffer, 120, 100, value_string, ALIGN_MIDDLE);

        if(new_width < last_width)
        {
            int diff = last_width - new_width;
            image_draw_rectangle(&frame_buffer, (struct rect){120 - last_width / 2, 100, diff / 2, 75}, COLOUR_BLACK);
            image_draw_rectangle(&frame_buffer, (struct rect){120 + new_width / 2, 100, diff / 2, 75}, COLOUR_BLACK);
        }

        last_width = new_width;

        lcd_update_display(lcd_device);
        sleep_ms(100);

        value += 0.2f;
    }
    
    lcd_destroy(lcd_device);
    spi_instance_destroy(spi_device);

    return 0;
}