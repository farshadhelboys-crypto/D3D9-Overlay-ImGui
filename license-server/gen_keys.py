"""Generate Ed25519 keypair for Farshad ESP license server."""
import base64
import sys

try:
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    from cryptography.hazmat.primitives import serialization
except ImportError:
    print("pip install cryptography")
    sys.exit(1)

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

print("=== Cloudflare Secret: SIGN_PRIVATE_KEY (PKCS8 base64) ===")
print(base64.b64encode(priv_pkcs8).decode())
print()
print("=== PUBLIC base64 (client) ===")
print(base64.b64encode(pub_raw).decode())
print()
print("=== C array ===")
print("static const unsigned char LICENSE_PUBKEY[32] = {")
print("  " + ", ".join(f"0x{b:02X}" for b in pub_raw))
print("};")
