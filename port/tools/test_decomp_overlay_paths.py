"""Tests for port/tools/decomp_overlay_paths.py (the shared overlay lookup every port tool uses).

Run: python -m pytest port/tools/test_decomp_overlay_paths.py -v
"""

import os
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import decomp_overlay as dov  # noqa: E402
import decomp_overlay_paths as dop  # noqa: E402
import decomp_root  # noqa: E402


def write(root, rel, data):
    p = decomp_root.path(root) / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(data)
    return p


@pytest.fixture
def tree(tmp_path):
    root = tmp_path / "decomp"
    write(root, "src/a.cpp", b"raw a\n")
    write(root, "src/sub/b.c", b"raw b\n")
    write(root, "include/h.h", b"raw h\n")
    (root / "port").mkdir()
    build = root / "build" / "port"
    return root, build


def make_overlay(build, files):
    ov = build / "decomp-overlay"
    for rel, data in files.items():
        write(ov, rel, data)
    write(ov, "overlay_manifest.txt", "".join(r + "\n" for r in sorted(files)).encode())
    return ov


def test_listed_path_reads_overlay_copy(tree):
    root, build = tree
    ov = make_overlay(build, {"src/a.cpp": b"patched a\n"})
    v = dop.DecompView(root, build)
    assert v.has("src/a.cpp")
    assert Path(v.path("src/a.cpp")) == ov / "src" / "a.cpp"
    assert v.read_text("src/a.cpp") == "patched a\n"
    assert v.has(".\\src\\a.cpp")  # spelling does not matter


def test_unlisted_path_reads_raw(tree):
    root, build = tree
    make_overlay(build, {"src/a.cpp": b"patched a\n"})
    v = dop.DecompView(root, build)
    assert not v.has("src/sub/b.c")
    assert Path(v.path("src/sub/b.c")) == root / "src" / "sub" / "b.c"
    assert v.read_text("src/sub/b.c") == "raw b\n"


def test_unlisted_file_in_overlay_dir_is_ignored(tree):
    root, build = tree
    ov = make_overlay(build, {"src/a.cpp": b"patched a\n"})
    write(ov, "include/h.h", b"stray\n")  # not in the manifest
    v = dop.DecompView(root, build)
    assert v.read_text("include/h.h") == "raw h\n"


def test_missing_manifest_is_raw_only(tree):
    root, build = tree
    write(build / "decomp-overlay", "src/a.cpp", b"no manifest\n")
    v = dop.DecompView(root, build)
    assert not v.has_manifest
    assert v.read_text("src/a.cpp") == "raw a\n"
    assert [r for r, _ in v.walk("src")] == ["src/a.cpp", "src/sub/b.c"]


def test_missing_manifest_with_nonempty_layer_refuses(tree):
    root, build = tree
    write(root, "port/decomp_patches/src/a.cpp.patch", b"--- a/src/a.cpp\n")
    with pytest.raises(dop.OverlayMissing) as e:
        dop.DecompView(root, build)
    assert "src/a.cpp.patch" in str(e.value)
    # opting out (a tool deliberately reading raw) still answers raw
    assert dop.DecompView(root, build, require_if_layer=False).read_text("src/a.cpp") == "raw a\n"


def test_layer_bookkeeping_files_do_not_count(tree):
    root, build = tree
    write(root, "port/decomp_patches/.gitattributes", b"* -text\n")
    write(root, "port/decomp_patches/README.md", b"x\n")
    write(root, "port/decomp_patches/BASE", b"abc\n")
    write(root, "port/decomp_overlay_files/.gitattributes", b"* -text\n")
    assert dop.DecompView(root, build).read_text("src/a.cpp") == "raw a\n"


def test_default_build_dir_is_build_port(tree):
    root, build = tree
    make_overlay(build, {"src/a.cpp": b"patched a\n"})
    assert dop.DecompView(root).read_text("src/a.cpp") == "patched a\n"


def test_walk_overlays_copies_and_adds_overlay_only_files(tree):
    root, build = tree
    ov = make_overlay(build, {"src/a.cpp": b"patched a\n", "src/new.cpp": b"only here\n",
                              "include/h.h": b"patched h\n"})
    v = dop.DecompView(root, build)
    got = dict(v.walk("src", (".cpp",)))
    assert sorted(got) == ["src/a.cpp", "src/new.cpp"]
    assert Path(got["src/a.cpp"]) == ov / "src" / "a.cpp"
    assert Path(got["src/new.cpp"]) == ov / "src" / "new.cpp"
    assert [r for r, _ in v.walk("src")] == ["src/a.cpp", "src/new.cpp", "src/sub/b.c"]


def test_listdir_is_one_level_with_overlay_copies(tree):
    root, build = tree
    ov = make_overlay(build, {"src/a.cpp": b"patched a\n", "src/top_new.c": b"new\n",
                              "src/sub/deep.c": b"deep\n"})
    v = dop.DecompView(root, build)
    got = dict(v.listdir("src"))
    assert sorted(got) == ["src/a.cpp", "src/top_new.c"]
    assert Path(got["src/a.cpp"]) == ov / "src" / "a.cpp"


def test_rel_of_maps_both_raw_and_overlay_paths(tree):
    root, build = tree
    ov = make_overlay(build, {"src/a.cpp": b"patched a\n"})
    v = dop.DecompView(root, build)
    assert v.rel_of(ov / "src" / "a.cpp") == "src/a.cpp"
    assert v.rel_of(root / "src" / "sub" / "b.c") == "src/sub/b.c"
    assert v.rel_of(build / "obj.obj") is None
    assert v.is_overlay_path(ov / "src" / "a.cpp")
    assert not v.is_overlay_path(root / "src" / "a.cpp")


def test_materialize_copies_listed_files_over_scratch(tree, tmp_path):
    root, build = tree
    make_overlay(build, {"src/a.cpp": b"patched a\n", "include/h.h": b"patched h\n"})
    scratch = tmp_path / "scratch"
    write(scratch, "src/a.cpp", b"raw a\n")
    v = dop.DecompView(root, build)
    assert v.materialize(scratch, ["src"]) == ["src/a.cpp"]
    assert (scratch / "src" / "a.cpp").read_bytes() == b"patched a\n"
    assert not (scratch / "include" / "h.h").exists()


def test_agrees_with_real_apply(tree):
    """The manifest decomp_overlay.py apply writes is the one the lookup reads."""
    root, build = tree
    port = root / "port"
    (port / "decomp_patches").mkdir(parents=True)
    edited = root.parent / "edited"
    write(edited, "src/a.cpp", b"edited a\n")
    listing = root.parent / "listing.txt"
    listing.write_text("src/a.cpp\n")
    assert dov.main(["cut", "--port-dir", str(port), "--decomp-root", str(root),
                     "--edited-root", str(edited), "--paths", str(listing)]) == 0
    with pytest.raises(dop.OverlayMissing):
        dop.DecompView(root, build)
    out = build / "decomp-overlay"
    assert dov.main(["apply", "--port-dir", str(port), "--decomp-root", str(root),
                     "--out", str(out)]) == 0
    v = dop.DecompView(root, build)
    assert v.read_text("src/a.cpp") == "edited a\n"
    assert v.read_text("src/sub/b.c") == "raw b\n"
