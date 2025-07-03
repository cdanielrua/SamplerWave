#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define AUDIO_PIN 0

int main() {
    // Configurar el pin de audio para la función PWM
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);

    // Encontrar el "slice" de PWM que corresponde a nuestro pin
    uint slice_num = pwm_gpio_to_slice_num(AUDIO_PIN);

    // Configurar el PWM para generar una onda cuadrada de ~440Hz
    // El reloj del sistema es 125MHz. Dividimos por 256 (wrap) y luego por 1100
    // para obtener una frecuencia de aproximadamente 440Hz.
    pwm_set_wrap(slice_num, 255);
    pwm_set_clkdiv(slice_num, 1100); 
    
    // Establecer el ciclo de trabajo al 50% para una onda cuadrada
    pwm_set_chan_level(slice_num, PWM_CHAN_A, 128);

    // ¡Activar el PWM!
    pwm_set_enabled(slice_num, true);

    // El programa se queda aquí para siempre, manteniendo el tono sonando
    while (true) {
        tight_loop_contents();
    }

    return 0;
}