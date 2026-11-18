#!/usr/bin/env python3
"""Decode CUTSCENE_OBJECT's ten pointer-to-member STATE arrays from the ROM.

WHAT IT READS, and why each read is there:

  src/__sinit_ov002_02107370.c  the matched constructor, found through the
                                checkout's srcpath (port/tools/decomp_src.py:
                                after the decomp bump it is part of
                                src/game/actors/d_a_demo.cpp). Two source forms:
                                the one-function file assigns every State cell
                                as `ARRAY[i].x = SRC` / `ARRAY[i].y = SRC`
                                (the S16 record is two eight-byte mwcc
                                pointer-to-member cells, `.x` flat index 2i,
                                `.y` 2i+1); the merged TU defines each table
                                with an initializer of `&Class::fn` members
                                (element i is flat cell i) and mwcc emits the
                                SRC records itself, so SRC is the {fn, 0}
                                record in the image whose +0 relocation names
                                fn.
  extracted/overlays/overlay_0002.bin
                                the ROM image. Each source record SRC is read
                                as two words: {function, this-adjustment}.
  config/arm9/overlays/ov002/relocs.txt
                                the relocation the linker applied at SRC+0.
                                The image word and the reloc target must
                                AGREE -- that is the cross-check that says the
                                cell really is a code pointer and the base is
                                right.

WHAT IT PRINTS: one row per flat cell,
    ARRAY  IDX  SRC_RECORD  FN_ADDR  DELTA  SRC_TU  LINKED?
plus a summary of the distinct bodies and the array spans (which must be
contiguous at eight bytes per cell -- a gap would mean a cell nothing fills,
and this tool says so instead of guessing).

  python port/tools/tail2_cells.py <root> [--tsv out.tsv]
"""
import pathlib
import re
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import ovdata  # noqa: E402
import decomp_root  # noqa: E402
import decomp_src  # noqa: E402

SINIT = "src/__sinit_ov002_02107370.c"


ASSIGN = re.compile(
    r"data_ov002_([0-9a-f]{8})\[(\d+)\]\.([xy])\s*=\s*data_ov002_([0-9a-f]{8});")
TABLE = re.compile(r"\b\w+\s+data_ov002_([0-9a-f]{8})\[(\d+)\]\s*=\s*\{([^;]*?)\};", re.S)
MEMBER = re.compile(r"&\s*\w+\s*::\s*(\w+)")


def assign_cells(text):
    """[(array, flat_idx, src)] from the one-function file's `ARRAY[i].x = SRC` form."""
    cells = []
    for m in ASSIGN.finditer(text):
        flat = 2 * int(m.group(2)) + (0 if m.group(3) == "x" else 1)
        cells.append((int(m.group(1), 16), flat, int(m.group(4), 16)))
    return cells


def init_cells(text):
    """[(array, flat_idx, member name)] from tables whose initializer is all `&Class::fn`."""
    cells = []
    for m in TABLE.finditer(text):
        body = m.group(3)
        names = MEMBER.findall(body)
        if not names or len(names) != body.count(",") + 1 - body.rstrip().endswith(","):
            continue
        if int(m.group(2)) != len(names):
            continue
        arr = int(m.group(1), 16)
        cells.extend((arr, i, n) for i, n in enumerate(names))
    return cells


def fn_addr(name, syms_text):
    """The ov002 address of a member function named in source: func_ov002_XXXXXXXX carries it, any other
    name is looked up as a mangled member (<len><name>) in symbols.txt and must be unique."""
    m = re.search(r"func_ov002_([0-9a-f]{8})$", name)
    if m:
        return int(m.group(1), 16)
    hits = re.findall(r"^\S*?(?<!\d)%d%sE\S*\s+kind:function\S*\s+addr:(0x[0-9a-fA-F]+)"
                      % (len(name), re.escape(name)), syms_text, re.M)
    return int(hits[0], 16) if len(set(hits)) == 1 else None


def main():
    root = decomp_root.path(pathlib.Path(
        decomp_root.root_arg(sys.argv[1] if len(sys.argv) > 1 else None)).resolve())
    out_tsv = None
    if "--tsv" in sys.argv:
        out_tsv = pathlib.Path(sys.argv[sys.argv.index("--tsv") + 1])

    base = ovdata.overlay_base(root, 2)
    img = (root / "extracted/overlays/overlay_0002.bin").read_bytes()

    # relocs at each source record's +0 word
    reloc = {}
    for line in (root / "config/arm9/overlays/ov002/relocs.txt").read_text().splitlines():
        m = re.match(r"from:(0x[0-9a-f]+) kind:(\S+) to:(0x[0-9a-f]+)", line)
        if m:
            reloc[int(m.group(1), 16)] = (m.group(2), int(m.group(3), 16))

    rel = decomp_src.resolve(root, SINIT, "ov002", 0x02107370)
    if rel is None:
        sys.exit("no file in %s holds %s (srcpath has no owner for ov002 0x02107370)" % (root, SINIT))
    text = (root / rel).read_text(encoding="utf-8", errors="replace")
    cells = assign_cells(text)
    named = None
    if not cells:
        named = init_cells(text)
        if not named:
            sys.exit("no State assignments or member-pointer tables found in " + rel)

    def word(a):
        off = a - base
        if off < 0 or off + 4 > len(img):
            sys.exit(f"{a:#010x} is outside ov002's image "
                     f"({base:#010x}..{base + len(img):#010x})")
        return struct.unpack_from("<I", img, off)[0]

    bad = 0
    if named is not None:
        syms = (root / "config/arm9/overlays/ov002/symbols.txt").read_text(errors="replace")
        cells = []
        for arr, flat, fname in named:
            fn = fn_addr(fname, syms)
            src = None
            if fn is not None:
                recs = sorted(a for a, (kind, to) in reloc.items()
                              if to == fn and base <= a and a + 8 <= base + len(img)
                              and struct.unpack_from("<I", img, a - base)[0] == fn
                              and struct.unpack_from("<i", img, a + 4 - base)[0] == 0)
                src = recs[0] if recs else None
            if fn is None or src is None:
                print(f"DEFECT: data_ov002_{arr:08x}[{flat}] = &{fname}: "
                      + ("no ov002 symbol by that name" if fn is None
                         else f"no {{fn, 0}} record with a relocation to {fn:#010x} in the image"))
                bad += 1
                continue
            cells.append((arr, flat, src))
    print(f"source: {rel} ({'member-pointer initializers' if named is not None else 'State assignments'})")
    rows = []
    for arr, flat, src in sorted(cells):
        fn = word(src)
        delta = struct.unpack_from("<i", img, src + 4 - base)[0]
        note = ""
        r = reloc.get(src)
        if r is None:
            note = "NO-RELOC"
            bad += 1
        elif r[1] != fn:
            note = f"RELOC-DISAGREES({r[1]:#010x})"
            bad += 1
        if delta != 0:
            note = (note + " ") if note else ""
            note += f"DELTA!={delta}"
            bad += 1
        # a cell whose +4 carries a relocation is not a {fn,0} pair at all
        if (src + 4) in reloc:
            note = (note + " " if note else "") + "PLUS4-RELOCATED"
            bad += 1
        rows.append((arr, flat, src, fn, delta, note))

    # array spans: each array's cells must be 0..n-1 with no gap, and the
    # arrays must tile the bss region without overlapping.
    spans = {}
    for arr, flat, src, fn, delta, note in rows:
        spans.setdefault(arr, set()).add(flat)
    print("ARRAY SPANS (8 bytes per flat cell)")
    order = sorted(spans)
    for k, arr in enumerate(order):
        idxs = spans[arr]
        n = max(idxs) + 1
        gap = sorted(set(range(n)) - idxs)
        end = arr + 8 * n
        nxt = order[k + 1] if k + 1 < len(order) else None
        tile = "" if nxt is None else ("CONTIGUOUS" if end == nxt
                                       else f"GAP/OVERLAP to {nxt:#010x}")
        print(f"  data_ov002_{arr:08x}  {n:3d} cells  "
              f"{arr:#010x}..{end:#010x}  {tile}"
              + (f"  MISSING {gap}" if gap else ""))
        if gap:
            bad += 1

    print()
    print("CELLS")
    print(f"{'array':>10} {'idx':>4} {'src record':>12} {'fn':>12} "
          f"{'delta':>5}  note")
    for arr, flat, src, fn, delta, note in rows:
        print(f"{arr:#010x} {flat:4d} {src:#012x} {fn:#012x} {delta:5d}  {note}")

    bodies = sorted({r[3] for r in rows})
    print()
    print(f"{len(rows)} cells, {len(bodies)} distinct bodies")
    for b in bodies:
        n = sum(1 for r in rows if r[3] == b)
        print(f"  func_ov002_{b:08x}  x{n}")
    print()
    print("DEFECTS:", bad)

    if out_tsv:
        with out_tsv.open("w") as f:
            f.write("array\tidx\tsrc\tfn\tdelta\tnote\n")
            for arr, flat, src, fn, delta, note in rows:
                f.write(f"0x{arr:08x}\t{flat}\t0x{src:08x}\t0x{fn:08x}\t"
                        f"{delta}\t{note}\n")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
