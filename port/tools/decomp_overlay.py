#!/usr/bin/env python3
"""The port's decomp patch layer.

The port builds the decomp's own `src/` and `include/` files, but some of them need host edits.
Those edits live here as one unified diff per decomp file instead of inside the decomp tree:

    port/decomp_patches/<decomp relative path>.patch   one patch per decomp file (a/ b/ prefixes)
    port/decomp_patches/BASE                           the decomp commit the patches were cut against
    port/decomp_overlay_files/<decomp relative path>   whole files for paths the decomp does not have

Commands:

    apply --decomp-root <dir> --out <overlay_dir> [--include-closure include]
        Write the patched copy of every patched file and a verbatim copy of every overlay file to
        <overlay_dir>/<path>, then <overlay_dir>/overlay_manifest.txt (sorted relative paths).
        Nothing is written unless every patch applies EXACTLY: a hunk that would only fit at an
        offset, or with fuzz, is a failure (exit 1, the failing paths are named). Files written by
        a previous apply that are no longer in the manifest are removed; nothing else is touched.
        The decomp root is only ever read. --include-closure also copies, unmodified, every file
        under that dir that reaches an overlay file through a quoted include (include_closure()).

    check --decomp-root <dir>
        The same as apply without writing anything. Exit 0 when everything applies exactly.

    cut --decomp-root <dir> --edited-root <dir> --paths <listing> [--base <commit>]
        For each relative path in <listing> (one per line, '#' comments allowed): write the patch
        (edited vs decomp); a path the decomp lacks becomes an overlay file instead; a path that
        is identical on both sides loses its patch.

Files are handled as bytes: a missing final newline and non-UTF-8 bytes (Shift-JIS comments)
survive. Line endings follow git: patches and overlay files are stored in the form git stores
(a uniformly CRLF file is turned into LF first; a file with mixed line endings is kept byte for
byte), so one patch applies to a CRLF checkout (Windows, core.autocrlf=true) and an LF checkout
(Linux, a fresh clone with autocrlf off). Outputs take the target's convention: a CRLF decomp
file gives a CRLF result. Patches are ordinary unified diffs that `git apply` accepts on an LF
tree; this tool applies them itself so that offsets and fuzz can be refused outright.
Python 3 standard library only.
"""

import argparse
import difflib
import os
import re
import sys
from pathlib import Path

PORT_DIR = Path(__file__).resolve().parent.parent
PATCHES_NAME = "decomp_patches"
FILES_NAME = "decomp_overlay_files"
MANIFEST_NAME = "overlay_manifest.txt"
PATCH_SUFFIX = ".patch"
CONTEXT = 3
NO_NEWLINE = b"\\ No newline at end of file\n"
GITATTRIBUTES = (b"# Written by port/tools/decomp_overlay.py. These files are byte-exact: never let\n"
                 b"# git convert their line endings (core.autocrlf would corrupt CRLF patches).\n"
                 b"* -text\n")
# names at the top of the layer dirs that belong to the layer itself, not to the decomp tree
PATCHES_RESERVED = {"BASE", "README.md", ".gitattributes"}
FILES_RESERVED = {".gitattributes"}

HUNK_RE = re.compile(rb"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@")


class OverlayError(Exception):
    """A failure tied to one decomp relative path."""

    def __init__(self, rel, msg):
        super().__init__("%s: %s" % (rel, msg))
        self.rel = rel


# ---------------------------------------------------------------------------
# paths and file io

def long_path(p):
    """An OS path that survives Windows' 260 character limit."""
    s = os.path.abspath(str(p))
    if os.name != "nt" or s.startswith("\\\\?\\"):
        return s
    if s.startswith("\\\\"):
        return "\\\\?\\UNC\\" + s[2:]
    return "\\\\?\\" + s


def check_rel(rel):
    """Validate a decomp relative path and return it with forward slashes."""
    rel = rel.replace("\\", "/")
    parts = rel.split("/")
    if not rel or rel.startswith("/") or re.match(r"^[A-Za-z]:", rel) or any(
            p in ("", ".", "..") for p in parts):
        raise OverlayError(rel or "<empty>", "not a clean relative path")
    return rel


def join(root, rel):
    return long_path(os.path.join(str(root), *rel.split("/")))


def read_bytes(path):
    with open(path, "rb") as f:
        return f.read()


def write_if_changed(path, data):
    """Write data unless the file already holds exactly it. Returns True when written."""
    try:
        if os.path.isfile(path) and read_bytes(path) == data:
            return False
    except OSError:
        pass
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".dov-tmp"
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, path)
    return True


def remove_and_prune(root, rel):
    """Delete root/rel if present, then any parent dirs it left empty (up to root)."""
    path = join(root, rel)
    if os.path.isfile(path):
        os.remove(path)
    parts = rel.split("/")[:-1]
    while parts:
        d = join(root, "/".join(parts))
        try:
            os.rmdir(d)  # only succeeds when empty
        except OSError:
            break
        parts.pop()


def walk_rel(root):
    """Every file under root as a sorted list of forward-slash relative paths."""
    base = long_path(root)
    out = []
    if not os.path.isdir(base):
        return out
    for dirpath, dirnames, filenames in os.walk(base):
        dirnames.sort()
        reld = os.path.relpath(dirpath, base)
        for name in filenames:
            rel = name if reld == "." else os.path.join(reld, name)
            out.append(rel.replace("\\", "/"))
    return sorted(out)


def is_within(child, parent):
    c = os.path.normcase(os.path.realpath(str(child)))
    p = os.path.normcase(os.path.realpath(str(parent)))
    return c == p or c.startswith(p.rstrip("\\/") + os.sep)


# ---------------------------------------------------------------------------
# line endings: canonical (git stored) form and the checkout's form

def eol_kind(data):
    """'crlf' when every LF is part of a CRLF, 'lf' when none is, 'mixed' otherwise, 'none' with no LF."""
    lf = data.count(b"\n")
    if not lf:
        return "none"
    crlf = data.count(b"\r\n")
    if crlf == lf:
        return "crlf"
    return "lf" if crlf == 0 else "mixed"


def canonical(data):
    """The form git stores: a uniformly CRLF file becomes LF, anything else is kept as is."""
    return data.replace(b"\r\n", b"\n") if eol_kind(data) == "crlf" else data


def materialize(data, crlf):
    """The checkout form: a uniformly LF result becomes CRLF on a CRLF target, anything else as is."""
    return data.replace(b"\n", b"\r\n") if crlf and eol_kind(data) == "lf" else data


def detect_tree_eol(decomp_root, rels=(), sample=64):
    """'crlf' or 'lf' for a decomp checkout: majority over rels, else over a sample of its sources."""
    votes = {"crlf": 0, "lf": 0}

    def vote(path):
        try:
            k = eol_kind(read_bytes(path))
        except OSError:
            return
        if k in votes:
            votes[k] += 1

    for rel in rels:
        vote(join(decomp_root, rel))
    if not votes["crlf"] and not votes["lf"]:
        seen = 0
        for sub in ("include", "src"):
            for dirpath, dirnames, filenames in os.walk(join(decomp_root, sub)):
                dirnames.sort()
                for name in sorted(filenames):
                    if seen < sample and name.endswith((".h", ".c", ".cpp", ".hpp", ".inc")):
                        vote(os.path.join(dirpath, name))
                        seen += 1
    return "crlf" if votes["crlf"] > votes["lf"] else "lf"


# ---------------------------------------------------------------------------
# line handling and unified diffs (bytes)

def split_lines(data):
    """Split on b'\\n' only, keeping the terminator. A last line without one is kept as is."""
    parts = data.split(b"\n")
    lines = [p + b"\n" for p in parts[:-1]]
    if parts[-1]:
        lines.append(parts[-1])
    return lines


def _range(start, count):
    # the unified diff convention git and diff -u use
    if count == 1:
        return b"%d" % (start + 1)
    if count == 0:
        return b"%d,0" % start
    return b"%d,%d" % (start + 1, count)


def _emit(prefix, line):
    if line.endswith(b"\n"):
        return prefix + line
    return prefix + line + b"\n" + NO_NEWLINE


def make_patch(rel, old, new):
    """A git-style unified diff of old -> new for rel, or b'' when they are identical."""
    if old == new:
        return b""
    a, b = split_lines(old), split_lines(new)
    relb = rel.encode("utf-8")
    out = [b"diff --git a/%s b/%s\n" % (relb, relb), b"--- a/%s\n" % relb, b"+++ b/%s\n" % relb]
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    for group in sm.get_grouped_opcodes(CONTEXT):
        i1, i2 = group[0][1], group[-1][2]
        j1, j2 = group[0][3], group[-1][4]
        out.append(b"@@ -%s +%s @@\n" % (_range(i1, i2 - i1), _range(j1, j2 - j1)))
        for tag, x1, x2, y1, y2 in group:
            if tag == "equal":
                out.extend(_emit(b" ", ln) for ln in a[x1:x2])
                continue
            if tag in ("replace", "delete"):
                out.extend(_emit(b"-", ln) for ln in a[x1:x2])
            if tag in ("replace", "insert"):
                out.extend(_emit(b"+", ln) for ln in b[y1:y2])
    return b"".join(out)


class Hunk:
    __slots__ = ("old_start", "old_count", "new_start", "new_count", "old", "new", "lead", "trail")

    def __init__(self, os_, oc, ns, nc):
        self.old_start, self.old_count, self.new_start, self.new_count = os_, oc, ns, nc
        self.old, self.new = [], []
        self.lead = self.trail = 0  # context lines before the first and after the last change


def parse_patch(rel, data):
    """Parse a single-file unified diff into hunks. Raises OverlayError when malformed."""
    lines = split_lines(data)
    i, n = 0, len(lines)
    source = target = None
    while i < n and not lines[i].startswith(b"@@"):
        ln = lines[i]
        if ln.startswith(b"--- "):
            source = ln[4:].rstrip(b"\r\n").split(b"\t")[0]
        elif ln.startswith(b"+++ "):
            target = ln[4:].rstrip(b"\r\n").split(b"\t")[0]
        elif ln.startswith(b"GIT binary patch") or ln.startswith(b"Binary files"):
            raise OverlayError(rel, "binary patches are not supported")
        i += 1
    if source == b"/dev/null":
        raise OverlayError(rel, "new-file patch (--- /dev/null) for a decomp path; a file the decomp lacks "
                                "belongs in decomp_overlay_files/")
    if source is None or target is None:
        raise OverlayError(rel, "patch has no ---/+++ header")
    if source != b"a/" + rel.encode("utf-8"):
        raise OverlayError(rel, "patch header names %r, expected a/%s" % (source.decode("utf-8", "replace"), rel))
    if target != b"b/" + rel.encode("utf-8"):
        raise OverlayError(rel, "patch header names %r, expected b/%s" % (target.decode("utf-8", "replace"), rel))
    hunks = []
    while i < n:
        m = HUNK_RE.match(lines[i])
        if not m:
            raise OverlayError(rel, "unexpected line %d in patch: %r" % (i + 1, lines[i][:60]))
        oc = 1 if m.group(2) is None else int(m.group(2))
        nc = 1 if m.group(4) is None else int(m.group(4))
        h = Hunk(int(m.group(1)), oc, int(m.group(3)), nc)
        i += 1
        o_left, n_left = oc, nc
        last = None  # which side lists the previous line belongs to
        changed = False
        while i < n and (o_left > 0 or n_left > 0 or lines[i].startswith(b"\\")):
            ln = lines[i]
            tag, body = ln[:1], ln[1:]
            if tag == b"\\":
                if last is None:
                    raise OverlayError(rel, "stray no-newline marker at patch line %d" % (i + 1))
                for side in last:
                    if not side[-1].endswith(b"\n"):
                        raise OverlayError(rel, "doubled no-newline marker at patch line %d" % (i + 1))
                    side[-1] = side[-1][:-1]
                last = None
            elif tag == b" ":
                h.old.append(body)
                h.new.append(body)
                o_left -= 1
                n_left -= 1
                last = (h.old, h.new)
                if changed:
                    h.trail += 1
                else:
                    h.lead += 1
            elif tag == b"-":
                h.old.append(body)
                o_left -= 1
                last = (h.old,)
                changed, h.trail = True, 0
            elif tag == b"+":
                h.new.append(body)
                n_left -= 1
                last = (h.new,)
                changed, h.trail = True, 0
            else:
                raise OverlayError(rel, "bad hunk line %d in patch: %r" % (i + 1, ln[:60]))
            if o_left < 0 or n_left < 0:
                raise OverlayError(rel, "hunk at patch line %d is longer than its header says" % (i + 1))
            i += 1
        if o_left or n_left:
            raise OverlayError(rel, "patch ends inside a hunk")
        hunks.append(h)
    if not hunks:
        raise OverlayError(rel, "patch has no hunks")
    return hunks


def _find_offsets(lines, block, near):
    """Positions where block occurs in lines, nearest to `near` first (for the error message)."""
    hits = []
    k = len(block)
    for s in range(0, len(lines) - k + 1):
        if lines[s:s + k] == block:
            hits.append(s)
    return sorted(hits, key=lambda s: abs(s - near))


def apply_patch(rel, data, patch):
    """Apply patch to data exactly (no offset, no fuzz) and return the new bytes."""
    lines = split_lines(data)
    out = []
    pos = 0
    for h in parse_patch(rel, patch):
        idx = h.old_start - 1 if h.old_count else h.old_start
        if idx < pos or idx + h.old_count > len(lines):
            raise OverlayError(rel, "hunk @@ -%d,%d does not fit the file (%d lines)"
                               % (h.old_start, h.old_count, len(lines)))
        # Short context verifies nothing about where the hunk sits: it is only allowed where the
        # hunk is pinned by the start or the end of the file (git's match_beginning/match_end, strict).
        if h.lead < CONTEXT and idx != 0:
            raise OverlayError(rel, "hunk @@ -%d,%d has %d line(s) of leading context away from the file "
                                    "start (%d needed), refused" % (h.old_start, h.old_count, h.lead, CONTEXT))
        if h.trail < CONTEXT and idx + h.old_count != len(lines):
            raise OverlayError(rel, "hunk @@ -%d,%d has %d line(s) of trailing context away from the file "
                                    "end (%d needed), refused" % (h.old_start, h.old_count, h.trail, CONTEXT))
        if lines[idx:idx + h.old_count] != h.old:
            msg = "hunk @@ -%d,%d does not match the decomp file exactly" % (h.old_start, h.old_count)
            hits = _find_offsets(lines, h.old, idx) if h.old else []
            if hits:
                msg += " (it would apply with offset %+d, refused)" % (hits[0] - idx)
            else:
                msg += " (its context is gone)"
            raise OverlayError(rel, msg)
        out.extend(lines[pos:idx])
        expect_new = len(out) + 1 if h.new_count else len(out)
        if h.new_start != expect_new:
            raise OverlayError(rel, "hunk @@ -%d +%d has an inconsistent new start (expected %d)"
                               % (h.old_start, h.new_start, expect_new))
        out.extend(h.new)
        pos = idx + h.old_count
    out.extend(lines[pos:])
    return b"".join(out)


# ---------------------------------------------------------------------------
# the layer

class LayerDirs:
    def __init__(self, port_dir):
        self.port = Path(port_dir)
        self.patches = self.port / PATCHES_NAME
        self.files = self.port / FILES_NAME

    def patch_rels(self):
        rels = []
        for r in walk_rel(self.patches):
            if r in PATCHES_RESERVED or not r.endswith(PATCH_SUFFIX):
                continue
            rels.append(check_rel(r[:-len(PATCH_SUFFIX)]))
        return rels

    def file_rels(self):
        return [check_rel(r) for r in walk_rel(self.files)
                if r not in FILES_RESERVED and not r.endswith(".dov-tmp")]

    def base(self):
        p = join(self.patches, "BASE")
        if os.path.isfile(p):
            return read_bytes(p).decode("utf-8", "replace").strip()
        return None


class CaseIndex:
    """Exact-case path lookups under a root, so a case-insensitive file system behaves like Linux."""

    def __init__(self, root):
        self.root = root
        self.dirs = {}

    def _entries(self, dir_rel):
        if dir_rel not in self.dirs:
            path = join(self.root, dir_rel) if dir_rel else long_path(self.root)
            try:
                self.dirs[dir_rel] = os.listdir(path)
            except OSError:
                self.dirs[dir_rel] = None
        return self.dirs[dir_rel]

    def lookup(self, rel):
        """('exact', rel), ('case', <the path as spelled on disk>) or ('missing', None)."""
        parts, actual, mismatch = rel.split("/"), [], False
        for part in parts:
            entries = self._entries("/".join(actual))
            if entries is None:
                return "missing", None
            if part in entries:
                actual.append(part)
                continue
            folded = [e for e in entries if e.casefold() == part.casefold()]
            if not folded:
                return "missing", None
            actual.append(folded[0])
            mismatch = True
        return ("case", "/".join(actual)) if mismatch else ("exact", rel)


def case_collisions(rels):
    """Failures for layer paths that differ only in letter case (one file on Windows, two on Linux)."""
    groups = {}
    for rel in rels:
        groups.setdefault(rel.casefold(), set()).add(rel)
    out = []
    for names in groups.values():
        if len(names) > 1:
            for rel in sorted(names):
                others = ", ".join(sorted(names - {rel}))
                out.append(OverlayError(rel, "differs only in letter case from %s in the layer" % others))
    return out


def build_overlay(layer, decomp_root, eol="auto"):
    """Compute every overlay output in memory. Returns (outputs dict rel->bytes, failures list)."""
    outputs, failures = {}, []
    patch_rels = layer.patch_rels()
    file_rels = layer.file_rels()
    tree_crlf = (eol if eol != "auto" else detect_tree_eol(decomp_root, patch_rels)) == "crlf"
    decomp_case = CaseIndex(decomp_root)
    both = set(patch_rels) & set(file_rels)
    for rel in sorted(both):
        failures.append(OverlayError(rel, "has both a patch and an overlay file"))
    collided = case_collisions(set(patch_rels) | set(file_rels))
    failures.extend(collided)
    both |= {e.rel for e in collided}
    for rel in patch_rels:
        if rel in both:
            continue
        src = join(decomp_root, rel)
        try:
            state, actual = decomp_case.lookup(rel)
            if state == "case":
                raise OverlayError(rel, "letter case differs from the decomp file %s" % actual)
            if state == "missing" or not os.path.isfile(src):
                raise OverlayError(rel, "patched file does not exist in the decomp root")
            target = read_bytes(src)
            kind = eol_kind(target)
            result = apply_patch(rel, canonical(target), read_bytes(join(layer.patches, rel + PATCH_SUFFIX)))
            # a CRLF target gives a CRLF result; a mixed or newline-free target follows the tree
            outputs[rel] = materialize(result, kind == "crlf" or (kind != "lf" and tree_crlf))
        except OverlayError as e:
            failures.append(e)
    for rel in file_rels:
        if rel in both:
            continue
        state, actual = decomp_case.lookup(rel)
        if state == "exact":
            failures.append(OverlayError(rel, "overlay file shadows a file that now exists in the decomp "
                                              "(re-cut it as a patch)"))
            continue
        if state == "case":
            failures.append(OverlayError(rel, "overlay file differs only in letter case from the decomp "
                                              "path %s" % actual))
            continue
        outputs[rel] = materialize(read_bytes(join(layer.files, rel)), tree_crlf)
    return outputs, failures


QUOTED_INCLUDE_RE = re.compile(rb'^[ \t]*#[ \t]*include[ \t]*"([^"\r\n]+)"', re.M)


def _quoted_includes(data):
    return [m.group(1).decode("utf-8", "replace").replace("\\", "/") for m in QUOTED_INCLUDE_RE.finditer(data)]


def posix_norm(p):
    return os.path.normpath(p).replace("\\", "/")


def _resolve_sibling(rel, target):
    """The decomp path `#include "target"` in decomp file `rel` names relative to rel's own directory."""
    d = rel.rsplit("/", 1)[0] if "/" in rel else ""
    joined = os.path.normpath(os.path.join(d, target)).replace("\\", "/")
    if joined == ".." or joined.startswith("../") or os.path.isabs(joined):
        return None
    return joined


def include_closure(outputs, decomp_root, scopes):
    """Pull verbatim decomp copies into the overlay so a quoted include never escapes it.

    The compiler resolves `#include "x.h"` in the including file's OWN directory before any -I
    path. Putting <overlay>/include ahead of the decomp's include/ therefore does not shadow a
    patched header for a raw header that includes it as a sibling: the raw includer finds the raw
    sibling first, and which copy a TU gets would depend on include order. So, to a fixpoint over
    the files under each scope dir (in practice `include`):

      - includer rule: a decomp file under a scope whose quoted include could name a file in the
        overlay is copied into the overlay too. MSVC tries the includer's own directory and then the
        directories of every file that included it (in reverse order of opening) before any -I path,
        and any directory of the scope can be on that chain, so a name counts when it resolves as a
        sibling, or against any parent directory up to the scope root, or (a name without "..") when
        some overlay file under the scope is <any dir>/<name>. Over-pulling only copies unmodified
        files; under-pulling lets a raw header reach a raw copy of a patched one;
      - includee rule: an overlay file in a SUBdirectory of a scope whose quoted include names a raw
        sibling the overlay lacks gets that sibling too, because the -I path cannot find it from
        there (include/math/X.h including "Matrix.h" means include/math/Matrix.h).

    Pulled files are byte-for-byte decomp copies and are listed in the manifest like every other
    output. Returns the list of pulled paths. With an empty overlay nothing is pulled."""
    if not outputs or not scopes:
        return []
    raw = {}
    for scope in scopes:
        scope = scope.strip("/")
        for r in walk_rel(join(decomp_root, scope)):
            raw[scope + "/" + r] = None
    scope_names = [sc.strip("/") for sc in scopes]

    def scope_of(rel):
        return next((sc for sc in scope_names if rel.startswith(sc + "/")), None)

    edges = {}

    def includes_of(rel):
        """(exact candidate paths, bare names) for rel's quoted includes, case-folded."""
        if rel not in edges:
            data = read_bytes(join(decomp_root, rel))
            scope = scope_of(rel)
            paths, names = set(), set()
            for inc in _quoted_includes(data):
                t = _resolve_sibling(rel, inc)
                if t:
                    paths.add(t.casefold())
                # the parent directories up to the scope root: the includers' folders
                d = rel.rsplit("/", 1)[0]
                while scope and "/" in d and d != scope:
                    d = d.rsplit("/", 1)[0]
                    t = _resolve_sibling(d + "/x", inc)
                    if t:
                        paths.add(t.casefold())
                if ".." not in inc.split("/"):
                    names.add(posix_norm(inc).casefold())
            edges[rel] = (paths, names)
        return edges[rel]

    # Windows resolves an include name without regard to letter case, so the match does too
    raw_fold = {r.casefold(): r for r in raw}
    out_fold = set()
    out_tails = set()  # every "<dir>/<name>" tail of an overlay file under a scope, case-folded

    def note(rel):
        out_fold.add(rel.casefold())
        scope = scope_of(rel)
        if scope:
            parts = rel[len(scope) + 1:].casefold().split("/")
            for k in range(len(parts)):
                out_tails.add("/".join(parts[k:]))

    for rel in outputs:
        note(rel)
    pulled = []

    def pull(rel):
        outputs[rel] = read_bytes(join(decomp_root, rel))
        note(rel)
        pulled.append(rel)

    changed = True
    while changed:
        changed = False
        for rel in sorted(raw):
            if rel.casefold() in out_fold:
                continue
            paths, names = includes_of(rel)
            if paths & out_fold or names & out_tails:
                pull(rel)
                changed = True
        for rel in sorted(outputs):
            scope = next((s.strip("/") for s in scopes if rel.startswith(s.strip("/") + "/")), None)
            if scope is None or rel.count("/") <= scope.count("/") + 1:
                continue  # not under a scope, or directly in it: the -I path finds its siblings
            for t in [_resolve_sibling(rel, i) for i in _quoted_includes(outputs[rel])]:
                hit = raw_fold.get(t.casefold()) if t else None
                if hit and hit.casefold() not in out_fold:
                    pull(hit)
                    changed = True
    return sorted(pulled)


def report_failures(failures):
    for e in failures:
        print("decomp_overlay: FAIL %s" % e, file=sys.stderr)
    print("decomp_overlay: %d file(s) failed to apply exactly" % len(failures), file=sys.stderr)


def read_manifest(out_dir):
    p = join(out_dir, MANIFEST_NAME)
    if not os.path.isfile(p):
        return []
    rels = []
    for ln in read_bytes(p).decode("utf-8").splitlines():
        ln = ln.strip()
        if ln:
            try:
                rels.append(check_rel(ln))
            except OverlayError:
                pass  # never act on a path we could not have written
    return rels


def cmd_check(layer, decomp_root, eol):
    outputs, failures = build_overlay(layer, decomp_root, eol)
    if failures:
        report_failures(failures)
        return 1
    print("decomp_overlay: check OK, %d file(s) apply exactly (base %s)" % (len(outputs), layer.base() or "?"))
    return 0


def out_dir_unsafe(out_dir, decomp_root, rels):
    """True when writing <out_dir>/<rel> could land on a decomp file: the out dir is the decomp root
    (or holds it), or it sits inside one of the decomp dirs the outputs mirror (src/, include/, the
    first component of any output). A build dir elsewhere under the checkout (build/port/...) is
    fine: that is where the port's build puts the overlay."""
    if is_within(decomp_root, out_dir):
        return True
    tops = {"src", "include"} | {r.split("/", 1)[0] for r in rels if "/" in r}
    return any(is_within(out_dir, os.path.join(str(decomp_root), t)) for t in sorted(tops))


def cmd_apply(layer, decomp_root, out_dir, eol, closure_scopes=()):
    outputs, failures = build_overlay(layer, decomp_root, eol)
    if out_dir_unsafe(out_dir, decomp_root, outputs):
        print("decomp_overlay: refusing to write where an output could replace a decomp file: %s"
              % out_dir, file=sys.stderr)
        return 1
    if failures:
        report_failures(failures)
        return 1
    pulled = include_closure(outputs, decomp_root, closure_scopes)
    if pulled:
        print("decomp_overlay: include closure pulled %d unmodified decomp file(s) into the overlay"
              % len(pulled))
    previous = read_manifest(out_dir)
    # 1. a manifest covering old and new outputs first: if apply dies part way, the next run
    #    still knows every file it may have written
    union = sorted(set(previous) | set(outputs))
    write_if_changed(join(out_dir, MANIFEST_NAME), "".join(r + "\n" for r in union).encode("utf-8"))
    # 2. stale files before new ones, so a case-only rename (Foo.h -> foo.h, one file on Windows)
    #    deletes the old name and then writes the new one, never the other way round
    stale = [r for r in previous if r not in outputs]
    for rel in stale:
        remove_and_prune(out_dir, rel)
    # 3. the outputs; a file left under a differently cased name is replaced so the name is exact
    out_case = CaseIndex(out_dir)
    written = 0
    for rel in sorted(outputs):
        path = join(out_dir, rel)
        if out_case.lookup(rel)[0] == "case" and os.path.isfile(path):
            os.remove(path)
        if write_if_changed(path, outputs[rel]):
            written += 1
    # 4. the final manifest
    manifest = "".join(r + "\n" for r in sorted(outputs)).encode("utf-8")
    write_if_changed(join(out_dir, MANIFEST_NAME), manifest)
    print("decomp_overlay: applied %d file(s) (%d rewritten, %d stale removed), base %s"
          % (len(outputs), written, len(stale), layer.base() or "?"))
    return 0


def read_listing(path):
    rels = []
    for ln in read_bytes(long_path(path)).decode("utf-8").splitlines():
        ln = ln.strip()
        if ln and not ln.startswith("#"):
            rels.append(check_rel(ln))
    return rels


def cmd_cut(layer, decomp_root, edited_root, listing, base):
    try:
        rels = read_listing(listing)
    except OverlayError as e:
        report_failures([e])
        return 1
    failures = []
    counts = {"patch": 0, "file": 0, "removed": 0}
    for d in (layer.patches, layer.files):
        write_if_changed(join(d, ".gitattributes"), GITATTRIBUTES)
    decomp_case, edited_case = CaseIndex(decomp_root), CaseIndex(edited_root)
    for rel in rels:
        try:
            if rel in PATCHES_RESERVED or rel in FILES_RESERVED:
                raise OverlayError(rel, "path collides with a layer file name")
            dpath, epath = join(decomp_root, rel), join(edited_root, rel)
            e_state, e_actual = edited_case.lookup(rel)
            if e_state == "case":
                raise OverlayError(rel, "letter case differs from the edited file %s" % e_actual)
            if e_state == "missing" or not os.path.isfile(epath):
                raise OverlayError(rel, "not in the edited root (deleting a decomp file is not supported)")
            d_state, d_actual = decomp_case.lookup(rel)
            if d_state == "case":
                raise OverlayError(rel, "letter case differs from the decomp file %s" % d_actual)
            new = canonical(read_bytes(epath))
            if d_state == "exact" and os.path.isfile(dpath):
                remove_and_prune(layer.files, rel)
                old = canonical(read_bytes(dpath))
                patch = make_patch(rel, old, new)
                if patch:
                    # prove it before keeping it
                    if apply_patch(rel, old, patch) != new:
                        raise OverlayError(rel, "internal error: cut patch does not round-trip")
                    write_if_changed(join(layer.patches, rel + PATCH_SUFFIX), patch)
                    counts["patch"] += 1
                else:
                    remove_and_prune(layer.patches, rel + PATCH_SUFFIX)
                    counts["removed"] += 1
            else:
                remove_and_prune(layer.patches, rel + PATCH_SUFFIX)
                write_if_changed(join(layer.files, rel), new)  # canonical form
                counts["file"] += 1
        except OverlayError as e:
            failures.append(e)
    if base:
        write_if_changed(join(layer.patches, "BASE"), (base.strip() + "\n").encode("utf-8"))
    print("decomp_overlay: cut %d patch(es), %d overlay file(s), %d identical (patch removed)"
          % (counts["patch"], counts["file"], counts["removed"]))
    if failures:
        report_failures(failures)
        return 1
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--port-dir", default=str(PORT_DIR),
                        help="the port dir holding decomp_patches/ and decomp_overlay_files/ "
                             "(default: this script's port/)")
    common.add_argument("--decomp-root", required=True, help="the decomp checkout (read only)")
    common.add_argument("--eol", choices=("auto", "lf", "crlf"), default="auto",
                        help="line endings of the decomp checkout, used for overlay files and for decomp "
                             "files that are neither uniformly LF nor CRLF (default: detect from the "
                             "patched files, else from a sample of include/ and src/)")
    p = sub.add_parser("apply", parents=[common], help="write the overlay")
    p.add_argument("--out", required=True, help="overlay output dir")
    p.add_argument("--include-closure", action="append", default=[], metavar="DIR",
                   help="decomp dir that is an -I root (repeatable, e.g. include): also copy every "
                        "unmodified file there that reaches an overlay file through a quoted include, "
                        "so a raw sibling can never shadow a patched header (see include_closure)")
    sub.add_parser("check", parents=[common], help="dry run of apply")
    p = sub.add_parser("cut", parents=[common], help="regenerate patches from an edited tree")
    p.add_argument("--edited-root", required=True)
    p.add_argument("--paths", required=True, help="file listing decomp relative paths, one per line")
    p.add_argument("--base", help="record this decomp commit in decomp_patches/BASE")
    args = ap.parse_args(argv)

    layer = LayerDirs(args.port_dir)
    if not os.path.isdir(long_path(args.decomp_root)):
        print("decomp_overlay: decomp root is not a directory: %s" % args.decomp_root, file=sys.stderr)
        return 1
    if args.cmd == "apply":
        return cmd_apply(layer, args.decomp_root, args.out, args.eol, args.include_closure)
    if args.cmd == "check":
        return cmd_check(layer, args.decomp_root, args.eol)
    return cmd_cut(layer, args.decomp_root, args.edited_root, args.paths, args.base)


if __name__ == "__main__":
    sys.exit(main())
