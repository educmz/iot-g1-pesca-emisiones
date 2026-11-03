# Nodo 4 — Lavador de gases

**Responsable:** Pedro

## Componentes
Interruptor de confirmación (digital), relé, motor paso a paso + A4988.

## Reglas y tareas
Confirmar funcionamiento por interruptor; si pierde gateway >60 s debe encenderse autónomamente.

## Ataque y defensa
Orden falsa de apagado o replay; rechazar por HMAC, contador y condición crítica.

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
