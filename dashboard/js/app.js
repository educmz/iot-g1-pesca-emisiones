/* G1 Pesca — interfaz base, sin datos de prueba inventados.
   Los datos proceden exclusivamente de mensajes MQTT recibidos. */
(function () {
  "use strict";
  const names = {
    nodo1: "Pozas · Fernando",
    nodo2: "Secadores · Fabricio",
    nodo3: "Perímetro · Gonzalo",
    nodo4: "Lavador · Pedro",
    nodo5: "Gateway · Eduardo"
  };
  const sensors = {};
  const state = {};
  const solution = document.getElementById("solutionNodes");
  const live = document.getElementById("liveNodes");
  const operations = document.getElementById("operationsLog");
  const security = document.getElementById("securityEvents");
  const status = document.getElementById("connectionStatus");
  const attackFeedback = document.getElementById("attackFeedback");
  const config = window.G1_CONFIG || {};
  let client = null;
  let operationCount = 0;
  let securityCount = 0;

  function el(tag, text, className) {
    const node = document.createElement(tag);
    if (text !== undefined) node.textContent = String(text);
    if (className) node.className = className;
    return node;
  }
  Object.entries(names).forEach(function ([id, label]) {
    const info = el("article", undefined, "node");
    info.append(el("h3", label), el("p", "Sección del nodo; completar con su circuito y parte del problema."));
    solution.appendChild(info);
    const box = el("article", undefined, "node");
    box.id = "live-" + id;
    box.append(el("h3", label));
    const data = el("div", "Sin datos");
    data.id = "data-" + id;
    box.appendChild(data);
    live.appendChild(box);
  });

  document.querySelectorAll(".tab").forEach(function (tab) {
    tab.addEventListener("click", function () {
      document.querySelectorAll(".tab").forEach(function (t) {
        t.classList.toggle("active", t === tab);
      });
      document.querySelectorAll(".panel").forEach(function (p) {
        const active = p.id === tab.dataset.tab;
        p.classList.toggle("active", active);
        p.hidden = !active;
      });
    });
  });

  function addOperation(message) {
    if (!operationCount++) operations.replaceChildren();
    const item = el("li", new Date().toLocaleTimeString("es-PE") + " · " + message);
    operations.prepend(item);
    while (operations.children.length > 35) operations.lastElementChild.remove();
  }
  function showNode(id) {
    const container = document.getElementById("data-" + id);
    if (!container) return;
    container.replaceChildren();
    const table = el("dl");
    const readings = sensors[id] || {};
    Object.entries(readings).forEach(function ([sensor, payload]) {
      table.append(el("dt", sensor), el("dd", (Date.now()-payload.receivedAt>12000 ? "CADUCADO" : String(payload.valor) + " " + (payload.unidad || "")) + " · recibido, HMAC no verificado en navegador"));
    });
    if (state[id]) table.append(el("dt", "Último estado"), el("dd", state[id]));
    if (!Object.keys(readings).length && !state[id]) container.append(el("p", "Sin datos"));
    else container.appendChild(table);
  }

  setInterval(function () { Object.keys(sensors).forEach(showNode); }, 1000);

  function addSecurityEvent(payload) {
    if (!securityCount++) security.replaceChildren();
    const row = document.createElement("tr");
    [payload.hora || "Sin hora", payload.nodo || payload.id || "Desconocido",
      payload.ataque || "Sin tipo", payload.accion || "Sin acción", payload.motivo || "Sin motivo"]
      .forEach(function (value) { row.append(el("td", value)); });
    security.prepend(row);
    while (security.children.length > 50) security.lastElementChild.remove();
  }

  function onMessage(topic, bytes) {
    let data;
    try { data = JSON.parse(bytes.toString()); }
    catch (_) { addOperation("Mensaje no JSON recibido (" + topic + ")"); return; }
    if (!data || typeof data !== "object" || Array.isArray(data)) return;
    if (topic === "upc/g1/seguridad/eventos") {
      addSecurityEvent(data); return;
    }
    const match = topic.match(/^upc\/g1\/(nodo[1-5])\/(datos|alerta|estado)$/);
    if (!match) return;
    const id = match[1];
    const kind = match[2];
    if (kind === "datos" && typeof data.valor !== "undefined") {
      // El dashboard solo visualiza. La firma se valida en el firmware/gateway, nunca con claves en el navegador.
      if (typeof data.valor === "object") return;
      sensors[id] = sensors[id] || {};
      data.receivedAt=Date.now();
      sensors[id][typeof data.sensor === "string" ? data.sensor : "valor"] = data;
      showNode(id);
    }
    if (kind === "estado") {
      state[id] = String(data.estado || data.valor || "actualizado");
      showNode(id);
    }
    if (kind === "alerta") {
      addOperation(id + ": " + String(data.tipo || data.alerta || data.valor || "alerta"));
    }
  }

  function connect() {
    if (!window.mqtt || !config.mqttUrl) {
      status.textContent = "MQTT: falta biblioteca o configuración";
      return;
    }
    try {
      client = window.mqtt.connect(config.mqttUrl, {
        clientId: "g1-panel-" + Math.random().toString(16).slice(2),
        reconnectPeriod: 3000, connectTimeout: 10000, clean: true
      });
      client.on("connect", function () {
        status.textContent = "MQTT: conectado (esperando datos)";
        client.subscribe(["upc/g1/+/datos", "upc/g1/+/alerta",
          "upc/g1/+/estado", "upc/g1/seguridad/eventos"]);
      });
      client.on("message", onMessage);
      client.on("close", function () { status.textContent = "MQTT: desconectado"; });
      client.on("error", function () { status.textContent = "MQTT: error de conexión"; });
    } catch (error) { status.textContent = "MQTT: conexión no disponible"; }
  }

  document.querySelectorAll("[data-attack]").forEach(function (button) {
    button.addEventListener("click", function () {
      if (!client || !client.connected) {
        attackFeedback.textContent = "Sin conexión MQTT: no se envió el ataque.";
        return;
      }
      const message = JSON.stringify({
        id: button.dataset.target,
        ataque: button.dataset.attack,
        ts: Date.now(),
        origen: "dashboard_simulacion"
      });
      client.publish("upc/g1/sim/ataque", message, { qos: 0, retain: false }, function (error) {
        attackFeedback.textContent = error
          ? "No se pudo publicar la solicitud de ataque."
          : "Solicitud de ataque enviada a " + button.dataset.target +
            ". Esperando el evento real del firmware; aún NO se confirma rechazo.";
      });
    });
  });

  const frameUrl = typeof config.thingspeakEmbedUrl === "string" ? config.thingspeakEmbedUrl.trim() : "";
  if (frameUrl) {
    try {
      const url = new URL(frameUrl);
      if (url.protocol === "https:" && (url.hostname === "thingspeak.com" || url.hostname.endsWith(".thingspeak.com"))) {
        const frame = document.createElement("iframe");
        frame.title = "Gráficos ThingSpeak";
        frame.loading = "lazy";
        frame.src = frameUrl;
        document.getElementById("thingspeakContainer").appendChild(frame);
      }
    } catch (_) { /* URL inválida: no se inserta iframe */ }
  }
  connect();
})();
