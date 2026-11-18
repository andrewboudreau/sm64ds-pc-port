#!/usr/bin/env python3
"""The one lookup every port tool uses to read a decomp file the way the build compiles it.

The port compiles some decomp files from a patched copy (port/decomp_patches/README.md): configure
runs `decomp_overlay.py apply` into <build dir>/decomp-overlay/ and writes
<build dir>/decomp-overlay/overlay_manifest.txt, one decomp relative path per line. A tool that
checks decomp source must read the copy the build compiled, never the raw decomp file, or it checks
the wrong text without an error. So:

    view = DecompView(decomp_root, build_dir)
    view.path("src/cstd.cpp")        # overlay copy if the manifest lists it, else the raw file
    view.walk("src", (".c", ".cpp"))  # every file under src/, overlay copies (and overlay-only
                                      # files) in place of the raw ones

The rules, all tested in test_decomp_overlay_paths.py:

  - listed in the manifest: the overlay copy;
  - not listed: the raw decomp file;
  - no manifest (no build yet, or a tool run on a bare tree): raw files only, UNLESS the port's
    layer (port/decomp_patches/*.patch, port/decomp_overlay_files/*) holds anything, in which case
    raw files are the wrong text and DecompView raises OverlayMissing instead of answering. The
    remedy is `decomp_overlay.py apply --decomp-root <root> --out <build dir>/decomp-overlay
    --include-closure include`, which is what configure and port/build-port.cmd run.

The build dir defaults to <decomp root>/build/port, the dir port/build-port.cmd builds into.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import decomp_root as _decomp_root  # noqa: E402
import shutil

MANIFEST_NAME = "overlay_manifest.txt"
OVERLAY_SUBDIR = "decomp-overlay"
# layer files that are bookkeeping, not content
_LAYER_META = {".gitattributes", "README.md", "BASE"}


class OverlayMissing(RuntimeError):
    pass


def norm_rel(rel):
    rel = str(rel).replace("\\", "/")
    while rel.startswith("./"):
        rel = rel[2:]
    return rel.strip("/")


def _same_dir(a, b):
    return os.path.normcase(os.path.realpath(str(a))) == os.path.normcase(os.path.realpath(str(b)))


def default_build_dir(decomp_root):
    """build/port of the port's own tree when decomp_root is the configured decomp (the decomp may
    live outside the port's tree; the build dir never does), else <decomp_root>/build/port (a
    self-contained tree a caller names, such as a test fixture)."""
    if _same_dir(decomp_root, _decomp_root.find()):
        return os.path.join(_decomp_root.PORT_TREE, "build", "port")
    return os.path.join(str(decomp_root), "build", "port")


def overlay_dir_for(build_dir):
    return os.path.join(str(build_dir), OVERLAY_SUBDIR)


def read_manifest(overlay_dir):
    """The manifest's paths, or None when there is no manifest."""
    p = os.path.join(str(overlay_dir), MANIFEST_NAME)
    if not os.path.isfile(p):
        return None
    with open(p, "rb") as fh:
        text = fh.read().decode("utf-8")
    out = []
    for ln in text.splitlines():
        ln = norm_rel(ln.strip())
        if not ln or ln.startswith("../") or "/../" in ln or os.path.isabs(ln):
            continue
        out.append(ln)
    return out


def layer_files(port_dir):
    """Content files of the port's patch layer (patches and whole overlay files)."""
    found = []
    for sub, keep in (("decomp_patches", lambda f: f.endswith(".patch")),
                      ("decomp_overlay_files", lambda f: f not in _LAYER_META)):
        top = os.path.join(str(port_dir), sub)
        if not os.path.isdir(top):
            continue
        for d, dirs, files in os.walk(top):
            dirs.sort()
            for f in sorted(files):
                if keep(f):
                    found.append(os.path.join(d, f))
    return found


class DecompView:
    """Decomp files as the build compiles them. See the module docstring."""

    def __init__(self, decomp_root, build_dir=None, overlay_dir=None, port_dir=None,
                 require_if_layer=True):
        self.root = os.path.abspath(str(decomp_root))
        if overlay_dir is None:
            overlay_dir = overlay_dir_for(build_dir if build_dir is not None
                                          else default_build_dir(self.root))
        self.overlay_dir = os.path.abspath(str(overlay_dir))
        listed = read_manifest(self.overlay_dir)
        self.has_manifest = listed is not None
        self.listed = frozenset(listed or ())
        if not self.has_manifest and require_if_layer:
            if port_dir is not None:
                pd = port_dir
            elif _same_dir(self.root, _decomp_root.find()):
                pd = _decomp_root.PORT_DIR
            else:
                pd = _decomp_root.join(self.root, "port")
            layer = layer_files(pd)
            if layer:
                raise OverlayMissing(
                    "decomp overlay: the port's patch layer has %d file(s) (first: %s) but %s has "
                    "no %s, so the raw decomp files are not what the build compiles. Configure the "
                    "build, or run: python port/tools/decomp_overlay.py apply --decomp-root %s "
                    "--out %s --include-closure include"
                    % (len(layer), _decomp_root.relpath(layer[0], self.root).replace("\\", "/"),
                       self.overlay_dir, MANIFEST_NAME, self.root, self.overlay_dir))

    # ---- single paths
    def has(self, rel):
        return norm_rel(rel) in self.listed

    def path(self, rel):
        """The file to READ for decomp relative path `rel`."""
        rel = norm_rel(rel)
        if rel in self.listed:
            return os.path.join(self.overlay_dir, *rel.split("/"))
        return _decomp_root.join(self.root, *rel.split("/"))

    def raw(self, rel):
        return _decomp_root.join(self.root, *norm_rel(rel).split("/"))

    def exists(self, rel):
        return os.path.isfile(self.path(rel))

    def read_text(self, rel, encoding="utf-8", errors="replace"):
        with open(self.path(rel), "r", encoding=encoding, errors=errors) as fh:
            return fh.read()

    def read_bytes(self, rel):
        with open(self.path(rel), "rb") as fh:
            return fh.read()

    def rel_of(self, path):
        """Decomp relative path of a raw decomp path or an overlay copy, else None. A path spelled
        through a link (the decomp/ folder as a junction or symlink to a checkout) and one spelled
        by its resolved location name the same file."""
        r = self._rel_of(os.path.abspath(str(path)), (self.overlay_dir, self.root))
        if r is None:
            r = self._rel_of(os.path.realpath(str(path)),
                             (os.path.realpath(self.overlay_dir), os.path.realpath(self.root)))
        return r

    def _rel_of(self, p, tops):
        """tops: (overlay dir, decomp root), both spelled the same way as p."""
        for i, top in enumerate(tops):
            try:
                r = os.path.relpath(p, top)
            except ValueError:
                continue
            r = norm_rel(r)
            if r == "." or r.startswith("../") or r == "..":
                continue
            if i == 1 and (r == "build" or r.startswith("build/")):
                continue  # the build dir sits under the root; its files are not decomp files
            return r
        return None

    def is_overlay_path(self, path):
        p = os.path.normcase(os.path.abspath(str(path)))
        top = os.path.normcase(self.overlay_dir)
        return p.startswith(top + os.sep)

    # ---- trees
    def walk(self, subdir, exts=None):
        """Sorted [(rel, path)] for every file under <subdir>, raw and overlay-only, each path the
        one to read. `exts`: optional tuple of suffixes to keep."""
        subdir = norm_rel(subdir)
        found = {}
        top = _decomp_root.join(self.root, *subdir.split("/")) if subdir else self.root
        if os.path.isdir(top):
            for d, dirs, files in os.walk(top):
                dirs.sort()
                for f in files:
                    if exts and not f.endswith(tuple(exts)):
                        continue
                    rel = norm_rel(_decomp_root.relpath(os.path.join(d, f), self.root))
                    found[rel] = os.path.join(d, f)
        pre = subdir + "/" if subdir else ""
        for rel in self.listed:
            if not rel.startswith(pre):
                continue
            if exts and not rel.endswith(tuple(exts)):
                continue
            found[rel] = os.path.join(self.overlay_dir, *rel.split("/"))
        return sorted(found.items())

    def listdir(self, subdir):
        """Sorted [(rel, path)] for the FILES directly in <subdir> (not recursive), each path the
        one to read; overlay copies (and overlay-only files) included."""
        subdir = norm_rel(subdir)
        found = {}
        top = _decomp_root.join(self.root, *subdir.split("/")) if subdir else self.root
        if os.path.isdir(top):
            for f in os.listdir(top):
                p = os.path.join(top, f)
                if os.path.isfile(p):
                    found[(subdir + "/" + f) if subdir else f] = p
        depth = subdir.count("/") + 1 if subdir else 0
        for rel in self.listed:
            if (not subdir or rel.startswith(subdir + "/")) and rel.count("/") == depth:
                found[rel] = os.path.join(self.overlay_dir, *rel.split("/"))
        return sorted(found.items())

    def listed_under(self, subdir):
        pre = norm_rel(subdir) + "/"
        return sorted(r for r in self.listed if r.startswith(pre))

    def materialize(self, dest_root, subdirs):
        """Copy every overlay copy under `subdirs` over a copy of the tree at dest_root (a scratch
        copy a tool made of the decomp). Returns the rels written."""
        done = []
        for sub in subdirs:
            for rel in self.listed_under(sub):
                dst = os.path.join(str(dest_root), *rel.split("/"))
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.copyfile(self.path(rel), dst)
                done.append(rel)
        return done


def add_build_dir_arg(ap):
    """The one spelling every tool takes for the build dir whose overlay it reads."""
    ap.add_argument("--build-dir", default=None,
                    help="build dir whose decomp-overlay/ to read decomp files through "
                         "(default: build/port in the port's own tree)")
    return ap
