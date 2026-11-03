# Arquitectura y partes del problema

| Capa | Implementación | Responsable |
|---|---|---|
| Device | MQ-2, DS18B20, DHT22, potenciómetro, interruptor, relés, motor, LEDs, NeoPixel, TFT | Cada dueño del nodo |
| Embedded | Firmware ESP32: leer, validar, temporizar, publicar | Cada dueño |
| Edge | Alarmas y actuaciones locales; modo seguro | Cada dueño |
| Fog | Gateway con regla de 2 sensores + viento, comandos, confirmación y registro offline | Eduardo |
| Cloud | ThingSpeak e historial; HTML local para presentación | Eduardo y equipo |

## Responsabilidades individuales

| Nodo | Entradas y señal | Salidas |
|---|---|---|
| 1 — Pozas (Fernando) | MQ-2 analógico, DS18B20 1-Wire | buzzer, relé de ventilación |
| 2 — Secadores (Fabricio) | MQ-2 analógico, DHT22 digital | LED RGB |
| 3 — Perímetro (Gonzalo) | MQ-2 analógico, potenciómetro analógico para viento | LED indicador |
| 4 — Lavador (Pedro) | interruptor digital de confirmación | relé de lavador, paso a paso + A4988 |
| 5 — Gateway (Eduardo) | pulsador digital, RTC DS1307 I2C | TFT ILI9341 SPI, NeoPixel |

Diagrama obligatorio: **un Cirkit general con los cinco nodos**, mostrando entradas, salidas y tipo de señal. Cada integrante aporta su sección; Fernando consolida el conjunto con la revisión de Eduardo.

La autonomía crítica local **no equivale** a que el broker público o la coordinación remota funcionen cuando cae internet. Documentar el límite y demostrar mecanismos Edge y modo seguro.
