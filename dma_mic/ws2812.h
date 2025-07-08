#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"


#define RED     0x190000  // (255*0.1, 0, 0)
#define GREEN   0x001900  // (0, 255*0.1, 0)
#define BLUE    0x000019  // (0, 0, 255*0.1)
#define WHITE   0x191919  // (255*0.1, 255*0.1, 255*0.1)
#define BLACK   0x000000  // (0, 0, 0)
#define YELLOW  0x191900  // (255*0.1, 255*0.1, 0)
#define CYAN    0x001919  // (0, 255*0.1, 255*0.1)
// Todos los colores corregidos para formato GRB (WS2812)
#define MAGENTA 0x001919  // G=0, R=0x19, B=0x19 (magenta: rojo+azul)
#define ORANGE  0x161900  // G=0x16, R=0x19, B=0   (naranja: rojo+verde)
#define PURPLE  0x000808  // G=0, R=0x08, B=0x08  (púrpura: rojo+azul)
#define PINK    0x13190C  // G=0x13, R=0x19, B=0x0C (rosa: rojo+verde+azul)
#define BROWN   0x020A02  // G=0x02, R=0x0A, B=0x02 (marrón: rojo+verde+azul)


#define NUM_PIXELS 16
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


void led_mix(uint8_t pattern_idx,uint8_t beat_idx,uint16_t *patterns) {
   
   for(uint8_t i = 0; i < NUM_PIXELS; ++i) {
        if(i == beat_idx) set_pixel_color(i, WHITE); // Highlight the current beat
        else if (patterns[pattern_idx] & (1 << i)) {
            switch (pattern_idx)
            {
            case 0:
                set_pixel_color(i, RED); // Kick pattern
                break;
            case 1:
                set_pixel_color(i, GREEN); // Snare pattern
                break;
            case 2:
                set_pixel_color(i, BLUE); // Hi-hat pattern

            default:
                break;
            } 
        } else {
            set_pixel_color(i, BLACK); // Inactive beat
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