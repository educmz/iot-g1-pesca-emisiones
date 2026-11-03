# Alternativa C — IA predictiva (bonificación opcional +1)

**Responsable principal:** Fabricio; datos de los cinco integrantes y comparaciones con Gonzalo.

1. Recopilar observaciones reales de la simulación con timestamp, gas, temperatura, humedad, viento y etiqueta de riesgo.
2. Separar datos de entrenamiento/prueba por corrida o por tiempo para evitar fuga de información.
3. Entrenar un modelo **local y gratuito** (p. ej. scikit-learn).
4. Comparar contra A y B bajo los mismos eventos; registrar tiempos de actuación y falsas activaciones.
5. Si C no supera a las reglas, explicar los resultados con evidencia.
6. Nunca sustituir protección Edge/Fog por inferencia de la IA.

Colocar datasets propios en `datos/`, scripts en `entrenamiento/` y modelo documentado en `modelos/`. No crear métricas ni datasets falsos.

**Estado actual:** no implementado.
