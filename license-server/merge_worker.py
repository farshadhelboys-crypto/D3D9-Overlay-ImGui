# Reconstruct worker.js from base64 chunks in this folder
import base64
import pathlib

parts = []
i = 0
while pathlib.Path(f"worker_b64_{i}.txt").exists():
    parts.append(pathlib.Path(f"worker_b64_{i}.txt").read_text().strip())
    i += 1
if not parts:
    raise SystemExit("no worker_b64_*.txt found")
data = base64.b64decode("".join(parts))
pathlib.Path("worker.js").write_bytes(data)
print("wrote worker.js", len(data), "bytes")
print("Deploy: npx wrangler deploy")
