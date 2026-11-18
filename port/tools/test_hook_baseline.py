"""Tests for port/tools/hook_baseline.py (catch-up plan Task 4.4 step 2: the hook check in the battery).

Run: python -m pytest port/tools/test_hook_baseline.py -v

A scratch git repo holds the old decomp (commit "pin") and the old port's patch layer and decomp_edits.tsv
(commit "stage0"). record() packs them; check() then runs against a bumped decomp root and re-cut patches:
the hook kept in its function is green, the canary (the hook moved into the neighbouring function) is red,
the canary with an allow row is green, and a baseline whose tarball was edited is refused.
"""

import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import decomp_overlay as dov  # noqa: E402
import hook_baseline as hb  # noqa: E402

C_FILE = """#include "types.h"

void Foo(int a)
{
    a += 1;
    a += 2;
    a += 3;
}

void Bar(int b)
{
    b += 1;
    b += 2;
    b += 3;
}
"""
HOOK = "#ifdef _MSC_VER\n    port_hook();\n#endif\n"
SYMS = "Foo kind:function(arm,size=0x10) addr:0x02100000\nBar kind:function(arm,size=0x10) addr:0x02100040\n"
REL = "src/foo.c"


def write(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data.encode("utf-8") if isinstance(data, str) else data)


def after(text, marker, add):
    k = text.index(marker) + len(marker)
    return text[:k] + add + text[k:]


def git(repo, *args):
    subprocess.run(["git", "-C", str(repo)] + list(args), check=True, capture_output=True)


def make_repo(tmp):
    repo = tmp / "repo"
    repo.mkdir()
    git(repo, "init", "-q")
    git(repo, "config", "user.email", "t@example.invalid")
    git(repo, "config", "user.name", "t")
    git(repo, "config", "core.autocrlf", "false")
    write(repo / REL, C_FILE)
    write(repo / "config/arm9/overlays/ov002/symbols.txt", SYMS)
    git(repo, "add", "-A")
    git(repo, "commit", "-q", "-m", "pin")
    git(repo, "tag", "pin")
    write(repo / "port/decomp_patches" / (REL + ".patch"),
          dov.make_patch(REL, C_FILE.encode(), after(C_FILE, "    a += 2;\n", HOOK).encode()))
    write(repo / "port/decomp_edits.tsv", "path\tstatus\tkind\tside\tlines_added\tlines_removed\tnote\n"
          "%s\tM\tHOOK\tplain\t3\t0\t\n" % REL)
    git(repo, "add", "-A")
    git(repo, "commit", "-q", "-m", "stage0")
    git(repo, "tag", "stage0")
    maps = tmp / "maps"
    write(maps / "renames.tsv", "#module\taddr\told_name\tnew_name\n")
    write(maps / "renames_kind.tsv", "#module\taddr\told_name\tnew_name\ttags\n")
    write(maps / "patchmap.tsv", "#path\tstatus\tnew_paths\tunowned\n")
    return repo, maps


def bumped(tmp, marker):
    """A bumped decomp (main added lines above Foo) and a re-cut patch with the hook after `marker`."""
    root = tmp / ("after_" + marker.strip().replace(" ", "").replace("+=", "_").replace(";", ""))
    src = C_FILE.replace('#include "types.h"\n', '#include "types.h"\n#include "other.h"\n\nint g;\n')
    write(root / "decomp" / REL, src)
    write(root / "decomp/config/arm9/overlays/ov002/symbols.txt", SYMS)
    write(root / "patches" / (REL + ".patch"), dov.make_patch(REL, src.encode(), after(src, marker, HOOK).encode()))
    return root / "decomp", root / "patches"


def test_record_and_check(tmp_path):
    repo, maps = make_repo(tmp_path)
    base = tmp_path / "baseline"
    assert hb.record(repo, "pin", "stage0", maps, base) == 0
    m = hb.read_manifest(base)
    assert m["hook_patches"] == "1" and m["patches"] == "1" and m["before_files"] == "1"
    cache = tmp_path / "cache"

    decomp, patches = bumped(tmp_path, "    a += 2;\n")
    assert hb.check(base, decomp, patches, cache, tmp_path / "r_same.tsv") == 0

    # the canary: the re-cut hook sits in Bar, not Foo
    decomp, patches = bumped(tmp_path, "    b += 2;\n")
    assert hb.check(base, decomp, patches, cache, tmp_path / "r_moved.tsv") == 1
    rows = [l for l in (tmp_path / "r_moved.tsv").read_text().splitlines() if l.startswith("MOVED\t")]
    assert rows and REL in rows[0]

    # the same move, listed as intentional, is green
    (base / "allow.tsv").write_text("%s\t1\tcanary: moved on purpose\n" % REL, encoding="utf-8")
    assert hb.check(base, decomp, patches, cache, tmp_path / "r_listed.tsv") == 0


def test_record_is_deterministic(tmp_path):
    repo, maps = make_repo(tmp_path)
    hb.record(repo, "pin", "stage0", maps, tmp_path / "a")
    hb.record(repo, "pin", "stage0", maps, tmp_path / "b")
    assert (tmp_path / "a" / hb.TARBALL).read_bytes() == (tmp_path / "b" / hb.TARBALL).read_bytes()


def test_edited_tarball_is_refused(tmp_path):
    repo, maps = make_repo(tmp_path)
    base = tmp_path / "baseline"
    hb.record(repo, "pin", "stage0", maps, base)
    data = bytearray((base / hb.TARBALL).read_bytes())
    data[-1] ^= 1
    (base / hb.TARBALL).write_bytes(bytes(data))
    decomp, patches = bumped(tmp_path, "    a += 2;\n")
    try:
        hb.check(base, decomp, patches, tmp_path / "cache", tmp_path / "r.tsv")
    except SystemExit as e:
        assert "sha256" in str(e)
    else:
        raise AssertionError("an edited baseline was accepted")


def test_dropped_hook_patch_is_counted(tmp_path):
    """A HOOK patch missing from the report's count fails even when nothing else does."""
    repo, maps = make_repo(tmp_path)
    base = tmp_path / "baseline"
    hb.record(repo, "pin", "stage0", maps, base)
    man = (base / "MANIFEST.txt").read_text().replace("hook_patches\t1", "hook_patches\t2")
    (base / "MANIFEST.txt").write_text(man)
    decomp, patches = bumped(tmp_path, "    a += 2;\n")
    assert hb.check(base, decomp, patches, tmp_path / "cache", tmp_path / "r.tsv") == 1


# ----------------------------------------------------------------------------- the plain half

EXTRA_HOOK = "#ifdef _MSC_VER\n    port_extra_hook();\n#endif\n"
REL2 = "src/baz.c"


def make_split_repo(tmp):
    """foo.c is mixed (a plain hook in Foo, a custom hook in Bar), baz.c is custom (one hook)."""
    repo = tmp / "repo"
    repo.mkdir()
    git(repo, "init", "-q")
    git(repo, "config", "user.email", "t@example.invalid")
    git(repo, "config", "user.name", "t")
    git(repo, "config", "core.autocrlf", "false")
    write(repo / REL, C_FILE)
    write(repo / REL2, C_FILE)
    write(repo / "config/arm9/overlays/ov002/symbols.txt", SYMS)
    git(repo, "add", "-A")
    git(repo, "commit", "-q", "-m", "pin")
    git(repo, "tag", "pin")
    full = after(after(C_FILE, "    a += 2;\n", HOOK), "    b += 2;\n", EXTRA_HOOK)
    patch = dov.make_patch(REL, C_FILE.encode(), full.encode())
    write(repo / "port/decomp_patches" / (REL + ".patch"), patch)
    write(repo / "port/decomp_patches" / (REL2 + ".patch"),
          dov.make_patch(REL2, C_FILE.encode(), after(C_FILE, "    b += 2;\n", EXTRA_HOOK).encode()))
    write(repo / "port/decomp_edits.tsv", "path\tstatus\tkind\tside\tlines_added\tlines_removed\tnote\n"
          "%s\tM\tHOOK\tmixed\t6\t0\tcustom units: extra\n%s\tM\tHOOK\tcustom\t3\t0\tcustom: extra\n" % (REL, REL2))
    git(repo, "add", "-A")
    git(repo, "commit", "-q", "-m", "stage0")
    git(repo, "tag", "stage0")
    maps = tmp / "maps"
    write(maps / "renames.tsv", "#module\taddr\told_name\tnew_name\n")
    write(maps / "renames_kind.tsv", "#module\taddr\told_name\tnew_name\ttags\n")
    write(maps / "patchmap.tsv", "#path\tstatus\tnew_paths\tunowned\n")
    # the custom hook's lines in the patch file (1-based), as the frozen table gives them
    plines = (patch.decode() if isinstance(patch, bytes) else patch).splitlines()
    k = next(i for i, l in enumerate(plines) if "port_extra_hook" in l)
    frozen = tmp / "frozen.tsv"
    write(frozen, "# frozen\npath\tside\treason\trom_data\n"
          "port/decomp_patches/%s.patch\tmixed\tcustom units: +1..3=patch L%d-%d\tno\n"
          "port/decomp_patches/%s.patch\tcustom\textra\tno\n"
          "port/decomp_patches/src/other.c.patch\tplain\tplain\tno\n" % (REL, k, k + 2, REL2))
    return repo, maps, frozen


def bumped_split(tmp, name, with_custom):
    """The bumped decomp and its re-cut patches: foo.c's plain hook (and its custom hook when with_custom)."""
    root = tmp / name
    src = C_FILE.replace('#include "types.h"\n', '#include "types.h"\n#include "other.h"\n\nint g;\n')
    for rel in (REL, REL2):
        write(root / "decomp" / rel, src)
    write(root / "decomp/config/arm9/overlays/ov002/symbols.txt", SYMS)
    foo = after(src, "    a += 2;\n", HOOK)
    if with_custom:
        foo = after(foo, "    b += 2;\n", EXTRA_HOOK)
    write(root / "patches" / (REL + ".patch"), dov.make_patch(REL, src.encode(), foo.encode()))
    return root / "decomp", root / "patches"


def test_cut_patch_renumbers_and_applies(tmp_path):
    """The plain cut of a mixed patch is the patch made from the plain-only edit, byte for byte."""
    full = after(after(C_FILE, "    a += 2;\n", HOOK), "    b += 2;\n", EXTRA_HOOK)
    patch = dov.make_patch(REL, C_FILE.encode(), full.encode())
    patch = patch if isinstance(patch, bytes) else patch.encode()
    lines = patch.decode().splitlines()
    k = next(i for i, l in enumerate(lines) if "port_extra_hook" in l)
    cut = hb.cut_patch(patch, set(range(k, k + 3)))
    want = dov.make_patch(REL, C_FILE.encode(), after(C_FILE, "    a += 2;\n", HOOK).encode())
    want = want if isinstance(want, bytes) else want.encode()
    assert b"port_extra_hook" not in cut
    assert cut == want


def test_record_frozen_writes_the_plain_half(tmp_path):
    repo, maps, frozen = make_split_repo(tmp_path)
    base = tmp_path / "baseline"
    assert hb.record(repo, "pin", "stage0", maps, base, frozen) == 0
    m = hb.read_manifest(base)
    assert (m["patches"], m["hook_patches"], m["cut_patches"]) == ("1", "1", "1")
    assert not (base / hb.CUSTOM_TARBALL).exists()
    with __import__("tarfile").open(base / hb.TARBALL, "r:gz") as tar:
        names = tar.getnames()
        blob = b"".join(tar.extractfile(n).read() for n in names if tar.getmember(n).isfile())
    assert b"port_extra_hook" not in blob and "patches/%s.patch" % REL2 not in names

    # the plain re-cut tree: green, 1 HOOK patch covered
    decomp_p, patches_p = bumped_split(tmp_path, "plain", False)
    assert hb.check(base, decomp_p, patches_p, tmp_path / "cache", tmp_path / "r_plain.tsv") == 0
    # a tree that still carries the custom hook: it has no before counterpart in the plain half, red
    decomp, patches = bumped_split(tmp_path, "full", True)
    assert hb.check(base, decomp, patches, tmp_path / "cache", tmp_path / "r_extra.tsv") == 1
