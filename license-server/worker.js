/**
 * Farshad ESP — Cloudflare Worker + KV License API
 * English admin panel + enable/disable + fixed create
 */

const OFFSETS_URL = "https://raw.githubusercontent.com/farshadhelboys-crypto/D3D9-Overlay-ImGui/main/offsets/offsets.json";

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const path = url.pathname;
    const cors = {
      "Access-Control-Allow-Origin": "*",
      "Access-Control-Allow-Headers": "Content-Type, X-Admin-Key",
      "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
    };
    if (request.method === "OPTIONS") return new Response(null, { headers: cors });

    try {
      if (path === "/api/time" && request.method === "GET") {
        return json({ ok: true, server_time: Math.floor(Date.now() / 1000) }, cors);
      }

      if (path === "/api/offsets" && request.method === "GET") {
        const r = await fetch(OFFSETS_URL, { cf: { cacheTtl: 60 } });
        const text = await r.text();
        return new Response(text, { headers: { ...cors, "Content-Type": "application/json" } });
      }

      if (path === "/api/activate" && request.method === "POST") {
        const body = await request.json();
        return json(await activate(env, body), cors);
      }

      if (path === "/api/heartbeat" && request.method === "POST") {
        const body = await request.json();
        return json(await heartbeat(env, body), cors);
      }

      if (path === "/api/report_debug" && request.method === "POST") {
        const body = await request.json();
        return json(await reportDebug(env, body), cors);
      }

      if (path === "/api/messages" && request.method === "POST") {
        const body = await request.json();
        return json(await heartbeat(env, body), cors);
      }

      if (path.startsWith("/admin")) {
        const key = request.headers.get("X-Admin-Key") || "";
        if (!env.ADMIN_KEY || key !== env.ADMIN_KEY) {
          return json({ ok: false, error: "unauthorized" }, cors, 401);
        }

        if (path === "/admin/list" && request.method === "GET")
          return json(await listLicenses(env), cors);
        if (path === "/admin/stats" && request.method === "GET")
          return json(await stats(env), cors);
        if (path === "/admin/create" && request.method === "POST")
          return json(await createLicense(env, await request.json()), cors);
        if (path === "/admin/disable" && request.method === "POST")
          return json(await disableLicense(env, await request.json()), cors);
        if (path === "/admin/enable" && request.method === "POST")
          return json(await enableLicense(env, await request.json()), cors);
        if (path === "/admin/message" && request.method === "POST")
          return json(await sendMessage(env, await request.json()), cors);

        return json({ ok: false, error: "not found" }, cors, 404);
      }

      if (path === "/" || path === "/admin" || path === "/index.html") {
        return new Response(ADMIN_HTML, {
          headers: { ...cors, "Content-Type": "text/html; charset=utf-8" },
        });
      }

      return json({ ok: false, error: "not found" }, cors, 404);
    } catch (e) {
      return json({ ok: false, error: String(e && e.message ? e.message : e) }, cors, 500);
    }
  },
};

function json(obj, cors = {}, status = 200) {
  return new Response(JSON.stringify(obj), {
    status,
    headers: { ...cors, "Content-Type": "application/json" },
  });
}

function genKey() {
  const a = crypto.getRandomValues(new Uint8Array(8));
  return Array.from(a, (b) => b.toString(16).padStart(2, "0"))
    .join("")
    .toUpperCase()
    .replace(/(.{4})/g, "$1-")
    .slice(0, -1);
}

async function getLic(env, key) {
  if (!env.KV) return null;
  const raw = await env.KV.get("lic:" + key);
  return raw ? JSON.parse(raw) : null;
}

async function putLic(env, key, data) {
  await env.KV.put("lic:" + key, JSON.stringify(data));
}

async function telegramMember(env, tgId) {
  if (!env.TELEGRAM_BOT_TOKEN || !env.TELEGRAM_CHANNEL_ID || !tgId) {
    return { ok: true, skipped: true };
  }
  const apiUrl =
    "https://api.telegram.org/bot" +
    env.TELEGRAM_BOT_TOKEN +
    "/getChatMember?chat_id=" +
    encodeURIComponent(env.TELEGRAM_CHANNEL_ID) +
    "&user_id=" +
    encodeURIComponent(tgId);
  const r = await fetch(apiUrl);
  const j = await r.json();
  if (!j.ok) return { ok: false, error: "telegram_api" };
  const st = j.result && j.result.status;
  const ok = st === "member" || st === "administrator" || st === "creator";
  return { ok: ok, status: st };
}

async function activate(env, body) {
  const key = String(body.key || "").trim().toUpperCase();
  const hwid = String(body.hwid || "").trim();
  const tgId = body.tg_id ? String(body.tg_id).trim() : "";
  if (!key || !hwid) return { ok: false, error: "missing_fields" };

  const lic = await getLic(env, key);
  if (!lic) return { ok: false, error: "invalid_key", message: "Invalid license key" };

  // treat only explicit true as disabled
  if (lic.disabled === true) {
    return {
      ok: false,
      error: "disabled",
      message: "License disabled: " + (lic.disable_reason || "admin"),
      reason: lic.disable_reason || "disabled",
    };
  }

  const now = Math.floor(Date.now() / 1000);
  if (lic.expires_at && now > Number(lic.expires_at)) {
    return { ok: false, error: "expired", message: "License expired" };
  }

  // Telegram only if BOTH configured
  if (env.TELEGRAM_BOT_TOKEN && env.TELEGRAM_CHANNEL_ID) {
    if (!tgId) {
      return { ok: false, error: "join_channel", message: "Please join the Telegram channel and enter your numeric Telegram ID" };
    }
    const mem = await telegramMember(env, tgId);
    if (!mem.ok) {
      return { ok: false, error: "join_channel", message: "Please join the Telegram channel" };
    }
  }

  if (lic.hwid && lic.hwid !== hwid) {
    return { ok: false, error: "hwid_mismatch", message: "This license is locked to another PC" };
  }
  if (!lic.hwid) {
    lic.hwid = hwid;
    lic.activated_at = now;
  }
  if (tgId) lic.tg_id = tgId;
  lic.disabled = false;
  lic.last_seen = now;
  await putLic(env, key, lic);

  return {
    ok: true,
    server_time: now,
    expires_at: lic.expires_at,
    days_left: lic.expires_at ? Math.max(0, Math.floor((lic.expires_at - now) / 86400)) : null,
    messages: lic.pending_messages || [],
  };
}

async function heartbeat(env, body) {
  const key = String(body.key || "").trim().toUpperCase();
  const hwid = String(body.hwid || "").trim();
  const lic = await getLic(env, key);
  if (!lic) return { ok: false, error: "invalid_key" };
  if (lic.disabled === true) {
    return { ok: false, error: "disabled", reason: lic.disable_reason || "" };
  }
  const now = Math.floor(Date.now() / 1000);
  if (lic.expires_at && now > Number(lic.expires_at)) {
    return { ok: false, error: "expired" };
  }
  if (lic.hwid && lic.hwid !== hwid) return { ok: false, error: "hwid_mismatch" };

  lic.last_seen = now;
  const msgs = lic.pending_messages || [];
  lic.pending_messages = [];
  await putLic(env, key, lic);

  return {
    ok: true,
    server_time: now,
    expires_at: lic.expires_at,
    days_left: lic.expires_at ? Math.max(0, Math.floor((lic.expires_at - now) / 86400)) : null,
    messages: msgs,
  };
}

async function reportDebug(env, body) {
  const key = String(body.key || "").trim().toUpperCase();
  const lic = await getLic(env, key);
  if (!lic) return { ok: false };
  lic.disabled = true;
  lic.disable_reason = "debug_detected";
  lic.disabled_at = Math.floor(Date.now() / 1000);
  await putLic(env, key, lic);
  return { ok: true, disabled: true };
}

async function createLicense(env, body) {
  if (!env.KV) return { ok: false, error: "KV not bound — add KV binding named KV in Worker settings" };
  const days = Math.max(1, Number(body.days) || 30);
  const note = body.note || "";
  const key = String(body.key || genKey()).trim().toUpperCase();
  const now = Math.floor(Date.now() / 1000);
  const lic = {
    key: key,
    created_at: now,
    expires_at: now + days * 86400,
    hwid: null,
    tg_id: null,
    disabled: false,
    disable_reason: null,
    note: note,
    last_seen: null,
    pending_messages: [],
  };
  await putLic(env, key, lic);

  const idxRaw = await env.KV.get("idx:keys");
  let idx = [];
  try { idx = idxRaw ? JSON.parse(idxRaw) : []; } catch (e) { idx = []; }
  if (idx.indexOf(key) === -1) idx.push(key);
  await env.KV.put("idx:keys", JSON.stringify(idx));

  return {
    ok: true,
    license: lic,
    status: "active",
    days_left: days,
  };
}

async function disableLicense(env, body) {
  const key = String(body.key || "").trim().toUpperCase();
  const lic = await getLic(env, key);
  if (!lic) return { ok: false, error: "not_found" };
  lic.disabled = true;
  lic.disable_reason = body.reason || "admin_disabled";
  lic.disabled_at = Math.floor(Date.now() / 1000);
  await putLic(env, key, lic);
  return { ok: true };
}

async function enableLicense(env, body) {
  const key = String(body.key || "").trim().toUpperCase();
  const lic = await getLic(env, key);
  if (!lic) return { ok: false, error: "not_found" };
  lic.disabled = false;
  lic.disable_reason = null;
  // optional: extend days
  if (body.days) {
    const days = Math.max(1, Number(body.days) || 30);
    const now = Math.floor(Date.now() / 1000);
    lic.expires_at = now + days * 86400;
  }
  await putLic(env, key, lic);
  return { ok: true, license: lic };
}

async function sendMessage(env, body) {
  const title = body.title || "Message";
  const text = body.body || body.text || "";
  const msg = { id: Date.now(), title: title, body: text, ts: Math.floor(Date.now() / 1000) };
  const idxRaw = await env.KV.get("idx:keys");
  let idx = [];
  try { idx = idxRaw ? JSON.parse(idxRaw) : []; } catch (e) { idx = []; }

  if (body.broadcast) {
    for (let i = 0; i < idx.length; i++) {
      const lic = await getLic(env, idx[i]);
      if (!lic || lic.disabled === true) continue;
      lic.pending_messages = lic.pending_messages || [];
      lic.pending_messages.push(msg);
      await putLic(env, idx[i], lic);
    }
    return { ok: true, sent: "broadcast" };
  }

  const key = String(body.key || "").trim().toUpperCase();
  const lic = await getLic(env, key);
  if (!lic) return { ok: false, error: "not_found" };
  lic.pending_messages = lic.pending_messages || [];
  lic.pending_messages.push(msg);
  await putLic(env, key, lic);
  return { ok: true, sent: key };
}

async function listLicenses(env) {
  if (!env.KV) return { ok: false, error: "KV not bound", licenses: [] };
  const idxRaw = await env.KV.get("idx:keys");
  let idx = [];
  try { idx = idxRaw ? JSON.parse(idxRaw) : []; } catch (e) { idx = []; }
  const now = Math.floor(Date.now() / 1000);
  const list = [];
  for (let i = 0; i < idx.length; i++) {
    const lic = await getLic(env, idx[i]);
    if (!lic) continue;
    const disabled = lic.disabled === true;
    const expired = !!(lic.expires_at && now > Number(lic.expires_at));
    const active = !disabled && !expired;
    list.push({
      key: lic.key,
      created_at: lic.created_at,
      expires_at: lic.expires_at,
      hwid: lic.hwid,
      tg_id: lic.tg_id,
      disabled: disabled,
      disable_reason: lic.disable_reason || "",
      note: lic.note || "",
      last_seen: lic.last_seen,
      days_left: lic.expires_at ? Math.max(0, Math.floor((Number(lic.expires_at) - now) / 86400)) : null,
      active: active,
      status: disabled ? "disabled" : (expired ? "expired" : "active"),
    });
  }
  return { ok: true, licenses: list, server_time: now };
}

async function stats(env) {
  const data = await listLicenses(env);
  if (!data.ok) return data;
  let active = 0, disabled = 0, expired = 0;
  for (let i = 0; i < data.licenses.length; i++) {
    const l = data.licenses[i];
    if (l.disabled) disabled++;
    else if (l.active) active++;
    else expired++;
  }
  return {
    ok: true,
    total: data.licenses.length,
    active: active,
    disabled: disabled,
    expired: expired,
    server_time: data.server_time,
  };
}

const ADMIN_HTML = "<!DOCTYPE html>\n" +
"<html lang=\"en\">\n" +
"<head>\n" +
"<meta charset=\"utf-8\"/>\n" +
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>\n" +
"<title>Farshad ESP Admin</title>\n" +
"<style>\n" +
"body{font-family:Segoe UI,Tahoma,sans-serif;background:#111;color:#eee;margin:0;padding:16px}\n" +
"input,button,textarea{font:inherit;padding:8px;margin:4px 0;border-radius:6px;border:1px solid #333;background:#1a1a1a;color:#eee}\n" +
"button{background:#0af;color:#000;cursor:pointer;border:none;font-weight:bold;margin-right:6px}\n" +
"button.danger{background:#e33;color:#fff}\n" +
"button.ok{background:#2a2;color:#fff}\n" +
".card{background:#1a1a1a;padding:12px;border-radius:10px;margin:10px 0}\n" +
"table{width:100%;border-collapse:collapse;font-size:13px}\n" +
"td,th{border:1px solid #333;padding:6px;text-align:left}\n" +
".ok{color:#0f0}.bad{color:#f55}\n" +
"#err{color:#f55;min-height:1.2em}\n" +
"</style>\n" +
"</head>\n" +
"<body>\n" +
"<h1>Farshad ESP — Admin Panel</h1>\n" +
"<div class=\"card\">\n" +
"  <label>Admin Key</label><br/>\n" +
"  <input id=\"akey\" type=\"password\" style=\"width:280px\" placeholder=\"ADMIN_KEY\"/>\n" +
"  <button type=\"button\" onclick=\"loadAll()\">Login / Refresh</button>\n" +
"  <div id=\"err\"></div>\n" +
"</div>\n" +
"<div class=\"card\" id=\"stats\">—</div>\n" +
"<div class=\"card\">\n" +
"  <h3>Create License</h3>\n" +
"  Days: <input id=\"days\" type=\"number\" value=\"30\" style=\"width:80px\"/>\n" +
"  Note: <input id=\"note\" style=\"width:200px\"/>\n" +
"  <button type=\"button\" onclick=\"createLic()\">Create</button>\n" +
"  <pre id=\"created\"></pre>\n" +
"</div>\n" +
"<div class=\"card\">\n" +
"  <h3>Send Message</h3>\n" +
"  Key (empty = broadcast): <input id=\"mkey\" style=\"width:200px\"/>\n" +
"  Title: <input id=\"mtitle\" value=\"Admin message\"/>\n" +
"  Body: <textarea id=\"mbody\" rows=\"3\" style=\"width:100%\"></textarea>\n" +
"  <button type=\"button\" onclick=\"sendMsg(false)\">Send to key</button>\n" +
"  <button type=\"button\" onclick=\"sendMsg(true)\">Broadcast all</button>\n" +
"</div>\n" +
"<div class=\"card\">\n" +
"  <h3>Licenses</h3>\n" +
"  <div id=\"list\"></div>\n" +
"</div>\n" +
"<script>\n" +
"var api = location.origin;\n" +
"function headers(){return {'Content-Type':'application/json','X-Admin-Key':document.getElementById('akey').value};}\n" +
"function showErr(m){document.getElementById('err').textContent=m||'';}\n" +
"async function loadAll(){\n" +
"  showErr('');\n" +
"  try {\n" +
"    var s = await fetch(api+'/admin/stats',{headers:headers()}).then(function(r){return r.json();});\n" +
"    if(!s.ok){ showErr('Error: '+(s.error||JSON.stringify(s))); document.getElementById('stats').textContent='—'; return; }\n" +
"    document.getElementById('stats').innerHTML = 'Total: '+s.total+' | Active: <span class=\"ok\">'+s.active+'</span> | Disabled: <span class=\"bad\">'+s.disabled+'</span> | Expired: '+s.expired;\n" +
"    var l = await fetch(api+'/admin/list',{headers:headers()}).then(function(r){return r.json();});\n" +
"    if(!l.ok){ document.getElementById('list').textContent=JSON.stringify(l); return; }\n" +
"    var h = '<table><tr><th>Key</th><th>Status</th><th>Days left</th><th>HWID</th><th>Reason</th><th>Actions</th></tr>';\n" +
"    for(var i=0;i<l.licenses.length;i++){\n" +
"      var x=l.licenses[i];\n" +
"      var st = x.status || (x.disabled?'disabled':(x.active?'active':'expired'));\n" +
"      var hw=(x.hwid||'-'); if(hw.length>14) hw=hw.slice(0,14);\n" +
"      h += '<tr><td>'+x.key+'</td><td>'+st+'</td><td>'+(x.days_left!=null?x.days_left:'-')+'</td><td>'+hw+'</td><td>'+(x.disable_reason||x.note||'')+'</td><td>';\n" +
"      if(x.disabled) h += '<button type=\"button\" class=\"ok\" onclick=\"en(\\''+x.key+'\\')\">Enable</button> ';\n" +
"      else h += '<button type=\"button\" class=\"danger\" onclick=\"dis(\\''+x.key+'\\')\">Disable</button> ';\n" +
"      h += '</td></tr>';\n" +
"    }\n" +
"    h += '</table>';\n" +
"    document.getElementById('list').innerHTML = h;\n" +
"  } catch(e){ showErr('Network error: '+e); }\n" +
"}\n" +
"async function createLic(){\n" +
"  showErr('');\n" +
"  try {\n" +
"    var days = +document.getElementById('days').value;\n" +
"    var note = document.getElementById('note').value;\n" +
"    var j = await fetch(api+'/admin/create',{method:'POST',headers:headers(),body:JSON.stringify({days:days,note:note})}).then(function(r){return r.json();});\n" +
"    if(j.ok){\n" +
"      document.getElementById('created').textContent = 'KEY: '+j.license.key+'\\nstatus: active\\nexpires: '+new Date(j.license.expires_at*1000).toLocaleString();\n" +
"      loadAll();\n" +
"    } else {\n" +
"      document.getElementById('created').textContent = JSON.stringify(j);\n" +
"      showErr(j.error||'create failed');\n" +
"    }\n" +
"  } catch(e){ showErr('Network error: '+e); }\n" +
"}\n" +
"async function dis(key){\n" +
"  if(!confirm('Disable this license?')) return;\n" +
"  try {\n" +
"    await fetch(api+'/admin/disable',{method:'POST',headers:headers(),body:JSON.stringify({key:key,reason:'admin_disabled'})});\n" +
"    loadAll();\n" +
"  } catch(e){ showErr(''+e); }\n" +
"}\n" +
"async function en(key){\n" +
"  try {\n" +
"    var j = await fetch(api+'/admin/enable',{method:'POST',headers:headers(),body:JSON.stringify({key:key})}).then(function(r){return r.json();});\n" +
"    if(!j.ok) showErr(j.error||'enable failed');\n" +
"    loadAll();\n" +
"  } catch(e){ showErr(''+e); }\n" +
"}\n" +
"async function sendMsg(broadcast){\n" +
"  try {\n" +
"    var body = {broadcast:broadcast, key:document.getElementById('mkey').value, title:document.getElementById('mtitle').value, body:document.getElementById('mbody').value};\n" +
"    var j = await fetch(api+'/admin/message',{method:'POST',headers:headers(),body:JSON.stringify(body)}).then(function(r){return r.json();});\n" +
"    alert(JSON.stringify(j));\n" +
"  } catch(e){ alert(''+e); }\n" +
"}\n" +
"</script>\n" +
"</body></html>\n";
