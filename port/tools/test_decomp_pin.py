"""Tests for battery.decomp_pin_check (port/DECOMP_PIN against decomp_patches/BASE and the
decomp checkout's HEAD). No repository is needed: the git lookup is passed in."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import battery  # noqa: E402

PIN = "03a6d6aea41"
FULL = PIN + "0" * 29


def port_dir(tmp_path, pin=PIN, base=PIN):
    d = tmp_path / "port"
    (d / "decomp_patches").mkdir(parents=True)
    if pin is not None:
        (d / "DECOMP_PIN").write_text(pin + "\n")
    if base is not None:
        (d / "decomp_patches" / "BASE").write_text(base + "\n")
    return str(d)


def clean(d, p):
    return []


def test_at_the_pin_is_ok(tmp_path):
    st, lines = battery.decomp_pin_check(port_dir(tmp_path), "D", lambda d: (FULL, FULL), clean)
    assert st == "ok", lines


def test_at_the_pin_with_local_changes_warns(tmp_path):
    st, lines = battery.decomp_pin_check(port_dir(tmp_path), "D", lambda d: (FULL, FULL),
                                         lambda d, p: ["tools/srcpath.py"])
    assert st == "warn" and "tools/srcpath.py" in lines[0], lines


def test_other_head_warns(tmp_path):
    st, lines = battery.decomp_pin_check(port_dir(tmp_path), "D", lambda d: ("f" * 40, FULL))
    assert st == "warn" and "WARNING" in lines[0] and PIN in lines[0]


def test_pin_unknown_to_the_checkout_warns(tmp_path):
    st, lines = battery.decomp_pin_check(port_dir(tmp_path), "D", lambda d: ("f" * 40, None))
    assert st == "warn"


def test_not_a_checkout_warns(tmp_path):
    st, lines = battery.decomp_pin_check(port_dir(tmp_path), "D", lambda d: (None, None))
    assert st == "warn" and "not a git checkout" in lines[0]


def test_base_differs_fails(tmp_path):
    st, lines = battery.decomp_pin_check(port_dir(tmp_path, base="0123456789a"), "D",
                                         lambda d: (FULL, FULL))
    assert st == "fail" and "BASE" in lines[0]


def test_missing_pin_fails(tmp_path):
    st, lines = battery.decomp_pin_check(port_dir(tmp_path, pin=None), "D", lambda d: (FULL, FULL))
    assert st == "fail"


def test_missing_base_fails(tmp_path):
    st, lines = battery.decomp_pin_check(port_dir(tmp_path, base=None), "D", lambda d: (FULL, FULL))
    assert st == "fail"


def test_the_tree_pin_matches_its_base():
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    st, lines = battery.decomp_pin_check(here, "D", lambda d: (FULL, FULL), clean)
    assert st == "ok", lines


def never(*a):
    raise AssertionError("the other rule's git lookup was called")


def test_in_tree_clean_is_ok(tmp_path):
    pd = port_dir(tmp_path)
    st, lines = battery.decomp_pin_check(pd, str(tmp_path), never, lambda d, p: [])
    assert st == "ok" and "in-tree" in lines[0], lines


def test_in_tree_ignores_head(tmp_path):
    # in-tree, HEAD is a port commit: only the content rule is asked
    pd = port_dir(tmp_path)
    st, lines = battery.decomp_pin_check(pd, str(tmp_path), never, lambda d, p: [])
    assert st == "ok"


def test_in_tree_changed_content_warns(tmp_path):
    pd = port_dir(tmp_path)
    st, lines = battery.decomp_pin_check(pd, str(tmp_path), never,
                                         lambda d, p: ["src/a.c", "include/b.h"])
    assert st == "warn" and "2 file(s)" in lines[0] and "src/a.c" in lines[0], lines


def test_in_tree_unknown_warns(tmp_path):
    pd = port_dir(tmp_path)
    st, lines = battery.decomp_pin_check(pd, str(tmp_path), never, lambda d, p: None)
    assert st == "warn" and "cannot compare" in lines[0], lines


def test_outside_tree_uses_head(tmp_path):
    pd = port_dir(tmp_path)
    other = tmp_path / "decomp"
    other.mkdir()
    st, lines = battery.decomp_pin_check(pd, str(other), lambda d: ("f" * 40, FULL), never)
    assert st == "warn" and "is at" in lines[0], lines


def test_decomp_tools_come_from_the_list(tmp_path):
    pd = port_dir(tmp_path)
    os.makedirs(os.path.join(pd, "tools"))
    with open(os.path.join(pd, "tools", "TOOLS_FROM_DECOMP.txt"), "w") as fh:
        fh.write("\n".join(["# comment", "srcpath.py", "", "relocs.py", ""]))
    assert battery.decomp_tools(pd) == ["tools/srcpath.py", "tools/relocs.py"]


def test_the_real_list_names_srcpath():
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    assert "tools/srcpath.py" in battery.decomp_tools(here)


def _repo(tmp_path):
    import subprocess
    r = tmp_path / "repo"
    for d in ("src", "include", "config", "tools"):
        (r / d).mkdir(parents=True)
    (r / "src" / "a.c").write_text("int a;")
    (r / "include" / "a.h").write_text("int a;")
    (r / "config" / "x.txt").write_text("x")
    (r / "tools" / "srcpath.py").write_text("x = 1")
    (r / "tools" / "other.py").write_text("x = 1")

    def git(*a):
        subprocess.run(["git", "-C", str(r), "-c", "user.name=t", "-c", "user.email=t@t"] + list(a),
                       check=True, capture_output=True)
    git("init", "-q")
    git("add", ".")
    git("commit", "-q", "-m", "pin")
    head = subprocess.run(["git", "-C", str(r), "rev-parse", "HEAD"], capture_output=True,
                          text=True).stdout.strip()
    return r, head


def test_git_changed_sees_untracked_src_and_tool_edits(tmp_path):
    r, pin = _repo(tmp_path)
    assert battery._git_changed(str(r), pin, ["tools/srcpath.py"]) == []
    (r / "src" / "new.c").write_text("int n;")
    (r / "include" / "new.h").write_text("int n;")
    (r / "tools" / "srcpath.py").write_text("x = 2")
    (r / "tools" / "other.py").write_text("x = 2")   # not a tool the port runs
    got = battery._git_changed(str(r), pin, ["tools/srcpath.py"])
    assert sorted(got) == ["include/new.h (untracked)", "src/new.c (untracked)",
                           "tools/srcpath.py"], got
