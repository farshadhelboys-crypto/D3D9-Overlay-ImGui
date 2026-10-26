/**
 * ============================================================
 *  Farshad ESP — Production License Server (Cloudflare Worker)
 * ============================================================
 *  Design goals:
 *   - One key → one PC (HWID bind on first activate)
 *   - Expiry from SERVER time only (not Windows clock)
 *   - Admin: create / enable / disable / reset HWID / extend
 *   - Optional Telegram channel membership (if env set)
 *   - Debug report from client → auto-disable key
 *   - English admin UI, no nested template bugs
 *
 *  Required Cloudflare setup:
 *   - KV binding variable name: KV
 *   - Secret/var: ADMIN_KEY
 *   - Optional: TELEGRAM_BOT_TOKEN, TELEGRAM_CHANNEL_ID
 * ============================================================
 */

const OFFSETS_URL =
  "https://raw.githubusercontent.com/farshadhelboys-crypto/D3D9-Overlay-ImGui/main/offsets/offsets.json";

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const path = url.pathname;
    const cors = {
      "Access-Control-Allow-Origin": "*",
      "Access-Control-Allow-Headers": "Content-Type, X-Admin-Key",
      "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
    };
    if (request.method === "OPTIONS") {
      return new Response(null, { headers: cors });
    }

    try {
      // -------- Public API --------
      if (path === "/api/time" && request.method === "GET") {
        return json({ ok: true, server_time: now() }, cors);
      }
      if (path === "/api/offsets" && request.method === "GET") {
        const r = await fetch(OFFSETS_URL, { cf: { cacheTtl: 60 } });
        return new Response(await r.text(), {
          headers: { ...cors, "Content-Type": "application/json" },
        });
      }
      if (path === "/api/activate" && request.method === "POST") {
        return json(await apiActivate(env, await request.json()), cors);
      }
      if (path === "/api/heartbeat" && request.method === "POST") {
        return json(await apiHeartbeat(env, await request.json()), cors);
      }
      if (path === "/api/report_debug" && request.method === "POST") {
        return json(await apiReportDebug(env, await request.json()), cors);
      }

      // -------- Admin API --------
      if (path.startsWith("/admin")) {
        const adminKey = request.headers.get("X-Admin-Key") || "";
        if (!env.ADMIN_KEY || adminKey !== env.ADMIN_KEY) {
          return json({ ok: false, error: "unauthorized" }, cors, 401);
        }
        if (!env.KV) {
          return json({ ok: false, error: "KV not bound. In Worker Settings > Bindings, add KV named exactly: KV" }, cors, 500);
        }

        if (path === "/admin/stats" && request.method === "GET")
          return json(await adminStats(env), cors);
        if (path === "/admin/list" && request.method === "GET")
          return json(await adminList(env), cors);
        if (path === "/admin/create" && request.method === "POST")
          return json(await adminCreate(env, await request.json()), cors);
        if (path === "/admin/enable" && request.method === "POST")
          return json(await adminEnable(env, await request.json()), cors);
        if (path === "/admin/disable" && request.method === "POST")
          return json(await adminDisable(env, await request.json()), cors);
        if (path === "/admin/reset_hwid" && request.method === "POST")
          return json(await adminResetHwid(env, await request.json()), cors);
        if (path === "/admin/extend" && request.method === "POST")
          return json(await adminExtend(env, await request.json()), cors);
        if (path === "/admin/delete" && request.method === "POST")
          return json(await adminDelete(env, await request.json()), cors);
        if (path === "/admin/message" && request.method === "POST")
          return json(await adminMessage(env, await request.json()), cors);

        return json({ ok: false, error: "unknown_admin_route" }, cors, 404);
      }

      // -------- Admin UI --------
      if (path === "/" || path === "/admin" || path === "/index.html") {
        return new Response(ADMIN_HTML, {
          headers: { ...cors, "Content-Type": "text/html; charset=utf-8" },
        });
      }

      return json({ ok: false, error: "not_found" }, cors, 404);
    } catch (e) {
      return json({ ok: false, error: String(e && e.message ? e.message : e) }, cors, 500);
    }
  },
};

// ===================== helpers =====================
function now() {
  return Math.floor(Date.now() / 1000);
}
function json(obj, cors, status) {
  return new Response(JSON.stringify(obj), {
    status: status || 200,
    headers: Object.assign({ "Content-Type": "application/json" }, cors || {}),
  });
}
function genKey() {
  const a = crypto.getRandomValues(new Uint8Array(8));
  return Array.from(a, function (b) {
    return b.toString(16).padStart(2, "0");
  })
    .join("")
    .toUpperCase()
    .replace(/(.{4})/g, "$1-")
    .slice(0, -1);
}
function normKey(k) {
  return String(k || "").trim().toUpperCase();
}
async function getLic(env, key) {
  const raw = await env.KV.get("lic:" + key);
  if (!raw) return null;
  try {
    return JSON.parse(raw);
  } catch (e) {
    return null;
  }
}
async function putLic(env, key, lic) {
  await env.KV.put("lic:" + key, JSON.stringify(lic));
}
async function getIndex(env) {
  const raw = await env.KV.get("idx:keys");
  if (!raw) return [];
  try {
    const arr = JSON.parse(raw);
    return Array.isArray(arr) ? arr : [];
  } catch (e) {
    return [];
  }
}
async function saveIndex(env, idx) {
  await env.KV.put("idx:keys", JSON.stringify(idx));
}
function statusOf(lic, t) {
  if (lic.disabled === true) return "disabled";
  if (lic.expires_at && t > Number(lic.expires_at)) return "expired";
  if (!lic.hwid) return "unused"; // created, not activated yet
  return "active";
}
function daysLeft(lic, t) {
  if (!lic.expires_at) return null;
  return Math.max(0, Math.floor((Number(lic.expires_at) - t) / 86400));
}

// ===================== public API =====================
async function checkTelegram(env, tgId) {
  if (!env.TELEGRAM_BOT_TOKEN || !env.TELEGRAM_CHANNEL_ID) {
    return { required: false, ok: true };
  }
  if (!tgId) {
    return {
      required: true,
      ok: false,
      message: "Enter your numeric Telegram user ID (join the channel first)",
    };
  }
  const u =
    "https://api.telegram.org/bot" +
    env.TELEGRAM_BOT_TOKEN +
    "/getChatMember?chat_id=" +
    encodeURIComponent(env.TELEGRAM_CHANNEL_ID) +
    "&user_id=" +
    encodeURIComponent(tgId);
  const r = await fetch(u);
  const j = await r.json();
  if (!j.ok) {
    return { required: true, ok: false, message: "Telegram check failed" };
  }
  const st = j.result && j.result.status;
  const ok = st === "member" || st === "administrator" || st === "creator";
  return {
    required: true,
    ok: ok,
    message: ok ? "ok" : "Please join the Telegram channel",
  };
}

async function apiActivate(env, body) {
  if (!env.KV) return { ok: false, error: "server_misconfigured", message: "KV missing" };

  const key = normKey(body.key);
  const hwid = String(body.hwid || "").trim();
  const tgId = body.tg_id ? String(body.tg_id).trim() : "";
  if (!key || !hwid) {
    return { ok: false, error: "missing_fields", message: "Key and HWID required" };
  }

  const lic = await getLic(env, key);
  if (!lic) {
    return { ok: false, error: "invalid_key", message: "Invalid license key" };
  }

  const t = now();
  const st = statusOf(lic, t);

  if (st === "disabled") {
    return {
      ok: false,
      error: "disabled",
      message: "License is disabled (" + (lic.disable_reason || "admin") + ")",
    };
  }
  if (st === "expired") {
    return { ok: false, error: "expired", message: "License expired" };
  }

  const tg = await checkTelegram(env, tgId);
  if (tg.required && !tg.ok) {
    return { ok: false, error: "join_channel", message: tg.message };
  }

  // HWID lock
  if (lic.hwid && lic.hwid !== hwid) {
    return {
      ok: false,
      error: "hwid_mismatch",
      message: "This license is locked to another PC. Ask admin to Reset HWID.",
    };
  }
  if (!lic.hwid) {
    lic.hwid = hwid;
    lic.activated_at = t;
  }
  if (tgId) lic.tg_id = tgId;
  lic.disabled = false;
  lic.last_seen = t;
  await putLic(env, key, lic);

  return {
    ok: true,
    server_time: t,
    expires_at: lic.expires_at,
    days_left: daysLeft(lic, t),
    status: "active",
    messages: lic.pending_messages || [],
  };
}

async function apiHeartbeat(env, body) {
  if (!env.KV) return { ok: false, error: "server_misconfigured" };
  const key = normKey(body.key);
  const hwid = String(body.hwid || "").trim();
  const lic = await getLic(env, key);
  if (!lic) return { ok: false, error: "invalid_key" };

  const t = now();
  const st = statusOf(lic, t);
  if (st === "disabled") {
    return { ok: false, error: "disabled", message: "License disabled" };
  }
  if (st === "expired") {
    return { ok: false, error: "expired", message: "License expired" };
  }
  if (lic.hwid && hwid && lic.hwid !== hwid) {
    return { ok: false, error: "hwid_mismatch" };
  }

  lic.last_seen = t;
  const msgs = lic.pending_messages || [];
  lic.pending_messages = [];
  await putLic(env, key, lic);

  return {
    ok: true,
    server_time: t,
    expires_at: lic.expires_at,
    days_left: daysLeft(lic, t),
    status: st === "unused" ? "active" : st,
    messages: msgs,
  };
}

async function apiReportDebug(env, body) {
  const key = normKey(body.key);
  const lic = await getLic(env, key);
  if (!lic) return { ok: false };
  lic.disabled = true;
  lic.disable_reason = "debug_detected";
  lic.disabled_at = now();
  await putLic(env, key, lic);
  return { ok: true, disabled: true };
}

// ===================== admin =====================
async function adminCreate(env, body) {
  const days = Math.max(1, Number(body.days) || 30);
  const note = String(body.note || "");
  const count = Math.min(50, Math.max(1, Number(body.count) || 1));
  const t = now();
  const created = [];
  const idx = await getIndex(env);

  for (let i = 0; i < count; i++) {
    const key = normKey(body.key && count === 1 ? body.key : genKey());
    const lic = {
      key: key,
      created_at: t,
      expires_at: t + days * 86400,
      hwid: null,
      tg_id: null,
      disabled: false,
      disable_reason: null,
      note: note,
      last_seen: null,
      activated_at: null,
      pending_messages: [],
    };
    await putLic(env, key, lic);
    if (idx.indexOf(key) === -1) idx.push(key);
    created.push({
      key: key,
      status: "unused",
      days_left: days,
      expires_at: lic.expires_at,
    });
  }
  await saveIndex(env, idx);

  return {
    ok: true,
    count: created.length,
    licenses: created,
    // single-create convenience (client panel expects license.key)
    license: created[0]
      ? {
          key: created[0].key,
          created_at: t,
          expires_at: created[0].expires_at,
          disabled: false,
          hwid: null,
        }
      : null,
  };
}

async function adminEnable(env, body) {
  const key = normKey(body.key);
  const lic = await getLic(env, key);
  if (!lic) return { ok: false, error: "not_found" };
  lic.disabled = false;
  lic.disable_reason = null;
  if (body.days) {
    const days = Math.max(1, Number(body.days) || 30);
    lic.expires_at = now() + days * 86400;
  }
  await putLic(env, key, lic);
  return { ok: true, status: statusOf(lic, now()), license: lic };
}

async function adminDisable(env, body) {
  const key = normKey(body.key);
  const lic = await getLic(env, key);
  if (!lic) return { ok: false, error: "not_found" };
  lic.disabled = true;
  lic.disable_reason = body.reason || "admin_disabled";
  lic.disabled_at = now();
  await putLic(env, key, lic);
  return { ok: true };
}

async function adminResetHwid(env, body) {
  const key = normKey(body.key);
  const lic = await getLic(env, key);
  if (!lic) return { ok: false, error: "not_found" };
  lic.hwid = null;
  lic.activated_at = null;
  await putLic(env, key, lic);
  return { ok: true, message: "HWID cleared — next activate will bind a new PC" };
}

async function adminExtend(env, body) {
  const key = normKey(body.key);
  const days = Math.max(1, Number(body.days) || 30);
  const lic = await getLic(env, key);
  if (!lic) return { ok: false, error: "not_found" };
  const t = now();
  const base = lic.expires_at && Number(lic.expires_at) > t ? Number(lic.expires_at) : t;
  lic.expires_at = base + days * 86400;
  if (lic.disabled === true && body.enable) {
    lic.disabled = false;
    lic.disable_reason = null;
  }
  await putLic(env, key, lic);
  return { ok: true, expires_at: lic.expires_at, days_left: daysLeft(lic, t) };
}

async function adminDelete(env, body) {
  const key = normKey(body.key);
  await env.KV.delete("lic:" + key);
  const idx = await getIndex(env);
  const next = idx.filter(function (k) {
    return k !== key;
  });
  await saveIndex(env, next);
  return { ok: true };
}

async function adminMessage(env, body) {
  const title = body.title || "Message";
  const text = body.body || body.text || "";
  const msg = { id: Date.now(), title: title, body: text, ts: now() };
  const idx = await getIndex(env);

  if (body.broadcast) {
    let n = 0;
    for (let i = 0; i < idx.length; i++) {
      const lic = await getLic(env, idx[i]);
      if (!lic || lic.disabled === true) continue;
      lic.pending_messages = lic.pending_messages || [];
      lic.pending_messages.push(msg);
      await putLic(env, idx[i], lic);
      n++;
    }
    return { ok: true, sent: n };
  }

  const key = normKey(body.key);
  const lic = await getLic(env, key);
  if (!lic) return { ok: false, error: "not_found" };
  lic.pending_messages = lic.pending_messages || [];
  lic.pending_messages.push(msg);
  await putLic(env, key, lic);
  return { ok: true, sent: 1 };
}

async function adminList(env) {
  const idx = await getIndex(env);
  const t = now();
  const list = [];
  for (let i = 0; i < idx.length; i++) {
    const lic = await getLic(env, idx[i]);
    if (!lic) continue;
    const st = statusOf(lic, t);
    list.push({
      key: lic.key,
      status: st,
      days_left: daysLeft(lic, t),
      expires_at: lic.expires_at,
      hwid: lic.hwid || null,
      tg_id: lic.tg_id || null,
      note: lic.note || "",
      disable_reason: lic.disable_reason || "",
      last_seen: lic.last_seen,
      created_at: lic.created_at,
      disabled: lic.disabled === true,
      active: st === "active" || st === "unused",
    });
  }
  // newest first
  list.sort(function (a, b) {
    return (b.created_at || 0) - (a.created_at || 0);
  });
  return { ok: true, licenses: list, server_time: t };
}

async function adminStats(env) {
  const data = await adminList(env);
  let active = 0,
    unused = 0,
    disabled = 0,
    expired = 0;
  for (let i = 0; i < data.licenses.length; i++) {
    const s = data.licenses[i].status;
    if (s === "active") active++;
    else if (s === "unused") unused++;
    else if (s === "disabled") disabled++;
    else if (s === "expired") expired++;
  }
  return {
    ok: true,
    total: data.licenses.length,
    active: active,
    unused: unused,
    disabled: disabled,
    expired: expired,
    server_time: data.server_time,
  };
}

// ===================== Admin HTML (English, no nested backticks) =====================
const ADMIN_HTML =
  "<!DOCTYPE html>\n" +
  "<html lang=\"en\">\n<head>\n" +
  "<meta charset=\"utf-8\"/>\n" +
  "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>\n" +
  "<title>Farshad ESP License Admin</title>\n" +
  "<style>\n" +
  "*{box-sizing:border-box}body{font-family:Segoe UI,Tahoma,sans-serif;background:#0d0d0d;color:#eee;margin:0;padding:20px}\n" +
  "h1{color:#5cf;margin:0 0 12px}h3{margin:0 0 8px;color:#aaa}\n" +
  ".card{background:#161616;border:1px solid #2a2a2a;border-radius:12px;padding:14px;margin:12px 0}\n" +
  "input,button,textarea,select{font:inherit;padding:8px 10px;margin:4px 4px 4px 0;border-radius:8px;border:1px solid #333;background:#111;color:#eee}\n" +
  "button{background:#09f;color:#000;border:none;font-weight:700;cursor:pointer}\n" +
  "button:hover{filter:brightness(1.1)}\n" +
  "button.danger{background:#e33;color:#fff}\n" +
  "button.ok{background:#2a5;color:#fff}\n" +
  "button.muted{background:#444;color:#ddd}\n" +
  "table{width:100%;border-collapse:collapse;font-size:13px;margin-top:8px}\n" +
  "th,td{border:1px solid #333;padding:8px;text-align:left;vertical-align:middle}\n" +
  "th{background:#1c1c1c;color:#9cf}\n" +
  ".tag{display:inline-block;padding:2px 8px;border-radius:999px;font-size:12px;font-weight:700}\n" +
  ".tag.active{background:#143;color:#6f6}\n" +
  ".tag.unused{background:#224;color:#8bf}\n" +
  ".tag.disabled{background:#422;color:#f88}\n" +
  ".tag.expired{background:#332;color:#fc6}\n" +
  "#err{color:#f66;min-height:1.2em;margin-top:6px}\n" +
  "#created{background:#0a0a0a;padding:10px;border-radius:8px;white-space:pre-wrap}\n" +
  "</style></head><body>\n" +
  "<h1>Farshad ESP — License Admin</h1>\n" +
  "<div class=\"card\">\n" +
  "  <label>Admin Key</label><br/>\n" +
  "  <input id=\"akey\" type=\"password\" style=\"width:300px\" placeholder=\"ADMIN_KEY from Cloudflare\"/>\n" +
  "  <button type=\"button\" onclick=\"loadAll()\">Login / Refresh</button>\n" +
  "  <div id=\"err\"></div>\n" +
  "</div>\n" +
  "<div class=\"card\" id=\"stats\">Login to see stats</div>\n" +
  "<div class=\"card\">\n" +
  "  <h3>Create license</h3>\n" +
  "  Days <input id=\"days\" type=\"number\" value=\"30\" style=\"width:90px\"/>\n" +
  "  Count <input id=\"count\" type=\"number\" value=\"1\" min=\"1\" max=\"50\" style=\"width:70px\"/>\n" +
  "  Note <input id=\"note\" style=\"width:220px\" placeholder=\"optional\"/>\n" +
  "  <button type=\"button\" onclick=\"createLic()\">Create</button>\n" +
  "  <div id=\"created\"></div>\n" +
  "</div>\n" +
  "<div class=\"card\">\n" +
  "  <h3>Message users</h3>\n" +
  "  Key (empty=all) <input id=\"mkey\" style=\"width:220px\"/>\n" +
  "  Title <input id=\"mtitle\" value=\"Admin message\" style=\"width:180px\"/>\n" +
  "  <textarea id=\"mbody\" rows=\"2\" style=\"width:100%\" placeholder=\"Message text\"></textarea>\n" +
  "  <button type=\"button\" onclick=\"sendMsg(false)\">Send to key</button>\n" +
  "  <button type=\"button\" class=\"muted\" onclick=\"sendMsg(true)\">Broadcast</button>\n" +
  "</div>\n" +
  "<div class=\"card\">\n" +
  "  <h3>All licenses</h3>\n" +
  "  <div id=\"list\"></div>\n" +
  "</div>\n" +
  "<script>\n" +
  "var api=location.origin;\n" +
  "function $(id){return document.getElementById(id);}\n" +
  "function headers(){return {'Content-Type':'application/json','X-Admin-Key':$('akey').value};}\n" +
  "function showErr(m){$('err').textContent=m||'';}\n" +
  "function tag(st){return '<span class=\"tag '+st+'\">'+st+'</span>';}\n" +
  "async function loadAll(){\n" +
  "  showErr('');\n" +
  "  try{\n" +
  "    localStorage.setItem('fe_admin',$('akey').value);\n" +
  "    var s=await fetch(api+'/admin/stats',{headers:headers()}).then(function(r){return r.json();});\n" +
  "    if(!s.ok){showErr('Error: '+(s.error||JSON.stringify(s)));return;}\n" +
  "    $('stats').innerHTML='Total: <b>'+s.total+'</b> &nbsp; Active: <b style=\"color:#6f6\">'+s.active+'</b> &nbsp; Unused: <b style=\"color:#8bf\">'+s.unused+'</b> &nbsp; Disabled: <b style=\"color:#f88\">'+s.disabled+'</b> &nbsp; Expired: <b style=\"color:#fc6\">'+s.expired+'</b>';\n" +
  "    var l=await fetch(api+'/admin/list',{headers:headers()}).then(function(r){return r.json();});\n" +
  "    if(!l.ok){$('list').textContent=JSON.stringify(l);return;}\n" +
  "    var h='<table><tr><th>Key</th><th>Status</th><th>Days</th><th>HWID</th><th>Note / Reason</th><th>Actions</th></tr>';\n" +
  "    for(var i=0;i<l.licenses.length;i++){\n" +
  "      var x=l.licenses[i];\n" +
  "      var hw=x.hwid||'-'; if(hw.length>16) hw=hw.slice(0,16)+'…';\n" +
  "      var reason=(x.disable_reason||x.note||'');\n" +
  "      h+='<tr><td><code>'+x.key+'</code> <button type=\"button\" class=\"muted\" onclick=\"copy(\\''+x.key+'\\')\">Copy</button></td>';\n" +
  "      h+='<td>'+tag(x.status)+'</td><td>'+(x.days_left!=null?x.days_left:'-')+'</td><td>'+hw+'</td><td>'+reason+'</td><td>';\n" +
  "      if(x.status==='disabled') h+='<button type=\"button\" class=\"ok\" onclick=\"en(\\''+x.key+'\\')\">Enable</button> ';\n" +
  "      else h+='<button type=\"button\" class=\"danger\" onclick=\"dis(\\''+x.key+'\\')\">Disable</button> ';\n" +
  "      h+='<button type=\"button\" class=\"muted\" onclick=\"rst(\\''+x.key+'\\')\">Reset HWID</button> ';\n" +
  "      h+='<button type=\"button\" class=\"muted\" onclick=\"ext(\\''+x.key+'\\')\">+30d</button> ';\n" +
  "      h+='<button type=\"button\" class=\"danger\" onclick=\"del(\\''+x.key+'\\')\">Delete</button>';\n" +
  "      h+='</td></tr>';\n" +
  "    }\n" +
  "    h+='</table>';\n" +
  "    $('list').innerHTML=h;\n" +
  "  }catch(e){showErr('Network: '+e);}\n" +
  "}\n" +
  "async function createLic(){\n" +
  "  showErr('');\n" +
  "  try{\n" +
  "    var body={days:+$('days').value,count:+$('count').value,note:$('note').value};\n" +
  "    var j=await fetch(api+'/admin/create',{method:'POST',headers:headers(),body:JSON.stringify(body)}).then(function(r){return r.json();});\n" +
  "    if(j.ok){\n" +
  "      var lines=[];\n" +
  "      for(var i=0;i<j.licenses.length;i++){\n" +
  "        var L=j.licenses[i];\n" +
  "        lines.push(L.key+'  ('+L.status+', '+L.days_left+' days)');\n" +
  "      }\n" +
  "      $('created').textContent=lines.join('\\n');\n" +
  "      loadAll();\n" +
  "    } else { $('created').textContent=JSON.stringify(j); showErr(j.error||'create failed'); }\n" +
  "  }catch(e){showErr('Network: '+e);}\n" +
  "}\n" +
  "async function dis(key){ if(!confirm('Disable '+key+'?'))return; await fetch(api+'/admin/disable',{method:'POST',headers:headers(),body:JSON.stringify({key:key,reason:'admin_disabled'})}); loadAll(); }\n" +
  "async function en(key){ await fetch(api+'/admin/enable',{method:'POST',headers:headers(),body:JSON.stringify({key:key})}); loadAll(); }\n" +
  "async function rst(key){ if(!confirm('Reset HWID for '+key+'?'))return; await fetch(api+'/admin/reset_hwid',{method:'POST',headers:headers(),body:JSON.stringify({key:key})}); loadAll(); }\n" +
  "async function ext(key){ await fetch(api+'/admin/extend',{method:'POST',headers:headers(),body:JSON.stringify({key:key,days:30})}); loadAll(); }\n" +
  "async function del(key){ if(!confirm('DELETE '+key+' forever?'))return; await fetch(api+'/admin/delete',{method:'POST',headers:headers(),body:JSON.stringify({key:key})}); loadAll(); }\n" +
  "async function sendMsg(broadcast){\n" +
  "  var body={broadcast:broadcast,key:$('mkey').value,title:$('mtitle').value,body:$('mbody').value};\n" +
  "  var j=await fetch(api+'/admin/message',{method:'POST',headers:headers(),body:JSON.stringify(body)}).then(function(r){return r.json();});\n" +
  "  alert(JSON.stringify(j));\n" +
  "}\n" +
  "function copy(t){ navigator.clipboard.writeText(t); }\n" +
  "(function(){ var k=localStorage.getItem('fe_admin'); if(k) $('akey').value=k; })();\n" +
  "</script>\n</body></html>\n";
