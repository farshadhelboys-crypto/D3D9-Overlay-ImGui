# راهنمای سیستم لایسنس Farshad ESP

## خلاصه معماری

```
[ برنامه ESP ]  --HTTPS-->  [ Cloudflare Worker + KV ]
                                  |
                                  +-- چک HWID (یک سیستم)
                                  +-- زمان سرور (نه ساعت ویندوز)
                                  +-- عضویت کانال تلگرام (ربات)
                                  +-- پنل ادمین وب
```

---

## مرحله ۱ — ساخت KV در Cloudflare

1. برو به [dash.cloudflare.com](https://dash.cloudflare.com)
2. Workers & Pages → KV → **Create a namespace**
3. اسم مثلاً: `farshad-esp-lic`
4. **Namespace ID** را کپی کن

---

## مرحله ۲ — دیپلوی ورکر

1. روی سیستم خودت:
```bash
npm install -g wrangler
wrangler login
```

2. پوشه ریپو را باز کن و برو داخل `license-server`

3. فایل `wrangler.toml` را ویرایش کن:
```toml
name = "farshad-esp-license"
main = "worker.js"
compatibility_date = "2024-10-01"

[[kv_namespaces]]
binding = "KV"
id = "همان_ID_که_کپی_کردی"

[vars]
ADMIN_KEY = "یک_رمز_قوی_برای_پنل"
TELEGRAM_BOT_TOKEN = "123456:ABC-token از BotFather"
TELEGRAM_CHANNEL_ID = "@channel_username یا -100id"
```

4. دیپلوی:
```bash
wrangler deploy
```

5. آدرس ورکر چیزی شبیه این می‌شود:
`https://farshad-esp-license.XXXX.workers.dev`

---

## مرحله ۳ — ربات تلگرام (اختیاری ولی توصیه‌شده)

1. به `@BotFather` بگو `/newbot` و توکن بگیر
2. ربات را **ادمین کانال** کن
3. توکن و آیدی کانال را در `wrangler.toml` بگذار و دوباره `wrangler deploy`

اگر توکن خالی باشد، چک عضویت کانال رد می‌شود (فقط لایسنس+HWID).

---

## مرحله ۴ — اتصال برنامه به سرور

در فایل:
`Source/D3D9 Overlay ImGui/license.h`

این خط را عوض کن:
```cpp
constexpr const char* API_BASE = "https://YOUR_WORKER.workers.dev";
```
به آدرس واقعی ورکرت.

سپس پروژه را **Rebuild** کن.

---

## مرحله ۵ — پنل ادمین

1. مرورگر: `https://YOUR_WORKER.workers.dev/`
2. **Admin Key** همان `ADMIN_KEY` در wrangler
3. می‌توانی:
   - لایسنس بسازی (تعداد روز)
   - لیست فعال / غیرفعال ببینی
   - لایسنس را غیرفعال کنی
   - پیام تکی یا همگانی بفرستی

---

## رفتار کلاینت

| حالت | نتیجه |
|------|--------|
| کلید درست + HWID آزاد | فعال می‌شود و به همان سیستم قفل می‌شود |
| همان کلید روی PC دیگر | `hwid_mismatch` |
| منقضی | `expired` |
| غیرفعال / دیباگ | `disabled` |
| عضو کانال نیست | پیام «لطفا در کانال عضو شوید» |
| دیباگر / CE وصل شود | پیام «داداش دیباگ نکن ممنونم» + غیرفعال شدن لایسنس روی سرور |

زمان انقضا فقط با **ساعت سرور Cloudflare** سنجیده می‌شود.

---

## API خلاصه

- `POST /api/activate` `{ key, hwid, tg_id? }`
- `POST /api/heartbeat` `{ key, hwid }`
- `POST /api/report_debug` `{ key, hwid }`
- `GET  /api/time`
- `GET  /api/offsets`
- Admin با هدر `X-Admin-Key`:
  - `GET /admin/list` `GET /admin/stats`
  - `POST /admin/create` `{ days, note? }`
  - `POST /admin/disable` `{ key, reason? }`
  - `POST /admin/message` `{ key?, broadcast?, title, body }`

---

## نکات امنیتی

- `ADMIN_KEY` را قوی بگذار و جای عمومی نگذار
- آفست‌ها در `offsets/offsets.json` روی گیت‌هاب؛ ورکر `/api/offsets` همان را پروکسی می‌کند
- آنتی‌دیباگ کامل نیست (هیچ محافظتی ۱۰۰٪ نیست) ولی لایه اول مناسبی است
- برای سخت‌گیری بیشتر بعداً می‌توان امضای پاسخ سرور و obfuscation اضافه کرد

---

## عیب‌یابی

| مشکل | کار |
|------|-----|
| Activate هیچ جوابی نمی‌دهد | URL در `license.h` و فایروال/آنتی‌ویروس |
| unauthorized در پنل | `ADMIN_KEY` اشتباه |
| join_channel همیشه | ربات ادمین کانال نیست / `tg_id` عددی کاربر |
| hwid_mismatch اشتباه | یک‌بار از پنل لایسنس جدید بساز یا KV را دستی پاک کن |
