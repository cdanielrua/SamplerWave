#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"

// --- Definiciones ---
#define LED_PIN 16
#define NUM_STEPS 16
#define NUM_PIXELS 16
#define IS_RGBW false

// --- Variables Globales ---
volatile uint8_t current_step = 0;
volatile bool step_changed = true;

// --- Funciones de LEDs ---
void update_leds() {
    for (int i = 0; i < NUM_PIXELS; ++i) {
        if (i == current_step) {
            // Playhead en blanco
            pio_sm_put_blocking(pio0, 0, 0x00FFFFFF << 8u);
        } else {
            // Pasos inactivos en morado oscuro
            pio_sm_put_blocking(pio0, 0, 0x00100010 << 8u);
        }
    }
}

// --- Secuenciador (Timer) ---
bool sequencer_timer_callback(struct repeating_timer *t) {
    current_step = (current_step + 1) % NUM_STEPS;
    step_changed = true;
    return true;
}

// --- Bucle Principal ---
int main() {
    // Inicializar PIO para los LEDs
    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, LED_PIN, 800000, IS_RGBW);

    // Configurar y empezar el temporizador con un tempo FIJO
    struct repeating_timer timer;
    add_repeating_timer_ms(-125, sequencer_timer_callback, NULL, &timer); // 120 BPM
    
    while (true) {
        if (step_changed) {
            update_leds();
            step_changed = false;
        }
    }
    return 0;
}