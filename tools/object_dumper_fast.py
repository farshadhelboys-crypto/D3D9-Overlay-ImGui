"""
Object Dumper FAST — فقط آبجکت‌های متحرک (XYZ که جابه‌جا می‌شن)
دو نمونه با فاصله کوتاه می‌گیره؛ استاتیک/جسد حذف می‌شه.

pip install pymem
Run as admin, game open.
"""
import pymem
import pymem.process
import struct
import math
import time
from datetime import datetime

PROCESS_NAME = "CMLauncher.exe"
GAME_MOD = "clientmod_game.dll"

MY_POS_X = 0x242F510
VTABLE_1 = 0x01DDAC3C
VTABLE_2 = 0x01DDA9C8
V_OBJ_ORIGIN = 0x538

MIN_DIST = 5.0
MAX_DIST = 8000.0
# حداقل جابه‌جایی بین دو نمونه (واحد بازی) — کمتر از این = ثابت/جسد
MOVE_EPS = 2.0          # ~2 unit
MOVE_EPS_SQ = MOVE_EPS * MOVE_EPS
SAMPLE_DELAY = 0.45     # ثانیه بین دو نمونه


def scan_vtable(pm, base):
    targets = [base + VTABLE_1, base + VTABLE_2]
    found = []
    total = (0x40000000 - 0x10000000) // 0x100000
    done = 0
    print("[*] Scanning VTable (~30-90 sec)...")
    for addr in range(0x10000000, 0x40000000, 0x100000):
        done += 1
        if done % 50 == 0:
            print(f"    ... {100.0 * done / total:.0f}%  ({len(found)} hits)")
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


def read_pos(pm, obj):
    try:
        return struct.unpack("<3f", pm.read_bytes(obj + V_OBJ_ORIGIN, 12))
    except Exception:
        return None


def valid_range(x, y, z):
    if not (-20000 < x < 20000 and -20000 < y < 20000):
        return False
    if abs(x) < 0.1 and abs(y) < 0.1:
        return False
    if z < -500 or z > 500:
        return False
    return True


def main():
    print("=" * 50)
    print("  Object Dumper — MOVING XYZ only")
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
    print(f"[*] Local: ({px:.1f}, {py:.1f}, {pz:.1f})")

    # ---- sample 1 ----
    print("[*] Sample 1 (positions)...")
    sample1 = {}
    for obj in objects:
        pos = read_pos(pm, obj)
        if pos is None:
            continue
        x, y, z = pos
        if not valid_range(x, y, z):
            continue
        dist = math.sqrt((x - px) ** 2 + (y - py) ** 2 + (z - pz) ** 2)
        if dist < MIN_DIST or dist > MAX_DIST:
            continue
        sample1[obj] = (x, y, z, dist)

    print(f"    candidates after range filter: {len(sample1)}")
    print(f"[*] Wait {SAMPLE_DELAY:.2f}s for movement...")
    time.sleep(SAMPLE_DELAY)

    # refresh local (player may have moved)
    try:
        px, py, pz = struct.unpack("<3f", pm.read_bytes(base + MY_POS_X, 12))
    except Exception:
        pass

    # ---- sample 2 + motion filter ----
    print("[*] Sample 2 + keep only MOVING...")
    lines = []
    lines.append(f"# dump MOVING only — {datetime.now().isoformat()}")
    lines.append(f"# base=0x{base:X}  local=({px:.2f},{py:.2f},{pz:.2f})")
    lines.append(f"# move_eps={MOVE_EPS}  sample_delay={SAMPLE_DELAY}s")
    lines.append("# idx  obj_addr      x          y          z        dist     move")
    lines.append("")

    live = []
    for obj, (x1, y1, z1, _) in sample1.items():
        pos2 = read_pos(pm, obj)
        if pos2 is None:
            continue
        x2, y2, z2 = pos2
        if not valid_range(x2, y2, z2):
            continue
        dx = x2 - x1
        dy = y2 - y1
        dz = z2 - z1
        move_sq = dx * dx + dy * dy + dz * dz
        if move_sq < MOVE_EPS_SQ:
            continue  # static → skip (corpse / prop)
        move = math.sqrt(move_sq)
        dist = math.sqrt((x2 - px) ** 2 + (y2 - py) ** 2 + (z2 - pz) ** 2)
        if dist < MIN_DIST or dist > MAX_DIST:
            continue
        live.append((obj, x2, y2, z2, dist, move))
        lines.append(
            f"{len(live):3d}  0x{obj:08X}  {x2:10.3f}  {y2:10.3f}  {z2:10.3f}  {dist:8.1f}  {move:6.2f}"
        )

    lines.append("")
    lines.append(f"# moving live: {len(live)}  (from {len(sample1)} range-valid)")
    text = "\n".join(lines) + "\n"

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out = f"obj_dump_moving_{stamp}.txt"
    with open(out, "w", encoding="utf-8") as f:
        f.write(text)

    print(text)
    print(f"[+] Saved: {out}")
    print(f"[+] Only moving objects: {len(live)}")


if __name__ == "__main__":
    main()
