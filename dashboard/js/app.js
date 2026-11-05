/* G1 Pesca — dashboard en vivo.
   Reglas: los datos proceden EXCLUSIVAMENTE de mensajes MQTT recibidos; no se inventan lecturas.
   La firma HMAC se valida en el firmware/gateway, nunca con claves en el navegador. */
(function () {
  "use strict";

  /* ------------------------------------------------------------------ */
  /* Configuración y metadatos                                          */
  /* ------------------------------------------------------------------ */
  const config = window.G1_CONFIG || {};
  const NODES = {
    nodo1: { n: 1, name: "Pozas", owner: "Fernando", inputs: "MQ-2 analógico, DS18B20 (1-Wire)", outputs: "Buzzer, relé de ventilación" },
    nodo2: { n: 2, name: "Secadores", owner: "Fabricio", inputs: "MQ-2 analógico, DHT22", outputs: "LED RGB" },
    nodo3: { n: 3, name: "Perímetro", owner: "Gonzalo", inputs: "MQ-2 analógico, potenciómetro (viento)", outputs: "LED indicador de riesgo" },
    nodo4: { n: 4, name: "Lavador", owner: "Pedro", inputs: "Interruptor de confirmación", outputs: "Relé del lavador, motor paso a paso A4988" },
    nodo5: { n: 5, name: "Gateway Fog", owner: "Eduardo", inputs: "Pulsador, RTC DS1307 (I2C)", outputs: "TFT ILI9341, NeoPixel" }
  };
  const SENSOR_LABEL = {
    gas_raw: "Gas (ADC)", gas_relativo: "Gas relativo", gas: "Gas", temperatura: "Temperatura", humedad: "Humedad",
    ventilacion: "Ventilación", estado_secadores: "Estado secadores", viento: "Viento", riesgo: "Riesgo",
    lavador: "Lavador", confirmacion: "Confirmación", intensidad: "Intensidad", modo_seguro: "Modo seguro", riesgo_alto: "Riesgo alto"
  };
  const RISK_TEXT = ["Normal", "Alerta", "Crítico"];
  const FRESH_MS = 35000;          // heartbeat 30 s + margen
  const KEEP_MS = 16 * 60 * 1000;  // historial en memoria (≥ ventana máxima de 15 min)
  const MAX_LOG = 20000;

  const $ = (id) => document.getElementById(id);
  const status = $("connectionStatus");
  const pill = $("connectionPill");
  const operations = $("operationsLog");
  const security = $("securityEvents");
  const attackFeedback = $("attackFeedback");

  /* ------------------------------------------------------------------ */
  /* Estado en memoria (solo de esta sesión del navegador)              */
  /* ------------------------------------------------------------------ */
  const series = {};      // "nodo3:gas" -> [{x: ms, y: valor}]
  const latest = {};      // nodo -> sensor -> payload
  const nodeState = {};   // nodo -> { estado, lastSeen, retained }
  const msgCount = {};    // nodo -> número de mensajes
  const recvTimes = [];   // marcas de tiempo de recepción (para msg/min)
  const sessionLog = [];  // filas para CSV
  const secByNode = {};   // nodo -> eventos de seguridad
  let windowMs = 300000;
  let client = null;
  let operationCount = 0;
  let securityCount = 0;
  let attacksSent = 0;
  let lastMsgAt = 0;

  function el(tag, text, className) {
    const node = document.createElement(tag);
    if (text !== undefined && text !== null) node.textContent = String(text);
    if (className) node.className = className;
    return node;
  }
  function hhmmss(ms) {
    return new Date(ms).toLocaleTimeString("es-PE", { hour12: false });
  }
  function ago(ms) {
    const s = Math.round((Date.now() - ms) / 1000);
    if (s < 60) return "hace " + s + " s";
    const m = Math.floor(s / 60);
    return "hace " + m + " min";
  }
  function fmt(v, digits) {
    if (typeof v !== "number" || !isFinite(v)) return String(v);
    return v.toLocaleString("es-PE", { maximumFractionDigits: digits === undefined ? 1 : digits });
  }
  function push(key, t, y, extra) {
    const arr = series[key] || (series[key] = []);
    const p = { x: t, y: y };
    if (extra) Object.assign(p, extra);
    arr.push(p);
    const cutoff = t - KEEP_MS;
    while (arr.length && arr[0].x < cutoff) arr.shift();
  }

  /* ------------------------------------------------------------------ */
  /* Tema claro/oscuro                                                  */
  /* ------------------------------------------------------------------ */
  const root = document.documentElement;
  try { const saved = localStorage.getItem("g1-theme"); if (saved === "light" || saved === "dark") root.dataset.theme = saved; } catch (_) { /* sin almacenamiento */ }
  function effectiveTheme() {
    if (root.dataset.theme) return root.dataset.theme;
    return window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
  }
  $("themeToggle").addEventListener("click", function () {
    const next = effectiveTheme() === "dark" ? "light" : "dark";
    root.dataset.theme = next;
    try { localStorage.setItem("g1-theme", next); } catch (_) { /* ignorar */ }
    restyleCharts();
  });
  if (window.matchMedia) {
    const mq = window.matchMedia("(prefers-color-scheme: dark)");
    if (mq.addEventListener) mq.addEventListener("change", restyleCharts);
  }
  function token(name) { return getComputedStyle(root).getPropertyValue(name).trim(); }

  /* ------------------------------------------------------------------ */
  /* Pestañas                                                           */
  /* ------------------------------------------------------------------ */
  document.querySelectorAll(".tab").forEach(function (tab) {
    tab.addEventListener("click", function () {
      document.querySelectorAll(".tab").forEach(function (t) {
        t.classList.toggle("active", t === tab);
        t.setAttribute("aria-selected", t === tab ? "true" : "false");
      });
      document.querySelectorAll(".panel").forEach(function (p) {
        const active = p.id === tab.dataset.tab;
        p.classList.toggle("active", active);
        p.hidden = !active;
      });
      Object.values(charts).forEach(function (c) { c.resize(); });
      refresh();
    });
  });

  /* ------------------------------------------------------------------ */
  /* Tarjetas de nodos (Solución y Operación) y barra de presencia      */
  /* ------------------------------------------------------------------ */
  const solution = $("solutionNodes");
  const live = $("liveNodes");
  const presence = $("presence");
  Object.entries(NODES).forEach(function ([id, meta]) {
    const info = el("article", undefined, "node");
    const top = el("div", undefined, "node-top");
    const badge = el("span", "N" + meta.n, "node-badge");
    const titles = el("div");
    titles.append(el("h4", meta.name), el("div", meta.owner, "owner"));
    top.append(badge, titles);
    const i1 = el("p", undefined, "io"); i1.append(el("b", "Entradas: "), document.createTextNode(meta.inputs));
    const i2 = el("p", undefined, "io"); i2.append(el("b", "Salidas: "), document.createTextNode(meta.outputs));
    info.append(top, i1, i2);
    solution.appendChild(info);

    const box = el("article", undefined, "node");
    const head = el("div", undefined, "node-top");
    head.append(el("span", "N" + meta.n, "node-badge"));
    const t2 = el("div"); t2.append(el("h4", meta.name), el("div", meta.owner, "owner"));
    head.append(t2);
    const data = el("div");
    data.id = "data-" + id;
    data.append(el("p", "Sin datos", "nodata"));
    box.append(head, data);
    live.appendChild(box);

    const p = el("div", undefined, "pres");
    p.id = "pres-" + id;
    p.dataset.st = "none";
    const txt = el("div");
    txt.append(el("div", "N" + meta.n + " · " + meta.name, "pres-name"), el("div", "Sin datos", "pres-sub"));
    p.append(el("span", undefined, "pres-dot"), txt);
    presence.appendChild(p);
  });

  function showNode(id) {
    const container = $("data-" + id);
    if (!container) return;
    container.replaceChildren();
    const table = el("dl");
    const readings = latest[id] || {};
    Object.entries(readings).forEach(function ([sensor, payload]) {
      let v = typeof payload.valor === "number" ? fmt(payload.valor, 2) + " " + (payload.unidad || "") : String(payload.valor) + " " + (payload.unidad || "");
      if (payload.unidad === "estado") v = Number(payload.valor) ? "Sí" : "No";
      if (payload.unidad === "nivel" && sensor === "riesgo") v = RISK_TEXT[Number(payload.valor)] || v;
      if (payload.unidad === "grados") v = fmt(Number(payload.valor), 0) + "°";
      table.append(el("dt", SENSOR_LABEL[sensor] || sensor), el("dd", v));
    });
    const st = nodeState[id];
    if (st && st.estado) table.append(el("dt", "Estado"), el("dd", st.estado + (st.retained ? " (retenido)" : "")));
    if (!Object.keys(readings).length && !(st && st.estado)) container.append(el("p", "Sin datos", "nodata"));
    else container.appendChild(table);
  }

  function updatePresence() {
    const now = Date.now();
    Object.keys(NODES).forEach(function (id) {
      const p = $("pres-" + id);
      const st = nodeState[id] || {};
      let state = "none", sub = "Sin datos";
      if (st.estado === "offline" && !st.retained && (!st.lastSeen || st.offlineAt >= st.lastSeen)) {
        state = "offline"; sub = "Offline (LWT) · " + ago(st.offlineAt);
      } else if (st.lastSeen && now - st.lastSeen <= FRESH_MS) {
        state = "online"; sub = "En línea · " + ago(st.lastSeen);
      } else if (st.estado === "offline") {
        state = "offline"; sub = "Offline (LWT retenido)";
      } else if (st.lastSeen) {
        state = "stale"; sub = "Sin señal · " + ago(st.lastSeen);
      } else if (st.estado) {
        sub = "Retenido: " + st.estado;
      }
      p.dataset.st = state;
      p.querySelector(".pres-sub").textContent = sub;
    });
  }

  /* ------------------------------------------------------------------ */
  /* Gráficos (Chart.js)                                                */
  /* ------------------------------------------------------------------ */
  const charts = {};
  const lineDefs = [];   // { chart, sets: [{key, color, offset, thr}] }

  const crosshair = {
    id: "crosshair",
    afterDatasetsDraw: function (chart) {
      const active = chart.tooltip && chart.tooltip.getActiveElements ? chart.tooltip.getActiveElements() : [];
      if (!active.length) return;
      const x = active[0].element.x;
      const ctx = chart.ctx;
      ctx.save();
      ctx.strokeStyle = token("--ink-3");
      ctx.globalAlpha = .6;
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(x, chart.chartArea.top);
      ctx.lineTo(x, chart.chartArea.bottom);
      ctx.stroke();
      ctx.restore();
    }
  };

  function baseOptions(extra) {
    const o = {
      responsive: true,
      maintainAspectRatio: false,
      animation: false,
      parsing: false,
      normalized: true,
      interaction: { mode: "nearest", axis: "x", intersect: false },
      layout: { padding: { top: 6, right: 6 } },
      plugins: {
        legend: { display: false },
        tooltip: {
          backgroundColor: "rgba(16,41,44,.95)", titleColor: "#fff", bodyColor: "#e2ebe9",
          padding: 10, cornerRadius: 3, displayColors: true, boxWidth: 8, boxHeight: 8, usePointStyle: true,
          titleFont: { family: "IBM Plex Mono", weight: "500" }, bodyFont: { family: "IBM Plex Mono", size: 12 },
          filter: function (item) { return !item.dataset.isThreshold; },
          callbacks: {
            title: function (items) { return items.length ? hhmmss(items[0].parsed.x) : ""; },
            label: function (item) {
              const raw = item.raw && item.raw.raw !== undefined ? item.raw.raw : item.parsed.y;
              const f = item.dataset.fmtValue;
              return " " + item.dataset.label + ": " + (f ? f(raw) : fmt(raw, 2) + (item.dataset.unit ? " " + item.dataset.unit : ""));
            }
          }
        }
      },
      scales: {
        x: {
          type: "linear",
          grid: { display: false },
          border: { display: true },
          ticks: { maxTicksLimit: 5, maxRotation: 0, callback: function (v) { return hhmmss(v).slice(0, 5 + (windowMs <= 60000 ? 3 : 0)); } }
        },
        y: { grid: { drawTicks: false }, border: { display: false }, ticks: { padding: 8, maxTicksLimit: 5 } }
      }
    };
    if (extra && extra.y) Object.assign(o.scales.y, extra.y);
    return o;
  }

  function makeLine(canvasId, sets, extra) {
    const canvas = $(canvasId);
    if (!canvas || !window.Chart) return null;
    const datasets = sets.map(function (s) {
      return {
        label: s.label, unit: s.unit, fmtValue: s.fmtValue, isThreshold: !!s.thr,
        data: [], borderWidth: s.thr ? 1 : 2, pointRadius: 0, pointHoverRadius: s.thr ? 0 : 5,
        pointHoverBorderWidth: 2, tension: 0, stepped: !!s.stepped,
        borderDash: s.thr ? [5, 4] : undefined,
        fill: s.fill || false, spanGaps: true, cubicInterpolationMode: "monotone"
      };
    });
    const chart = new window.Chart(canvas, { type: "line", data: { datasets: datasets }, options: baseOptions(extra), plugins: [crosshair] });
    charts[canvasId] = chart;
    lineDefs.push({ chart: chart, sets: sets, box: canvas.parentElement });
    return chart;
  }

  function makeBar(canvasId, labels) {
    const canvas = $(canvasId);
    if (!canvas || !window.Chart) return null;
    const chart = new window.Chart(canvas, {
      type: "bar",
      data: { labels: labels, datasets: [{ label: "Mensajes", data: labels.map(function () { return 0; }), borderRadius: 2, borderSkipped: "start", maxBarThickness: 20, categoryPercentage: .7 }] },
      options: {
        responsive: true, maintainAspectRatio: false, animation: false, indexAxis: "y",
        plugins: {
          legend: { display: false },
          tooltip: { backgroundColor: "rgba(16,41,44,.95)", padding: 10, cornerRadius: 3, bodyFont: { family: "IBM Plex Mono" } }
        },
        scales: {
          x: { beginAtZero: true, grid: { drawTicks: false }, border: { display: false }, ticks: { precision: 0, padding: 6 } },
          y: { grid: { display: false } }
        }
      }
    });
    charts[canvasId] = chart;
    return chart;
  }

  function colorOf(c) { return c && c.startsWith("--") ? token(c) : c; }
  function withAlpha(hex, a) {
    const m = /^#([0-9a-f]{6})$/i.exec(hex);
    if (!m) return hex;
    const n = parseInt(m[1], 16);
    return "rgba(" + (n >> 16) + "," + ((n >> 8) & 255) + "," + (n & 255) + "," + a + ")";
  }

  function restyleCharts() {
    if (!window.Chart) return;
    const ink2 = token("--ink-2"), grid = token("--grid"), border = token("--border"), surface = token("--surface");
    window.Chart.defaults.color = ink2;
    window.Chart.defaults.font.family = "'IBM Plex Mono', ui-monospace, monospace";
    window.Chart.defaults.font.size = 11;
    Object.values(charts).forEach(function (ch) {
      Object.values(ch.options.scales).forEach(function (s) {
        if (s.grid) s.grid.color = grid;
        if (s.border) s.border.color = border;
        if (s.ticks) s.ticks.color = ink2;
      });
    });
    lineDefs.forEach(function (d) {
      d.chart.data.datasets.forEach(function (ds, i) {
        const c = colorOf(d.sets[i].color);
        ds.borderColor = c;
        ds.backgroundColor = d.sets[i].fill ? withAlpha(c, .14) : c;
        ds.pointHoverBackgroundColor = c;
        ds.pointHoverBorderColor = surface;
      });
      d.chart.update("none");
    });
    ["chSec", "chCount"].forEach(function (id) {
      const ch = charts[id];
      if (!ch) return;
      ch.data.datasets[0].backgroundColor = token("--s1");
      ch.update("none");
    });
  }

  function initCharts() {
    if (!window.Chart) return;
    const thr = { color: "--ink-3", thr: true, label: "Umbral" };
    makeLine("chN1Gas", [
      { key: "nodo1:gas_raw", color: "--s1", label: "N1 gas", unit: "ADC", fill: "start" },
      Object.assign({ value: 3820 }, thr)
    ]);
    makeLine("chN2Gas", [
      { key: "nodo2:gas_relativo", color: "--s2", label: "N2 gas", unit: "%", fill: "start" },
      Object.assign({ value: 60 }, thr)
    ]);
    makeLine("chN3Gas", [
      { key: "nodo3:gas", color: "--s3", label: "N3 gas", unit: "ppm", fill: "start", min: 1 },
      Object.assign({ value: 1000 }, thr)
    ], { y: { type: "logarithmic", ticks: { padding: 8, callback: function (v) {
      const l = Math.log10(v);
      return Math.abs(l - Math.round(l)) < 1e-6 ? (v >= 1000 ? (v / 1000) + "k" : v) : "";
    } } } });
    makeLine("chTemp", [
      { key: "nodo1:temperatura", color: "--s1", label: "N1 Pozas", unit: "°C" },
      { key: "nodo2:temperatura", color: "--s2", label: "N2 Secadores", unit: "°C" }
    ]);
    makeLine("chHum", [
      { key: "nodo2:humedad", color: "--s2", label: "N2 humedad", unit: "%", fill: "start" }
    ], { y: { suggestedMin: 0, suggestedMax: 100 } });
    const riskFmt = function (v) { return (RISK_TEXT[v] || v) + " (" + v + ")"; };
    makeLine("chRisk", [
      { key: "nodo5:riesgo", color: "--s1", label: "Fog N5", stepped: true, fmtValue: riskFmt },
      { key: "nodo3:riesgo", color: "--s3", label: "Perímetro N3", stepped: true, offset: -0.06, fmtValue: riskFmt }
    ], { y: { min: -0.25, max: 2.25, afterBuildTicks: function (axis) { axis.ticks = [0, 1, 2].map(function (v) { return { value: v }; }); },
      ticks: { padding: 8, callback: function (v) { return RISK_TEXT[v] || ""; } } } });
    const onOff = function (v) { return v ? "Encendido" : "Apagado"; };
    const lanes = ["Ventilación N1", "Confirmación", "Lavador N4"];
    makeLine("chAct", [
      { key: "nodo4:lavador", color: "--s1", label: "Lavador N4", stepped: true, lane: 2, fill: { value: 2 }, fmtValue: onOff },
      { key: "nodo4:confirmacion", color: "--s2", label: "Confirmación N4", stepped: true, lane: 1, fill: { value: 1 }, fmtValue: function (v) { return v ? "Confirmado" : "Sin confirmar"; } },
      { key: "nodo1:ventilacion", color: "--s3", label: "Ventilación N1", stepped: true, lane: 0, fill: { value: 0 }, fmtValue: onOff }
    ], { y: { min: -0.15, max: 2.85, afterBuildTicks: function (axis) { axis.ticks = [0.35, 1.35, 2.35].map(function (v) { return { value: v }; }); },
      ticks: { padding: 8, callback: function (v) { return lanes[Math.round(v - 0.35)] || ""; } } } });
    makeBar("chSec", ["N1 Pozas", "N2 Secadores", "N3 Perímetro", "N4 Lavador"]);
    makeBar("chCount", Object.keys(NODES).map(function (id) { return "N" + NODES[id].n + " " + NODES[id].name; }));
    restyleCharts();
  }

  function refreshCharts() {
    const now = Date.now();
    const min = now - windowMs;
    lineDefs.forEach(function (d) {
      let any = false;
      d.chart.options.scales.x.min = min;
      d.chart.options.scales.x.max = now;
      d.chart.data.datasets.forEach(function (ds, i) {
        const s = d.sets[i];
        if (s.thr) {
          ds.data = [{ x: min, y: s.value }, { x: now, y: s.value }];
          return;
        }
        const src = series[s.key] || [];
        let pts = src.filter(function (p) { return p.x >= min - 5000; });
        if (s.lane !== undefined) pts = pts.map(function (p) { return { x: p.x, y: s.lane + (p.y ? 0.7 : 0), raw: p.y }; });
        else if (s.offset) pts = pts.map(function (p) { return { x: p.x, y: p.y + s.offset, raw: p.y }; });
        else if (s.min) pts = pts.map(function (p) { return { x: p.x, y: Math.max(s.min, p.y), raw: p.y }; });
        // Extiende el último valor escalonado hasta "ahora" (solo visual, si es reciente)
        if (s.stepped && pts.length && now - pts[pts.length - 1].x < FRESH_MS) {
          const last = pts[pts.length - 1];
          pts = pts.concat([{ x: now, y: last.y, raw: last.raw !== undefined ? last.raw : last.y }]);
        }
        if (pts.length) any = true;
        ds.data = pts;
      });
      d.box.classList.toggle("has-data", any);
      d.chart.update("none");
    });
  }

  function refreshBars() {
    const sec = charts.chSec;
    if (sec) {
      const vals = ["nodo1", "nodo2", "nodo3", "nodo4"].map(function (id) { return secByNode[id] || 0; });
      sec.data.datasets[0].data = vals;
      sec.canvas.parentElement.classList.toggle("has-data", vals.some(Boolean));
      sec.update("none");
    }
    const cnt = charts.chCount;
    if (cnt) {
      const vals = Object.keys(NODES).map(function (id) { return msgCount[id] || 0; });
      cnt.data.datasets[0].data = vals;
      cnt.canvas.parentElement.classList.toggle("has-data", vals.some(Boolean));
      cnt.update("none");
    }
  }

  /* ------------------------------------------------------------------ */
  /* KPIs                                                               */
  /* ------------------------------------------------------------------ */
  function sectorPath(fromDeg, toDeg) {
    const r = 44, cx = 50, cy = 50;
    const pt = function (d) { const a = d * Math.PI / 180; return [cx + r * Math.sin(a), cy - r * Math.cos(a)]; };
    const a = pt(fromDeg), b = pt(toDeg);
    return "M50 50 L" + a[0].toFixed(2) + " " + a[1].toFixed(2) + " A" + r + " " + r + " 0 0 1 " + b[0].toFixed(2) + " " + b[1].toFixed(2) + " Z";
  }
  $("communitySector").setAttribute("d", sectorPath(45, 135));

  function lastOf(key) { const a = series[key]; return a && a.length ? a[a.length - 1] : null; }

  function refreshKpis() {
    const now = Date.now();
    // Fog
    const fog = lastOf("nodo5:riesgo");
    const kFog = $("kpiFog");
    if (fog) {
      kFog.dataset.level = String(fog.y);
      $("fogLevel").textContent = RISK_TEXT[fog.y] || ("Nivel " + fog.y);
      const n5 = nodeState.nodo5 || {};
      const extra = n5.fuentes !== undefined && now - n5.fuentesAt <= FRESH_MS ? " · fuentes elevadas: " + n5.fuentes : "";
      $("fogFoot").textContent = "Actualizado " + ago(fog.x) + extra;
    }
    // Gas N3
    const g = lastOf("nodo3:gas");
    if (g) {
      $("kpiN3Gas").textContent = fmt(g.y, 0);
      const pct = Math.max(0, Math.min(1, (Math.log10(Math.max(g.y, 1)) - 2) / 2)) * 100;
      const m = $("n3Meter");
      m.style.width = pct.toFixed(1) + "%";
      m.className = "meter-fill" + (g.y >= 1000 ? " crit" : g.y >= 500 ? " warn" : "");
    }
    // Viento N3
    const w = lastOf("nodo3:viento");
    if (w) {
      $("kpiWind").textContent = fmt(w.y, 0) + "°";
      const arrow = $("windArrow");
      arrow.style.transform = "rotate(" + w.y + "deg)";
      arrow.setAttribute("opacity", "1");
      const diff = Math.abs(((w.y - 90) % 360 + 540) % 360 - 180);
      $("kpiWindFoot").textContent = diff <= 45 ? "Hacia la comunidad (este)" : "Hacia otra dirección";
    }
    // Lavador N4
    const lav = lastOf("nodo4:lavador");
    if (lav) {
      $("kpiWasher").dataset.on = lav.y ? "1" : "0";
      $("kpiWasherVal").textContent = lav.y ? "Encendido" : "Apagado";
      const conf = lastOf("nodo4:confirmacion");
      const ms = lastOf("nodo4:modo_seguro");
      let foot = "Confirmación: " + (conf ? (conf.y ? "sí" : "no") : "—");
      if (ms && ms.y) foot += " · modo seguro";
      $("kpiWasherFoot").textContent = foot;
    }
    // Último valor en las mini-gráficas
    [["lastN1Gas", "nodo1:gas_raw", 0], ["lastN2Gas", "nodo2:gas_relativo", 1], ["lastN3Gas", "nodo3:gas", 0]].forEach(function (r) {
      const p = lastOf(r[1]);
      if (p) $(r[0]).textContent = fmt(p.y, r[2]);
    });
    // Tasa de mensajes
    while (recvTimes.length && recvTimes[0] < now - 60000) recvTimes.shift();
    $("msgRate").textContent = String(recvTimes.length);
    const liveNow = lastMsgAt && now - lastMsgAt < 10000;
    $("liveBadge").hidden = !liveNow;
    if (client && client.connected) {
      pill.dataset.state = liveNow ? "live" : "ok";
      status.textContent = liveNow ? "MQTT: recibiendo datos" : (lastMsgAt ? "MQTT: conectado · último dato " + ago(lastMsgAt) : "MQTT: conectado (esperando datos)");
    }
  }

  function refresh() {
    refreshCharts();
    refreshKpis();
    updatePresence();
  }

  /* ------------------------------------------------------------------ */
  /* Eventos                                                            */
  /* ------------------------------------------------------------------ */
  function severity(data, text) {
    const nivel = Number(data.nivel);
    if (nivel === 2 || /falla|critic|alto|rechaz/i.test(text)) return "crit";
    if (nivel === 1 || /riesgo|alerta|advert/i.test(text)) return "warn";
    if (/on$|ejecutad|confirm|normal/i.test(text)) return "ok";
    return "";
  }
  function addOperation(src, message, sev) {
    if (!operationCount++) operations.replaceChildren();
    const item = el("li");
    if (sev) item.dataset.sev = sev;
    item.append(el("time", hhmmss(Date.now())), el("span", src, "src"), el("span", message));
    operations.prepend(item);
    while (operations.children.length > 60) operations.lastElementChild.remove();
  }

  function addSecurityEvent(payload) {
    if (!securityCount++) security.replaceChildren();
    const nodo = String(payload.nodo || payload.id || "Desconocido");
    const key = /^d4$/i.test(nodo) ? "nodo4" : nodo;
    secByNode[key] = (secByNode[key] || 0) + 1;
    const row = document.createElement("tr");
    [payload.hora || hhmmss(Date.now()), nodo, payload.ataque || "Sin tipo"].forEach(function (v) { row.append(el("td", v)); });
    const act = el("td"); act.append(el("span", payload.accion || "Sin acción", "act")); row.append(act);
    row.append(el("td", payload.motivo || "Sin motivo"));
    security.prepend(row);
    while (security.children.length > 50) security.lastElementChild.remove();
    $("secRecv").textContent = String(securityCount);
    addOperation(NODES[key] ? "N" + NODES[key].n : nodo, "Seguridad: " + (payload.ataque || "evento") + " → " + (payload.accion || ""), "crit");
    refreshBars();
  }

  function logRow(topic, node, data) {
    sessionLog.push([new Date().toISOString(), topic, node, data.sensor || data.tipo || data.estado || "", data.valor !== undefined ? data.valor : "", data.unidad || "", data.calidad || "", data.seq !== undefined ? data.seq : ""]);
    if (sessionLog.length > MAX_LOG) sessionLog.shift();
  }

  /* ------------------------------------------------------------------ */
  /* MQTT                                                               */
  /* ------------------------------------------------------------------ */
  function onMessage(topic, bytes, packet) {
    const now = Date.now();
    const retained = !!(packet && packet.retain);
    let data;
    try { data = JSON.parse(bytes.toString()); }
    catch (_) { addOperation("MQTT", "Mensaje no JSON recibido (" + topic + ")", "warn"); return; }
    if (!data || typeof data !== "object" || Array.isArray(data)) return;

    if (topic === "upc/g1/seguridad/eventos") {
      if (!retained) { lastMsgAt = now; recvTimes.push(now); }
      logRow(topic, String(data.nodo || data.id || ""), data);
      addSecurityEvent(data);
      return;
    }
    const match = topic.match(/^upc\/g1\/(nodo[1-5])\/(datos|alerta|estado)$/);
    if (!match) return;
    const id = match[1];
    const kind = match[2];
    const st = nodeState[id] || (nodeState[id] = {});
    if (!retained) {
      st.lastSeen = now; lastMsgAt = now; recvTimes.push(now);
      msgCount[id] = (msgCount[id] || 0) + 1;
    }
    logRow(topic, id, data);

    if (kind === "datos" && typeof data.valor !== "undefined") {
      if (typeof data.valor === "object") return;
      const sensor = typeof data.sensor === "string" ? data.sensor : "valor";
      (latest[id] = latest[id] || {})[sensor] = data;
      const v = Number(data.valor);
      if (isFinite(v) && !retained) push(id + ":" + sensor, now, v);
      showNode(id);
    }
    if (kind === "estado") {
      st.estado = String(data.estado || data.valor || "actualizado");
      st.retained = retained;
      if (st.estado === "offline" && !retained) st.offlineAt = now;
      if (id === "nodo5" && data.riesgo !== undefined && isFinite(Number(data.riesgo)) && !retained) {
        push("nodo5:riesgo", now, Number(data.riesgo));
      }
      if (st.estado === "offline" && !retained) addOperation("N" + NODES[id].n, "Nodo offline (LWT)", "crit");
      showNode(id);
    }
    if (kind === "alerta") {
      if (id === "nodo5" && data.nivel !== undefined && isFinite(Number(data.nivel))) {
        st.fuentes = data.fuentes; st.fuentesAt = now;
        if (!retained) push("nodo5:riesgo", now, Number(data.nivel));
      }
      let text = String(data.tipo || data.alerta || data.sensor || data.valor || "alerta");
      if (data.nivel !== undefined) text += " · nivel " + data.nivel + " (" + (RISK_TEXT[data.nivel] || "?") + ")";
      if (data.fuentes !== undefined) text += " · fuentes " + data.fuentes;
      if (data.motivo) text += " — " + data.motivo;
      addOperation("N" + NODES[id].n, text + (retained ? " (retenido)" : ""), severity(data, text));
    }
    refreshKpis();
    updatePresence();
    refreshBars();
  }

  function setConn(state, text) { pill.dataset.state = state; status.textContent = text; }

  function connect() {
    if (!window.mqtt || !config.mqttUrl) {
      setConn("err", "MQTT: falta biblioteca o configuración");
      return;
    }
    try {
      setConn("wait", "MQTT: conectando…");
      client = window.mqtt.connect(config.mqttUrl, {
        clientId: "g1-panel-" + Math.random().toString(16).slice(2),
        reconnectPeriod: 3000, connectTimeout: 10000, clean: true
      });
      client.on("connect", function () {
        setConn("ok", "MQTT: conectado (esperando datos)");
        client.subscribe(["upc/g1/+/datos", "upc/g1/+/alerta",
          "upc/g1/+/estado", "upc/g1/seguridad/eventos"]);
      });
      client.on("message", onMessage);
      client.on("reconnect", function () { setConn("wait", "MQTT: reconectando…"); });
      client.on("close", function () { setConn("err", "MQTT: desconectado"); });
      client.on("error", function () { setConn("err", "MQTT: error de conexión"); });
    } catch (error) { setConn("err", "MQTT: conexión no disponible"); }
  }

  /* ------------------------------------------------------------------ */
  /* Ataques simulados                                                  */
  /* ------------------------------------------------------------------ */
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
        if (!error) { attacksSent++; $("secSent").textContent = String(attacksSent); }
        attackFeedback.textContent = error
          ? "No se pudo publicar la solicitud de ataque."
          : "Solicitud de ataque enviada a " + button.dataset.target +
            ". Esperando el evento real del firmware; aún NO se confirma rechazo.";
      });
    });
  });

  /* ------------------------------------------------------------------ */
  /* Controles: ventana de tiempo y CSV                                 */
  /* ------------------------------------------------------------------ */
  document.querySelectorAll("#windowSeg button").forEach(function (b) {
    b.addEventListener("click", function () {
      windowMs = Number(b.dataset.win) * 1000;
      document.querySelectorAll("#windowSeg button").forEach(function (x) { x.classList.toggle("on", x === b); });
      refreshCharts();
    });
  });
  $("exportCsv").addEventListener("click", function () {
    const head = ["recibido_iso", "topico", "nodo", "sensor_o_tipo", "valor", "unidad", "calidad", "seq"];
    const esc = function (v) { const s = String(v); return /[",\n;]/.test(s) ? '"' + s.replace(/"/g, '""') + '"' : s; };
    const csv = [head].concat(sessionLog).map(function (r) { return r.map(esc).join(","); }).join("\n");
    const blob = new Blob(["﻿" + csv], { type: "text/csv;charset=utf-8" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = "g1-pesca-sesion-" + new Date().toISOString().slice(0, 19).replace(/[:T]/g, "-") + ".csv";
    document.body.appendChild(a);
    a.click();
    setTimeout(function () { URL.revokeObjectURL(a.href); a.remove(); }, 0);
  });

  /* ------------------------------------------------------------------ */
  /* ThingSpeak (solo si se autoriza una URL pública)                   */
  /* ------------------------------------------------------------------ */
  const frameUrl = typeof config.thingspeakEmbedUrl === "string" ? config.thingspeakEmbedUrl.trim() : "";
  if (frameUrl) {
    try {
      const url = new URL(frameUrl);
      if (url.protocol === "https:" && (url.hostname === "thingspeak.com" || url.hostname.endsWith(".thingspeak.com"))) {
        const frame = document.createElement("iframe");
        frame.title = "Gráficos ThingSpeak";
        frame.loading = "lazy";
        frame.src = frameUrl;
        $("thingspeakContainer").appendChild(frame);
      }
    } catch (_) { /* URL inválida: no se inserta iframe */ }
  }

  initCharts();
  refresh();
  refreshBars();
  setInterval(refresh, 1000);
  connect();
})();
