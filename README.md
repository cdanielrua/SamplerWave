# Sampler Wave

## Descripción del Proyecto

Sampler Wave es un **sampler/secuenciador de audio** inspirado en las clásicas drum machines (como las de Roland) utilizadas por DJs y productores musicales.  
El dispositivo permite reproducir y combinar múltiples sonidos (samples) para generar secuencias rítmicas, y está diseñado para operar sobre un Raspberry Pi Pico (RP2040) utilizando el SDK en C. El enfoque es aprovechar al máximo las capacidades del microcontrolador con una arquitectura de hardware minimalista para mantener el coste y la complejidad bajos.

**Características principales:**

- **Reproducción de sonidos:**  
  Los samples se almacenan en la memoria flash interna del Pico o, en fases posteriores, a través de una interfaz con tarjeta microSD.

- **Disparo de sonidos:**  
  A través de botones físicos, el usuario podrá disparar samples en tiempo real o programarlos en la secuencia.

- **Secuenciación de audio:**  
  Los sonidos se organizan en una matriz (por ejemplo, 4 sonidos x 16 pasos) y se recorre la secuencia con un "playhead" sincronizado a un tempo configurable.
  
  > Ejemplo de matriz de secuenciación:
  >
  > |           | Paso 1 | Paso 2 | ... | Paso 16 |
  > |-----------|--------|--------|-----|---------|
  > | **sn1**   | X      | -      | ... | X       |
  > | **sn2**   | -      | X      | ... | -       |
  > | **sn3**   | X      | X      | ... | X       |
  > | **sn4**   | -      | -      | ... | X       |
  >
  > "X" indica activación del sonido y "-" inactividad.

- **Retroalimentación visual:**  
  Uso de LEDs o tiras de LED para indicar el paso actual, el estado de la secuencia y confirmar la activación de un sample.

- **Control de parámetros:**  
  Un potenciómetro u otro control analógico permite ajustar el tempo (BPM) y el volumen en tiempo real.

- **Referencia comercial:**  
  [Dispositivo comercial avanzado](https://www.youtube.com/watch?v=JdR55Te_rKg) basado en principios similares.

---

## Requisitos Funcionales (RF)

### Procesamiento de Audio y Datos

- **RF1:** El sistema debe leer y reproducir sonidos almacenados.
- **RF2:** La reproducción del audio se realizará mediante una salida de la Raspberry Pi Pico junto con implementaciones adicionales si es necesario para generar una señal analógica.
- **RF3:** El sistema debe gestionar la secuenciación de los sonidos de acuerdo a la temporización configurada por el usuario.

### Interacción y Control

- **RF4:** Los botones físicos deben asignarse a la activación de sonidos específicos y responder rapidamente.
- **RF5:** Un potenciómetro o perilla permitirá al usuario ajustar el tempo (BPM) de la secuencia en tiempo de ejecución.
- **RF6:** El sistema debe permitir tanto la ejecución en vivo de sonidos.

### Interfaz de Usuario

- **RF7:** Proveer feedback visual mediante LEDs o tira de LED, indicando el paso actual de la secuencia y la activación de un sonido.
- **RF8:** La interfaz debe ser intuitiva y de fácil interpretación para usuarios sin formación técnica avanzada.

### Gestión del Almacenamiento

- **RF9:** El sistema debe cargar los sonidos desde un archivo .h.
- **RF10:** Se debe utilizar un formato de array de elementos de 12 bits que representan las muestras del sonido.

---

## Requisitos No Funcionales (RNF)

- **RNF1:** El sistema deberá reproducir sonidos y responder a la activación de botones.
- **RNF2:** La secuenciación debe mantenerse estable y precisa con el tempo establecido.
- **RNF3:** El consumo total del sistema y sus periféricos no deberá superar la capacidad maxima sin usar baterias externas, solamente alimentado por USB.
- **RNF4:** La arquitectura del sistema debe ser modular y bien documentada para facilitar futuras ampliaciones o actualizaciones.
- **RNF5:** El diseño debe permitir la integración de nuevos módulos (botones, perillas, LEDs, pantallas, etc.) sin reestructuraciones mayores.
- **RNF6:** El sistema debe operar de forma continua y estable sin reinicios inesperados durante sesiones de uso o demostraciones.
- **RNF7:** Los métodos de comunicación como la salida de audio deben ser accesibles por dispositivos comerciales(por ejemplo, conexión de un parlante por la salida de un jack de audio).

---

## Configuración del Entorno de Pruebas

### Disposición Física

- Montaje de los componentes (Raspberry Pi Pico, botones, potenciómetro, LEDs/tira de LED y, opcionalmente, una tarjeta microSD) en una placa de pruebas o gabinete.

### Pruebas de Funcionalidad

1. **Carga y Reproducción de Samples:**  
   - *Procedimiento:* Pre-cargar varios sonidos en arreglos en archivos .h.  
   - *Criterio de aceptación:* Al presionar el botón asignado, el sample debe reproducirse rapidamente.

2. **Secuenciación de Audio:**  
   - *Procedimiento:* Configurar una secuencia (16 pasos x 4 canales) y verificar el avance mediante LEDs que marquen el paso actual.  
   - *Criterio de aceptación:* La secuencia debe mantenerse sincronizada con el tempo configurado.

3. **Control de Tempo:**  
   - *Procedimiento:* Ajustar el BPM usando el potenciómetro mientras la secuencia está en ejecución.  
   - *Criterio de aceptación:* El sistema debe ajustar el tempo en tiempo de ejecución sin que se interrumpa.

4. **Interfaz de Usuario:**  
   - *Procedimiento:* Verificar que los LEDs respondan de forma clara a la activación de sonidos y al avance de la secuencia.  
   - *Criterio de aceptación:* La interfaz visual debe ser comprensible por el usuario.

5. **Pruebas de Integración:**  
   - *Procedimiento:* Ejecutar la secuencia completa combinando la activación manual de botones, la lectura del potenciómetro y la reproducción de audio de forma concurrente.  
   - *Criterio de aceptación:* Todos los módulos deben funcionar integradamente sin latencia o fallos perceptibles.

6. **Pruebas de Fiabilidad y Consumo Energético:**  
   - *Procedimiento:* Someter el sistema a una prueba de funcionamiento continuo durante un periodo de tiempo considerable.  
   - *Criterio de aceptación:* El sistema se mantiene estable sin reinicios y sin generación de sobrecalentamiento por consumo excesivo de potencia.

---

## Hardware

- **Microcontrolador:** Raspberry Pi Pico (RP2040).
- **Placa:** PCB o protoboard.
- **Filtros electrónicos:** (Por ejemplo, filtros RC para suavizar la señal de salida PWM) o un DAC.
- **Periféricos:**
  - Botones o pads para la activación de samples.
  - Potenciómetro para el control del tempo.
  - LEDs o tiras de LED (como WS2812) para retroalimentación visual.
  - Opcional: Módulo para tarjeta microSD para ampliación de almacenamiento.
- **Componentes de Soporte:**  
  - Fuentes de alimentación o reguladores (por ejemplo, fuente USB o módulo de batería recargable).
  - Carcasa o estructura para montar el dispositivo.

---

## Software

- **SDK en C para el RP2040.**
- **Librerías de control de periféricos:** Para manejo de LEDs, comunicación SPI/I2C y otros componentes.

---

## Herramientas y Equipos

- **Equipos de prueba y medición:** Multímetro, osciloscopio, fuentes de alimentación.
- **Herramientas de diseño:** Software CAD o de diseño de PCB (versiones educativas sin costo).
- **Equipos de prototipado:** Impresora 3D para fabricar carcasas o soportes (disponibles en el laboratorio de la universidad).

---

## Costos de Diseño y Prototipado

- **Diseño de PCB (en caso de ser necesario):**  
  - Costos asociados al uso de herramientas de diseño y al envío de archivos a un fabricante.
  - Número de prototipos planificados.

- **Pruebas y Validación:**  
  - Gastos en componentes de reemplazo en caso de fallas.
  - Costos adicionales para ensayos y mejoras.

- **Carcasas o Montajes:**  
  - Considerar el coste de impresión 3D o mecanizado según el material a utilizar.

---


