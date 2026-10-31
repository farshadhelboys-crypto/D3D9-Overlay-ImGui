"""Decode worker.js.b64 -> worker.js (full signed worker with trial/offsets)."""
import base64
import pathlib

root = pathlib.Path(__file__).resolve().parent
b64 = (root / "worker.js.b64").read_text().strip()
out = root / "worker.js"
out.write_bytes(base64.b64decode(b64))
print("wrote", out, out.stat().st_size, "bytes")
print("Paste worker.js into Cloudflare Worker and Deploy.")
