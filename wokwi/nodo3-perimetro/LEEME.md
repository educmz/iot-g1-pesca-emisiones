# Nodo 3 · Perímetro (G1 Pesca) · Gonzalo Aliaga Urbina

**Parte del problema que resuelve:** verifica si la emisión de la planta llega a la zona residencial según la dirección del viento.

## Archivos

| Archivo | Para qué | ¿Va a GitHub? |
|---|---|---|
| `sketch.ino` | Firmware del ESP32 | Sí |
| `diagram.json` | Circuito de Wokwi | Sí |
| `libraries.txt` | Librerías de Wokwi (PubSubClient, ArduinoJson) | Sí |
| `secrets.example.h` | Plantilla de claves, sin la clave real | Sí |
| `panel-nodo3.html` | Panel del nodo con botones de ataque | Sí, si Eduardo lo acepta |
| `secrets.h` | Wi-Fi, broker y clave HMAC | **No**, solo en Wokwi |
| `LEEME.md` | Esta guía | Opcional |

## 1. Montarlo en Wokwi

1. Abre tu proyecto ESP32 en wokwi.com.
2. Reemplaza `sketch.ino`, `diagram.json` y `libraries.txt` por los nuevos.
3. Si existe la pestaña `config.h` de la versión anterior, bórrala. Crea `secrets.h` con el botón "+" y pega su contenido.
4. Dale Play. Cada 2 s verás una línea así: `gas=400 ppm [ADC 3628] (ok) viento=… riesgo=…`
5. Prueba en este orden:
   - Deslizador del MQ-2 en 400 ppm → riesgo **BAJO**, LED apagado.
   - Sube a unos 2 000 ppm → riesgo **MEDIO**, LED parpadea.
   - Sube a 10 000 ppm o más y gira la perilla hasta `viento≈90° HACIA comunidad` → riesgo **ALTO**, LED fijo.
   - Con 10 000 ppm, gira el viento a otro lado → vuelve a **MEDIO**.

Al inicio verás `[modo seguro]` porque todavía no existe el gateway. Es lo correcto: sin gateway, el nodo es más estricto y el ALTO se activa ya con 1 000 ppm.

**Calibración:** el MQ-2 de Wokwi entrega una señal logarítmica (400 ppm da un ADC de unos 3628, y 100 000 ppm, unos 4042). Si el valor del monitor no coincide con el deslizador, ajusta `ADC_EN_400PPM` y `ADC_POR_DECADA` al inicio del sketch.

## 2. Circuito (igual en Cirkit y en Wokwi)

| Componente | Pin del componente | Pin del ESP32 | Tipo de señal |
|---|---|---|---|
| MQ-2 | VCC / GND | 3V3 / GND | alimentación |
| MQ-2 | AOUT | **GPIO34** | analógica (entrada) |
| Potenciómetro (viento) | extremos | 3V3 / GND | alimentación |
| Potenciómetro (viento) | cursor (SIG) | **GPIO35** | analógica (entrada) |
| LED rojo de riesgo | ánodo, con resistencia de 220 Ω | **GPIO26** | digital (salida) |
| LED rojo de riesgo | cátodo | GND | — |

## 3. Cómo funciona

**Detecta (cada 0,5 s):** gas en ppm simulados del MQ-2 y viento de 0 a 359°. La comunidad está al **este (90°)**: el viento va hacia ella si está entre 45° y 135°.

**Decide en el propio nodo (edge):**

| Situación | Riesgo | LED |
|---|---|---|
| Gas < 1 000 ppm | BAJO | apagado |
| Gas ≥ 1 000 ppm, pero el viento va a otro lado o no llega a 5 000 | MEDIO | parpadea |
| Gas ≥ 5 000 ppm **y** viento hacia la comunidad | ALTO | fijo |

- **Histéresis del 10 %:** para bajar de nivel el gas debe caer un poco más, así el LED no titila en el límite.
- **Modo seguro:** sin latido **firmado** de D5 recibido por upc/g1/nodo3/cmd durante 60 s, el ALTO se activa con 1 000 ppm. El estado no firmado de nodo5/estado no desactiva esta protección.

**Comunica, según el contrato del repo (`docs/contrato-mqtt`):**

| Tópico | Qué publica |
|---|---|
| `upc/g1/nodo3/datos` | Cada 2 s, tres mensajes: `sensor` = `gas` (ppm), `viento` (grados) y `riesgo` (0, 1 o 2). Campos: `id, ts (ms), seq, sensor, valor, unidad, calidad, firma` |
| `upc/g1/nodo3/alerta` | Cambios de riesgo y de modo seguro |
| `upc/g1/nodo3/estado` | `online` retenido cada 30 s. LWT `{"id":"nodo3","estado":"offline"}` |
| `upc/g1/seguridad/eventos` | Ataques rechazados: `hora, nodo, ataque, accion, motivo` |
| Escucha `upc/g1/sim/ataque` | Botones del dashboard (`{"id":"nodo3","ataque":"aire_limpio_falso"}`) |
| Escucha `upc/g1/nodo1/datos` y `upc/g1/nodo2/datos` | Gas de pozas y secadores, para explicar la incoherencia |

**Firma (propuesta para el contrato):** HMAC-SHA256 en hex del texto `id|ts|seq|sensor|valor(2 decimales)|unidad|calidad`.

## 4. Ataque y defensa (ataque 3 del laboratorio)

**Ataque:** alguien publica en `upc/g1/nodo3/datos` que el gas es 0 ppm ("aire limpio") para ocultar una emisión. Se dispara de dos formas:
- El botón del dashboard envía la solicitud a `upc/g1/sim/ataque` y el nodo inyecta un mensaje falso, sin firma válida.
- El botón "ataque externo" del panel publica el mensaje falso directo desde el navegador.

**Defensa del nodo:** escucha su propio tópico y revisa cada mensaje.
1. **Firma:** si el HMAC no coincide, es una suplantación.
2. **Coherencia:** si el mensaje dice menos de 1 000 ppm pero el MQ-2 real mide más, es un `aire_limpio_falso`. El motivo incluye el viento y lo que reportan pozas y secadores.
3. **Repetición:** el eco de una publicación propia solo se consume una vez; un segundo envío idéntico y firmado, aunque sea reciente, es un replay.
4. **Respuesta:** publica el evento en `upc/g1/seguridad/eventos` y republica en seguida las lecturas reales firmadas. El panel descarta el valor falso.

**Para el gateway (Eduardo):** verificar la misma firma con la clave del nodo 3. Si el perímetro dice "limpio" pero pozas o secadores están altos y el viento va hacia la comunidad, no bajar el riesgo.

## 5. Guion de 25 s

- **0–8 s:** "Mi nodo es el perímetro: responde si el olor está llegando a los vecinos, combinando el gas del borde con el viento."
- **8–18 s:** subes el MQ-2 y giras el viento al este. "Con gas alto y viento hacia la comunidad, el LED queda fijo y el gateway recibe el riesgo ALTO para el lavador."
- **18–25 s:** botón de ataque. "Llega un 'aire limpio' falso. Mi nodo ve que la firma no es válida y que contradice al sensor, lo rechaza y aquí está el evento."

## 6. Preguntas probables en la visita

- **¿Por qué GPIO34 y 35?** Son del ADC1. El ADC2 no funciona con el Wi-Fi encendido.
- **¿Por qué la resistencia de 220 Ω?** Limita la corriente del LED a unos 6 mA para no quemarlo ni dañar el pin.
- **¿El MQ-2 mide H2S?** No específicamente: detecta humo y gases combustibles. En una planta real irían un MQ-136 (H2S) y un MQ-137 (amoníaco), a 5 V y con divisor de tensión, porque el ADC del ESP32 solo acepta hasta 3,3 V. Por eso hablamos de "ppm simulados MQ-2".
- **¿Por qué la escala es logarítmica?** El sensor responde así: su resistencia cambia según el logaritmo de la concentración.
- **¿Por qué el LED se decide en el nodo?** El requerimiento pide funcionar sin internet; el nodo actúa en menos de 1 s aunque se caiga la red.
- **¿Corrosión salina?** En una instalación real: caja IP65, conectores sellados y placa con barniz protector.
- **Cambiar un parámetro:** todos están al inicio de `sketch.ino` (`UMBRAL_ALERTA`, `UMBRAL_CRITICO`, `DIR_COMUNIDAD`, `TOLERANCIA_DIR`, `PERIODO_PUBLICAR`).


## Nota de revisión 2026

El modo normal requiere heartbeat de D5 firmado HMAC sobre D5|ts|seq|HB|1, recibido en upc/g1/nodo3/cmd. Hasta que Eduardo integre ese heartbeat, el Nodo 3 estará siempre en modo seguro y no debe interpretarse como fallo de la conexión MQTT. El LED entra en riesgo ALTO preventivo si el gas tiene calidad inválida. Las evidencias antiguas no prueban estas correcciones. Repetir Wokwi y renovar capturas. Para MQ-2 físico, adaptar la señal de salida del módulo de 5 V al ADC de 3,3 V del ESP32 usando un divisor (10 kΩ / 15 kΩ) y verificarlo en Cirkit.
