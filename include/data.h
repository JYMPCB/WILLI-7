#pragma once
#include <Arduino.h>

String Pagina = R"====(
<!doctype html>
<html lang="es">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <title>J2F OTA</title>
  <style>
    :root{
      --bg:#000;
      --card:#0b0b0b;
      --text:#eaeaea;
      --muted:#9aa0a6;
      --accent: rgb(32,244,10);
      --border:#1f1f1f;
    }
    *{ box-sizing:border-box; }
    body{
      margin:24px;
      max-width:760px;
      font-family: system-ui, Arial, sans-serif;
      background:var(--bg);
      color:var(--text);
    }
    .brand{
      display:flex; align-items:center; gap:12px; margin-bottom:14px;
    }
    .dot{
      width:12px; height:12px; border-radius:50%;
      background:var(--accent);
      box-shadow:0 0 14px rgba(32,244,10,0.65);
    }
    h2{ margin: 8px 0 14px; letter-spacing: 0.5px; }
    .card{
      background:var(--card);
      border:1px solid var(--border);
      border-radius:14px;
      padding:16px;
    }
    .row{ display:flex; gap:12px; flex-wrap:wrap; align-items:center; }
    .muted{ color:var(--muted); }
    .ok{ color:var(--accent); }
    .err{ color:#ff5a5a; white-space: pre-wrap; }
    input[type="file"]{
      padding:10px;
      border-radius:12px;
      border:1px solid var(--border);
      background:#050505;
      color:var(--text);
    }
    button{
      padding:10px 14px;
      border-radius:12px;
      border:1px solid var(--border);
      background:#0a0a0a;
      color:var(--text);
      cursor:pointer;
    }
    button.primary{
      border-color: rgba(32,244,10,0.6);
      box-shadow: 0 0 0 2px rgba(32,244,10,0.12) inset;
    }
    button:disabled{ opacity:0.45; cursor:not-allowed; }
    progress{
      width:100%;
      height:18px;
      border-radius:999px;
      overflow:hidden;
      background:#111;
      border:1px solid var(--border);
    }
    /* Chrome/Edge */
    progress::-webkit-progress-bar{ background:#111; }
    progress::-webkit-progress-value{ background:var(--accent); }
    /* Firefox */
    progress::-moz-progress-bar{ background:var(--accent); }

    .hint{ font-size:0.95em; line-height:1.35; }
    code{
      background:#070707;
      border:1px solid var(--border);
      padding:2px 6px;
      border-radius:8px;
      color:var(--text);
    }
  </style>
</head>

<body>
  <div class="brand">
    <div class="dot"></div>
    <div class="muted">J2F · Actualización de firmware</div>
  </div>

  <h2>ACTUALIZAR PANTALLA</h2>

  <div class="card" style="margin-bottom:14px;">
    <div><b>Estado:</b> <span id="state" class="muted">Listo.</span></div>
    <div id="msg" class="muted hint" style="margin-top:8px;">
      1) Elegí el archivo de nombre <b>firmware.bin</b>.<br/>
      2) Presioná el botón <b>Actualizar</b> y esperá a que finalice.<br/>
      3) Al terminar te avisará que ya podés cerrar esta página.
    </div>
    <div id="err" class="err" style="margin-top:10px;"></div>
  </div>

  <div class="card">
    <div class="row">
      <input id="file" type="file" accept=".bin,application/octet-stream"/>
      <button id="btn" class="primary" disabled>Actualizar</button>
    </div>

    <div style="margin:14px 0 6px;">
      <progress id="prog" value="0" max="100"></progress>
    </div>
    <div class="muted"><span id="pct">0</span>%</div>
  </div>

<script>
const file = document.getElementById("file");
const btn  = document.getElementById("btn");
const prog = document.getElementById("prog");
const pct  = document.getElementById("pct");
const msg  = document.getElementById("msg");
const err  = document.getElementById("err");
const state= document.getElementById("state");

function setState(t, cls="muted"){ state.className = cls; state.textContent = t; }
function setMsg(t, cls="muted"){ msg.className = cls + " hint"; msg.innerHTML = t; }
function setErr(t=""){ err.textContent = t; }

file.addEventListener("change", () => {
  setErr("");
  prog.value = 0; pct.textContent = "0";

  const f = file.files && file.files[0];
  if(!f){ btn.disabled = true; return; }

  const ok = f.name.toLowerCase().endsWith(".bin");
  btn.disabled = !ok;
  if(!ok){
    setState("Archivo inválido.", "err");
    setMsg("Elegí un archivo con extensión <b>.bin</b>.");
  } else {
    setState("Listo para actualizar.", "ok");
    setMsg(`Archivo seleccionado: <code>${f.name}</code> · ${Math.round(f.size/1024)} KB`);
  }
});

btn.addEventListener("click", () => {
  setErr("");
  const f = file.files && file.files[0];
  if(!f){ setErr("Elegí un archivo .bin"); return; }
  if(!f.name.toLowerCase().endsWith(".bin")){ setErr("El archivo debe ser .bin"); return; }

  btn.disabled = true;
  setState("Subiendo… no cierres esta pestaña.", "muted");
  setMsg("Cargando firmware. Esto puede tardar unos segundos…");

  const xhr = new XMLHttpRequest();
  xhr.open("POST", "/actualizar", true);

  xhr.upload.onprogress = (ev) => {
    if(ev.lengthComputable){
      const p = Math.round((ev.loaded / ev.total) * 100);
      prog.value = p; pct.textContent = String(p);
    }
  };

  xhr.onreadystatechange = () => {
    if(xhr.readyState !== 4) return;

    if(xhr.status === 200){
      prog.value = 100; pct.textContent = "100";
      // CLAVE: no dependemos de reconexión (porque tal vez la OTA solo existe en “config”)
      setState("Finalizado.", "ok");
      setMsg("<b>Finalizado, puede cerrar esta página.</b><br/>Si la pantalla reinició, es normal que la conexión se corte.");
    } else {
      setState("Falló la actualización.", "err");
      setErr(`HTTP ${xhr.status}\n${xhr.responseText || ""}`);
      btn.disabled = false;
    }
  };

  const data = new FormData();
  data.append("update", f, f.name);
  xhr.send(data);
});
</script>
</body>
</html>
)====";
