#!/usr/bin/env python3
"""mount_reconcile -- settle ROM data that the port hosts twice, by ROM address.

    python port/tools/mount_reconcile.py --map <link.map> --log <link.log> [--log ...]
        --build <build dir> [--apply] [--report <dir>]
    python port/tools/mount_reconcile.py --check [--aliases port/hal/cxx_data_aliases_gen.cpp]

The ROM mounts (port/ov*_syms.txt through tools/ovdata.py) define ROM data
under its C name. Main's decomp TUs now define much of the same data
themselves, often with C++ linkage, and reference the rest with C++ linkage.
Three symptoms, one cause:

  LNK4006/LNK2005  a mount row and a linked decomp TU define the same name;
  two storages     a TU defines ?X@@3... and a mount (or port file) defines _X,
                   so code on either side reads a different copy;
  LNK2001/LNK2019  a TU references ?X@@3... and only the C mount name exists.

Every name is turned into a ROM address before anything is decided: a mount
row's address comes from the mount's own .c.map (ovdata's "provides"), every
other name from the config/**/symbols.txt tables, through its C name or, for
an MSVC decorated name, the Itanium spelling of its scope chain. Two names are
only ever tied together when they land on the same (module, address). Nothing
is matched by spelling alone.

Per ROM address the decomp TU's copy wins:
  * a mount row at an address a linked decomp TU defines is dropped from its
    list (--apply edits port/ov*_syms.txt), and when the TU's spelling differs
    the C name is aliased onto it so C references follow;
  * an unresolved reference whose address has exactly one storage gets an
    /alternatename row onto that storage, written to
    port/hal/cxx_data_aliases_gen.cpp;
  * anything else (a port-side copy such as romdata.c or a hal table at a TU's
    address, two TUs at one address, an address no table knows) is listed in
    unsettled.tsv with the owner of the port-side copy.

--check reads the generated file back and fails on any row whose two names do
not share a ROM address, on a left hand side aliased twice, and on a left hand
side some other alias file already aliases elsewhere. It needs only the tree.

Pointer-to-member data (P8 in the decoration) is left alone: hal/pmf_host_values.cpp
gives those host values by design.
"""
import argparse
import json
import os
import re
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import decomp_root  # noqa: E402

PORT = HERE.parent
# The decomp checkout (config/), found the way every port tool finds it; joining it with
# port/... leads into the port's own tree (decomp_root.path).
ROOT = decomp_root.path(decomp_root.find())
GEN = PORT / "hal" / "cxx_data_aliases_gen.cpp"
BY_DESIGN_OBJS = {"pmf_host_values.cpp.obj"}

# ---------------------------------------------------------------- tables


def load_symbols(config):
    """name -> {(module, addr)}; (module, addr) -> {name}."""
    by_name = defaultdict(set)
    by_addr = defaultdict(set)
    for p in Path(config).rglob("symbols.txt"):
        parts = p.relative_to(config).parts
        if "overlays" in parts:
            mod = parts[parts.index("overlays") + 1]
        else:
            mod = parts[-2] if len(parts) > 1 else "arm9"
        for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
            f = line.split()
            if len(f) < 3:
                continue
            m = re.search(r"\baddr:0x([0-9a-fA-F]+)", line)
            if not m or "kind:function" in line or "kind:label" in line:
                continue
            key = (mod, int(m.group(1), 16))
            by_name[f[0]].add(key)
            by_addr[key].add(f[0])
    return by_name, by_addr


def load_mounts(host_src):
    """mount object -> {"mod", "rows": {name: (mod, addr, size)}}."""
    mounts = {}
    for p in Path(host_src).glob("*.c.map"):
        try:
            d = json.loads(p.read_text(encoding="utf-8"))
        except ValueError:
            continue
        if "provides" not in d:
            continue
        mod = d.get("overlay", "arm9")
        rows = {}
        for addr, size, name in d["provides"]:
            rows[name] = (mod, addr, size)
        mounts[p.name[:-len(".map")] + ".obj"] = {"mod": mod, "rows": rows,
                                                  "stem": p.name[:-len(".c.map")]}
    return mounts


def load_lists(build, mounts):
    """mount object -> list file (repo-relative) from build.ninja's --from-list."""
    txt = (Path(build) / "build.ninja").read_text(encoding="utf-8", errors="replace")
    out = {}
    for m in re.finditer(r"ovdata\.py (\S+) (\S+) --from-list (\S+)", txt):
        obj = Path(m.group(2)).name + ".obj"
        lst = Path(m.group(3).strip('"')).as_posix()
        i = lst.find("/port/")
        out[obj] = lst[i + 1:] if i >= 0 else lst
    return {k: v for k, v in out.items() if k in mounts}


def load_objects(build):
    """object basename -> {source path relative to port/ (posix)} for the slice."""
    txt = (Path(build) / "build.ninja").read_text(encoding="utf-8", errors="replace")
    out = defaultdict(set)
    for m in re.finditer(r"^build CMakeFiles\\[^\\\s]+\.dir\\(\S+?\.obj):", txt, re.M):
        rel = m.group(1).replace("\\", "/")
        out[Path(rel).name].add(rel[:-len(".obj")])
    return out


def load_owners():
    members = Path(os.environ.get("MOUNT_RECONCILE_MEMBERS", ""))
    owners = {}
    if members.is_dir():
        for p in members.glob("*.txt"):
            if p.name.startswith("_"):
                continue
            for line in p.read_text(encoding="utf-8").splitlines():
                owners[line.strip()] = p.stem
    return owners


def load_map(path):
    """name -> {object} for every public outside the code section, and name -> host address."""
    defs = defaultdict(set)
    rva = {}
    pub = re.compile(r"^\s*([0-9a-f]{4}):[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s+(?:f\s+)?(?:i\s+)?(\S+)\s*$")
    on = False
    for line in Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
        if "Publics by Value" in line:
            on = True
            continue
        if on and line.startswith(" entry point"):
            break
        if not on:
            continue
        m = pub.match(line)
        if m and m.group(1) not in ("0000", "0001"):
            defs[m.group(2)].add(m.group(4).split(":")[-1])
            rva[m.group(2)] = m.group(3)
    return defs, rva


def load_logs(paths):
    dup = []          # (name, second obj, first obj)
    unres = defaultdict(set)
    r4006 = re.compile(r"^(\S+) : warning LNK4006: (\S+) already defined in (\S+); second definition ignored")
    r2001 = re.compile(r'^(\S+) : error LNK20(?:01|19): unresolved external symbol (?:"[^"]*" \((\S+?)\)|(\S+))')
    for p in paths:
        for line in Path(p).read_text(encoding="utf-8", errors="replace").splitlines():
            m = r4006.match(line)
            if m:
                dup.append((m.group(2), m.group(1), m.group(3)))
                continue
            m = r2001.match(line)
            if m:
                unres[m.group(2) or m.group(3)].add(m.group(1))
    return dup, unres


def load_existing_aliases(skip):
    """LHS -> [(RHS, where)] from every live /alternatename in port/hal."""
    out = defaultdict(list)
    rx = re.compile(r'^\s*#pragma comment\(linker,\s*"/alternatename:([^=]+)=([^"]+)"\)')
    for p in sorted((PORT / "hal").rglob("*")):
        if p.suffix not in (".c", ".cpp", ".h", ".inc") or p.resolve() == Path(skip).resolve():
            continue
        for n, line in enumerate(p.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            m = rx.match(line)
            if m:
                out[m.group(1)].append((m.group(2), f"{decomp_root.relpath(str(p), str(ROOT)).replace(os.sep, "/")}:{n}"))
    return out

# ---------------------------------------------------------------- names


def _frag_chain(s):
    """'name@scope1@scope2@@rest' -> ([name, scope1, scope2], rest) or None."""
    end = s.find("@@")
    if end < 0:
        return None
    frags = s[:end].split("@")
    if any(not f or not re.fullmatch(r"[A-Za-z_$][A-Za-z0-9_$]*", f) for f in frags):
        return None
    return frags, s[end + 2:]


def pin_addr(name):
    """A pin-era address name (data_02086b58, data_ov045_021131d8) carries its
    own ROM address, the way ovdata resolves it; main may have renamed the
    symbol since, so symbols.txt no longer lists the old spelling."""
    m = re.fullmatch(r"data_(?:(ov\d{3})_)?([0-9a-f]{8})", name)
    return {(m.group(1) or "arm9", int(m.group(2), 16))} if m else set()


def keys_for(host):
    """Host symbol -> (symbols.txt keys, is_data, is_pmf). Decorated functions give no keys."""
    if host.startswith("?"):
        if host.startswith("??"):
            return [], False, False
        r = _frag_chain(host[1:])
        if not r:
            return [], False, False
        frags, rest = r
        if not rest or rest[0] not in "0123":
            return [], False, False
        pmf = rest[1:3] == "P8" or "P8" in rest[:4]
        name, scopes = frags[0], frags[1:]
        keys = []
        if not scopes:
            keys.append(name)
        else:
            keys.append("_ZN" + "".join(f"{len(s)}{s}" for s in reversed(scopes))
                        + f"{len(name)}{name}E")
        return keys, True, pmf
    if host.startswith("_"):
        return [host[1:]], True, False
    return [], False, False


def c_host(name):
    return "_" + name


class Tables:
    def __init__(self, config, mounts):
        self.by_name, self.by_addr = load_symbols(config)
        self.mounts = mounts
        self.mount_by_name = defaultdict(set)
        for obj, m in mounts.items():
            for n, (mod, addr, size) in m["rows"].items():
                self.mount_by_name[n].add((mod, addr))

    def addrs(self, host, obj=None):
        """ROM addresses a host symbol stands for."""
        if obj in self.mounts and host.startswith("_"):
            row = self.mounts[obj]["rows"].get(host[1:])
            if row:
                return {(row[0], row[1])}
        keys, _, _ = keys_for(host)
        out = set()
        for k in keys:
            out |= self.by_name.get(k, set())
            out |= self.mount_by_name.get(k, set())
            if not out:
                out |= pin_addr(k)
        return out

# ---------------------------------------------------------------- analysis


def decomp_src(s):
    """'src/...' when an object source is a decomp TU (hostgen copy, patch
    overlay, or the decomp tree compiled in place), else None."""
    for pre in ("host-src/", "decomp-overlay/"):
        if s.startswith(pre + "src/"):
            return s[len(pre):]
    if s.startswith("C_/") and "/port/" not in s:
        i = s.find("/src/")
        if i >= 0:
            return s[i + 1:]
    return None


def classify(obj, mounts, objects):
    if obj in mounts:
        return "mount"
    srcs = objects.get(obj)
    if not srcs:
        return "lib"
    if all(decomp_src(s) for s in srcs):
        return "tu"
    return "port"


def owner_of(obj, objects, owners):
    for s in sorted(objects.get(obj, ())):
        if decomp_src(s):
            return owners.get(decomp_src(s), "?")
        if s.startswith("host-src/"):
            stem = Path(s).name.split(".")[0]
            for cand in (f"port/tools/{stem}.py",):
                if cand in owners:
                    return owners[cand]
            return "?"
        return owners.get("port/" + s, "?")
    return "?"


def analyse(a):
    mounts = load_mounts(Path(a.build) / "host-src")
    lists = load_lists(a.build, mounts)
    objects = load_objects(a.build)
    owners = load_owners()
    t = Tables(a.config, mounts)
    defs, rva = load_map(a.map)
    existing = load_existing_aliases(GEN)
    # A fired /alternatename publishes its left hand side at the right hand
    # side's host address; that is one storage, not two.
    fired = {lhs for lhs, rows in existing.items() if lhs in rva
             and any(rva.get(rhs) == rva[lhs] for rhs, _ in rows)}
    for lhs in fired:
        defs.pop(lhs, None)
    dup, unres = load_logs(a.log)
    for name, second, first in dup:
        defs[name].add(second)
        defs[name].add(first)

    # storages per ROM address
    store = defaultdict(lambda: defaultdict(set))   # addr -> kind -> {(name, obj)}
    ambiguous = []
    for name, objs in defs.items():
        keys, is_data, pmf = keys_for(name)
        if not is_data:
            continue
        for obj in objs:
            if obj in BY_DESIGN_OBJS:
                continue
            kind = classify(obj, mounts, objects)
            ad = t.addrs(name, obj)
            if not ad:
                continue
            if len(ad) > 1:
                # several modules carry this name: keep only addresses some other storage holds
                ambiguous.append((name, obj, ad))
                continue
            store[next(iter(ad))][kind].add((name, obj))
    for name, obj, ad in ambiguous:
        hit = [x for x in ad if x in store]
        if len(hit) == 1:
            store[hit[0]][classify(obj, mounts, objects)].add((name, obj))

    # --add-mount: a symbols.txt name no storage hosts yet becomes a row of
    # its module's list mount, unless a linked TU already holds its address.
    adds = []
    for name in a.add_mount:
        ad = t.by_name.get(name, set())
        if len(ad) != 1:
            print(f"add-mount: {name} has {len(ad)} addresses in symbols.txt; skipped")
            continue
        addr = next(iter(ad))
        if store.get(addr, {}).get("tu"):
            print(f"add-mount: {name} is defined by a linked TU; not mounted")
            continue
        if store.get(addr, {}).get("mount"):
            have = sorted(n for n, _ in store[addr]["mount"])
            print(f"add-mount: {name} shares its address with mount row {have[0]}; aliased, not mounted")
            continue
        mobj = next((o for o, m in mounts.items() if m["mod"] == addr[0] and o in lists), None)
        if not mobj:
            print(f"add-mount: no list mount for {addr[0]}; {name} skipped")
            continue
        store[addr]["mount"].add(("_" + name, mobj))
        adds.append((lists[mobj], name))

    drops = []      # (list file, mount name, addr, winner, winner obj)
    aliases = {}    # lhs -> (rhs, addr, why)
    unsettled = []  # (class, addr, names, port obj, owner, note)
    retired = []    # (port obj, name, addr, winner): definitions --apply removes
    list_tokens = {}
    winner = {}
    for addr, kinds in store.items():
        tus = sorted(kinds.get("tu", ()))
        mts = sorted(kinds.get("mount", ()))
        prt = sorted(kinds.get("port", ()))
        if len({n for n, _ in tus}) > 1 or len({o for _, o in tus}) > 1:
            unsettled.append(("two decomp TUs", addr, [n for n, _ in tus], "", "SLICE4",
                              "two linked decomp TUs define one ROM address"))
        if tus:
            win = tus[0]
            winner[addr] = win[0]
            for mname, mobj in mts:
                lst = lists.get(mobj)
                if not lst:
                    unsettled.append(("mount without list", addr, [mname, win[0]], mobj, "?",
                                      "whole-overlay mount; cannot drop one row"))
                    continue
                if lst not in list_tokens:
                    p = ROOT / lst
                    list_tokens[lst] = {re.split(r"[@:!]", x)[0] for x in
                                        p.read_text(encoding="utf-8").split()} if p.is_file() else set()
                if mname[1:] not in list_tokens[lst]:
                    unsettled.append(("mount row not in its list", addr, [mname, win[0]], mobj,
                                      "SYM4", f"ovdata emits {mname[1:]} (a gap filler or a "
                                      f"renamed row) though {lst} does not name it; the TU's "
                                      f"definition overlaps mount bytes"))
                    continue
                drops.append((lst, mname[1:], addr, win[0], win[1]))
                if mname != win[0]:
                    aliases[mname] = (win[0], addr, "dropped mount row; C name follows the TU")
            for pname, pobj in prt:
                if pname == win[0] and pobj == win[1]:
                    continue
                if pobj in a.retire:
                    retired.append((pobj, pname, addr, win[0]))
                    if pname != win[0]:
                        aliases[pname] = (win[0], addr, f"retired port copy in {pobj}; C name follows the TU")
                    continue
                unsettled.append(("two-storage port copy", addr, [win[0], pname], pobj,
                                  owner_of(pobj, objects, owners),
                                  f"decomp TU {win[1]} wins; drop the port-side copy in {pobj} "
                                  f"(alias {pname}={win[0]} if C code still names it)"))
        elif mts:
            winner[addr] = mts[0][0]
            if prt:
                for pname, pobj in prt:
                    if pobj in a.retire:
                        retired.append((pobj, pname, addr, mts[0][0]))
                        if pname != mts[0][0]:
                            aliases[pname] = (mts[0][0], addr, f"retired port copy in {pobj}")
                        continue
                    unsettled.append(("two-storage mount and port", addr, [mts[0][0], pname], pobj,
                                      owner_of(pobj, objects, owners),
                                      "a mount row and a port file both host this ROM address"))
        elif prt:
            winner[addr] = prt[0][0]
        elif kinds.get("lib"):
            winner[addr] = sorted(kinds["lib"])[0][0]

    # unresolved references
    nowhere = []
    for ref, robjs in sorted(unres.items()):
        keys, is_data, pmf = keys_for(ref)
        if not is_data or pmf:
            continue
        ad = t.addrs(ref)
        hits = sorted({winner[x] for x in ad if x in winner})
        hit_addr = [x for x in ad if x in winner]
        if len(hits) == 1:
            if ref in aliases:
                continue
            aliases[ref] = (hits[0], hit_addr[0], "unresolved reference; one storage at its address")
        elif not hits:
            nowhere.append((ref, sorted(robjs), sorted(ad)))
        else:
            unsettled.append(("ambiguous reference", hit_addr[0], [ref] + hits, "", "?",
                              "several storages answer this reference's addresses"))

    conflicts = []
    for lhs, (rhs, addr, why) in list(aliases.items()):
        clash = [(o, w) for o, w in existing.get(lhs, ()) if o != rhs]
        if clash:
            # Another file already aliases this name elsewhere: two directives
            # for one name do not link. Left to that file's owner (conflicts.tsv).
            for old_rhs, where in clash:
                conflicts.append((lhs, rhs, old_rhs, where))
            del aliases[lhs]
            continue
        if lhs in defs and any(classify(o, mounts, objects) != "mount" for o in defs[lhs]) \
                and why.startswith("unresolved"):
            del aliases[lhs]

    for lhs in dead_rows(aliases, a.config, a.aliases):
        del aliases[lhs]

    counts = count_classes(defs, dup, unres, mounts, objects, t, store, drops, aliases, retired)
    return dict(drops=drops, aliases=aliases, unsettled=unsettled, nowhere=nowhere,
                conflicts=conflicts, counts=counts, lists=lists, tables=t,
                retired=retired, objects=objects, adds=adds)


def count_classes(defs, dup, unres, mounts, objects, t, store, drops, aliases, retired):
    gone = {(n, o) for o, n, _, _ in retired}
    dropped = {(lst, n) for lst, n, *_ in drops}
    dropped_names = {"_" + n for _, n, *_ in drops}
    c = {}
    # LNK4006: a mount and a non-mount object define one name
    m_dup = [(n, a, b) for n, a, b in dup if (a in mounts) != (b in mounts)]
    c["LNK2005 mount row also defined by TU"] = (
        len(m_dup), sum(1 for n, a, b in m_dup if n not in dropped_names))
    # linkage mismatch: decorated data ref unresolved while a C mount name exists at its address
    lm = []
    for ref in unres:
        keys, is_data, pmf = keys_for(ref)
        if ref.startswith("?") and is_data and not pmf:
            ad = t.addrs(ref)
            if any(store.get(x, {}).get("mount") for x in ad):
                lm.append(ref)
    c["C/C++ linkage mismatch"] = (len(lm), sum(1 for r in lm if r not in aliases))
    # two storage: one ROM address held by more than one storage
    two = [a for a, k in store.items() if sum(len(v) for v in k.values()) > 1]

    def settled(a):
        k = store[a]
        left = [x for x in k.get("mount", ()) if x[0] not in dropped_names]
        prt = [x for x in k.get("port", ()) if x not in gone]
        tus = {x[1] for x in k.get("tu", ())}
        return len(tus) + len(left) + len(prt) <= 1
    c["two-storage (one ROM address, several storages)"] = (len(two), sum(1 for a in two if not settled(a)))
    # C name defined nowhere
    cn = [r for r in unres if r.startswith("_") and not r.startswith("__Z") and keys_for(r)[1]]
    c["C name unresolved"] = (len(cn), sum(1 for r in cn if r not in aliases))
    return c

# ---------------------------------------------------------------- output


HEADER = """// GENERATED by port/tools/mount_reconcile.py. Do not edit; rerun the tool.
//
// Each row binds a name the link asks for onto the one storage that holds
// the same ROM (module, address): the decomp TU's own definition when one is
// linked, else the ROM mount's C name. `python port/tools/mount_reconcile.py
// --check` proves every row's two names share an address.
"""


CODE_SUFFIXES = (".c", ".cpp", ".h", ".hpp", ".inc")
_COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
# a C name as code spells it: not inside an MSVC decoration (?name@@..., Scope@name)
_CNAME = re.compile(r"(?<![?@\w])([A-Za-z_]\w*)(?![\w@])")


def named_in_tree(names, skip):
    """The subset of C names (no leading underscore) that C or C++ code under src/, include/ or
    port/ spells, comments stripped, outside the generated alias file and port/tools/ (fixtures)."""
    import subprocess
    want = set(names)
    if not want:
        return set()
    files = subprocess.run(["git", "-C", str(ROOT), "ls-files", "-z", "--", "src", "include", "port"],
                           capture_output=True, text=True).stdout.split("\0")
    skip = Path(skip).resolve()
    found = set()
    # A mount list token names the object too: ovdata's generated code references
    # a name-only ("name!") row's name when it rebases a ROM pointer onto it.
    for f in files:
        if re.fullmatch(r"port/ov\d+_syms\.txt", f):
            for line in (ROOT / f).read_text(encoding="utf-8", errors="replace").splitlines():
                if not line.lstrip().startswith("#"):
                    found |= want & {re.split(r"[@:!]", t)[0] for t in line.split()}
    for f in files:
        if not f.endswith(CODE_SUFFIXES) or f.startswith("port/tools/"):
            continue
        p = ROOT / f
        if p.resolve() == skip:
            continue
        try:
            text = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if not any(n in text for n in want - found):
            continue
        found |= want & set(_CNAME.findall(_COMMENT.sub(" ", text)))
        if found == want:
            break
    return found


def dead_rows(rows, config, skip):
    """Left hand sides whose C spelling symbols.txt no longer lists (a pin-era name main has
    renamed) and that nothing in the tree names: a row for them aliases a name no object can
    reference, so it is not written (port_refcheck counts each as a stale hal-link)."""
    by_name, _ = load_symbols(config)
    cand = [lhs[1:] for lhs in rows if lhs.startswith("_") and not lhs.startswith("__")
            and lhs[1:] not in by_name]
    named = named_in_tree(cand, skip)
    return {"_" + n for n in cand if n not in named}


def write_aliases(path, aliases):
    lines = [HEADER]
    for lhs in sorted(aliases):
        rhs, (mod, addr), why = aliases[lhs]
        lines.append(f'#pragma comment(linker, "/alternatename:{lhs}={rhs}")  /* {mod} 0x{addr:08x} */')
    Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def apply_drops(drops):
    """Turn each dropped mount row into a name-only row (see below); returns the count."""
    by_file = defaultdict(set)
    for lst, name, *_ in drops:
        by_file[lst].add(name)
    done = 0
    for lst, names in sorted(by_file.items()):
        p = ROOT / lst
        text = p.read_text(encoding="utf-8")
        nl = "\r\n" if "\r\n" in text else "\n"
        out = []
        for line in text.split(nl):
            if line.lstrip().startswith("#") or not line.strip():
                out.append(line)
                continue
            # A dropped row becomes NAME-ONLY ("name!"), never deleted: the TU owns
            # the storage, but tools/ovdata.py's pointer pass still needs the name to
            # rebase ROM pointers into the object onto it. Deleting the token (as
            # catchup1 round 2 did) sent those pointers to gap blocks.
            toks = line.split()
            new = [x + "!" if re.split(r"[@:!]", x)[0] in names and not x.endswith("!") else x
                   for x in toks]
            done += sum(1 for a, b in zip(toks, new) if a != b)
            if new != toks:
                line = " ".join(new)
            out.append(line)
        p.write_text(nl.join(out), encoding="utf-8", newline="")
    return done


def add_mounts(adds):
    by_file = defaultdict(list)
    for lst, name in adds:
        by_file[lst].append(name)
    for lst, names in by_file.items():
        p = ROOT / lst
        text = p.read_text(encoding="utf-8")
        have = {re.split(r"[@:]", x)[0] for x in text.split()}
        names = [n for n in names if n not in have]
        if not names:
            continue
        nl = "\r\n" if "\r\n" in text else "\n"
        if not text.endswith(nl):
            text += nl
        text += "# mount_reconcile --add-mount: ROM data a linked TU names but none defines" + nl
        text += " ".join(names) + nl
        p.write_text(text, encoding="utf-8", newline="")


def retire_defs(retired, objects):
    """Remove each retired one-line file-scope definition.

    Returns the left hand sides whose definition stays: not a plain one-line
    definition, or sized by kPortBodySlots (the port widens per-player state
    on purpose, so the ROM-sized TU copy cannot replace it). A name with no
    definition line left counts as already retired."""
    by_file = defaultdict(dict)
    left = set()
    for pobj, pname, addr, win in retired:
        if pname.startswith("?"):
            r = _frag_chain(pname[1:])
            ident = r[0][0] if r and len(r[0]) == 1 else None
        else:
            ident = pname[1:]
        srcs = [s for s in objects.get(pobj, ()) if not decomp_src(s)]
        if not ident or len(srcs) != 1:
            left.add(pname)
            continue
        by_file[srcs[0]][ident] = pname
    for rel, idents in sorted(by_file.items()):
        p = PORT / rel
        text = p.read_text(encoding="utf-8")
        nl = "\r\n" if "\r\n" in text else "\n"
        lines = text.split(nl)
        for ident, pname in sorted(idents.items()):
            rx = re.compile(r"^\s*(?:static\s+|volatile\s+|unsigned\s+|signed\s+)*[A-Za-z_]\w*\s*\**\s*"
                            + re.escape(ident) + r"\s*(?:\[[^\]]*\]\s*)*(?:=[^;]*)?;\s*(?:/\*.*\*/|//.*)?$")
            word = re.compile(r"\b" + re.escape(ident) + r"\b")
            hits = [i for i, l in enumerate(lines) if l is not None and rx.match(l)]
            decl = [l for l in lines if l is not None and word.search(l) and l.rstrip().endswith(";")
                    and not l.lstrip().startswith(("/", "*", "#", "extern"))]
            if len(hits) == 1 and "kPortBodySlots" not in lines[hits[0]]:
                lines[hits[0]] = None
            elif hits or decl:
                left.add(pname)
                print(f"retire: {rel} keeps {ident} (port-widened or not a one-line definition)")
        p.write_text(nl.join(l for l in lines if l is not None), encoding="utf-8", newline="")
    return left


def check(a):
    config = a.config
    mounts = {}
    by_name, _ = load_symbols(config)
    rx = re.compile(r'^#pragma comment\(linker, "/alternatename:([^=]+)=([^"]+)"\)')
    seen = {}
    bad = 0
    rows = 0
    existing = load_existing_aliases(a.aliases)
    for n, line in enumerate(Path(a.aliases).read_text(encoding="utf-8").splitlines(), 1):
        m = rx.match(line)
        if not m:
            continue
        rows += 1
        lhs, rhs = m.group(1), m.group(2)
        al = set()
        for k in keys_for(lhs)[0]:
            al |= by_name.get(k, set()) | pin_addr(k)
        ar = set()
        for k in keys_for(rhs)[0]:
            ar |= by_name.get(k, set()) | pin_addr(k)
        if not (al & ar):
            print(f"{a.aliases}:{n}: {lhs} {sorted(al)} and {rhs} {sorted(ar)} share no ROM address")
            bad += 1
        if lhs in seen and seen[lhs] != rhs:
            print(f"{a.aliases}:{n}: {lhs} aliased twice ({seen[lhs]}, {rhs})")
            bad += 1
        seen[lhs] = rhs
        for old, where in existing.get(lhs, ()):
            if old != rhs:
                print(f"{a.aliases}:{n}: {lhs} is also aliased to {old} at {where}")
                bad += 1
    dead = dead_rows(seen, config, a.aliases)
    for lhs in sorted(dead):
        print(f"{a.aliases}: {lhs} is a renamed pin spelling nothing names (dead row)")
    bad += len(dead)
    print(f"mount_reconcile --check: {rows} rows, {bad} bad")
    return 1 if bad else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--map")
    ap.add_argument("--log", action="append", default=[])
    ap.add_argument("--build")
    ap.add_argument("--config", default=str(ROOT / "config"))
    ap.add_argument("--aliases", default=str(GEN))
    ap.add_argument("--report")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--add-mount", action="append", default=[],
                    help="symbols.txt name to mount in its module's list (when no linked TU defines it)")
    ap.add_argument("--retire", action="append", default=[],
                    help="object basename whose port-side copies of a TU's ROM data are retired: "
                         "its definitions are removed (--apply) and its C names aliased onto the winner")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--prune-dead", action="store_true",
                    help="drop the generated file's dead rows (see --check) in place, nothing else")
    a = ap.parse_args(argv)
    if a.prune_dead:
        p = Path(a.aliases)
        rx = re.compile(r'^#pragma comment\(linker, "/alternatename:([^=]+)=')
        lines = p.read_text(encoding="utf-8").splitlines()
        dead = dead_rows({m.group(1) for m in map(rx.match, lines) if m}, a.config, a.aliases)
        keep = [l for l in lines if not ((m := rx.match(l)) and m.group(1) in dead)]
        p.write_text("\n".join(keep) + "\n", encoding="utf-8", newline="\n")
        print(f"mount_reconcile --prune-dead: {len(lines) - len(keep)} dead rows dropped")
        return 0
    if a.check:
        return check(a)
    if not (a.map and a.build):
        ap.error("--map and --build are required unless --check")
    r = analyse(a)
    for k, (before, after) in r["counts"].items():
        print(f"{k:52s} before {before:5d}  after {after:5d}")
    print(f"drops {len(r['drops'])}, aliases {len(r['aliases'])}, unsettled {len(r['unsettled'])}, "
          f"unresolved with no storage at any address {len(r['nowhere'])}, alias conflicts {len(r['conflicts'])}")
    if a.report:
        rep = Path(a.report)
        rep.mkdir(parents=True, exist_ok=True)
        with open(rep / "drops.tsv", "w", encoding="utf-8") as f:
            f.write("list\tname\tmodule\taddr\twinner\twinner_obj\n")
            for lst, n, (mod, ad), w, wo in sorted(r["drops"]):
                f.write(f"{lst}\t{n}\t{mod}\t0x{ad:08x}\t{w}\t{wo}\n")
        with open(rep / "aliases.tsv", "w", encoding="utf-8") as f:
            f.write("lhs\trhs\tmodule\taddr\twhy\n")
            for lhs, (rhs, (mod, ad), why) in sorted(r["aliases"].items()):
                f.write(f"{lhs}\t{rhs}\t{mod}\t0x{ad:08x}\t{why}\n")
        with open(rep / "unsettled.tsv", "w", encoding="utf-8") as f:
            f.write("class\tmodule\taddr\tnames\tport_obj\towner\tnote\n")
            for cls, (mod, ad), names, pobj, own, note in sorted(r["unsettled"], key=lambda x: (x[0], x[1])):
                f.write(f"{cls}\t{mod}\t0x{ad:08x}\t{' '.join(names)}\t{pobj}\t{own}\t{note}\n")
        with open(rep / "nowhere.tsv", "w", encoding="utf-8") as f:
            f.write("reference\treferenced_by\taddresses\n")
            for ref, objs, ad in r["nowhere"]:
                f.write(f"{ref}\t{' '.join(objs)}\t{' '.join(f'{m}:0x{x:08x}' for m, x in ad)}\n")
        with open(rep / "conflicts.tsv", "w", encoding="utf-8") as f:
            f.write("lhs\tnew_rhs\told_rhs\twhere\n")
            for row in r["conflicts"]:
                f.write("\t".join(row) + "\n")
    if a.apply:
        kept = retire_defs(r["retired"], r["objects"])
        for lhs in kept:
            r["aliases"].pop(lhs, None)
        n = apply_drops(r["drops"])
        add_mounts(r["adds"])
        write_aliases(a.aliases, r["aliases"])
        print(f"applied: {n} mount tokens made name-only, {len(r['aliases'])} alias rows -> {a.aliases}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
