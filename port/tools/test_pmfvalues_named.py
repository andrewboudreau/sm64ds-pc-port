"""pmfvalues --append: the named (pmf_*) constants get the shapes their types and targets call for."""
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import pmfvalues as P

DUMP = """
Dump of file C:\\b\\host-src\\src\\minigames\\d_s_mg_a.cpp.obj

001 00000000 UNDEF  notype       External     | ?pmf_A_c_Go@@3P8A_c@@AEXH@ZQ1@
002 00000000 UNDEF  notype       External     | ?pmf_A_c_Stay@@3P8A_c@@AEXXZQ1@
003 00000000 UNDEF  notype       External     | ?pmf_A_c_Rec@@3UStateRecord@@A
004 00000010 SECT1B notype ()    External     | ?Go@A_c@@QAEXXZ (public: void __thiscall A_c::Go(void))
005 00000020 SECT1B notype ()    External     | ?Stay@A_c@@QAEXXZ (public: void __thiscall A_c::Stay(void))

Dump of file C:\\b\\src\\minigames\\d_s_mg_b.cpp.obj

001 00000000 UNDEF  notype       External     | _pmf_Flat_Run
002 00000000 UNDEF  notype       External     | _pmf_A_c_Stay
003 00000000 UNDEF  notype       External     | _pmf_Word
004 00000030 SECT2  notype ()    External     | ?Flat_Run@@YAXXZ (void __cdecl Flat_Run(void))
"""

SRC_B = """
typedef void (Sprite::*SpriteFn)();
extern SpriteFn pmf_Flat_Run;
extern Word32 pmf_Word;
"""


class Named(unittest.TestCase):
    def test_codes(self):
        self.assertEqual([P.code_arity(c) for c in ("X", "H@", "HH@", "PAX@")], [0, 1, 2, None])

    def test_c_linkage_type_reads_the_tus_typedef(self):
        self.assertEqual(P.c_linkage_type(SRC_B, "pmf_Flat_Run"), (("Sprite", 0), None))
        self.assertEqual(P.c_linkage_type(SRC_B, "pmf_Word"), (None, "declared Word32"))
        self.assertEqual(P.c_linkage_type(SRC_B, "pmf_Missing"), (None, "declared None"))

    def test_rows_shapes_order_and_skips(self):
        undefs, defs = P.read_dumpbin(DUMP)
        self.assertEqual([u[0] for u in undefs], ["d_s_mg_a"] * 3 + ["d_s_mg_b"] * 3)
        with tempfile.TemporaryDirectory() as root:
            src = pathlib.Path(root, "src", "minigames")
            src.mkdir(parents=True)
            (src / "d_s_mg_b.cpp").write_text(SRC_B, encoding="utf-8")
            addrs = {"pmf_A_c_Go": (6, 0x100), "pmf_A_c_Stay": (6, 0x108), "pmf_Flat_Run": (6, 0x110)}
            words = {0x100: 0x2000, 0x108: 0x2010, 0x110: 0x2020}
            targets = {0x2000: "_ZN3A_c2GoEv", 0x2010: "_ZN3A_c4StayEv", 0x2020: "_Z8Flat_Runv"}
            rows, refused = P.named_rows(
                undefs, defs, set(), addrs.get,
                lambda n, a: words.get(a, 0), lambda n, c: targets.get(c), root)
        self.assertEqual([(r[0], r[4]) for r in rows],
                         [("pmf_A_c_Go", "drop1"), ("pmf_A_c_Stay", "member"), ("pmf_Flat_Run", "flat")])
        self.assertEqual(sorted(n for n, _ in refused), ["pmf_A_c_Rec", "pmf_Word"])
        text = "\n".join(P.emit_named(rows))
        self.assertIn("/alternatename:@port_pmfm_pmf_A_c_Go@8=?Go@A_c@@QAEXXZ", text)
        self.assertIn("/alternatename:_port_pmff_pmf_Flat_Run=?Flat_Run@@YAXXZ", text)
        self.assertIn("/alternatename:_pmf_Flat_Run=_port_pmfv_pmf_Flat_Run", text)

    def test_kept_rows_are_not_emitted_twice(self):
        head = ("/* x\n * file for the rule. 1 rows: y\n */\n\n"
                "extern \"C\" PortPmfPair port_pmfv_pmf_A_c_Stay = { 0, 0 };\n\n")
        kept, bound = P.split_kept(head)
        self.assertEqual(bound, {"pmf_A_c_Stay"})
        self.assertIn(P.SECTION_MARKERS[1], kept)
        again, bound2 = P.split_kept(kept + "extern \"C\" PortPmfPair port_pmfv_pmf_Late = { 0, 0 };\n")
        self.assertEqual((again, bound2), (kept, bound))


if __name__ == "__main__":
    unittest.main()
