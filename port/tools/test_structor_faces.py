"""Tests for port/tools/structor_faces.py: a structor the decomp already gives a body is not faced.

Run: python -m pytest port/tools/test_structor_faces.py -v
"""

import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import structor_faces as sf  # noqa: E402


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def tree(tmp):
    aliases = [
        {"symbol": "_ZN7Vector3D1Ev", "canonical_symbol": "_Z13func_02017a9ci"},
        {"symbol": "_ZN10KpaFilePtrC1Ej", "canonical_symbol": "_ZN13SharedFilePtr9ConstructEj"},
        {"symbol": "_ZN10KpaFilePtrD1Ev", "canonical_symbol": "_Z13func_02017ab4i"},
        {"symbol": "_ZN6WidgetC1Ei", "canonical_symbol": "_Z13func_02000000i"},
        {"symbol": "_ZN6WidgetD1Ev", "canonical_symbol": "_Z13func_02000004i"},
    ]
    write(tmp / "config/tu_manifest.d/a.json", json.dumps({"undefined_symbol_aliases": aliases}))
    # Vector3: inline destructor body in a header; Widget: out-of-line ctor in a src file;
    # KpaFilePtr: declared only (the face is needed). Widget's call `Widget(3) {` shape is not a definition.
    write(tmp / "include/types.h", "struct Vector3 {\n    ~Vector3() {}\n    Vector3() {}\n};\n")
    write(tmp / "include/kpa.h", "struct KpaFilePtr {\n    KpaFilePtr(unsigned int id);\n    ~KpaFilePtr();\n};\n")
    write(tmp / "src/widget.cpp", "Widget::Widget(int n) : mN(n) {\n}\nvoid f() { if (Widget(3).ok()) {} }\n")
    return tmp


def test_decomp_bodies_are_not_faced(tmp_path):
    root = tree(tmp_path)
    rows, _refused = sf.collect(str(root))
    have = sf.decomp_defined(str(root), rows)
    assert have == {"Vector3": {"D1"}, "Widget": {"C1"}}


def test_generated_file_skips_them(tmp_path, capsys):
    root = tree(tmp_path)
    out = tmp_path / "gen.cpp"
    sys.argv = ["structor_faces.py", "--root", str(root), "--out", str(out)]
    assert sf.main() == 0
    text = out.read_text(encoding="utf-8")
    assert "Vector3::~Vector3()" not in text
    assert "Widget::Widget(" not in text and "Widget::~Widget()" in text
    assert "KpaFilePtr::KpaFilePtr(" in text and "KpaFilePtr::~KpaFilePtr()" in text
