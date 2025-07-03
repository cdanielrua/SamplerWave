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

// Array con todos los samples para fácil acceso
static const sample_t samples[] = {
    { kick_sample_data, kick_sample_len },
    { snare_sample_data, snare_sample_len },
    { hat_sample_data, hat_sample_len },
    { clap_sample_data, clap_sample_len }
};