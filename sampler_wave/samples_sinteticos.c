#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/sync.h"
#include "ws2812.pio.h"

// --- Definiciones ---
#define AUDIO_PIN 0
#define POT_PIN 26
#define LED_PIN 16
static const uint BUTTON_PINS[] = {10, 11, 12, 13};
#define NUM_SAMPLES 4
#define NUM_STEPS 16
#define SAMPLE_RATE 8000  // Reducido para mejor rendimiento
#define PWM_WRAP_VALUE 255
#define IS_RGBW false
#define NUM_PIXELS 16
#define BUFFER_SIZE 256   // Reducido
static const uint32_t DEBOUNCE_DELAY_US = 50000;  // 50ms debounce

// --- Tipos de sonidos sintéticos ---
typedef enum {
    SOUND_KICK = 0,
    SOUND_SNARE = 1,
    SOUND_HIHAT = 2,
    SOUND_CLAP = 3
} sound_type_t;

typedef struct {
    sound_type_t type;
    uint32_t phase;
    uint32_t duration;
    uint32_t time_left;
    float amplitude;
    bool active;
} synth_voice_t;

// --- Variables Globales ---
static bool sequence_grid[NUM_SAMPLES][NUM_STEPS] = {{false}};
static volatile uint8_t current_step = 0;
static volatile uint bpm = 120;
static volatile bool step_changed = true;
static uint8_t dma_buffer[2][BUFFER_SIZE];
static volatile uint8_t active_buffer = 0;
static int dma_chan;
static synth_voice_t voices[8];  // Máximo 8 voces
static volatile bool button_states[NUM_SAMPLES] = {false};
static volatile absolute_time_t last_press_time[NUM_SAMPLES];
static volatile uint32_t sample_counter = 0;

// --- Prototipos ---
static void update_leds(void);
static void trigger_sound(sound_type_t type);
static void dma_irq_handler(void);
static void init_audio(void);
static void button_callback(uint gpio, uint32_t events);
static void init_buttons(void);
static void check_and_advance_sequencer(uint32_t* step_delay_us, absolute_time_t* next_step_time);
static void fill_audio_buffer(uint8_t *buffer);
static uint8_t generate_kick(synth_voice_t* voice);
static uint8_t generate_snare(synth_voice_t* voice);
static uint8_t generate_hihat(synth_voice_t* voice);
static uint8_t generate_clap(synth_voice_t* voice);

// --- Generadores de sonidos sintéticos ---
static uint8_t generate_kick(synth_voice_t* voice) {
    // Kick: Onda senoidal de frecuencia decreciente + ruido
    float freq = 80.0f * (1.0f - (float)(voice->duration - voice->time_left) / voice->duration);
    float phase_inc = (freq * 2.0f * M_PI) / SAMPLE_RATE;
    voice->phase += (uint32_t)(phase_inc * 1000000);
    
    float sine = sinf((float)voice->phase / 1000000.0f);
    float envelope = (float)voice->time_left / voice->duration;
    envelope *= envelope; // Envelope cuadrático para decay rápido
    
    return (uint8_t)(128 + (sine * 100 * envelope * voice->amplitude));
}

static uint8_t generate_snare(synth_voice_t* voice) {
    // Snare: Ruido blanco + tono a 200Hz
    float envelope = (float)voice->time_left / voice->duration;
    envelope *= envelope;
    
    // Ruido pseudo-aleatorio
    static uint32_t noise_seed = 1;
    noise_seed = noise_seed * 1103515245 + 12345;
    float noise = ((float)((noise_seed >> 16) & 0x7fff) / 32768.0f) - 1.0f;
    
    // Tono a 200Hz
    float phase_inc = (200.0f * 2.0f * M_PI) / SAMPLE_RATE;
    voice->phase += (uint32_t)(phase_inc * 1000000);
    float tone = sinf((float)voice->phase / 1000000.0f) * 0.3f;
    
    float mixed = (noise * 0.7f + tone) * envelope * voice->amplitude;
    return (uint8_t)(128 + mixed * 80);
}

static uint8_t generate_hihat(synth_voice_t* voice) {
    // Hi-Hat: Ruido blanco filtrado paso alto con envelope rápido
    float envelope = (float)voice->time_left / voice->duration;
    envelope = envelope * envelope * envelope; // Decay muy rápido
    
    // Ruido pseudo-aleatorio
    static uint32_t noise_seed = 7919;
    noise_seed = noise_seed * 1103515245 + 12345;
    float noise = ((float)((noise_seed >> 16) & 0x7fff) / 32768.0f) - 1.0f;
    
    // Simular filtro paso alto (diferencia entre samples)
    static float last_noise = 0;
    float filtered = noise - last_noise * 0.8f;
    last_noise = noise;
    
    float output = filtered * envelope * voice->amplitude;
    return (uint8_t)(128 + output * 60);
}

static uint8_t generate_clap(synth_voice_t* voice) {
    // Clap: Múltiples burst de ruido con envelope
    float envelope = (float)voice->time_left / voice->duration;
    
    // Crear pattern de clap con múltiples peaks
    float clap_pattern = 1.0f;
    float time_progress = 1.0f - envelope;
    
    if (time_progress < 0.1f) clap_pattern = 1.0f;
    else if (time_progress < 0.15f) clap_pattern = 0.3f;
    else if (time_progress < 0.25f) clap_pattern = 0.8f;
    else if (time_progress < 0.3f) clap_pattern = 0.2f;
    else if (time_progress < 0.4f) clap_pattern = 0.6f;
    else clap_pattern = 0.0f;
    
    // Ruido pseudo-aleatorio
    static uint32_t noise_seed = 12345;
    noise_seed = noise_seed * 1103515245 + 12345;
    float noise = ((float)((noise_seed >> 16) & 0x7fff) / 32768.0f) - 1.0f;
    
    float output = noise * envelope * clap_pattern * voice->amplitude;
    return (uint8_t)(128 + output * 70);
}

// --- Motor de Audio ---
static void trigger_sound(sound_type_t type) {
    // Buscar una voz libre
    for (int i = 0; i < 8; i++) {
        if (!voices[i].active) {
            voices[i].type = type;
            voices[i].phase = 0;
            voices[i].amplitude = 1.0f;
            voices[i].active = true;
            
            // Configurar duración según el tipo de sonido
            switch (type) {
                case SOUND_KICK:
                    voices[i].duration = SAMPLE_RATE / 4;  // 0.25 segundos
                    break;
                case SOUND_SNARE:
                    voices[i].duration = SAMPLE_RATE / 8;  // 0.125 segundos
                    break;
                case SOUND_HIHAT:
                    voices[i].duration = SAMPLE_RATE / 16; // 0.0625 segundos
                    break;
                case SOUND_CLAP:
                    voices[i].duration = SAMPLE_RATE / 6;  // ~0.17 segundos
                    break;
            }
            
            voices[i].time_left = voices[i].duration;
            printf("Sound %d triggered on voice %d\n", type, i);
            return;
        }
    }
    
    // Si no hay voces libres, usar la primera (reemplazar)
    voices[0].type = type;
    voices[0].phase = 0;
    voices[0].amplitude = 1.0f;
    voices[0].active = true;
    
    switch (type) {
        case SOUND_KICK:
            voices[0].duration = SAMPLE_RATE / 4;
            break;
        case SOUND_SNARE:
            voices[0].duration = SAMPLE_RATE / 8;
            break;
        case SOUND_HIHAT:
            voices[0].duration = SAMPLE_RATE / 16;
            break;
        case SOUND_CLAP:
            voices[0].duration = SAMPLE_RATE / 6;
            break;
    }
    
    voices[0].time_left = voices[0].duration;
    printf("Sound %d replaced voice 0\n", type);
}

static void fill_audio_buffer(uint8_t *buffer) {
    // Inicializar buffer con silencio (128 es el punto medio)
    for (int i = 0; i < BUFFER_SIZE; i++) {
        buffer[i] = 128;
    }
    
    // Generar y mezclar todas las voces activas
    for (int voice_idx = 0; voice_idx < 8; voice_idx++) {
        if (voices[voice_idx].active) {
            for (int sample_idx = 0; sample_idx < BUFFER_SIZE; sample_idx++) {
                if (voices[voice_idx].time_left > 0) {
                    uint8_t sample_value = 128;
                    
                    // Generar sample según el tipo
                    switch (voices[voice_idx].type) {
                        case SOUND_KICK:
                            sample_value = generate_kick(&voices[voice_idx]);
                            break;
                        case SOUND_SNARE:
                            sample_value = generate_snare(&voices[voice_idx]);
                            break;
                        case SOUND_HIHAT:
                            sample_value = generate_hihat(&voices[voice_idx]);
                            break;
                        case SOUND_CLAP:
                            sample_value = generate_clap(&voices[voice_idx]);
                            break;
                    }
                    
                    // Mezclar con el buffer existente
                    int buffer_signed = (int)buffer[sample_idx] - 128;
                    int sample_signed = (int)sample_value - 128;
                    int mixed = buffer_signed + sample_signed;
                    
                    // Clamp para evitar distorsión
                    if (mixed > 127) mixed = 127;
                    else if (mixed < -128) mixed = -128;
                    
                    buffer[sample_idx] = (uint8_t)(mixed + 128);
                    voices[voice_idx].time_left--;
                } else {
                    // Sonido terminado, marcar voz como inactiva
                    voices[voice_idx].active = false;
                    break;
                }
            }
        }
    }
    
    sample_counter++;
}

static void dma_irq_handler(void) {
    dma_hw->ints0 = 1u << dma_chan;
    
    // Llenar el buffer que no está siendo usado
    uint8_t* next_buffer = dma_buffer[1 - active_buffer];
    fill_audio_buffer(next_buffer);
    
    // Configurar el DMA para usar el buffer que acabamos de llenar
    dma_channel_set_read_addr(dma_chan, next_buffer, true);
    
    // Alternar los búferes
    active_buffer = 1 - active_buffer;
}

static void init_audio(void) {
    // Inicializar todas las voces como inactivas
    for (int i = 0; i < 8; i++) {
        voices[i].active = false;
        voices[i].phase = 0;
        voices[i].amplitude = 1.0f;
    }
    
    // Configurar PWM para audio
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(AUDIO_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, PWM_WRAP_VALUE);
    
    // Calcular divisor de reloj
    float clock_div = (float)clock_get_hz(clk_sys) / (SAMPLE_RATE * (PWM_WRAP_VALUE + 1));
    pwm_config_set_clkdiv(&config, clock_div);
    pwm_init(slice_num, &config, true);
    
    // Configurar PWM con nivel inicial en el medio
    pwm_set_gpio_level(AUDIO_PIN, 128);

    // Configurar DMA
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_config = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, pwm_get_dreq(slice_num));
    channel_config_set_chain_to(&dma_config, dma_chan);
    
    // Preparar búferes iniciales
    fill_audio_buffer(dma_buffer[0]);
    fill_audio_buffer(dma_buffer[1]);
    
    // Configurar e iniciar el DMA
    dma_channel_configure(dma_chan, &dma_config, &pwm_hw->slice[slice_num].cc, dma_buffer[0], BUFFER_SIZE, true);
    active_buffer = 0;

    // Configurar interrupción del DMA
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    irq_set_priority(DMA_IRQ_0, 0);
    
    printf("Audio initialized: Sample rate=%d, Clock div=%.2f\n", SAMPLE_RATE, clock_div);
}

// --- Lógica de LEDs ---
static void update_leds(void) {
    static uint32_t local_counter = 0;
    local_counter++;
    
    for (int i = 0; i < NUM_PIXELS; ++i) {
        if (i == current_step) {
            // LED blanco brillante para el step actual
            pio_sm_put_blocking(pio0, 0, 0x00808080 << 8u);
        } else {
            // Contar samples activos en este step
            uint32_t active_samples[NUM_SAMPLES];
            int num_active = 0;
            
            for(int j = 0; j < NUM_SAMPLES; ++j) {
                if(sequence_grid[j][i]) { 
                    active_samples[num_active] = j;
                    num_active++;
                }
            }
            
            if (num_active == 0) {
                // LED apagado
                pio_sm_put_blocking(pio0, 0, 0x00020202 << 8u);
            } else if (num_active == 1) {
                // Un solo sample - color fijo
                uint32_t color = 0x00020202;
                switch(active_samples[0]) {
                    case 0: color = 0x00200000; break; // Kick: Rojo
                    case 1: color = 0x00002000; break; // Snare: Verde
                    case 2: color = 0x00000020; break; // Hi-Hat: Azul
                    case 3: color = 0x00202000; break; // Clap: Amarillo
                }
                pio_sm_put_blocking(pio0, 0, color << 8u);
            } else {
                // Múltiples samples - alternar colores
                uint32_t blink_index = (local_counter / 30) % num_active;
                uint32_t color = 0x00020202;
                
                switch(active_samples[blink_index]) {
                    case 0: color = 0x00300000; break; // Kick: Rojo brillante
                    case 1: color = 0x00003000; break; // Snare: Verde brillante
                    case 2: color = 0x00000030; break; // Hi-Hat: Azul brillante
                    case 3: color = 0x00303000; break; // Clap: Amarillo brillante
                }
                pio_sm_put_blocking(pio0, 0, color << 8u);
            }
        }
    }
}

// --- Lógica de Botones ---
static void button_callback(uint gpio, uint32_t events) {
    absolute_time_t now = get_absolute_time();
    
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        if (gpio == BUTTON_PINS[i]) {
            bool current_state = !gpio_get(gpio);  // Invertir por pull-up
            
            if (current_state != button_states[i]) {
                uint64_t diff_us = absolute_time_diff_us(last_press_time[i], now);
                
                if (diff_us > DEBOUNCE_DELAY_US) {
                    button_states[i] = current_state;
                    last_press_time[i] = now;
                    
                    if (current_state) {
                        printf("Button %d pressed\n", i);
                        
                        // Reproducir sonido inmediatamente
                        trigger_sound((sound_type_t)i);
                        
                        // Toggle en el secuenciador
                        uint32_t interrupts = save_and_disable_interrupts();
                        sequence_grid[i][current_step] = !sequence_grid[i][current_step];
                        step_changed = true;
                        restore_interrupts(interrupts);
                    }
                }
            }
            break;
        }
    }
}

static void init_buttons(void) {
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        gpio_init(BUTTON_PINS[i]);
        gpio_set_dir(BUTTON_PINS[i], GPIO_IN);
        gpio_pull_up(BUTTON_PINS[i]);
        button_states[i] = false;
        sleep_ms(10);
        gpio_set_irq_enabled_with_callback(BUTTON_PINS[i], GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &button_callback);
    }
    printf("Buttons initialized\n");
}

// --- Secuenciador ---
static void check_and_advance_sequencer(uint32_t* step_delay_us, absolute_time_t* next_step_time) {
    if (time_reached(*next_step_time)) {
        current_step = (current_step + 1) % NUM_STEPS;
        
        // Reproducir sonidos programados
        for (int i = 0; i < NUM_SAMPLES; ++i) {
            if (sequence_grid[i][current_step]) {
                trigger_sound((sound_type_t)i);
            }
        }
        
        step_changed = true;
        *next_step_time = delayed_by_us(*next_step_time, *step_delay_us);
    }
}

// --- Función Principal ---
int main() {
    stdio_init_all();
    
    // Inicializar LEDs WS2812
    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, LED_PIN, 800000, IS_RGBW);
    
    // Inicializar ADC
    adc_init();
    adc_gpio_init(POT_PIN);
    adc_select_input(0);
    
    // Inicializar sistemas
    init_audio();
    init_buttons();
    
    // Inicializar estados
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        last_press_time[i] = nil_time;
        button_states[i] = false;
    }
    
    // Configurar timing del secuenciador
    uint32_t step_delay_us = 60 * 1000 * 1000 / (bpm * 4);
    absolute_time_t next_step_time = delayed_by_us(get_absolute_time(), step_delay_us);
    
    update_leds();
    
    printf("Drum machine started with synthetic sounds!\n");
    
    // Loop principal
    while (true) {
        // Leer potenciómetro y actualizar BPM
        uint16_t adc_value = adc_read();
        uint new_bpm = 60 + (adc_value * 160 / 4095);
        if (new_bpm != bpm) {
            bpm = new_bpm;
            step_delay_us = 60 * 1000 * 1000 / (bpm * 4);
        }
        
        // Verificar y avanzar secuenciador
        check_and_advance_sequencer(&step_delay_us, &next_step_time);
        
        // Actualizar LEDs
        if (step_changed) {
            update_leds();
            step_changed = false;
        }
        
        // Actualizar LEDs para animación
        static uint32_t led_counter = 0;
        led_counter++;
        if (led_counter % 1000 == 0) {
            update_leds();
        }
        
        sleep_us(100);
    }
    
    return 0;
}