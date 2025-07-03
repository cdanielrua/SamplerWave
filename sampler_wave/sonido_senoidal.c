#include <stdio.h>
#include <math.h> // Necesario para la función sin()
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"

// --- Definiciones ---
#define AUDIO_PIN 0
#define BUTTON_PIN 10
#define SAMPLE_RATE 11025
#define PWM_WRAP_VALUE 255
#define SINE_WAVE_FREQUENCY 440.0f // Frecuencia de la nota 'La' (A4)
#define SINE_WAVE_SAMPLES 512      // Longitud de nuestra onda generada

// --- Estructuras y Variables Globales ---
typedef struct { const uint8_t *data; uint32_t len; uint32_t pos; } audio_voice_t;
audio_voice_t voice;
uint8_t dma_buffer[256];
int dma_chan;

// Array para guardar nuestro sonido generado
uint8_t sine_wave_data[SINE_WAVE_SAMPLES];

// --- Funciones de Audio (simplificadas para una sola voz) ---
void play_sine_wave() {
    // Si ya está sonando, no hacer nada
    if (voice.pos < voice.len) return;

    voice.data = sine_wave_data;
    voice.len = SINE_WAVE_SAMPLES;
    voice.pos = 0;
}

void dma_irq_handler() {
    dma_hw->ints0 = 1u << dma_chan;
    for (int i = 0; i < sizeof(dma_buffer); i++) dma_buffer[i] = 128; // Silencio
    
    if (voice.pos < voice.len) {
        for (int j = 0; j < sizeof(dma_buffer); ++j) {
            if (voice.pos < voice.len) {
                // No hay mezcla, solo copiamos el dato de la única voz
                dma_buffer[j] = voice.data[voice.pos];
                voice.pos++;
            } else break;
        }
    }
    dma_channel_set_read_addr(dma_chan, dma_buffer, true);
}

void init_audio() {
    // Generar la onda senoidal
    for (int i = 0; i < SINE_WAVE_SAMPLES; ++i) {
        // La fórmula convierte una onda sin() que va de -1 a 1, a una onda de 8-bit que va de 0 a 255
        sine_wave_data[i] = (uint8_t)(127.0f * sinf(2 * M_PI * SINE_WAVE_FREQUENCY * i / SAMPLE_RATE) + 128.0f);
    }
    
    voice.len = 0; // Inicialmente no está sonando nada
    voice.pos = 0;

    // El resto es la configuración estándar de PWM y DMA
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
    
    dma_irq_handler(); // Prepara el primer búfer con silencio
}

// --- Entrada de Botón (simplificada) ---
void button_callback(uint gpio, uint32_t events) {
    if (gpio == BUTTON_PIN) {
        play_sine_wave();
    }
}

// --- Bucle Principal ---
int main() {
    init_audio();

    // Configurar el único botón de prueba
    gpio_init(BUTTON_PIN);
    gpio_pull_up(BUTTON_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &button_callback);

    while (true) {
        // El CPU puede dormir, solo nos importan las interrupciones del botón y el DMA
        //__wfi();
    }
    return 0;
}