# Checklist de entregables (rúbrica del laboratorio)

- [x] Base documental: diagnóstico de ≥5 defectos y requerimiento reescrito (revisar/validar con el equipo).
- [ ] Contrato MQTT **acordado e implementado** por los cinco nodos.
- [ ] Reglas y tabla de problemas/nodos revisadas.
- [x] Enlace del [Cirkit general con cinco nodos](https://app.cirkitdesigner.com/project/373d1c97-d707-40e2-8416-f6e8a5aca5a3) registrado.
- [ ] Captura/exportación del Cirkit general, señales y GPIO verificados contra Wokwi.
- [ ] Wokwi público o compartido por cada nodo, con firmware, `diagram.json` y `libraries.txt` si aplica.
- [ ] MQTT: todos publican y gateway recibe, decide, ordena y confirma.
- [ ] 10 s sin confirmación ⇒ alerta + ventilación máxima; 60 s sin gateway ⇒ encendido preventivo del lavador.
- [ ] Cuatro ataques visibles y rechazados; registro de hora, nodo, tipo, acción y motivo; lavador sigue encendido ante orden falsa.
- [ ] Página HTML local de 5 pestañas con operación y seguridad **en vivo**.
- [ ] ThingSpeak conectado; escrituras solamente desde gateway y ≥15 s.
- [ ] Datos propios de pruebas A y B; tiempos y falsas activaciones; prueba de cambio de parámetro.
- [ ] Planes de contingencia A, B, C probados.
- [ ] IA alternativa C evaluada con datos propios (**bonificación, opcional**).
- [ ] Guion 25 s por integrante y defensa individual ante docente.

**Puntaje:** diagnóstico 3; integración/dashboard 3; comparación 2; ciberseguridad 2; nodo individual 4; exposición individual 3; defensa individual 3; IA C +1 opcional.

**Atención:** no basta con subir código. Cada integrante debe demostrar su nodo y participar en la exposición; sin intervención, el PDF indica que no hay nota.
