#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

// --- Definiciones Mejoradas ---
#define AUDIO_PIN 0
static const uint BUTTON_PINS[] = {10, 11, 12, 13};
#define NUM_BUTTONS 4
#define SAMPLE_RATE 20000  // Frecuencia mejorada para reducir pitido
#define PWM_WRAP_VALUE 255  // Volver a 8 bits temporalmente
#define SILENCE_LEVEL 128   // Punto medio para 8 bits

// --- Variables Globales ---
static volatile bool button_pressed[NUM_BUTTONS] = {false};
static volatile bool sound_playing = false;
static volatile uint32_t sound_timer = 0;
static volatile uint32_t sound_phase = 0;
static volatile uint8_t current_sound = 0;
static uint slice_num;
static uint8_t last_sample = 128;  // Para filtro de software

// --- Funciones de Generación de Sonidos Mejoradas ---
static uint8_t generate_kick(uint32_t phase, uint32_t time) {
    // Kick: Frecuencia decreciente de 80Hz a 40Hz
    float freq = 80.0f - (40.0f * time / 2000.0f);
    float phase_f = (float)phase * freq * 2.0f * M_PI / SAMPLE_RATE;
    
    // Envelope exponencial más natural
    float amplitude = expf(-2.0f * time / 2000.0f);
    if (amplitude < 0.001f) amplitude = 0;
    
    // Añadir un poco de armónico para más cuerpo
    float sample = sinf(phase_f) * amplitude * 0.9f;
    sample += sinf(phase_f * 2.0f) * amplitude * 0.2f;  // Segundo armónico sutil
    
    return (uint8_t)(SILENCE_LEVEL + sample * 100);
}

static uint8_t generate_snare(uint32_t phase, uint32_t time) {
    // Snare: Ruido + tono con envelope exponencial
    float amplitude = expf(-3.0f * time / 1000.0f);
    if (amplitude < 0.001f) amplitude = 0;
    
    // Ruido pseudo-aleatorio mejorado
    static uint32_t noise_seed = 1;
    noise_seed = (noise_seed * 1103515245 + 12345) & 0x7fffffff;
    float noise = ((float)(noise_seed % 1000) / 500.0f) - 1.0f;
    
    // Tono a 200Hz con armónico para más brillo
    float tone_phase = (float)phase * 200.0f * 2.0f * M_PI / SAMPLE_RATE;
    float tone = sinf(tone_phase) * 0.5f;
    tone += sinf(tone_phase * 1.3f) * 0.2f;  // Armónico no entero para más textura
    
    float sample = (noise * 0.7f + tone * 0.3f) * amplitude;
    return (uint8_t)(SILENCE_LEVEL + sample * 85);
}

static uint8_t generate_hihat(uint32_t phase, uint32_t time) {
    // Hi-Hat: Ruido con envelope exponencial muy rápido
    float amplitude = expf(-6.0f * time / 500.0f);
    if (amplitude < 0.001f) amplitude = 0;
    
    // Ruido de alta frecuencia con doble generador
    static uint32_t noise_seed = 7919;
    noise_seed = (noise_seed * 1103515245 + 12345) & 0x7fffffff;
    float noise1 = ((float)(noise_seed % 1000) / 500.0f) - 1.0f;
    
    // Segundo generador para más textura
    static uint32_t noise_seed2 = 31337;
    noise_seed2 = (noise_seed2 * 1664525 + 1013904223) & 0x7fffffff;
    float noise2 = ((float)(noise_seed2 % 1000) / 500.0f) - 1.0f;
    
    float sample = (noise1 * 0.6f + noise2 * 0.4f) * amplitude;
    return (uint8_t)(SILENCE_LEVEL + sample * 65);
}

static uint8_t generate_clap(uint32_t phase, uint32_t time) {
    // Clap: Ruido con pattern de burst y envelope exponencial
    float amplitude = expf(-1.5f * time / 1500.0f);
    if (amplitude < 0.001f) amplitude = 0;
    
    // Pattern de clap mejorado (múltiples peaks)
    float pattern = 1.0f;
    if (time > 80 && time < 120) pattern = 0.2f;
    else if (time > 180 && time < 220) pattern = 0.8f;
    else if (time > 280 && time < 320) pattern = 0.4f;
    else if (time > 380 && time < 420) pattern = 0.6f;
    else if (time > 500) pattern = 0.1f;
    
    // Ruido con mejor algoritmo
    static uint32_t noise_seed = 12345;
    noise_seed = (noise_seed * 1664525 + 1013904223) & 0x7fffffff;
    float noise = ((float)(noise_seed % 1000) / 500.0f) - 1.0f;
    
    float sample = noise * amplitude * pattern;
    return (uint8_t)(SILENCE_LEVEL + sample * 75);
}

// --- Callback del Timer con Filtro ---
static void pwm_interrupt_handler() {
    pwm_clear_irq(slice_num);
    
    if (sound_playing) {
        uint8_t sample = SILENCE_LEVEL;  // Silencio por defecto
        
        switch (current_sound) {
            case 0:
                sample = generate_kick(sound_phase, sound_timer);
                if (sound_timer > 2000) {
                    sound_playing = false;
                    pwm_set_enabled(slice_num, false);
                    pwm_set_gpio_level(AUDIO_PIN, SILENCE_LEVEL);
                }
                break;
            case 1:
                sample = generate_snare(sound_phase, sound_timer);
                if (sound_timer > 1000) {
                    sound_playing = false;
                    pwm_set_enabled(slice_num, false);
                    pwm_set_gpio_level(AUDIO_PIN, SILENCE_LEVEL);
                }
                break;
            case 2:
                sample = generate_hihat(sound_phase, sound_timer);
                if (sound_timer > 500) {
                    sound_playing = false;
                    pwm_set_enabled(slice_num, false);
                    pwm_set_gpio_level(AUDIO_PIN, SILENCE_LEVEL);
                }
                break;
            case 3:
                sample = generate_clap(sound_phase, sound_timer);
                if (sound_timer > 1500) {
                    sound_playing = false;
                    pwm_set_enabled(slice_num, false);
                    pwm_set_gpio_level(AUDIO_PIN, SILENCE_LEVEL);
                }
                break;
        }
        
        // Filtro paso-bajo simple para suavizar la señal
        sample = (sample * 3 + last_sample) / 4;
        last_sample = sample;
        
        pwm_set_gpio_level(AUDIO_PIN, sample);
        sound_phase++;
        sound_timer++;
    }
}

// --- Callback de Botones ---
static void button_callback(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_FALL) {
        for (int i = 0; i < NUM_BUTTONS; i++) {
            if (gpio == BUTTON_PINS[i]) {
                printf("Button %d pressed!\n", i);
                
                // Detener sonido actual si está reproduciéndose
                if (sound_playing) {
                    sound_playing = false;
                    pwm_set_enabled(slice_num, false);
                    pwm_set_gpio_level(AUDIO_PIN, SILENCE_LEVEL);
                    busy_wait_us(1000);  // Esperar un poco
                }
                
                // Iniciar nuevo sonido
                current_sound = i;
                sound_phase = 0;
                sound_timer = 0;
                sound_playing = true;
                
                // Habilitar PWM
                pwm_set_enabled(slice_num, true);
                
                break;
            }
        }
    }
}

// --- Inicialización ---
static void init_audio(void) {
    // Configurar GPIO para PWM
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(AUDIO_PIN);
    
    // Configurar PWM
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, PWM_WRAP_VALUE);
    
    // Calcular divisor para conseguir sample rate
    uint32_t sys_clock = clock_get_hz(clk_sys);
    float clock_div = (float)sys_clock / (SAMPLE_RATE * (PWM_WRAP_VALUE + 1));
    
    // Verificar que el divisor sea válido
    if (clock_div < 1.0f) {
        clock_div = 1.0f;
    }
    
    pwm_config_set_clkdiv(&config, clock_div);
    pwm_init(slice_num, &config, false);  // Inicialmente deshabilitado
    pwm_set_gpio_level(AUDIO_PIN, SILENCE_LEVEL);  // Nivel medio
    
    // Configurar interrupción del PWM
    pwm_clear_irq(slice_num);
    pwm_set_irq_enabled(slice_num, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_interrupt_handler);
    irq_set_enabled(PWM_IRQ_WRAP, true);
    
    float pwm_freq = (float)sys_clock / (clock_div * (PWM_WRAP_VALUE + 1));
    printf("Audio initialized:\n");
    printf("  System clock: %d Hz\n", sys_clock);
    printf("  Sample rate: %d Hz\n", SAMPLE_RATE);
    printf("  PWM frequency: %.1f Hz\n", pwm_freq);
    printf("  Clock div: %.2f\n", clock_div);
    printf("  PWM wrap: %d\n", PWM_WRAP_VALUE);
}

static void init_buttons(void) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        gpio_init(BUTTON_PINS[i]);
        gpio_set_dir(BUTTON_PINS[i], GPIO_IN);
        gpio_pull_up(BUTTON_PINS[i]);
        
        // Configurar interrupción
        gpio_set_irq_enabled_with_callback(BUTTON_PINS[i], GPIO_IRQ_EDGE_FALL, true, &button_callback);
    }
    printf("Buttons initialized\n");
}

// --- Función Principal ---
int main() {
    stdio_init_all();
    
    printf("Enhanced Audio Test Starting...\n");
    
    // Inicializar sistemas
    init_audio();
    init_buttons();
    
    printf("Ready! Press buttons to test improved sounds:\n");
    printf("Button 0 (GPIO 10): Kick (deeper, more natural)\n");
    printf("Button 1 (GPIO 11): Snare (brighter, crisper)\n");
    printf("Button 2 (GPIO 12): Hi-Hat (more textured)\n");
    printf("Button 3 (GPIO 13): Clap (realistic burst pattern)\n");
    printf("\nImprovements: Higher sample rate, exponential envelopes, software filtering\n");
    
    // Prueba inicial de sonido
    printf("\nTesting audio output...\n");
    current_sound = 0;
    sound_phase = 0;
    sound_timer = 0;
    sound_playing = true;
    pwm_set_enabled(slice_num, true);
    
    // Esperar a que termine el sonido de prueba
    sleep_ms(1000);
    while (sound_playing) {
        sleep_ms(100);
    }
    printf("Test complete!\n");
    
    // Loop principal
    while (true) {
        sleep_ms(100);
        
        // Mostrar estado cada 2 segundos
        static uint32_t counter = 0;
        counter++;
        if (counter % 20 == 0) {
            printf("Status: %s\n", sound_playing ? "Playing" : "Idle");
        }
    }
    
    return 0;
}