"""Tests for port/tools/decomp_src.py and the two source forms port/tools/tail2_cells.py reads.

Run: python -m pytest port/tools/test_decomp_src.py -v

A scratch checkout carries a stub tools/srcpath.py (owner_at by address range, path_for by name), so the
resolution order is tested without depending on where today's decomp keeps any file.
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import decomp_src  # noqa: E402
import tail2_cells  # noqa: E402

STUB = '''
import pathlib
BASE = None
RANGES = {"ov002": [(0x02107000, 0x02108000, "src/game/actors/d_a_demo.cpp")],
          "arm9": [(0x0203d7b8, 0x0203f000, "src/engine/link/unit0203d7b8.cpp")]}
NAMES = {"HeapAllocator_Destroy": "src/engine/heap/Heap.cpp"}
def set_root(repo):
    global BASE
    BASE = pathlib.Path(str(repo))
def owner_at(module, addr):
    for lo, hi, rel in RANGES.get(module, ()):
        if lo <= addr < hi and (BASE / rel).is_file():
            return BASE / rel
    return None
def path_for(symbol):
    rel = NAMES.get(symbol)
    return BASE / rel if rel else None
'''


def make_root(tmp):
    root = tmp / "decomp"
    for rel in ("tools/srcpath.py", "src/game/actors/d_a_demo.cpp", "src/engine/link/unit0203d7b8.cpp",
                "src/engine/heap/Heap.cpp", "src/still/here.c"):
        p = root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(STUB if rel.endswith("srcpath.py") else "// %s\n" % rel, encoding="utf-8")
    return root


def test_existing_path_is_itself(tmp_path):
    root = make_root(tmp_path)
    assert decomp_src.resolve(root, "src/still/here.c") == "src/still/here.c"


def test_address_shaped_names_resolve_by_range(tmp_path):
    root = make_root(tmp_path)
    assert decomp_src.resolve(root, "src/__sinit_ov002_02107370.c") == "src/game/actors/d_a_demo.cpp"
    assert decomp_src.resolve(root, "src/func_0203ea5c.c") == "src/engine/link/unit0203d7b8.cpp"
    # the overlay is part of the key: the same address in another module is not this file
    assert decomp_src.resolve(root, "src/func_ov003_02107370.c") is None


def test_explicit_location_and_symbol_name(tmp_path):
    root = make_root(tmp_path)
    assert decomp_src.resolve(root, "src/HeapAllocator_Destroy.cpp") == "src/engine/heap/Heap.cpp"
    assert decomp_src.resolve(root, "src/renamed_thing.c", "ov002", 0x02107370) == "src/game/actors/d_a_demo.cpp"
    assert decomp_src.resolve(root, "src/renamed_thing.c") is None


def test_no_srcpath_in_root(tmp_path):
    (tmp_path / "empty").mkdir()
    assert decomp_src.resolve(tmp_path / "empty", "src/func_0203ea5c.c") is None


ASSIGN_FORM = """
void __sinit_ov002_02107370(void) {
    data_ov002_02110e24[0].x = data_ov002_0210b770;
    data_ov002_02110e24[0].y = data_ov002_0210b778;
    data_ov002_02110e34[0].x = data_ov002_0210b780;
}
"""
INIT_FORM = """
StaffRollStep data_ov002_02110e24[2] = {&StaffRoll::func_ov002_020f32e4,
                                        &StaffRoll::func_ov002_020f30f8};
StaffRollStep data_ov002_02110e34[1] = {&StaffRoll::Named};
int data_ov002_0210b630[2] = {(int)0x81f020f0, (int)0xffff0108};
"""


def test_tail2_reads_both_source_forms():
    assert tail2_cells.assign_cells(ASSIGN_FORM) == [
        (0x02110e24, 0, 0x0210b770), (0x02110e24, 1, 0x0210b778), (0x02110e34, 0, 0x0210b780)]
    assert tail2_cells.init_cells(INIT_FORM) == [
        (0x02110e24, 0, "func_ov002_020f32e4"), (0x02110e24, 1, "func_ov002_020f30f8"),
        (0x02110e34, 0, "Named")]
    # a table that is not all member pointers is not a state table
    assert tail2_cells.init_cells("int data_ov002_0210b630[2] = {1, 2};") == []


def test_tail2_member_name_to_address():
    syms = ("_ZN9StaffRoll5NamedEv kind:function(arm,size=0x10) addr:0x020f3000\n"
            "_ZN9StaffRoll7NamedToEv kind:function(arm,size=0x10) addr:0x020f3100\n")
    assert tail2_cells.fn_addr("func_ov002_020f32e4", syms) == 0x020f32e4
    assert tail2_cells.fn_addr("Named", syms) == 0x020f3000
    assert tail2_cells.fn_addr("Missing", syms) is None


def test_tailjump_guard_rebases_moved_tus(tmp_path):
    import tailjump_guard
    root = make_root(tmp_path)
    rows = [{"frame": "a", "tu": "src/func_0203ea5c.c"},
            {"frame": "b", "tu": "src/still/here.c"},
            {"frame": "c", "tu": "build/port/host-src/src/x.cpp"},
            {"frame": "d", "tu": "src/func_0300ea5c.c"}]
    out, moved = tailjump_guard.rebase_moved(rows, str(root))
    assert moved == [("src/func_0203ea5c.c", "src/engine/link/unit0203d7b8.cpp")]
    assert [r["tu"] for r in out] == ["src/engine/link/unit0203d7b8.cpp", "src/still/here.c",
                                      "build/port/host-src/src/x.cpp", "src/func_0300ea5c.c"]
    assert out[0]["tu_declared"] == "src/func_0203ea5c.c"
