#!/usr/bin/env python3
"""Generate signalhunter/src/core/camera_oui_table.h from the IEEE registries.

Downloads the MA-L (24-bit), MA-M (28-bit) and MA-S (36-bit) assignment lists,
keeps every block whose organisation matches tools/camera_vendors.py, and emits
sorted lookup tables for camera_match.cpp.

Usage:
    python tools/gen_camera_oui.py            # regenerate (uses tools/.cache if present)
    python tools/gen_camera_oui.py --refresh  # re-download the registries first
    python tools/gen_camera_oui.py --check    # exit 1 if the committed header is out of date

Standard library only. If the IEEE site refuses the download, the Wireshark
`manuf` file is used as a fallback source.
"""
import argparse
import csv
import io
import os
import re
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)  # works under `python -I` too
from camera_vendors import VENDORS, CAM  # noqa: E402

ROOT = os.path.dirname(HERE)
OUT_PATH = os.path.join(ROOT, "signalhunter", "src", "core", "camera_oui_table.h")
CACHE = os.path.join(HERE, ".cache")

IEEE_SOURCES = [
    ("MA-L", "https://standards-oui.ieee.org/oui/oui.csv", 24),
    ("MA-M", "https://standards-oui.ieee.org/oui28/mam.csv", 28),
    ("MA-S", "https://standards-oui.ieee.org/oui36/oui36.csv", 36),
]
WIRESHARK_MANUF = "https://www.wireshark.org/download/automated/data/manuf"
USER_AGENT = "Mozilla/5.0 (compatible; SignalHunter-OUI-generator)"


def fetch(url, cache_name, refresh):
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, cache_name)
    if not refresh and os.path.exists(path):
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            return f.read()
    last_err = None
    for attempt in range(3):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(req, timeout=60) as resp:
                text = resp.read().decode("utf-8", errors="replace")
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
            return text
        except Exception as e:  # noqa: BLE001
            last_err = e
            time.sleep(2 * (attempt + 1))
    raise RuntimeError(f"download failed for {url}: {last_err}")


def classify(org):
    for rx, name, tier in VENDORS:
        if re.search(rx, org, re.IGNORECASE):
            return name, tier
    return None


def parse_ieee_csv(text, bits):
    """Yield (key, org) from an IEEE CSV (Registry,Assignment,Organization Name,...)."""
    reader = csv.DictReader(io.StringIO(text))
    for row in reader:
        assignment = (row.get("Assignment") or "").strip()
        org = (row.get("Organization Name") or "").strip()
        if not assignment or not org:
            continue
        if len(assignment) * 4 != bits:
            continue
        yield int(assignment, 16), org


def parse_wireshark_manuf(text):
    """Yield (bits, key, org) from Wireshark's manuf file."""
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) < 2:
            continue
        prefix = parts[0]
        org = parts[-1] if len(parts) >= 3 else parts[1]
        if "/" in prefix:
            addr, bits = prefix.split("/")
            bits = int(bits)
        else:
            addr, bits = prefix, 24
        hexstr = addr.replace(":", "").replace("-", "")
        if bits not in (24, 28, 36):
            continue
        value = int(hexstr.ljust(12, "0"), 16) >> (48 - bits)
        yield bits, value, org


def collect(refresh):
    tables = {24: {}, 28: {}, 36: {}}
    source_note = []
    try:
        for name, url, bits in IEEE_SOURCES:
            text = fetch(url, f"ieee_{bits}.csv", refresh)
            for key, org in parse_ieee_csv(text, bits):
                tables[bits][key] = org
            source_note.append(f"{name}: {url}")
    except RuntimeError as e:
        print(f"warning: {e}", file=sys.stderr)
        print("falling back to the Wireshark manuf file", file=sys.stderr)
        tables = {24: {}, 28: {}, 36: {}}
        text = fetch(WIRESHARK_MANUF, "wireshark_manuf.txt", refresh)
        for bits, key, org in parse_wireshark_manuf(text):
            tables[bits][key] = org
        source_note = [f"Wireshark manuf: {WIRESHARK_MANUF}"]
    return tables, source_note


def build(tables):
    vendor_index = {}
    vendor_rows = []  # (name, tier)
    out = {24: [], 28: [], 36: []}
    counts = {}
    for bits in (24, 28, 36):
        for key, org in tables[bits].items():
            hit = classify(org)
            if not hit:
                continue
            name, tier = hit
            if name not in vendor_index:
                vendor_index[name] = len(vendor_rows)
                vendor_rows.append((name, tier))
            out[bits].append((key, vendor_index[name]))
            counts[name] = counts.get(name, 0) + 1
        out[bits].sort()
    return vendor_rows, out, counts


def render(vendor_rows, out, counts, source_note):
    lines = []
    w = lines.append
    w("#pragma once")
    w("// GENERATED by tools/gen_camera_oui.py from the IEEE OUI registries.")
    w("// Do not edit by hand: change tools/camera_vendors.py and regenerate.")
    w("// Fetched: " + time.strftime("%Y-%m-%d"))
    for s in source_note:
        w("// Source: " + s)
    w(f"// Entries: {len(out[24])} x 24-bit, {len(out[28])} x 28-bit, {len(out[36])} x 36-bit")
    w("//")
    w("// Blocks per vendor:")
    for name, _tier in vendor_rows:
        w(f"//   {name:<12} {counts.get(name, 0)}")
    w("#include <cstddef>")
    w("#include <cstdint>")
    w("")
    w('#include "detection.h"')
    w("")
    w("namespace sh {")
    w("")
    w("struct OuiEntry {")
    w("    uint64_t key;     // the first 24 / 28 / 36 bits of the MAC, right-aligned")
    w("    uint8_t  vendor;  // index into kOuiVendorNames / kOuiVendorTiers")
    w("};")
    w("")
    w("inline constexpr const char* kOuiVendorNames[] = {")
    for name, _tier in vendor_rows:
        w(f'    "{name}",')
    w("};")
    w("")
    w("inline constexpr VendorTier kOuiVendorTiers[] = {")
    for _name, tier in vendor_rows:
        w("    VendorTier::Camera," if tier == CAM else "    VendorTier::Iot,")
    w("};")
    for bits in (24, 28, 36):
        entries = out[bits]
        w("")
        w(f"// {bits}-bit prefixes, sorted by key.")
        w(f"inline constexpr size_t kOui{bits}Count = {len(entries)};")
        w(f"inline constexpr OuiEntry kOui{bits}[] = {{")
        if not entries:
            w("    {0xFFFFFFFFFFFFull, 0},  // placeholder: table is empty")
        width = bits // 4
        row = []
        for key, vendor in entries:
            row.append(f"{{0x{key:0{width}X}ull, {vendor}}}")
            if len(row) == 4:
                w("    " + ", ".join(row) + ",")
                row = []
        if row:
            w("    " + ", ".join(row) + ",")
        w("};")
    w("")
    w("}  // namespace sh")
    w("")
    return "\n".join(lines)


def strip_volatile(text):
    return "\n".join(l for l in text.splitlines() if not l.startswith("// Fetched:"))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--refresh", action="store_true", help="re-download the registries")
    ap.add_argument("--check", action="store_true", help="verify the committed header is current")
    ap.add_argument("--out", default=OUT_PATH, help="output header path")
    args = ap.parse_args()

    tables, source_note = collect(args.refresh)
    vendor_rows, out, counts = build(tables)
    if not vendor_rows:
        print("error: no vendor matched anything; registry download broken?", file=sys.stderr)
        return 2
    text = render(vendor_rows, out, counts, source_note)

    if args.check:
        if not os.path.exists(args.out):
            print(f"check failed: {args.out} does not exist")
            return 1
        with open(args.out, "r", encoding="utf-8") as f:
            current = f.read()
        if strip_volatile(current) != strip_volatile(text):
            print(f"check failed: {args.out} is out of date; run tools/gen_camera_oui.py")
            return 1
        print("check ok")
        return 0

    with open(args.out, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    total = sum(len(v) for v in out.values())
    print(f"wrote {args.out}: {total} prefixes for {len(vendor_rows)} vendors")
    for name, _tier in vendor_rows:
        print(f"  {name:<12} {counts.get(name, 0)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
