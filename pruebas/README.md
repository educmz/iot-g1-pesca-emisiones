# Pruebas y comparación de alternativas

## Cómo medir

Definir escenarios idénticos por corrida: condiciones normales, pico real, lectura falsa, viento hacia/desde comunidad, fallo de confirmación y pérdida de comunicación. Usar **los mismos estímulos** para A y B (y C opcional). Conservar los timestamps originales de inicio del pico, detección y actuación.

- **Latencia:** `t_actuacion - t_deteccion` (segundos); definir actuación como salida/relé efectivo, no solo comando enviado.
- **Falsa activación:** arranque del lavador en corrida sin pico verdadero simulado.
- **Picos no detectados:** escenario con pico confirmado por la inyección, sin actuación dentro del plazo de observación.
- **Cambio de parámetro:** repetir una corrida cambiando un umbral o ventana; guardar valores y efecto.

La plantilla CSV incluye solo cabecera; **no contiene resultados**.

## Evidencia requerida
- Registros CSV y resultados calculados para A y B.
- Capturas/logs de Wokwi + eventos MQTT y dashboard.
- Gráficos en ThingSpeak o desde datos propios.
- Justificación de alternativa elegida.
- Ataques: usar `ciberseguridad/README.md` y registrar rechazos reales.

La alternativa C es opcional. **No inventar tiempos ni falsas alarmas**.
