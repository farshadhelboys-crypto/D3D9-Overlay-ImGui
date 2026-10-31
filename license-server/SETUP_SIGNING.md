# Signed license + trial setup

## 1) Keys
```bash
pip install cryptography
python gen_keys.py
```
- Put **PKCS8 base64** into Cloudflare Secret `SIGN_PRIVATE_KEY`
- Embed **public** key in the C++ client

## 2) Cloudflare vars
| Name | Type | Purpose |
|------|------|--------|
| KV | Binding | must be named `KV` |
| ADMIN_KEY | Secret | admin panel |
| SIGN_PRIVATE_KEY | Secret | Ed25519 PKCS8 |
| GITHUB_TOKEN | Secret | private offsets repo |
| OFFSETS_PUBLIC | Plain `1` | temp legacy GET offsets |
| TRIAL_MAX_PER_DAY | Plain | default 60 |

## 3) Deploy
```bash
npx wrangler deploy
```

## 4) Trial
Admin UI → Trial key + hours → Save.

## 5) Client rules
- Send `nonce` on activate/offsets
- Verify `sig` over exact `payload` string with public key
- Use only fields inside payload
- Offsets: `POST /api/offsets` only
