# Nodo 5 — Gateway Fog (integrador)

**Responsable:** Eduardo

## Componentes
Pulsador (digital), RTC DS1307 (I2C), TFT ILI9341 (SPI), NeoPixel.

## Reglas y tareas
Decisión con 2 sensores + promedio móvil + viento; enviar comandos firmados, alertar tras 10 s sin confirmar, centralizar ThingSpeak.

## Ataque y defensa
Integrar eventos de los cuatro ataques y presentar registro de ciberseguridad.

## Archivos del prototipo
- `sketch.ino` — firmware fase 2 (Wi-Fi + MQTT sin validación HMAC).
- `diagram.json` — circuito exportado de Wokwi.
- `libraries.txt` — librerías utilizadas.
- `wokwi-project.txt` — enlace original.

## Registro de entrega
- **Enlace Wokwi:** https://wokwi.com/projects/479700208383720449
- **Enlace Cirkit (sección de nodo):** PENDIENTE
- **Pruebas locales:** TFT muestra información, RTC detectado, NeoPixel cambia con N/A/C, pulsador reconoce alerta (evidenciado por capturas y consola).
- **Prueba MQTT:** Wi-Fi y MQTT conectados; heartbeat publicado y lecturas/estados de otros nodos recibidos en el monitor serial (captura compartida).
- **Validación HMAC y anti-replay:** PENDIENTE
- **Lógica Fog y orden firmada al lavador:** PENDIENTE
- **Ataques rechazados/registrados:** PENDIENTE
- **ThingSpeak:** PENDIENTE

**Seguridad:** usa HiveMQ público por TCP 1883, sin TLS. Los mensajes entrantes se muestran como **NO VERIFICADOS** y no deben utilizarse todavía para actuar. Las firmas recibidas no se validan. No se incluyen credenciales reales.

**Nota:** el MQ-2 simulado no mide H2S ni NH3 de manera específica o calibrada.
