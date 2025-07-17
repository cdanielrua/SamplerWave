#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"


#define RED     0x800000  // (255*0.5, 0, 0)
#define GREEN   0x008000  // (0, 255*0.5, 0)
#define BLUE    0x000080  // (0, 0, 255*0.5)
#define WHITE   0x808080  // (255*0.5, 255*0.5, 255*0.5)
#define BLACK   0x000000  // (0, 0, 0)
#define YELLOW  0x808000  // (255*0.5, 255*0.5, 0)
// Todos los colores corregidos para formato GRB (WS2812)
#define CYAN    0x008080  // (0, 255*0.5, 255*0.5)
#define MAGENTA 0x800080  // G=0, R=0x80, B=0x80 (magenta: rojo+azul)
#define ORANGE  0x408000  // G=0x40, R=0x80, B=0   (naranja: rojo+verde)
#define PURPLE  0x000040  // G=0, R=0x00, B=0x40  (púrpura: rojo+azul)
#define PINK    0x608040  // G=0x60, R=0x80, B=0x40 (rosa: rojo+verde+azul)
#define BROWN   0x102810  // G=0x10, R=0x28, B=0x10 (marrón: rojo+verde+azul)


#define NUM_PIXELS 24
#define WS2812_PIN 2

PIO pio;
uint sm;
uint offset;
uint32_t pixels[NUM_PIXELS] = {0};
static inline void set_pixel_color(uint32_t pixel_index, uint32_t pixel_grb) {
 
    
    if (pixel_index < NUM_PIXELS) {
        pixels[pixel_index] = pixel_grb;
    }
    for (uint i = 0; i < NUM_PIXELS; ++i) {
        pio_sm_put_blocking(pio, sm, pixels[i] << 8u);
    }
}

// To actually update the LEDs, you need a function like this:
static inline void ws2812_show() {
    for (uint i = 0; i < NUM_PIXELS; ++i) {
        pio_sm_put_blocking(pio, sm, pixels[i] << 8u);
    }
}


void led_mix(uint8_t pattern_idx, uint8_t beat_idx, uint16_t *patterns, uint8_t slice) {
    uint8_t slice_start = slice;
    uint8_t slice_end = slice + 8;
    uint32_t slice_soft_color = CYAN; // Gris suave

    // LEDs 0-15: pattern y beat
    for (uint8_t i = 8; i < 25; ++i) {
        if (i == beat_idx) {
            set_pixel_color(i, YELLOW); // Prioridad máxima: beat
        }
        else if (patterns[pattern_idx] & (1 << i)) {
            switch (pattern_idx) {
                case 0: set_pixel_color(i, RED); break;
                case 1: set_pixel_color(i, GREEN); break;
                case 2: set_pixel_color(i, BLUE); break;
                default: break;
            }
        }
        else if (i >= slice_start && i < slice_end) {
            set_pixel_color(i, slice_soft_color); // Prioridad baja: slice activo
        }
        else {
            set_pixel_color(i, BLACK); // Inactivo
        }
    }

    // LEDs 16-23: color del pattern actual
    
    
    for (uint8_t i = 16; i < 25; ++i) {
        switch (pattern_idx) {
            case 0: set_pixel_color(i, RED); break;
            case 1: set_pixel_color(i, GREEN); break;
            case 2: set_pixel_color(i, BLUE); break;
            default: break;
        
        }
        
    }
}



static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

void ws2812_init(uint8_t pin) {
   
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio, &sm, &offset, pin, 1, true);
    hard_assert(success);

    ws2812_program_init(pio, sm, offset, pin, 800000, false);
}