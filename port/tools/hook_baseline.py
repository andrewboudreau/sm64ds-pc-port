#!/usr/bin/env python3
"""The hook placement check against a recorded baseline, so the battery can run it (catch-up plan Task 4.4).

hook_anchor_check.py compares every HOOK patch BEFORE a decomp bump (the old patch on the old decomp) with
the same change AFTER it (the re-cut patch on the new decomp). Its before side needs three things a normal
checkout does not have: the old patch set, the old decomp's files those patches apply to (and its
config/**/symbols.txt), and the old decomp_edits.tsv that says which patches are HOOK patches. The last one
matters most: port/decomp_edits.tsv is rebuilt after a bump and keyed by the new paths, and a table that no
longer lists a moved HOOK patch drops it from the check without an error.

So the before side is recorded ONCE, at the bump, into port/hook_baseline/:

  stage0.tar.gz   before/   the old decomp's files every old patch applies to, plus its config/**/symbols.txt
                  patches/  the old port/decomp_patches (BASE, README.md and every .patch)
                  edits.tsv the old port/decomp_edits.tsv, frozen
                  maps/     renames.tsv, renames_kind.tsv, patchmap.tsv of the bump
  MANIFEST.txt    what was recorded from where, the tarball's sha256, and the HOOK patch count the report
                  must cover (a smaller count means a HOOK patch fell out of the check)
  allow.tsv       hook_anchor_check's allow file: one row per hunk moved with its ROM logic or dropped
                  because main covers it ("<before patch rel path><TAB><hunk | * | after:n><TAB><reason>")

    python port/tools/hook_baseline.py record --repo <checkout> --pin <old decomp rev> --stage0 <old port rev>
                                              --maps <bump maps dir> [--out port/hook_baseline]
                                              [--frozen <plain/custom table>]
    python port/tools/hook_baseline.py check [--decomp-root <dir>] [--report <tsv>]

check unpacks the tarball once into build/hook_baseline/<sha> (verified against MANIFEST.txt), runs
hook_anchor_check with the after side = the decomp root and port/decomp_patches, and passes only when the
check is green (zero MOVED, zero UNKNOWN, every intentional move in allow.tsv) AND the report covers the
recorded HOOK patch count. The next bump records a new baseline over this one.

PLAIN HALF. This repo must not carry the custom patches, so record --frozen keeps the plain half of the
baseline only, split by the frozen plain/custom table (keyed by the old port revision's patch paths, a mixed
row naming its custom units as patch file lines):

  stage0.tar.gz, MANIFEST.txt, allow.tsv
        every plain patch, and every mixed patch cut to its plain hunks (the table's custom units are taken
        out and the hunk headers renumbered); edits.tsv lists those patches only, a mixed row marked plain

The hunk numbers in allow.tsv are the plain half's: a cut patch's hunks are renumbered, so a row is keyed to
the hunk as this baseline holds it. check reads the plain half and covers its HOOK count.
"""

import argparse
import hashlib
import io
import os
import re
import subprocess
import sys
import tarfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import decomp_root  # noqa: E402
import hook_anchor_check as hac  # noqa: E402

PORT = HERE.parent
DEFAULT_DIR = PORT / "hook_baseline"
TARBALL = "stage0.tar.gz"
CUSTOM_TARBALL = "stage0_custom.tar.gz"
CUSTOM_ALLOW = "allow_custom.tsv"
PATCH_PREFIX = "port/decomp_patches/"
MAPS_FILES = ("renames.tsv", "renames_kind.tsv", "patchmap.tsv")


def _git(repo, *args, binary=False):
    r = subprocess.run(["git", "-C", str(repo)] + list(args), capture_output=True)
    if r.returncode:
        raise SystemExit("hook_baseline: git %s failed: %s" % (" ".join(args), r.stderr.decode(errors="replace")))
    return r.stdout if binary else r.stdout.decode("utf-8", errors="replace")


def _add(tar, name, data):
    info = tarfile.TarInfo(name)
    info.size = len(data)
    info.mtime = 0
    info.mode = 0o644
    tar.addfile(info, io.BytesIO(data))


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def hook_count(edits_text, rels):
    kinds = {}
    for line in edits_text.splitlines():
        cols = line.split("\t")
        if len(cols) >= 3 and cols[0] != "path" and not line.startswith("#"):
            kinds[cols[0]] = cols[2]
    return sum(1 for r in rels if kinds.get(r) == "HOOK")


def write_tarball(path, members):
    """members: [(name, bytes)] in order. Deterministic: mtime 0 in the tar and the gzip header."""
    import gzip
    buf = io.BytesIO()
    with gzip.GzipFile(fileobj=buf, mode="wb", mtime=0, compresslevel=9) as gz:
        with tarfile.open(fileobj=gz, mode="w", format=tarfile.PAX_FORMAT) as tar:
            for name, data in members:
                _add(tar, name, data)
    Path(path).write_bytes(buf.getvalue())


def read_tarball(path):
    with tarfile.open(path, "r:gz") as tar:
        return [(m.name, tar.extractfile(m).read()) for m in tar.getmembers() if m.isfile()]


def write_manifest(path, lines):
    Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def _ensure_allow(path, what):
    if not Path(path).exists():
        Path(path).write_text("# hook_anchor_check allow rows for %s: <before patch rel path>\\t<hunk|*|after:n>"
                              "\\t<reason>\n" % what, encoding="utf-8", newline="\n")


# ----------------------------------------------------------------------------- the plain/custom split

def read_frozen(path):
    """{patch rel (no .patch): ("custom", None) | ("mixed", set of custom patch file lines)} from the frozen
    plain/custom table; plain patches are absent. A mixed row names its custom units as "patch L<a>-<b>"."""
    out = {}
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        cols = line.split("\t")
        if len(cols) < 3 or not cols[0].startswith(PATCH_PREFIX) or not cols[0].endswith(".patch"):
            continue
        rel = cols[0][len(PATCH_PREFIX):-len(".patch")]
        if cols[1] == "custom":
            out[rel] = ("custom", None)
        elif cols[1] == "mixed":
            lines = set()
            for lo, hi in re.findall(r"patch L(\d+)-(\d+)", cols[2]):
                lines.update(range(int(lo), int(hi) + 1))
            if not lines:
                raise SystemExit("hook_baseline: mixed row %s names no custom patch lines" % cols[0])
            out[rel] = ("mixed", lines)
        elif cols[1] != "plain":
            raise SystemExit("hook_baseline: frozen table side %r for %s" % (cols[1], cols[0]))
    return out


HUNK = re.compile(rb"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@(.*)$", re.S)


def cut_patch(data, drop):
    """The patch with the custom unit at the 1-based patch file line numbers in drop taken out: a '+' line
    there is removed, a '-' line there is kept as context (the custom unit removed it), a context line stays
    (a unit is a range of new file lines, so it can span the unchanged line between two added ones). Hunk
    headers are renumbered (the new side's count and start); a hunk left with no change is dropped whole."""
    lines = data.splitlines(keepends=True)
    bad = sorted(n for n in drop if n < 1 or n > len(lines))
    if bad:
        raise SystemExit("hook_baseline: cut lines outside the patch: %s" % bad[:8])
    out = []
    state = {"hunk": None, "shift": 0}

    def flush():
        hunk = state["hunk"]
        if hunk is None:
            return
        head, body, removed = hunk
        bare = head.rstrip(b"\r\n")
        m = HUNK.match(bare)
        eol = head[len(bare):]
        old_s, old_n = int(m.group(1)), int(m.group(2) if m.group(2) is not None else 1)
        new_s, new_n = int(m.group(3)), int(m.group(4) if m.group(4) is not None else 1)
        if any(l[:1] in (b"+", b"-") for l in body):
            out.append(b"@@ -%d,%d +%d,%d @@%s%s" % (old_s, old_n, new_s - state["shift"], new_n - removed,
                                                     m.group(5), eol))
            out.extend(body)
        state["shift"] += removed

    i = 0
    while i < len(lines):
        line = lines[i]
        if line.startswith(b"@@ "):
            flush()
            state["hunk"] = [line, [], 0]
        elif state["hunk"] is None:
            out.append(line)
        elif i + 1 in drop and line.startswith(b"+"):
            state["hunk"][2] += 1
            if i + 1 < len(lines) and lines[i + 1].startswith(b"\\"):
                i += 1                      # a "\ No newline" marker belongs to the line it follows
        elif i + 1 in drop and line.startswith(b"-"):
            state["hunk"][2] -= 1           # the new side keeps the line again
            state["hunk"][1].append(b" " + line[1:])
        else:
            state["hunk"][1].append(line)
        i += 1
    flush()
    return b"".join(out)


def _rel_of(name, prefix, suffix=""):
    return name[len(prefix):len(name) - len(suffix)]


def split_members(members, frozen):
    """(plain members, custom members, stats) from one full baseline's members."""
    custom_rels = {r for r, (side, _l) in frozen.items() if side == "custom"}
    mixed = {r: l for r, (side, l) in frozen.items() if side == "mixed"}
    held = {_rel_of(n, "patches/", ".patch") for n, _d in members if n.startswith("patches/") and n.endswith(".patch")}
    missing = sorted((custom_rels | set(mixed)) - held)
    if missing:
        raise SystemExit("hook_baseline: frozen table names patches the baseline does not hold: %s" % missing[:8])
    plain, custom = [], []
    stats = {"custom": 0, "mixed": 0, "plain": 0}
    for name, data in members:
        if name.startswith("patches/") and name.endswith(".patch"):
            rel = _rel_of(name, "patches/", ".patch")
            if rel in custom_rels:
                custom.append((name, data))
                stats["custom"] += 1
            elif rel in mixed:
                plain.append((name, cut_patch(data, mixed[rel])))
                custom.append((name, data))
                stats["mixed"] += 1
            else:
                plain.append((name, data))
                stats["plain"] += 1
        elif name.startswith("before/") and not name.endswith("symbols.txt"):
            rel = _rel_of(name, "before/")
            if rel in custom_rels:
                custom.append((name, data))
            elif rel in mixed:
                plain.append((name, data))
                custom.append((name, data))
            else:
                plain.append((name, data))
        elif name == "edits.tsv":
            p_rows, c_rows = [], []
            for line in data.decode("utf-8").splitlines(keepends=True):
                cols = line.rstrip("\r\n").split("\t")
                if line.startswith("#") or cols[0] == "path":
                    p_rows.append(line)
                    c_rows.append(line)
                elif cols[0] in custom_rels or (len(cols) > 3 and cols[3] == "custom"):
                    c_rows.append(line)         # a custom patch, or a custom row with no patch (an added file)
                elif cols[0] in mixed:
                    c_rows.append(line)
                    cols[3] = "plain"
                    cols[6] = "plain hunks only: the custom units are cut out of this baseline's patch"
                    p_rows.append("\t".join(cols) + "\n")
                else:
                    p_rows.append(line)
            plain.append((name, "".join(p_rows).encode("utf-8")))
            custom.append(("edits_custom.tsv", "".join(c_rows).encode("utf-8")))
        else:
            plain.append((name, data))
    return plain, custom, stats


def write_split(out, members, frozen, head):
    """Write the plain half (stage0.tar.gz, MANIFEST.txt) of one full baseline; the custom members are
    counted, not written. head: the manifest's pin/stage0/maps lines."""
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    plain, custom, stats = split_members(members, frozen)
    write_tarball(out / TARBALL, plain)
    p_rels = [_rel_of(n, "patches/", ".patch") for n, _d in plain if n.startswith("patches/") and n.endswith(".patch")]
    p_edits = dict(plain)["edits.tsv"].decode("utf-8")
    n_hook = hook_count(p_edits, p_rels)
    write_manifest(out / "MANIFEST.txt", [
        "# hook_baseline MANIFEST, plain half (written by port/tools/hook_baseline.py; do not hand edit)"] + head + [
        "patches\t%d" % len(p_rels),
        "hook_patches\t%d" % n_hook,
        "before_files\t%d" % sum(1 for n, _d in plain if n.startswith("before/") and not n.endswith("symbols.txt")),
        "cut_patches\t%d" % stats["mixed"],
        "symbol_files\t%d" % sum(1 for n, _d in plain if n.startswith("before/") and n.endswith("symbols.txt")),
        "sha256\t%s" % sha256(out / TARBALL),
    ])
    _ensure_allow(out / "allow.tsv", "this baseline")
    print("hook_baseline: plain half -> %d patches (%d cut to plain hunks, %d HOOK); %d custom patches left out" % (
        len(p_rels), stats["mixed"], n_hook, stats["custom"]))
    return stats


def record(repo, pin, stage0, maps, out, frozen=None):
    """Write <out>/stage0.tar.gz and <out>/MANIFEST.txt (and the custom half when frozen is given).
    Deterministic: sorted members, mtime 0."""
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    listing = _git(repo, "ls-tree", "-r", "--name-only", stage0, "port/decomp_patches").split("\n")
    patch_files = sorted(p for p in listing if p)
    rels = sorted(p[len("port/decomp_patches/"):-len(".patch")] for p in patch_files if p.endswith(".patch"))
    syms = sorted(p for p in _git(repo, "ls-tree", "-r", "--name-only", pin, "config").split("\n")
                  if p.endswith("symbols.txt"))
    edits = _git(repo, "show", "%s:port/decomp_edits.tsv" % stage0, binary=True)
    missing = []
    members = []
    for p in patch_files:
        members.append(("patches/" + p[len("port/decomp_patches/"):],
                        _git(repo, "show", "%s:%s" % (stage0, p), binary=True)))
    for rel in rels:
        try:
            data = _git(repo, "show", "%s:%s" % (pin, rel), binary=True)
        except SystemExit:
            missing.append(rel)       # an overlay-only path: no decomp file at the pin
            continue
        members.append(("before/" + rel, data))
    for s in syms:
        members.append(("before/" + s, _git(repo, "show", "%s:%s" % (pin, s), binary=True)))
    members.append(("edits.tsv", edits))
    for name in MAPS_FILES:
        members.append(("maps/" + name, (Path(maps) / name).read_bytes()))
    head = [
        "pin\t%s" % _git(repo, "rev-parse", pin).strip(),
        "stage0\t%s" % _git(repo, "rev-parse", stage0).strip(),
        "maps\t%s" % ",".join("%s=%s" % (n, hashlib.sha256((Path(maps) / n).read_bytes()).hexdigest()[:16])
                              for n in MAPS_FILES),
    ]
    if frozen is not None:
        write_split(out, members, read_frozen(frozen), head)
        return 0
    write_tarball(out / TARBALL, members)
    n_hook = hook_count(edits.decode("utf-8", errors="replace"), rels)
    lines = ["# hook_baseline MANIFEST (written by port/tools/hook_baseline.py record; do not hand edit)"] + head + [
        "patches\t%d" % len(rels),
        "hook_patches\t%d" % n_hook,
        "before_files\t%d" % (len(rels) - len(missing)),
        "overlay_only\t%d" % len(missing),
        "symbol_files\t%d" % len(syms),
        "sha256\t%s" % sha256(out / TARBALL),
    ]
    write_manifest(out / "MANIFEST.txt", lines)
    _ensure_allow(out / "allow.tsv", "this baseline")
    print("hook_baseline: recorded %d patches (%d HOOK), %d before files, %d symbol files -> %s (%s)" % (
        len(rels), n_hook, len(rels) - len(missing), len(syms), out / TARBALL, lines[-1].split("\t")[1][:16]))
    return 0




# ----------------------------------------------------------------------------- check

def read_manifest(d, name="MANIFEST.txt"):
    m = {}
    for line in (Path(d) / name).read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#") and "\t" in line:
            k, v = line.split("\t", 1)
            m[k] = v
    return m


def _extract(tb, dest):
    with tarfile.open(tb, "r:gz") as tar:
        for member in tar.getmembers():
            if member.name.startswith("/") or ".." in Path(member.name).parts:
                raise SystemExit("hook_baseline: refusing member %s" % member.name)
        tar.extractall(dest)


def _verified(tb, want):
    got = sha256(tb)
    if got != want:
        raise SystemExit("hook_baseline: %s sha256 %s does not match its manifest %s" % (tb, got, want))
    return got


def merge_edits(plain_text, custom_text):
    """The plain half's edits rows with the custom half's rows laid over them by path (a mixed patch's
    recorded row replaces its plain-cut row), custom-only rows appended."""
    over = {}
    for line in custom_text.splitlines(keepends=True):
        cols = line.split("\t")
        if not line.startswith("#") and cols[0] != "path" and len(cols) >= 3:
            over[cols[0]] = line
    out = []
    for line in plain_text.splitlines(keepends=True):
        out.append(line if line.startswith("#") else over.pop(line.split("\t")[0], line))
    out.extend(over[k] for k in sorted(over))
    return "".join(out)




def unpack(d, cache_root):
    """(unpacked dir, plain manifest, HOOK count to cover, allow file or None, custom half read)."""
    d = Path(d)
    m = read_manifest(d)
    got = _verified(d / TARBALL, m.get("sha256"))
    want = int(m.get("hook_patches", "0"))
    allows = [d / "allow.tsv"]
    key = got[:16]
    dest = Path(cache_root) / key
    stamp = dest / ".complete"
    if not stamp.exists():
        _extract(d / TARBALL, dest)
        stamp.write_text(key, encoding="utf-8")
    allow = None
    parts = [a.read_bytes() for a in allows if a.exists()]
    if parts:
        allow = dest / ".allow.tsv"
        allow.write_bytes(b"".join(p if p.endswith(b"\n") else p + b"\n" for p in parts))
    return dest, m, want, allow, False


def check(d, decomp, after_patches, cache_root, report):
    dest, m, want, allow, with_custom = unpack(d, cache_root)
    args = argparse.Namespace(
        before_root=str(dest / "before"), after_root=str(decomp),
        before_patches=str(dest / "patches"), after_patches=str(after_patches),
        maps=str(dest / "maps"), allow=str(allow) if allow else None,
        report=str(report), edits=str(dest / "edits.tsv"), kinds="HOOK")
    rc = hac.run(args)
    covered = None
    for line in Path(report).read_text(encoding="utf-8").splitlines():
        if line.startswith("# hook_patches\t"):
            covered = int(line.split("\t")[1])
    if covered != want:
        print("hook_baseline: the report covers %s HOOK patches, the baseline recorded %d" % (covered, want))
        rc = 1
    if with_custom:
        shape = "plain + custom halves"
    elif "cut_patches" in m:
        shape = "plain half only"
    else:
        shape = "one full baseline"
    print("hook_baseline: %s (%s HOOK patches of %d recorded, %s, baseline pin %s)" % (
        "PASS" if rc == 0 else "FAIL", covered, want, shape, m.get("pin", "?")[:11]))
    return rc


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("record")
    r.add_argument("--repo", required=True)
    r.add_argument("--pin", required=True, help="the old decomp revision the old patches apply to")
    r.add_argument("--stage0", required=True, help="the old port revision (its decomp_patches and decomp_edits.tsv)")
    r.add_argument("--maps", required=True, help="the bump's maps dir (renames.tsv, renames_kind.tsv, patchmap.tsv)")
    r.add_argument("--out", default=str(DEFAULT_DIR))
    r.add_argument("--frozen", default=None, help="the frozen plain/custom table: record the plain half")
    c = sub.add_parser("check")
    c.add_argument("--baseline", default=str(DEFAULT_DIR))
    c.add_argument("--decomp-root", default=None, help="default: DECOMP_ROOT (decomp_root.find())")
    c.add_argument("--patches", default=str(PORT / "decomp_patches"))
    c.add_argument("--cache", default=None, help="default: <port tree>/build/hook_baseline")
    c.add_argument("--report", default=None, help="default: <cache>/report.tsv")
    a = ap.parse_args(argv)
    if a.cmd == "record":
        return record(a.repo, a.pin, a.stage0, a.maps, a.out, a.frozen)
    decomp = a.decomp_root or decomp_root.find()
    cache = Path(a.cache or os.path.join(decomp_root.PORT_TREE, "build", "hook_baseline"))
    report = a.report or str(cache / "report.tsv")
    return check(a.baseline, decomp, a.patches, cache, report)


if __name__ == "__main__":
    sys.exit(main())
