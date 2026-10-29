"""
Box Tester v2 — باکس پایدار
- یک‌بار حرکت کافی است تا وارد لیست شود
- بعد از آن هر فریم XYZ زنده خوانده می‌شود (باکس قفل نمی‌شود)
- فقط بعد از ~5 ثانیه سکون کامل = جسد → حذف
- ایستادن عادی باکس را حذف نمی‌کند

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
MIN_BOX_H = 5
MAX_BOX_H = 1500

MOVE_EPS_SQ = 4.0
DEAD_STATIC_FRAMES = 300
HOLD_FRAMES = 12
DISCOVER_EVERY = 120


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
    found = sorted(set(found))
    print(f"[+] {len(found)} objects")
    return found


def read_xyz(pm, obj):
    try:
        return struct.unpack("<3f", pm.read_bytes(obj + V_OBJ_ORIGIN, 12))
    except Exception:
        return None


def in_world(x, y, z):
    if not (-20000 < x < 20000 and -20000 < y < 20000 and -500 < z < 500):
        return False
    if abs(x) < 0.1 and abs(y) < 0.1:
        return False
    return True


def main():
    try:
        pm = pymem.Pymem(PROCESS_NAME)
        base = pymem.process.module_from_name(pm.process_handle, GAME_MOD).lpBaseOfDll
        print(f"[+] base=0x{base:X}")
    except Exception as e:
        print(f"[!] {e}")
        return

    winfo = get_game_window(pm.process_id)
    if not winfo:
        print("[!] window not found")
        return
    hwnd, wx, wy, ww, wh = winfo
    print(f"[+] window {ww}x{wh}")

    all_objs = scan_vtable(pm, base)
    tracked = {}

    root = tk.Tk()
    root.overrideredirect(True)
    root.attributes("-topmost", True)
    root.attributes("-transparentcolor", "black")
    root.config(bg="black")
    root.geometry(f"{ww}x{wh}+{wx}+{wy}")
    canvas = tk.Canvas(root, bg="black", highlightthickness=0, width=ww, height=wh)
    canvas.pack(fill="both", expand=True)

    state = {"frame": 0, "fps": 0.0, "last_t": time.time()}

    def discover_new(px, py, pz):
        for obj in all_objs:
            if obj in tracked:
                continue
            pos = read_xyz(pm, obj)
            if pos is None:
                continue
            x, y, z = pos
            if not in_world(x, y, z):
                continue
            dist = math.sqrt((x-px)**2 + (y-py)**2 + (z-pz)**2)
            if dist < MIN_DIST or dist > MAX_DIST:
                continue
            tracked[obj] = {
                "pos": (x, y, z),
                "prev": (x, y, z),
                "static": 0,
                "ever_moved": False,
                "hold": 0,
            }

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
            fr = state["frame"]

            if fr == 0 or fr % DISCOVER_EVERY == 0:
                discover_new(px, py, pz)

            canvas.delete("all")
            drawn = 0
            players = 0
            to_delete = []

            for obj, te in tracked.items():
                pos = read_xyz(pm, obj)
                if pos is None or not in_world(*pos):
                    te["hold"] -= 1
                    if te["hold"] <= 0:
                        to_delete.append(obj)
                    continue

                x, y, z = pos
                px0, py0, pz0 = te["prev"]
                dx, dy, dz = x - px0, y - py0, z - pz0
                moved = (dx*dx + dy*dy + dz*dz) >= MOVE_EPS_SQ
                if moved:
                    te["static"] = 0
                    te["ever_moved"] = True
                else:
                    te["static"] += 1

                te["prev"] = (x, y, z)
                te["pos"] = (x, y, z)
                te["hold"] = HOLD_FRAMES

                if te["static"] > DEAD_STATIC_FRAMES:
                    to_delete.append(obj)
                    continue

                if not te["ever_moved"]:
                    continue

                dist = math.sqrt((x-px)**2 + (y-py)**2 + (z-pz)**2)
                if dist < MIN_DIST or dist > MAX_DIST:
                    continue

                players += 1
                feet = w2s(mat, (x, y, z), sw, sh)
                head = w2s(mat, (x, y, z + HEAD_OFFSET), sw, sh)
                if not (feet[2] and head[2]):
                    continue
                fx, fy = feet[0], feet[1]
                hx, hy = head[0], head[1]
                box_h = abs(fy - hy)
                if box_h < MIN_BOX_H or box_h > MAX_BOX_H:
                    continue
                box_w = box_h * 0.40
                top_y = min(fy, hy)
                cx = (fx + hx) * 0.5
                if not (-100 < cx < sw + 100 and -100 < top_y < sh + 100):
                    continue

                canvas.create_rectangle(
                    cx - box_w/2, top_y, cx + box_w/2, top_y + box_h,
                    outline="#ff3333", width=2
                )
                canvas.create_text(
                    cx, top_y - 10, text=f"{int(dist)}m",
                    fill="#ff6666", font=("Consolas", 9)
                )
                drawn += 1

            for obj in to_delete:
                tracked.pop(obj, None)

            now = time.time()
            dt = now - state["last_t"]
            state["last_t"] = now
            if dt > 0:
                state["fps"] = 0.9 * state["fps"] + 0.1 / dt

            info = (
                f"FPS:{state['fps']:.0f}  Draw:{drawn}  "
                f"Players:{players}  Tracked:{len(tracked)}"
            )
            canvas.create_text(10, 10, text=info, fill="white",
                               font=("Consolas", 11), anchor="nw")
            state["frame"] = fr + 1
        except Exception:
            pass
        root.after(16, update)

    print("[*] Overlay running")
    root.after(100, update)
    root.mainloop()


if __name__ == "__main__":
    main()
