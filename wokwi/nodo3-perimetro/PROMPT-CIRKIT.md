# Prompt para la IA de Cirkit Designer · Nodo 3 Perímetro

Copia todo el bloque de abajo y pégalo en el chat de IA de Cirkit Designer.

```
Diseña el circuito del "Nodo 3 – Perímetro" de un sistema IoT de monitoreo de emisiones (planta de harina de pescado, grupo G1 Pesca). Sigue EXACTAMENTE estas especificaciones, sin agregar ni quitar componentes.

1) COMPONENTES (solo estos 5):
- 1 placa ESP32 DevKit (ESP32-WROOM-32, 30 o 38 pines). Nombre: "ESP32 Nodo 3".
- 1 módulo sensor de gas MQ-2 de 4 pines (VCC, GND, DO, AO). Nombre: "MQ-2 gas perímetro".
- 1 potenciómetro rotativo de 10 kΩ (3 pines). Nombre: "Potenciómetro viento".
- 1 LED rojo de 5 mm. Nombre: "LED riesgo comunidad".
- 1 resistencia de 220 Ω. Nombre: "R 220Ω LED".

2) CONEXIONES (cablea exactamente así):
- MQ-2 VCC -> ESP32 3V3 (cable rojo)
- MQ-2 GND -> ESP32 GND (cable negro)
- MQ-2 AO  -> ESP32 GPIO34 (cable verde)
- MQ-2 DO  -> SIN CONECTAR (no se usa)
- Potenciómetro pin extremo 1 -> ESP32 3V3 (cable rojo)
- Potenciómetro pin extremo 2 -> ESP32 GND (cable negro)
- Potenciómetro pin central (cursor/wiper) -> ESP32 GPIO35 (cable azul)
- ESP32 GPIO26 -> resistencia 220 Ω, terminal 1 (cable naranja)
- Resistencia 220 Ω, terminal 2 -> ánodo del LED (pata larga, +) (cable naranja)
- Cátodo del LED (pata corta, −) -> ESP32 GND (cable negro)
Todas las tierras (GND) deben quedar unidas: tierra común.

3) ETIQUETAS DE TEXTO EN EL LIENZO (agrega cuadros de texto visibles):
- Título arriba: "Nodo 3 – Perímetro (Gonzalo) · Verifica si la emisión llega a la comunidad según el viento"
- Junto al cable de GPIO34: "Entrada ANALÓGICA – gas MQ-2 (ADC1)"
- Junto al cable de GPIO35: "Entrada ANALÓGICA – dirección del viento 0–359° (ADC1)"
- Junto al cable de GPIO26: "Salida DIGITAL – LED de riesgo (apagado = BAJO, parpadeo = MEDIO, fijo = ALTO)"
- Junto a los cables de VCC/3V3 y GND: "Alimentación 3,3 V / tierra común"
- Un cuadro aparte: "Comunicación: Wi-Fi + MQTT (broker HiveMQ) hacia el gateway Nodo 5. Sin LoRa."

4) REGLAS:
- Las entradas analógicas van SOLO en GPIO34 y GPIO35 (ADC1), porque el ADC2 falla con el Wi-Fi activo. GPIO26 se usa como salida DIGITAL, y eso sí es correcto.
- No agregues protoboard, fuente externa, pantalla, buzzer ni otros sensores.
- Ordena los componentes para que no se crucen cables: sensores a la izquierda del ESP32 y el LED con su resistencia a la derecha.

5) AL TERMINAR, muéstrame una tabla de verificación con columnas: Componente | Pin del componente | Pin del ESP32 | Tipo de señal | Color del cable, y confirma que no hay pines sin conectar salvo MQ-2 DO.
```

## Revisión rápida después (compárala con tu captura)

| Componente | Pin | ESP32 | Señal |
|---|---|---|---|
| MQ-2 | VCC / GND / AO | 3V3 / GND / **GPIO34** | analógica (entrada) |
| MQ-2 | DO | sin conectar | — |
| Potenciómetro | extremos / centro | 3V3, GND / **GPIO35** | analógica (entrada) |
| LED + 220 Ω | ánodo vía resistencia / cátodo | **GPIO26** / GND | digital (salida) |
