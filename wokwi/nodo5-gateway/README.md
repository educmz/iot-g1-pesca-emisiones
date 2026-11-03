# Nodo 5 — Gateway Fog (integrador)

**Responsable:** Eduardo

## Componentes
Pulsador (digital), RTC DS1307 (I2C), TFT ILI9341 (SPI), NeoPixel.

## Reglas y tareas
Decisión con 2 sensores + promedio móvil + viento; enviar comandos firmados, alertar tras 10 s sin confirmar, centralizar ThingSpeak.

## Ataque y defensa
Integrar eventos de los cuatro ataques y presentar registro de ciberseguridad.

## Lo que debes subir aquí
- `sketch.ino` — firmware completo para ESP32.
- `diagram.json` — circuito exportado de Wokwi.
- `libraries.txt` — librerías usadas (si corresponde).
- Capturas o logs reales que demuestren detección, actuación y rechazo del ataque.
- Código de tu panel/botón de ataque integrado en la carpeta que acuerdes con Eduardo.

## Registro de entrega
- **Enlace Wokwi:** PENDIENTE
- **Enlace Cirkit (sección de nodo):** PENDIENTE
- **Publicación MQTT:** PENDIENTE
- **Actuación comprobada:** PENDIENTE
- **Ataque rechazado y registrado:** PENDIENTE

No agregar firmware simulado o capturas ficticias para llenar estos campos.
