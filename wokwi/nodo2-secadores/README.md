# Nodo 2 — Secadores

**Responsable:** Fabricio

## Componentes
MQ-2 (analógica), DHT22 (digital), LED RGB.

## Reglas y tareas
Baliza verde/amarillo/rojo; aportar datos para alternativa A e IA C.

## Ataque y defensa
Suplantación: mensaje con id de este nodo sin HMAC válido; rechazar por verificación de firma.

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
