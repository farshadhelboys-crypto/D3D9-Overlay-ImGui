"""
ESP Offset Dumper
- attaches to a running ESP process
- hunts offsets that arrived from server and stay in RAM
- dumps JSON/config strings, RVA tables, known constants

pip install pymem
Run as Administrator.
"""
import pymem
import pymem.process
import pymem.memory
import struct
import re
import os
import sys
import time
from datetime import datetime
from collections import defaultdict

KNOWN_VALUES = {
    0x27603D0: "matrix_rva (working)",
    0x277FF50: "matrix_rva_old",
    0x242F510: "my_pos_x",
    0x242F514: "my_pos_y",
    0x242F518: "my_pos_z",
    0x01DDAC3C: "vtable_1",
    0x01DDA9C8: "vtable_2",
    0x538: "v_obj_origin",
    0x2478218: "player_res_rva",
    0x1270: "res_hp",
    0x1128: "res_team",
    0x318: "v_obj_origin_b",
    0x290: "v_obj_origin_c",
    0x11C: "obj_hp_off",
    0x120: "obj_life_state",
    0x1FC: "obj_team_off",
}

TEXT_KEYWORDS = [
    b"matrix", b"vtable", b"origin", b"offset", b"rva", b"entity",
    b"player", b"clientmod", b"CMLauncher", b"viewmatrix", b"bone",
    b"health", b"team", b"pos_x", b"pos_y", b"pos_z", b"life_state",
    b"obj_hp", b"my_pos", b"0x27", b"0x24", b"0x01DD",
    b"\"matrix", b"\"vtable", b"\"origin", b"offsets", b"config",
]

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)) or ".", "esp_dumps")


def pick_process():
    print("=" * 60)
    print("  ESP Offset Dumper")
    print("=" * 60)
    print("\n[1] Enter ESP process name (e.g. MyEsp.exe) or partial name")
    print("[2] Or numeric PID")
    print("    Note: game=CMLauncher | ESP usually has another name\n")
    q = input("Process name or PID: ").strip()
    if not q:
        return None, None

    if q.isdigit():
        pid = int(q)
        try:
            pm = pymem.Pymem()
            pm.open_process_from_id(pid)
            return pm, pid
        except Exception as e:
            print(f"[!] Open PID {pid} failed: {e}")
            return None, None

    try:
        pm = pymem.Pymem(q)
        return pm, pm.process_id
    except Exception:
        pass

    try:
        import subprocess
        out = subprocess.check_output(
            ["wmic", "process", "get", "ProcessId,Name", "/FORMAT:CSV"],
            stderr=subprocess.DEVNULL, text=True, errors="ignore"
        )
        matches = []
        for line in out.splitlines():
            parts = [p.strip() for p in line.split(",")]
            if len(parts) >= 3 and parts[-1].isdigit():
                name, pid = parts[-2], int(parts[-1])
                if q.lower() in name.lower():
                    matches.append((pid, name))
        if not matches:
            print(f"[!] No process matching '{q}'")
            return None, None
        print("\nMatches:")
        for i, (pid, name) in enumerate(matches[:20]):
            print(f"  [{i}] PID={pid}  {name}")
        idx = input("Pick index [0]: ").strip() or "0"
        pid, name = matches[int(idx)]
        pm = pymem.Pymem()
        pm.open_process_from_id(pid)
        print(f"[+] Attached: {name} PID={pid}")
        return pm, pid
    except Exception as e:
        print(f"[!] {e}")
        return None, None


def iter_regions(pm):
    address = 0
    while address < 0x7FFFFFFF:
        try:
            mbi = pymem.memory.virtual_query(pm.process_handle, address)
        except Exception:
            break
        size = mbi.RegionSize or 0x1000
        protect = mbi.Protect
        state = mbi.State
        if state == 0x1000 and size > 0 and size < 0x10000000:
            if protect and not (protect & 0x01) and not (protect & 0x100):
                yield mbi.BaseAddress, size
        nxt = address + size
        if nxt <= address:
            break
        address = nxt


def scan_known_u32(pm):
    print("\n[*] Scanning known offset constants...")
    hits = defaultdict(list)
    targets = {struct.pack("<I", k): (k, name) for k, name in KNOWN_VALUES.items() if k > 0xFF}
    checked = 0
    for base, size in iter_regions(pm):
        try:
            data = pm.read_bytes(base, min(size, 0x400000))
        except Exception:
            continue
        checked += 1
        for needle, (val, name) in targets.items():
            start = 0
            while True:
                off = data.find(needle, start)
                if off == -1:
                    break
                hits[val].append(base + off)
                start = off + 4
                if len(hits[val]) >= 8:
                    break
    print(f"    regions: {checked}")
    return hits


def scan_text_configs(pm):
    print("\n[*] Scanning config/JSON text...")
    findings = []
    for base, size in iter_regions(pm):
        try:
            data = pm.read_bytes(base, min(size, 0x200000))
        except Exception:
            continue
        for kw in TEXT_KEYWORDS:
            start = 0
            while True:
                off = data.find(kw, start)
                if off == -1:
                    break
                a = max(0, off - 64)
                b = min(len(data), off + 256)
                window = data[a:b]
                text = ""
                for c in window:
                    if 32 <= c < 127:
                        text += chr(c)
                    elif c in (9, 10, 13):
                        text += " "
                    else:
                        text += "."
                text = re.sub(r"\s+", " ", text).strip()
                if len(text) > 20:
                    findings.append((base + a, text[:300]))
                start = off + len(kw)
    uniq = []
    seen = set()
    for addr, text in findings:
        key = text[:80]
        if key in seen:
            continue
        seen.add(key)
        uniq.append((addr, text))
    return uniq[:80]


def scan_rva_tables(pm):
    print("\n[*] Scanning RVA-like tables...")
    tables = []
    for base, size in iter_regions(pm):
        try:
            data = pm.read_bytes(base, min(size, 0x100000))
        except Exception:
            continue
        if len(data) < 32:
            continue
        i = 0
        while i + 24 <= len(data):
            vals = []
            ok = True
            for k in range(6):
                v = struct.unpack_from("<I", data, i + k * 4)[0]
                if v == 0:
                    vals.append(v)
                    continue
                if 0x100 <= v <= 0x4000000:
                    vals.append(v)
                else:
                    ok = False
                    break
            if ok and sum(1 for v in vals if v > 0x10000) >= 3:
                tables.append((base + i, vals))
                i += 24
            else:
                i += 4
            if len(tables) > 200:
                break
        if len(tables) > 200:
            break
    scored = []
    for addr, vals in tables:
        score = sum(1 for v in vals if v in KNOWN_VALUES)
        score += sum(1 for v in vals if 0x2000000 <= v <= 0x2800000)
        scored.append((score, addr, vals))
    scored.sort(reverse=True)
    return scored[:40]


def dump_module_list(pm):
    mods = []
    try:
        for mod in pm.list_modules():
            mods.append((mod.lpBaseOfDll, mod.SizeOfImage, mod.name))
    except Exception as e:
        mods.append((0, 0, f"list_modules error: {e}"))
    return mods


def extract_hex_strings(pm):
    print("\n[*] Extracting 0x hex strings...")
    pat = re.compile(rb"0x[0-9a-fA-F]{5,8}")
    found = defaultdict(int)
    for base, size in iter_regions(pm):
        try:
            data = pm.read_bytes(base, min(size, 0x200000))
        except Exception:
            continue
        for m in pat.findall(data):
            try:
                found[m.decode().lower()] += 1
            except Exception:
                pass
    return sorted(found.items(), key=lambda x: -x[1])[:60]


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    pm, pid = pick_process()
    if not pm:
        return

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_path = os.path.join(OUT_DIR, f"esp_dump_{pid}_{stamp}.txt")
    lines = []
    lines.append(f"# ESP Offset Dump  {datetime.now().isoformat()}")
    lines.append(f"# target_pid={pid}")
    lines.append("")

    lines.append("=== MODULES ===")
    for base, size, name in dump_module_list(pm):
        if isinstance(base, int) and base:
            lines.append(f"  0x{base:08X}  size=0x{size:X}  {name}")
        else:
            lines.append(f"  {name}")
    lines.append("")

    hits = scan_known_u32(pm)
    lines.append("=== KNOWN CONSTANT HITS ===")
    if not hits:
        lines.append("  (none — may be encrypted or different values)")
    for val, addrs in sorted(hits.items(), key=lambda x: -len(x[1])):
        name = KNOWN_VALUES.get(val, "?")
        addr_s = ", ".join(f"0x{a:X}" for a in addrs[:5])
        lines.append(f"  0x{val:X}  ({name})  count={len(addrs)}  @ {addr_s}")
    lines.append("")

    texts = scan_text_configs(pm)
    lines.append("=== CONFIG / JSON TEXT ===")
    for addr, text in texts:
        lines.append(f"  @0x{addr:08X}  {text}")
    lines.append("")

    hexes = extract_hex_strings(pm)
    lines.append("=== FREQUENT 0x HEX STRINGS ===")
    for s, cnt in hexes:
        tag = ""
        try:
            v = int(s, 16)
            if v in KNOWN_VALUES:
                tag = f"  << {KNOWN_VALUES[v]}"
        except Exception:
            pass
        lines.append(f"  {s}  x{cnt}{tag}")
    lines.append("")

    tables = scan_rva_tables(pm)
    lines.append("=== POSSIBLE RVA TABLES ===")
    for score, addr, vals in tables:
        vs = " ".join(f"0x{v:X}" for v in vals)
        mark = " **KNOWN**" if score > 0 else ""
        lines.append(f"  score={score}  @0x{addr:08X}  [{vs}]{mark}")
    lines.append("")

    lines.append("=== NOTES ===")
    lines.append("- Server-fetched offsets often appear as JSON text or hex strings in heap.")
    lines.append("- KNOWN hits mean that ESP uses same values we already have.")
    lines.append("- New frequent hex strings may be updated RVAs after game patch.")
    lines.append("- Dump once after ESP loaded config, while in match.")
    lines.append("")

    text = "\n".join(lines)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(text)
    print("\n" + text)
    print(f"\n[+] Saved: {out_path}")


if __name__ == "__main__":
    main()
