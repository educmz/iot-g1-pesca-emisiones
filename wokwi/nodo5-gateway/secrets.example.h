#pragma once
// Copiar localmente como secrets.h en Wokwi. NO publicar secretos.
// N1 usa HMAC_KEY_NODO1; N2 usa HMAC_KEY; N3 usa HMAC_KEY_NODO3;
// N4 usa CLAVE_NODO4; N1/N3/N4 usan la clave D5 para comandos.
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define HMAC_KEY_NODO1 "REEMPLAZAR_CLAVE_PRIVADA_NODO1_32B"
#define HMAC_KEY_NODO2 "REEMPLAZAR_CLAVE_PRIVADA_NODO2_32B"
#define HMAC_KEY_NODO3 "REEMPLAZAR_CLAVE_PRIVADA_NODO3_32B"
#define HMAC_KEY_NODO4 "REEMPLAZAR_CLAVE_PRIVADA_NODO4_32B"
#define HMAC_KEY_GATEWAY "REEMPLAZAR_CLAVE_PRIVADA_D5_32B"
// ThingSpeak es opcional: Write API Key y CA PEM deben quedar privados.
#define THINGSPEAK_WRITE_API_KEY ""
#define THINGSPEAK_ROOT_CA ""
