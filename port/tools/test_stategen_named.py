"""stategen reads constructor fixtures whose destinations the decomp bump named.

Run: python -m pytest port/tools/test_stategen_named.py -v

catchup_maps carries the bump's names into port/tools/stategen_fixtures (data_ov006_021418b0 becomes
sCurlingAimTable), and port/ovNNN_syms.txt mounts the new names. parse_sinit(path, root) keys each
destination by its address from the root's symbols.txt and reports it under the root's name.
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import stategen  # noqa: E402

SINIT = """struct Pair { int a, b; };
struct Dst2 { struct Pair p0, p1; };

extern struct Pair data_ov006_0213c23c;
extern struct Pair data_ov006_0213c2bc;
extern struct Dst2 sCurlingAimTable;
extern struct Dst2 data_ov006_021418c0;

void __sinit_ov006_021304ac(void)
{
    sCurlingAimTable.p0 = data_ov006_0213c23c;
    sCurlingAimTable.p1 = data_ov006_0213c2bc;
    data_ov006_021418c0.p0 = data_ov006_0213c2bc;
}
"""
SYMS = ("sCurlingAimTable kind:bss addr:0x021418b0\n"
        "sCurlingBitKindTable kind:bss addr:0x021418c0\n")


def setup(tmp):
    root = tmp / "root"
    (root / "config/arm9/overlays/ov006").mkdir(parents=True)
    (root / "config/arm9/overlays/ov006/symbols.txt").write_text(SYMS)
    src = tmp / "__sinit_ov006_021304ac.c"
    src.write_text(SINIT)
    return root, src


def test_named_destination_reads_by_address(tmp_path):
    root, src = setup(tmp_path)
    ovn, out = stategen.parse_sinit(src, root)
    assert ovn == 6
    assert sorted(out) == [0x021418b0, 0x021418c0]
    assert out[0x021418b0]["sym"] == "sCurlingAimTable"
    assert [s[0] for s in out[0x021418b0]["slots"]] == [0, 1]
    # an address-spelled destination the root has named carries the root's name
    assert out[0x021418c0]["sym"] == "sCurlingBitKindTable"


def test_without_root_a_named_destination_is_not_read(tmp_path):
    _root, src = setup(tmp_path)
    ovn, out = stategen.parse_sinit(src)
    assert sorted(out) == [0x021418c0]
    assert out[0x021418c0]["sym"] == "data_ov006_021418c0"
