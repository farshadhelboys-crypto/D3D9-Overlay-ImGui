"""
Object Dumper FAST — فقط XYZ آبجکت‌های زنده
بدون اسکن سنگین pointer-list (اون بخش ۵+ دقیقه طول می‌کشید)

pip install pymem
Run as admin, game open.
"""
import pymem
import pymem.process
import struct
import math
import os
from datetime import datetime

PROCESS_NAME = "CMLauncher.exe"
GAME_MOD = "clientmod_game.dll"

MY_POS_X = 0x242F510
VTABLE_1 = 0x01DDAC3C
VTABLE_2 = 0x01DDA9C8
V_OBJ_ORIGIN = 0x538

MIN_DIST = 5.0
MAX_DIST = 8000.0


def scan_vtable(pm, base):
    targets = [base + VTABLE_1, base + VTABLE_2]
    found = []
    total = (0x40000000 - 0x10000000) // 0x100000
    done = 0
    print("[*] Scanning VTable (this part ~30-90 sec)...")
    for addr in range(0x10000000, 0x40000000, 0x100000):
        done += 1
        if done % 50 == 0:
            pct = 100.0 * done / total
            print(f"    ... {pct:.0f}%  ({len(found)} hits so far)")
        try:
            data = pm.read_bytes(addr, 0x100000)
            for vt in targets:
                needle = struct.pack("<I", vt)
                off = 0
                while True:
                    off = data.find(needle, off)
                    if off == -1:
                        break
                    found.append(addr + off)
                    off += 4
        except Exception:
            pass
    found = sorted(set(found))
    print(f"[+] Found {len(found)} unique objects")
    return found


def main():
    print("=" * 50)
    print("  Object Dumper FAST (XYZ only)")
    print("=" * 50)
    try:
        pm = pymem.Pymem(PROCESS_NAME)
        base = pymem.process.module_from_name(pm.process_handle, GAME_MOD).lpBaseOfDll
        print(f"[+] base=0x{base:X}")
    except Exception as e:
        print(f"[!] {e}")
        print("    Run as admin + game open")
        return

    objects = scan_vtable(pm, base)
    if not objects:
        print("[!] No objects")
        return

    try:
        px, py, pz = struct.unpack("<3f", pm.read_bytes(base + MY_POS_X, 12))
    except Exception:
        px = py = pz = 0.0

    print(f"[*] Local pos: ({px:.1f}, {py:.1f}, {pz:.1f})")
    print("[*] Reading XYZ (few seconds)...")

    lines = []
    lines.append(f"# dump {datetime.now().isoformat()}")
    lines.append(f"# base=0x{base:X} local=({px:.2f},{py:.2f},{pz:.2f})")
    lines.append("# idx  obj_addr      x          y          z        dist")
    lines.append("")

    live = []
    for obj in objects:
        try:
            x, y, z = struct.unpack("<3f", pm.read_bytes(obj + V_OBJ_ORIGIN, 12))
        except Exception:
            continue
        if not (-20000 < x < 20000 and -20000 < y < 20000 and -20000 < z < 500):
            continue
        if abs(x) < 0.1 and abs(y) < 0.1:
            continue
        if z < -500 or z > 500:
            continue
        dist = math.sqrt((x - px) ** 2 + (y - py) ** 2 + (z - pz) ** 2)
        if dist < MIN_DIST or dist > MAX_DIST:
            continue
        live.append((obj, x, y, z, dist))
        lines.append(
            f"{len(live):3d}  0x{obj:08X}  {x:10.3f}  {y:10.3f}  {z:10.3f}  {dist:8.1f}"
        )

    lines.append("")
    lines.append(f"# live valid: {len(live)}")
    text = "\n".join(lines) + "\n"

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out = f"obj_dump_{stamp}.txt"
    with open(out, "w", encoding="utf-8") as f:
        f.write(text)

    print(text)
    print(f"[+] Saved: {out}")
    print(f"[+] Done in one pass — {len(live)} live objects")


if __name__ == "__main__":
    main()
