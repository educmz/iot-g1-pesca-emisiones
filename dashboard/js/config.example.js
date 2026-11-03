// Copiar como config.js (ignorado en Git). No incluir claves HMAC, contraseñas o Write API keys aquí.
// Verificar endpoint WSS y conectividad; este es un ejemplo del broker público del laboratorio.
window.G1_CONFIG = {
  mqttUrl: "wss://broker.hivemq.com:8884/mqtt",
  // Configurar solo cuando el canal ThingSpeak exista y su lectura esté autorizada:
  thingspeakEmbedUrl: ""
};
