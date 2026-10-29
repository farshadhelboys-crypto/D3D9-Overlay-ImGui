"""
Array Hunter — از XYZ زنده‌ها دنبال آرایه پوینتر می‌گردد
اگر پیدا شد، یک‌بار باکس می‌کشد تا تأیید شود.

pip install pymem pywin32
Run as admin, game open.
"""
import pymem
import pymem.process
import struct
import math
import time
import tkinter as tk
import win32gui
import win32process

PROCESS_NAME = "CMLauncher.exe"
GAME_MOD = "clientmod_game.dll"

MY_POS_X = 0x242F510
MATRIX_RVA = 0x27603D0
VTABLE_1 = 0x01DDAC3C
VTABLE_2 = 0x01DDA9C8
V_OBJ_ORIGIN = 0x538

HEAD_OFFSET = 72.0
MIN_DIST = 30.0
MAX_DIST = 5000.0
MOVE_EPS_SQ = 4.0


def get_game_window(pid):
    result = []
    def cb(hwnd, _):
        if not win32gui.IsWindowVisible(hwnd):
            return
        _, wpid = win32process.GetWindowThreadProcessId(hwnd)
        if wpid == pid:
            rect = win32gui.GetClientRect(hwnd)
            if rect[2] > 100 and rect[3] > 100:
                pt = win32gui.ClientToScreen(hwnd, (0, 0))
                result.append((hwnd, pt[0], pt[1], rect[2], rect[3]))
    win32gui.EnumWindows(cb, None)
    return result[0] if result else None


def w2s(mat, pos, sw, sh):
    x, y, z = pos
    cx = mat[0]*x + mat[1]*y + mat[2]*z + mat[3]
    cy = mat[4]*x + mat[5]*y + mat[6]*z + mat[7]
    cw = mat[12]*x + mat[13]*y + mat[14]*z + mat[15]
    if abs(cw) < 0.001:
        return None, None, False
    sx = (sw * 0.5) * (cx / cw) + sw * 0.5
    sy = -(sh * 0.5) * (cy / cw) + sh * 0.5
    return sx, sy, cw > 0.2


def scan_vtable(pm, base):
    targets = [base + VTABLE_1, base + VTABLE_2]
    found = []
    print("[*] VTable scan...")
    total = (0x40000000 - 0x10000000) // 0x100000
    done = 0
    for addr in range(0x10000000, 0x40000000, 0x100000):
        done += 1
        if done % 80 == 0:
            print(f"    ... {100.0*done/total:.0f}%")
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
    return sorted(set(found))


def read_xyz(pm, obj):
    try:
        return struct.unpack("<3f", pm.read_bytes(obj + V_OBJ_ORIGIN, 12))
    except Exception:
        return None


def in_world(x, y, z):
    return (-20000 < x < 20000 and -20000 < y < 20000 and -500 < z < 500
            and not (abs(x) < 0.1 and abs(y) < 0.1))


def get_live_players(pm, objects, px, py, pz):
    s1 = {}
    for obj in objects:
        pos = read_xyz(pm, obj)
        if pos is None or not in_world(*pos):
            continue
        x, y, z = pos
        dist = math.sqrt((x-px)**2 + (y-py)**2 + (z-pz)**2)
        if MIN_DIST <= dist <= MAX_DIST:
            s1[obj] = pos
    time.sleep(0.4)
    live = []
    for obj, (x1, y1, z1) in s1.items():
        pos = read_xyz(pm, obj)
        if pos is None:
            continue
        x2, y2, z2 = pos
        if (x2-x1)**2 + (y2-y1)**2 + (z2-z1)**2 < MOVE_EPS_SQ:
            continue
        live.append(obj)
    return live


def hunt_pointer_arrays(pm, live_objs, base):
    obj_set = set(live_objs)
    if len(obj_set) < 2:
        print("[!] Need at least 2 live players for array hunt")
        return []

    print(f"[*] Hunting pointer arrays for {len(obj_set)} live objects...")
    print("    (this takes ~1-2 min)")

    candidates = []
    seen_addrs = set()
    chunk = 0x200000
    for region in range(0x10000000, 0x40000000, chunk):
        try:
            blob = pm.read_bytes(region, chunk)
        except Exception:
            continue
        n = len(blob) // 4
        for i in range(n):
            ptr = struct.unpack_from("<I", blob, i * 4)[0]
            if ptr not in obj_set:
                continue
            abs_addr = region + i * 4
            if abs_addr in seen_addrs:
                continue
            score = 0
            uniq = set()
            window = blob[i*4 : i*4 + 64*4]
            for j in range(0, len(window), 4):
                p = struct.unpack_from("<I", window, j)[0]
                if p in obj_set:
                    score += 1
                    uniq.add(p)
            if score >= 3 and len(uniq) >= 2:
                candidates.append((abs_addr, score, len(uniq)))
                for k in range(-2, score + 2):
                    seen_addrs.add(abs_addr + k * 4)

    candidates.sort(key=lambda t: (-t[2], -t[1], t[0]))
    filtered = []
    for addr, score, uniq in candidates:
        if any(abs(addr - a) < 0x100 for a, _, _ in filtered):
            continue
        filtered.append((addr, score, uniq))
        if len(filtered) >= 15:
            break

    print(f"[+] Top candidates: {len(filtered)}")
    for addr, score, uniq in filtered:
        rva = addr - base if addr >= base else -1
        rva_s = f"RVA=0x{rva:X}" if 0 < rva < 0x4000000 else "heap"
        print(f"    0x{addr:08X}  hits={score}/64  unique={uniq}  {rva_s}")
    return filtered


def validate_array(pm, arr_addr, expected_objs, slots=64):
    try:
        raw = pm.read_bytes(arr_addr, slots * 4)
    except Exception:
        return 0, []
    good = []
    for i in range(slots):
        ptr = struct.unpack_from("<I", raw, i * 4)[0]
        if ptr < 0x10000 or ptr > 0x7FFFFFFF:
            continue
        pos = read_xyz(pm, ptr)
        if pos and in_world(*pos):
            good.append((ptr, pos))
    return len(good), good


def overlay_from_array(pm, base, arr_addr, slots=64):
    winfo = get_game_window(pm.process_id)
    if not winfo:
        print("[!] no window")
        return
    hwnd, wx, wy, ww, wh = winfo

    root = tk.Tk()
    root.overrideredirect(True)
    root.attributes("-topmost", True)
    root.attributes("-transparentcolor", "black")
    root.config(bg="black")
    root.geometry(f"{ww}x{wh}+{wx}+{wy}")
    canvas = tk.Canvas(root, bg="black", highlightthickness=0, width=ww, height=wh)
    canvas.pack(fill="both", expand=True)

    state = {"fps": 0.0, "last_t": time.time()}
    print(f"[*] Overlay from ARRAY 0x{arr_addr:X}")

    def update():
        try:
            w2 = get_game_window(pm.process_id)
            if w2:
                _, wx2, wy2, ww2, wh2 = w2
                root.geometry(f"{ww2}x{wh2}+{wx2}+{wy2}")
                canvas.config(width=ww2, height=wh2)
                sw, sh = float(ww2), float(wh2)
            else:
                sw, sh = float(ww), float(wh)

            px, py, pz = struct.unpack("<3f", pm.read_bytes(base + MY_POS_X, 12))
            mat = struct.unpack("<16f", pm.read_bytes(base + MATRIX_RVA, 64))
            raw = pm.read_bytes(arr_addr, slots * 4)

            canvas.delete("all")
            drawn = 0
            for i in range(slots):
                ptr = struct.unpack_from("<I", raw, i * 4)[0]
                if ptr < 0x10000:
                    continue
                pos = read_xyz(pm, ptr)
                if not pos or not in_world(*pos):
                    continue
                x, y, z = pos
                dist = math.sqrt((x-px)**2 + (y-py)**2 + (z-pz)**2)
                if dist < MIN_DIST or dist > MAX_DIST:
                    continue
                feet = w2s(mat, (x, y, z), sw, sh)
                head = w2s(mat, (x, y, z + HEAD_OFFSET), sw, sh)
                if not (feet[2] and head[2]):
                    continue
                box_h = abs(feet[1] - head[1])
                if box_h < 5 or box_h > 1500:
                    continue
                box_w = box_h * 0.4
                top_y = min(feet[1], head[1])
                cx = (feet[0] + head[0]) * 0.5
                canvas.create_rectangle(
                    cx-box_w/2, top_y, cx+box_w/2, top_y+box_h,
                    outline="#00ff88", width=2
                )
                canvas.create_text(
                    cx, top_y-10, text=f"{int(dist)}m",
                    fill="#00ff88", font=("Consolas", 9)
                )
                drawn += 1

            now = time.time()
            dt = now - state["last_t"]
            state["last_t"] = now
            if dt > 0:
                state["fps"] = 0.9*state["fps"] + 0.1/dt
            canvas.create_text(
                10, 10,
                text=f"ARRAY 0x{arr_addr:X}  FPS:{state['fps']:.0f}  Draw:{drawn}",
                fill="white", font=("Consolas", 11), anchor="nw"
            )
        except Exception:
            pass
        root.after(16, update)

    root.after(50, update)
    root.mainloop()


def main():
    print("=" * 55)
    print("  Array Hunter + optional box verify")
    print("=" * 55)
    try:
        pm = pymem.Pymem(PROCESS_NAME)
        base = pymem.process.module_from_name(pm.process_handle, GAME_MOD).lpBaseOfDll
        print(f"[+] base=0x{base:X}")
    except Exception as e:
        print(f"[!] {e}")
        return

    objects = scan_vtable(pm, base)
    print(f"[+] objects={len(objects)}")

    try:
        px, py, pz = struct.unpack("<3f", pm.read_bytes(base + MY_POS_X, 12))
    except Exception:
        px = py = pz = 0.0

    live = get_live_players(pm, objects, px, py, pz)
    print(f"[+] live moving players: {len(live)}")
    for i, o in enumerate(live[:12]):
        pos = read_xyz(pm, o)
        print(f"    {i+1}  0x{o:08X}  {pos}")

    if len(live) < 2:
        print("[!] Need 2+ moving players in match.")
        return

    cands = hunt_pointer_arrays(pm, live, base)
    if not cands:
        print("[!] No array candidate found.")
        print("    -> Fallback: VTable tracker into C++")
        return

    print("\n[*] Validating candidates...")
    best = None
    best_n = 0
    for addr, score, uniq in cands[:10]:
        n, goods = validate_array(pm, addr, live, slots=64)
        print(f"    0x{addr:08X}  valid_xyz={n}")
        if n > best_n:
            best_n = n
            best = addr

    if not best or best_n < 2:
        print("[!] Candidates weak.")
        print("    -> Fix VTable method into C++ project.")
        return

    print(f"\n[+] BEST array: 0x{best:08X}  valid_xyz={best_n}")
    rva = best - base
    if 0 < rva < 0x4000000:
        print(f"    possible module RVA: 0x{rva:X}")
    else:
        print("    (heap address - changes each run)")

    ans = input("\nDraw boxes FROM this array? [y/N]: ").strip().lower()
    if ans == "y":
        overlay_from_array(pm, base, best, slots=64)
    else:
        print("Done.")


if __name__ == "__main__":
    main()
