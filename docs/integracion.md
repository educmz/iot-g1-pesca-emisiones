# Guía de integración (responsable: Eduardo)

## Antes de conectar

1. Aprobar el contrato MQTT y unificar firmware: tópicos, mensajes, sensor, seq, firma y órdenes.
2. Verificar que cada nodo hace detección + actuación local.
3. En el laboratorio actual, unificar `broker.emqx.io:1883` en las cinco instancias Wokwi; verificar `MQTT_HOST` (N1, N3, N4, N5) o `MQTT_BROKER` (N2). Usar Client ID **único por instancia**. El acceso de GitHub no modifica el Wokwi guardado.
4. Comprobar heartbeats de 30 s y `offline` al desconectar.
5. Probar que no se puede apagar el lavador con órdenes falsas/repetidas.

**Incidencia actual:** D5 pierde MQTT (state=-3) aunque WiFi=3. HTTPS sí guardó registros reales en ThingSpeak. Ver [diagnóstico detallado](diagnostico-mqtt-2026-11-05.md). No asumir corregido.

## Secuencia obligatoria de prueba

1. Probar D5 solo 120 s con ThingSpeak desactivado; añadir N1, después N2, N3 y N4, comprobando MQTT RX y HMAC ACEPTADO en cada paso. Luego repetir con ThingSpeak activo.
2. Abrir dashboard local; comprobar **datos reales** MQTT de los cinco paneles.
3. Generar pico simulado en pozas y orientar viento hacia comunidad.
4. Observar decisión A y, en prueba separada, decisión B; mantener mismos estímulos y registrar marcas temporales.
5. Orden al lavador; en primera prueba negar/no emitir confirmación. Tras 10 s: `falla_lavador` y `ventilacion_max` en pozas.
6. Verificar el estado en dashboard y ThingSpeak.
7. Simular pérdida de contacto gateway >60 s; lavador debe encenderse preventivamente por lógica local.
8. Activar botones de ataques y observar **eventos de rechazo reales**.
9. Repetir cambiando un parámetro; guardar resultados.

## Planes de contingencia

- **A:** MQTT WebSocket + ThingSpeak disponible.
- **B:** WebSocket bloqueado, consulta ThingSpeak cada 15 s o red alternativa.
- **C:** ambos bloqueados, pantalla TFT de Wokwi y resultados CSV locales.

El plan C debe usar datos guardados de pruebas, **nunca presentados como datos en tiempo real**. La autonomía Edge y el modo seguro del lavador son independientes del plan de visualización.
