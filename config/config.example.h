#pragma once
// CONFIGURACIÓN DE EJEMPLO. No agregar datos sensibles reales a este archivo.
// Copiar a config.local.h o secrets.h (ambos ignorados por Git) en cada nodo.
#define WIFI_SSID "REEMPLAZAR_LOCALMENTE"
#define WIFI_PASSWORD "REEMPLAZAR_LOCALMENTE"
#define MQTT_HOST "broker.hivemq.com"
#define MQTT_PORT 1883
#define MQTT_NODE_ID "nodoX"
// Definir claves HMAC en el archivo local privado, NUNCA en GitHub.
// Para ThingSpeak, solo el nodo gateway debe tener la clave de escritura local.
