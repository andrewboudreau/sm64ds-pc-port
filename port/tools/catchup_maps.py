"""Build the decomp bump maps: what the port must rename and re-point when the pin moves.

Run catchup1, Task 3.1. Compares two read-only checkouts of the decomp (the current pin
and the bump target; each needs config/, include/, src/ and tools/) and writes:

  renames.tsv    module  addr  old_name  new_name
                 one row for EVERY pin (module, addr) whose name changes at the target,
                 whether or not a port reference to it was found. Keyed on (module, addr)
                 from config/**/symbols.txt; a name is never carried across by name: if the
                 pin's name for an address is gone, the row says what the target calls THAT
                 address. Rows decided by rule are included (tags alias_merge and
                 cross_module:<module>).
  renames_kind.tsv  module  addr  old_name  new_name  tags
                 the same rows with apply tags (comma list, "plain" when none):
                   not_identifier      new name is not a C identifier (@984, sUnitY$1,
                                       __sinit_x.cpp): it cannot be written into source;
                                       route it through the alias machinery.
                   old_not_identifier  old name is not a C identifier.
                   linkage_c_to_cxx / linkage_cxx_to_c
                                       new name is Itanium-mangled and the old one is not
                                       (or the reverse): a text swap inside an extern "C"
                                       declaration names a symbol MSVC never emits; the
                                       declaration's linkage has to change with it.
                   chain               the new name is another row's old name (or the
                                       reverse): _ZdlPv, pmf_Dealer_StateDeal and
                                       __sinit_d_a_obj_fl_maruta.cpp at this bump.
                   reused_old_name     the old name labels another address at the target.
                   reused_new_name     the new name labelled another address at the pin.
                   new_name_shared     the new name also labels another target address.
                   alias_merge         the pin had an alias pair here; the survivor is used.
                   cross_module:<m>    the pin's label was misattributed and main dropped
                                       it; module <m> holds exactly one name at the same
                                       address, and the row follows it there.
                 APPLY RULE: substitute every row in ONE pass (each old name -> new name
                 over the original text). Applying rows one after another corrupts the
                 chain rows.
  decided.tsv    kind  module  addr  name  action
                 rows settled by rule that need no rename (pseudo_symbol: overlay_100 and
                 overlay_102 sit below the ROM address space; the port defines them).
  srcmoves.tsv   symbol  old_path  new_path
                 where tools/srcpath.py (run inside each checkout) puts the symbol's
                 source at the pin and at the target. The target side is looked up by
                 ADDRESS first (srcpath.owner_at), then by the target name only if that
                 name is unique in the target's symbol tables. "-" means no source.
  patchmap.tsv   path  status  new_paths  unowned
                 for every port/decomp_patches/<path>.patch, status is one of: exists |
                 exists_partial (some symbols now live elsewhere) | moved (its symbols'
                 target owners) | renamed (git rename detection at -M30%) |
                 absorbed@<commit> (deleted with no rename: the surviving file that gained
                 most of its identifiers in the deleting commit) | missing. renamed and
                 absorbed need --git-repo/--pin-rev/--target-rev. unowned counts its pin
                 symbols that no target source owns.
  hand.tsv       kind  module  addr  name  detail
                 only what a person must judge, candidates in detail: deleted (no unique
                 owner of the address anywhere), ambiguous, patch_path_missing.
  scope.tsv      name  categories   report only: pin symbols the port is seen to name
  counts.txt     the per-category counts

Each TSV starts with one "#"-prefixed header line.

scope.tsv is a report, not a filter. It lists the pin symbols.txt names that a slice row
(port/slice_*.txt and every other list port_read_lines() reads), a hal/ file, an
ovNNN_syms.txt, a hostgen script, hal/cxx_aliases.cpp or a decomp patch names; slice rows
and patch paths that name a source file add every symbol that file owns at the pin.

APPLY (Task 4.1 step 2): `catchup_maps.py apply --maps <dir> --port <port dir>
[--target <target checkout>] [--dry-run] [--force] --report <file>` writes renames_kind.tsv
and srcmoves.tsv into a port tree. The maps dir also holds decided.tsv, target.txt (PIN=,
TARGET=) and optionally hand.tsv. decided.tsv names are never touched; hand.tsv names and
any old name with two different new names are reported, never swapped. The rule
throughout: apply never rebinds a reference silently; when a swap could make a name mean
another function, the occurrence is reported instead.

  One pass. Each file is read once and every name is looked up in the ORIGINAL text, so
  a chain (A -> B while B -> C) never substitutes twice.
  Whole tokens. A token is a run of [A-Za-z0-9_$] that may also split at '.' and end at
  '@' (the maps hold .p__sinit_x, .L_x, @962, sUnitY$1 and __sinit_x.cpp; MSVC spells
  ?name@@YAX...). '@' starts a token only at the start of a run, so "1@262" in an input
  script is not @262. func_0201 never matches inside func_02010; a run that ends in a
  source extension and is not itself a map name is a file name and is left alone; a path
  is never tokenised (see srcmoves). Outside code (list, string and comment contexts, so
  a commented-out pragma stays whole) a leading '_' is the MSVC x86 C decoration
  (/alternatename:_data_x=...), so _old -> _new there as well.
  Files: every text file under the port dir except third_party/, decomp_patches/,
  decomp_overlay_files/ (Task 4.2 re-cuts those), this tool and its test, binary files
  (a NUL byte) and files that are not UTF-8 (counted in the report).

  Word rows. An old name that is one plain word ([A-Za-z][a-z]{2,}: main, Entry, Mod, Crash,
  Div, Halt, Sqrt at this bump) is also English and may be a host name (mods API Mod), so
  it is never swapped. Its occurrences outside prose (comments, '#' tails of list lines,
  .md/.rst docs) and outside Python code (def main) are reported as "hand_word" items.

  Symbol list vs source. Each file is cut into contexts:
    C/C++ (.c .cc .cpp .cxx .h .hh .hpp .hxx .inc .inl .ipp): code, string literal
      (incl. #pragma comment(linker, "/alternatename:...") tables), comment.
    Python (.py): code, string literal (symbol tables are quoted names), comment.
    Everything else (*_syms.txt, slice_*.txt and every other list, CMakeLists.txt and
      *.cmake symbol variables, .def, .tsv, .json, docs): one "list" context.
  A plain row is swapped in every context.
  A row tagged linkage_c_to_cxx, linkage_cxx_to_c, not_identifier, old_not_identifier or
  cross_module:<m> is swapped in list, string and comment contexts; in code it is left as
  it is and reported as a "hand" item (file:line, old, new, tags): the declaration's
  linkage or an alias has to be decided by the lane. Its swaps inside a C/C++ or Python
  string are also reported ("tagged_in_string") for strings that are really C templates.
  A row tagged chain, reused_old_name or reused_new_name (the name also means another
  address) is swapped only in list and comment contexts. In code AND in strings it is a
  "hand" item, both ends of every chain: an /alternatename equation like
  __ZdlPv=??3@YAXPAX@Z keeps naming MSVC's operator delete on its right side, so renaming
  the left side by address would move it onto another ROM function and still link.
  name_collision: a new name the unapplied port already spells in C/C++ code (e.g. a host
  stub __register_global_object) is not swapped in code; the occurrence is reported.
  self_alias: a swap that leaves "/alternatename:X=X" on a line is reported so the lane
  deletes the line.

  srcmoves. Rows are grouped by old path. A path written as src/... or include/...
  (also after ${DECOMP_ROOT}/ or ../) is rewritten only when all of its moved symbols go
  to one new file that holds one symbol (with --target: what the target's srcpath says the
  file owns; without: the srcmoves rows that land there) and no other old file lands
  there. Otherwise every occurrence is a report item: path_into_tu (moved into a file
  holding several symbols: the lane decides whether the slice now names the TU),
  path_split (symbols went to several files), path_gone (no target source), path_partial
  (with --target: the old file still exists but some symbols left it).

  Stamp and idempotence. A real apply writes <port>/DECOMP_MAPS_APPLIED with the maps'
  sha256, pin rev and target rev; COMMIT IT WITH THE APPLY. Before writing anything:
    stamp sha equals these maps   -> chain rows (and path chains) are switched off; every
                                     other row's new name is not an old name, so a second
                                     apply changes nothing (old names brought back later,
                                     e.g. by merging an older branch, are still caught);
    stamp names other maps        -> refused (exit 2, nothing written) unless these maps'
                                     PIN equals the stamp's target rev (the next bump);
    no stamp, tree looks applied  -> refused: a chain tail (a new name no row renames
                                     further) is present, or more plain rows show only
                                     their new name than show their old one.
  --force overrides both refusals.

  Report: "# key value" count lines, then kind file line old new tags context rows.
  Every swap made goes to <report>.changes.tsv (file line old new tags context).

Usage:
  python port/tools/catchup_maps.py --pin <pin checkout> --target <target checkout>
      --port <port dir> --out <dir> [--out <dir> ...]
      [--git-repo <repo> --pin-rev <sha> --target-rev <sha>]
  python port/tools/catchup_maps.py apply --maps <maps dir> --port <port dir>
      [--target <target checkout>] [--dry-run] [--force] --report <file>

The checkouts and the port dir are always passed in; nothing is guessed from where this
script lives, so it keeps working while port/ is reshaped around it.
"""
import argparse
import collections
import json
import pathlib
import re
import subprocess
import sys

_SYM_RE = re.compile(r"^(\S+)\s+kind:\S+.*?\baddr:0x([0-9a-fA-F]+)")
_TOKEN_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
_PATH_ROW_RE = re.compile(r"^((?:src|include)/\S+\.(?:c|cpp|h|hpp|inc))\b")


def parse_symbols(text):
    rows = []
    for line in text.splitlines():
        m = _SYM_RE.match(line.strip())
        if m:
            rows.append((m.group(1), int(m.group(2), 16)))
    return rows


def _module_files(checkout):
    """(label, symbols.txt) the way tools/relocs.iter_symbol_files labels them."""
    cfg = pathlib.Path(checkout) / "config" / "arm9"
    yield "arm9", cfg / "symbols.txt"
    yield "itcm", cfg / "itcm" / "symbols.txt"
    yield "dtcm", cfg / "dtcm" / "symbols.txt"
    for path in sorted((cfg / "overlays").glob("ov*/symbols.txt")):
        yield path.parent.name, path


class SymbolTable:
    def __init__(self):
        self.at = collections.defaultdict(set)      # (module, addr) -> {names}
        self.where = collections.defaultdict(set)   # name -> {(module, addr)}

    @classmethod
    def from_rows(cls, by_module):
        t = cls()
        for module, rows in by_module.items():
            for name, addr in rows:
                t.at[(module, addr)].add(name)
                t.where[name].add((module, addr))
        return t

    @classmethod
    def load(cls, checkout):
        by_module = {}
        seen = set()
        for label, path in _module_files(checkout):
            if path.is_file():
                by_module[label] = parse_symbols(path.read_text(encoding="utf-8", errors="ignore"))
                seen.add(path.resolve())
        missed = [p for p in (pathlib.Path(checkout) / "config").rglob("symbols.txt")
                  if p.resolve() not in seen]
        if missed:
            raise SystemExit(f"catchup_maps: symbols.txt not covered: {missed}")
        return cls.from_rows(by_module)

    def names(self):
        return set(self.where)


def _loc(module, addr):
    return f"{module}:0x{addr:08x}"


Loc = collections.namedtuple("Loc", "name module addr new_name pin_unique new_unique")


class RenameResult:
    def __init__(self):
        self.renames = []     # (module, addr, old, new)
        self.tags = {}        # (module, addr, old) -> {tag}
        self.decided = []     # (kind, module, addr, name, action)  decided by rule, no rename row
        self.hand = []        # (kind, module, addr, name, detail)  left for a person
        self.unchanged = set()
        self.locations = []   # [Loc] for every pin location considered
        self.counts = collections.Counter()


_IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")

# Addresses below the ARM9 address space are not code or data: overlay_100/overlay_102
# (0x64/0x66) are labels the port defines itself (hal/scene_boot.cpp), so a pin-only one
# needs no rename.
_PSEUDO_LIMIT = 0x01000000


def _mangled(name):
    return name.startswith("_Z")


def _tag_row(r, module, addr, old, new, extra=()):
    tags = set(extra)
    if not _IDENT_RE.match(new):
        tags.add("not_identifier")
    if not _IDENT_RE.match(old):
        tags.add("old_not_identifier")
    if _mangled(old) != _mangled(new):
        tags.add("linkage_c_to_cxx" if _mangled(new) else "linkage_cxx_to_c")
    r.renames.append((module, addr, old, new))
    r.tags[(module, addr, old)] = tags


def build_renames(pin, tgt, scope_names=None):
    """Address-keyed rename map. scope_names=None means every pin name (the default the
    run uses: the maps must not depend on finding each reference in port/ first)."""
    r = RenameResult()
    names = sorted(pin.where) if scope_names is None else sorted(scope_names)
    by_addr = collections.defaultdict(list)          # addr -> [(module, names)] at target
    for (m, a), ns in tgt.at.items():
        by_addr[a].append((m, ns))
    for name in names:
        for module, addr in sorted(pin.where.get(name, ())):
            pin_names = pin.at[(module, addr)]
            tgt_names = tgt.at.get((module, addr), set())
            new_name = None
            elsewhere = tgt.where.get(name, set()) - {(module, addr)}
            if name in tgt_names:
                new_name = name
                r.unchanged.add(name)
                r.counts["unchanged"] += 1
            elif not tgt_names:
                others = sorted((m, sorted(ns)) for m, ns in by_addr.get(addr, ()) if m != module)
                if addr < _PSEUDO_LIMIT:
                    r.decided.append(("pseudo_symbol", module, addr, name,
                                      "not a ROM address; the port defines it, no rename"))
                    r.counts["decided_pseudo"] += 1
                elif len(others) == 1 and len(others[0][1]) == 1:
                    # main moved a misattributed label to the module that really owns the
                    # address: follow it there.
                    new_name = others[0][1][0]
                    _tag_row(r, module, addr, name, new_name, {"cross_module:" + others[0][0]})
                    r.counts["decided_cross_module"] += 1
                else:
                    cands = "; ".join(f"{m}:{','.join(ns)}" for m, ns in others) or "none"
                    detail = f"no target symbol here; same address in other modules: {cands}"
                    if elsewhere:
                        detail += "; name now labels " + ",".join(_loc(*l) for l in sorted(elsewhere))
                    r.hand.append(("deleted", module, addr, name, detail))
                    r.counts["deleted"] += 1
            else:
                fresh = sorted(tgt_names - pin_names)
                if len(fresh) == 1:
                    new_name = fresh[0]
                    extra = set()
                    if tgt.where.get(new_name, set()) - {(module, addr)}:
                        extra.add("new_name_shared")
                    _tag_row(r, module, addr, name, new_name, extra)
                    r.counts["renamed"] += 1
                elif not fresh and len(tgt_names) == 1:
                    # an alias pair lost one of its names; the survivor is the address's name
                    new_name = next(iter(tgt_names))
                    _tag_row(r, module, addr, name, new_name, {"alias_merge"})
                    r.counts["decided_alias_merge"] += 1
                else:
                    r.hand.append(("ambiguous", module, addr, name,
                                   "candidates " + ",".join(sorted(tgt_names))))
                    r.counts["ambiguous"] += 1
            if new_name is not None and new_name != name and elsewhere:
                r.tags[(module, addr, name)].add("reused_old_name")
            r.locations.append(Loc(name, module, addr, new_name,
                                   len(pin.where.get(name, ())) == 1,
                                   new_name is not None and len(tgt.where.get(new_name, ())) == 1))
    # A new name that was some other pin address's name: either a chain inside this map
    # (that address is renamed too) or a name that used to mean something else.
    olds = collections.Counter(old for _, _, old, _ in r.renames)
    news = collections.Counter(new for _, _, _, new in r.renames)
    for module, addr, old, new in r.renames:
        tags = r.tags[(module, addr, old)]
        if olds[new] or news[old]:
            tags.add("chain")
        elif pin.where.get(new, set()) - {(module, addr)}:
            tags.add("reused_new_name")
    r.counts["renamed_total"] = len(r.renames)
    r.counts["locations"] = len(r.locations)
    return r


class MoveResult:
    def __init__(self):
        self.rows = []
        self.counts = collections.Counter()


def _owner_maps(locs, res, name_of, unique_of):
    owner, byname, _ = res.batch([(l.module, l.addr) for l in locs],
                                 sorted({name_of(l) for l in locs if name_of(l) and unique_of(l)}), [])
    return owner, byname


def build_srcmoves(locations, pin_res, tgt_res):
    m = MoveResult()
    p_owner, p_byname = _owner_maps(locations, pin_res, lambda l: l.name, lambda l: l.pin_unique)
    t_owner, t_byname = _owner_maps(locations, tgt_res, lambda l: l.new_name, lambda l: l.new_unique)
    new_paths = set()
    resolved = []
    for l in locations:
        old = p_owner.get((l.module, l.addr)) or (p_byname.get(l.name) if l.pin_unique else None)
        new = t_owner.get((l.module, l.addr)) or (
            t_byname.get(l.new_name) if l.new_unique and l.new_name else None)
        resolved.append((l, old, new))
        if new:
            new_paths.add(new)
    _, _, owned = tgt_res.batch([], [], sorted(new_paths))
    for l, old, new in resolved:
        if old == new:
            m.counts["same" if old else "no_source_either"] += 1
            continue
        m.rows.append((l.name, old or "-", new or "-"))
        if not new:
            m.counts["source_gone"] += 1
        elif not old:
            m.counts["new_source"] += 1
        elif len(owned.get(new, ())) > 1:
            m.counts["into_tu"] += 1
        else:
            m.counts["moved"] += 1
    return m


def map_patch_paths(paths, pin, pin_res, tgt_res, tgt_root, file_renames=None, follow=None):
    """[(path, status, new_paths, unowned)] for each patched decomp path.

    status: exists | exists_partial (some of its symbols now live elsewhere) | moved (its
    symbols live in new_paths) | renamed (git rename detection, file_renames) | absorbed
    (follow(path) -> (new_path, commit): the file that gained its content in the commit
    that deleted it) | missing.
    unowned counts the path's pin symbols that no target source owns (unmatched there)."""
    tgt_root = pathlib.Path(tgt_root)
    file_renames = file_renames or {}
    src_paths = [p for p in paths if p.startswith("src/")]
    _, _, owned = pin_res.batch([], [], src_paths)
    locs_by_path = {p: sorted({loc for s in owned.get(p, ()) for loc in pin.where.get(s, ())})
                    for p in src_paths}
    all_locs = sorted({loc for ls in locs_by_path.values() for loc in ls})
    t_owner, _, _ = tgt_res.batch(all_locs, [], [])
    rows = []
    for p in paths:
        exists = (tgt_root / p).is_file()
        locs = locs_by_path.get(p, ())
        targets = sorted({t_owner.get(loc) for loc in locs} - {None})
        unowned = sum(1 for loc in locs if t_owner.get(loc) is None)
        if exists and (not targets or targets == [p]):
            rows.append((p, "exists", p, unowned))
        elif exists:
            rows.append((p, "exists_partial", ";".join(sorted(set(targets) | {p})), unowned))
        elif targets:
            rows.append((p, "moved", ";".join(targets), unowned))
        elif p in file_renames:
            rows.append((p, "renamed", file_renames[p], unowned))
        else:
            got, commit = follow(p) if follow else (None, None)
            if got:
                rows.append((p, f"absorbed@{commit[:11]}", got, unowned))
            else:
                rows.append((p, "missing", "-", unowned))
    return rows


def _git(repo, *args):
    return subprocess.run(["git", "-C", str(repo), *args], capture_output=True, text=True,
                          encoding="utf-8", errors="ignore", check=True).stdout


# 30%, not git's default 50%: main's renames of class headers (Player.h -> daPly_c.h, R038)
# rewrote most of the file along with the name.
RENAME_SIMILARITY = "30%"


def git_file_renames(repo, pin_rev, target_rev):
    """{old_path: new_path} from git's rename detection between the two revisions."""
    # -l0 lifts diff.renameLimit; thousands of src/ paths changed, and over the limit git
    # silently stops detecting renames at all.
    out = {}
    for line in _git(repo, "diff", "--name-status", f"-M{RENAME_SIMILARITY}", "-l0",
                     pin_rev, target_rev, "--", "src", "include").splitlines():
        parts = line.split(chr(9))
        if parts[0].startswith("R") and len(parts) == 3:
            out[parts[1]] = parts[2]
    return out


def _ident_set(text):
    return {t for t in _TOKEN_RE.findall(text) if len(t) >= 4}


def best_absorber(deleted_text, candidates):
    """Pick the file that absorbed a deleted file's content.

    candidates: {path: text that file GAINED in the deleting commit}. Score is the count
    of the deleted file's distinctive identifiers (4+ chars) found in the gained text.
    Returns (path, score) for a unique best score above zero, else (None, scores)."""
    want = _ident_set(deleted_text)
    scores = {p: len(want & _ident_set(t)) for p, t in candidates.items()}
    if not scores:
        return None, scores
    best = max(scores.values())
    top = [p for p, v in scores.items() if v == best]
    if best <= 0 or len(top) != 1:
        return None, scores
    return top[0], scores


def git_follow_deleted(repo, pin_rev, target_rev, path):
    """For a file deleted between the revisions with no rename: the commit that deleted it
    and the surviving file in that commit that gained the most of its content."""
    commit = _git(repo, "log", "--diff-filter=D", "--format=%H", "-1",
                  f"{pin_rev}..{target_rev}", "--", path).strip()
    if not commit:
        return None, None
    top = pathlib.PurePosixPath(path).parts[0]
    cands = {}
    for line in _git(repo, "show", "--name-status", "--format=", commit).splitlines():
        parts = line.split(chr(9))
        status, cand = parts[0], parts[-1]
        if status[:1] in "AMR" and cand.startswith(top + "/") and cand != path:
            if _git(repo, "ls-tree", "--name-only", target_rev, "--", cand).strip():
                diff = _git(repo, "show", "--format=", commit, "--", cand)
                cands[cand] = "\n".join(l[1:] for l in diff.splitlines()
                                        if l.startswith("+") and not l.startswith("+++"))
    deleted = _git(repo, "show", f"{commit}^:{path}")
    best, _ = best_absorber(deleted, cands)
    return best, commit


def _tokens(text, names):
    found = set()
    for tok in _TOKEN_RE.findall(text):
        if tok in names:
            found.add(tok)
        elif tok.startswith("_") and tok[1:] in names:
            found.add(tok[1:])
    return found


def _read(path):
    return path.read_text(encoding="utf-8", errors="ignore")


def _list_files(port):
    """Every list port_read_lines() reads, plus every slice_*.txt."""
    files = set(port.glob("slice_*.txt"))
    cml = port / "CMakeLists.txt"
    if cml.is_file():
        for m in re.finditer(r'port_read_lines\(\s*"\$\{CMAKE_CURRENT_SOURCE_DIR\}/([^"]+)"', _read(cml)):
            p = port / m.group(1)
            if p.is_file():
                files.add(p)
    return sorted(files)


def patch_paths(port):
    root = port / "decomp_patches"
    return sorted(p.relative_to(root).as_posix()[:-len(".patch")] for p in root.rglob("*.patch"))


def collect_scope(port, pin, pin_res):
    """{pin symbol name: {categories}} for every pin symbol the port names."""
    port = pathlib.Path(port)
    names = pin.names()
    scope = collections.defaultdict(set)
    row_paths = collections.defaultdict(set)   # src path -> categories

    def add(found, cat):
        for n in found:
            scope[n].add(cat)

    for f in _list_files(port):
        for line in _read(f).splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            pm = _PATH_ROW_RE.match(line)
            if pm and pm.group(1).startswith("src/"):
                row_paths[pm.group(1)].add("slice")
            add(_tokens(line.split("#", 1)[0], names), "slice")
    hal = port / "hal"
    if hal.is_dir():
        for f in hal.rglob("*"):
            if f.is_file() and f.suffix in (".c", ".cpp", ".h", ".hpp", ".inc", ".txt", ".def"):
                add(_tokens(_read(f), names), "cxx_aliases" if f.name == "cxx_aliases.cpp" else "hal")
    for f in port.glob("ov*_syms.txt"):
        add(_tokens(_read(f), names), "ovsyms")
    for f in (port / "tools").glob("hostgen*.py"):
        add(_tokens(_read(f), names), "hostgen")
    for rel in patch_paths(port):
        if rel.startswith("src/"):
            row_paths[rel].add("patch")
        text = _read(port / "decomp_patches" / (rel + ".patch"))
        changed = "\n".join(l for l in text.splitlines()
                            if l[:1] in "+-@" and not l.startswith(("+++", "---")))
        add(_tokens(changed, names), "patch")
    if row_paths:
        _, _, owned = pin_res.batch([], [], sorted(row_paths))
        for p, cats in row_paths.items():
            for s in owned.get(p, ()):
                if s in names:
                    scope[s] |= cats
    return dict(scope)


_SRCPATH_DRIVER = r"""
import json, pathlib, sys
root = pathlib.Path(sys.argv[1]).resolve()  # decomp-root-check: explicit: the --pin or --target checkout this child process serves
sys.path.insert(0, str(root / "tools"))
import srcpath
q = json.load(sys.stdin)
def rel(p):
    if p is None:
        return None
    p = pathlib.Path(p).resolve()
    try:
        return p.relative_to(root).as_posix()
    except ValueError:
        return p.as_posix()
out = {
    "owner": [rel(srcpath.owner_at(m, a)) for m, a in q["locs"]],
    "byname": [rel(srcpath.path_for(n)) for n in q["names"]],
    "owned": [srcpath.symbols_for(root / p) for p in q["paths"]],
}
json.dump(out, sys.stdout)
"""


class SrcpathResolver:
    """Runs the checkout's own tools/srcpath.py in a child process (one per batch)."""

    def __init__(self, checkout):
        self.checkout = str(pathlib.Path(checkout).resolve())

    def batch(self, locs, names, paths):
        locs, names, paths = list(locs), list(names), list(paths)
        if not (locs or names or paths):
            return {}, {}, {}
        proc = subprocess.run([sys.executable, "-c", _SRCPATH_DRIVER, self.checkout],
                              input=json.dumps({"locs": locs, "names": names, "paths": paths}),
                              capture_output=True, text=True, cwd=self.checkout)
        if proc.returncode:
            raise SystemExit(f"srcpath in {self.checkout} failed:\n{proc.stderr}")
        out = json.loads(proc.stdout)
        return (dict(zip(map(tuple, locs), out["owner"])), dict(zip(names, out["byname"])),
                dict(zip(paths, out["owned"])))


def _write(outs, name, header, rows):
    text = "#" + "\t".join(header) + "\n" + "".join("\t".join(map(str, r)) + "\n" for r in rows)
    for d in outs:
        (d / name).write_text(text, encoding="utf-8", newline="\n")


# ---------------------------------------------------------------------------------------
# apply: write the maps into a port tree (Task 4.1 step 2). See APPLY in the module doc.

APPLY_STAMP = "DECOMP_MAPS_APPLIED"
# Rows with one of these tags are never text-swapped where a compiler reads the name as an
# identifier (C/C++ or Python code); they are swapped in symbol lists and become hand items.
APPLY_BLOCKING = ("linkage_c_to_cxx", "linkage_cxx_to_c", "not_identifier",
                  "old_not_identifier", "cross_module", "reused_old_name", "reused_new_name")
# Rows with one of these tags name something that also means another address (a chain, or
# a name reused at another address). A swap in code or in a string (an /alternatename
# equation whose other side is still an MSVC name) can rebind a reference to the wrong
# function without a link error, so they are swapped only in lists and comments.
APPLY_REBIND = ("chain", "reused_old_name", "reused_new_name")
# catchup_parked holds patches a bump parked for re-placement: their context lines must stay
# as they were cut, so a re-run of apply after parking leaves them alone.
APPLY_SKIP_DIRS = ("third_party", "decomp_patches", "decomp_overlay_files", "catchup_parked")
APPLY_SKIP_NAMES = (APPLY_STAMP, "catchup_maps.py", "test_catchup_maps.py")
# A file whose first 2 KB hold this marker keeps its old names on purpose (a test fixture
# spelled in the pin's names, such as test_hook_anchor_check.py's BEFORE tree).
APPLY_SKIP_MARKER = "catchup_maps: no-apply"
_C_EXTS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inc", ".inl", ".ipp"}
_PY_EXTS = {".py", ".pyw"}
_SRCFILE_RE = re.compile(r"\.(?:c|cc|cpp|cxx|h|hh|hpp|hxx|inc|inl|s)$")
_RUN_RE = re.compile(r"[A-Za-z0-9_.$@/\-]+")
_SUBRUN_RE = re.compile(r"[A-Za-z0-9_.$@]+")
_TREEPATH_RE = re.compile(r"(?:^|/)((?:src|include)/[A-Za-z0-9_.$@/\-]+)$")
# An old name that is one plain word (main, Entry, Mod, Crash, Div, Halt, Sqrt at this bump)
# is also English and a host name (the mods API's Mod): it is never swapped, only reported.
_WORD_RE = re.compile(r"[A-Za-z][a-z]{2,}")
_DOC_EXTS = {".md", ".rst"}


def _blocking(tags):
    return sorted(t for t in tags if t.split(":")[0] in APPLY_BLOCKING)


def _rebinding(tags):
    return sorted(t for t in tags if t in APPLY_REBIND)


def _read_tsv(path):
    rows = []
    for line in pathlib.Path(path).read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            rows.append(line.split("\t"))
    return rows


class ApplyMaps:
    """The three maps, ready for one-pass substitution.

    rows: [(module, addr, old, new, {tags})]; decided: {name}; hand: {name: kind};
    moves: [(symbol, old_path, new_path)]; owned_count: path -> number of symbols the
    target source owns (None: count srcmoves rows instead); old_exists: path -> bool
    (None: unknown); applied: the port already carries these maps (stamp matches), so
    chain rows, whose new names are other rows' old names, are switched off."""

    def __init__(self, rows, decided=(), hand=None, moves=(), owned_count=None,
                 old_exists=None, applied=False, collide=()):
        self.decided = set(decided)
        self.collide = set(collide)   # new names the unapplied port already spells in code
        news = collections.defaultdict(set)
        tags = collections.defaultdict(set)
        for _, _, old, new, t in rows:
            news[old].add(new)
            tags[old] |= set(t)
        self.hand = dict(hand or {})
        self.row = {}
        for old, ns in news.items():
            if old in self.decided:
                continue
            if len(ns) > 1:
                self.hand.setdefault(old, "conflict:" + "|".join(sorted(ns)))
                continue
            self.row[old] = (next(iter(ns)), frozenset(tags[old]))
        self.words = {o: self.row.pop(o) for o in list(self.row) if _WORD_RE.fullmatch(o)}
        all_new = {n for n, _ in self.row.values()}
        self.chain = {o for o in self.row if o in all_new}
        all_old = set(self.row)
        self.tails = {n for o, (n, t) in self.row.items() if "chain" in t and n not in all_old}
        self.applied = applied
        if applied:
            for o in self.chain:
                del self.row[o]
        self.keys = (set(self.row) | set(self.hand) | set(self.words)) - self.decided
        self.path = self._paths(moves, owned_count, old_exists, applied)

    @staticmethod
    def _paths(moves, owned_count, old_exists, applied):
        """old path -> ("move", new) | (item kind, detail)."""
        by_old = collections.defaultdict(dict)
        syms_of_new = collections.defaultdict(set)
        olds_of_new = collections.defaultdict(set)
        for sym, old, new in moves:
            if old == "-" or old == new:
                continue
            by_old[old][sym] = new
            if new != "-":
                syms_of_new[new].add(sym)
                olds_of_new[new].add(old)
        out = {}
        for old, syms in by_old.items():
            targets = sorted(set(syms.values()))
            some = sorted(syms)[:4]
            detail = (f"{len(syms)} moved symbol(s) -> {','.join(targets[:6])}"
                      f"{',...' if len(targets) > 6 else ''}; e.g. {','.join(some)}")
            if old_exists is not None and old_exists(old):
                out[old] = ("path_partial", detail)
            elif targets == ["-"]:
                out[old] = ("path_gone", detail)
            elif len(targets) > 1:
                out[old] = ("path_split", detail)
            else:
                new = targets[0]
                count = owned_count(new) if owned_count else len(syms_of_new[new])
                if count > 1 or len(olds_of_new[new]) > 1:
                    out[old] = ("path_into_tu", detail)
                else:
                    out[old] = ("move", new)
        if applied:
            moved_to = {v[1] for v in out.values() if v[0] == "move"}
            for old in [o for o, v in out.items() if v[0] == "move" and o in moved_to]:
                del out[old]
        return out


def _segments(text, lang):
    """[(start, end, ctx)] with ctx code | string | comment (C/C++ and Python) or list."""
    if lang == "list":
        return [(0, len(text), "list")]
    segs, n, i, start = [], len(text), 0, 0

    def cut(a, b, ctx):
        if b > a:
            segs.append((a, b, ctx))

    while i < n:
        c = text[i]
        end = None
        if lang == "c" and text.startswith("//", i) or lang == "py" and c == "#":
            j = text.find("\n", i)
            end, ctx = (n if j < 0 else j), "comment"
        elif lang == "c" and text.startswith("/*", i):
            j = text.find("*/", i + 2)
            end, ctx = (n if j < 0 else j + 2), "comment"
        elif c in "\"'":
            q = c * 3 if lang == "py" and text.startswith(c * 3, i) else c
            j = i + len(q)
            while j < n and not text.startswith(q, j) and (len(q) == 3 or text[j] != "\n"):
                j += 2 if text[j] == "\\" else 1
            end, ctx = min(n, j + (len(q) if text.startswith(q, j) else 0)), "string"
        if end is None:
            i += 1
            continue
        cut(start, i, "code")
        cut(i, end, ctx)
        i = start = max(end, i + 1)
    cut(start, n, "code")
    return segs


def _ident_matches(r, keys, decorated):
    """[(s, e, old, prefix)] whole-token matches inside one run of [A-Za-z0-9_.$@].
    Tokens split only at '.' and '@' (the forms the maps use: .p__sinit_x, @962, x.cpp)."""
    def hit(tok):
        if tok in keys:
            return tok, ""
        if decorated and tok[:1] == "_" and tok[1:] in keys:
            return tok[1:], "_"
        return None

    if "." not in r and "@" not in r:
        h = hit(r)
        return [(0, len(r), h[0], h[1])] if h else []
    h = hit(r)
    if h:
        return [(0, len(r), h[0], h[1])]
    if _SRCFILE_RE.search(r):
        return []                      # a file name, not a symbol
    marks = [i for i, ch in enumerate(r) if ch in ".@"]
    # '.' starts a token (.p__sinit_x) and so does the char after it (x.@984); '@' only
    # ends one (MSVC ?name@@YAX...): "1@262" in an input script is not the symbol @262.
    starts = sorted({0} | {i for i in marks if r[i] == "."} | {i + 1 for i in marks if r[i] == "."})
    ends = sorted({len(r)} | set(marks), reverse=True)
    out, pos = [], 0
    for s in starts:
        if s < pos:
            continue
        for e in ends:
            if e <= s:
                break
            h = hit(r[s:e])
            if h:
                out.append((s, e, h[0], h[1]))
                pos = e
                break
    return out


def _scan(text, lang, keys, paths=None):
    """Yield ("path", off, path, ctx) and ("ident", off, length, old, prefix, ctx) events."""
    for a, b, ctx in _segments(text, lang):
        symbolic = ctx != "code"
        for run in _RUN_RE.finditer(text, a, b):
            r = run.group(0)
            if "/" in r:
                stripped = r.rstrip(".-/")
                pm = _TREEPATH_RE.search(stripped) if _SRCFILE_RE.search(stripped) else None
                if pm:
                    yield ("path", run.start() + pm.start(1), pm.group(1), ctx)
                    continue
            for sub in _SUBRUN_RE.finditer(r):
                for s, e, old, pre in _ident_matches(sub.group(0), keys, symbolic):
                    yield ("ident", run.start() + sub.start() + s, e - s, old, pre, ctx)


_ALIAS_RE = re.compile(r'/alternatename:([^=\s"]+)=([^\s"]+)')


def _self_aliases(old_text, new_text):
    """[(offset of the line in old_text, alias)] for /alternatename X=X lines a swap made."""
    out, off = [], 0
    for before, after in zip(old_text.splitlines(True), new_text.splitlines(True)):
        if before != after:
            was = {m.group(1) for m in _ALIAS_RE.finditer(before) if m.group(1) == m.group(2)}
            for m in _ALIAS_RE.finditer(after):
                if m.group(1) == m.group(2) and m.group(1) not in was:
                    out.append((off, m.group(1)))
        off += len(before)
    return out


def apply_text(text, lang, maps, doc=False):
    """One pass over text. Returns (new_text, changes, items).
    changes: [(offset, old, new, tags, ctx)]; items: [(kind, offset, old, new, tags, ctx)]."""
    edits, changes, items = [], [], []
    for ev in _scan(text, lang, maps.keys):
        if ev[0] == "path":
            _, off, p, ctx = ev
            act = maps.path.get(p)
            if act and act[0] == "move":
                edits.append((off, off + len(p), act[1]))
                changes.append((off, p, act[1], ("srcmove",), ctx))
            elif act:
                items.append((act[0], off, p, act[1], ("srcmove",), ctx))
            continue
        _, off, length, old, pre, ctx = ev
        if old in maps.hand:
            items.append(("hand_listed", off, pre + old, "-", (maps.hand[old],), ctx))
            continue
        if old in maps.words:
            new, tags = maps.words[old]
            ls = text.rfind("\n", 0, off) + 1
            prose = ctx == "comment" or doc or (ctx == "list" and "#" in text[ls:off])
            if not prose and not (lang == "py" and ctx == "code"):
                items.append(("hand_word", off, pre + old, pre + new,
                              tuple(sorted(tags)) or ("plain",), ctx))
            continue
        new, tags = maps.row[old]
        block = _blocking(tags)
        rebind = _rebinding(tags)
        if ctx == "code" and (block or rebind):
            items.append(("hand", off, pre + old, pre + new, tuple(sorted(set(block + rebind))), ctx))
            continue
        if ctx == "string" and rebind:
            items.append(("hand", off, pre + old, pre + new, tuple(rebind), ctx))
            continue
        if ctx == "code" and new in maps.collide:
            items.append(("name_collision", off, old, new, tuple(sorted(tags)) or ("plain",), ctx))
            continue
        edits.append((off, off + length, pre + new))
        changes.append((off, pre + old, pre + new, tuple(sorted(tags)) or ("plain",), ctx))
        if block and ctx == "string" and lang != "list":
            items.append(("tagged_in_string", off, pre + old, pre + new, tuple(block), ctx))
    if not edits:
        return text, changes, items
    parts, last = [], 0
    for s, e, rep in sorted(edits):
        parts.append(text[last:s])
        parts.append(rep)
        last = e
    parts.append(text[last:])
    new_text = "".join(parts)
    for off, name in _self_aliases(text, new_text):
        items.append(("self_alias", off, name, name, ("alternatename",), "string"))
    return new_text, changes, items


def _lang(path):
    ext = path.suffix.lower()
    return "c" if ext in _C_EXTS else "py" if ext in _PY_EXTS else "list"


def iter_port_files(port):
    import os
    port = pathlib.Path(port)
    for dirpath, dirs, files in os.walk(port):
        relroot = pathlib.Path(dirpath).relative_to(port).as_posix()
        dirs[:] = sorted(d for d in dirs
                         if d not in (".git", "__pycache__")
                         and not (relroot == "." and d in APPLY_SKIP_DIRS))
        for f in sorted(files):
            if f in APPLY_SKIP_NAMES:
                continue
            p = pathlib.Path(dirpath) / f
            if _has_skip_marker(p):
                continue
            yield p, p.relative_to(port).as_posix()


def _has_skip_marker(path):
    try:
        with open(path, "rb") as fh:
            head = fh.read(2048)
    except OSError:
        return False
    return APPLY_SKIP_MARKER.encode() in head


def maps_digest(maps_dir):
    import hashlib
    h = hashlib.sha256()
    for name in ("renames_kind.tsv", "srcmoves.tsv", "decided.tsv"):
        h.update(name.encode() + b"\0" + (pathlib.Path(maps_dir) / name).read_bytes() + b"\0")
    return h.hexdigest()


def maps_revs(maps_dir):
    """(pin_rev, target_rev) from the maps' target.txt (PIN=, TARGET=); "-" when absent."""
    revs = {"PIN": "-", "TARGET": "-"}
    f = pathlib.Path(maps_dir) / "target.txt"
    if f.is_file():
        for line in f.read_text(encoding="utf-8").splitlines():
            k, _, v = line.partition("=")
            if k in revs and v.strip():
                revs[k] = v.strip()
    return revs["PIN"], revs["TARGET"]


def read_stamp(port):
    f = pathlib.Path(port) / APPLY_STAMP
    if not f.is_file():
        return None
    out = {}
    for line in f.read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            k, _, v = line.partition(" ")
            out[k] = v.strip()
    return out


def write_stamp(port, digest, pin_rev, target_rev):
    (pathlib.Path(port) / APPLY_STAMP).write_text(
        "# port/tools/catchup_maps.py apply: these maps are applied to this tree.\n"
        "# Commit this file with the apply. A second apply of the same maps changes nothing;\n"
        "# other maps are refused unless their pin is this target (the next bump) or --force.\n"
        f"maps_sha256 {digest}\npin_rev {pin_rev}\ntarget_rev {target_rev}\n",
        encoding="utf-8", newline="\n")


def _read_port_text(path):
    data = path.read_bytes()
    if b"\0" in data:
        return None, "binary"
    try:
        return data.decode("utf-8"), None
    except UnicodeDecodeError:
        return None, "undecodable"


def prescan(port, maps):
    """Before any write: which map names the port already spells.
    Returns (present: names seen in any context, code: names seen in C/C++ code)."""
    wanted = set(maps.row) | {n for n, _ in maps.row.values()} | set(maps.words)
    present, code = set(), set()
    for path, _ in iter_port_files(port):
        text, _ = _read_port_text(path)
        if text is None:
            continue
        lang = _lang(path)
        for ev in _scan(text, lang, wanted):
            if ev[0] == "ident":
                present.add(ev[3])
                if lang == "c" and ev[5] == "code" and not ev[4]:
                    code.add(ev[3])
    return present, code


def looks_applied(maps, present):
    """Why a tree with no stamp already looks applied ("" when it does not): a chain tail
    (a new name no row renames further) is present, or more plain rows have only their new
    name present than have their old name present."""
    tails = sorted(t for t in maps.tails if t in present)
    if tails:
        return "chain tail(s) already present: " + ",".join(tails)
    plain = [(o, n) for o, (n, t) in maps.row.items() if not t]
    old_seen = sum(1 for o, _ in plain if o in present)
    new_only = sum(1 for o, n in plain if n in present and o not in present)
    if new_only > old_seen:
        return f"{new_only} plain rows show only the new name, {old_seen} still show the old one"
    return ""


def load_apply_maps(maps_dir, port, target=None, force=False):
    """Returns (maps, info) or raises ApplyRefused. info: digest, revs, stamp state."""
    maps_dir = pathlib.Path(maps_dir)
    rows = [(m, a, o, n, set(t.split(",")) - {"plain"})
            for m, a, o, n, t in _read_tsv(maps_dir / "renames_kind.tsv")]
    decided = {r[3] for r in _read_tsv(maps_dir / "decided.tsv")}
    hand = {}
    if (maps_dir / "hand.tsv").is_file():
        for r in _read_tsv(maps_dir / "hand.tsv"):
            if r[1] != "-":
                hand.setdefault(r[3], r[0])
    moves = [tuple(r) for r in _read_tsv(maps_dir / "srcmoves.tsv")]
    digest = maps_digest(maps_dir)
    pin_rev, target_rev = maps_revs(maps_dir)
    stamp = read_stamp(port)
    if stamp is None:
        state = "none"
    elif stamp.get("maps_sha256") == digest:
        state = "matched"
    elif pin_rev != "-" and stamp.get("target_rev") == pin_rev:
        state = "next_bump"
    else:
        state = "other"
    owned_count = old_exists = None
    if target:
        res = SrcpathResolver(target)
        news = sorted({n for _, o, n in moves if n not in ("-", o)})
        _, _, owned = res.batch([], [], news)
        owned_count = lambda p: len(owned.get(p) or ())  # noqa: E731
        old_exists = lambda p: (pathlib.Path(target) / p).is_file()  # noqa: E731
    probe = ApplyMaps(rows, decided, hand, moves, owned_count, old_exists)
    present, code = prescan(port, probe)
    why = ""
    if state == "other":
        why = (f"stamp {APPLY_STAMP} records other maps (sha {stamp.get('maps_sha256')}, "
               f"target {stamp.get('target_rev')}) and these maps' pin is {pin_rev}, not that target")
    elif state == "none":
        why = looks_applied(probe, present)
        if why:
            why = f"no {APPLY_STAMP} stamp but the tree already looks applied: {why}"
    if why and not force:
        raise ApplyRefused(why)
    maps = ApplyMaps(rows, decided, hand, moves, owned_count, old_exists,
                     applied=state == "matched", collide=code & {n for n, _ in probe.row.values()})
    return maps, {"digest": digest, "pin_rev": pin_rev, "target_rev": target_rev,
                  "stamp_state": state, "forced": why}


class ApplyRefused(Exception):
    pass


def apply_port(port, maps, dry_run=True):
    """Returns (files_changed, all_changes, all_items, skipped) with file:line rows."""
    import bisect
    files_changed, all_changes, all_items = [], [], []
    skipped = collections.Counter()
    scanned = 0
    for path, rel in iter_port_files(port):
        text, why = _read_port_text(path)
        if text is None:
            skipped[why] += 1
            continue
        scanned += 1
        new, changes, items = apply_text(text, _lang(path), maps, path.suffix.lower() in _DOC_EXTS)
        if not changes and not items:
            continue
        nl = [i for i, ch in enumerate(text) if ch == "\n"]

        def line(off):
            return bisect.bisect_right(nl, off - 1) + 1

        all_changes += [(rel, line(o), old, nw, ",".join(t), ctx) for o, old, nw, t, ctx in changes]
        all_items += [(k, rel, line(o), old, nw, ",".join(t), ctx) for k, o, old, nw, t, ctx in items]
        if new != text:
            files_changed.append(rel)
            if not dry_run:
                path.write_bytes(new.encode("utf-8"))
    skipped["scanned"] = scanned
    return files_changed, all_changes, all_items, skipped


def apply_main(argv):
    ap = argparse.ArgumentParser(prog="catchup_maps.py apply",
                                 description="Apply renames_kind.tsv and srcmoves.tsv to a port tree.")
    ap.add_argument("--maps", required=True,
                    help="dir with renames_kind.tsv, srcmoves.tsv, decided.tsv, target.txt "
                         "(hand.tsv optional)")
    ap.add_argument("--port", required=True, help="the port directory to rewrite")
    ap.add_argument("--report", required=True, help="report file (hand items); changes go to <report>.changes.tsv")
    ap.add_argument("--target", help="optional: checkout of the bump target, to tell a promoted TU from "
                                     "a one-symbol move by what the target file owns")
    ap.add_argument("--dry-run", action="store_true", help="write only the report")
    ap.add_argument("--force", action="store_true",
                    help="apply although the stamp records other maps or the tree looks applied")
    a = ap.parse_args(argv)
    port = pathlib.Path(a.port)
    try:
        maps, info = load_apply_maps(a.maps, port, a.target, a.force)
    except ApplyRefused as e:
        print(f"catchup_maps apply: REFUSED, nothing written: {e}", file=sys.stderr)
        return 2
    changed, changes, items, skipped = apply_port(port, maps, a.dry_run)
    if not a.dry_run:
        write_stamp(port, info["digest"], info["pin_rev"], info["target_rev"])

    by_tag = collections.Counter(t.split(":")[0] for c in changes for t in c[4].split(","))
    by_kind = collections.Counter(i[0] for i in items)
    item_tags = collections.Counter(t.split(":")[0] for i in items for t in i[5].split(","))
    per_file = collections.Counter(i[1] for i in items)
    head = [f"catchup_maps apply report ({'dry run' if a.dry_run else 'applied'})",
            f"maps_sha256 {info['digest']}", f"pin_rev {info['pin_rev']}", f"target_rev {info['target_rev']}",
            f"stamp_before {info['stamp_state']}",
            f"forced {info['forced'] or '-'}",
            f"word_rows {len(maps.words)} ({','.join(sorted(maps.words))}): never swapped",
            f"chain_rows {len(maps.chain)} ({'switched off: already applied' if maps.applied else 'active'})",
            f"collide_names {len(maps.collide)}",
            f"target_checkout {'given' if a.target else 'not given (TU test counts srcmoves rows)'}",
            f"files_scanned {skipped['scanned']}", f"files_skipped_binary {skipped['binary']}",
            f"files_skipped_undecodable {skipped['undecodable']}",
            f"files_changed {len(changed)}", f"replacements {len(changes)}",
            f"files_with_items {len(per_file)}", f"items {len(items)}"]
    head += [f"replacements.tag {k} {v}" for k, v in sorted(by_tag.items())]
    head += [f"items.kind {k} {v}" for k, v in sorted(by_kind.items())]
    head += [f"items.tag {k} {v}" for k, v in sorted(item_tags.items())]
    head += [f"items.top_file {f} {n}" for f, n in per_file.most_common(20)]
    report = pathlib.Path(a.report)
    report.parent.mkdir(parents=True, exist_ok=True)
    body = "".join("\t".join(map(str, r)) + "\n" for r in sorted(items, key=lambda r: (r[1], r[2], r[0])))
    report.write_text("".join(f"# {h}\n" for h in head)
                      + "#kind\tfile\tline\told\tnew\ttags\tcontext\n" + body,
                      encoding="utf-8", newline="\n")
    ch = pathlib.Path(str(report) + ".changes.tsv")
    ch.write_text("#file\tline\told\tnew\ttags\tcontext\n"
                  + "".join("\t".join(map(str, r)) + "\n" for r in changes),
                  encoding="utf-8", newline="\n")
    print("\n".join(head))
    return 0


def main(argv=None):
    argv = sys.argv[1:] if argv is None else list(argv)
    if argv[:1] == ["apply"]:
        return apply_main(argv[1:])
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--pin", required=True, help="read-only checkout of the pinned decomp")
    ap.add_argument("--target", required=True, help="read-only checkout of the bump target")
    ap.add_argument("--port", required=True, help="the port directory (holds hal/, slice_*.txt, decomp_patches/)")
    ap.add_argument("--out", required=True, action="append", help="output dir (repeatable)")
    ap.add_argument("--git-repo", help="optional: a git repo holding both revisions, for file renames")
    ap.add_argument("--pin-rev", help="with --git-repo: the pin revision")
    ap.add_argument("--target-rev", help="with --git-repo: the target revision")
    a = ap.parse_args(argv)
    outs = [pathlib.Path(o) for o in a.out]
    for o in outs:
        o.mkdir(parents=True, exist_ok=True)
    port = pathlib.Path(a.port)

    pin, tgt = SymbolTable.load(a.pin), SymbolTable.load(a.target)
    pin_res, tgt_res = SrcpathResolver(a.pin), SrcpathResolver(a.target)
    scope = collect_scope(port, pin, pin_res)
    ren = build_renames(pin, tgt)        # every pin address; scope.tsv is a report only
    moves = build_srcmoves(ren.locations, pin_res, tgt_res)
    renamed_files = (git_file_renames(a.git_repo, a.pin_rev, a.target_rev)
                     if a.git_repo and a.pin_rev and a.target_rev else {})
    follow = ((lambda p: git_follow_deleted(a.git_repo, a.pin_rev, a.target_rev, p))
              if a.git_repo and a.pin_rev and a.target_rev else None)
    pmap = map_patch_paths(patch_paths(port), pin, pin_res, tgt_res, a.target, renamed_files, follow)

    _write(outs, "renames.tsv", ("module", "addr", "old_name", "new_name"),
           [(m, f"0x{ad:08x}", o, n) for m, ad, o, n in ren.renames])
    _write(outs, "renames_kind.tsv", ("module", "addr", "old_name", "new_name", "tags"),
           [(m, f"0x{ad:08x}", o, n, ",".join(sorted(ren.tags[(m, ad, o)])) or "plain")
            for m, ad, o, n in ren.renames])
    _write(outs, "decided.tsv", ("kind", "module", "addr", "name", "action"),
           sorted((k, m, f"0x{ad:08x}", n, d) for k, m, ad, n, d in ren.decided))
    _write(outs, "srcmoves.tsv", ("symbol", "old_path", "new_path"), sorted(moves.rows))
    _write(outs, "patchmap.tsv", ("path", "status", "new_paths", "unowned"), pmap)
    hand = [(k, m, f"0x{ad:08x}", n, d) for k, m, ad, n, d in ren.hand]
    hand += [("patch_path_missing", "-", "-", row[0],
              "patched decomp file is gone at the target and no rename was detected")
             for row in pmap if row[1] == "missing"]
    _write(outs, "hand.tsv", ("kind", "module", "addr", "name", "detail"), sorted(hand))
    _write(outs, "scope.tsv", ("name", "categories"),
           [(n, ",".join(sorted(c))) for n, c in sorted(scope.items())])

    cat_counts = collections.Counter(c for cats in scope.values() for c in cats)
    pstat = collections.Counter(row[1].split("@")[0] for row in pmap)
    lines = [f"pin_symbols\t{len(pin.where)}", f"target_symbols\t{len(tgt.where)}"]
    lines += [f"scope.{k}\t{v}" for k, v in sorted(cat_counts.items())]
    lines += [f"renames.{k}\t{v}" for k, v in sorted(ren.counts.items())]
    tag_counts = collections.Counter(t.split(":")[0] for tags in ren.tags.values() for t in tags)
    lines += [f"tags.{k}\t{v}" for k, v in sorted(tag_counts.items())]
    lines.append(f"tags.plain\t{sum(1 for t in ren.tags.values() if not t)}")
    lines.append(f"renames.in_port_scope\t{sum(1 for _, _, o, _ in ren.renames if o in scope)}")
    lines += [f"hand.{k}\t{v}" for k, v in sorted(collections.Counter(h[0] for h in hand).items())]
    lines += [f"srcmoves.{k}\t{v}" for k, v in sorted(moves.counts.items())]
    lines += [f"patches.{k}\t{v}" for k, v in sorted(pstat.items())]
    lines.append(f"patches.total\t{len(pmap)}")
    for o in outs:
        (o / "counts.txt").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())
