"""Where the decomp checkout the port builds against lives, and where the port's own tree is.

The port compiles the decomp's src/ and include/ (through the patch layer, port/decomp_patches) and
reads its config/, tools/ and extracted/ ROM dump. Those used to be found as "the directory above
port/", which only works while the port sits inside a decomp checkout. Every port tool now asks this
module instead, so the decomp can live anywhere:

    import decomp_root
    root = decomp_root.find()            # the decomp checkout: src/, include/, config/, tools/, extracted/
    port = decomp_root.PORT_DIR          # this port/ directory
    tree = decomp_root.PORT_TREE         # the directory holding port/: build/ and runs/ live here

find() answers, in order:
  1. the value given to it (a tool's --decomp-root option; add_argument() below adds it),
  2. the environment variable SM64DS_DECOMP_ROOT,
  3. default(): the port tree's decomp/ folder when it holds src/ and include/ (the public repo:
     port/ beside a decomp/ submodule), else the parent of port/ (the in-tree layout).
A tool's root argument goes through root_arg(), which reads a missing value or "." as find().

CMake has the same rule: the cache variable DECOMP_ROOT (default: the parent of port/), exported to
every configure-time and build-time tool as SM64DS_DECOMP_ROOT. port/build-port.cmd and
port/build-port-trace.cmd take it from SM64DS_DECOMP_ROOT too.

Tools written for the in-tree layout take one root and join it with decomp paths and port paths
alike ("src/x.c", "port/hal/x.cpp", "build/port/x.map"). join(root, ...), relpath(path, root) and
path(root) (a pathlib.Path) keep those right when the root is an external decomp: port/, build/
and runs/ come from PORT_TREE. For any other root they are plain os.path / pathlib.

PORT_TREE is never the decomp. Paths under it are the port's own: build/<dir> (build output,
including each build's decomp-overlay/), build/assets (the asset catalog), runs/. A path that names
decomp content (src, include, config, tools/<decomp script>, extracted) is joined onto find(), never
onto PORT_TREE; port/tools/decomp_root_check.py enforces that.
"""
import os

ENV = "SM64DS_DECOMP_ROOT"

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PORT_DIR = os.path.dirname(TOOLS_DIR)
PORT_TREE = os.path.dirname(PORT_DIR)


def default():
    """The decomp root when nothing names one: the port tree's decomp/ folder (the public repo
    layout: port/ beside a decomp/ submodule) when it holds src/ and include/, else the parent of
    port/ (the in-tree layout, where port/ sits inside a decomp checkout)."""
    sub = os.path.join(PORT_TREE, "decomp")
    if os.path.isdir(os.path.join(sub, "src")) and os.path.isdir(os.path.join(sub, "include")):
        return sub
    return PORT_TREE


def find(given=None):
    """The decomp root: `given` if set, else $SM64DS_DECOMP_ROOT, else default()."""
    if given:
        return os.path.abspath(given)
    env = os.environ.get(ENV)
    if env:
        return os.path.abspath(env)
    return default()


def root_arg(value=None):
    """A tool's root argument: None, "" and "." mean the configured decomp root (find()), not the
    current directory; anything else is taken as given. Port tools are run from the port's tree,
    where "." is the decomp only in the in-tree layout."""
    if value is None or str(value).strip() in ("", "."):
        return find()
    return os.path.abspath(str(value))


def add_argument(parser):
    """Add --decomp-root to an argparse parser; pass the parsed value to find()."""
    parser.add_argument("--decomp-root", default=None,
                        help="decomp checkout to read (default: $%s, else the port tree's decomp/ "
                             "folder, else the parent of port/)" % ENV)
    return parser


def is_default():
    """True when the decomp is the directory above port/ (the old, in-tree layout)."""
    return os.path.normcase(os.path.realpath(find())) == os.path.normcase(os.path.realpath(PORT_TREE))


_PORT_OWNED = ("port", "build", "runs")
_folded = {}


def _fold(path):
    p = str(path)
    f = _folded.get(p)
    if f is None:
        f = _folded[p] = os.path.normcase(os.path.realpath(p))
    return f


def join(root, *parts):
    """os.path.join(root, *parts), except that a path under port/, build/ or runs/ is taken from
    PORT_TREE when `root` is the configured decomp root.

    Many tools were written for the in-tree layout and take one root that holds both the decomp
    (src/, include/, config/) and the port (port/, build/). Given the decomp root, they join it with
    "port/hal/x.cpp" as readily as with "src/x.cpp". This keeps both right when the decomp lives
    outside the port's tree: decomp paths stay on the decomp, the port's own paths go to its tree.
    A root that is not the configured decomp (a test fixture tree, the port tree itself) is joined
    exactly as os.path.join would."""
    rel = os.path.join(*[str(p) for p in parts]) if parts else ""
    if _redirects(root, rel):
        return os.path.join(PORT_TREE, rel)
    return os.path.join(str(root), rel) if parts else str(root)


def _redirects(root, rel):
    head = str(rel).replace("\\", "/").split("/", 1)[0]
    return (head in _PORT_OWNED and _fold(root) == _fold(find())
            and _fold(root) != _fold(PORT_TREE))


def relpath(path, root):
    """os.path.relpath(path, root), the inverse of join(): a path in the port's tree (under its
    port/, build/ or runs/) is named relative to PORT_TREE when `root` is the configured decomp
    root, so it reads "port/hal/x.cpp" as it did in the in-tree layout."""
    if _fold(root) == _fold(find()) and _fold(root) != _fold(PORT_TREE):
        p = _fold(path)
        tree = _fold(PORT_TREE)
        for head in _PORT_OWNED:
            if p == os.path.join(tree, head) or p.startswith(os.path.join(tree, head) + os.sep):
                return os.path.relpath(str(path), PORT_TREE)
    return os.path.relpath(str(path), str(root))


import pathlib as _pathlib


class _RootPath(type(_pathlib.Path())):
    """A pathlib.Path whose / and joinpath() follow join(): from the configured decomp root,
    port/, build/ and runs/ lead into the port's tree. It subclasses the concrete class
    (WindowsPath or PosixPath), which every Python 3 version allows."""

    def __truediv__(self, key):
        if _redirects(self, key):
            return _pathlib.Path(PORT_TREE) / key
        return super().__truediv__(key)

    def joinpath(self, *parts):
        if parts and _redirects(self, os.path.join(*[str(p) for p in parts])):
            return _pathlib.Path(PORT_TREE).joinpath(*parts)
        return super().joinpath(*parts)


def path(root):
    """pathlib.Path(root) for a root that may be the decomp root: joining it with port/, build/ or
    runs/ (root / "port" / ...) leads into the port's tree, as join() does."""
    return _RootPath(str(root))


def extracted_dir(asset_root=None, shipping=False):
    """The extracted/ folder (the unpacked cartridge) a game run with this asset root reads: the
    rule of port/hal/extracted_root.h, for the tools that check or read what the game will open.

    asset_root is SM64DS_ASSET_ROOT as the run gets it (default: PORT_TREE, the developer build's
    fallback). First match: <asset_root>/extracted (the in-tree layout, every packaged kit), then
    for a developer build the decomp root's extracted/ (find()), then <asset_root>/decomp/extracted
    (the public layout; the only step a shipping, ROM-clean build has, since it embeds no path),
    else <asset_root>/extracted, so a missing dump is reported where it always was."""
    root = str(asset_root) if asset_root else PORT_TREE
    own = os.path.join(root, "extracted")
    if os.path.isdir(own):
        return own
    if not shipping:
        dec = os.path.join(find(), "extracted")
        if os.path.isdir(dec):
            return dec
    sub = os.path.join(root, "decomp", "extracted")
    if os.path.isdir(sub):
        return sub
    return own


def cache_root(build_dir):
    """The decomp root a configured build directory was configured with (its CMakeCache.txt
    DECOMP_ROOT entry, absolute), or None when it has no cache or the cache predates DECOMP_ROOT."""
    cache = os.path.join(str(build_dir), "CMakeCache.txt")
    try:
        with open(cache, encoding="utf-8", errors="replace") as fh:
            for line in fh:
                if line.startswith("DECOMP_ROOT:"):
                    return os.path.abspath(line.split("=", 1)[1].strip())
    except OSError:
        return None
    return None


def cache_matches(build_dir):
    """True when build_dir was configured against the decomp root in use now. A cache without the
    entry was configured before DECOMP_ROOT existed, that is against the parent of port/."""
    got = cache_root(build_dir)
    if got is None:
        got = PORT_TREE  # a cache from before DECOMP_ROOT: built against the parent of port/
    return _fold(got) == _fold(find())


def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(description="Print the decomp root in use, or check a build dir "
                                 "against it.")
    add_argument(ap)
    ap.add_argument("--check-cache", metavar="BUILD_DIR",
                    help="exit 1 when BUILD_DIR's CMakeCache.txt names another decomp root "
                         "(port/build-port.cmd reconfigures then)")
    a = ap.parse_args(argv)
    root = find(a.decomp_root)
    if a.check_cache:
        if a.decomp_root:
            os.environ[ENV] = root
        if cache_matches(a.check_cache):
            print("decomp_root: %s is configured against %s" % (a.check_cache, root))
            return 0
        print("decomp_root: %s is configured against %s, not %s: reconfiguring"
              % (a.check_cache, cache_root(a.check_cache) or PORT_TREE, root))
        return 1
    print(root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
