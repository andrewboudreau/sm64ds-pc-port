"""ovdata.py reads the loader sources its residency model parses through the decomp patch overlay.

Run: python -m pytest port/tools/test_ovdata_overlay.py -v
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import decomp_overlay_paths as dop  # noqa: E402
import ovdata  # noqa: E402


def write(p, text):
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text)


def test_src_defining_prefers_the_overlay_copy(tmp_path):
    root = tmp_path / "decomp"
    write(root / "src" / "GetSceneOverlayID.c", "raw\n")
    write(root / "src" / "func_0201a694.c", "raw switch\n")
    build = root / "build" / "port"
    ov = build / "decomp-overlay"
    write(ov / "src" / "GetSceneOverlayID.c", "patched\n")
    write(ov / "src" / "only_in_layer_0201aaaa.c", "layer only\n")
    write(ov / "overlay_manifest.txt", "src/GetSceneOverlayID.c\nsrc/only_in_layer_0201aaaa.c\n")
    view = dop.DecompView(root, build)
    assert ovdata._src_defining(root, "GetSceneOverlayID", view).read_text() == "patched\n"
    assert ovdata._src_defining(root, "func_0201a694", view).read_text() == "raw switch\n"
    assert ovdata._src_defining(root, "0201aaaa", view).read_text() == "layer only\n"


def test_cross_view_reads_the_build_dir_off_the_output_path(tmp_path):
    root = tmp_path / "decomp"
    write(root / "src" / "GetSceneOverlayID.c", "raw\n")
    other = tmp_path / "elsewhere"
    write(other / "decomp-overlay" / "src" / "GetSceneOverlayID.c", "patched\n")
    write(other / "decomp-overlay" / "overlay_manifest.txt", "src/GetSceneOverlayID.c\n")
    view = ovdata._cross_view(root, other / "host-src" / "ov_cross.c")
    assert Path(view.path("src/GetSceneOverlayID.c")).read_text() == "patched\n"
    view = ovdata._cross_view(root, tmp_path / "x" / "ov_cross.c")
    assert Path(view.path("src/GetSceneOverlayID.c")).read_text() == "raw\n"
