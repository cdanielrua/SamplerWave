#include "pico/stdlib.h"

#define TEST_PIN 0 // Estamos probando el pin GPIO0

int main() {
    // No necesitamos stdio_init_all() para esta prueba
    
    gpio_init(TEST_PIN);
    gpio_set_dir(TEST_PIN, GPIO_OUT);

    while (true) {
        gpio_put(TEST_PIN, 1); // Encender el LED
        sleep_ms(500);
        gpio_put(TEST_PIN, 0); // Apagar el LED
        sleep_ms(500);
    }
    return 0;
}