#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "audio_table.h"
#include "sampler.h"
#include "ws2812.h"


#define MIC_PIN 26 // GPIO pin for the microphone input
#define PWM_PIN 15 // GPIO pin for the PWM output
#define BUTTON_PIN 0 // GPIO pin for the button input
#define PWM_WRAP_VALUE      4096 
#define SAMPLES 120000 // Number of samples to read from the microphone
#define SAMPLE_RATE 24000 // Sample rate in Hz
#define PATTERN_STEPS_PER_BUFFER (DMA_HALF_BUFFER_SIZE / 4)
void pwm_sample_rate_init(uint32_t sample_rate);
void play_samples_pwm_dma();
void update_tempo(uint32_t new_bpm);
//volatile uint16_t buffer[SAMPLES];
void fill_and_mix_buffer(uint16_t *buffer_ptr, size_t num_samples_to_fill);
SamplePlayer players[3];
#define NUM_SOUNDS         3 
volatile bool dma = false;
volatile int dma_chan = 0;
volatile int trigger_channel = 0;
volatile uint8_t button_num = 0;
void dma_handler() {
    // Clear the DMA interrupt
    dma_hw->ints0 = 1u << 0; // Clear the interrupt for channel 0
   // dma_channel_set_trans_count(dma_chan, 2400, false); // Reset the transfer count
  //  dma_channel_set_read_addr(dma_chan,audio_data , true);
   dma = true; // Set the flag to indicate DMA transfer is complete
   if(current_buffer_is_upper_half){
        dma_channel_set_read_addr(dma_chan,sampler_buffer,false);
        fill_and_mix_buffer(sampler_buffer + HALF_BUFFER_SIZE,HALF_BUFFER_SIZE);
   }else{
        dma_channel_set_read_addr(dma_chan,sampler_buffer + HALF_BUFFER_SIZE,false);
        fill_and_mix_buffer(sampler_buffer,HALF_BUFFER_SIZE);
   }
   current_buffer_is_upper_half = !current_buffer_is_upper_half;
   dma_channel_set_trans_count(dma_chan, HALF_BUFFER_SIZE, true);

}


void dma_trigger(uint freq){
    const uint32_t start_adc_cmd = (adc_hw->cs|ADC_CS_START_ONCE_BITS); // ADC base address
    trigger_channel = dma_claim_unused_channel(true);
 
  //  int timer = dma_claim_unused_timer(true);
    dma_channel_config config = dma_channel_get_default_config(trigger_channel);
    
  //  printf("DMA timer %d claimed successfully.\n",timer);
    
   // dma_timer_set_fraction(timer, 1, SYS_CLK_HZ/freq); 
   
    // Configure the DMA channel
    channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, DREQ_ADC);
    dma_channel_set_irq0_enabled(trigger_channel, true); // Enable DMA channel IRQ
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);


    dma_channel_configure(trigger_channel, &config,
                          0, // Destination buffer
                          &adc_hw->fifo, // Source address
                          SAMPLES, // Number of transfers
                          false); 

}
volatile uint8_t idx = 0;
int64_t update(alarm_id_t id, void *user_data) {
    led_mix(idx, beat_index, patterns);
    return 20000; // Retornar el tiempo para la próxima llamada
}

int64_t my_alarm_callback(alarm_id_t id, void *user_data) {
    adc_hw->cs |= ADC_CS_START_ONCE_BITS;
    return 1000000 / SAMPLE_RATE;
}

bool button_pressed = false;
volatile uint32_t last_button_time = 0;
const uint32_t debounce_ms = 300;

void button_handler(uint gpio, uint32_t events) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if ((events & GPIO_IRQ_EDGE_FALL)) {
        if (now - last_button_time > debounce_ms) {
            button_pressed = true; // Set the flag to indicate button press
            last_button_time = now;
            button_num = gpio - BUTTON_PIN; // Calculate the button number based on the GPIO pin
        }
    }
}

/// @brief Inicializa 8 botones en los pines consecutivos a partir de pin_start.
/// @param pin_start Pin de inicio para los botones (0-7).
void button_init(uint8_t pin_start){

    gpio_init_mask(0x1FF << pin_start);
    gpio_set_dir_in_masked(0x1FF << pin_start); // Set the button pin as input
   
    
    for(uint8_t i = pin_start; i < pin_start + 9; i++) {
        gpio_pull_up(i); // Enable pull-up resistor on each button pin
       gpio_set_irq_enabled_with_callback(i, GPIO_IRQ_EDGE_FALL, true, &button_handler);
    }
    
}

void print_bin(uint16_t value) {
    for (int i = 15; i >= 0; i--) {
        printf("%d", (value >> i) & 1);
    }
    printf("\n");
}

int main()
{
    stdio_init_all();
    ws2812_init(16);
    button_init(BUTTON_PIN);
    adc_init();
    adc_gpio_init(MIC_PIN); // Initialize the GPIO pin for the microphone
    adc_select_input(0); // Select ADC input 0 (GPIO 26)
    adc_set_clkdiv(1.0f); // Set ADC clock divider to 1.0 (default)
    adc_fifo_setup(true, // Enable the FIFO
                    1, // Don't enable the DMA
                    1, 
                    false, 
                    false); // Right shift the result to 8 bits  
   // Initialize PWM with the desired sample rate   
   
   
   players[0] = (SamplePlayer){.data = kick_data, .length = KICK_SIZE, .position = 0, .active = false};
   players[1] = (SamplePlayer){.data = snare_data, .length = SNARE_SIZE, .position = 0, .active = false};
   players[2] = (SamplePlayer){.data = hihat_data, .length = HIHAT_SIZE, .position = 0, .active = false};
   update_tempo(112);   
   fill_and_mix_buffer(sampler_buffer, HALF_BUFFER_SIZE);


    sleep_ms(2000); 
    add_alarm_in_us(250000, update, NULL, true);
    pwm_sample_rate_init(SAMPLE_RATE);
    dma_chan = dma_claim_unused_channel(true); 
    play_samples_pwm_dma(); // Initialize the DMA transfer for PWM output
   
    dma_channel_start(dma_chan); // Start the DMA channel
    
  

    while (true) {


        if (button_pressed) {
            button_pressed = false; // Reset the button press flag
            printf("Button pressed on pin %d\n", button_num);
           if(button_num == 8){
            idx = (idx + 1) % 3;
            printf("Pattern: %d\n", idx);
           }
           else{
             patterns[idx] ^= (1 << button_num); 
           }
           
           print_bin(patterns[idx]);
            
        }
        if(dma) {
            dma = false; // Reset the DMA flag
            
            
           // play_samples_pwm_dma(buffer, SAMPLES, SAMPLE_RATE);
        }
        tight_loop_contents(); // Keep the CPU busy
    }
}






void pwm_sample_rate_init(uint32_t sample_rate){
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
    uint32_t sys_clk = clock_get_hz(clk_sys); 
    uint16_t wrap = 4000; 
    float clkdiv = (float)sys_clk / (sample_rate * (wrap + 1));
    pwm_set_wrap(slice_num, wrap);
    pwm_set_clkdiv(slice_num, clkdiv);
    pwm_set_enabled(slice_num, true);
    //pwm_set_chan_level(slice_num,PWM_CHAN_B,10); // Set initial PWM level to 0
}


void play_samples_pwm_dma() {
    
    if(dma_channel_is_busy(dma_chan)) {
        printf("DMA channel is busy, cannot play samples.\n");
        return;
    }
    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
    dma_channel_cleanup(dma_chan); // Limpia el canal DMA antes de configurarlo
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pwm_get_dreq(slice_num)); // Configura DREQ para el PWM
    // Habilita la interrupción del canal DMA
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    dma_channel_configure(
        dma_chan,
        &c,
        &pwm_hw->slice[slice_num].cc, // destino: registro CC del PWM
        sampler_buffer,                      // fuente: buffer de samples
        HALF_BUFFER_SIZE,                  // número de transferencias
        false                          // inicia inmediatamente
    );
    // Espera a que termine la reproducción
    
   // dma_channel_wait_for_finish_blocking(dma_chan);
   // pwm_set_enabled(slice_num, false);
    
}

void fill_and_mix_buffer(uint16_t *buffer_ptr, size_t num_samples_to_fill) {
    static size_t samples_since_last_pattern_step = 0;

    for (size_t i = 0; i < num_samples_to_fill; ++i) {
        int32_t mixed_sample_value = 0;
        int active_samples_count = 0;

        // --- Lógica de Patrón y Tempo ---
        samples_since_last_pattern_step++;

        if (samples_since_last_pattern_step >= pattern_samples_per_step) {
            samples_since_last_pattern_step = 0; // Reiniciar el contador para el nuevo paso

            pattern_index++; // Avanzamos al siguiente paso del patrón global

            if (pattern_index >= 16) { // Si hemos completado un ciclo de 16 pasos
                pattern_index = 0; // Reiniciar el índice para repetir el patrón
            }

            
            uint16_t current_step_bit_mask = (1u <<(15 - pattern_index) );
            beat_index = 15 - pattern_index ;
            printf("beat_index %d\n",beat_index); // Actualizar el índice del beat actual
            if (patterns[0] & current_step_bit_mask) { // Si el bit correspondiente en pattern_kick es '1'
                players[0].active = true;
                players[0].position = 0;
            }

            if (patterns[1] & current_step_bit_mask) { // Si el bit correspondiente en pattern_snare es '1'
                players[1].active = true;
                players[1].position = 0;
            }

            if (patterns[2] & current_step_bit_mask) { // Si el bit correspondiente en pattern_hihat es '1'
                players[2].active = true;
                players[2].position = 0;
            }

        }
     
        for (uint8_t s = 0; s < NUM_SOUNDS; ++s) {
            if (players[s].active) {
                if (players[s].position >= players[s].length) {
                    players[s].active = false;
                } else {
                    mixed_sample_value += players[s].data[players[s].position];
                    players[s].position++;
                    active_samples_count++;
                }
            }
        }
        // ... (normalización y escritura en buffer_ptr[i])
        uint16_t final_output;
        if (active_samples_count > 0) {
            final_output = (uint16_t)(mixed_sample_value / active_samples_count);
            if (final_output > 4095) final_output = 4095;
        } else {
            final_output = 4095 / 2;
        }
        buffer_ptr[i] = final_output;
    }
}

void update_tempo(uint32_t new_bpm) {
    if (new_bpm == 0) new_bpm = 1; // Evitar división por cero

    current_bpm = new_bpm;

    float samples_per_beat = ((float)SAMPLE_RATE * 60.0f) / (float)current_bpm;
  
    pattern_samples_per_step = (size_t)(samples_per_beat / (float)pattern_samples_per_step);

    if (pattern_samples_per_step == 0) {
        pattern_samples_per_step = 1;
    }


    printf("New BPM: %lu, Samples per Step: %zu\n", current_bpm, pattern_samples_per_step);
}