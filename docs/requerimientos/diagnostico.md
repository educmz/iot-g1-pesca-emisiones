# Diagnóstico del requerimiento original

Requerimiento recibido: «Queremos un sistema con inteligencia artificial en la nube que controle todos los olores de la planta en tiempo real, para que los vecinos ya no se quejen. Tiene que ser 100 % confiable, barato y funcionar aunque se caiga el internet. Usen LoRa, que nos dijeron que es lo mejor.»

## Defectos identificados

1. **Contradicción nube/sin internet:** no se define la autonomía local.
2. **Confiabilidad absoluta:** 100 % no define un objetivo realista o una prueba.
3. **Tiempo real ambiguo:** sin latencia máxima medible.
4. **Barato indefinido:** sin presupuesto en el pedido (el caso sí fija WAN ≤ S/150/mes).
5. **LoRa impuesto y prohibido:** contradice las restricciones de auditoría.
6. **Alcance indefinido:** «todos los olores» no establece fuentes ni variables.
7. **Quejas como garantía:** no se puede asegurar eliminar todas las quejas solo con IoT.
8. **Sin seguridad ni lazo cerrado:** no exige autenticación, confirmación o manejo de fallos.

Ver [requerimiento verificable](requerimiento.md). El diagnóstico responde a la primera tarea del PDF; los criterios de pruebas y valores faltantes deben precisarse con el grupo.
