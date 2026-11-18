#!/usr/bin/env python3
"""pmfvalues -- build real host values for the ROM's pointer-to-member constants.

    python port/tools/pmfvalues.py --unresolved <list> --map <map> --out port/hal/pmf_host_values.cpp

THE PROBLEM. Main's class TUs declare ROM state functions as file-scope
pointer-to-member data (`extern PMF data_ov102_0214e278;`) and copy them into
their own State tables in the compiler's static initialiser. The ROM holds each
as an eight-byte pair {code address, this-adjust}, and the port compiles with
/vmg /vmm so that an MSVC pointer to member is the same eight bytes. The
decorated reference (`?data_ov102_0214e278@@3P8C@@AEXXZQ1@`) therefore had
nothing to bind to except the mount's ROM bytes, whose code word is a DS address:
the first call through it jumps into mapped data.

THE ANSWER. Define the constant natively. Each row here reads the pair out of
the cartridge, names the function its code word points at (config symbols), and
emits {&host function, 0} under the decorated name:

  * a MEMBER target (`_ZN7daCnn_c8StateAimEv`): the host definition
    is the TU's own thiscall member; the pair holds its address, found in the
    link map by class and method name;
  * a FLAT target (`_ZN11daKpaFire_c19func_ov060_021169b0Ev`, receiver as the first cdecl
    argument): the pair holds a generated __fastcall thunk of the PMF's arity,
    which forwards (this, args...) to the cdecl body. A __fastcall callee with
    (this, dummy, a1..an) is entered exactly like a thiscall member with n
    stack arguments, and pops them the same way.

The values are link-time constants, so a TU's static initialiser can copy them
before main() and a call through them lands on host code, with no seam at the
call site. An adjustment word other than zero, a target with no host
definition, or an overloaded member name refuses the row and says so.

Pointer-to-PMF arrays (`P8...` behind a second `*`) are not handled here; they
are tables the ROM's own sinit fills and need the table's owner.
"""
import argparse
import os
import pathlib
import re
import struct
import sys
import decomp_root  # noqa: E402

HERE = pathlib.Path(__file__).resolve().parent
REPO = decomp_root.path(decomp_root.find())
sys.path.insert(0, str(HERE))

SYM_RE = re.compile(r"^(\S+)\s+kind:(function|data|bss)(?:\(([^)]*)\))?\s+addr:0x([0-9a-fA-F]+)")
SEC_RE = re.compile(r"^\s+(\S+)\s+start:0x([0-9a-fA-F]+)\s+end:0x([0-9a-fA-F]+)\s+kind:(\w+)")


def load_syms(path):
    out = {}
    p = pathlib.Path(path)
    if not p.is_file():
        return out
    for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
        m = SYM_RE.match(line.strip())
        if m and m.group(2) == "function":
            out.setdefault(int(m.group(4), 16), m.group(1))
    return out


class Overlay:
    def __init__(self, n):
        self.n = n
        dl = REPO / ("config/arm9/overlays/ov%03d/delinks.txt" % n)
        secs = []
        for line in dl.read_text(encoding="utf-8-sig", errors="replace").splitlines():
            if line and not line.startswith(" "):
                break
            m = SEC_RE.match(line)
            if m:
                secs.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16), m.group(4)))
        infile = [s for s in secs if s[3] != "bss"]
        self.base = infile[0][1]
        self.image = (REPO / ("extracted/overlays/overlay_%04d.bin" % n)).read_bytes()
        self.syms = load_syms(REPO / ("config/arm9/overlays/ov%03d/symbols.txt" % n))

    def word(self, addr):
        off = addr - self.base
        if not 0 <= off <= len(self.image) - 4:
            return None
        return struct.unpack_from("<I", self.image, off)[0]


# ---- named constants (pmf_*) -----------------------------------------------------------------
#
# Main's later TUs name a pointer-to-member constant after its target (pmf_dScMgBomroom_c_
# BombMarch_LinedUp, pmf_Heyho_MotionChase_2) instead of its address, so the address comes from
# the config row of that name. The list is every UNDEF pmf_* the compiled objects ask for
# (`dumpbin /symbols`), and the same listing supplies the host definitions, so no link is needed.
# A decorated reference carries its pointer-to-member type; a C-linkage one does not, and its
# type is the TU's own `extern TYPE name;` with TYPE a pointer-to-member typedef. Anything else
# (a raw record the TU copies as words, a typedef of another arity) keeps ROM bytes and is
# skipped with its reason. Rows come out per TU in name order (TUs in name order); a name an
# earlier TU or a kept row already binds is not emitted twice.

DUMP_FILE_RE = re.compile(r"^Dump of file (.+)$")
DUMP_SYM_RE = re.compile(r"^[0-9A-F]+ [0-9A-F]{8} (SECT[0-9A-F]+|UNDEF)\s.*\|\s(\S+)")
PMF_DEC_RE = re.compile(r"^\?(pmf_\w+)@@3P8(\w+)@@AE(_N|[A-Z])(X|(?:_N|[A-Z])+@)ZQ1@$")
MEMBER_DEC_RE = re.compile(r"^\?(\w+)@(\w+)@@QAE(_N|[A-Z])(X|(?:_N|[A-Z])+@)Z$")
SIMPLE_CODES = re.compile(r"_N|[DEFGHIJKMN]")


def code_arity(params):
    """Parameter count of an MSVC parameter list code (X = void), or None for a type this
    generator does not spell (pointers, classes)."""
    if params == "X":
        return 0
    body = params[:-1]
    codes = SIMPLE_CODES.findall(body)
    return len(codes) if "".join(codes) == body else None


def read_dumpbin(text):
    """(undefs, defs) from `dumpbin /symbols` over the compiled objects: undefs is a list of
    (TU stem, spelling) for every UNDEF pmf_* reference, defs the External names some object
    defines."""
    undefs, defs, tu = [], set(), None
    for line in text.splitlines():
        m = DUMP_FILE_RE.match(line.strip())
        if m:
            base = re.split(r"[\\/]", m.group(1).strip())[-1]
            tu = re.sub(r"\.(c|cpp)\.obj$|\.obj$", "", base, flags=re.I)
            continue
        m = DUMP_SYM_RE.match(line)
        if not m or "External" not in line:
            continue
        spelling = m.group(2)
        if m.group(1) == "UNDEF":
            core = spelling[1:spelling.index("@@")] if spelling.startswith("?") and "@@" in spelling else spelling.lstrip("_")
            if core.startswith("pmf_"):
                undefs.append((tu, spelling))
        else:
            defs.add(spelling)
    return undefs, defs


def tu_source(src_root, tu):
    hits = sorted(pathlib.Path(src_root, "src").rglob(tu + ".c*"))
    hits = [h for h in hits if h.suffix in (".c", ".cpp")]
    return hits[0] if len(hits) == 1 else None


def c_linkage_type(src_text, name):
    """(class, arity) of a C-linkage constant the TU declares `extern TYPE name;` with TYPE a
    pointer-to-member typedef, or (None, reason)."""
    m = re.search(r"\bextern\s+(?:const\s+)?(\w+)\s+" + re.escape(name) + r"\s*;", src_text)
    if not m:
        return None, "declared None"
    tname = m.group(1)
    t = (re.search(r"\btypedef\s+\w+\s*\(\s*(\w+)::\*\s*" + re.escape(tname) + r"\s*\)\s*\(([^)]*)\)\s*;", src_text)
         or re.search(r"\busing\s+" + re.escape(tname) + r"\s*=\s*\w+\s*\(\s*(\w+)::\*\s*\)\s*\(([^)]*)\)\s*;", src_text))
    if not t:
        return None, "declared %s" % tname
    params = t.group(2).strip()
    arity = 0 if params in ("", "void") else params.count(",") + 1
    if arity > 1:
        return None, "declared %s" % tname
    return (t.group(1), arity), None


def itanium_parts(sym):
    """Nested name parts of an Itanium function symbol (_ZN5Heyho6MotionEv -> [Heyho, Motion],
    _Z15Slot3_StateDonev -> [Slot3_StateDone])."""
    i = 3 if sym.startswith("_ZN") else 2 if sym.startswith("_Z") else None
    if i is None:
        return []
    parts = []
    while i < len(sym) and sym[i].isdigit():
        j = i
        while sym[j].isdigit():
            j += 1
        ln = int(sym[i:j])
        parts.append(sym[j:j + ln])
        i = j + ln
        if not sym.startswith("_ZN"):
            break
    return parts


def named_rows(undefs, defs, bound, find_addr, word_at, target_name, src_root):
    """Rows for the pmf_* references: (tag, spelling, code, target, kind, host, pmf arity)."""
    members = {}
    for d in defs:
        m = MEMBER_DEC_RE.match(d)
        if m:
            members.setdefault((m.group(2), m.group(1)), []).append(d)
    seen, rows, refused = set(bound), [], []
    order = sorted(undefs, key=lambda u: (u[0] or "", u[1][1:u[1].index("@@")] if u[1].startswith("?") else u[1].lstrip("_")))
    sources = {}
    for tu, spelling in order:
        dec = spelling.startswith("?")
        name = spelling[1:spelling.index("@@")] if dec else spelling.lstrip("_")
        if name in seen:
            continue
        seen.add(name)
        if dec:
            m = PMF_DEC_RE.match(spelling)
            arity = code_arity(m.group(4)) if m else None
            if arity is None or arity > 1:
                refused.append((name, "raw record or untyped reference %s" % spelling))
                continue
        else:
            if tu not in sources:
                path = tu_source(src_root, tu)
                sources[tu] = path.read_text(encoding="utf-8", errors="replace") if path else ""
            typed, why = c_linkage_type(sources[tu], name)
            if typed is None:
                refused.append((name, "C linkage, %s (not a pointer-to-member typedef of arity 0/1)" % why))
                continue
            arity = typed[1]
        loc = find_addr(name)
        if loc is None:
            refused.append((name, "no config symbol of that name"))
            continue
        n, addr = loc
        code, adj = word_at(n, addr), word_at(n, addr + 4)
        if code is None or adj is None:
            refused.append((name, "address outside the overlay file image (a .bss table)"))
            continue
        if adj != 0:
            refused.append((name, "nonzero this-adjust 0x%08x" % adj))
            continue
        tgt = target_name(n, code)
        if not tgt:
            refused.append((name, "no function symbol at 0x%08x" % code))
            continue
        parts = itanium_parts(tgt)
        kind = host = None
        if tgt.startswith("_ZN") and len(parts) == 2:
            cands = members.get((parts[0], parts[1]), [])
            if len(cands) > 1:
                refused.append((name, "member %s::%s is overloaded: %s" % (parts[0], parts[1], cands)))
                continue
            if cands:
                marity = code_arity(MEMBER_DEC_RE.match(cands[0]).group(4))
                if marity == arity:
                    kind, host = "member", cands[0]
                elif marity == 0 and arity == 1:
                    kind, host = "drop1", cands[0]
                else:
                    refused.append((name, "member %s takes %s arguments, the table passes %d" % (cands[0], marity, arity)))
                    continue
        if kind is None and arity == 0:
            flat = ["_" + tgt] if ("_" + tgt) in defs else []
            if not flat and len(parts) == 1:
                flat = [d for d in defs if d.startswith("?%s@@YA" % parts[0])]
            if len(flat) == 1:
                kind, host = "flat", flat[0]
        if kind is None:
            refused.append((name, "no host definition of %s" % tgt))
            continue
        rows.append((name, spelling, code, tgt, kind, host, arity))
    return rows, refused


def emit_named(rows):
    lines = []
    w = lines.append
    for tag, spelling, code, tgt, kind, host, arity in rows:
        if kind == "member":
            w('extern "C" void port_pmft_%s(void);' % tag)
            w('#pragma comment(linker, "/alternatename:_port_pmft_%s=%s")' % (tag, host))
            value = "(unsigned)&port_pmft_%s" % tag
        elif kind == "drop1":
            w('extern "C" void __fastcall port_pmfm_%s(void *self, void *edx);' % tag)
            w('#pragma comment(linker, "/alternatename:@port_pmfm_%s@8=%s")' % (tag, host))
            w("static void __fastcall port_pmfthunk_%s(void *self, void *, int)" % tag)
            w("{ port_pmfm_%s(self, 0); }  /* the member takes no argument; the table passes one */" % tag)
            value = "(unsigned)&port_pmfthunk_%s" % tag
        else:
            w('extern "C" void port_pmff_%s(void *self);' % tag)
            w('#pragma comment(linker, "/alternatename:_port_pmff_%s=%s")' % (tag, host))
            w("static void __fastcall port_pmfthunk_%s(void *self, void *)" % tag)
            w("{ port_pmff_%s(self); }  /* flat body, receiver first (cdecl: extra arguments are the caller's to pop) */" % tag)
            value = "(unsigned)&port_pmfthunk_%s" % tag
        w('extern "C" PortPmfPair port_pmfv_%s = { %s, 0 };  /* ROM 0x%08x %s */' % (tag, value, code, tgt))
        w('#pragma comment(linker, "/alternatename:%s=_port_pmfv_%s")' % (spelling, tag))
        w("")
    return lines


NAMED_SECTION = """/* The named constants (pmf_*): main's TUs that read pointer-to-member state constants
 * by name rather than address. Same rule as above, keyed on the config symbol of that
 * name; the list is every UNDEF pmf_* the compiled TUs ask for (dumpbin), regenerated by
 * pmfvalues.py --append --symbols. Three shapes:
 *   member  {&member, 0} straight;
 *   drop1   the ROM state function takes no argument while its table passes
 *           one: a __fastcall thunk of the table's arity pops it;
 *   flat    a free function taking the receiver first: a __fastcall thunk
 *           forwards (this) to the cdecl body.
 * Raw records (pmf_* the TUs declare as structs, ints or words and copy as
 * data) are not here: they stay ROM bytes.
 */
"""
SECTION_MARKERS = ("/* catchup1 PLY4: ", "/* The named constants (pmf_*)")
ROWS_LINE_RE = re.compile(r"^( \* file for the rule\. )(\d+)( rows: )", re.M)


def split_kept(text):
    """(kept head with the section comment, names bound in the head) of an existing
    pmf_host_values.cpp; without a section comment the whole file is the head and the
    section comment is appended."""
    starts = [text.find(m) for m in SECTION_MARKERS if m in text]
    if starts:
        start = min(starts)
        end = text.index("*/", start) + 2
        head = text[:end] + "\n\n"
        body_bound = text[:start]
    else:
        head = text.rstrip("\n") + "\n\n" + NAMED_SECTION + "\n"
        body_bound = text
    bound = set(re.findall(r"PortPmfPair port_pmfv_(\w+) =", body_bound))
    return head, bound


def append_mode(a):
    """Regenerate the named section of --out from --symbols, keeping every row before it."""
    out = pathlib.Path(a.out)
    head, bound = split_kept(out.read_text(encoding="utf-8"))
    undefs, defs = read_dumpbin(pathlib.Path(a.symbols).read_text(encoding="utf-8", errors="replace"))
    arm9 = load_syms(REPO / "config/arm9/symbols.txt")
    ovs = {}

    def ov(n):
        if n not in ovs:
            ovs[n] = Overlay(n)
        return ovs[n]

    names = {}
    for p in sorted((REPO / "config/arm9/overlays").glob("ov*/symbols.txt")):
        n = int(p.parent.name[2:])
        for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
            m = SYM_RE.match(line.strip())
            if m and m.group(1).startswith("pmf_"):
                names.setdefault(m.group(1), []).append((n, int(m.group(4), 16)))

    def find_addr(name):
        hits = names.get(name, [])
        return hits[0] if len(hits) == 1 else None

    def target_name(n, code):
        for k in (n, 2, 4, 6, 9):
            try:
                t = ov(k).syms.get(code)
            except OSError:
                continue
            if t:
                return t
        return arm9.get(code)

    rows, refused = named_rows(undefs, defs, bound, find_addr, lambda n, addr: ov(n).word(addr),
                               target_name, decomp_root.path(a.src_root) if a.src_root else REPO)
    total = len(re.findall(r"PortPmfPair port_pmfv_\w+ =", head)) + len(rows)
    head = ROWS_LINE_RE.sub(lambda m: m.group(1) + str(total) + m.group(3), head, count=1)
    out.write_text(head + "\n".join(emit_named(rows)), encoding="utf-8", newline="\n")
    print("pmfvalues --append: %d named rows (%d member, %d drop1, %d flat), %d in the file -> %s" % (
        len(rows), sum(r[4] == "member" for r in rows), sum(r[4] == "drop1" for r in rows),
        sum(r[4] == "flat" for r in rows), total, out))
    for name, why in refused:
        print("  skipped: %s (%s)" % (name, why))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--unresolved")
    ap.add_argument("--map")
    ap.add_argument("--out", required=True)
    ap.add_argument("--append", action="store_true",
                    help="regenerate only the named (pmf_*) section of --out from --symbols")
    ap.add_argument("--symbols", help="dumpbin /symbols over the compiled objects (with --append)")
    ap.add_argument("--src-root", help="decomp root whose src/ declares the C-linkage constants")
    a = ap.parse_args()
    if a.append:
        if not a.symbols:
            ap.error("--append needs --symbols")
        return append_mode(a)
    if not (a.unresolved and a.map):
        ap.error("--unresolved and --map are required without --append")

    map_txt = open(a.map, encoding="utf-8", errors="replace").read()
    defs = set(re.findall(r"\s(_[\w$@?]+|\?[\w$@?]+)\s+[0-9a-f]{8}\s", map_txt))
    arm9 = load_syms(REPO / "config/arm9/symbols.txt")
    ovs = {}

    def ov(n):
        if n not in ovs:
            ovs[n] = Overlay(n)
        return ovs[n]

    def target_name(n, code):
        for k in (n, 2, 4, 6, 9):
            try:
                t = ov(k).syms.get(code)
            except OSError:
                continue
            if t:
                return t
        return arm9.get(code)

    members = {}
    for d in defs:
        m = re.match(r"\?(\w+)@(\w+)@@", d)
        if m:
            members.setdefault((m.group(2), m.group(1)), []).append(d)

    rows = []
    refused = []
    for line in open(a.unresolved, encoding="utf-8", errors="replace"):
        line = line.split("  <- ")[0].strip()
        if "::*" not in line or "::**" in line:
            continue
        mm = re.search(r"\* (data_ov(\d+)_([0-9a-f]{8}))\)\((.*?)\)\"\s*\((\?[\w$@?]+)\)$", line)
        if not mm:
            refused.append((line[:90], "unparsed"))
            continue
        name, n, addr = mm.group(1), int(mm.group(2)), int(mm.group(3), 16)
        params, dec = mm.group(4).strip(), mm.group(5)
        arity = 0 if params in ("", "void") else params.count(",") + 1
        o = ov(n)
        code, adj = o.word(addr), o.word(addr + 4)
        if code is None or adj is None:
            refused.append((name, "address outside the overlay file image (a .bss table)"))
            continue
        if adj != 0:
            refused.append((name, "nonzero this-adjust 0x%08x" % adj))
            continue
        tgt = target_name(n, code)
        if not tgt:
            refused.append((name, "no function symbol at 0x%08x" % code))
            continue
        kind = None
        host = None
        if tgt.startswith("_ZN"):
            i, parts = 3, []
            while i < len(tgt) and tgt[i].isdigit():
                j = i
                while tgt[j].isdigit():
                    j += 1
                ln = int(tgt[i:j])
                parts.append(tgt[j:j + ln])
                i = j + ln
            if len(parts) == 2:
                cands = members.get((parts[0], parts[1]), [])
                if len(cands) == 1:
                    kind, host = "member", cands[0]
                elif len(cands) > 1:
                    refused.append((name, "member %s::%s is overloaded in the link: %s" % (parts[0], parts[1], cands)))
                    continue
            if kind is None and ("_" + tgt) in defs:
                kind, host = "flat", "_" + tgt
            if kind is None and ("__" + tgt[1:]) in defs:
                kind, host = "flat", "__" + tgt[1:]
            if kind is None:
                refused.append((name, "no host definition of %s" % tgt))
                continue
        else:
            if ("_" + tgt) in defs:
                kind, host = "flat", "_" + tgt
            else:
                refused.append((name, "no host definition of %s" % tgt))
                continue
        rows.append((name, dec, code, tgt, kind, host, arity))

    lines = []
    w = lines.append
    w("/* hal/pmf_host_values.cpp -- host values for the ROM's pointer-to-member constants.")
    w(" *")
    w(" * PORTSYNC3. GENERATED by port/tools/pmfvalues.py, then checked in. Read that")
    w(" * file for the rule. %d rows: each is a ROM {code, this-adjust} pair rebuilt as" % len(rows))
    w(" * {host function, 0}. A flat (cdecl) target gets a __fastcall thunk of the")
    w(" * PMF's arity. Includes nothing, for hal/actor_class_faces.cpp's reasons.")
    w(" */")
    w("")
    w("struct PortPmfPair { unsigned code; int adj; };")
    w("")
    for name, dec, code, tgt, kind, host, arity in rows:
        tag = name
        if kind == "member":
            w('extern "C" void port_pmft_%s(void);' % tag)
            w('#pragma comment(linker, "/alternatename:_port_pmft_%s=%s")' % (tag, host))
            tgt_expr = "(unsigned)&port_pmft_%s" % tag
        else:
            args = "".join(", int a%d" % i for i in range(1, arity + 1))
            call = "".join(", a%d" % i for i in range(1, arity + 1))
            w('extern "C" int %s_decl_%s(void *self%s);' % ("port_pmff", tag, "".join(", int" for _ in range(arity))))
            w('#pragma comment(linker, "/alternatename:_port_pmff_decl_%s=%s")' % (tag, host))
            w("static int __fastcall port_pmfthunk_%s(void *self, void *%s)" % (tag, args))
            w("{ return port_pmff_decl_%s(self%s); }" % (tag, call))
            tgt_expr = "(unsigned)&port_pmfthunk_%s" % tag
        w('extern "C" PortPmfPair port_pmfv_%s = { %s, 0 };  /* ROM 0x%08x %s */' % (tag, tgt_expr, code, tgt))
        w('#pragma comment(linker, "/alternatename:%s=_port_pmfv_%s")' % (dec, tag))
        w("")
    pathlib.Path(a.out).write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print("pmfvalues: %d rows (%d member, %d flat) -> %s" % (
        len(rows), sum(1 for r in rows if r[4] == "member"), sum(1 for r in rows if r[4] == "flat"), a.out))
    for name, why in refused:
        print("  refused: %s (%s)" % (name, why))


if __name__ == "__main__":
    main()
