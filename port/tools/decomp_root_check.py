"""Check that the port reaches the decomp only through its configured decomp root.

    python port/tools/decomp_root_check.py [--port-dir <port dir>] [--list]

The port used to find the decomp as "the directory above port/". It now takes it from one place:
CMake's DECOMP_ROOT cache variable, SM64DS_DECOMP_ROOT in the .cmd scripts, and
port/tools/decomp_root.py in Python (see that file). This check fails on any reach that goes around
it. Exit 0 with zero hits, 1 otherwise (each hit printed as path:line: rule: text).

Rules, by file kind (comment lines are skipped in every kind):

  cmake   port/CMakeLists.txt, port/*.cmake, port/cmake/*.cmake
          - ${CMAKE_CURRENT_SOURCE_DIR}/.., ${CMAKE_SOURCE_DIR}/.., ${PROJECT_SOURCE_DIR}/..,
            ${CMAKE_CURRENT_LIST_DIR}/../.. (a walk above port/)
          - a literal ../src, ../include, ../config, ../tools, ../extracted, ../symbols
  cmd     port/*.cmd, port/**/*.cmd, *.bat, *.ps1, *.sh under port/
          - %~dp0.. or %~dp0\\.. (the parent of port/) other than %~dp0..\build (the port's own
            build output), %CD% (the caller's directory), and literal ../ walks that climb out of
            port/ (either slash)
  python  port/**/*.py
          - walks from __file__ to the tree above port/: parents[2] and deeper on a __file__ path,
            "..", ".." path joins, dirname(dirname(dirname(...))), .parent.parent.parent, and one
            step above a name holding port/ (dirname(PORT_DIR), PORT_DIR.parent, join(PORT, ".."),
            HERE.parent.parent or HERE.parents[1] where HERE is port/tools)
          - a name bound to decomp_root.PORT_TREE joined with a decomp directory (src, include,
            config, extracted, symbols, tools/<a decomp script>): PORT_TREE is the port's own tree,
            the decomp is decomp_root.find()
          - a root that may be the decomp root (root, ROOT, REPO, tree, self.root, a.root, ...)
            joined by hand: os.path.join(root, ...), os.path.relpath(x, root), Path(root), or
            root = Path(...). Those go through decomp_root.join / relpath / path, which take
            port/, build/ and runs/ from the port's tree when the root is the decomp

          - a root that defaults to the current directory (default=".", `... else "."`,
            os.getcwd()): run from the port's tree, "." is not an external decomp. Root arguments
            go through decomp_root.root_arg(), which reads a missing value or "." as find()
            (also os.curdir, `root or "."` on a root-named value, and a root-named variable or
            parameter defaulting to "." or "..")
          - decomp content opened relative to the current directory (open("src/..."))
          - a module-level use of decomp_root (or a name from `from decomp_root import`) above the
            line that imports it, and decomp_root.<attr> in a file that never binds decomp_root
            (NameError on run)
          Not checked, because they are too noisy to tell from output and scratch folders: a bare
          os.getcwd() or Path.cwd() or Path("."), and relative reads through join, glob or exists.
          A root bound by `for root, dirs, files in os.walk(...)` is a walked directory, not a
          decomp root, and may be joined with os.path.join.

          A tool that takes a decomp tree as an explicit, named argument (two checkouts compared
          side by side, a test's temp fixture tree) is not reaching for the configured decomp. That
          line may end with the comment `# decomp-root-check: explicit: <reason>`, which exempts it
          from the four root-name rules above (join, relpath, Path(root), root = Path(...)) and
          from nothing else. The reason is required; a marker without one, or on a line none of
          those four rules hits, is itself a hit.

The definitions themselves are allowed: one line per file kind that sets the default (CMake's
DECOMP_ROOT and PORT_TREE_DIR, the .cmd scripts' DECOMP_ROOT and PORT_TREE), each marked with the
comment `decomp-root-check: definition`. A marked line anywhere else is itself a hit, so the marker
cannot be used to wave a reach through. port/tools/decomp_root.py is the Python definition and is not
scanned. port/third_party/ is vendored code and is not scanned.
"""
import argparse
import os
import re
import sys

MARK = "decomp-root-check: definition"
# A line carrying MARK must set the default of one of these names (the decomp root or the port's
# own tree) and nothing else; a marked line that does not is a hit of its own.
DEFINES = re.compile(r"""(?:^|[\s("$])(DECOMP_ROOT|PORT_TREE|PORT_TREE_DIR)\b\s*(?:=|"\$\{CMAKE_CURRENT_SOURCE_DIR\}/\.\.")""")

DECOMP_DIRS = ("src", "include", "config", "extracted", "symbols")

# A per-line exemption for a root that is an explicit, named argument (two checkouts compared side by
# side, a test's temp fixture) rather than the configured decomp. It covers EXEMPT_RULES only and
# needs a reason after the second colon.
EXEMPT = "decomp-root-check: explicit"
EXEMPT_RE = re.compile(r"#\s*decomp-root-check: explicit:[ \t]*(\S.*)?$")
EXEMPT_RULES = ("py-root-join", "py-root-relpath", "py-root-path", "py-root-path-assign")


def exempt_reason(line):
    """The reason given by a line's explicit-root exemption, or None."""
    m = EXEMPT_RE.search(line)
    return m.group(1).strip() if m and m.group(1) else None

CMAKE_RULES = (
    ("cmake-parent", re.compile(r"\$\{(CMAKE_CURRENT_SOURCE_DIR|CMAKE_SOURCE_DIR|PROJECT_SOURCE_DIR)\}/\.\.")),
    ("cmake-listdir-up2", re.compile(r"\$\{CMAKE_CURRENT_LIST_DIR\}/\.\./\.\.")),
    ("cmake-literal", re.compile(r"(?<![\w.])\.\./(src|include|config|tools|extracted|symbols)\b")),
)
CMD_RULES = (
    # %~dp0.. is the directory holding port/. Its build\ is the port's own build output (allowed);
    # anything else under it is the decomp and has to come from DECOMP_ROOT.
    # (%~dp0\.. is the same walk spelled with a separator)
    ("cmd-parent", re.compile(r"%~dp0\\?\.\.(?![\\/]build\b)")),
    # the current directory is wherever the caller stood, not the decomp
    ("cmd-cwd", re.compile(r"%CD%", re.I)),
)
TOOLS_NAMES = r"(?:HERE|here|TOOLS|TOOLS_DIR|tools_dir|_HERE|THIS_DIR|this_dir)"
PORT_NAMES = r"(?:PORT|PORT_DIR|PORTDIR|port_dir|PORT_ROOT|port_root\(\))"
# names the tools give a root that may be the decomp root
ROOT_NAMES = r"(?:ROOT|root|REPO|repo|tree|TREE|self\.root|a\.root|args\.root|aroot|src_root|CS\.REPO)"
# Python rules run over the whole file (a walk is often split over two lines); \s spans newlines.
PY_RULES = (
    ("py-parents", re.compile(r"parents\[([2-9])\]")),
    ("py-dotdot-join", re.compile(r"""["']\.\.["']\s*,\s*["']\.\.["']""")),
    ("py-pardir", re.compile(r"os\.pardir\s*,\s*os\.pardir")),
    ("py-dirname3", re.compile(r"dirname\(\s*(os\.path\.)?dirname\(\s*(os\.path\.)?dirname\(\s*"
                               r"(os\.path\.)?(abspath|realpath)\(\s*__file__")),
    ("py-parent3", re.compile(r"\.parent\.parent\.parent")),
    # one step above a name that holds port/, or two above one that holds port/tools
    ("py-above-port", re.compile(r"dirname\(\s*%s\s*\)" % PORT_NAMES
                                 + r"|\b%s\.parent\b" % PORT_NAMES
                                 + r"""|join\(\s*%s\s*,\s*["']\.\.["']""" % PORT_NAMES
                                 + r"|dirname\(\s*(os\.path\.)?dirname\(\s*%s\s*\)" % TOOLS_NAMES
                                 + r"|\b%s\.parent\.parent\b" % TOOLS_NAMES
                                 + r"|\b%s\.parents\[1\]" % TOOLS_NAMES)),
    # A root that may be the decomp root, joined or made relative by hand: when it is the decomp,
    # port/, build/ and runs/ are not under it. decomp_root.join / relpath / path take those from
    # the port tree (and are plain os.path / pathlib for any other root).
    ("py-root-join", re.compile(r"(?<![\w.])os\.path\.join\(\s*%s\s*," % ROOT_NAMES)),
    ("py-root-relpath", re.compile(r"(?<![\w.])os\.path\.relpath\((?:[^()\n]|\([^()\n]*\))*,\s*%s\s*\)"
                                   % ROOT_NAMES)),
    ("py-root-path", re.compile(r"(?<![\w.])(?:pathlib\.)?Path\(\s*(?:%s|decomp_root\.find\(\))\s*\)"
                                % ROOT_NAMES)),
    ("py-root-path-assign", re.compile(r"^[ \t]*%s[ \t]*=[ \t]*(?:pathlib\.)?Path\(" % ROOT_NAMES, re.M)),
    # A root that defaults to the current directory: default=".", `... else "."`, os.getcwd(). Run
    # from the port's tree with an external decomp, "." is the port's tree, not the decomp. A root
    # argument defaults through decomp_root.root_arg(), which reads None and "." as find().
    ("py-cwd-root", re.compile(r"(?:(?<![\w.])default\s*=\s*|(?<![\w.])else\s+)(?:os\.getcwd\(\)|"
                               r"os\.curdir\b|[\"']\.[\"'])")),
    # the same default spelled `root or "."` (a root-named value, or a line assigning a root-named
    # name; `os.path.dirname(out) or "."` for an output file is not a root and is not flagged)
    ("py-or-cwd", re.compile(r"(?:^[ \t]*[\w.]*(?:root|ROOT|repo|REPO|tree|TREE)\w*[ \t]*=[^\n#]*"
                             r"|[\w.]*(?:root|ROOT|repo|REPO|tree|TREE)\w*(?:\(\))?\s+)"
                             r"(?<![\w.])or\s+(?:os\.getcwd\(\)|os\.curdir\b|[\"']\.[\"'])", re.M)),
    # a root-named variable or parameter that defaults to the current directory or its parent:
    # root=".", repo_root="..", tree = os.curdir
    ("py-root-dot-default", re.compile(r"(?<![\w.])\w*(?:root|ROOT|repo|REPO|tree|TREE)\w*\s*=\s*"
                                       r"(?:[\"']\.{1,2}[\"']|os\.curdir\b)")),
    # decomp content opened relative to the current directory
    ("py-cwd-open", re.compile(r"(?<![\w.])open\(\s*[rbuf]?[\"'](?:\./)?"
                               r"(?:%s)/" % "|".join(DECOMP_DIRS))),
)
WALK_BIND = re.compile(r"\bfor\s+(\w+)\s*,\s*\w+\s*,\s*\w+\s+in\s+os\.walk\(")
TREE_BIND = re.compile(r"^\s*([A-Za-z_][A-Za-z_0-9]*)\s*=\s*(?:pathlib\.Path\(|Path\(|str\()?\s*"
                       r"decomp_root\.PORT_TREE\)?\s*(?:#.*)?$")
CMAKE_EXT = (".cmake",)
CMD_EXT = (".cmd", ".bat", ".ps1", ".sh")


UPS = re.compile(r"(?:\.\.[\\/])*\.\.(?![\w.])|(?:\.\.[\\/])+")


def climbs_above_port(rel, code):
    """A literal ../ chain in a script under port/ that climbs out of port/ (more steps than the
    script sits below port/). Script-relative walks inside port/ are fine."""
    depth = rel.count("/")
    code = code.replace("%~dp0\\..", "").replace("%~dp0..", "")  # the cmd-parent rule judges these
    for m in UPS.finditer(code):
        if m.group(0).count("..") > depth:
            return True
    return False


def is_comment(kind, line):
    s = line.strip()
    if not s:
        return True
    if kind in ("cmake", "python"):
        return s.startswith("#")
    low = s.lower()
    return low.startswith("rem ") or low == "rem" or low.startswith("::") or s.startswith("#")


def tree_join_rule(names):
    alt = "|".join(re.escape(n) for n in sorted(names))
    dirs = "|".join(DECOMP_DIRS)
    return re.compile(r"(?:join\(\s*(?:%s)\s*,\s*[\"'](?:%s|tools)[\"'/\\]|(?:%s)\s*/\s*[\"'](?:%s|tools)[\"'/\\])"
                      % (alt, dirs, alt, dirs))


def scan_file(path, rel, kind):
    hits = []
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError as e:
        return [(rel, 0, "unreadable", str(e))]
    lines = text.splitlines()
    rules = {"cmake": CMAKE_RULES, "cmd": CMD_RULES, "python": PY_RULES}[kind]
    tree_names = set()
    if kind == "python":
        for line in lines:
            m = TREE_BIND.match(line)
            if m:
                tree_names.add(m.group(1))
        tree_names.add("decomp_root.PORT_TREE")
    tj = tree_join_rule(tree_names) if tree_names else None
    starts = [0]
    for line in lines:
        starts.append(starts[-1] + len(line) + 1)

    def line_of(pos):
        lo, hi = 0, len(lines)
        while lo < hi:
            mid = (lo + hi) // 2
            if starts[mid + 1] <= pos:
                lo = mid + 1
            else:
                hi = mid
        return lo + 1

    seen = set()
    exempted = set()
    if kind == "python":
        flat = "\n".join(lines)
        # `for root, dirs, files in os.walk(...)`: that root is a directory being walked, not a
        # decomp root, and joining it with a file name is plain os.path.join
        walked = set(WALK_BIND.findall(flat))
        for name, rx in rules:
            for m in rx.finditer(flat):
                no = line_of(m.start())
                line = lines[no - 1]
                if no in seen or is_comment(kind, line) or "#" in line[:m.start() - starts[no - 1]]:
                    continue
                if name == "py-root-join" and m.group(0).split("(", 1)[1].strip(" ,\n") in walked:
                    continue
                if name in EXEMPT_RULES and exempt_reason(line):
                    exempted.add(no)
                    continue
                seen.add(no)
                hits.append((rel, no, name, line.strip()))
    for no, line in enumerate(lines, 1):
        if no in seen or is_comment(kind, line):
            continue
        code = line
        if kind in ("cmake", "python") and " #" in line:
            # drop a trailing comment (good enough: these files keep '#' out of strings on reach lines)
            code = line.split(" #", 1)[0]
        if kind == "python" and EXEMPT in line:
            # the exemption must give a reason and must sit on a line a root-name rule hits
            if not exempt_reason(line):
                hits.append((rel, no, "exempt-no-reason", line.strip()))
                continue
            if no not in exempted:
                hits.append((rel, no, "stray-exempt", line.strip()))
                continue
        if MARK in line:
            if not DEFINES.search(line):
                hits.append((rel, no, "stray-marker", line.strip()))
            continue
        if kind == "cmd" and climbs_above_port(rel, code):
            hits.append((rel, no, "cmd-up-out-of-port", line.strip()))
            continue
        if kind != "python":
            for name, rx in rules:
                if rx.search(code):
                    hits.append((rel, no, name, line.strip()))
                    break
            continue
        if tj is not None and tj.search(code):
            hits.append((rel, no, "py-tree-is-not-decomp", line.strip()))
    if kind == "python":
        hits.extend(used_before_import(rel, text, lines))
    return hits


def used_before_import(rel, text, lines):
    """A module-level use of decomp_root (or its import alias) above the top-level line that imports
    it: the tool dies with NameError the moment it is run or imported."""
    import ast
    try:
        tree = ast.parse(text)
    except SyntaxError:
        return []
    names = {}
    for node in tree.body:
        if isinstance(node, ast.Import):
            for a in node.names:
                if a.name == "decomp_root":
                    names.setdefault(a.asname or "decomp_root", node.lineno)
        elif isinstance(node, ast.ImportFrom) and node.module == "decomp_root":
            # from decomp_root import find, join as rjoin: each bound name counts
            for a in node.names:
                if a.name != "*":
                    names.setdefault(a.asname or a.name, node.lineno)

    out = []
    # decomp_root.<attr> read in a file that never binds the name (no import of any kind, no
    # parameter, no assignment): NameError the first time that line runs
    bound = set()
    for node in ast.walk(tree):
        if isinstance(node, (ast.Import, ast.ImportFrom)):
            bound.update((a.asname or a.name).split(".")[0] for a in node.names)
        elif isinstance(node, ast.Name) and not isinstance(node.ctx, ast.Load):
            bound.add(node.id)
        elif isinstance(node, ast.arg):
            bound.add(node.arg)
    if "decomp_root" not in bound:
        used = sorted(n.lineno for n in ast.walk(tree) if isinstance(n, ast.Attribute)
                      and isinstance(n.value, ast.Name) and n.value.id == "decomp_root")
        if used:
            out.append((rel, used[0], "py-root-no-import", lines[used[0] - 1].strip()))

    def loads(node):
        # names read when the module runs: function and lambda bodies run later
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.Lambda)):
            return
        if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load):
            yield node
        for child in ast.iter_child_nodes(node):
            yield from loads(child)

    for name, at in names.items():
        early = [n.lineno for node in tree.body for n in loads(node) if n.id == name and n.lineno < at]
        if early:
            out.append((rel, min(early), "py-root-before-import", lines[min(early) - 1].strip()))
    return out


def walk(port_dir):
    for dp, dn, fn in os.walk(port_dir):
        rel_dp = os.path.relpath(dp, port_dir).replace("\\", "/")
        dn[:] = sorted(d for d in dn if d not in ("third_party", "__pycache__", ".git")
                       and not (rel_dp == "." and d == "build"))
        for f in sorted(fn):
            rel = os.path.normpath(os.path.join(rel_dp, f)).replace("\\", "/")
            if f == "CMakeLists.txt" or f.endswith(CMAKE_EXT):
                kind = "cmake"
            elif f.endswith(CMD_EXT):
                kind = "cmd"
            elif f.endswith(".py"):
                if rel == "tools/decomp_root.py" or rel == "tools/decomp_root_check.py":
                    continue
                kind = "python"
            else:
                continue
            yield os.path.join(dp, f), rel, kind


def run(port_dir):
    hits = []
    n = 0
    for path, rel, kind in walk(port_dir):
        n += 1
        hits.extend(scan_file(path, rel, kind))
    return n, hits


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--port-dir", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    ap.add_argument("--list", action="store_true", help="print every hit (default: first 200)")
    a = ap.parse_args(argv)
    n, hits = run(a.port_dir)
    shown = hits if a.list else hits[:200]
    for rel, no, rule, text in shown:
        print("port/%s:%d: %s: %s" % (rel, no, rule, text[:160]))
    if len(shown) < len(hits):
        print("... %d more (--list prints all)" % (len(hits) - len(shown)))
    by_rule = {}
    for h in hits:
        by_rule[h[2]] = by_rule.get(h[2], 0) + 1
    print("decomp_root_check: %d file(s) scanned, %d hit(s)%s" % (
        n, len(hits), (" " + ", ".join("%s %d" % kv for kv in sorted(by_rule.items()))) if hits else ""))
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main())
