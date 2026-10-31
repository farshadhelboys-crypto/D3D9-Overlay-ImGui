"""Generate Ed25519 keys and auto-save to files (easy copy for Cloudflare + C++)."""
import base64
import sys
from pathlib import Path

try:
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    from cryptography.hazmat.primitives import serialization
except ImportError:
    print("pip install cryptography")
    sys.exit(1)

out_dir = Path(__file__).resolve().parent
priv = Ed25519PrivateKey.generate()
pub = priv.public_key()

pub_raw = pub.public_bytes(
    encoding=serialization.Encoding.Raw,
    format=serialization.PublicFormat.Raw,
)
priv_pkcs8 = priv.private_bytes(
    encoding=serialization.Encoding.DER,
    format=serialization.PrivateFormat.PKCS8,
    encryption_algorithm=serialization.NoEncryption(),
)

pkcs8_b64 = base64.b64encode(priv_pkcs8).decode()
pub_b64 = base64.b64encode(pub_raw).decode()
c_array = (
    "static const unsigned char LICENSE_PUBKEY[32] = {\n  "
    + ", ".join(f"0x{b:02X}" for b in pub_raw)
    + "\n};"
)

(out_dir / "SIGN_PRIVATE_KEY.txt").write_text(pkcs8_b64 + "\n", encoding="utf-8")
(out_dir / "PUBLIC_KEY_BASE64.txt").write_text(pub_b64 + "\n", encoding="utf-8")
(out_dir / "LICENSE_PUBKEY_C.txt").write_text(c_array + "\n", encoding="utf-8")

combined = f"""=== 1) Cloudflare Secret: SIGN_PRIVATE_KEY ===
Copy everything below into Cloudflare Worker Secret named SIGN_PRIVATE_KEY

{pkcs8_b64}

=== 2) Public key base64 (optional) ===
{pub_b64}

=== 3) Paste into license.h (LICENSE_PUBKEY) ===
{c_array}
"""
(out_dir / "KEYS_ALL.txt").write_text(combined, encoding="utf-8")

print("OK - files written in:", out_dir)
print("  SIGN_PRIVATE_KEY.txt   <- Cloudflare Secret")
print("  LICENSE_PUBKEY_C.txt   <- paste into license.h")
print("  PUBLIC_KEY_BASE64.txt")
print("  KEYS_ALL.txt           <- everything in one file")
print()
print("--- SIGN_PRIVATE_KEY (Cloudflare) ---")
print(pkcs8_b64)
print()
print("--- C array (license.h) ---")
print(c_array)
