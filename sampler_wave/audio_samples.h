#pragma once

#include "pico/stdlib.h"

// Incluye los headers generados por el script de Python
#include "kick_sample.h"
#include "snare_sample.h"
#include "hat_sample.h"
#include "clap_sample.h"

// Estructura para agrupar cada sample
typedef struct {
    const uint8_t* data;
    const uint32_t len;
} sample_t;

// No definir el array aquí para evitar conflictos
// El array se define en main.c