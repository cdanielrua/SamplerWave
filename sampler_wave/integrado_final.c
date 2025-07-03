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

// --- Definiciones ---
#define AUDIO_PIN 0
#define POT_PIN 26
#define LED_PIN 16
#define NUM_SAMPLES 4
#define NUM_STEPS 16
#define MAX_VOICES 8
#define SAMPLE_RATE 44100  // Aumentado para mejor calidad
#define PWM_WRAP_VALUE 255
#define IS_RGBW false
#define NUM_PIXELS 16
#define SAMPLE_QUEUE_SIZE 16
#define BUFFER_SIZE 1024   // Aumentado para mejor audio
#define SILENCE_LEVEL 128
#define DEBOUNCE_DELAY_US 50000

// Button pins
const uint BUTTON_PINS[NUM_SAMPLES] = {10, 11, 12, 13};

// --- PIO Program para WS2812 (Corregido) ---
static const uint16_t ws2812_program_instructions[] = {
    0x6221, // out    x, 1            side 0 [2]
    0x1123, // jmp    !x, 3           side 1 [1]
    0x1400, // jmp    0               side 1 [4]
    0xa442, // nop                    side 0 [4]
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length = 4,
    .origin = -1,
};

// Función mejorada para WS2812
static inline void ws2812_program_init(PIO pio, uint sm, uint offset, uint pin, float freq, bool rgbw) {
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 3);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_out_shift(&c, false, true, rgbw ? 32 : 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    float div = clock_get_hz(clk_sys) / (freq * 10); // Ajustado para timing correcto
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

// Función para enviar color a WS2812
static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

// Función para crear color GRB
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t) (g) << 16) | ((uint32_t) (r) << 8) | (uint32_t) (b);
}

// --- Estructuras y Variables Globales ---
typedef struct { 
    uint32_t pos; 
    uint32_t len;
    uint32_t phase;
    bool active;
    float volume;
    uint8_t sound_type;
    float freq_base;      // Frecuencia base para el sonido
    float freq_mod;       // Modificador de frecuencia
} audio_voice_t;

typedef struct { 
    uint8_t queue[SAMPLE_QUEUE_SIZE]; 
    volatile uint8_t head; 
    volatile uint8_t tail; 
    volatile uint8_t count; 
} sample_queue_t;

static bool sequence_grid[NUM_SAMPLES][NUM_STEPS];
static volatile uint8_t current_step = 0;
static volatile uint bpm = 120;
static volatile bool step_changed = true;
static uint8_t dma_buffer[2][BUFFER_SIZE];
static volatile uint8_t active_buffer = 0;
static int dma_chan;
static audio_voice_t voices[MAX_VOICES];
static volatile bool button_states[NUM_SAMPLES];
static volatile absolute_time_t last_press_time[NUM_SAMPLES];
static sample_queue_t sample_queue = {.head = 0, .tail = 0, .count = 0};
static volatile uint32_t audio_sample_counter = 0;
static uint slice_num;
static uint sm = 0;
static uint offset;

// --- Duraciones de los sonidos (en samples) ---
static const uint32_t SOUND_DURATIONS[NUM_SAMPLES] = {
    SAMPLE_RATE * 2,      // Kick: 2 segundos
    SAMPLE_RATE * 1,      // Snare: 1 segundo
    SAMPLE_RATE / 2,      // Hi-Hat: 0.5 segundos
    SAMPLE_RATE * 1       // Clap: 1 segundo
};

// --- Prototipos ---
static void update_leds(void);
static bool queue_sample(uint8_t sample_index);
static void process_sample_queue(void);
static void play_sample(uint sample_index);
static void dma_irq_handler(void);
static void init_audio(void);
static void button_callback(uint gpio, uint32_t events);
static void init_buttons(void);
static void check_and_advance_sequencer(uint32_t* step_delay_us, absolute_time_t* next_step_time);
static void fill_audio_buffer(uint8_t *buffer);

// --- Funciones de Generación de Sonidos Mejoradas ---
static uint8_t generate_kick(uint32_t pos, uint32_t total_time) {
    float t = (float)pos / SAMPLE_RATE;
    float decay = expf(-t * 8.0f);  // Decay más rápido
    
    // Frecuencia que baja de 60Hz a 30Hz
    float freq = 60.0f - (30.0f * t / 2.0f);
    float phase = 2.0f * M_PI * freq * t;
    
    // Onda seno con un poco de distorsión
    float sample = sinf(phase) * decay;
    sample += sinf(phase * 2.0f) * decay * 0.3f;  // Armónico
    
    // Añadir click inicial
    if (pos < 1000) {
        sample += (1000.0f - pos) / 1000.0f * 0.5f;
    }
    
    return (uint8_t)(SILENCE_LEVEL + sample * 120);
}

static uint8_t generate_snare(uint32_t pos, uint32_t total_time) {
    float t = (float)pos / SAMPLE_RATE;
    float decay = expf(-t * 6.0f);
    
    // Generador de ruido mejorado
    static uint32_t lfsr = 0xACE1u;
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
    float noise = ((float)lfsr / 65536.0f) - 0.5f;
    
    // Tono fundamental a 200Hz
    float tone = sinf(2.0f * M_PI * 200.0f * t) * 0.3f;
    
    float sample = (noise * 0.8f + tone * 0.2f) * decay;
    return (uint8_t)(SILENCE_LEVEL + sample * 100);
}

static uint8_t generate_hihat(uint32_t pos, uint32_t total_time) {
    float t = (float)pos / SAMPLE_RATE;
    float decay = expf(-t * 12.0f);  // Decay muy rápido
    
    // Ruido de alta frecuencia
    static uint32_t lfsr1 = 0xACE1u;
    static uint32_t lfsr2 = 0xC0DEu;
    
    lfsr1 = (lfsr1 >> 1) ^ (-(lfsr1 & 1u) & 0xB400u);
    lfsr2 = (lfsr2 >> 1) ^ (-(lfsr2 & 1u) & 0xD008u);
    
    float noise = ((float)(lfsr1 ^ lfsr2) / 65536.0f) - 0.5f;
    
    // Filtro paso-alto simple
    static float last_noise = 0;
    float filtered = noise - last_noise;
    last_noise = noise;
    
    float sample = filtered * decay;
    return (uint8_t)(SILENCE_LEVEL + sample * 80);
}

static uint8_t generate_clap(uint32_t pos, uint32_t total_time) {
    float t = (float)pos / SAMPLE_RATE;
    float decay = expf(-t * 3.0f);
    
    // Patrón de clap con múltiples picos
    float pattern = 1.0f;
    float t_ms = t * 1000.0f;
    
    if (t_ms > 50 && t_ms < 80) pattern = 0.3f;
    else if (t_ms > 120 && t_ms < 150) pattern = 0.7f;
    else if (t_ms > 180 && t_ms < 210) pattern = 0.5f;
    else if (t_ms > 250 && t_ms < 280) pattern = 0.8f;
    else if (t_ms > 350) pattern = 0.2f;
    
    // Ruido modulado
    static uint32_t lfsr = 0xBEEFu;
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xD008u);
    float noise = ((float)lfsr / 65536.0f) - 0.5f;
    
    float sample = noise * decay * pattern;
    return (uint8_t)(SILENCE_LEVEL + sample * 90);
}

// --- Lógica de la Cola de Samples ---
static bool queue_sample(uint8_t sample_index) {
    uint32_t interrupts = save_and_disable_interrupts();
    if (sample_queue.count >= SAMPLE_QUEUE_SIZE) {
        restore_interrupts(interrupts);
        return false;
    }
    sample_queue.queue[sample_queue.head] = sample_index;
    sample_queue.head = (sample_queue.head + 1) % SAMPLE_QUEUE_SIZE;
    sample_queue.count++;
    restore_interrupts(interrupts);
    return true;
}

static void process_sample_queue(void) {
    while (sample_queue.count > 0) {
        uint32_t interrupts = save_and_disable_interrupts();
        uint8_t sample_index = sample_queue.queue[sample_queue.tail];
        sample_queue.tail = (sample_queue.tail + 1) % SAMPLE_QUEUE_SIZE;
        sample_queue.count--;
        restore_interrupts(interrupts);
        play_sample(sample_index);
    }
}

// --- Motor de Audio ---
static void play_sample(uint sample_index) {
    if (sample_index >= NUM_SAMPLES) return;
    
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!voices[i].active) {
            voices[i].sound_type = sample_index;
            voices[i].len = SOUND_DURATIONS[sample_index];
            voices[i].pos = 0;
            voices[i].phase = 0;
            voices[i].active = true;
            voices[i].volume = 1.0f;
            return;
        }
    }
    
    // Si no hay voces libres, usar la primera
    voices[0].sound_type = sample_index;
    voices[0].len = SOUND_DURATIONS[sample_index];
    voices[0].pos = 0;
    voices[0].phase = 0;
    voices[0].active = true;
    voices[0].volume = 1.0f;
}

static void fill_audio_buffer(uint8_t *buffer) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        int32_t mixed_sample = 0;
        
        // Mezclar todas las voces activas
        for (int voice_idx = 0; voice_idx < MAX_VOICES; ++voice_idx) {
            if (voices[voice_idx].active && voices[voice_idx].pos < voices[voice_idx].len) {
                uint8_t sample_value = SILENCE_LEVEL;
                
                switch (voices[voice_idx].sound_type) {
                    case 0:
                        sample_value = generate_kick(voices[voice_idx].pos, voices[voice_idx].len);
                        break;
                    case 1:
                        sample_value = generate_snare(voices[voice_idx].pos, voices[voice_idx].len);
                        break;
                    case 2:
                        sample_value = generate_hihat(voices[voice_idx].pos, voices[voice_idx].len);
                        break;
                    case 3:
                        sample_value = generate_clap(voices[voice_idx].pos, voices[voice_idx].len);
                        break;
                }
                
                mixed_sample += (int32_t)sample_value - SILENCE_LEVEL;
                voices[voice_idx].pos++;
                
                if (voices[voice_idx].pos >= voices[voice_idx].len) {
                    voices[voice_idx].active = false;
                }
            }
        }
        
        // Limitar y convertir a uint8_t
        mixed_sample += SILENCE_LEVEL;
        if (mixed_sample > 255) mixed_sample = 255;
        if (mixed_sample < 0) mixed_sample = 0;
        
        buffer[i] = (uint8_t)mixed_sample;
    }
}

static void dma_irq_handler(void) {
    dma_hw->ints0 = 1u << dma_chan;
    
    uint8_t* next_buffer = dma_buffer[1 - active_buffer];
    fill_audio_buffer(next_buffer);
    
    dma_channel_set_read_addr(dma_chan, next_buffer, true);
    active_buffer = 1 - active_buffer;
}

static void init_audio(void) {
    // Inicializar voces
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].active = false;
        voices[i].pos = 0;
        voices[i].phase = 0;
        voices[i].volume = 1.0f;
        voices[i].sound_type = 0;
    }
    
    // Configurar PWM para audio
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(AUDIO_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, PWM_WRAP_VALUE);
    
    // Calcular divisor para sample rate exacto
    uint32_t sys_clock = clock_get_hz(clk_sys);
    float clock_div = (float)sys_clock / (SAMPLE_RATE * (PWM_WRAP_VALUE + 1));
    
    if (clock_div < 1.0f) clock_div = 1.0f;
    
    pwm_config_set_clkdiv(&config, clock_div);
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(AUDIO_PIN, SILENCE_LEVEL);

    // Configurar DMA
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_config = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, pwm_get_dreq(slice_num));
    
    fill_audio_buffer(dma_buffer[0]);
    fill_audio_buffer(dma_buffer[1]);
    
    dma_channel_configure(dma_chan, &dma_config, &pwm_hw->slice[slice_num].cc, dma_buffer[0], BUFFER_SIZE, true);
    active_buffer = 0;

    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    
    printf("Audio initialized - Sample rate: %d Hz\n", SAMPLE_RATE);
}

// --- Lógica de LEDs Mejorada ---
static void update_leds(void) {
    static uint32_t counter = 0;
    counter++;
    
    for (int i = 0; i < NUM_PIXELS; ++i) {
        uint32_t color = 0;
        
        if (i == current_step) {
            // Step actual - blanco brillante
            color = urgb_u32(255, 255, 255);
        } else {
            // Contar samples activos en este step
            int active_count = 0;
            int active_samples[NUM_SAMPLES];
            
            for (int j = 0; j < NUM_SAMPLES; j++) {
                if (sequence_grid[j][i]) {
                    active_samples[active_count] = j;
                    active_count++;
                }
            }
            
            if (active_count == 0) {
                // Sin samples - LED muy tenue
                color = urgb_u32(2, 2, 2);
            } else if (active_count == 1) {
                // Un sample - color fijo
                switch (active_samples[0]) {
                    case 0: color = urgb_u32(80, 0, 0); break;   // Kick - Rojo
                    case 1: color = urgb_u32(0, 80, 0); break;   // Snare - Verde
                    case 2: color = urgb_u32(0, 0, 80); break;   // Hi-Hat - Azul
                    case 3: color = urgb_u32(80, 80, 0); break;  // Clap - Amarillo
                }
            } else {
                // Múltiples samples - alternar colores
                int current_color = (counter / 50) % active_count;
                switch (active_samples[current_color]) {
                    case 0: color = urgb_u32(120, 0, 0); break;   // Kick - Rojo más brillante
                    case 1: color = urgb_u32(0, 120, 0); break;   // Snare - Verde más brillante
                    case 2: color = urgb_u32(0, 0, 120); break;   // Hi-Hat - Azul más brillante
                    case 3: color = urgb_u32(120, 120, 0); break; // Clap - Amarillo más brillante
                }
            }
        }
        
        put_pixel(color);
    }
}

// --- Funciones de Botones ---
static void button_callback(uint gpio, uint32_t events) {
    absolute_time_t now = get_absolute_time();
    
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        if (gpio == BUTTON_PINS[i]) {
            bool current_state = !gpio_get(gpio);
            
            if (current_state != button_states[i]) {
                uint64_t diff_us = absolute_time_diff_us(last_press_time[i], now);
                
                if (diff_us > DEBOUNCE_DELAY_US) {
                    button_states[i] = current_state;
                    last_press_time[i] = now;
                    
                    if (current_state) {
                        printf("Button %d pressed\n", i);
                        queue_sample(i);
                        
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

static void check_and_advance_sequencer(uint32_t* step_delay_us, absolute_time_t* next_step_time) {
    if (time_reached(*next_step_time)) {
        current_step = (current_step + 1) % NUM_STEPS;
        
        for (int i = 0; i < NUM_SAMPLES; ++i) {
            if (sequence_grid[i][current_step]) {
                queue_sample(i);
            }
        }
        
        step_changed = true;
        *next_step_time = delayed_by_us(*next_step_time, *step_delay_us);
    }
}

// --- Función Principal ---
int main() {
    stdio_init_all();
    printf("Complete Drum Machine Starting...\n");
    
    // Inicializar arrays
    for (int i = 0; i < NUM_SAMPLES; i++) {
        for (int j = 0; j < NUM_STEPS; j++) {
            sequence_grid[i][j] = false;
        }
        button_states[i] = false;
        last_press_time[i] = nil_time;
    }
    
    // Inicializar WS2812
    PIO pio = pio0;
    offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, LED_PIN, 800000, IS_RGBW);
    
    // Inicializar ADC
    adc_init();
    adc_gpio_init(POT_PIN);
    adc_select_input(0);
    
    // Inicializar sistemas
    init_audio();
    init_buttons();
    
    // Configurar secuenciador
    uint32_t step_delay_us = 60 * 1000 * 1000 / (bpm * 4);
    absolute_time_t next_step_time = delayed_by_us(get_absolute_time(), step_delay_us);
    
    // LEDs iniciales
    update_leds();
    
    printf("Drum Machine Ready!\n");
    printf("- Buttons: GPIO 10-13 (Kick, Snare, Hi-Hat, Clap)\n");
    printf("- Potentiometer: GPIO 26 (BPM)\n");
    printf("- LEDs: GPIO 16 (WS2812)\n");
    
    // Prueba de sonido
    printf("Testing sounds...\n");
    for (int i = 0; i < NUM_SAMPLES; i++) {
        queue_sample(i);
        sleep_ms(500);
    }
    
    // Loop principal
    while (true) {
        process_sample_queue();
        
        // Leer BPM del potenciómetro
        uint16_t adc_value = adc_read();
        uint new_bpm = 60 + (adc_value * 160 / 4095);
        if (new_bpm != bpm) {
            bpm = new_bpm;
            step_delay_us = 60 * 1000 * 1000 / (bpm * 4);
            printf("BPM: %d\n", bpm);
        }
        
        check_and_advance_sequencer(&step_delay_us, &next_step_time);
        
        if (step_changed) {
            update_leds();
            step_changed = false;
        }
        
        // Actualizar LEDs para animación
        static uint32_t led_counter = 0;
        led_counter++;
        if (led_counter % 2000 == 0) {
            update_leds();
        }
        
        sleep_us(50);
    }
    
    return 0;
}