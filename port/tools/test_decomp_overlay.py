"""Tests for port/tools/decomp_overlay.py (the port's decomp patch layer).

Run: python -m pytest port/tools/test_decomp_overlay.py -v
"""

import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import decomp_overlay as dov  # noqa: E402
import decomp_root  # noqa: E402


# ---------------------------------------------------------------------------
# helpers

def write(root, rel, data):
    p = decomp_root.path(root) / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(data)
    return p


def read(root, rel):
    return (decomp_root.path(root) / rel).read_bytes()


def tree_hash(root):
    h = hashlib.sha256()
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames.sort()
        for name in sorted(filenames):
            p = Path(dirpath) / name
            h.update(str(p.relative_to(root)).replace("\\", "/").encode())
            h.update(b"\0")
            h.update(p.read_bytes())
            h.update(b"\0")
    return h.hexdigest()


class Layer:
    """A temp port dir (patches + overlay files), decomp root, edited root and overlay out dir."""

    def __init__(self, tmp):
        self.tmp = Path(tmp)
        self.port = self.tmp / "port"
        self.patches = self.port / "decomp_patches"
        self.files = self.port / "decomp_overlay_files"
        self.decomp = self.tmp / "decomp"
        self.edited = self.tmp / "edited"
        self.out = self.tmp / "overlay"
        for d in (self.patches, self.files, self.decomp, self.edited):
            d.mkdir(parents=True, exist_ok=True)

    def run(self, *args):
        argv = list(args) + ["--port-dir", str(self.port)]
        return dov.main(argv)

    def cut(self, paths):
        listing = self.tmp / "paths.txt"
        listing.write_text("\n".join(paths) + "\n")
        return self.run("cut", "--decomp-root", str(self.decomp),
                        "--edited-root", str(self.edited), "--paths", str(listing))

    def apply(self):
        return self.run("apply", "--decomp-root", str(self.decomp), "--out", str(self.out))

    def check(self):
        return self.run("check", "--decomp-root", str(self.decomp))

    def manifest(self):
        return (self.out / "overlay_manifest.txt").read_text().splitlines()


@pytest.fixture
def layer(tmp_path):
    return Layer(tmp_path)


BASE_LF = b"".join(b"line %d\n" % i for i in range(1, 21))


def edit_lf(data):
    return data.replace(b"line 10\n", b"line 10 edited\nextra\n")


# ---------------------------------------------------------------------------
# apply

def test_exact_apply_writes_patched_copy_and_manifest(layer):
    rel = "src/a/file.cpp"
    write(layer.decomp, rel, BASE_LF)
    write(layer.edited, rel, edit_lf(BASE_LF))
    assert layer.cut([rel]) == 0
    assert (layer.patches / (rel + ".patch")).is_file()

    assert layer.apply() == 0
    assert read(layer.out, rel) == edit_lf(BASE_LF)
    assert layer.manifest() == [rel]


def test_patch_uses_git_style_headers(layer):
    rel = "src/a/file.cpp"
    write(layer.decomp, rel, BASE_LF)
    write(layer.edited, rel, edit_lf(BASE_LF))
    layer.cut([rel])
    text = (layer.patches / (rel + ".patch")).read_bytes()
    assert text.startswith(b"diff --git a/src/a/file.cpp b/src/a/file.cpp\n"
                           b"--- a/src/a/file.cpp\n+++ b/src/a/file.cpp\n@@ ")


def test_context_shifted_by_one_line_fails_naming_file(layer, capsys):
    rel = "src/a/file.cpp"
    write(layer.decomp, rel, BASE_LF)
    write(layer.edited, rel, edit_lf(BASE_LF))
    layer.cut([rel])
    # the decomp moves on: one new line at the top shifts every hunk by one
    write(layer.decomp, rel, b"new first line\n" + BASE_LF)
    assert layer.apply() == 1
    err = capsys.readouterr().err
    assert rel in err
    assert "offset" in err
    assert not (layer.out / rel).exists()


def test_context_gone_fails(layer, capsys):
    rel = "src/a/file.cpp"
    write(layer.decomp, rel, BASE_LF)
    write(layer.edited, rel, edit_lf(BASE_LF))
    layer.cut([rel])
    write(layer.decomp, rel, BASE_LF.replace(b"line 9\n", b"line nine\n"))
    assert layer.apply() == 1
    assert rel in capsys.readouterr().err


def test_patch_target_missing_from_decomp_fails(layer, capsys):
    rel = "src/a/file.cpp"
    write(layer.decomp, rel, BASE_LF)
    write(layer.edited, rel, edit_lf(BASE_LF))
    layer.cut([rel])
    (layer.decomp / rel).unlink()
    assert layer.apply() == 1
    assert rel in capsys.readouterr().err


def test_one_bad_patch_fails_whole_apply_and_names_only_it(layer, capsys):
    good, bad = "src/good.cpp", "src/bad.cpp"
    for rel in (good, bad):
        write(layer.decomp, rel, BASE_LF)
        write(layer.edited, rel, edit_lf(BASE_LF))
    layer.cut([good, bad])
    write(layer.decomp, bad, b"x\n" + BASE_LF)
    assert layer.apply() == 1
    err = capsys.readouterr().err
    assert bad in err and good not in err


# ---------------------------------------------------------------------------
# cut -> apply round trips, byte for byte

ROUND_TRIP_CASES = {
    "lf": (BASE_LF, edit_lf(BASE_LF)),
    "crlf": (BASE_LF.replace(b"\n", b"\r\n"),
             edit_lf(BASE_LF).replace(b"\n", b"\r\n")),
    "mixed_eol": (BASE_LF.replace(b"line 5\n", b"line 5\r\n"),
                  edit_lf(BASE_LF).replace(b"line 5\n", b"line 5\r\n").replace(b"extra\n", b"extra\r\n")),
    "no_final_newline_both": (BASE_LF.rstrip(b"\n"), edit_lf(BASE_LF).rstrip(b"\n")),
    "final_newline_removed": (BASE_LF, BASE_LF.rstrip(b"\n")),
    "final_newline_added": (BASE_LF.rstrip(b"\n"), BASE_LF),
    "edit_last_line_no_newline": (BASE_LF.rstrip(b"\n"),
                                  BASE_LF.rstrip(b"\n") + b" changed"),
    "shift_jis_comment": (
        b"// \x83}\x83\x8a\x83I \x82\xcc\x8f\x88\x97\x9d\r\nint a;\r\n" + BASE_LF.replace(b"\n", b"\r\n"),
        b"// \x83}\x83\x8a\x83I \x82\xcc\x8f\x88\x97\x9d\r\nint a; // \x95\xcf\x8dX\r\n"
        + BASE_LF.replace(b"\n", b"\r\n").replace(b"line 15", b"line 15 host"),
    ),
    "invalid_utf8_and_lone_cr": (b"a\rb\nc\xff\xfe\n" + BASE_LF,
                                 b"a\rb\nc\xff\xfe\x80\n" + BASE_LF + b"tail\r"),
    "empty_to_content": (b"", b"hello\n"),
    "content_to_empty": (b"hello\n", b""),
    "edit_first_line": (BASE_LF, b"LINE 1\n" + BASE_LF.split(b"\n", 1)[1]),
    "many_hunks": (BASE_LF * 10,
                   (BASE_LF * 10).replace(b"line 3\n", b"line 3!\n").replace(b"line 17\n", b"")),
}


@pytest.mark.parametrize("case", sorted(ROUND_TRIP_CASES))
def test_cut_then_apply_round_trips(layer, case):
    before, after = ROUND_TRIP_CASES[case]
    rel = "src/case/%s.cpp" % case
    write(layer.decomp, rel, before)
    write(layer.edited, rel, after)
    assert layer.cut([rel]) == 0
    assert (layer.patches / (rel + ".patch")).is_file()
    assert layer.apply() == 0
    assert read(layer.out, rel) == after


@pytest.mark.parametrize("case", ["lf", "crlf", "no_final_newline_both", "final_newline_removed",
                                  "shift_jis_comment", "many_hunks"])
def test_cut_patches_are_accepted_by_git_apply(layer, case, tmp_path):
    """The patches are ordinary unified diffs in git's stored (LF) form: git apply on the
    canonical form of the file gives the canonical form of the edit."""
    if shutil.which("git") is None:
        pytest.skip("git not on PATH")
    before, after = ROUND_TRIP_CASES[case]
    rel = "src/case/%s.cpp" % case
    write(layer.decomp, rel, before)
    write(layer.edited, rel, after)
    layer.cut([rel])
    work = tmp_path / "gitwork"
    write(work, rel, dov.canonical(before))
    patch = layer.patches / (rel + ".patch")
    env = dict(os.environ, GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull)
    r = subprocess.run(["git", "apply", "--check", "-v", str(patch)], cwd=work,
                       capture_output=True, env=env)
    assert r.returncode == 0, r.stderr
    r = subprocess.run(["git", "apply", str(patch)], cwd=work, capture_output=True, env=env)
    assert r.returncode == 0, r.stderr
    assert read(work, rel) == dov.canonical(after)


def test_reads_git_generated_patch(layer, tmp_path):
    """A patch written by git diff --no-index from LF files applies through the tool, to a CRLF
    decomp file too (giving a CRLF result)."""
    if shutil.which("git") is None:
        pytest.skip("git not on PATH")
    rel = "src/g.cpp"
    before = BASE_LF
    after = edit_lf(BASE_LF).rstrip(b"\n")
    wa, wb = tmp_path / "ga", tmp_path / "gb"
    write(wa, "a/" + rel, before)
    write(wb, "b/" + rel, after)
    env = dict(os.environ, GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull)
    # run from a dir that holds both sides as a/ and b/
    both = tmp_path / "both"
    shutil.copytree(wa / "a", both / "a")
    shutil.copytree(wb / "b", both / "b")
    r = subprocess.run(["git", "diff", "--no-index", "--no-color", "--no-prefix", "a/" + rel, "b/" + rel],
                       cwd=both, capture_output=True, env=env)
    assert r.returncode == 1, r.stderr
    write(layer.patches, rel + ".patch", r.stdout)
    write(layer.decomp, rel, before)
    assert layer.apply() == 0
    assert read(layer.out, rel) == after
    write(layer.decomp, rel, before.replace(b"\n", b"\r\n"))
    assert layer.apply() == 0
    assert read(layer.out, rel) == after.replace(b"\n", b"\r\n")


# ---------------------------------------------------------------------------
# line endings: one patch for a CRLF checkout and an LF checkout

def crlf(data):
    return data.replace(b"\n", b"\r\n")


@pytest.mark.parametrize("case", ["lf", "no_final_newline_both", "final_newline_removed",
                                  "final_newline_added", "edit_last_line_no_newline",
                                  "edit_first_line", "many_hunks", "empty_to_content",
                                  "content_to_empty"])
@pytest.mark.parametrize("cut_on,apply_on", [("crlf", "lf"), ("lf", "crlf"),
                                             ("crlf", "crlf"), ("lf", "lf")])
def test_patch_is_portable_between_checkouts(layer, case, cut_on, apply_on):
    before_lf, after_lf = ROUND_TRIP_CASES[case]
    conv = {"lf": lambda d: d, "crlf": crlf}
    rel = "src/case/%s.cpp" % case
    write(layer.decomp, rel, conv[cut_on](before_lf))
    write(layer.edited, rel, conv[cut_on](after_lf))
    assert layer.cut([rel]) == 0
    patch = layer.patches / (rel + ".patch")
    assert b"\r" not in patch.read_bytes()  # stored in git's LF form
    write(layer.decomp, rel, conv[apply_on](before_lf))
    # a newline-free decomp file takes the tree's convention; give the tree one sample file
    write(layer.decomp, "include/sample.h", conv[apply_on](b"a\nb\n"))
    assert layer.apply() == 0
    assert read(layer.out, rel) == conv[apply_on](after_lf)


def test_crlf_and_lf_cuts_give_the_same_patch(layer):
    rel = "src/a.cpp"
    write(layer.decomp, rel, crlf(BASE_LF))
    write(layer.edited, rel, crlf(edit_lf(BASE_LF)))
    layer.cut([rel])
    from_crlf = (layer.patches / (rel + ".patch")).read_bytes()
    write(layer.decomp, rel, BASE_LF)
    write(layer.edited, rel, edit_lf(BASE_LF))
    layer.cut([rel])
    assert (layer.patches / (rel + ".patch")).read_bytes() == from_crlf


def test_mixed_eol_file_is_patched_byte_exact(layer):
    rel = "include/mixed.h"
    before = b"a\r\nb\nc\r\n" + BASE_LF
    after = b"a\r\nb changed\nc\r\nnew\r\n" + BASE_LF
    write(layer.decomp, rel, before)
    write(layer.edited, rel, after)
    write(layer.decomp, "include/sample.h", b"x\r\n")  # a CRLF tree around it
    assert layer.cut([rel]) == 0
    assert b"\r\n" in (layer.patches / (rel + ".patch")).read_bytes()  # cut byte exact
    assert layer.apply() == 0
    assert read(layer.out, rel) == after


def test_uniform_decomp_file_edited_into_mixed_keeps_edit_bytes(layer):
    """git stores a mixed file as is, so an edit that adds CRLF lines to an LF-stored file is kept raw."""
    rel = "include/m.h"
    after_canon = BASE_LF.replace(b"line 4\n", b"line 4\r\n")
    for tree in ("lf", "crlf"):
        conv = crlf if tree == "crlf" else (lambda d: d)
        write(layer.decomp, rel, conv(BASE_LF))
        write(layer.edited, rel, after_canon)
        assert layer.cut([rel]) == 0
        assert layer.apply() == 0
        assert read(layer.out, rel) == after_canon


@pytest.mark.parametrize("tree", ["lf", "crlf"])
def test_overlay_file_takes_tree_convention(layer, tree):
    rel = "src/port_only/x.cpp"
    data_lf = b"// port only\nint x;\n"
    write(layer.edited, rel, crlf(data_lf))  # cut on a CRLF checkout
    assert layer.cut([rel]) == 0
    assert read(layer.files, rel) == data_lf  # stored LF
    write(layer.decomp, "include/sample.h", b"x\r\n" if tree == "crlf" else b"x\n")
    assert layer.apply() == 0
    assert read(layer.out, rel) == (crlf(data_lf) if tree == "crlf" else data_lf)


def test_eol_flag_overrides_detection(layer):
    rel = "src/port_only/x.cpp"
    write(layer.edited, rel, b"a\nb\n")
    layer.cut([rel])
    write(layer.decomp, "include/sample.h", b"x\n")
    assert layer.run("apply", "--decomp-root", str(layer.decomp), "--out", str(layer.out),
                     "--eol", "crlf") == 0
    assert read(layer.out, rel) == b"a\r\nb\r\n"


def test_mixed_overlay_file_kept_byte_exact(layer):
    rel = "src/port_only/m.cpp"
    data = b"a\r\nb\nc"
    write(layer.edited, rel, data)
    layer.cut([rel])
    assert read(layer.files, rel) == data
    write(layer.decomp, "include/sample.h", b"x\r\n")
    assert layer.apply() == 0
    assert read(layer.out, rel) == data


# ---------------------------------------------------------------------------
# overlay files, removals, stale cleanup

def test_overlay_file_copied_verbatim(layer):
    rel = "src/port_only/new_file.cpp"
    data = b"// port only\r\nint x;\xff\x00\nno newline"
    write(layer.edited, rel, data)
    assert layer.cut([rel]) == 0
    assert read(layer.files, rel) == data
    assert not (layer.patches / (rel + ".patch")).exists()
    assert layer.apply() == 0
    assert read(layer.out, rel) == data
    assert layer.manifest() == [rel]


def test_manifest_sorted_and_covers_patches_and_files(layer):
    write(layer.decomp, "src/z.cpp", BASE_LF)
    write(layer.edited, "src/z.cpp", edit_lf(BASE_LF))
    write(layer.decomp, "include/a.h", BASE_LF)
    write(layer.edited, "include/a.h", edit_lf(BASE_LF))
    write(layer.edited, "src/m_new.cpp", b"new\n")
    layer.cut(["src/z.cpp", "include/a.h", "src/m_new.cpp"])
    assert layer.apply() == 0
    assert layer.manifest() == ["include/a.h", "src/m_new.cpp", "src/z.cpp"]


def test_identical_edited_file_removes_its_patch(layer):
    rel = "src/a.cpp"
    write(layer.decomp, rel, BASE_LF)
    write(layer.edited, rel, edit_lf(BASE_LF))
    layer.cut([rel])
    assert (layer.patches / (rel + ".patch")).exists()
    write(layer.edited, rel, BASE_LF)
    assert layer.cut([rel]) == 0
    assert not (layer.patches / (rel + ".patch")).exists()


def test_cut_moves_between_patch_and_overlay_file(layer):
    rel = "src/a.cpp"
    write(layer.edited, rel, b"port only\n")
    layer.cut([rel])
    assert (layer.files / rel).exists()
    # the decomp grows the file: the next cut turns it into a patch
    write(layer.decomp, rel, b"decomp\n")
    layer.cut([rel])
    assert not (layer.files / rel).exists()
    assert (layer.patches / (rel + ".patch")).exists()
    assert layer.apply() == 0
    assert read(layer.out, rel) == b"port only\n"


def test_cut_path_missing_from_edited_root_fails(layer, capsys):
    write(layer.decomp, "src/a.cpp", BASE_LF)
    assert layer.cut(["src/a.cpp"]) == 1
    assert "src/a.cpp" in capsys.readouterr().err


def test_removed_patch_output_is_cleaned_on_next_apply(layer):
    keep, drop = "src/keep.cpp", "src/sub/drop.cpp"
    for rel in (keep, drop):
        write(layer.decomp, rel, BASE_LF)
        write(layer.edited, rel, edit_lf(BASE_LF))
    layer.cut([keep, drop])
    assert layer.apply() == 0
    assert (layer.out / drop).exists()
    unrelated = write(layer.out, "src/sub/not_ours.txt", b"leave me")
    (layer.patches / (drop + ".patch")).unlink()
    assert layer.apply() == 0
    assert not (layer.out / drop).exists()
    assert unrelated.read_bytes() == b"leave me"
    assert (layer.out / keep).exists()
    assert layer.manifest() == [keep]


def test_apply_is_idempotent(layer):
    rel = "src/a.cpp"
    write(layer.decomp, rel, BASE_LF)
    write(layer.edited, rel, edit_lf(BASE_LF))
    write(layer.edited, "src/b.cpp", b"b\n")
    layer.cut([rel, "src/b.cpp"])
    assert layer.apply() == 0
    first = tree_hash(layer.out)
    mtime = (layer.out / rel).stat().st_mtime_ns
    assert layer.apply() == 0
    assert tree_hash(layer.out) == first
    # unchanged outputs are not rewritten (keeps incremental builds quiet)
    assert (layer.out / rel).stat().st_mtime_ns == mtime


def test_decomp_root_never_modified(layer):
    for i in range(3):
        rel = "src/f%d.cpp" % i
        write(layer.decomp, rel, BASE_LF)
        write(layer.edited, rel, edit_lf(BASE_LF))
    write(layer.edited, "src/new.cpp", b"n\n")
    before = tree_hash(layer.decomp)
    layer.cut(["src/f0.cpp", "src/f1.cpp", "src/f2.cpp", "src/new.cpp"])
    assert layer.check() == 0
    assert layer.apply() == 0
    # a failing apply must not touch it either
    write(layer.decomp, "src/f1.cpp", b"shift\n" + BASE_LF)
    before = tree_hash(layer.decomp)
    assert layer.apply() == 1
    assert layer.check() == 1
    assert tree_hash(layer.decomp) == before


def test_apply_refuses_out_inside_decomp_root(layer, capsys):
    rel = "src/a.cpp"
    write(layer.decomp, rel, BASE_LF)
    write(layer.edited, rel, edit_lf(BASE_LF))
    layer.cut([rel])
    before = tree_hash(layer.decomp)
    rc = layer.run("apply", "--decomp-root", str(layer.decomp), "--out", str(layer.decomp))
    assert rc == 1
    assert tree_hash(layer.decomp) == before


@pytest.mark.parametrize("inside", ["src", "src/a", "include"])
def test_apply_refuses_out_inside_a_mirrored_decomp_dir(layer, inside):
    rel = "src/a.cpp"
    write(layer.decomp, rel, BASE_LF)
    write(layer.edited, rel, edit_lf(BASE_LF))
    layer.cut([rel])
    before = tree_hash(layer.decomp)
    rc = layer.run("apply", "--decomp-root", str(layer.decomp), "--out", str(layer.decomp / inside))
    assert rc == 1
    assert tree_hash(layer.decomp) == before


def test_apply_into_a_build_dir_under_the_checkout(layer):
    # the port's build puts the overlay at <checkout>/build/<dir>/decomp-overlay
    rel = "src/a.cpp"
    write(layer.decomp, rel, BASE_LF)
    write(layer.edited, rel, edit_lf(BASE_LF))
    layer.cut([rel])
    out = layer.decomp / "build" / "port" / "decomp-overlay"
    assert layer.run("apply", "--decomp-root", str(layer.decomp), "--out", str(out)) == 0
    assert read(out, rel) == edit_lf(BASE_LF)
    assert read(layer.decomp, rel) == BASE_LF


def test_check_writes_nothing(layer, capsys):
    rel = "src/a.cpp"
    write(layer.decomp, rel, BASE_LF)
    write(layer.edited, rel, edit_lf(BASE_LF))
    layer.cut([rel])
    assert layer.check() == 0
    assert not layer.out.exists()
    write(layer.decomp, rel, b"x\n" + BASE_LF)
    assert layer.check() == 1
    assert rel in capsys.readouterr().err
    assert not layer.out.exists()


def test_failed_apply_leaves_previous_overlay_alone(layer):
    rel = "src/a.cpp"
    write(layer.decomp, rel, BASE_LF)
    write(layer.edited, rel, edit_lf(BASE_LF))
    layer.cut([rel])
    assert layer.apply() == 0
    snap = tree_hash(layer.out)
    write(layer.decomp, rel, b"x\n" + BASE_LF)
    assert layer.apply() == 1
    assert tree_hash(layer.out) == snap


def test_overlay_file_shadowing_a_decomp_file_fails(layer, capsys):
    rel = "src/a.cpp"
    write(layer.edited, rel, b"port only\n")
    layer.cut([rel])
    write(layer.decomp, rel, b"now main has it\n")
    assert layer.apply() == 1
    assert rel in capsys.readouterr().err


def test_cut_marks_layer_dirs_binary_for_git(layer):
    """core.autocrlf would rewrite CRLF bytes inside committed patches; -text stops it."""
    write(layer.decomp, "src/a.cpp", BASE_LF)
    write(layer.edited, "src/a.cpp", edit_lf(BASE_LF))
    write(layer.edited, "src/b.cpp", b"b\n")
    layer.cut(["src/a.cpp", "src/b.cpp"])
    for d in (layer.patches, layer.files):
        assert b"-text" in (d / ".gitattributes").read_bytes()
    assert layer.apply() == 0
    assert ".gitattributes" not in layer.manifest()


def test_cut_writes_base_when_given(layer):
    write(layer.decomp, "src/a.cpp", BASE_LF)
    write(layer.edited, "src/a.cpp", edit_lf(BASE_LF))
    listing = layer.tmp / "p.txt"
    listing.write_text("src/a.cpp\n")
    rc = layer.run("cut", "--decomp-root", str(layer.decomp), "--edited-root", str(layer.edited),
                   "--paths", str(listing), "--base", "03a6d6aea41")
    assert rc == 0
    assert (layer.patches / "BASE").read_text() == "03a6d6aea41\n"


def test_long_paths(layer):
    rel = "src/" + "/".join(["d%02d_long_directory_name" % i for i in range(12)]) + "/file.cpp"
    assert len(str(layer.out / rel)) > 300
    write_long = dov.long_path
    for root, data in ((layer.decomp, BASE_LF), (layer.edited, edit_lf(BASE_LF))):
        p = Path(write_long(root / rel))
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
    assert layer.cut([rel]) == 0
    assert layer.apply() == 0
    assert Path(write_long(layer.out / rel)).read_bytes() == edit_lf(BASE_LF)


# ---------------------------------------------------------------------------
# review fixes (REV-OVL1)

LINES30 = [b"line %02d\n" % i for i in range(1, 31)]


def rename_case(root, old_rel, new_rel):
    a, b = decomp_root.path(root) / old_rel, decomp_root.path(root) / new_rel
    tmp = a.with_name(a.name + ".tmp")
    os.rename(a, tmp)
    os.rename(tmp, b)


def test_case_only_rename_keeps_the_new_output(layer):
    """Fix 1: Foo.h -> foo.h between two applies must not delete the file just written."""
    write(layer.decomp, "include/Foo.h", b"".join(LINES30))
    write(layer.edited, "include/Foo.h", b"".join(LINES30[:2] + [b"HOOK\n"] + LINES30[2:]))
    assert layer.cut(["include/Foo.h"]) == 0
    assert layer.apply() == 0
    rename_case(layer.decomp, "include/Foo.h", "include/foo.h")
    rename_case(layer.edited, "include/Foo.h", "include/foo.h")
    (layer.patches / "include/Foo.h.patch").unlink()
    assert layer.cut(["include/foo.h"]) == 0
    assert layer.apply() == 0
    assert layer.manifest() == ["include/foo.h"]
    assert os.listdir(layer.out / "include") == ["foo.h"]  # present, and with the new case
    assert read(layer.out, "include/foo.h") == b"".join(LINES30[:2] + [b"HOOK\n"] + LINES30[2:])


def _patch(rel, body):
    r = rel.encode()
    return b"diff --git a/%s b/%s\n--- a/%s\n+++ b/%s\n" % (r, r, r, r) + body


def test_zero_context_hunk_is_refused(layer, capsys):
    """Fix 2: a -U0 hunk verifies nothing around it and would land wherever its line number points."""
    rel = "src/b.cpp"
    write(layer.decomp, rel, b"".join(LINES30))
    write(layer.patches, rel + ".patch", _patch(rel, b"@@ -10,0 +11 @@\n+HOOK();\n"))
    assert layer.apply() == 1
    err = capsys.readouterr().err
    assert rel in err and "context" in err


@pytest.mark.parametrize("lead,trail", [(2, 3), (3, 2), (1, 1)])
def test_short_context_away_from_file_edges_is_refused(layer, capsys, lead, trail):
    rel = "src/s.cpp"
    write(layer.decomp, rel, b"".join(LINES30))
    # replace line 15 with HOOK, keeping `lead` lines before and `trail` lines after
    start = 15 - lead
    body = [b" " + ln for ln in LINES30[start - 1:14]] + [b"-" + LINES30[14], b"+HOOK\n"] + \
           [b" " + ln for ln in LINES30[15:15 + trail]]
    n = lead + 1 + trail
    hdr = b"@@ -%d,%d +%d,%d @@\n" % (start, n, start, n)
    write(layer.patches, rel + ".patch", _patch(rel, hdr + b"".join(body)))
    assert layer.apply() == 1
    assert rel in capsys.readouterr().err


def test_short_context_at_file_edges_is_allowed(layer):
    rel = "src/e.cpp"
    write(layer.decomp, rel, b"".join(LINES30))
    # first line changed (1 trailing context is NOT allowed: needs 3) -> use 3 trailing; 0 leading at line 1
    first = b"@@ -1,4 +1,4 @@\n-line 01\n+LINE 01\n line 02\n line 03\n line 04\n"
    # last line changed: 3 leading, 0 trailing at end of file
    last = b"@@ -27,4 +27,4 @@\n line 27\n line 28\n line 29\n-line 30\n+LINE 30\n"
    write(layer.patches, rel + ".patch", _patch(rel, first + last))
    assert layer.apply() == 0
    want = b"".join([b"LINE 01\n"] + LINES30[1:29] + [b"LINE 30\n"])
    assert read(layer.out, rel) == want


def test_cut_patches_always_satisfy_the_context_rule(layer):
    """Edits next to the edges and in the middle: everything cut must apply."""
    rel = "src/c.cpp"
    before = b"".join(LINES30)
    after = before.replace(b"line 02\n", b"X\n").replace(b"line 15\n", b"").replace(
        b"line 29\n", b"line 29\nY\n")
    write(layer.decomp, rel, before)
    write(layer.edited, rel, after)
    assert layer.cut([rel]) == 0
    assert layer.apply() == 0
    assert read(layer.out, rel) == after


def test_new_file_patch_for_existing_decomp_file_is_refused(layer, capsys):
    """Fix 2: --- /dev/null against a file the decomp has would prepend to it."""
    rel = "src/c.cpp"
    write(layer.decomp, rel, b"".join(LINES30))
    write(layer.patches, rel + ".patch",
          b"diff --git a/src/c.cpp b/src/c.cpp\nnew file mode 100644\n--- /dev/null\n"
          b"+++ b/src/c.cpp\n@@ -0,0 +1,2 @@\n+int a;\n+int b;\n")
    assert layer.apply() == 1
    err = capsys.readouterr().err
    assert rel in err and "/dev/null" in err


def test_patch_path_case_must_match_decomp(layer, capsys):
    """Fix 3: include/bar.h.patch must not apply to include/Bar.h (it would not on Linux)."""
    write(layer.decomp, "include/Bar.h", b"".join(LINES30))
    write(layer.edited, "include/Bar.h", b"".join(LINES30[:2] + [b"HOOK\n"] + LINES30[2:]))
    layer.cut(["include/Bar.h"])
    pb = (layer.patches / "include/Bar.h.patch").read_bytes().replace(b"include/Bar.h", b"include/bar.h")
    (layer.patches / "include/Bar.h.patch").unlink()
    write(layer.patches, "include/bar.h.patch", pb)
    assert layer.apply() == 1
    err = capsys.readouterr().err
    assert "include/bar.h" in err and "case" in err and "include/Bar.h" in err


def test_patch_dir_case_must_match_decomp(layer, capsys):
    write(layer.decomp, "src/Game/x.cpp", b"".join(LINES30))
    write(layer.edited, "src/Game/x.cpp", b"".join(LINES30[:2] + [b"HOOK\n"] + LINES30[2:]))
    layer.cut(["src/Game/x.cpp"])
    data = (layer.patches / "src/Game/x.cpp.patch").read_bytes().replace(b"src/Game/", b"src/game/")
    shutil.rmtree(layer.patches / "src")
    write(layer.patches, "src/game/x.cpp.patch", data)
    assert layer.apply() == 1
    assert "case" in capsys.readouterr().err


def test_overlay_file_case_variant_of_decomp_file_is_refused(layer, capsys):
    write(layer.edited, "include/new.h", b"x\n")
    layer.cut(["include/new.h"])
    write(layer.decomp, "include/New.h", b"decomp\n")
    assert layer.apply() == 1
    err = capsys.readouterr().err
    assert "include/new.h" in err and "case" in err


def test_patch_and_overlay_file_differing_only_in_case_is_refused(layer, capsys):
    write(layer.decomp, "include/A.h", b"".join(LINES30))
    write(layer.edited, "include/A.h", b"".join(LINES30[:2] + [b"HOOK\n"] + LINES30[2:]))
    layer.cut(["include/A.h"])
    write(layer.files, "include/a.h", b"other\n")
    assert layer.apply() == 1
    assert "include/a.h" in capsys.readouterr().err


def test_cut_refuses_case_mismatch(layer, capsys):
    write(layer.decomp, "include/C.h", b"".join(LINES30))
    write(layer.edited, "include/C.h", b"".join(LINES30[:2] + [b"HOOK\n"] + LINES30[2:]))
    assert layer.cut(["include/c.h"]) == 1
    assert "case" in capsys.readouterr().err
    assert not (layer.patches / "include/c.h.patch").exists()
    assert not (layer.files / "include/c.h").exists()


def test_crash_between_outputs_and_manifest_is_recoverable(layer, monkeypatch):
    """Fix 4: outputs written before a kill must be in a manifest so the next apply can clean them."""
    write(layer.decomp, "src/old.cpp", BASE_LF)
    write(layer.edited, "src/old.cpp", edit_lf(BASE_LF))
    layer.cut(["src/old.cpp"])
    assert layer.apply() == 0
    write(layer.decomp, "src/new.cpp", BASE_LF)
    write(layer.edited, "src/new.cpp", edit_lf(BASE_LF))
    layer.cut(["src/new.cpp"])

    real = dov.write_if_changed

    class Killed(Exception):
        pass

    def dying(path, data):
        if path.endswith("overlay_manifest.txt") and os.path.isfile(dov.join(layer.out, "src/new.cpp")):
            raise Killed()
        return real(path, data)

    monkeypatch.setattr(dov, "write_if_changed", dying)
    with pytest.raises(Killed):
        layer.apply()
    monkeypatch.setattr(dov, "write_if_changed", real)
    assert (layer.out / "src/new.cpp").exists()
    assert "src/new.cpp" in layer.manifest()  # the union manifest was written first
    # the patch goes away before the next apply: the orphan must still be cleaned
    (layer.patches / "src/new.cpp.patch").unlink()
    assert layer.apply() == 0
    assert not (layer.out / "src/new.cpp").exists()
    assert layer.manifest() == ["src/old.cpp"]


# ---------------------------------------------------------------------------
# include closure (apply --include-closure include)

def apply_closure(layer):
    return layer.run("apply", "--decomp-root", str(layer.decomp), "--out", str(layer.out),
                     "--include-closure", "include")


def hdr(*includes):
    return b"".join(b'#include "%s"\n' % i.encode() for i in includes) + BASE_LF


def test_closure_pulls_raw_includers_of_a_patched_header(layer):
    write(layer.decomp, "include/types.h", BASE_LF)
    write(layer.edited, "include/types.h", edit_lf(BASE_LF))
    write(layer.decomp, "include/a.h", hdr("types.h"))          # direct includer
    write(layer.decomp, "include/b.h", hdr("a.h"))              # includer of an includer
    write(layer.decomp, "include/math/m.h", hdr("../types.h"))  # sibling-relative from a subdir
    write(layer.decomp, "include/c.h", hdr("unrelated.h"))
    write(layer.decomp, "include/unrelated.h", BASE_LF)
    write(layer.decomp, "include/d.h", b'// #include "types.h"\n' + BASE_LF)  # commented out
    assert layer.cut(["include/types.h"]) == 0
    assert apply_closure(layer) == 0
    assert layer.manifest() == ["include/a.h", "include/b.h", "include/math/m.h", "include/types.h"]
    for rel in ("include/a.h", "include/b.h", "include/math/m.h"):
        assert read(layer.out, rel) == read(layer.decomp, rel)  # pulled copies are verbatim
    assert read(layer.out, "include/types.h") == edit_lf(BASE_LF)


def test_closure_pulls_subdir_siblings_of_an_overlay_header(layer):
    write(layer.decomp, "include/math/X.h", hdr("Matrix.h"))
    write(layer.edited, "include/math/X.h", edit_lf(hdr("Matrix.h")))
    write(layer.decomp, "include/math/Matrix.h", BASE_LF)
    write(layer.decomp, "include/Matrix.h", BASE_LF)  # the -I path would find this one instead
    assert layer.cut(["include/math/X.h"]) == 0
    assert apply_closure(layer) == 0
    assert layer.manifest() == ["include/math/Matrix.h", "include/math/X.h"]


def test_closure_ignores_angle_includes_and_src(layer):
    write(layer.decomp, "include/types.h", BASE_LF)
    write(layer.edited, "include/types.h", edit_lf(BASE_LF))
    write(layer.decomp, "include/a.h", b"#include <types.h>\n" + BASE_LF)
    write(layer.decomp, "src/x.cpp", hdr("types.h"))  # resolved through -I, never a sibling
    assert layer.cut(["include/types.h"]) == 0
    assert apply_closure(layer) == 0
    assert layer.manifest() == ["include/types.h"]


def test_closure_with_no_layer_writes_an_empty_manifest(layer):
    write(layer.decomp, "include/a.h", hdr("b.h"))
    write(layer.decomp, "include/b.h", BASE_LF)
    assert apply_closure(layer) == 0
    assert layer.manifest() == []


def test_closure_pulls_are_removed_when_no_longer_needed(layer):
    write(layer.decomp, "include/types.h", BASE_LF)
    write(layer.edited, "include/types.h", edit_lf(BASE_LF))
    write(layer.decomp, "include/a.h", hdr("types.h"))
    assert layer.cut(["include/types.h"]) == 0
    assert apply_closure(layer) == 0
    assert (layer.out / "include/a.h").is_file()
    (layer.patches / "include/types.h.patch").unlink()
    assert apply_closure(layer) == 0
    assert layer.manifest() == []
    assert not (layer.out / "include/a.h").exists()


def test_closure_follows_the_includer_chain_folders(layer):
    # MSVC tries the folders of the files that included a header before any -I path, so a raw
    # include/math/Matrix.h doing #include "types.h" opened from a raw include/ header finds the raw
    # include/types.h: Matrix.h (and whoever includes it) must come into the overlay too.
    write(layer.decomp, "include/types.h", BASE_LF)
    write(layer.edited, "include/types.h", edit_lf(BASE_LF))
    write(layer.decomp, "include/math/Matrix.h", hdr("types.h"))      # not a sibling: parent folder
    write(layer.decomp, "include/math/deep/V.h", hdr("../../types.h"))  # sibling-relative, two up
    write(layer.decomp, "include/math/deep/W.h", hdr("math/Matrix.h"))  # reached through the root
    write(layer.decomp, "include/a.h", hdr("math/Matrix.h"))
    write(layer.decomp, "include/b.h", hdr("other.h"))
    write(layer.decomp, "include/other.h", BASE_LF)
    assert layer.cut(["include/types.h"]) == 0
    assert apply_closure(layer) == 0
    assert layer.manifest() == ["include/a.h", "include/math/Matrix.h", "include/math/deep/V.h",
                                "include/math/deep/W.h", "include/types.h"]


def test_closure_matches_names_without_regard_to_case(layer):
    write(layer.decomp, "include/types.h", BASE_LF)
    write(layer.edited, "include/types.h", edit_lf(BASE_LF))
    write(layer.decomp, "include/a.h", hdr("Types.H"))
    assert layer.cut(["include/types.h"]) == 0
    assert apply_closure(layer) == 0
    assert layer.manifest() == ["include/a.h", "include/types.h"]
