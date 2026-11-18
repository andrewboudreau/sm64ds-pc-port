"""Tests for port/tools/decomp_root_check.py's explicit-root exemption: a line that takes a decomp
tree as a named argument may carry the explicit-root comment (drc.EXEMPT plus a reason), which
waives only the four root-name rules, needs a reason, and is itself a hit on a line none of those
rules hits. The fixture source is assembled from pieces so this file does not trip the check."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import decomp_root_check as drc  # noqa: E402

MARK = "  # " + drc.EXEMPT  # the comment, without its ": <reason>"
DOT = "'" + "." + "'"


def scan(tmp_path, body):
    tools = tmp_path / "tools"
    tools.mkdir(exist_ok=True)
    f = tools / "x.py"
    f.write_text(body, encoding="utf-8")
    return [(no, rule) for _, no, rule, _ in drc.scan_file(str(f), "tools/x.py", "python")]


def test_unmarked_root_path_is_a_hit(tmp_path):
    body = "import pathlib\n" + "root = pathlib." + "Path(arg)\n"
    assert scan(tmp_path, body) == [(2, "py-root-path-assign")]


def test_marker_with_reason_waives_root_rules(tmp_path):
    why = MARK + ": the --pin checkout\n"
    body = ("import os, pathlib\n"
            + "root = pathlib." + "Path(arg)" + why
            + "x = pathlib." + "Path(" + "root) / 'config'" + why
            + "y = os.path." + "join(root, 'src')" + why)
    assert scan(tmp_path, body) == []


def test_marker_without_reason_is_a_hit(tmp_path):
    for tail in (MARK + ":\n", MARK + "\n"):
        body = "import pathlib\n" + "root = pathlib." + "Path(arg)" + tail
        assert scan(tmp_path, body) == [(2, "py-root-path-assign")]


def test_marker_without_reason_on_a_clean_line(tmp_path):
    assert scan(tmp_path, "x = 1" + MARK + "\n") == [(1, "exempt-no-reason")]


def test_marker_on_a_clean_line_is_stray(tmp_path):
    assert scan(tmp_path, "x = 1" + MARK + ": nothing to waive\n") == [(1, "stray-exempt")]


def test_marker_does_not_waive_other_rules(tmp_path):
    why = MARK + ": not a root rule\n"
    body = "import os\n" + "p = os.path.join(os.path.dirname(__file__), '..'" + ", '..')" + why
    assert "py-dotdot-join" in [r for _, r in scan(tmp_path, body)]
    body = "import argparse\n" + "ap.add_argument('--root', default" + "=" + DOT + ")" + why
    assert "py-cwd-root" in [r for _, r in scan(tmp_path, body)]


def rules(tmp_path, body):
    return [r for _, r in scan(tmp_path, body)]


def scan_cmd(tmp_path, body):
    f = tmp_path / "x.cmd"
    f.write_text(body, encoding="utf-8")
    return [rule for _, _, rule, _ in drc.scan_file(str(f), "x.cmd", "cmd")]


CURDIR = "os." + "curdir"


def test_curdir_default_is_a_hit(tmp_path):
    body = "import os, sys\n" + "r = sys.argv[1] if sys.argv[1:] else " + CURDIR + "\n"
    assert rules(tmp_path, body) == ["py-cwd-root"]


def test_root_or_dot_is_a_hit(tmp_path):
    assert rules(tmp_path, "import os\n" + "root = os.environ.get('R') " + "or " + DOT + "\n") \
        == ["py-or-cwd"]
    assert rules(tmp_path, "import os\n" + "f(a.root " + "or " + CURDIR + ")\n") == ["py-or-cwd"]


def test_output_dir_or_dot_is_not_a_root(tmp_path):
    body = "import os\n" + "os.makedirs(os.path.dirname(out) " + "or " + DOT + ")\n"
    assert rules(tmp_path, body) == []


def test_root_dot_default_argument_is_a_hit(tmp_path):
    assert rules(tmp_path, "def f(repo_root" + "=" + DOT + "):\n    pass\n") == ["py-root-dot-default"]
    assert rules(tmp_path, "def f(root" + "=" + "'..'" + "):\n    pass\n") == ["py-root-dot-default"]
    assert rules(tmp_path, "def f(root=None):\n    pass\n") == []


def test_decomp_root_used_without_import(tmp_path):
    body = "def f():\n    return decomp_root" + ".find()\n"
    assert rules(tmp_path, body) == ["py-root-no-import"]
    # imported inside the function, or passed in, binds the name
    assert rules(tmp_path, "def f():\n    import decomp_root\n    return decomp_root" + ".find()\n") == []
    assert rules(tmp_path, "def f(decomp_root):\n    return decomp_root" + ".find()\n") == []


def test_from_import_used_before_it(tmp_path):
    body = "R = find()\n" + "from decomp_root import find\n"
    assert rules(tmp_path, body) == ["py-root-before-import"]
    assert rules(tmp_path, "from decomp_root import find\n" + "R = find()\n") == []


def test_cmd_dp0_with_separator_is_a_hit(tmp_path):
    sep = chr(92)
    assert scan_cmd(tmp_path, "set R=%~dp0" + sep + ".." + sep + "src\n") == ["cmd-parent"]
    assert scan_cmd(tmp_path, "set B=%~dp0" + sep + ".." + sep + "build\n") == []


def test_cmd_current_directory_is_a_hit(tmp_path):
    assert scan_cmd(tmp_path, "set R=%" + "CD%\n") == ["cmd-cwd"]
