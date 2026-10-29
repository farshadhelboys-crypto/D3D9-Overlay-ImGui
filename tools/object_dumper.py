"""
Object Dumper — dumps live VTable player objects + XYZ
Use the dump to reverse entity array / pointer lists.

Requirements: pip install pymem pywin32
Run as admin while in-game.
"""
import pymem
import pymem.process
import struct
import math
import time
import os
from datetime import datetime

PROCESS_NAME = "CMLauncher.exe"
GAME_MOD = "clientmod_game.dll"

# Working offsets (same as proven ESP)
MY_POS_X = 0x242F510
MY_POS_Y = 0x242F514
MY_POS_Z = 0x242F518
MATRIX_RVA = 0x27603D0
VTABLE_1 = 0x01DDAC3C
VTABLE_2 = 0x01DDA9C8
V_OBJ_ORIGIN = 0x538

# Filters (same as working ESP)
MIN_DIST = 5.0
MAX_DIST = 8000.0
POS_EPS_SQ = 16.0


def scan_vtable(pm, base):
    targets = [base + VTABLE_1, base + VTABLE_2]
    found = []
    print("[*] Scanning VTable objects (0x10000000 .. 0x40000000)...")
    for addr in range(0x10000000, 0x40000000, 0x100000):
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
    # unique + sort
    found = sorted(set(found))
    print(f"[+] Found {len(found)} unique VTable objects")
    return found


def read_xyz(pm, obj):
    try:
        raw = pm.read_bytes(obj + V_OBJ_ORIGIN, 12)
        x, y, z = struct.unpack("<3f", raw)
        return x, y, z
    except Exception:
        return None


def is_valid_pos(x, y, z, px, py, pz):
    if not (-20000 < x < 20000 and -20000 < y < 20000 and -20000 < z < 20000):
        return False
    if abs(x) < 0.1 and abs(y) < 0.1:
        return False
    if not (-500 < z < 500):
        return False
    dist = math.sqrt((x - px) ** 2 + (y - py) ** 2 + (z - pz) ** 2)
    if dist < MIN_DIST or dist > MAX_DIST:
        return False
    return True


def dump_once(pm, base, objects, out_path):
    # local player pos
    try:
        px, py, pz = struct.unpack("<3f", pm.read_bytes(base + MY_POS_X, 12))
    except Exception:
        px = py = pz = 0.0
        print("[!] Could not read local pos")

    lines = []
    lines.append(f"# Object dump — {datetime.now().isoformat()}")
    lines.append(f"# base=0x{base:X}  local=({px:.2f},{py:.2f},{pz:.2f})")
    lines.append(f"# origin_off=0x{V_OBJ_ORIGIN:X}")
    lines.append("# format: index  obj_addr  x  y  z  dist")
    lines.append("")

    live = []
    for i, obj in enumerate(objects):
        pos = read_xyz(pm, obj)
        if pos is None:
            continue
        x, y, z = pos
        if not is_valid_pos(x, y, z, px, py, pz):
            continue
        dist = math.sqrt((x - px) ** 2 + (y - py) ** 2 + (z - pz) ** 2)
        live.append((obj, x, y, z, dist))
        lines.append(f"{len(live):3d}  0x{obj:08X}  {x:10.3f}  {y:10.3f}  {z:10.3f}  {dist:8.1f}")

    lines.append("")
    lines.append(f"# total live objects with valid XYZ: {len(live)}")

    # --- pointer-array hunt: search for arrays of pointers to these objects ---
    if live:
        obj_set = {o for o, *_ in live}
        lines.append("")
        lines.append("# --- pointer list candidates (regions that hold many of our obj ptrs) ---")
        # scan heap-ish for consecutive pointers matching our objects
        hits = []
        for region_start in range(0x10000000, 0x40000000, 0x200000):
            try:
                blob = pm.read_bytes(region_start, 0x200000)
            except Exception:
                continue
            # walk as uint32 pointers
            for off in range(0, len(blob) - 4, 4):
                ptr = struct.unpack_from("<I", blob, off)[0]
                if ptr in obj_set:
                    # count how many of next 32 slots are also our objects
                    count = 0
                    for k in range(0, 32 * 4, 4):
                        if off + k + 4 > len(blob):
                            break
                        p2 = struct.unpack_from("<I", blob, off + k)[0]
                        if p2 in obj_set:
                            count += 1
                    if count >= 3:
                        hits.append((region_start + off, count))
        # unique by address, keep best counts
        hits = sorted(set(hits), key=lambda t: -t[1])[:20]
        for addr, cnt in hits:
            lines.append(f"# ptr_list candidate 0x{addr:08X}  matches={cnt}/32")

    text = "\n".join(lines) + "\n"
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(text)
    print(text)
    print(f"[+] Wrote {out_path}  ({len(live)} live objects)")
    return live


def main():
    print("=" * 60)
    print("  VTable Object Dumper  (XYZ + pointer-list hunt)")
    print("=" * 60)
    try:
        pm = pymem.Pymem(PROCESS_NAME)
        base = pymem.process.module_from_name(pm.process_handle, GAME_MOD).lpBaseOfDll
        print(f"[+] Process OK  base=0x{base:X}")
    except Exception as e:
        print(f"[!] Attach failed: {e}")
        print("    → Run as admin, make sure game is open")
        return

    objects = scan_vtable(pm, base)
    if not objects:
        print("[!] No VTable objects found")
        return

    out_dir = os.path.dirname(os.path.abspath(__file__)) or "."
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_path = os.path.join(out_dir, f"obj_dump_{stamp}.txt")

    print("\n[*] Dumping live objects (one shot)...")
    dump_once(pm, base, objects, out_path)

    print("\n[*] Press Enter to dump again, or Ctrl+C to exit")
    try:
        while True:
            input()
            stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            out_path = os.path.join(out_dir, f"obj_dump_{stamp}.txt")
            dump_once(pm, base, objects, out_path)
    except KeyboardInterrupt:
        print("\n[+] Done")


if __name__ == "__main__":
    main()
