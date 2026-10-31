"""Build full worker.js from worker.js.b64.part0 + part1"""
import base64
import pathlib

root = pathlib.Path(__file__).resolve().parent
parts = []
for name in ("worker.js.b64.part0", "worker.js.b64.part1", "worker.js.b64"):
    p = root / name
    if p.exists():
        parts.append(p.read_text().strip())
if not parts:
    raise SystemExit("missing worker.js.b64.part0/part1")
# if both parts exist, prefer joined parts only
p0, p1 = root / "worker.js.b64.part0", root / "worker.js.b64.part1"
if p0.exists() and p1.exists():
    blob = p0.read_text().strip() + p1.read_text().strip()
else:
    blob = "".join(parts)
out = root / "worker.js"
out.write_bytes(base64.b64decode(blob))
print("wrote", out, out.stat().st_size, "bytes")
print("Open worker.js, paste into Cloudflare Worker, Deploy.")
