/**
 * Farshad ESP — Cloudflare Worker + KV License API
 * Fixed admin panel (no nested backticks)
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

        if (path === "/admin/list" && request.method === "GET") {
          return json(await listLicenses(env), cors);
        }
        if (path === "/admin/stats" && request.method === "GET") {
          return json(await stats(env), cors);
        }
        if (path === "/admin/create" && request.method === "POST") {
          const body = await request.json();
          return json(await createLicense(env, body), cors);
        }
        if (path === "/admin/disable" && request.method === "POST") {
          const body = await request.json();
          return json(await disableLicense(env, body), cors);
        }
        if (path === "/admin/message" && request.method === "POST") {
          const body = await request.json();
          return json(await sendMessage(env, body), cors);
        }
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
  const url =
    "https://api.telegram.org/bot" +
    env.TELEGRAM_BOT_TOKEN +
    "/getChatMember?chat_id=" +
    encodeURIComponent(env.TELEGRAM_CHANNEL_ID) +
    "&user_id=" +
    encodeURIComponent(tgId);
  const r = await fetch(url);
  const j = await r.json();
  if (!j.ok) return { ok: false, error: "telegram_api" };
  const st = j.result && j.result.status;
  const ok = st === "member" || st === "administrator" || st === "creator";
  return { ok: ok, status: st };
}

async function activate(env, body) {
  const key = (body.key || "").trim().toUpperCase();
  const hwid = (body.hwid || "").trim();
  const tgId = body.tg_id ? String(body.tg_id).trim() : "";
  if (!key || !hwid) return { ok: false, error: "missing_fields" };

  const lic = await getLic(env, key);
  if (!lic) return { ok: false, error: "invalid_key" };
  if (lic.disabled) return { ok: false, error: "disabled", reason: lic.disable_reason || "disabled" };

  const now = Math.floor(Date.now() / 1000);
  if (lic.expires_at && now > lic.expires_at) return { ok: false, error: "expired" };

  if (env.TELEGRAM_CHANNEL_ID && env.TELEGRAM_BOT_TOKEN) {
    if (!tgId) return { ok: false, error: "join_channel", message: "لطفا در کانال عضو شوید" };
    const mem = await telegramMember(env, tgId);
    if (!mem.ok) return { ok: false, error: "join_channel", message: "لطفا در کانال عضو شوید" };
  }

  if (lic.hwid && lic.hwid !== hwid) {
    return { ok: false, error: "hwid_mismatch", message: "این لایسنس روی سیستم دیگری فعال است" };
  }
  if (!lic.hwid) {
    lic.hwid = hwid;
    lic.activated_at = now;
  }
  if (tgId) lic.tg_id = tgId;
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
  const key = (body.key || "").trim().toUpperCase();
  const hwid = (body.hwid || "").trim();
  const lic = await getLic(env, key);
  if (!lic) return { ok: false, error: "invalid_key" };
  if (lic.disabled) return { ok: false, error: "disabled", reason: lic.disable_reason || "" };
  const now = Math.floor(Date.now() / 1000);
  if (lic.expires_at && now > lic.expires_at) return { ok: false, error: "expired" };
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
  const key = (body.key || "").trim().toUpperCase();
  const lic = await getLic(env, key);
  if (!lic) return { ok: false };
  lic.disabled = true;
  lic.disable_reason = "debug_detected";
  lic.disabled_at = Math.floor(Date.now() / 1000);
  await putLic(env, key, lic);
  return { ok: true, disabled: true };
}

async function createLicense(env, body) {
  if (!env.KV) return { ok: false, error: "KV not bound" };
  const days = Number(body.days) || 30;
  const note = body.note || "";
  const key = (body.key || genKey()).toUpperCase();
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
  const idx = idxRaw ? JSON.parse(idxRaw) : [];
  if (idx.indexOf(key) === -1) idx.push(key);
  await env.KV.put("idx:keys", JSON.stringify(idx));
  return { ok: true, license: lic };
}

async function disableLicense(env, body) {
  const key = (body.key || "").trim().toUpperCase();
  const lic = await getLic(env, key);
  if (!lic) return { ok: false, error: "not_found" };
  lic.disabled = true;
  lic.disable_reason = body.reason || "admin_disabled";
  lic.disabled_at = Math.floor(Date.now() / 1000);
  await putLic(env, key, lic);
  return { ok: true };
}

async function sendMessage(env, body) {
  const title = body.title || "پیام";
  const text = body.body || body.text || "";
  const msg = { id: Date.now(), title: title, body: text, ts: Math.floor(Date.now() / 1000) };
  const idxRaw = await env.KV.get("idx:keys");
  const idx = idxRaw ? JSON.parse(idxRaw) : [];

  if (body.broadcast) {
    for (let i = 0; i < idx.length; i++) {
      const k = idx[i];
      const lic = await getLic(env, k);
      if (!lic || lic.disabled) continue;
      lic.pending_messages = lic.pending_messages || [];
      lic.pending_messages.push(msg);
      await putLic(env, k, lic);
    }
    return { ok: true, sent: "broadcast" };
  }

  const key = (body.key || "").trim().toUpperCase();
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
  const idx = idxRaw ? JSON.parse(idxRaw) : [];
  const now = Math.floor(Date.now() / 1000);
  const list = [];
  for (let i = 0; i < idx.length; i++) {
    const lic = await getLic(env, idx[i]);
    if (!lic) continue;
    list.push({
      key: lic.key,
      created_at: lic.created_at,
      expires_at: lic.expires_at,
      hwid: lic.hwid,
      tg_id: lic.tg_id,
      disabled: !!lic.disabled,
      disable_reason: lic.disable_reason,
      note: lic.note,
      last_seen: lic.last_seen,
      days_left: lic.expires_at ? Math.max(0, Math.floor((lic.expires_at - now) / 86400)) : null,
      active: !lic.disabled && (!lic.expires_at || lic.expires_at > now),
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
"<html lang=\"fa\" dir=\"rtl\">\n" +
"<head>\n" +
"<meta charset=\"utf-8\"/>\n" +
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>\n" +
"<title>Farshad ESP Admin</title>\n" +
"<style>\n" +
"body{font-family:Tahoma,sans-serif;background:#111;color:#eee;margin:0;padding:16px}\n" +
"input,button,textarea{font:inherit;padding:8px;margin:4px 0;border-radius:6px;border:1px solid #333;background:#1a1a1a;color:#eee}\n" +
"button{background:#0af;color:#000;cursor:pointer;border:none;font-weight:bold}\n" +
"button.danger{background:#e33;color:#fff}\n" +
".card{background:#1a1a1a;padding:12px;border-radius:10px;margin:10px 0}\n" +
"table{width:100%;border-collapse:collapse;font-size:13px}\n" +
"td,th{border:1px solid #333;padding:6px;text-align:right}\n" +
".ok{color:#0f0}.bad{color:#f55}\n" +
"#err{color:#f55;min-height:1.2em}\n" +
"</style>\n" +
"</head>\n" +
"<body>\n" +
"<h1>Farshad ESP — پنل ادمین</h1>\n" +
"<div class=\"card\">\n" +
"  <label>Admin Key</label><br/>\n" +
"  <input id=\"akey\" type=\"password\" style=\"width:280px\" placeholder=\"ADMIN_KEY\"/>\n" +
"  <button type=\"button\" onclick=\"loadAll()\">ورود / رفرش</button>\n" +
"  <div id=\"err\"></div>\n" +
"</div>\n" +
"<div class=\"card\" id=\"stats\">—</div>\n" +
"<div class=\"card\">\n" +
"  <h3>ساخت لایسنس</h3>\n" +
"  روز: <input id=\"days\" type=\"number\" value=\"30\" style=\"width:80px\"/>\n" +
"  یادداشت: <input id=\"note\" style=\"width:200px\"/>\n" +
"  <button type=\"button\" onclick=\"createLic()\">بساز</button>\n" +
"  <pre id=\"created\"></pre>\n" +
"</div>\n" +
"<div class=\"card\">\n" +
"  <h3>ارسال پیام</h3>\n" +
"  کلید (خالی=همه): <input id=\"mkey\" style=\"width:200px\"/>\n" +
"  عنوان: <input id=\"mtitle\" value=\"پیام مدیریت\"/>\n" +
"  متن: <textarea id=\"mbody\" rows=\"3\" style=\"width:100%\"></textarea>\n" +
"  <button type=\"button\" onclick=\"sendMsg(false)\">ارسال به کلید</button>\n" +
"  <button type=\"button\" onclick=\"sendMsg(true)\">ارسال به همه</button>\n" +
"</div>\n" +
"<div class=\"card\">\n" +
"  <h3>لایسنس‌ها</h3>\n" +
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
"    if(!s.ok){ showErr('خطا: '+(s.error||JSON.stringify(s))); document.getElementById('stats').textContent='—'; return; }\n" +
"    document.getElementById('stats').innerHTML = 'کل: '+s.total+' | فعال: <span class=\"ok\">'+s.active+'</span> | غیرفعال: <span class=\"bad\">'+s.disabled+'</span> | منقضی: '+s.expired;\n" +
"    var l = await fetch(api+'/admin/list',{headers:headers()}).then(function(r){return r.json();});\n" +
"    if(!l.ok){ document.getElementById('list').textContent=JSON.stringify(l); return; }\n" +
"    var h = '<table><tr><th>کلید</th><th>وضعیت</th><th>روز مانده</th><th>HWID</th><th>دلیل</th><th></th></tr>';\n" +
"    for(var i=0;i<l.licenses.length;i++){\n" +
"      var x=l.licenses[i];\n" +
"      var st=x.disabled?'غیرفعال':(x.active?'فعال':'منقضی');\n" +
"      var hw=(x.hwid||'-'); if(hw.length>12) hw=hw.slice(0,12);\n" +
"      h += '<tr><td>'+x.key+'</td><td>'+st+'</td><td>'+(x.days_left!=null?x.days_left:'-')+'</td><td>'+hw+'</td><td>'+(x.disable_reason||x.note||'')+'</td>';\n" +
"      h += '<td><button type=\"button\" class=\"danger\" onclick=\"dis(\\''+x.key+'\\')\">غیرفعال</button></td></tr>';\n" +
"    }\n" +
"    h += '</table>';\n" +
"    document.getElementById('list').innerHTML = h;\n" +
"  } catch(e){ showErr('شبکه/خطا: '+e); }\n" +
"}\n" +
"async function createLic(){\n" +
"  showErr('');\n" +
"  try {\n" +
"    var days = +document.getElementById('days').value;\n" +
"    var note = document.getElementById('note').value;\n" +
"    var j = await fetch(api+'/admin/create',{method:'POST',headers:headers(),body:JSON.stringify({days:days,note:note})}).then(function(r){return r.json();});\n" +
"    if(j.ok){\n" +
"      document.getElementById('created').textContent = 'KEY: '+j.license.key+'\\nexpires: '+new Date(j.license.expires_at*1000).toLocaleString();\n" +
"      loadAll();\n" +
"    } else {\n" +
"      document.getElementById('created').textContent = JSON.stringify(j);\n" +
"      showErr(j.error||'create failed');\n" +
"    }\n" +
"  } catch(e){ showErr('شبکه/خطا: '+e); }\n" +
"}\n" +
"async function dis(key){\n" +
"  if(!confirm('غیرفعال؟')) return;\n" +
"  try {\n" +
"    await fetch(api+'/admin/disable',{method:'POST',headers:headers(),body:JSON.stringify({key:key,reason:'admin_disabled'})});\n" +
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
