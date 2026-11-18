"""Tests for port/tools/decomp_root.py: the default decomp root (the port tree's decomp/ folder, else
the parent of port/), root_arg() reading "." and None as the configured root, and path()/join()
keeping the port's own paths in the port's tree. Runs on any Python 3: path() subclasses the
concrete Path class."""
import os
import pathlib
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import decomp_root  # noqa: E402


def fake_tree(tmp_path, monkeypatch, with_decomp):
    ptree = tmp_path / "ptree"
    (ptree / "port" / "tools").mkdir(parents=True)
    if with_decomp:
        for d in ("src", "include"):
            (ptree / "decomp" / d).mkdir(parents=True)
    monkeypatch.setattr(decomp_root, "PORT_TREE", str(ptree))
    monkeypatch.setattr(decomp_root, "PORT_DIR", str(ptree / "port"))
    monkeypatch.delenv(decomp_root.ENV, raising=False)
    decomp_root._folded.clear()
    return ptree


def test_default_is_the_parent_without_decomp_folder(tmp_path, monkeypatch):
    ptree = fake_tree(tmp_path, monkeypatch, False)
    assert decomp_root.find() == str(ptree)


def test_default_is_the_decomp_folder_when_it_has_src_and_include(tmp_path, monkeypatch):
    ptree = fake_tree(tmp_path, monkeypatch, True)
    assert decomp_root.find() == os.path.join(str(ptree), "decomp")


def test_decomp_folder_without_include_is_not_used(tmp_path, monkeypatch):
    ptree = fake_tree(tmp_path, monkeypatch, False)
    (ptree / "decomp" / "src").mkdir(parents=True)
    assert decomp_root.find() == str(ptree)


def test_env_wins_over_the_decomp_folder(tmp_path, monkeypatch):
    fake_tree(tmp_path, monkeypatch, True)
    other = tmp_path / "other"
    other.mkdir()
    monkeypatch.setenv(decomp_root.ENV, str(other))
    assert decomp_root.find() == str(other)


def test_root_arg_dot_and_none_are_the_configured_root(tmp_path, monkeypatch):
    ptree = fake_tree(tmp_path, monkeypatch, True)
    want = os.path.join(str(ptree), "decomp")
    assert decomp_root.root_arg(None) == want
    assert decomp_root.root_arg(".") == want
    assert decomp_root.root_arg("") == want
    assert decomp_root.root_arg(str(tmp_path)) == str(tmp_path)


def test_port_paths_come_from_the_port_tree(tmp_path, monkeypatch):
    ptree = fake_tree(tmp_path, monkeypatch, True)
    droot = decomp_root.find()
    assert decomp_root.join(droot, "src", "a.c") == os.path.join(droot, "src", "a.c")
    assert decomp_root.join(droot, "build", "port") == os.path.join(str(ptree), "build", "port")
    p = decomp_root.path(droot)
    assert isinstance(p, pathlib.Path)
    assert p / "port" / "hal" == pathlib.Path(ptree) / "port" / "hal"
    assert p / "src" == pathlib.Path(droot) / "src"
    assert p.joinpath("runs", "x") == pathlib.Path(ptree) / "runs" / "x"
    assert decomp_root.relpath(os.path.join(str(ptree), "port", "x.cpp"), droot) == os.path.join("port", "x.cpp")


def test_other_roots_are_plain(tmp_path, monkeypatch):
    fake_tree(tmp_path, monkeypatch, True)
    other = str(tmp_path / "fixture")
    assert decomp_root.join(other, "build") == os.path.join(other, "build")
    assert decomp_root.path(other) / "port" == pathlib.Path(other) / "port"


def test_extracted_dir_in_tree_layout_is_the_asset_roots_own(tmp_path, monkeypatch):
    ptree = fake_tree(tmp_path, monkeypatch, False)
    (ptree / "extracted").mkdir()
    assert decomp_root.extracted_dir() == os.path.join(str(ptree), "extracted")
    assert decomp_root.extracted_dir(shipping=True) == os.path.join(str(ptree), "extracted")


def test_extracted_dir_public_layout_is_the_decomp_submodules(tmp_path, monkeypatch):
    ptree = fake_tree(tmp_path, monkeypatch, True)
    (ptree / "decomp" / "extracted").mkdir()
    want = os.path.join(str(ptree), "decomp", "extracted")
    assert decomp_root.extracted_dir() == want
    # the shipping build has no embedded decomp root, only the relative decomp/ step
    assert decomp_root.extracted_dir(str(ptree), shipping=True) == want


def test_extracted_dir_developer_build_follows_an_external_decomp_root(tmp_path, monkeypatch):
    ptree = fake_tree(tmp_path, monkeypatch, False)
    other = tmp_path / "other"
    (other / "extracted").mkdir(parents=True)
    monkeypatch.setenv(decomp_root.ENV, str(other))
    assert decomp_root.extracted_dir() == os.path.join(str(other), "extracted")
    # a shipping exe cannot see it; a missing dump is named under the asset root
    assert decomp_root.extracted_dir(shipping=True) == os.path.join(str(ptree), "extracted")


def test_extracted_dir_asset_roots_own_wins(tmp_path, monkeypatch):
    ptree = fake_tree(tmp_path, monkeypatch, True)
    (ptree / "decomp" / "extracted").mkdir()
    kit = tmp_path / "kit"
    (kit / "extracted").mkdir(parents=True)
    assert decomp_root.extracted_dir(str(kit)) == os.path.join(str(kit), "extracted")
