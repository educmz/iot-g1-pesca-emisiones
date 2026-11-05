# G1 Pesca — monitoreo y control de emisiones (IoT)

Laboratorio universitario para **Pesquera Costa Norte S.A.C. (caso ficticio)**: cinco nodos ESP32 simulados, MQTT, decisiones Edge/Fog, actuación y confirmación, ThingSpeak y dashboard HTML. La alternativa C de IA es **opcional**.

> **Estado al 05/11/2026:** firmwares de los cinco nodos y dashboard GitHub Pages implementados; envío HTTPS del Nodo 5 a ThingSpeak verificado (entradas guardadas). **Incidencia abierta:** desconexiones MQTT `state=-3` con WiFi conectado, y telemetría N2/N3/N4 no confirmada en la prueba distribuida. Las pruebas end-to-end y A/B siguen pendientes. Ver [diagnóstico](docs/diagnostico-mqtt-2026-11-05.md).

## Integrantes y responsabilidades

| Integrante | Nodo | Tarea | Carpeta |
|---|---|---|---|
| Fernando | 1 | Pozas: MQ-2, DS18B20, buzzer y relé; lectura falsificada | `wokwi/nodo1-pozas/` |
| Fabricio | 2 | Secadores: MQ-2, DHT22, LED RGB; suplantación HMAC; IA C | `wokwi/nodo2-secadores/` |
| Gonzalo | 3 | Perímetro: MQ-2, viento simulado, LED; aire limpio falso; resultados | `wokwi/nodo3-perimetro/` |
| Pedro | 4 | Lavador: interruptor, relé y A4988; orden falsa/replay | `wokwi/nodo4-lavador/` |
| Eduardo | 5 | Gateway: RTC, pulsador, TFT y NeoPixel; integración, MQTT, dashboard | `wokwi/nodo5-gateway/` |

## Estructura

- [Requerimiento y diagnóstico](docs/requerimientos/)
- [Arquitectura y asignación](docs/arquitectura/README.md)
- [Contrato MQTT](docs/contrato-mqtt/README.md) — **leer antes de programar**
- [Reglas de detección y actuación](docs/reglas/README.md)
- [Diseño en Cirkit](cirkit/README.md)
- [Nodos Wokwi](wokwi/)
- [Dashboard HTML](dashboard/README.md)
- [ThingSpeak](thingspeak/README.md)
- [Pruebas y comparación A/B/C](pruebas/README.md)
- [IA (bonificación)](inteligencia-artificial/README.md)
- [Entregables y evidencias](docs/entregables.md)
- [Guía de integración](docs/integracion.md)
- [Colaboración y Pull Requests](CONTRIBUTING.md)

## Enlaces activos de simulación y nube

- N1 Fernando: https://wokwi.com/projects/479819491217347585
- N2 Fabricio: https://wokwi.com/projects/479809534640024577
- N3 Gonzalo: https://wokwi.com/projects/479699850241480705
- N4 Pedro: https://wokwi.com/projects/479710105894416385
- N5 Eduardo: https://wokwi.com/projects/479700208383720449
- [Dashboard en GitHub Pages](https://educmz.github.io/iot-g1-pesca-emisiones/); [canal ThingSpeak 3567916 (privado)](https://thingspeak.com/channels/3567916).
- Broker **de laboratorio**: `broker.emqx.io:1883`, dashboard WebSocket `wss://broker.emqx.io:8084/mqtt`. El broker no tiene garantía de disponibilidad; cada Wokwi requiere configurar su archivo local y todas las instancias deben usar el mismo host y puerto.

## Inicio rápido del equipo

1. El integrador acuerda el contrato MQTT con los cuatro responsables antes de conectarse al broker.
2. Cada integrante crea su rama `nodo1-fernando`, `nodo2-fabricio`, etc., a partir de `main`.
3. Cada uno sube a su carpeta el `sketch.ino`, `diagram.json`, `libraries.txt` (si aplica), enlace Wokwi, evidencias y su panel/botón de ataque.
4. Abre un **Pull Request** hacia `main`. El integrador revisa y prueba antes de fusionarlo.
5. Comparar A vs. B con el **mismo escenario reproducible** y datos propios. C es bonificación.
6. Exponer desde el dashboard; todos participan.

## Reglas de seguridad

- No publicar `secrets.h`, `.env`, claves HMAC, claves de escritura de ThingSpeak ni credenciales reales.
- Copiar los archivos `config.example.*` a un archivo local ignorado por Git.
- Un broker MQTT público no garantiza privacidad, integridad ni disponibilidad. La lógica crítica debe tener un modo seguro y actuaciones locales.
- El MQ-2 en simulación **no** equivale a una medición calibrada de H₂S o NH₃.

## Alcance de las pruebas

**A (obligatoria):** umbral fijo decidido en Edge. **B (obligatoria):** gateway Fog verifica 2 sensores, promedio móvil y viento. **C (opcional):** predicción entrenada con datos propios. Medir tiempos desde detección hasta actuación y falsas activaciones, sin fabricar resultados.

**Restricciones:** sin LoRa; conectividad WAN ≤ S/ 150/mes; considerar corrosión salina; detección/acción local sin internet; heartbeat cada 30 s; confirmar lavador en ≤10 s; al perder gateway >60 s, lavador preventivo; ThingSpeak solo desde gateway a intervalos ≥15 s.

**No presentar el prototipo como integración end-to-end validada:** ThingSpeak funciona para campos 1 y 8; aún se deben comprobar los campos 2–7 con lecturas HMAC frescas. Las claves nunca se versionan.
