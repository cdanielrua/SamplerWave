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
#define MAX_VOICES 4
#define SAMPLE_RATE 22050
#define PWM_WRAP_VALUE 255
#define IS_RGBW false
#define NUM_PIXELS 16
#define SAMPLE_QUEUE_SIZE 8
#define BUFFER_SIZE 256        // Reducido para mejor rendimiento
#define SILENCE_LEVEL 128
#define DEBOUNCE_DELAY_US 50000

// Button pins
const uint BUTTON_PINS[NUM_SAMPLES] = {10, 11, 12, 13};

// --- PIO Program para WS2812 ---
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

// --- WS2812 Functions ---
static inline void ws2812_program_init(PIO pio, uint sm, uint offset, uint pin, float freq, bool rgbw) {
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 3);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_out_shift(&c, false, true, rgbw ? 32 : 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    float div = clock_get_hz(clk_sys) / (freq * 10);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t) (g) << 16) | ((uint32_t) (r) << 8) | (uint32_t) (b);
}

// --- Estructuras ---
typedef struct { 
    uint32_t pos; 
    uint32_t len;
    bool active;
    uint8_t sound_type;
} audio_voice_t;

typedef struct { 
    uint8_t queue[SAMPLE_QUEUE_SIZE]; 
    volatile uint8_t head; 
    volatile uint8_t tail; 
    volatile uint8_t count; 
} sample_queue_t;

// --- Variables Globales ---
static bool sequence_grid[NUM_SAMPLES][NUM_STEPS];
static volatile uint8_t current_step = 0;
static volatile uint bpm = 120;
static volatile bool step_changed = true;
static uint8_t dma_buffer[2][BUFFER_SIZE] __attribute__((aligned(4)));
static volatile uint8_t active_buffer = 0;
static int dma_chan;
static audio_voice_t voices[MAX_VOICES];
static volatile bool button_states[NUM_SAMPLES];
static volatile absolute_time_t last_press_time[NUM_SAMPLES];
static sample_queue_t sample_queue = {.head = 0, .tail = 0, .count = 0};
static uint slice_num;
static uint sm = 0;
static uint offset;
static volatile bool dma_busy = false;

// --- Duraciones de sonidos ---
static const uint32_t SOUND_DURATIONS[NUM_SAMPLES] = {
    SAMPLE_RATE / 2,      // Kick: 0.5 segundos
    SAMPLE_RATE / 3,      // Snare: 0.33 segundos
    SAMPLE_RATE / 8,      // Hi-Hat: 0.125 segundos
    SAMPLE_RATE / 4       // Clap: 0.25 segundos
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

// --- Generadores de sonido ---
static uint8_t generate_kick(uint32_t pos) {
    if (pos >= SOUND_DURATIONS[0]) return SILENCE_LEVEL;
    
    float t = (float)pos / SAMPLE_RATE;
    float decay = expf(-t * 8.0f);
    
    // Frecuencia que baja progresivamente
    float freq = 50.0f * (1.0f - t * 0.8f);
    float phase = 2.0f * M_PI * freq * t;
    
    float sample = sinf(phase) * decay;
    
    // Agregar click inicial
    if (pos < 100) {
        sample += (100.0f - pos) / 100.0f * 0.4f;
    }
    
    return (uint8_t)(SILENCE_LEVEL + sample * 90);
}

static uint8_t generate_snare(uint32_t pos) {
    if (pos >= SOUND_DURATIONS[1]) return SILENCE_LEVEL;
    
    float t = (float)pos / SAMPLE_RATE;
    float decay = expf(-t * 6.0f);
    
    // Ruido blanco simple
    static uint32_t lfsr = 0xACE1u;
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
    float noise = ((float)(lfsr & 0xFFFF) / 32768.0f) - 1.0f;
    
    // Tono fundamental
    float tone = sinf(2.0f * M_PI * 200.0f * t) * 0.5f;
    
    float sample = (noise * 0.8f + tone * 0.2f) * decay;
    return (uint8_t)(SILENCE_LEVEL + sample * 70);
}

static uint8_t generate_hihat(uint32_t pos) {
    if (pos >= SOUND_DURATIONS[2]) return SILENCE_LEVEL;
    
    float t = (float)pos / SAMPLE_RATE;
    float decay = expf(-t * 20.0f);
    
    // Ruido de alta frecuencia
    static uint32_t lfsr = 0xC0DEu;
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xD008u);
    float noise = ((float)(lfsr & 0xFFFF) / 32768.0f) - 1.0f;
    
    float sample = noise * decay;
    return (uint8_t)(SILENCE_LEVEL + sample * 50);
}

static uint8_t generate_clap(uint32_t pos) {
    if (pos >= SOUND_DURATIONS[3]) return SILENCE_LEVEL;
    
    float t = (float)pos / SAMPLE_RATE;
    float decay = expf(-t * 4.0f);
    
    // Patrón de múltiples claps
    float pattern = 1.0f;
    uint32_t t_ms = (uint32_t)(t * 1000.0f);
    
    if (t_ms > 40 && t_ms < 60) pattern = 0.2f;
    else if (t_ms > 80 && t_ms < 100) pattern = 0.6f;
    else if (t_ms > 120 && t_ms < 140) pattern = 0.4f;
    else if (t_ms > 160 && t_ms < 180) pattern = 0.8f;
    else if (t_ms > 200) pattern = 0.3f;
    
    // Ruido
    static uint32_t lfsr = 0xBEEFu;
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xD008u);
    float noise = ((float)(lfsr & 0xFFFF) / 32768.0f) - 1.0f;
    
    float sample = noise * decay * pattern;
    return (uint8_t)(SILENCE_LEVEL + sample * 60);
}

// --- Cola de samples ---
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

// --- Motor de audio ---
static void play_sample(uint sample_index) {
    if (sample_index >= NUM_SAMPLES) return;
    
    // Buscar voz libre
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!voices[i].active) {
            voices[i].sound_type = sample_index;
            voices[i].len = SOUND_DURATIONS[sample_index];
            voices[i].pos = 0;
            voices[i].active = true;
            return;
        }
    }
    
    // Si no hay voces libres, usar la primera
    voices[0].sound_type = sample_index;
    voices[0].len = SOUND_DURATIONS[sample_index];
    voices[0].pos = 0;
    voices[0].active = true;
}

// --- Llenar buffer de audio ---
static void fill_audio_buffer(uint8_t *buffer) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        int32_t mixed_sample = 0;
        int active_voices = 0;
        
        // Mezclar voces activas
        for (int voice_idx = 0; voice_idx < MAX_VOICES; voice_idx++) {
            if (voices[voice_idx].active) {
                if (voices[voice_idx].pos < voices[voice_idx].len) {
                    uint8_t sample_value = SILENCE_LEVEL;
                    
                    switch (voices[voice_idx].sound_type) {
                        case 0: sample_value = generate_kick(voices[voice_idx].pos); break;
                        case 1: sample_value = generate_snare(voices[voice_idx].pos); break;
                        case 2: sample_value = generate_hihat(voices[voice_idx].pos); break;
                        case 3: sample_value = generate_clap(voices[voice_idx].pos); break;
                    }
                    
                    mixed_sample += (int32_t)sample_value - SILENCE_LEVEL;
                    active_voices++;
                    voices[voice_idx].pos++;
                } else {
                    voices[voice_idx].active = false;
                }
            }
        }
        
        // Mezclar y normalizar
        if (active_voices > 0) {
            mixed_sample = mixed_sample / active_voices;
        }
        
        mixed_sample += SILENCE_LEVEL;
        
        // Limitar
        if (mixed_sample > 255) mixed_sample = 255;
        else if (mixed_sample < 0) mixed_sample = 0;
        
        buffer[i] = (uint8_t)mixed_sample;
    }
}

// --- DMA Handler corregido ---
static void dma_irq_handler(void) {
    // Limpiar interrupción DMA
    if (dma_hw->ints0 & (1u << dma_chan)) {
        dma_hw->ints0 = 1u << dma_chan;
        
        // Cambiar buffer activo
        active_buffer = 1 - active_buffer;
        
        // Llenar el buffer que no se está usando
        fill_audio_buffer(dma_buffer[active_buffer]);
        
        // Configurar DMA para el próximo buffer
        dma_channel_set_read_addr(dma_chan, dma_buffer[active_buffer], true);
    }
}

// --- Inicialización de audio corregida ---
static void init_audio(void) {
    // Inicializar voces
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].active = false;
        voices[i].pos = 0;
        voices[i].sound_type = 0;
        voices[i].len = 0;
    }
    
    // Configurar PWM
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(AUDIO_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, PWM_WRAP_VALUE);
    
    // Calcular divisor de reloj
    uint32_t sys_clock = clock_get_hz(clk_sys);
    float clock_div = (float)sys_clock / (SAMPLE_RATE * (PWM_WRAP_VALUE + 1));
    
    pwm_config_set_clkdiv(&config, clock_div);
    pwm_init(slice_num, &config, true);
    
    // Configurar DMA
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_config = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, pwm_get_dreq(slice_num));
    
    // Inicializar buffers
    for (int i = 0; i < BUFFER_SIZE; i++) {
        dma_buffer[0][i] = SILENCE_LEVEL;
        dma_buffer[1][i] = SILENCE_LEVEL;
    }
    
    // Configurar DMA
    dma_channel_configure(dma_chan, &dma_config, 
                         &pwm_hw->slice[slice_num].cc, 
                         dma_buffer[0], 
                         BUFFER_SIZE, 
                         true);
    
    // Configurar interrupción DMA
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    
    printf("Audio initialized - Sample rate: %d Hz\n", SAMPLE_RATE);
}

// --- LEDs con parpadeo controlado para múltiples samples ---
static void update_leds(void) {
    // Contador estable para el parpadeo (se actualiza cada vez que se llama)
    static uint32_t blink_counter = 0;
    blink_counter++;
    
    for (int i = 0; i < NUM_PIXELS; i++) {
        uint32_t color = 0;
        
        if (i == current_step) {
            // Step actual - blanco brillante constante
            color = urgb_u32(255, 255, 255);
        } else {
            // Verificar qué samples están activos en este step
            bool has_kick = sequence_grid[0][i];
            bool has_snare = sequence_grid[1][i];
            bool has_hihat = sequence_grid[2][i];
            bool has_clap = sequence_grid[3][i];
            
            // Crear array de samples activos
            int active_samples[NUM_SAMPLES];
            int active_count = 0;
            
            if (has_kick) active_samples[active_count++] = 0;
            if (has_snare) active_samples[active_count++] = 1;
            if (has_hihat) active_samples[active_count++] = 2;
            if (has_clap) active_samples[active_count++] = 3;
            
            if (active_count == 0) {
                // Sin samples - LED muy tenue pero visible
                color = urgb_u32(2, 2, 2);
            } else if (active_count == 1) {
                // Un sample - color fijo y brillante
                switch (active_samples[0]) {
                    case 0: color = urgb_u32(100, 0, 0); break;   // Kick - Rojo
                    case 1: color = urgb_u32(0, 100, 0); break;   // Snare - Verde
                    case 2: color = urgb_u32(0, 0, 100); break;   // Hi-Hat - Azul
                    case 3: color = urgb_u32(100, 100, 0); break; // Clap - Amarillo
                }
            } else {
                // Múltiples samples - PARPADEO entre colores
                // Período de parpadeo más lento y estable
                uint32_t blink_period = 80;  // Cambiar cada 80 llamadas
                uint32_t current_color_index = (blink_counter / blink_period) % active_count;
                
                // Mostrar el color del sample actual en la rotación
                switch (active_samples[current_color_index]) {
                    case 0: color = urgb_u32(120, 0, 0); break;   // Kick - Rojo brillante
                    case 1: color = urgb_u32(0, 120, 0); break;   // Snare - Verde brillante
                    case 2: color = urgb_u32(0, 0, 120); break;   // Hi-Hat - Azul brillante
                    case 3: color = urgb_u32(120, 120, 0); break; // Clap - Amarillo brillante
                }
            }
        }
        
        put_pixel(color);
    }
}

// --- Funciones de botones ---
static void button_callback(uint gpio, uint32_t events) {
    absolute_time_t now = get_absolute_time();
    
    for (int i = 0; i < NUM_SAMPLES; i++) {
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
                        
                        // Modificar grid
                        sequence_grid[i][current_step] = !sequence_grid[i][current_step];
                        step_changed = true;
                    }
                }
            }
            break;
        }
    }
}

static void init_buttons(void) {
    for (int i = 0; i < NUM_SAMPLES; i++) {
        gpio_init(BUTTON_PINS[i]);
        gpio_set_dir(BUTTON_PINS[i], GPIO_IN);
        gpio_pull_up(BUTTON_PINS[i]);
        button_states[i] = false;
        last_press_time[i] = nil_time;
        gpio_set_irq_enabled_with_callback(BUTTON_PINS[i], GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &button_callback);
    }
    printf("Buttons initialized\n");
}

static void check_and_advance_sequencer(uint32_t* step_delay_us, absolute_time_t* next_step_time) {
    if (time_reached(*next_step_time)) {
        current_step = (current_step + 1) % NUM_STEPS;
        
        // Tocar samples del step actual
        for (int i = 0; i < NUM_SAMPLES; i++) {
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
    printf("Drum Machine Starting...\n");
    
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
    
    // Prueba de sonidos
    printf("Testing sounds...\n");
    for (int i = 0; i < NUM_SAMPLES; i++) {
        queue_sample(i);
        sleep_ms(400);
    }
    
    // Variables para el loop principal
    uint32_t bpm_counter = 0;
    uint32_t led_counter = 0;
    
    // Loop principal
    while (true) {
        // Procesar cola de samples
        process_sample_queue();
        
        // Leer BPM cada cierto tiempo
        if (++bpm_counter >= 2000) {
            bpm_counter = 0;
            uint16_t adc_value = adc_read();
            uint new_bpm = 60 + (adc_value * 160 / 4095);
            if (new_bpm != bpm) {
                bpm = new_bpm;
                step_delay_us = 60 * 1000 * 1000 / (bpm * 4);
                printf("BPM: %d\n", bpm);
            }
        }
        
        // Verificar secuenciador
        check_and_advance_sequencer(&step_delay_us, &next_step_time);
        
        // Actualizar LEDs más frecuentemente para el parpadeo suave
        if (step_changed || (++led_counter >= 200)) {
            update_leds();
            step_changed = false;
            led_counter = 0;
        }
        
        // Pequeño delay
        sleep_us(50);
    }
    
    return 0;
}