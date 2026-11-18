#!/usr/bin/env python3
"""Find a decomp source file that a port tool names by an old path.

Port tools carry literal decomp paths such as src/__sinit_ov002_02107370.c or
src/unnamed/ov007/func_ov007_020b3360.c. A decomp bump merges one-function files into
translation units and renames symbols, so such a path can be gone while the code it
names is still there under another file. Opening the literal path then fails, or worse,
a guard that compares it with what the build compiled reads the move as a typo.

resolve(root, rel) answers with the file that owns that code in the checkout at root:

  1. rel itself, when it exists there;
  2. else, when the file name is address shaped (func_XXXXXXXX, func_ovNNN_XXXXXXXX,
     __sinit_ovNNN_XXXXXXXX, data_...), the enrolled file whose delinks range holds
     that address (the checkout's own tools/srcpath.py, owner_at);
  3. else the file srcpath gives for the stem as a symbol name (path_for);
  4. else None.

The answer is repo relative with forward slashes. srcpath is loaded from the checkout
being read, never from this tree, so a tool pointed at another root asks that root.
"""

import importlib.util
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import decomp_root  # noqa: E402

ADDR_STEM = re.compile(r"^(?:func|__sinit|data|sinit)_(?:(ov\d{3})_)?([0-9a-fA-F]{8})$")

_LOADED = {}


def srcpath_for(root):
    """The srcpath module of the checkout at root, pointed at that checkout."""
    root = decomp_root.path(root).resolve()
    key = str(root).lower()
    mod = _LOADED.get(key)
    if mod is None:
        path = root / "tools" / "srcpath.py"
        if not path.is_file():
            return None
        spec = importlib.util.spec_from_file_location("srcpath_%d" % len(_LOADED), path)
        mod = importlib.util.module_from_spec(spec)
        added = str(root / "tools")
        sys.path.insert(0, added)
        try:
            spec.loader.exec_module(mod)
        finally:
            if sys.path and sys.path[0] == added:
                sys.path.pop(0)
        mod.set_root(root)
        _LOADED[key] = mod
    return mod


def address_of(rel):
    """(module, addr) from an address shaped file name, or None."""
    stem = os.path.splitext(os.path.basename(rel.replace("\\", "/")))[0]
    m = ADDR_STEM.match(stem)
    if not m:
        return None
    return (m.group(1) or "arm9"), int(m.group(2), 16)


def _rel(root, path):
    return os.path.relpath(str(path), str(root)).replace("\\", "/")


def resolve(root, rel, module=None, addr=None):
    """The repo relative file at root that holds what rel named, or None (see the module doc).
    module/addr override the address read off the file name."""
    root = decomp_root.path(root).resolve()
    rel = rel.replace("\\", "/")
    if (root / rel).is_file():
        return rel
    sp = srcpath_for(root)
    if sp is None:
        return None
    loc = (module, addr) if module is not None and addr is not None else address_of(rel)
    if loc is not None:
        p = sp.owner_at(loc[0], loc[1])
        if p is not None:
            return _rel(root, p)
    stem = os.path.splitext(os.path.basename(rel))[0]
    p = sp.path_for(stem)
    if p is not None and Path(p).is_file():
        return _rel(root, p)
    return None


def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("root")
    ap.add_argument("rel", nargs="+")
    a = ap.parse_args(argv)
    bad = 0
    for r in a.rel:
        got = resolve(a.root, r)
        bad += got is None
        print("%s\t%s" % (r, got or "-"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
