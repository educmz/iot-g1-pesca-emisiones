# Ciberseguridad Nodo2

Prueba de ataque por MQTT individual observada en captura (19:57:13):
- Nodo: nodo2; tipo SUPLANTACION_O_MENSAJE_INVALIDO; accion RECHAZADO; motivo HMAC_INVALIDO.
- Adjuntar la imagen real a `wokwi/nodo2-secadores/evidencias/05-hmac-rechazado.png`.
- Para el botón del panel grupal, usar backend opcional habilitando `ENABLE_GROUP_ATTACKS=1`, y observar el evento MQTT.
- No afirmar que la defensa del gateway grupal está verificada antes de probarla.
