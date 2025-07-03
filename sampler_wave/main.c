// =================================================================
// CÓDIGO FINAL Y DEFINITIVO - Sampler Wave
// =================================================================
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "audio_samples.h"

// --- Definiciones del Hardware y Secuenciador ---
#define AUDIO_PIN 0
#define POT_PIN 26
#define LED_PIN 16
const uint BUTTON_PINS[] = {10, 11, 12, 13};

#define NUM_SAMPLES 4
#define NUM_STEPS 16
#define MAX_VOICES 8
#define SAMPLE_RATE 11025
#define PWM_WRAP_VALUE 255
#define IS_RGBW false
#define NUM_PIXELS 16
const uint32_t DEBOUNCE_DELAY_US = 70000; // Retardo de 70ms para debounce

// --- Estructuras de Datos ---
typedef struct {
    const uint8_t *data;
    uint32_t len;
    uint32_t pos;
} audio_voice_t;

// --- Variables Globales ---
bool sequence_grid[NUM_SAMPLES][NUM_STEPS] = {false};
volatile uint8_t current_step = 0;
volatile uint bpm = 120;
volatile bool step_changed = true;
uint8_t dma_buffer[256];
int dma_chan;
audio_voice_t voices[MAX_VOICES];
volatile absolute_time_t last_press_time[NUM_SAMPLES];

// --- Funciones de LEDs ---
void update_leds() {
    for (int i = 0; i < NUM_PIXELS; ++i) {
        if (i == current_step) {
            pio_sm_put_blocking(pio0, 0, 0x00FFFFFF << 8u);
        } else {
            bool active_step = false;
            for(int j = 0; j < NUM_SAMPLES; ++j) {
                if(sequence_grid[j][i]) {
                    active_step = true;
                    break;
                }
            }
            pio_sm_put_blocking(pio0, 0, (active_step ? 0x0000FF : 0x100010) << 8u);
        }
    }
}

// --- Motor de Audio ---
void play_sample(uint sample_index) {
    if (sample_index >= NUM_SAMPLES || samples[sample_index].len == 0) return;
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].pos >= voices[i].len) {
            voices[i].data = samples[sample_index].data;
            voices[i].len = samples[sample_index].len;
            voices[i].pos = 0;
            return;
        }
    }
}

void dma_irq_handler() {
    dma_hw->ints0 = 1u << dma_chan;
    for (int i = 0; i < sizeof(dma_buffer); i++) dma_buffer[i] = 128;
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].pos < voices[i].len) {
            for (int j = 0; j < sizeof(dma_buffer); ++j) {
                if (voices[i].pos < voices[i].len) {
                    int mixed_sample = (int)dma_buffer[j] + (int)voices[i].data[voices[i].pos] - 128;
                    if (mixed_sample > 255) mixed_sample = 255;
                    if (mixed_sample < 0) mixed_sample = 0;
                    dma_buffer[j] = (uint8_t)mixed_sample;
                    voices[i].pos++;
                } else break;
            }
        }
    }
    dma_channel_set_read_addr(dma_chan, dma_buffer, true);
}

void init_audio() {
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(AUDIO_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, PWM_WRAP_VALUE);
    pwm_config_set_clkdiv(&config, (float)clock_get_hz(clk_sys) / (SAMPLE_RATE * (PWM_WRAP_VALUE + 1)));
    pwm_init(slice_num, &config, true);
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_config = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, pwm_get_dreq(slice_num));
    dma_channel_configure(dma_chan, &dma_config, &pwm_hw->slice[slice_num].cc, dma_buffer, sizeof(dma_buffer), false);
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    for (int i = 0; i < MAX_VOICES; ++i) { voices[i].len = 0; voices[i].pos = 0; }
    dma_irq_handler();
}

// --- Secuenciador y Entradas de Usuario ---
bool sequencer_timer_callback(struct repeating_timer *t) {
    current_step = (current_step + 1) % NUM_STEPS;
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        if (sequence_grid[i][current_step]) {
            play_sample(i);
        }
    }
    step_changed = true;
    return true;
}

void button_callback(uint gpio, uint32_t events) {
    absolute_time_t now = get_absolute_time();
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        if (gpio == BUTTON_PINS[i]) {
            uint64_t diff_us = absolute_time_diff_us(last_press_time[i], now);
            if (diff_us > DEBOUNCE_DELAY_US) {
                last_press_time[i] = now;
                play_sample(i);
                sequence_grid[i][current_step] = !sequence_grid[i][current_step];
                step_changed = true;
            }
            break;
        }
    }
}

void init_buttons() {
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        gpio_init(BUTTON_PINS[i]);
        gpio_pull_up(BUTTON_PINS[i]);
        gpio_set_irq_enabled_with_callback(BUTTON_PINS[i], GPIO_IRQ_EDGE_FALL, true, &button_callback);
    }
}

// --- Bucle Principal ---
int main() {
    stdio_init_all();

    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, LED_PIN, 800000, IS_RGBW);

    adc_init();
    adc_gpio_init(POT_PIN);
    adc_select_input(0);

    init_audio();
    init_buttons();
    
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        last_press_time[i] = nil_time;
    }

    struct repeating_timer timer;
    long delay_us = 60 * 1000 * 1000 / (bpm * 4);
    add_repeating_timer_us(-delay_us, sequencer_timer_callback, NULL, &timer);
    
    while (true) {
        uint16_t adc_value = adc_read();
        uint new_bpm = 60 + (adc_value * 160 / 4095);
        if (new_bpm != bpm) {
            bpm = new_bpm;
            cancel_repeating_timer(&timer);
            delay_us = 60 * 1000 * 1000 / (bpm * 4);
            add_repeating_timer_us(-delay_us, sequencer_timer_callback, NULL, &timer);
        }

        if (step_changed) {
            update_leds();
            step_changed = false;
        }
    }
    return 0;
}