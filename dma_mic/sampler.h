#include "pico/stdlib.h"

#define BUFFER_SIZE 128
#define HALF_BUFFER_SIZE (BUFFER_SIZE / 2)


volatile bool current_buffer_is_upper_half = false;

uint16_t patterns[3]= {
    0, // Kick pattern
    0, // Snare pattern
    0  // Hi-hat pattern
};
volatile uint8_t beat_index = 0; // Current beat index    
volatile uint8_t pattern_index = 0;
volatile uint16_t pattern_samples_per_step = 4;
uint16_t sampler_buffer[BUFFER_SIZE];
uint16_t current_bpm = 115; // Beats per minute

typedef struct{
    const uint16_t *data;
    uint16_t length;
    uint16_t position;
    uint8_t active;
} SamplePlayer;

