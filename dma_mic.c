#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"

#define MIC_PIN 26 // GPIO pin for the microphone input
#define PWM_PIN 15 // GPIO pin for the PWM output
#define BUTTON_PIN 0 // GPIO pin for the button input

#define SAMPLES 120000 // Number of samples to read from the microphone
#define SAMPLE_RATE 8000 // Sample rate in Hz

void play_samples_pwm_dma(volatile uint16_t *samples, uint32_t num_samples, uint32_t sample_rate);
volatile uint16_t buffer[SAMPLES];
volatile bool dma = false;

void dma_handler() {
    // Clear the DMA interrupt
    dma_hw->ints0 = 1u << 0; // Clear the interrupt for channel 0

   dma = true; // Set the flag to indicate DMA transfer is complete
   
}


void dma_trigger(uint freq){
    const uint32_t start_adc_cmd = (adc_hw->cs|ADC_CS_START_ONCE_BITS); // ADC base address
    int dma_channel = dma_claim_unused_channel(true);
  //  int timer = dma_claim_unused_timer(true);
    dma_channel_config config = dma_channel_get_default_config(dma_channel);
    
  //  printf("DMA timer %d claimed successfully.\n",timer);
    
   // dma_timer_set_fraction(timer, 1, SYS_CLK_HZ/freq); 
   
    // Configure the DMA channel
    channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, DREQ_ADC);
    dma_channel_set_irq0_enabled(dma_channel, true); // Enable DMA channel IRQ
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);


    dma_channel_configure(dma_channel, &config,
                          buffer, // Destination buffer
                          &adc_hw->fifo, // Source address
                          SAMPLES, // Number of transfers
                          true); 

}

int64_t my_alarm_callback(alarm_id_t id, void *user_data) {
    adc_hw->cs |= ADC_CS_START_ONCE_BITS;
    return 1000000 / SAMPLE_RATE;
}

void button_handler(uint gpio, uint32_t events) {
    if (gpio == BUTTON_PIN && (events & GPIO_IRQ_EDGE_FALL)) {
        // Button pressed, trigger DMA transfer
        printf("Button pressed! Triggering DMA transfer...\n");
        play_samples_pwm_dma(buffer, SAMPLES, SAMPLE_RATE);
    }
}

int main()
{
    stdio_init_all();
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN); // Enable pull-up resistor
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &button_handler);
    adc_init();
    adc_gpio_init(MIC_PIN); // Initialize the GPIO pin for the microphone
    adc_select_input(0); // Select ADC input 0 (GPIO 26)
    adc_set_clkdiv(1.0f); // Set ADC clock divider to 1.0 (default)
    adc_fifo_setup(true, // Enable the FIFO
                    1, // Don't enable the DMA
                    1, 
                    false, 
                    false); // Right shift the result to 8 bits  
    sleep_ms(4000); 
    add_alarm_in_ms(1000000 / SAMPLE_RATE, my_alarm_callback, NULL, false);
    dma_trigger(SAMPLE_RATE);

    while (true) {

        if(dma) {
            dma = false; // Reset the DMA flag
            printf("DMA transfer complete. Samples:\n");
            play_samples_pwm_dma(buffer, SAMPLES, SAMPLE_RATE);
        }
        tight_loop_contents(); // Keep the CPU busy
    }
}

void play_samples_pwm_dma(volatile uint16_t *samples, uint32_t num_samples, uint32_t sample_rate) {
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);

    // Calcula el clkdiv para lograr el sample_rate deseado
    // PWM_freq = sys_clk / (clkdiv * (wrap + 1))
    // clkdiv = sys_clk / (sample_rate * (wrap + 1))
    uint32_t sys_clk = clock_get_hz(clk_sys); // Normalmente 125 MHz
    uint16_t wrap = 4096; // 12 bits de resolución
    float clkdiv = (float)sys_clk / (sample_rate * (wrap + 1));

    pwm_set_wrap(slice_num, wrap);
    pwm_set_clkdiv(slice_num, clkdiv);
    pwm_set_enabled(slice_num, true);

    // Configura DMA para transferir samples al registro de nivel del PWM
    int dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, DREQ_PWM_WRAP0 + slice_num);

    dma_channel_configure(
        dma_chan,
        &c,
        &pwm_hw->slice[slice_num].cc, // destino: registro CC del PWM
        samples,                      // fuente: buffer de samples
        num_samples,                  // número de transferencias
        true                          // inicia inmediatamente
    );

    // Espera a que termine la reproducción
    while (dma_channel_is_busy(dma_chan)) {
        tight_loop_contents();
    }
   
    pwm_set_enabled(slice_num, false);
    dma_channel_unclaim(dma_chan);
}