"""hostgen_portsync3's cstd rows follow the decomp root hostgen reads (raw or the patch overlay).

Run: python -m pytest port/tools/test_hostgen_portsync3.py -v

The fixture keeps its address name on purpose: hostgen builds the local from it (catchup_maps: no-apply).
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import hostgen_portsync3 as hp3  # noqa: E402
import decomp_root  # noqa: E402

CSTD = ("int func_02053274(int x)\n{\n%s    volatile int *sqrtcnt = (volatile int *)0x40002b0;\n"
        "    *sqrtcnt = 0;\n    return x;\n}\n")


def test_rows_come_from_the_decomp_root_hostgen_reads(tmp_path):
    raw = tmp_path / "raw" / "src"
    ovl = tmp_path / "ovl" / "src"
    raw.mkdir(parents=True)
    ovl.mkdir(parents=True)
    (raw / "cstd.cpp").write_text(CSTD % "")
    (ovl / "cstd.cpp").write_text(CSTD % "    /* patched */\n")
    rows = hp3._cstd_sqrt_rows(["--decomp", str(tmp_path / "ovl"), "src/cstd.cpp"])
    assert len(rows) == 1 and "/* patched */" in rows[0][0] and "sqrtcnt_3274" in rows[0][1]
    rows = hp3._cstd_sqrt_rows(["--decomp=%s" % (tmp_path / "raw"), "src/cstd.cpp"])
    assert len(rows) == 1 and "/* patched */" not in rows[0][0]


def test_default_root_is_the_repo():
    assert hp3._hostgen_decomp_root([]).resolve() == decomp_root.path(decomp_root.find()).resolve()
