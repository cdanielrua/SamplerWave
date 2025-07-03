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
#include "audio_samples.h"

// --- Definiciones ---
#define AUDIO_PIN 0
#define POT_PIN 26
#define LED_PIN 16
static const uint BUTTON_PINS[] = {10, 11, 12, 13};
#define NUM_SAMPLES 4
#define NUM_STEPS 16
#define MAX_VOICES 8
#define SAMPLE_RATE 11250  // Corregido a 11250 Hz
#define PWM_WRAP_VALUE 255
#define IS_RGBW false
#define NUM_PIXELS 16
#define SAMPLE_QUEUE_SIZE 16
#define BUFFER_SIZE 512    // Aumentado para mejor rendimiento
static const uint32_t DEBOUNCE_DELAY_US = 50000;  // 50ms debounce

// --- Estructuras y Variables Globales ---
typedef struct { 
    const uint8_t *data; 
    uint32_t len; 
    uint32_t pos; 
    bool active;
    float volume;  // Agregado para control de volumen
} audio_voice_t;

typedef struct { 
    uint8_t queue[SAMPLE_QUEUE_SIZE]; 
    volatile uint8_t head; 
    volatile uint8_t tail; 
    volatile uint8_t count; 
} sample_queue_t;

// Definir los samples usando la estructura sample_t
static const sample_t samples[NUM_SAMPLES] = {
    {kick_sample_data, kick_sample_len},    // Botón 0: Kick
    {snare_sample_data, snare_sample_len},  // Botón 1: Snare
    {hat_sample_data, hat_sample_len},      // Botón 2: Hi-Hat
    {clap_sample_data, clap_sample_len}     // Botón 3: Clap
};

static bool sequence_grid[NUM_SAMPLES][NUM_STEPS] = {{false}};
static volatile uint8_t current_step = 0;
static volatile uint bpm = 120;
static volatile bool step_changed = true;
static uint8_t dma_buffer[2][BUFFER_SIZE];
static volatile uint8_t active_buffer = 0;
static int dma_chan;
static audio_voice_t voices[MAX_VOICES];
static volatile bool button_states[NUM_SAMPLES] = {false};
static volatile absolute_time_t last_press_time[NUM_SAMPLES];
static sample_queue_t sample_queue = {.head = 0, .tail = 0, .count = 0};
static volatile uint32_t led_blink_counter = 0;
static volatile uint32_t audio_sample_counter = 0;

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
    if (sample_index >= NUM_SAMPLES || samples[sample_index].len == 0) return;
    
    printf("Playing sample %d, length: %d\n", sample_index, samples[sample_index].len);
    
    // Buscar una voz libre
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!voices[i].active) {
            voices[i].data = samples[sample_index].data;
            voices[i].len = samples[sample_index].len;
            voices[i].pos = 0;
            voices[i].active = true;
            voices[i].volume = 1.0f;  // Volumen completo
            printf("Voice %d activated\n", i);
            return;
        }
    }
    
    // Si no hay voces libres, usar la primera (reemplazar)
    voices[0].data = samples[sample_index].data;
    voices[0].len = samples[sample_index].len;
    voices[0].pos = 0;
    voices[0].active = true;
    voices[0].volume = 1.0f;
    printf("Voice 0 replaced\n");
}

static void fill_audio_buffer(uint8_t *buffer) {
    // Inicializar buffer con silencio (128 es el punto medio para audio de 8 bits sin signo)
    for (int i = 0; i < BUFFER_SIZE; i++) {
        buffer[i] = 128;
    }
    
    // Mezclar todas las voces activas
    for (int voice_idx = 0; voice_idx < MAX_VOICES; ++voice_idx) {
        if (voices[voice_idx].active) {
            for (int sample_idx = 0; sample_idx < BUFFER_SIZE; ++sample_idx) {
                if (voices[voice_idx].pos < voices[voice_idx].len) {
                    // Tomar el sample directamente (ya está en formato correcto 0-255)
                    int sample_value = voices[voice_idx].data[voices[voice_idx].pos];
                    
                    // Aplicar volumen si es necesario
                    if (voices[voice_idx].volume != 1.0f) {
                        sample_value = (int)(sample_value * voices[voice_idx].volume);
                        if (sample_value > 255) sample_value = 255;
                        if (sample_value < 0) sample_value = 0;
                    }
                    
                    // Para mezclar, convertir a signed, mezclar y volver a unsigned
                    int buffer_signed = (int)buffer[sample_idx] - 128;
                    int sample_signed = sample_value - 128;
                    int mixed = buffer_signed + sample_signed;
                    
                    // Clamp
                    if (mixed > 127) mixed = 127;
                    else if (mixed < -128) mixed = -128;
                    
                    buffer[sample_idx] = (uint8_t)(mixed + 128);
                    voices[voice_idx].pos++;
                } else {
                    // Sample terminado, marcar voz como inactiva
                    voices[voice_idx].active = false;
                    break;
                }
            }
        }
    }
    
    // Incrementar contador para debugging
    audio_sample_counter++;
}

static void dma_irq_handler(void) {
    dma_hw->ints0 = 1u << dma_chan;
    
    // Llenar el buffer que no está siendo usado actualmente
    uint8_t* next_buffer = dma_buffer[1 - active_buffer];
    fill_audio_buffer(next_buffer);
    
    // Configurar el DMA para usar el buffer que acabamos de llenar
    dma_channel_set_read_addr(dma_chan, next_buffer, true);
    
    // Alternar los búferes
    active_buffer = 1 - active_buffer;
}

static void init_audio(void) {
    // Inicializar todas las voces como inactivas
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].active = false;
        voices[i].pos = 0;
        voices[i].volume = 1.0f;  // Volumen completo inicialmente
    }
    
    // Configurar PWM para audio
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(AUDIO_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, PWM_WRAP_VALUE);
    
    // Calcular divisor de reloj más preciso
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
    channel_config_set_chain_to(&dma_config, dma_chan);  // Auto-reiniciar
    
    // Preparar ambos búferes con silencio inicialmente
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

// --- Lógica de Interfaz y Secuenciador ---
static void update_leds(void) {
    static uint32_t local_counter = 0;
    local_counter++;
    
    for (int i = 0; i < NUM_PIXELS; ++i) {
        if (i == current_step) {
            // LED blanco brillante para el step actual
            pio_sm_put_blocking(pio0, 0, 0x00808080 << 8u);
        } else {
            // Contar cuántos samples están activos en este step
            uint32_t active_samples[NUM_SAMPLES];
            int num_active = 0;
            
            for(int j = 0; j < NUM_SAMPLES; ++j) {
                if(sequence_grid[j][i]) { 
                    active_samples[num_active] = j;
                    num_active++;
                }
            }
            
            if (num_active == 0) {
                // LED apagado con brillo mínimo
                pio_sm_put_blocking(pio0, 0, 0x00020202 << 8u);
            } else if (num_active == 1) {
                // Un solo sample - color fijo con brillo reducido
                uint32_t color = 0x00020202; // Default
                switch(active_samples[0]) {
                    case 0: color = 0x00200000; break; // Kick: Rojo
                    case 1: color = 0x00002000; break; // Snare: Verde
                    case 2: color = 0x00000020; break; // Hi-Hat: Azul
                    case 3: color = 0x00202000; break; // Clap: Amarillo
                }
                pio_sm_put_blocking(pio0, 0, color << 8u);
            } else {
                // Múltiples samples - parpadear entre colores con velocidad fija
                uint32_t blink_index = (local_counter / 30) % num_active;  // Velocidad fija más lenta
                uint32_t color = 0x00020202; // Default
                
                switch(active_samples[blink_index]) {
                    case 0: color = 0x00300000; break; // Kick: Rojo más brillante
                    case 1: color = 0x00003000; break; // Snare: Verde más brillante
                    case 2: color = 0x00000030; break; // Hi-Hat: Azul más brillante
                    case 3: color = 0x00303000; break; // Clap: Amarillo más brillante
                }
                pio_sm_put_blocking(pio0, 0, color << 8u);
            }
        }
    }
}

static void button_callback(uint gpio, uint32_t events) {
    absolute_time_t now = get_absolute_time();
    
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        if (gpio == BUTTON_PINS[i]) {
            bool current_state = !gpio_get(gpio);  // Invertir porque usamos pull-up
            
            // Solo procesar si ha cambiado el estado y ha pasado el tiempo de debounce
            if (current_state != button_states[i]) {
                uint64_t diff_us = absolute_time_diff_us(last_press_time[i], now);
                
                if (diff_us > DEBOUNCE_DELAY_US) {
                    button_states[i] = current_state;
                    last_press_time[i] = now;
                    
                    // Solo actuar en la transición de liberado a presionado
                    if (current_state) {
                        printf("Button %d pressed\n", i);
                        
                        // Reproducir sample inmediatamente
                        queue_sample(i);
                        
                        // Toggle del step en el secuenciador
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
        
        // Inicializar estado del botón
        button_states[i] = false;
        
        // Pequeña pausa para estabilizar
        sleep_ms(10);
        
        // Configurar interrupciones en ambos flancos para mejor detección
        gpio_set_irq_enabled_with_callback(BUTTON_PINS[i], GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &button_callback);
    }
    
    printf("Buttons initialized\n");
}

static void check_and_advance_sequencer(uint32_t* step_delay_us, absolute_time_t* next_step_time) {
    if (time_reached(*next_step_time)) {
        // Avanzar al siguiente step
        current_step = (current_step + 1) % NUM_STEPS;
        
        // Reproducir samples programados para este step
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
    
    // Inicializar LEDs WS2812
    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, LED_PIN, 800000, IS_RGBW);
    
    // Inicializar ADC para el potenciómetro
    adc_init();
    adc_gpio_init(POT_PIN);
    adc_select_input(0);
    
    // Inicializar sistemas
    init_audio();
    init_buttons();
    
    // Inicializar estados de botones
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        last_press_time[i] = nil_time;
        button_states[i] = false;
    }
    
    // Configurar timing inicial del secuenciador
    uint32_t step_delay_us = 60 * 1000 * 1000 / (bpm * 4);
    absolute_time_t next_step_time = delayed_by_us(get_absolute_time(), step_delay_us);
    
    // Actualizar LEDs inicial
    update_leds();
    
    // Loop principal
    while (true) {
        // Procesar cola de samples
        process_sample_queue();
        
        // Leer potenciómetro y actualizar BPM
        uint16_t adc_value = adc_read();
        uint new_bpm = 60 + (adc_value * 160 / 4095);
        if (new_bpm != bpm) {
            bpm = new_bpm;
            step_delay_us = 60 * 1000 * 1000 / (bpm * 4);
        }
        
        // Verificar y avanzar secuenciador
        check_and_advance_sequencer(&step_delay_us, &next_step_time);
        
        // Actualizar LEDs si es necesario
        if (step_changed) {
            update_leds();
            step_changed = false;
        }
        
        // Actualizar LEDs para el parpadeo (velocidad fija independiente del BPM)
        static uint32_t led_update_counter = 0;
        led_update_counter++;
        if (led_update_counter % 1000 == 0) {  // Actualizar cada 1000 ciclos
            update_leds();
        }
        
        // Pequeña pausa para no saturar el CPU
        sleep_us(100);
    }
    
    return 0;
}