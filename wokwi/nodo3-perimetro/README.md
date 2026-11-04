# Nodo 3 — Perímetro

**Responsable:** Gonzalo

## Componentes
MQ-2 (analógica), potenciómetro (viento simulado), LED indicador.

## Reglas y tareas
Convertir el potenciómetro a dirección de viento simulada y enviarla al gateway.

## Ataque y defensa
Mensaje falso «aire limpio»; rechazar por firma y coherencia con pozas/secadores/viento.

## Lo que debes subir aquí
- `sketch.ino` — firmware completo para ESP32.
- `diagram.json` — circuito exportado de Wokwi.
- `libraries.txt` — librerías usadas (si corresponde).
- Capturas o logs reales que demuestren detección, actuación y rechazo del ataque.
- Código de tu panel/botón de ataque integrado en la carpeta que acuerdes con Eduardo.

## Registro de entrega
- **Enlace Wokwi:** https://wokwi.com/projects/479699850241480705
- **Enlace Cirkit (sección de nodo):** https://app.cirkitdesigner.com/project/37ff5f4e-1d1d-4788-b2f4-8eea0eca768c
- **Publicación MQTT:** upc/g1/nodo3/datos (gas, viento, riesgo), alerta, estado; eventos en upc/g1/seguridad/eventos
- **Actuación comprobada:** LED fijo con gas ≥ 5000 ppm simulados y viento 45–135° (ver evidencias/02-riesgo-alto.png)
- **Ataque rechazado y registrado:** aire limpio falso rechazado por firma HMAC y coherencia (ver evidencias/03-ataque-rechazado.png)

Guía técnica, cableado y pruebas: [LEEME.md](LEEME.md) · Evidencias: [evidencias/](evidencias/)
