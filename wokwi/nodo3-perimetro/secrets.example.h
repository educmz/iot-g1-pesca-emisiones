// Copiar como secrets.h en Wokwi. No publicar claves privadas reales.
#pragma once
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define MQTT_HOST "broker.hivemq.com"
#define MQTT_PORT 1883
// Nodo3 firma sus sensores con esta clave, distinta de la clave de comandos D5.
#define HMAC_KEY_NODO3 "REEMPLAZAR_CLAVE_NODO3_LOCALMENTE"
// Compartir PRIVADAMENTE esta clave con el Gateway para mensajes D5|ts|seq|HB|1.
#define HMAC_KEY_GATEWAY "REEMPLAZAR_CLAVE_GATEWAY_LOCALMENTE"
