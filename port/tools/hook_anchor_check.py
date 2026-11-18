#!/usr/bin/env python3
"""Hook placement check for a decomp bump (catch-up plan Task 4.4).

The decomp patch layer refuses offsets and fuzz, but a hook that is re-cut by hand onto a reshuffled
decomp file can still sit in the wrong function and apply exactly. This tool compares, for every HOOK
patch, where each change sits BEFORE the bump (old patch on the old decomp) and AFTER it (re-cut patch
on the new decomp).

For every patch:
  1. the patch is applied (exactly, through decomp_overlay.apply_patch) to its decomp file;
  2. the patched file is scanned (C and C++: namespaces, extern "C", classes with inline methods,
     templates, initializers, preprocessor branches) and every added or removed line gets an anchor:
       function  the innermost function (or data initializer) that existed before the patch, i.e. not
                 made only of added lines; host functions a hook adds are skipped outward;
       filescope a change outside any function: the enclosing class or namespace plus the nearest
                 preceding declaration (function, declaration, #include, #define) that the patch did
                 not add. The report says "filescope" for these. When main deleted that declaration
                 or put a new one in between, the nearest declaration present on BOTH sides is
                 compared instead, and the note says so.
     Blank, comment-only, bare-brace and #if/#else/#endif lines are ignored when placing a change,
     because a diff slides them (a host function added after Foo is often cut as '+}' first).
  3. each anchor is identified as (module, addr) through config/**/symbols.txt of that side (mangled
     names are demangled to their qualified name); an anchor with no symbol (inline functions in
     headers, statics the ROM never named, declarations) is identified by its qualified name, which the
     rename map (renames.tsv / renames_kind.tsv, keyed by (module, addr)) translates from the old
     spelling to the new one.

Changes are paired BEFORE to AFTER by change group (a run of consecutive -/+ lines), not by hunk, because
a re-cut can merge or split hunks when context moves. Pairs are found by identical change text, then by
similarity. The before path maps to after paths through patchmap.tsv (moved, renamed, absorbed files).

Result per before hunk (the worst of its groups):
  SAME          every group has the same anchor before and after;
  MOVED_LISTED  a group moved (or has no counterpart) and the hunk is in the --allow file;
  MOVED         a group sits in a different function or after a different declaration;
  UNKNOWN       no anchor, no counterpart change, a patch that does not apply, a missing file.
Changes that appear only AFTER (in a file a HOOK patch maps to) are reported as UNKNOWN rows too.
Exit 1 when any MOVED or UNKNOWN remains.

Allow file: one line per intentional move, "<before patch rel path><TAB><hunk number or *><TAB><reason>";
blank lines and "#" comments ignored. Hunk numbers are 1-based in the BEFORE patch; after-only rows use
"after:<n>" with n the 1-based hunk number in the after patch, and the after path.

Usage:
  python port/tools/hook_anchor_check.py --before-root <decomp dir> --after-root <decomp dir> \
      --before-patches <dir> --after-patches <dir> --maps <dir> [--allow <file>] --report <file> \
      [--edits port/decomp_edits.tsv] [--kinds HOOK]
"""

import argparse
import difflib
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import decomp_overlay as dov  # noqa: E402
import decomp_root  # noqa: E402

DEFAULT_EDITS = HERE.parent / "decomp_edits.tsv"
SEVERITY = {"SAME": 0, "MOVED_LISTED": 1, "MOVED": 2, "UNKNOWN": 3}


# ---------------------------------------------------------------------------
# Itanium demangling, only as far as the qualified name (no parameter types)

OPERATORS = {
    "nw": "operator new", "na": "operator new[]", "dl": "operator delete", "da": "operator delete[]",
    "ps": "operator+", "ng": "operator-", "ad": "operator&", "de": "operator*", "co": "operator~",
    "pl": "operator+", "mi": "operator-", "ml": "operator*", "dv": "operator/", "rm": "operator%",
    "an": "operator&", "or": "operator|", "eo": "operator^", "aS": "operator=", "pL": "operator+=",
    "mI": "operator-=", "mL": "operator*=", "dV": "operator/=", "rM": "operator%=", "aN": "operator&=",
    "oR": "operator|=", "eO": "operator^=", "ls": "operator<<", "rs": "operator>>", "lS": "operator<<=",
    "rS": "operator>>=", "eq": "operator==", "ne": "operator!=", "lt": "operator<", "gt": "operator>",
    "le": "operator<=", "ge": "operator>=", "nt": "operator!", "aa": "operator&&", "oo": "operator||",
    "pp": "operator++", "mm": "operator--", "cm": "operator,", "pm": "operator->*", "pt": "operator->",
    "cl": "operator()", "ix": "operator[]",
}
BUILTIN_TYPES = set("vwbcahstijlmxynofdegz")


class _Demangle(Exception):
    pass


class _Dm:
    def __init__(self, s):
        self.s, self.i = s, 0

    def peek(self, k=1):
        return self.s[self.i:self.i + k]

    def take(self, k=1):
        if self.i + k > len(self.s):
            raise _Demangle()
        t = self.s[self.i:self.i + k]
        self.i += k
        return t

    def number(self):
        j = self.i
        while self.i < len(self.s) and self.s[self.i].isdigit():
            self.i += 1
        if j == self.i:
            raise _Demangle()
        return int(self.s[j:self.i])

    def source_name(self):
        n = self.number()
        name = self.take(n)
        if name.startswith("_GLOBAL__N"):
            return ""
        return name

    def seq_id(self):
        # S_ or S<base36>_
        while self.peek() and self.peek() != "_":
            self.take()
        self.take()

    def substitution(self):
        self.take()  # S
        c = self.peek()
        if c in ("t", "a", "b", "s", "i", "o", "d"):
            self.take()
            return "std" if c == "t" else None
        self.seq_id()
        return None

    def template_args(self):
        self.take()  # I
        while self.peek() != "E":
            self.template_arg()
        self.take()

    def template_arg(self):
        c = self.peek()
        if c == "L":
            self.literal()
        elif c == "X":
            self.take()
            depth = 1
            while depth:
                d = self.take()
                depth += 1 if d in "XIL" else (-1 if d == "E" else 0)
        elif c == "J":
            self.take()
            while self.peek() != "E":
                self.template_arg()
            self.take()
        else:
            self.type()

    def literal(self):
        self.take()  # L
        if self.peek(2) == "_Z":
            self.take(2)
            self.encoding_name()
        else:
            self.type()
            while self.peek() != "E":
                self.take()
        self.take()

    def type(self):
        c = self.peek()
        if not c:
            raise _Demangle()
        if c in BUILTIN_TYPES:
            self.take()
            return
        if c == "u":
            self.take()
            self.source_name()
            return
        if c in "PRKVrOC G":
            self.take()
            self.type()
            return
        if c == "D":
            self.take()
            d = self.take()
            if d in ("t", "T"):
                self.expression_until_e()
            elif d in ("p",):
                self.type()
            return
        if c.isdigit() or c == "N" or c == "Z":
            self.name()
            return
        if c == "S":
            self.substitution()
            if self.peek() == "I":
                self.template_args()
            elif self.s[self.i - 1:self.i] == "t" or self.peek().isdigit():
                if self.peek().isdigit():
                    self.source_name()
                    if self.peek() == "I":
                        self.template_args()
            return
        if c == "T":
            self.take()
            self.seq_id()
            if self.peek() == "I":
                self.template_args()
            return
        if c == "F":
            self.take()
            if self.peek() == "Y":
                self.take()
            while self.peek() != "E":
                self.type()
            self.take()
            return
        if c == "A":
            self.take()
            while self.peek() != "_":
                self.take()
            self.take()
            self.type()
            return
        if c == "M":
            self.take()
            self.type()
            self.type()
            return
        raise _Demangle()

    def expression_until_e(self):
        depth = 1
        while depth:
            d = self.take()
            depth += 1 if d in "IL" else (-1 if d == "E" else 0)

    def unqualified(self, parts):
        c = self.peek()
        if c.isdigit():
            return self.source_name()
        if c == "C" and self.peek(2)[1:] in ("1", "2", "3", "4", "5"):
            self.take(2)
            return parts[-1] if parts else "?"
        if c == "D" and self.peek(2)[1:] in ("0", "1", "2", "4", "5"):
            self.take(2)
            return "~" + (parts[-1] if parts else "?")
        if c == "L":  # internal linkage marker before a source name
            self.take()
            return self.unqualified(parts)
        code = self.peek(2)
        if code == "cv":
            self.take(2)
            self.type()
            return "operator cast"
        if code in OPERATORS:
            self.take(2)
            return OPERATORS[code]
        if c == "v" and self.peek(2)[1:].isdigit():
            self.take(2)
            return self.source_name()
        raise _Demangle()

    def name(self):
        """Return the list of qualified-name parts."""
        c = self.peek()
        parts = []
        if c == "N":
            self.take()
            while self.peek() in ("r", "V", "K", "R", "O"):
                self.take()
            while self.peek() != "E":
                if self.peek() == "S":
                    sub = self.substitution()
                    if sub:
                        parts.append(sub)
                    continue
                if self.peek() == "I":
                    self.template_args()
                    continue
                if self.peek() == "T":
                    self.take()
                    self.seq_id()
                    continue
                part = self.unqualified(parts)
                if part:
                    parts.append(part)
            self.take()
            return parts
        if c == "Z":  # local name: <function> E <entity>
            self.take()
            self.encoding_name()
            self.take()  # E
            return ["<local>"] + self.name()
        if c == "S" and self.peek(2) == "St":
            self.take(2)
            parts = ["std", self.unqualified([])]
        else:
            parts = [self.unqualified([])]
        if self.peek() == "I":
            self.template_args()
        return parts

    def encoding_name(self):
        parts = self.name()
        # the parameter types follow; the caller does not need them
        while self.peek() and self.peek() != "E":
            try:
                self.type()
            except _Demangle:
                break
        return parts


def demangle_qualname(sym):
    """'Cls::Method' for an Itanium function or object name, None for special names or a failure."""
    if not sym.startswith("_Z") or sym.startswith(("_ZT", "_ZTh", "_ZGV")):
        return None
    d = _Dm(sym)
    d.take(2)
    try:
        parts = d.name()
    except (_Demangle, IndexError):
        return None
    if not parts or "<local>" in parts:
        return None
    return "::".join(parts)


def class_of_special(sym):
    """'Cls' for _ZTV / _ZTI / _ZTS names (vtable, typeinfo), else None."""
    if not sym.startswith(("_ZTV", "_ZTI", "_ZTS")):
        return None
    d = _Dm(sym)
    d.take(4)
    try:
        parts = d.name()
    except (_Demangle, IndexError):
        return None
    return "::".join(parts) if parts else None


def normalize_qualname(q):
    """Remove template arguments and white space: 'Foo<T> :: Bar' -> 'Foo::Bar'."""
    out, depth = [], 0
    for ch in q:
        if ch == "<" and not out[-8:] == list("operator"):
            depth += 1
            continue
        if ch == ">" and depth:
            depth -= 1
            continue
        if not depth and not ch.isspace():
            out.append(ch)
    s = "".join(out)
    return s[2:] if s.startswith("::") else s


# ---------------------------------------------------------------------------
# symbol index: names -> {(module, addr)}

SYM_RE = re.compile(r"^(\S+)\s+kind:(\w+)\S*\s.*?addr:(0x[0-9a-fA-F]+)")


def module_of(symbols_path, config_dir):
    p = Path(symbols_path).parent
    rel = p.relative_to(config_dir).parts
    if not rel:
        return "?"
    return rel[-1]


class SymbolIndex:
    def __init__(self, root):
        self.raw = {}     # name as written -> {(module, addr)}
        self.qual = {}    # demangled qualified name (or the C name) -> {(module, addr)}
        self.count = 0
        config = decomp_root.path(root) / "config"
        if not config.is_dir():
            return
        for path in sorted(config.rglob("symbols.txt")):
            module = module_of(path, config)
            with open(path, encoding="utf-8", errors="replace") as fh:
                for line in fh:
                    m = SYM_RE.match(line)
                    if not m:
                        continue
                    name, addr = m.group(1), m.group(3).lower()
                    key = (module, addr)
                    self.count += 1
                    self.raw.setdefault(name, set()).add(key)
                    q = demangle_qualname(name) if name.startswith("_Z") else name
                    if q:
                        self.qual.setdefault(normalize_qualname(q), set()).add(key)

    def lookup(self, qname):
        if not qname:
            return set()
        if qname in self.raw:
            return set(self.raw[qname])
        parts = qname.split("::")
        while parts:
            hit = self.qual.get("::".join(parts))
            if hit:
                return set(hit)
            if len(parts) <= 2:
                break
            parts = parts[1:]
        return set()


# ---------------------------------------------------------------------------
# rename map: old spelling -> new spelling, derived from the (module, addr) keyed rows

class RenameMap:
    def __init__(self, maps_dir=None):
        self.names = {}      # old qualified (or C) name -> {new}
        self.scopes = {}     # old class/namespace qualified name -> {new: votes}
        self.rows = 0
        self.patchmap = {}   # before rel -> [after rel]
        self._scope_best = {}
        if not maps_dir:
            return
        maps_dir = Path(maps_dir)
        seen = set()
        for fname in ("renames.tsv", "renames_kind.tsv"):
            p = maps_dir / fname
            if not p.is_file():
                continue
            for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
                if not line or line.startswith("#"):
                    continue
                cols = line.split("\t")
                if len(cols) < 4:
                    continue
                key = (cols[0], cols[1].lower(), cols[2], cols[3])
                if key in seen:
                    continue
                seen.add(key)
                self.rows += 1
                self.add(cols[2], cols[3])
        p = maps_dir / "patchmap.tsv"
        if p.is_file():
            for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
                if not line or line.startswith("#"):
                    continue
                cols = line.split("\t")
                if len(cols) >= 3 and cols[2] not in ("", "-"):
                    self.patchmap[cols[0]] = [x for x in re.split(r"[,;\s]+", cols[2]) if x]
        self._scope_best = {}
        for old, votes in self.scopes.items():
            best = max(votes.items(), key=lambda kv: (kv[1], kv[0]))
            self._scope_best[old] = best[0]

    @staticmethod
    def _qual(sym):
        if sym.startswith("_Z"):
            c = class_of_special(sym)
            if c:
                return None
            q = demangle_qualname(sym)
            return normalize_qualname(q) if q else None
        return sym

    def add(self, old, new):
        cold, cnew = class_of_special(old), class_of_special(new)
        if cold and cnew:
            if cold != cnew:
                self.scopes.setdefault(cold, {}).setdefault(cnew, 0)
                self.scopes[cold][cnew] += 1000  # a vtable/typeinfo rename names the class outright
            return
        qo, qn = self._qual(old), self._qual(new)
        if not qo or not qn:
            return
        if qo != qn:
            self.names.setdefault(qo, set()).add(qn)
        po, pn = qo.split("::"), qn.split("::")
        if len(po) >= 2 and len(pn) >= 2:
            so, sn = "::".join(po[:-1]), "::".join(pn[:-1])
            if so != sn:
                self.scopes.setdefault(so, {}).setdefault(sn, 0)
                self.scopes[so][sn] += 1

    def finish(self):
        self._scope_best = {}
        for old, votes in self.scopes.items():
            best = max(votes.items(), key=lambda kv: (kv[1], kv[0]))
            self._scope_best[old] = best[0]

    def translate(self, qname):
        """Every new spelling an old qualified name may have at the target (always includes itself)."""
        out = {qname}
        if not qname:
            return out
        out |= self.names.get(qname, set())
        parts = qname.split("::")
        for k in range(len(parts) - 1, 0, -1):
            scope = "::".join(parts[:k])
            new_scope = self._scope_best.get(scope)
            if new_scope:
                rest = parts[k:]
                old_last = parts[k - 1]
                new_last = new_scope.split("::")[-1]
                # constructors and destructors carry the class name
                rest = [new_last if r == old_last else ("~" + new_last if r == "~" + old_last else r)
                        for r in rest]
                out.add("::".join([new_scope] + rest))
                break
        if len(parts) == 1 and qname in self._scope_best:
            out.add(self._scope_best[qname])
        return out


# ---------------------------------------------------------------------------
# C / C++ scope scanner

TYPE_WORDS = {
    "if", "while", "for", "switch", "return", "sizeof", "alignof", "alignas", "decltype", "__attribute__",
    "__declspec", "_Pragma", "__pragma", "static_assert", "_Static_assert", "void", "int", "char", "short",
    "long", "unsigned", "signed", "float", "double", "bool", "const", "volatile", "static", "inline",
    "extern", "asm", "__asm", "throw", "noexcept", "defined", "case", "do", "else", "new", "delete",
}
QID = (r"(?:[A-Za-z_]\w*\s*(?:<[^;{}()]*?>)?\s*::\s*)*"
       r"(?:~\s*[A-Za-z_]\w*|operator\s*(?:\(\s*\)|\[\s*\]|new\b(?:\s*\[\s*\])?|delete\b(?:\s*\[\s*\])?"
       r"|->\*?|[+\-*/%^&|~!=<>,]+|[A-Za-z_][\w\s:*&]*?)|[A-Za-z_]\w*)")
CALL_RE = re.compile(r"(?<![\w:~])(" + QID + r")\s*(?:<[^;{}()]*?>\s*)?\(")
IDENT_RE = re.compile(r"[A-Za-z_]\w*")
CLASS_RE = re.compile(r"^(?:template\s*<.*?>\s*)?(?:typedef\s+)?(?:class|struct|union)\b\s*"
                      r"(?:__attribute__\s*\(\(.*?\)\)\s*|alignas\s*\(.*?\)\s*|__declspec\s*\(.*?\)\s*)*"
                      r"([A-Za-z_]\w*(?:\s*<[^{]*>)?)?\s*(?:final\s*)?(?::(?!:).*)?$", re.S)


class Scope:
    __slots__ = ("kind", "name", "start", "brace", "end", "parent", "sid")

    def __init__(self, kind, name, start, brace, parent, sid):
        self.kind, self.name, self.start, self.brace = kind, name, start, brace
        self.end = None
        self.parent, self.sid = parent, sid

    def chain(self):
        s, out = self, []
        while s is not None:
            out.append(s)
            s = s.parent
        return out  # innermost first

    def qual_prefix(self):
        parts = [s.name for s in reversed(self.chain()) if s.kind in ("namespace", "class") and s.name]
        return "::".join(parts)


class Item:
    """A top-level-ish declaration: a function definition, a declaration, #include or #define."""
    __slots__ = ("kind", "name", "start", "end", "container")

    def __init__(self, kind, name, start, end, container):
        self.kind, self.name, self.start, self.end, self.container = kind, name, start, end, container


def _paren_depths(p):
    depth, out = 0, []
    for ch in p:
        if ch in ")]":
            depth -= 1
        out.append(depth)
        if ch in "([":
            depth += 1
    return out


def _top_level_has(p, ch):
    depths = _paren_depths(p)
    return any(c == ch and d == 0 for c, d in zip(p, depths))


def _function_name(p):
    depths = _paren_depths(p)
    for m in CALL_RE.finditer(p):
        if depths[m.start()] != 0:
            continue
        name = re.sub(r"\s+", "", m.group(1))
        if name in TYPE_WORDS or name.split("::")[-1] in TYPE_WORDS:
            continue
        return normalize_qualname(m.group(1))
    return None


def _top_level_eq(p):
    depths = _paren_depths(p)
    for k, ch in enumerate(p):
        if ch != "=" or depths[k] != 0:
            continue
        prev = p[k - 1] if k else ""
        nxt = p[k + 1] if k + 1 < len(p) else ""
        if prev in "=!<>+-*/%&|^" or nxt == "=":
            continue
        if re.search(r"operator\s*$", p[:k]):
            continue
        return k
    return -1


def _declarator_name(p):
    """The declared name of 'type name[...] = ...' / 'type name : 3' / 'typedef ... name'."""
    p = p.strip()
    m = re.search(r"\(\s*\*\s*([A-Za-z_]\w*)\s*\)", p)
    if m and p.startswith("typedef"):
        return m.group(1)
    p = re.sub(r"\[[^\]]*\]", "", p)
    p = re.sub(r":\s*\d+\s*$", "", p)
    ids = [i for i in IDENT_RE.findall(p) if i not in TYPE_WORDS]
    return ids[-1] if ids else None


def classify_prefix(p, inner_kind):
    """(kind, name) for the statement text before a '{'."""
    if inner_kind in ("function", "block", "init", "enum"):
        return "block", None
    p = " ".join(p.split())
    if not p:
        return "other", None
    m = re.search(r"\bnamespace\b\s*([\w:]*)\s*$", p)
    if m:
        return "namespace", m.group(1) or ""
    if re.match(r'^extern\s*""\s*$', p):
        return "extern", None
    eq = _top_level_eq(p)
    if eq >= 0:
        return "init", _declarator_name(p[:eq])
    head = p.split(":", 1)[0] if "::" not in p.split(":", 1)[0] else p
    m = CLASS_RE.match(p)
    if m and "(" not in head:
        name = m.group(1)
        return "class", normalize_qualname(name) if name else ""
    if re.match(r"^(?:typedef\s+)?enum\b", p):
        m = re.match(r"^(?:typedef\s+)?enum\s*(?:class\s+|struct\s+)?([A-Za-z_]\w*)?", p)
        return "enum", m.group(1) if m and m.group(1) else ""
    if _top_level_has(p, "("):
        name = _function_name(p)
        if name:
            return "function", name
    return "other", _declarator_name(p)


def _decl_name(p):
    p = " ".join(p.split())
    if not p:
        return None
    if p.startswith(("using ", "friend ", "template")) and "(" not in p:
        ids = IDENT_RE.findall(p)
        return ids[-1] if ids else None
    eq = _top_level_eq(p)
    if eq >= 0:
        return _declarator_name(p[:eq])
    if not p.startswith("typedef") and _top_level_has(p, "("):
        name = _function_name(p)
        if name:
            return name
    return _declarator_name(p)


class ScanResult:
    def __init__(self):
        self.line_stack = []   # per line (0-based): Scope innermost at the line's first code char
        self.scopes = []
        self.items = []
        self.warnings = []


def scan(text):
    """Scan C/C++ text; record the scope innermost at the start of every line, scopes and items."""
    res = ScanResult()
    n = len(text)
    i = 0
    line = 0
    root = Scope("file", "", 0, 0, None, 0)
    root.end = None
    cur = root
    res.scopes.append(root)
    prefix = []
    prefix_start = None
    at_line_start = True
    line_scope = [root]  # filled as lines start
    pp_stack = []  # [(scope at #if, first-branch end scope or None)]
    sid = 1

    while i < n:
        ch = text[i]
        if ch == "\n":
            line += 1
            i += 1
            at_line_start = True
            line_scope.append(cur)
            continue
        if at_line_start and ch in " \t\r\f\v":
            i += 1
            continue
        if at_line_start and ch == "#":
            # preprocessor directive, with continuations and comments
            j = i
            buf = []
            while j < n:
                c = text[j]
                if c == "\\" and j + 1 < n and text[j + 1] in "\r\n":
                    j += 1
                    if text[j] == "\r" and j + 1 < n and text[j + 1] == "\n":
                        j += 1
                    j += 1
                    line += 1
                    line_scope.append(cur)
                    continue
                if c == "/" and j + 1 < n and text[j + 1] == "*":
                    k = text.find("*/", j + 2)
                    k = n if k < 0 else k + 2
                    nl = text.count("\n", j, k)
                    for _ in range(nl):
                        line_scope.append(cur)
                    line += nl
                    j = k
                    buf.append(" ")
                    continue
                if c == "/" and j + 1 < n and text[j + 1] == "/":
                    while j < n and text[j] != "\n":
                        j += 1
                    break
                if c == "\n":
                    break
                buf.append(c)
                j += 1
            directive = "".join(buf)[1:].strip()
            word = re.match(r"(\w*)", directive).group(1)
            if word in ("if", "ifdef", "ifndef"):
                pp_stack.append([cur, None])
            elif word in ("elif", "else"):
                if pp_stack:
                    if pp_stack[-1][1] is None:
                        pp_stack[-1][1] = cur
                    cur = pp_stack[-1][0]
            elif word == "endif":
                if pp_stack:
                    at_if, first_end = pp_stack.pop()
                    if first_end is not None:
                        cur = first_end
            elif word in ("include", "define"):
                arg = directive[len(word):].strip()
                if word == "define":
                    m = IDENT_RE.match(arg)
                    nm = "#define " + (m.group(0) if m else arg)
                else:
                    nm = "#include " + arg.strip('<>"')
                res.items.append(Item("pp", nm, line, line, cur))
            i = j
            at_line_start = False
            continue
        at_line_start = False
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "*":
            k = text.find("*/", i + 2)
            k = n if k < 0 else k + 2
            nl = text.count("\n", i, k)
            for _ in range(nl):
                line_scope.append(cur)
            line += nl
            i = k
            prefix.append(" ")
            if nl:
                at_line_start = False
            continue
        if ch in "\"'":
            j = i + 1
            while j < n and text[j] != ch and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            i = j + 1
            if prefix_start is None:
                prefix_start = line
            prefix.append('""' if ch == '"' else "0")
            continue
        if ch == "{":
            p = "".join(prefix)
            kind, name = classify_prefix(p, cur.kind)
            start = prefix_start if prefix_start is not None else line
            s = Scope(kind, name, start, line, cur, sid)
            sid += 1
            res.scopes.append(s)
            cur = s
            prefix, prefix_start = [], None
            i += 1
            continue
        if ch == "}":
            if cur.parent is not None:
                cur.end = line
                done = cur
                cur = cur.parent
                if done.kind in ("function", "class", "init", "enum", "namespace") and done.name is not None:
                    res.items.append(Item(done.kind, done.name, done.start, line, cur))
            else:
                res.warnings.append("unbalanced '}' at line %d" % (line + 1))
            prefix, prefix_start = [], None
            i += 1
            continue
        if ch == ";":
            if cur.kind in ("file", "namespace", "class", "extern"):
                nm = _decl_name("".join(prefix))
                if nm:
                    res.items.append(Item("decl", nm, prefix_start if prefix_start is not None else line,
                                          line, cur))
            prefix, prefix_start = [], None
            i += 1
            continue
        if prefix_start is None and not ch.isspace():
            prefix_start = line
        prefix.append(ch)
        i += 1
    if cur is not root:
        res.warnings.append("%d scope(s) still open at end of file" % len(cur.chain()[:-1]))
    res.line_stack = line_scope
    return res


# ---------------------------------------------------------------------------
# anchors

class Anchor:
    __slots__ = ("kind", "qual", "container", "keys", "note", "chain", "names")

    def __init__(self, kind, qual, container="", keys=None, note="", chain=(), names=()):
        self.kind, self.qual, self.container = kind, qual, container
        self.keys = keys or set()
        self.note = note
        self.chain = list(chain)   # filescope: preceding (qual, keys) in the container, nearest first
        self.names = list(names)   # filescope: (qual, keys) of every declaration in the container

    def text(self):
        k = ",".join("%s:%s" % kv for kv in sorted(self.keys)) if self.keys else "-"
        if self.kind == "filescope":
            return "filescope[%s] after %s {%s}" % (self.container or "file", self.qual or "<scope start>", k)
        return "%s %s {%s}" % (self.kind, self.qual, k)


def _all_added(start, end, added):
    if end is None:
        return False
    return all(ln in added for ln in range(start, end + 1))


def _effective(scope):
    """The class, namespace or file scope a declaration belongs to (extern "C" blocks are transparent)."""
    while scope is not None and scope.kind not in ("class", "namespace", "file"):
        scope = scope.parent
    return scope


def anchor_at(res, line, added, symbols):
    """The anchor of a change at 0-based line `line` of the patched file."""
    if line >= len(res.line_stack):
        line = len(res.line_stack) - 1
    if line < 0:
        return None
    scope = res.line_stack[line]
    # a line of a definition's header (before its '{') belongs to that definition
    for s in res.scopes:
        if s.parent is scope and s.kind in ("function", "init", "enum") and s.start <= line <= s.brace:
            scope = s
            break
    for s in scope.chain():
        if s.kind in ("function", "init", "enum") and not _all_added(s.start, s.end, added):
            prefix = s.qual_prefix()
            qual = "::".join(x for x in (prefix, s.name or "") if x)
            kind = "function" if s.kind == "function" else "data"
            return Anchor(kind, qual, keys=symbols.lookup(qual))
        if s.kind in ("class", "namespace", "file") and not _all_added(s.start, s.end, added):
            container = s
            break
    else:
        container = res.scopes[0]
    cont_q = "::".join(x for x in (container.qual_prefix(),) if x)

    def ident(it):
        if it.kind == "pp":
            return it.name, set()
        q = "::".join(x for x in (cont_q, it.name) if x)
        return q, symbols.lookup(q)

    own, preceding = [], []
    for it in res.items:
        if _effective(it.container) is not container or it.kind == "namespace":
            continue
        if _all_added(it.start, it.end if it.end is not None else it.start, added):
            continue
        own.append(it)
        if it.start >= line or (it.end is not None and it.end >= line and it.kind != "pp"):
            continue
        preceding.append(it)
    preceding.sort(key=lambda it: it.start, reverse=True)
    chain = [ident(it) for it in preceding]
    names = [ident(it) for it in own]
    if not chain:
        return Anchor("filescope", "", cont_q, note="no preceding declaration in this scope", names=names)
    q, keys = chain[0]
    return Anchor("filescope", q, cont_q, keys=keys, chain=chain, names=names)


# ---------------------------------------------------------------------------
# patches -> change groups

class Group:
    __slots__ = ("path", "hunk", "index", "minus", "plus", "lines", "plus_lines", "anchors", "error",
                 "partner", "result", "basis", "kind", "ctx", "part")

    def __init__(self, path, hunk, index):
        self.path, self.hunk, self.index = path, hunk, index
        self.minus, self.plus = [], []
        self.lines = []   # 0-based lines in the patched file the change touches
        self.plus_lines = []  # the added ones among them, in order (parallel to self.plus)
        self.ctx = None   # (scan result, effective added set, insignificant set, symbol index)
        self.part = None  # (a, b): this BEFORE group is added lines a..b-1 of its AFTER partner
        self.anchors = []
        self.error = None
        self.partner = None
        self.result = None
        self.basis = ""
        self.kind = ""

    def signature(self):
        return (tuple(self.minus), tuple(self.plus))

    def text(self):
        return "\n".join(["-" + x for x in self.minus] + ["+" + x for x in self.plus])


def _norm_line(b):
    return " ".join(b.decode("latin-1").split())


def patch_groups(rel, patch):
    """Change groups of a patch, with 0-based line numbers in the patched (new) file."""
    lines = dov.split_lines(patch)
    groups = []
    added = set()
    i, n = 0, len(lines)
    while i < n and not lines[i].startswith(b"@@"):
        i += 1
    hunk_no = 0
    while i < n:
        m = dov.HUNK_RE.match(lines[i])
        if not m:
            i += 1
            continue
        hunk_no += 1
        oc = 1 if m.group(2) is None else int(m.group(2))
        nc = 1 if m.group(4) is None else int(m.group(4))
        new_pos = int(m.group(3)) - 1 if nc else int(m.group(3))
        o_left, n_left = oc, nc
        i += 1
        g = None
        gi = 0
        while i < n and (o_left > 0 or n_left > 0 or lines[i].startswith(b"\\")):
            tag, body = lines[i][:1], lines[i][1:]
            if tag == b"\\":
                i += 1
                continue
            if tag == b" ":
                g = None
                new_pos += 1
                o_left -= 1
                n_left -= 1
            else:
                if g is None:
                    gi += 1
                    g = Group(rel, hunk_no, gi)
                    groups.append(g)
                if tag == b"-":
                    g.minus.append(_norm_line(body))
                    g.lines.append(new_pos)  # where the removed line was: the line now at that spot
                    o_left -= 1
                else:
                    g.plus.append(_norm_line(body))
                    g.lines.append(new_pos)
                    g.plus_lines.append(new_pos)
                    added.add(new_pos)
                    new_pos += 1
                    n_left -= 1
            i += 1
    return groups, added


def load_patched(decomp_root, rel, patch):
    """(patched text, None) or (None, error)."""
    path = Path(decomp_root) / rel
    if not path.is_file():
        return None, "decomp file missing: %s" % rel
    data = dov.canonical(path.read_bytes())
    try:
        new = dov.apply_patch(rel, data, patch)
    except dov.OverlayError as e:
        return None, "patch does not apply: %s" % e
    return new.decode("latin-1"), None


PP_COND_RE = re.compile(r"^#\s*(?:if|ifdef|ifndef|elif|else|endif)\b")
INSIG_RE = re.compile(r"^[{}();,\s]*$")


def strip_comments(text):
    """The text with comments blanked and string contents removed; line breaks are kept."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            k = text.find("*/", i + 2)
            k = n if k < 0 else k + 2
            out.append(" " + "\n" * text.count("\n", i, k))
            i = k
            continue
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(c + c)
            i = j + 1
            continue
        out.append(c)
        i += 1
    return "".join(out)


def insignificant_lines(text):
    """Lines that say nothing about where a change sits: blank, comment only, bare braces/parens,
    and preprocessor conditionals. A diff slides these freely (a hook added after a function is often
    cut as '+}' at the top of the block), so anchors and 'made only of added lines' ignore them."""
    out = set()
    for k, ln in enumerate(strip_comments(text).split("\n")):
        t = ln.strip()
        if INSIG_RE.match(t) or PP_COND_RE.match(t):
            out.add(k)
    return out


def group_anchors(g, plus_slice=None):
    """Distinct anchors of a group's significant lines (all its lines when none is significant).
    plus_slice=(a, b) restricts to the group's added lines a..b-1 (used when one AFTER group holds
    a BEFORE group as a part)."""
    res, eff_added, insig, symbols = g.ctx
    lines = g.plus_lines[plus_slice[0]:plus_slice[1]] if plus_slice is not None else g.lines
    use = [ln for ln in lines if ln not in insig] or list(lines)
    seen = []
    for ln in use:
        a = anchor_at(res, ln, eff_added, symbols)
        if a is None:
            continue
        if not any(a.text() == b.text() for b in seen):
            seen.append(a)
    return seen


def analyse_side(decomp_root, patches_dir, rels, symbols):
    """{rel: (groups, scan warnings, error)} for every patch rel on one side."""
    out = {}
    for rel in rels:
        pf = Path(patches_dir) / (rel + dov.PATCH_SUFFIX)
        if not pf.is_file():
            out[rel] = ([], [], "no patch file")
            continue
        patch = pf.read_bytes()
        groups, added = patch_groups(rel, patch)
        text, err = load_patched(decomp_root, rel, patch)
        if err:
            for g in groups:
                g.error = err
            out[rel] = (groups, [], err)
            continue
        res = scan(text)
        insig = insignificant_lines(text)
        eff_added = added | insig
        for g in groups:
            g.ctx = (res, eff_added, insig, symbols)
            g.anchors = group_anchors(g)
            if not g.anchors:
                g.error = "no anchor found"
        out[rel] = (groups, res.warnings, None)
    return out


def patch_rels(patches_dir):
    rels = []
    pdir = Path(patches_dir)
    if not pdir.is_dir():
        return rels
    for p in pdir.rglob("*" + dov.PATCH_SUFFIX):
        rels.append(p.relative_to(pdir).as_posix()[:-len(dov.PATCH_SUFFIX)])
    return sorted(rels)


def read_kinds(edits_path):
    kinds = {}
    if edits_path and Path(edits_path).is_file():
        for line in Path(edits_path).read_text(encoding="utf-8", errors="replace").splitlines():
            cols = line.split("\t")
            if len(cols) >= 3 and cols[0] != "path" and not line.startswith("#"):
                kinds[cols[0]] = cols[2]
    return kinds


def read_allow(path):
    allow = {}
    if not path:
        return allow
    for line in Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        cols = line.split("\t")
        rel = cols[0].strip()
        hunk = cols[1].strip() if len(cols) > 1 and cols[1].strip() else "*"
        reason = cols[2].strip() if len(cols) > 2 else ""
        allow[(rel, hunk)] = reason
    return allow


# ---------------------------------------------------------------------------
# comparison

def compare_anchor(b, a, renames):
    """(same?, basis)."""
    if b.kind != a.kind:
        return False, "kind %s -> %s" % (b.kind, a.kind)
    if b.kind == "filescope":
        if a.container not in renames.translate(b.container) and not (not a.container and not b.container):
            return False, "container %s -> %s" % (b.container or "file", a.container or "file")
    same, basis = _same_ident((b.qual, b.keys), (a.qual, a.keys), renames)
    if same or b.kind != "filescope":
        return same, basis
    # A filescope anchor is the nearest preceding declaration. When main deleted that declaration, or
    # added a new one between it and the hook, the nearest one changes although the hook did not move:
    # compare the nearest preceding declarations that exist on BOTH sides instead.
    b_eff = next((x for x in b.chain if any(_same_ident(x, y, renames)[0] for y in a.names)), None)
    a_eff = next((y for y in a.chain if any(_same_ident(x, y, renames)[0] for x in b.names)), None)
    if b_eff is None and a_eff is None:
        return True, "start of scope (neighbours changed by main: %s -> %s)" % (b.qual or "-", a.qual or "-")
    if b_eff is not None and a_eff is not None:
        same2, basis2 = _same_ident(b_eff, a_eff, renames)
        if same2:
            return True, "%s, after %s on both sides (nearest neighbour changed by main: %s -> %s)" % (
                basis2, a_eff[0], b.qual or "-", a.qual or "-")
    return False, basis


def _same_ident(b, a, renames):
    """b, a: (qual, keys). (same?, basis)."""
    bq, bk = b
    aq, ak = a
    if bk and ak:
        if bk & ak:
            return True, "addr"
        return False, "addr differs"
    if not bq and not aq:
        return True, "start of scope"
    if aq in renames.translate(bq):
        return True, "name" if aq == bq else "name via map"
    return False, "name %s -> %s" % (bq, aq)


def _sublist_at(small, big):
    """Index where small occurs contiguously in big, or -1 (an empty small matches at 0)."""
    if not small:
        return 0
    k = len(small)
    for i in range(len(big) - k + 1):
        if big[i:i + k] == small:
            return i
    return -1


def pair_all(before, after, mapping, before_rels):
    """Pair BEFORE groups with AFTER groups across all files, in passes from strongest to weakest:
    identical change text in the same path, identical text in a mapped path, similar text (ratio
    >= 0.6, same path preferred), then containment (a BEFORE group that is a contiguous part of a
    bigger AFTER group, as when main deleted the lines that separated two hooks)."""
    def candidates(rel):
        out = []
        for t in mapping[rel]:
            out.extend(after[t][0])
        return out

    for same_only in (True, False):
        for rel in before_rels:
            for g in before[rel][0]:
                if g.partner is not None:
                    continue
                for x in candidates(rel):
                    if x.partner is None and (x.path == rel or not same_only) and x.signature() == g.signature():
                        g.partner, x.partner = x, g
                        break
    for rel in before_rels:
        for g in before[rel][0]:
            if g.partner is not None:
                continue
            best, best_key = None, None
            for x in candidates(rel):
                if x.partner is not None:
                    continue
                r = difflib.SequenceMatcher(None, g.text(), x.text(), autojunk=False).ratio()
                key = (r, x.path == rel)
                if r >= 0.6 and (best_key is None or key > best_key):
                    best, best_key = x, key
            if best is not None:
                g.partner, best.partner = best, g
    for rel in before_rels:
        for g in before[rel][0]:
            if g.partner is not None or not (g.plus or g.minus):
                continue
            for x in sorted(candidates(rel), key=lambda x: x.path != rel):
                at = _sublist_at(g.plus, x.plus)
                if (at >= 0 and _sublist_at(g.minus, x.minus) >= 0
                        and len(g.plus) + len(g.minus) < len(x.plus) + len(x.minus)):
                    g.partner = x
                    if x.partner is None:
                        x.partner = g
                    g.part = (at, at + len(g.plus)) if g.plus else None
                    break


def run(args):
    renames = RenameMap(args.maps)
    sym_before = SymbolIndex(args.before_root)
    sym_after = SymbolIndex(args.after_root)
    kinds = read_kinds(args.edits)
    want = set(k.strip() for k in args.kinds.split(",") if k.strip())
    allow = read_allow(args.allow)

    before_rels = patch_rels(args.before_patches)
    after_rels = set(patch_rels(args.after_patches))

    def is_hook(rel):
        return kinds.get(rel, "HOOK" if not kinds else None) in want

    # every before patch takes part in pairing (a non-HOOK change moved into the same TU must not show
    # up as an after-only change); only HOOK patches are reported
    mapping = {}
    for rel in before_rels:
        targets = []
        if rel in after_rels:
            targets.append(rel)
        for t in renames.patchmap.get(rel, []):
            if t in after_rels and t not in targets:
                targets.append(t)
        mapping[rel] = targets

    before = analyse_side(args.before_root, args.before_patches, before_rels, sym_before)
    after_needed = sorted(set(t for ts in mapping.values() for t in ts))
    after = analyse_side(args.after_root, args.after_patches, after_needed, sym_after)

    pair_all(before, after, mapping, before_rels)

    rows = []
    hook_after = set()
    for rel in before_rels:
        if not is_hook(rel):
            continue
        hook_after.update(mapping[rel])
        groups, warnings, err = before[rel]
        if err == "no patch file":
            rows.append(("UNKNOWN", rel, "*", "-", "-", "-", "", "before patch missing"))
            continue
        hunks = {}
        for g in groups:
            hunks.setdefault(g.hunk, []).append(g)
        for h in sorted(hunks):
            gs = hunks[h]
            for g in gs:
                p = g.partner
                p_anchors = []
                if p is not None and not p.error:
                    p_anchors = group_anchors(p, g.part) if g.part else p.anchors
                if g.error:
                    g.result, g.basis = "UNKNOWN", g.error
                elif not mapping[rel]:
                    g.result, g.basis = "UNKNOWN", "no after patch for this path"
                elif p is None:
                    g.result, g.basis = "UNKNOWN", "no counterpart change after"
                elif p.error:
                    g.result, g.basis = "UNKNOWN", "after: " + p.error
                elif len(g.anchors) != len(p_anchors):
                    g.result, g.basis = "MOVED", "anchor count %d -> %d" % (len(g.anchors), len(p_anchors))
                else:
                    ok, bases = True, []
                    for ba, aa in zip(g.anchors, p_anchors):
                        same, basis = compare_anchor(ba, aa, renames)
                        bases.append(basis)
                        ok = ok and same
                    g.result = "SAME" if ok else "MOVED"
                    g.basis = "; ".join(bases)
                if g.result in ("MOVED", "UNKNOWN"):
                    reason = allow.get((rel, str(h)), allow.get((rel, "*")))
                    if reason is not None:
                        g.basis = "listed (was %s: %s): %s" % (g.result, g.basis, reason)
                        g.result = "MOVED_LISTED"
                before_txt = " | ".join(a.text() for a in g.anchors) or "-"
                after_txt = (" | ".join(a.text() for a in p_anchors) or "-") if p is not None else "-"
                where = ("%s#%d" % (p.path, p.hunk)) if p is not None else "-"
                note = g.basis
                if g.part:
                    note += " (part of a larger change after)"
                if warnings:
                    note += " [scan: %s]" % "; ".join(warnings)
                rows.append((g.result, rel, str(h), str(g.index), before_txt, after_txt, where, note))
    for t in sorted(hook_after):
        groups, warnings, err = after[t]
        for g in groups:
            if g.partner is not None:
                continue
            res = "UNKNOWN"
            note = "change only after (no before counterpart)"
            reason = allow.get((t, "after:%d" % g.hunk), allow.get((t, "*")))
            if reason is not None:
                res, note = "MOVED_LISTED", "listed (was UNKNOWN: %s): %s" % (note, reason)
            rows.append((res, t, "after:%d" % g.hunk, str(g.index), "-",
                         " | ".join(a.text() for a in g.anchors) or "-", "%s#%d" % (t, g.hunk), note))

    # per hunk: worst of its groups
    hunk_res = {}
    for r in rows:
        key = (r[1], r[2])
        if key not in hunk_res or SEVERITY[r[0]] > SEVERITY[hunk_res[key]]:
            hunk_res[key] = r[0]
    counts = {k: 0 for k in SEVERITY}
    for v in hunk_res.values():
        counts[v] += 1
    gcounts = {k: 0 for k in SEVERITY}
    for r in rows:
        gcounts[r[0]] += 1
    akinds = {}
    for rel in before_rels:
        if not is_hook(rel):
            continue
        for g in before[rel][0]:
            for a in g.anchors:
                basis = "addr" if a.keys else "name"
                akinds[(a.kind, basis)] = akinds.get((a.kind, basis), 0) + 1

    out = []
    out.append("# hook_anchor_check report")
    out.append("# before_root\t%s" % args.before_root)
    out.append("# after_root\t%s" % args.after_root)
    out.append("# hook_patches\t%d" % sum(1 for r in before_rels if is_hook(r)))
    out.append("# symbols_before\t%d\tsymbols_after\t%d\trename_rows\t%d"
               % (sym_before.count, sym_after.count, renames.rows))
    out.append("# hunks\t" + "\t".join("%s=%d" % (k, counts[k]) for k in SEVERITY))
    out.append("# groups\t" + "\t".join("%s=%d" % (k, gcounts[k]) for k in SEVERITY))
    out.append("# before_anchors\t" + "\t".join("%s/%s=%d" % (k[0], k[1], v) for k, v in sorted(akinds.items())))
    out.append("result\tpath\thunk\tgroup\tbefore_anchor\tafter_anchor\tafter_where\tnote")
    order = sorted(rows, key=lambda r: (-SEVERITY[r[0]], r[1], r[2], int(r[3]) if r[3].isdigit() else 0))
    for r in order:
        out.append("\t".join(x.replace("\t", " ") for x in r))
    Path(args.report).parent.mkdir(parents=True, exist_ok=True)
    Path(args.report).write_text("\n".join(out) + "\n", encoding="utf-8")

    print("hook_anchor_check: %d HOOK patches, hunks: %s" % (
        sum(1 for r in before_rels if is_hook(r)), ", ".join("%s %d" % (k, counts[k]) for k in SEVERITY)))
    print("groups: %s" % ", ".join("%s %d" % (k, gcounts[k]) for k in SEVERITY))
    bad = [r for r in order if r[0] in ("MOVED", "UNKNOWN")]
    for r in bad[:40]:
        print("%s %s hunk %s group %s: %s -> %s (%s)" % (r[0], r[1], r[2], r[3], r[4], r[5], r[7]))
    if len(bad) > 40:
        print("... %d more in %s" % (len(bad) - 40, args.report))
    return 1 if bad else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--before-root", required=True, help="decomp checkout the BEFORE patches apply to")
    ap.add_argument("--after-root", required=True, help="decomp checkout the AFTER patches apply to")
    ap.add_argument("--before-patches", required=True, help="patch dir before (port/decomp_patches)")
    ap.add_argument("--after-patches", required=True, help="patch dir after (the re-cut patches)")
    ap.add_argument("--maps", help="dir with renames.tsv, renames_kind.tsv, patchmap.tsv")
    ap.add_argument("--allow", help="allow-list of intentional moves")
    ap.add_argument("--report", required=True, help="TSV report to write")
    ap.add_argument("--edits", default=str(DEFAULT_EDITS), help="decomp_edits.tsv giving each patch's kind")
    ap.add_argument("--kinds", default="HOOK", help="comma list of kinds to report (default HOOK)")
    args = ap.parse_args(argv)
    return run(args)


if __name__ == "__main__":
    sys.exit(main())
