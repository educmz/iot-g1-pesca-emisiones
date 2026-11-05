// Configuracion PUBLICA del dashboard G1 Pesca (GitHub Pages).
// Solo endpoints publicos; NUNCA poner aqui HMAC, contrasenas ni Write API Keys.
window.G1_CONFIG = {
  mqttUrl: "wss://broker.emqx.io:8084/mqtt",
  // El canal ThingSpeak 3567916 es privado. Agregar URL de incrustacion
  // solo si el acceso publico ha sido autorizado y no expone claves privadas.
  thingspeakEmbedUrl: ""
};
