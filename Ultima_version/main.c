/**
 * @file main.c
 * @brief Archivo principal del secuenciador de batería para Raspberry Pi Pico.
 * @details Este programa implementa una caja de ritmos de 16 pasos con 3 instrumentos (kick, snare, hi-hat).
 * Utiliza PWM y DMA para la salida de audio, ADC para el control de tempo,
 * y PIO para la retroalimentación visual en una tira de LEDs WS2812.
 * @author Tu Nombre
 * @date 16 de Julio, 2025
 */

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
 
 // --- Definiciones de Hardware y Parámetros ---
 
 #define POT_PIN 27                  ///< Pin GPIO para la entrada del potenciómetro (ADC1).
 #define MIC_PIN 26                  ///< Pin GPIO para una entrada de micrófono (no usado actualmente).
 #define PWM_PIN 15                  ///< Pin GPIO para la salida de audio PWM.
 #define BUTTON_PIN 0                ///< Pin GPIO inicial para los 10 botones de entrada.
 #define PWM_WRAP_VALUE      4096    ///< Valor de envoltura del PWM (resolución).
 #define SAMPLES 120000              ///< Número de muestras a leer (usado en una función inactiva).
 #define SAMPLE_RATE 24000           ///< Frecuencia de muestreo del audio en Hz.
 #define PATTERN_STEPS_PER_BUFFER (DMA_HALF_BUFFER_SIZE / 4) ///< Pasos de patrón por búfer (sin uso activo).
 #define NUM_SOUNDS          3       ///< Número total de sonidos (kick, snare, hi-hat).
 
 // --- Prototipos de Funciones ---
 
 void pwm_sample_rate_init(uint32_t sample_rate);
 void play_samples_pwm_dma();
 void update_tempo(uint32_t new_bpm);
 void fill_and_mix_buffer(uint16_t *buffer_ptr, size_t num_samples_to_fill);
 
 // --- Variables Globales ---
 
 SamplePlayer players[3];              ///< Arreglo de reproductores de muestras para cada sonido.
 volatile bool adc_ready = false;      ///< Bandera que indica que una nueva lectura del ADC está lista.
 volatile bool dma = false;            ///< Bandera que indica que el DMA ha completado una transferencia.
 volatile int dma_chan = 0;            ///< Canal DMA utilizado para la reproducción de audio.
 volatile int trigger_channel = 0;     ///< Canal DMA para el disparo por ADC (no usado).
 volatile uint8_t button_num = 0;      ///< Almacena el número del último botón presionado (0-9).
 volatile uint8_t pattern_slice = 0;   ///< Desplazamiento del patrón (0 o 8) para edición con 8 botones.
 volatile uint8_t idx = 0;             ///< Índice del instrumento actualmente seleccionado para edición (0-2).
 
 
 /**
  * @brief Manejador de interrupción para el canal DMA de audio.
  * @details Se activa cuando el DMA termina de transferir una mitad del búfer de audio.
  * Limpia la bandera de interrupción y activa la bandera 'dma' para que el
  * bucle principal sepa que debe rellenar la mitad del búfer que ha quedado libre.
  */
 void dma_handler() {
     dma_hw->ints0 = 1u << dma_chan; // Limpia la interrupción para el canal específico.
     dma = true;                     // Activa la bandera para el bucle principal.
 }
 
 /**
  * @brief Configura un canal DMA para ser disparado por el ADC (función no utilizada actualmente).
  * @param freq Frecuencia de disparo.
  */
 void dma_trigger(uint freq){
     // ... Código para configurar una captura de ADC por DMA ...
 }
 
 /**
  * @brief Callback de alarma para actualizar el estado de los LEDs.
  * @details Esta función se llama periódicamente para refrescar la tira de LEDs,
  * mostrando el patrón actual y el pulso activo.
  * @param id ID de la alarma que se disparó.
  * @param user_data Puntero a datos de usuario (no utilizado).
  * @return int64_t Tiempo en microsegundos para la próxima llamada (10ms).
  */
 int64_t update(alarm_id_t id, void *user_data) {
     led_mix(idx, beat_index, patterns, pattern_slice);
     return 10000;
 }
 
 /**
  * @brief Callback de alarma para iniciar una conversión del ADC.
  * @details Esta función se llama periódicamente para iniciar una nueva lectura
  * del potenciómetro y activa la bandera 'adc_ready'.
  * @param id ID de la alarma.
  * @param user_data Puntero a datos de usuario (no utilizado).
  * @return int64_t Tiempo en microsegundos para la próxima llamada (250ms).
  */
 int64_t my_alarm_callback(alarm_id_t id, void *user_data) {
     adc_hw->cs |= ADC_CS_START_ONCE_BITS;
     adc_ready = true;
     return 250000;
 }
 
 // --- Lógica de Entrada de Usuario ---
 
 bool button_pressed = false;            ///< Bandera que indica si se ha presionado un botón.
 volatile uint32_t last_button_time = 0; ///< Marca de tiempo de la última pulsación para anti-rebote.
 const uint32_t debounce_ms = 300;       ///< Tiempo de anti-rebote (debounce) en milisegundos.
 
 /**
  * @brief Manejador de interrupción para los botones.
  * @details Se activa con un flanco de bajada en cualquier pin de botón.
  * Implementa una lógica anti-rebote simple basada en tiempo y, si la
  * pulsación es válida, activa la bandera 'button_pressed' e identifica qué botón fue.
  * @param gpio El pin GPIO que generó la interrupción.
  * @param events El tipo de evento de interrupción (ej. flanco de bajada).
  */
 void button_handler(uint gpio, uint32_t events) {
     uint32_t now = to_ms_since_boot(get_absolute_time());
     if ((events & GPIO_IRQ_EDGE_FALL)) {
         if (now - last_button_time > debounce_ms) {
             button_pressed = true;
             last_button_time = now;
             button_num = gpio - BUTTON_PIN;
         }
     }
 }
 
 /**
  * @brief Inicializa 10 pines de botones consecutivos con interrupciones.
  * @param pin_start Pin GPIO inicial desde donde se configuran los 10 botones.
  */
 void button_init(uint8_t pin_start){
     gpio_init_mask(0x3FF << pin_start);
     gpio_set_dir_in_masked(0x3FF << pin_start);
     
     for(uint8_t i = pin_start; i < pin_start + 10; i++) {
         gpio_pull_up(i);
         gpio_set_irq_enabled_with_callback(i, GPIO_IRQ_EDGE_FALL, true, &button_handler);
     }
 }
 
 /**
  * @brief Imprime el valor de un entero de 16 bits en formato binario por la consola.
  * @param value El valor de 16 bits a imprimir.
  */
 void print_bin(uint16_t value) {
     for (int i = 15; i >= 0; i--) {
         printf("%d", (value >> i) & 1);
     }
     printf("\n");
 }
 
 
 /**
  * @brief Punto de entrada principal del programa.
  * @details Realiza la inicialización de todos los periféricos y luego entra en un
  * bucle infinito que gestiona los eventos del sistema de forma reactiva
  * (pulsaciones de botón, finalización de DMA, lecturas de ADC).
  * @return int Código de salida del programa (teóricamente, nunca retorna).
  */
 int main()
 {
     // --- Fase de Inicialización ---
     stdio_init_all();
     ws2812_init(16);
     button_init(BUTTON_PIN);
     adc_init();
     adc_gpio_init(POT_PIN);
     adc_select_input(1); // ADC1 corresponde a GPIO27
     adc_set_clkdiv(80.0f);
     
     // Inicializa los reproductores de muestras
     players[0] = (SamplePlayer){.data = kick_data, .length = KICK_SIZE, .position = 0, .active = false};
     players[1] = (SamplePlayer){.data = snare_data, .length = SNARE_SIZE, .position = 0, .active = false};
     players[2] = (SamplePlayer){.data = hihat_data, .length = HIHAT_SIZE, .position = 0, .active = false};
     
     update_tempo(112);
     fill_and_mix_buffer(sampler_buffer, HALF_BUFFER_SIZE); // Pre-llena la mitad del búfer
 
     sleep_ms(2000); // Pausa inicial
 
     // Inicia tareas periódicas
     add_alarm_in_us(10000, update, NULL, true); // Alarma para LEDs
     add_alarm_in_us(250000, my_alarm_callback, NULL, true); // Alarma para ADC
     
     // Inicia el motor de audio
     pwm_sample_rate_init(SAMPLE_RATE);
     dma_chan = dma_claim_unused_channel(true);
     play_samples_pwm_dma();
     dma_channel_start(dma_chan);
 
     // --- Bucle Principal (Orientado a Eventos) ---
     while (true) {
         if (button_pressed) {
             button_pressed = false;
             printf("Button pressed on pin %d\n", button_num);
             if(button_num == 8){ // Botón para cambiar de instrumento
                 idx = (idx + 1) % 3;
                 printf("Pattern: %d\n", idx);
             }
             else if (button_num == 9){ // Botón para cambiar de slice de patrón
                 pattern_slice = (pattern_slice + 8) % 16;
                 printf("Pattern slice: %d\n", pattern_slice);
             }
             else{ // Botones 0-7 para editar el patrón
                 patterns[idx] ^= (1 << (button_num + pattern_slice));
             }
         }
 
         if(dma) { // Si una transferencia DMA ha terminado
             dma = false;
             // Lógica de doble búfer
             if(current_buffer_is_upper_half){
                 dma_channel_set_read_addr(dma_chan, sampler_buffer, false);
                 fill_and_mix_buffer(sampler_buffer + HALF_BUFFER_SIZE, HALF_BUFFER_SIZE);
             } else {
                 dma_channel_set_read_addr(dma_chan, sampler_buffer + HALF_BUFFER_SIZE, false);
                 fill_and_mix_buffer(sampler_buffer, HALF_BUFFER_SIZE);
             }
             current_buffer_is_upper_half = !current_buffer_is_upper_half;
             dma_channel_set_trans_count(dma_chan, HALF_BUFFER_SIZE, true); // Inicia la siguiente transferencia
         }
         
         if (adc_ready) { // Si hay una nueva lectura de ADC
             adc_ready = false;
             uint16_t adc_value = adc_read();
             uint new_bpm = 60 + (adc_value * 160 / 4095); // Mapea el valor a un rango de BPM
             if (abs((int)current_bpm - (int)new_bpm) > 2) { // Evita fluctuaciones pequeñas
                 update_tempo(new_bpm);
                 printf("BPM updated to: %d\n", new_bpm);
             }
         }
         
         tight_loop_contents(); // Mantiene la CPU en bajo consumo mientras espera interrupciones
     }
 }
 
 /**
  * @brief Configura el periférico PWM para operar a una frecuencia de muestreo específica.
  * @details Calcula el divisor de reloj necesario para que la frecuencia del ciclo
  * del PWM coincida con la frecuencia de muestreo del audio.
  * @param sample_rate La frecuencia de muestreo deseada en Hz.
  */
 void pwm_sample_rate_init(uint32_t sample_rate){
     gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
     uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
     uint32_t sys_clk = clock_get_hz(clk_sys);
     uint16_t wrap = 4000;
     float clkdiv = (float)sys_clk / (sample_rate * (wrap + 1));
     pwm_set_wrap(slice_num, wrap);
     pwm_set_clkdiv(slice_num, clkdiv);
     pwm_set_enabled(slice_num, true);
 }
 
 /**
  * @brief Configura e inicia una transferencia DMA desde el búfer de audio al PWM.
  * @details Establece un canal DMA para transferir datos de 16 bits desde el
  * 'sampler_buffer' al registro de ciclo de trabajo del PWM, sincronizado
  * por las solicitudes de datos (DREQ) del propio PWM.
  */
 void play_samples_pwm_dma() {
     if(dma_channel_is_busy(dma_chan)) {
         printf("DMA channel is busy, cannot play samples.\n");
         return;
     }
     uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
     dma_channel_cleanup(dma_chan);
     dma_channel_config c = dma_channel_get_default_config(dma_chan);
     channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
     channel_config_set_read_increment(&c, true);
     channel_config_set_write_increment(&c, false);
     channel_config_set_dreq(&c, pwm_get_dreq(slice_num));
 
     dma_channel_set_irq0_enabled(dma_chan, true);
     irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
     irq_set_enabled(DMA_IRQ_0, true);
 
     dma_channel_configure(
         dma_chan,
         &c,
         &pwm_hw->slice[slice_num].cc, // Destino: registro de nivel del PWM
         sampler_buffer,               // Fuente: búfer de audio
         HALF_BUFFER_SIZE,             // Número de transferencias
         false                         // No iniciar inmediatamente
     );
 }
 
 /**
  * @brief Rellena un búfer con muestras de audio mezcladas según el patrón actual.
  * @details Esta es la función principal del motor de audio. Avanza el secuenciador
  * basado en el tempo, dispara los sonidos correspondientes a cada paso
  * del patrón, mezcla las muestras de los sonidos activos y las escribe en el búfer.
  * @param buffer_ptr Puntero al búfer de audio que se va a rellenar.
  * @param num_samples_to_fill Número de muestras a generar.
  */
 void fill_and_mix_buffer(uint16_t *buffer_ptr, size_t num_samples_to_fill) {
     static size_t samples_since_last_pattern_step = 0;
 
     for (size_t i = 0; i < num_samples_to_fill; ++i) {
         int32_t mixed_sample_value = 0;
         int active_samples_count = 0;
 
         // --- Lógica del Secuenciador ---
         samples_since_last_pattern_step++;
         if (samples_since_last_pattern_step >= pattern_samples_per_step) {
             samples_since_last_pattern_step = 0;
             pattern_index = (pattern_index + 1) % 16; // Avanza y cicla el índice del patrón
 
             uint16_t current_step_bit_mask = (1u << (15 - pattern_index));
             beat_index = 15 - pattern_index;
 
             // Dispara los sonidos si el bit correspondiente está activo en el patrón
             if (patterns[0] & current_step_bit_mask) {
                 players[0].active = true;
                 players[0].position = 0;
             }
             if (patterns[1] & current_step_bit_mask) {
                 players[1].active = true;
                 players[1].position = 0;
             }
             if (patterns[2] & current_step_bit_mask) {
                 players[2].active = true;
                 players[2].position = 0;
             }
         }
         
         // --- Lógica de Mezcla de Audio ---
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
         
         // --- Normalización y Salida ---
         uint16_t final_output;
         if (active_samples_count > 0) {
             final_output = (uint16_t)(mixed_sample_value / active_samples_count);
             if (final_output > 4095) final_output = 4095; // Evita desbordamiento
         } else {
             final_output = 2048; // Silencio (valor medio)
         }
         buffer_ptr[i] = final_output;
     }
 }
 
 /**
  * @brief Actualiza el tempo (BPM) y recalcula la duración de los pasos en muestras.
  * @param new_bpm El nuevo valor de Beats Per Minute.
  */
 void update_tempo(uint32_t new_bpm) {
     if (new_bpm == 0) new_bpm = 1; // Evita división por cero
     current_bpm = new_bpm;
 
     // Calcula cuántas muestras de audio corresponden a cada paso (semicorchea) del patrón
     float beats_per_step = 1.0f / 4.0f; // 4 semicorcheas por pulso (beat)
     float samples_per_step = ((float)SAMPLE_RATE * 60.0f * beats_per_step) / (float)current_bpm;
 
     pattern_samples_per_step = (size_t)samples_per_step;
     if (pattern_samples_per_step == 0) pattern_samples_per_step = 1;
 }