"""Tests for catchup_maps.py (run catchup1, Task 3.1).

Run: python -m unittest port/tools/test_catchup_maps.py  (or python port/tools/test_catchup_maps.py)

The four cases the plan names are the first four tests. Renames are keyed on
(module, addr); a name is never carried across by name alone.
"""
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import catchup_maps as cm  # noqa: E402


def table(rows):
    """rows: [(module, name, addr)] -> SymbolTable"""
    by_mod = {}
    for module, name, addr in rows:
        by_mod.setdefault(module, []).append((name, addr))
    return cm.SymbolTable.from_rows(by_mod)


class FakeResolver:
    """Stands in for srcpath run inside one checkout.

    owner: {(module, addr): path}; byname: {name: path}; owned: {path: [symbols]}"""

    def __init__(self, owner=None, byname=None, owned=None):
        self.owner = owner or {}
        self.byname = byname or {}
        self.owned = owned or {}

    def batch(self, locs, names, paths):
        return ({loc: self.owner.get(loc) for loc in locs},
                {n: self.byname.get(n) for n in names},
                {p: list(self.owned.get(p, [pathlib.PurePosixPath(p).stem])) for p in paths})


class RenameTests(unittest.TestCase):
    def test_address_whose_name_changed_is_renamed(self):
        pin = table([("arm9", "func_02000100", 0x02000100)])
        tgt = table([("arm9", "_ZN5Actor4MoveEv", 0x02000100)])
        res = cm.build_renames(pin, tgt, {"func_02000100"})
        self.assertEqual(res.renames, [("arm9", 0x02000100, "func_02000100", "_ZN5Actor4MoveEv")])
        self.assertEqual(res.hand, [])
        self.assertEqual(res.counts["renamed"], 1)

    def test_name_now_at_other_address_is_not_renamed_by_name(self):
        # X lived at 0x200. On main, 0x200 is called Z and the NAME X now labels 0x400.
        # A by-name map would say "X unchanged" and point every X reference at 0x400's
        # code. The address map must say X -> Z and tag the row.
        pin = table([("ov002", "X", 0x200), ("ov002", "W", 0x400)])
        tgt = table([("ov002", "Z", 0x200), ("ov002", "X", 0x400)])
        res = cm.build_renames(pin, tgt, {"X"})
        self.assertEqual(res.renames, [("ov002", 0x200, "X", "Z")])
        self.assertNotIn("X", res.unchanged)
        self.assertIn("reused_old_name", res.tags[("ov002", 0x200, "X")])
        self.assertEqual(res.hand, [])

    def test_name_moved_and_old_address_empty_is_deletion_not_rename(self):
        pin = table([("arm9", "X", 0x02000200)])
        tgt = table([("arm9", "X", 0x02000400)])
        res = cm.build_renames(pin, tgt, {"X"})
        self.assertEqual(res.renames, [])
        self.assertEqual([h[0] for h in res.hand], ["deleted"])
        self.assertIn("name now labels arm9:0x02000400", res.hand[0][4])

    def test_new_name_previously_owned_elsewhere_is_tagged(self):
        pin = table([("arm9", "A", 0x10), ("arm9", "B", 0x20)])
        tgt = table([("arm9", "B", 0x10)])  # 0x10 is now called B; 0x20 gone
        res = cm.build_renames(pin, tgt, {"A"})
        self.assertEqual(res.renames, [("arm9", 0x10, "A", "B")])
        self.assertIn("reused_new_name", res.tags[("arm9", 0x10, "A")])

    def test_symbol_deleted_on_main(self):
        pin = table([("ov006", "func_ov006_02100000", 0x02100000)])
        tgt = table([("ov006", "func_ov006_02100010", 0x02100010)])
        res = cm.build_renames(pin, tgt, {"func_ov006_02100000"})
        self.assertEqual(res.renames, [])
        self.assertEqual([h[0] for h in res.hand], ["deleted"])
        self.assertEqual(res.counts["deleted"], 1)

    def test_same_name_two_locations_different_outcomes(self):
        pin = table([("ov001", "f", 0x02100000), ("ov002", "f", 0x02100000)])
        tgt = table([("ov001", "f", 0x02100000), ("ov002", "g", 0x02100000)])
        res = cm.build_renames(pin, tgt, {"f"})
        self.assertEqual(res.renames, [("ov002", 0x02100000, "f", "g")])
        self.assertEqual(res.counts["unchanged"], 1)

    def test_alias_slot_ambiguity_goes_to_hand_with_candidates(self):
        pin = table([("dtcm", "a", 0x10)])
        tgt = table([("dtcm", "b", 0x10), ("dtcm", "c", 0x10)])
        res = cm.build_renames(pin, tgt, {"a"})
        self.assertEqual(res.renames, [])
        self.assertEqual(res.hand, [("ambiguous", "dtcm", 0x10, "a", "candidates b,c")])

    def test_explicit_scope_limits_rows(self):
        pin = table([("arm9", "a", 0x10)])
        tgt = table([("arm9", "b", 0x10)])
        self.assertEqual(cm.build_renames(pin, tgt, set()).renames, [])

    def test_default_covers_every_changed_address(self):
        # The run's maps must not depend on finding a reference in port/ first.
        pin = table([("arm9", "a", 0x10), ("arm9", "keep", 0x20), ("ov001", "c", 0x30)])
        tgt = table([("arm9", "b", 0x10), ("arm9", "keep", 0x20), ("ov001", "d", 0x30)])
        res = cm.build_renames(pin, tgt)
        self.assertEqual(sorted(res.renames), [("arm9", 0x10, "a", "b"), ("ov001", 0x30, "c", "d")])

    def test_alias_dropped_with_one_survivor_is_decided(self):
        pin = table([("arm9", "data_0209f40c", 0x0209f40c), ("arm9", "STAR_MARKERS", 0x0209f40c)])
        tgt = table([("arm9", "STAR_MARKERS", 0x0209f40c)])
        res = cm.build_renames(pin, tgt)
        self.assertEqual(res.renames, [("arm9", 0x0209f40c, "data_0209f40c", "STAR_MARKERS")])
        self.assertIn("alias_merge", res.tags[("arm9", 0x0209f40c, "data_0209f40c")])
        self.assertEqual(res.hand, [])

    def test_deleted_label_with_one_owner_in_other_module_follows_it(self):
        pin = table([("ov000", "data_ov000_020beb68", 0x020beb68)])
        tgt = table([("ov004", "gMgScene", 0x020beb68), ("ov000", "other", 0x020aa000)])
        res = cm.build_renames(pin, tgt)
        self.assertEqual(res.renames, [("ov000", 0x020beb68, "data_ov000_020beb68", "gMgScene")])
        self.assertIn("cross_module:ov004", res.tags[("ov000", 0x020beb68, "data_ov000_020beb68")])
        self.assertEqual(res.hand, [])

    def test_deleted_label_with_two_candidates_stays_in_hand(self):
        pin = table([("ov000", "data_ov000_020ad494", 0x020ad494)])
        tgt = table([("ov001", "_ZTV9dThIcon_c", 0x020ad494), ("ov001", "data_ov001_020ad494", 0x020ad494)])
        res = cm.build_renames(pin, tgt)
        self.assertEqual(res.renames, [])
        self.assertEqual(res.hand[0][0], "deleted")
        self.assertIn("ov001:_ZTV9dThIcon_c,data_ov001_020ad494", res.hand[0][4])

    def test_pseudo_symbol_below_rom_space_is_decided_without_rename(self):
        pin = table([("arm9", "overlay_100", 0x64)])
        res = cm.build_renames(pin, table([]))
        self.assertEqual(res.renames, [])
        self.assertEqual(res.hand, [])
        self.assertEqual([d[0] for d in res.decided], ["pseudo_symbol"])

    def test_apply_tags(self):
        pin = table([("arm9", "Banner_SetupFall", 0x10), ("arm9", "_ZN3Foo3BarEv", 0x20),
                     ("ov075", "data_ov075_0211d354", 0x30), ("arm9", "plain_old", 0x40)])
        tgt = table([("arm9", "_Z16Banner_SetupFallP8MgBanner", 0x10), ("arm9", "Foo_Bar", 0x20),
                     ("ov075", "@984", 0x30), ("arm9", "plain_new", 0x40)])
        res = cm.build_renames(pin, tgt)
        self.assertEqual(res.tags[("arm9", 0x10, "Banner_SetupFall")], {"linkage_c_to_cxx"})
        self.assertEqual(res.tags[("arm9", 0x20, "_ZN3Foo3BarEv")], {"linkage_cxx_to_c"})
        self.assertEqual(res.tags[("ov075", 0x30, "data_ov075_0211d354")], {"not_identifier"})
        self.assertEqual(res.tags[("arm9", 0x40, "plain_old")], set())

    def test_chain_rows_are_tagged(self):
        # 0x0203cbcc takes the name _ZdlPv from 0x0203cbf0, which is renamed itself.
        pin = table([("arm9", "_ZN6Memory16operator_delete2EPv", 0x0203cbcc), ("arm9", "_ZdlPv", 0x0203cbf0)])
        tgt = table([("arm9", "_ZdlPv", 0x0203cbcc), ("arm9", "_Z13func_0203cbf0Pv", 0x0203cbf0)])
        res = cm.build_renames(pin, tgt)
        self.assertIn("chain", res.tags[("arm9", 0x0203cbcc, "_ZN6Memory16operator_delete2EPv")])
        self.assertIn("chain", res.tags[("arm9", 0x0203cbf0, "_ZdlPv")])


class SrcMoveTests(unittest.TestCase):
    def test_symbol_moved_into_promoted_tu(self):
        pin = table([("arm9", "_ZN4Foo3RunEv", 0x02010000)])
        tgt = table([("arm9", "_ZN4Foo3RunEv", 0x02010000)])
        pin_res = FakeResolver(owner={("arm9", 0x02010000): "src/_ZN4Foo3RunEv.cpp"})
        tgt_res = FakeResolver(owner={("arm9", 0x02010000): "src/game/Foo.cpp"},
                               owned={"src/game/Foo.cpp": ["_ZN4Foo3RunEv", "_ZN4Foo4StopEv"]})
        ren = cm.build_renames(pin, tgt, {"_ZN4Foo3RunEv"})
        moves = cm.build_srcmoves(ren.locations, pin_res, tgt_res)
        self.assertEqual(moves.rows, [("_ZN4Foo3RunEv", "src/_ZN4Foo3RunEv.cpp", "src/game/Foo.cpp")])
        self.assertEqual(moves.counts["into_tu"], 1)

    def test_renamed_symbol_resolved_at_target_by_address(self):
        pin = table([("arm9", "old", 0x20)])
        tgt = table([("arm9", "new", 0x20), ("arm9", "old", 0x40)])
        pin_res = FakeResolver(owner={("arm9", 0x20): "src/old.c"})
        # by name, "old" would find 0x40's file; the address must win.
        tgt_res = FakeResolver(owner={("arm9", 0x20): "src/new.c"},
                               byname={"old": "src/wrong.c", "new": "src/new.c"})
        ren = cm.build_renames(pin, tgt, {"old"})
        moves = cm.build_srcmoves(ren.locations, pin_res, tgt_res)
        self.assertEqual(moves.rows, [("old", "src/old.c", "src/new.c")])
        self.assertEqual(moves.counts["moved"], 1)

    def test_unenrolled_symbol_with_unique_name_falls_back_to_name(self):
        pin = table([("arm9", "s", 0x20)])
        tgt = table([("arm9", "s", 0x20)])
        pin_res = FakeResolver(byname={"s": "src/s.c"})
        tgt_res = FakeResolver(byname={"s": "src/s.c"})
        moves = cm.build_srcmoves(cm.build_renames(pin, tgt, {"s"}).locations, pin_res, tgt_res)
        self.assertEqual(moves.rows, [])
        self.assertEqual(moves.counts["same"], 1)

    def test_source_gone_at_target(self):
        pin = table([("arm9", "s", 0x20)])
        tgt = table([("arm9", "s", 0x20)])
        pin_res = FakeResolver(owner={("arm9", 0x20): "src/s.c"})
        tgt_res = FakeResolver()
        moves = cm.build_srcmoves(cm.build_renames(pin, tgt, {"s"}).locations, pin_res, tgt_res)
        self.assertEqual(moves.rows, [("s", "src/s.c", "-")])
        self.assertEqual(moves.counts["source_gone"], 1)


class ScopeAndPatchTests(unittest.TestCase):
    def test_scope_reads_slice_rows_aliases_and_patches(self):
        pin = table([("arm9", "func_02000100", 0x02000100), ("arm9", "data_0208e6ec", 0x0208e6ec),
                     ("arm9", "_ZN3Foo3BarEv", 0x02000200), ("arm9", "_ZN3Foo3BazEv", 0x02000210),
                     ("arm9", "Unmentioned", 0x02000300)])
        pin_res = FakeResolver(owned={"src/game/Foo.cpp": ["_ZN3Foo3BarEv", "_ZN3Foo3BazEv"]})
        with tempfile.TemporaryDirectory() as d:
            port = pathlib.Path(d)
            (port / "hal").mkdir()
            (port / "slice_gate1.txt").write_text("# header\nsrc/func_02000100.c\nsrc/game/Foo.cpp\n")
            (port / "hal" / "cxx_aliases.cpp").write_text(
                '#pragma comment(linker, "/alternatename:?data_0208e6ec@@3PAHA=_data_0208e6ec")\n')
            (port / "decomp_patches" / "src" / "game").mkdir(parents=True)
            (port / "decomp_patches" / "src" / "game" / "Foo.cpp.patch").write_text("+// host\n")
            scope = cm.collect_scope(port, pin, pin_res)
        self.assertEqual(scope["func_02000100"], {"slice"})
        self.assertEqual(scope["_ZN3Foo3BarEv"], {"slice", "patch"})
        self.assertIn("cxx_aliases", scope["data_0208e6ec"])
        self.assertNotIn("Unmentioned", scope)

    def test_patch_path_mapping(self):
        pin = table([("arm9", "f", 0x10), ("arm9", "g", 0x20)])
        tgt = table([("arm9", "f", 0x10), ("arm9", "g", 0x20)])
        pin_res = FakeResolver(owned={"src/f.c": ["f"], "src/fg.cpp": ["f", "g"]})
        tgt_res = FakeResolver(owner={("arm9", 0x10): "src/area/FG.cpp", ("arm9", 0x20): "src/area/FG.cpp"})
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)  # decomp-root-check: explicit: temp fixture tree
            (root / "include").mkdir()
            (root / "include" / "kept.h").write_text("")
            rows = cm.map_patch_paths(["src/f.c", "include/kept.h", "include/gone.h", "include/Old.h"],
                                      pin, pin_res, tgt_res, root, {"include/Old.h": "include/New.h"})
        self.assertEqual(rows[0], ("src/f.c", "moved", "src/area/FG.cpp", 0))
        self.assertEqual(rows[1], ("include/kept.h", "exists", "include/kept.h", 0))
        self.assertEqual(rows[2], ("include/gone.h", "missing", "-", 0))
        self.assertEqual(rows[3], ("include/Old.h", "renamed", "include/New.h", 0))


class HeaderFollowTests(unittest.TestCase):
    def test_best_absorber_picks_unique_best(self):
        deleted = "struct SysTracker { Contents* mContents; Manager* mManager; };"
        best, scores = cm.best_absorber(deleted, {
            "include/dPa_c.h": "// The tree called it SysTracker\nManager* mManager;\nContents* mContents;",
            "include/Other.h": "int unrelated;"})
        self.assertEqual(best, "include/dPa_c.h")
        self.assertEqual(scores["include/Other.h"], 0)

    def test_best_absorber_refuses_ties_and_zero(self):
        self.assertIsNone(cm.best_absorber("struct Alpha {};", {"a.h": "Alpha", "b.h": "Alpha"})[0])
        self.assertIsNone(cm.best_absorber("struct Alpha {};", {"a.h": "Beta"})[0])

    def test_patch_path_absorbed_status(self):
        pin, tgt = table([]), table([])
        with tempfile.TemporaryDirectory() as d:
            rows = cm.map_patch_paths(["include/Gone.h"], pin, FakeResolver(), FakeResolver(), d,
                                      {}, lambda p: ("include/Big.h", "d56b10628ddddaf9"))
        self.assertEqual(rows, [("include/Gone.h", "absorbed@d56b10628dd", "include/Big.h", 0)])

    @unittest.skipUnless(shutil.which("git"), "git not on PATH")
    def test_git_rename_and_follow_on_a_scratch_repo(self):
        with tempfile.TemporaryDirectory() as d:
            repo = pathlib.Path(d)  # decomp-root-check: explicit: temp scratch git repo

            def git(*args):
                subprocess.run(["git", "-C", d, *args], check=True, capture_output=True)

            git("init", "-q")
            git("config", "user.email", "t@example.invalid")
            git("config", "user.name", "t")
            inc = repo / "include"
            inc.mkdir()
            body = "".join(f"    int mField{i:02d}Value;\n" for i in range(30))
            (inc / "Player.h").write_text("struct Player {\n" + body + "};\n")
            (inc / "SysTracker.h").write_text("struct SysTracker { int mManagerSlot; int mContentsSlot; };\n")
            (inc / "dPa_c.h").write_text("struct dPa_c { int x; };\n")
            (inc / "Noise.h").write_text("int noise;\n")
            git("add", "-A")
            git("commit", "-q", "-m", "pin")
            pin_rev = subprocess.run(["git", "-C", d, "rev-parse", "HEAD"], capture_output=True,
                                     text=True, check=True).stdout.strip()
            # Rename with most of the body rewritten: below git's default 50%, above 30%.
            kept = "".join(f"    int mField{i:02d}Value;\n" for i in range(12))
            new = "".join(f"    float mOther{i:02d}Thing;\n" for i in range(14))
            (inc / "Player.h").unlink()
            (inc / "daPly_c.h").write_text("struct daPly_c {\n" + kept + new + "};\n")
            git("add", "-A")
            git("commit", "-q", "-m", "rename")
            (inc / "SysTracker.h").unlink()
            (inc / "dPa_c.h").write_text("struct dPa_c { int x; int mManagerSlot; int mContentsSlot; };\n")
            (inc / "Noise.h").write_text("int noise2;\n")
            git("add", "-A")
            git("commit", "-q", "-m", "fold")
            renames = cm.git_file_renames(d, pin_rev, "HEAD")
            self.assertEqual(renames.get("include/Player.h"), "include/daPly_c.h")
            got, commit = cm.git_follow_deleted(d, pin_rev, "HEAD", "include/SysTracker.h")
            self.assertEqual(got, "include/dPa_c.h")
            self.assertTrue(commit)


def apply_maps(rows, moves=(), owned=None, hand=None, decided=(), applied=False):
    """rows: [(old, new, tags)] -> ApplyMaps"""
    full = [("arm9", "0x0", o, n, set(t)) for o, n, t in rows]
    count = (lambda p: owned.get(p, 1)) if owned is not None else None
    return cm.ApplyMaps(full, decided, hand, moves, count, None, applied)


def write_maps(d, rows, moves=(), decided=(), pin="p0", target="t1"):
    d = pathlib.Path(d)
    d.mkdir(parents=True, exist_ok=True)
    (d / "renames_kind.tsv").write_text(
        "#module\taddr\told_name\tnew_name\ttags\n"
        + "".join(f"arm9\t0x0\t{o}\t{n}\t{t or 'plain'}\n" for o, n, t in rows))
    (d / "srcmoves.tsv").write_text("#symbol\told_path\tnew_path\n"
                                    + "".join("\t".join(m) + "\n" for m in moves))
    (d / "decided.tsv").write_text("#kind\tmodule\taddr\tname\taction\n"
                                   + "".join(f"pseudo_symbol\tarm9\t0x64\t{n}\tx\n" for n in decided))
    (d / "target.txt").write_text(f"TARGET={target}\nPIN={pin}\nwhy: test\n")


def snapshot(root):
    return {p: p.read_bytes() for p in pathlib.Path(root).rglob("*") if p.is_file()}  # decomp-root-check: explicit: temp fixture tree


class ApplyTests(unittest.TestCase):
    def test_chain_substitutes_once(self):
        # 0x10 A -> B while 0x20 B -> C: A's text must become B, not C.
        m = apply_maps([("A", "B", ["chain"]), ("B", "C", ["chain"])])
        out, changes, items = cm.apply_text("A B\nB A\n", "list", m)
        self.assertEqual(out, "B C\nC B\n")
        self.assertEqual(len(changes), 4)
        self.assertEqual(items, [])

    def test_chain_both_ends_reported_in_code(self):
        # F2: _ZN6Memory16operator_delete2EPv -> _ZdlPv and _ZdlPv -> _Z13func_0203cbf0Pv.
        # Swapping either end in code would spell two ROM functions the same way.
        m = apply_maps([("_ZN6Memory16operator_delete2EPv", "_ZdlPv", ["chain"]),
                        ("_ZdlPv", "_Z13func_0203cbf0Pv", ["chain", "reused_old_name"])])
        src = "extern void _ZN6Memory16operator_delete2EPv(void*);\nvoid f(void*p){_ZdlPv(p);}\n"
        out, changes, items = cm.apply_text(src, "c", m)
        self.assertEqual(out, src)
        self.assertEqual(changes, [])
        self.assertEqual([(i[0], i[2]) for i in items],
                         [("hand", "_ZN6Memory16operator_delete2EPv"), ("hand", "_ZdlPv")])
        self.assertIn("chain", items[0][4])

    def test_rebinding_rows_never_swapped_in_pragmas(self):
        # F1: ??3@YAXPAX@Z is MSVC operator delete; renaming the left side by address would
        # move the equation onto another ROM function and still link.
        m = apply_maps([("_ZdlPv", "_Z13func_0203cbf0Pv", ["chain", "reused_old_name"]),
                        ("data_x", "data_y", ["reused_new_name"])])
        src = ('#pragma comment(linker, "/alternatename:__ZdlPv=??3@YAXPAX@Z")\n'
               'static const char *n = "data_x";\n')
        out, changes, items = cm.apply_text(src, "c", m)
        self.assertEqual(out, src)
        self.assertEqual([(i[0], i[2], i[5]) for i in items],
                         [("hand", "__ZdlPv", "string"), ("hand", "data_x", "string")])
        # a symbol list still follows the address
        out, _, _ = cm.apply_text("_ZdlPv\n", "list", m)
        self.assertEqual(out, "_Z13func_0203cbf0Pv\n")

    def test_new_name_already_in_port_code_is_a_collision(self):
        # F4: func_020731dc -> __register_global_object while hal already defines a stub of
        # that name: swapping the call would bind it to the stub.
        with tempfile.TemporaryDirectory() as d:
            port, maps = pathlib.Path(d) / "port", pathlib.Path(d) / "maps"
            (port / "hal").mkdir(parents=True)
            (port / "hal" / "rt.cpp").write_text('extern "C" void __register_global_object(void*) {}\n')
            (port / "hal" / "a.cpp").write_text("void g(){ func_020731dc(0); } // func_020731dc\n")
            (port / "slice_1.txt").write_text("func_020731dc\n")
            write_maps(maps, [("func_020731dc", "__register_global_object", "")])
            rep_ = pathlib.Path(d) / "rep.tsv"
            self.assertEqual(cm.main(["apply", "--maps", str(maps), "--port", str(port),
                                      "--report", str(rep_)]), 0)
            self.assertEqual((port / "hal" / "a.cpp").read_text(),
                             "void g(){ func_020731dc(0); } // __register_global_object\n")
            self.assertEqual((port / "slice_1.txt").read_text(), "__register_global_object\n")
            self.assertIn("name_collision\thal/a.cpp\t1\tfunc_020731dc\t__register_global_object",
                          rep_.read_text())

    def test_self_alias_is_reported(self):
        # F5: _data_ov000_x=_data_ov001_x becomes _data_ov001_x=_data_ov001_x.
        m = apply_maps([("data_ov000_020ad524", "data_ov001_020ad524", ["cross_module:ov001"])])
        src = ('#pragma comment(linker, "/alternatename:_data_ov000_020ad524=_data_ov001_020ad524")\n')
        out, _, items = cm.apply_text(src, "c", m)
        self.assertIn("_data_ov001_020ad524=_data_ov001_020ad524", out)
        self.assertEqual([(i[0], i[2]) for i in items], [("tagged_in_string", "_data_ov000_020ad524"),
                                                         ("self_alias", "_data_ov001_020ad524")])

    def test_at_only_starts_a_token_at_run_start(self):
        m = apply_maps([("data_a", "@262", ["not_identifier"]), ("@262", "@999", [])])
        out, _, _ = cm.apply_text('"1@2480-2620,1@262-2655"\n', "list", m)
        self.assertEqual(out, '"1@2480-2620,1@262-2655"\n')

    def _stamp_case(self, d, rows):
        port, maps = pathlib.Path(d) / "port", pathlib.Path(d) / "maps"
        (port / "hal").mkdir(parents=True)
        (port / "slice_1.txt").write_text("A B func_x func_y\n")
        write_maps(maps, rows)
        rep_ = pathlib.Path(d) / "rep.tsv"

        def run(*extra, maps_dir=maps):
            return cm.main(["apply", "--maps", str(maps_dir), "--port", str(port),
                            "--report", str(rep_), *extra])
        return port, maps, run

    def test_stamp_guards_chain_double_apply(self):
        rows = [("A", "B", "chain"), ("B", "C", "chain"), ("func_x", "Foo_1", ""), ("func_y", "Bar", "")]
        with tempfile.TemporaryDirectory() as d:
            port, maps, run = self._stamp_case(d, rows)
            self.assertEqual(run(), 0)
            self.assertEqual((port / "slice_1.txt").read_text(), "B C Foo_1 Bar\n")
            stamp = (port / cm.APPLY_STAMP).read_text()
            self.assertIn("pin_rev p0", stamp)
            self.assertIn("target_rev t1", stamp)
            applied = snapshot(port)
            # stamp deleted: the tree looks applied (chain tail C present) -> refuse
            (port / cm.APPLY_STAMP).unlink()
            self.assertEqual(run(), 2)
            self.assertEqual({k: v for k, v in applied.items() if k.name != cm.APPLY_STAMP}, snapshot(port))
            (port / cm.APPLY_STAMP).write_bytes(applied[port / cm.APPLY_STAMP])
            # regenerated maps for the same bump (other sha) -> refuse, nothing written
            write_maps(maps, rows + [("func_z", "Baz", "")])
            self.assertEqual(run(), 2)
            self.assertEqual(applied, snapshot(port))
            # --force overrides
            self.assertEqual(run("--dry-run", "--force"), 0)
            # maps for the next bump (pin = the stamp's target) -> allowed
            nxt = pathlib.Path(d) / "maps2"
            write_maps(nxt, [("Foo_1", "Foo_2", "")], pin="t1", target="t2")
            self.assertEqual(run(maps_dir=nxt), 0)
            self.assertEqual((port / "slice_1.txt").read_text(), "B C Foo_2 Bar\n")
            self.assertIn("target_rev t2", (port / cm.APPLY_STAMP).read_text())

    def test_stampless_plain_tree_that_looks_applied_is_refused(self):
        rows = [("func_x", "Foo", ""), ("func_y", "Bar", ""), ("func_w", "Baz", "")]
        with tempfile.TemporaryDirectory() as d:
            port, maps, run = self._stamp_case(d, rows)
            (port / "slice_1.txt").write_text("Foo Bar func_w\n")
            self.assertEqual(run("--dry-run"), 2)
            (port / "slice_1.txt").write_text("func_x Bar func_w\n")
            self.assertEqual(run("--dry-run"), 0)

    def test_token_boundary(self):
        m = apply_maps([("func_0201", "Foo", [])])
        out, _, _ = cm.apply_text("func_0201 func_02010 xfunc_0201 func_0201_x p->func_0201;", "c", m)
        self.assertEqual(out, "Foo func_02010 xfunc_0201 func_0201_x p->Foo;")
        m = apply_maps([("func_02010", "Bar", [])])
        out, _, _ = cm.apply_text("func_0201 func_02010\n", "list", m)
        self.assertEqual(out, "func_0201 Bar\n")

    def test_linkage_row_in_cpp_is_reported_and_in_syms_replaced(self):
        rows = [("Banner_SetupFall", "_Z16Banner_SetupFallP8MgBanner", ["linkage_c_to_cxx"])]
        m = apply_maps(rows)
        src = ('extern "C" void Banner_SetupFall(void*);\n'
               '#pragma comment(linker, "/alternatename:_Banner_SetupFall=_host_fall")\n'
               '// Banner_SetupFall is the ROM body\n')
        out, changes, items = cm.apply_text(src, "c", m)
        self.assertIn('extern "C" void Banner_SetupFall(void*);', out)
        self.assertIn("/alternatename:__Z16Banner_SetupFallP8MgBanner=_host_fall", out)
        self.assertIn("// _Z16Banner_SetupFallP8MgBanner is the ROM body", out)
        self.assertEqual([(i[0], i[2], i[4]) for i in items],
                         [("hand", "Banner_SetupFall", ("linkage_c_to_cxx",)),
                          ("tagged_in_string", "_Banner_SetupFall", ("linkage_c_to_cxx",))])
        out, _, items = cm.apply_text("Banner_SetupFall\nother\n", "list", m)
        self.assertEqual(out, "_Z16Banner_SetupFallP8MgBanner\nother\n")
        self.assertEqual(items, [])
        # end to end: the file kind decides it
        with tempfile.TemporaryDirectory() as d:
            port, maps = pathlib.Path(d) / "port", pathlib.Path(d) / "maps"
            (port / "hal").mkdir(parents=True)
            (port / "hal" / "x.cpp").write_text("void Banner_SetupFall();\n")
            (port / "ov006_syms.txt").write_text("Banner_SetupFall\n")
            write_maps(maps, [(o, n, ",".join(t)) for o, n, t in rows])
            rep = pathlib.Path(d) / "rep.tsv"
            cm.main(["apply", "--maps", str(maps), "--port", str(port), "--report", str(rep)])
            self.assertEqual((port / "hal" / "x.cpp").read_text(), "void Banner_SetupFall();\n")
            self.assertEqual((port / "ov006_syms.txt").read_text(), "_Z16Banner_SetupFallP8MgBanner\n")
            self.assertIn("hand\thal/x.cpp\t1\tBanner_SetupFall\t_Z16Banner_SetupFallP8MgBanner"
                          "\tlinkage_c_to_cxx\tcode", rep.read_text())

    def test_non_identifier_row(self):
        m = apply_maps([("data_ov075_0211d354", "@984", ["not_identifier"]),
                        (".p__sinit_02073a24", ".p__sinit_d_camera.cpp",
                         ["not_identifier", "old_not_identifier"]),
                        ("__sinit_x.cpp", "__sinit_y.cpp", ["not_identifier", "old_not_identifier"])])
        out, _, items = cm.apply_text("extern int data_ov075_0211d354;\n", "c", m)
        self.assertEqual(out, "extern int data_ov075_0211d354;\n")
        self.assertEqual(items[0][0], "hand")
        out, _, _ = cm.apply_text(
            "data_ov075_0211d354 .p__sinit_02073a24 __sinit_x.cpp src/__sinit_x.cpp x.data_ov075_0211d354\n",
            "list", m)
        self.assertEqual(out, "@984 .p__sinit_d_camera.cpp __sinit_y.cpp src/__sinit_x.cpp x.@984\n")
        # MSVC-decorated C++ forms split at '@'
        m = apply_maps([("_ZN6Player4StepEv", "_ZN7daPly_c4StepEv", [])])
        out, _, _ = cm.apply_text('"/alternatename:?_ZN6Player4StepEv@@YAXPAX@Z=__ZN6Player4StepEv"', "c", m)
        self.assertEqual(out, '"/alternatename:?_ZN7daPly_c4StepEv@@YAXPAX@Z=__ZN7daPly_c4StepEv"')
        out, _, _ = cm.apply_text("// x=__ZN6Player4StepEv\nint __ZN6Player4StepEv;\n", "c", m)
        self.assertEqual(out, "// x=__ZN7daPly_c4StepEv\nint __ZN6Player4StepEv;\n")

    def test_srcmove_paths(self):
        moves = [("func_a", "src/func_a.c", "src/engine/func_a.c"),           # one-symbol move
                 ("func_b", "src/func_b.c", "src/game/d_tu.cpp"),            # into a TU
                 ("func_c", "src/func_c.c", "src/game/d_tu.cpp"),
                 ("func_d", "src/func_d.c", "-")]                            # gone
        m = apply_maps([("func_b", "_ZN2Tu1bEv", [])], moves)
        src = ("src/func_a.c\nsrc/func_b.c  # b\n${DECOMP_ROOT}/src/func_d.c\n"
               "src/func_b_helper.c\n")
        out, changes, items = cm.apply_text(src, "list", m)
        self.assertEqual(out, "src/engine/func_a.c\nsrc/func_b.c  # b\n${DECOMP_ROOT}/src/func_d.c\n"
                              "src/func_b_helper.c\n")
        self.assertEqual([(i[0], i[2]) for i in items],
                         [("path_into_tu", "src/func_b.c"), ("path_gone", "src/func_d.c")])
        # the target says the new file owns several symbols: a TU even with one moved row
        m = apply_maps([], [moves[0]], owned={"src/engine/func_a.c": 5})
        out, _, items = cm.apply_text("src/func_a.c\n", "list", m)
        self.assertEqual(out, "src/func_a.c\n")
        self.assertEqual(items[0][0], "path_into_tu")

    def test_patches_untouched_and_idempotent(self):
        rows = [("A", "B", "chain"), ("B", "C", "chain"), ("func_x", "Foo", ""),
                ("LnkFn", "_Z5LnkFnv", "linkage_c_to_cxx")]
        moves = [("func_x", "src/func_x.c", "src/sub/func_x.c")]
        with tempfile.TemporaryDirectory() as d:
            port, maps = pathlib.Path(d) / "port", pathlib.Path(d) / "maps"
            for sub in ("decomp_patches/src", "decomp_overlay_files/src", "third_party", "hal"):
                (port / sub).mkdir(parents=True)
            patch = "--- a/src/func_x.c\n+++ b/src/func_x.c\n+ A(); func_x();\n"
            (port / "decomp_patches/src/func_x.c.patch").write_text(patch)
            (port / "decomp_overlay_files/src/y.cpp").write_text("func_x A\n")
            (port / "third_party/z.h").write_text("func_x A\n")
            (port / "hal/h.cpp").write_text("void A(); void B(); int func_x; void LnkFn();\n")
            (port / "slice_1.txt").write_text("src/func_x.c\nA B LnkFn\n")
            (port / "blob.bin").write_bytes(b"A\0B")
            write_maps(maps, rows, moves)
            rep = pathlib.Path(d) / "rep.tsv"
            before = snapshot(port)
            cm.main(["apply", "--maps", str(maps), "--port", str(port), "--report", str(rep), "--dry-run"])
            self.assertEqual(before, snapshot(port))
            self.assertIn("# files_changed 2", rep.read_text())
            cm.main(["apply", "--maps", str(maps), "--port", str(port), "--report", str(rep)])
            # chain names are never swapped in code (reported); plain rows are
            self.assertEqual((port / "hal/h.cpp").read_text(), "void A(); void B(); int Foo; void LnkFn();\n")
            self.assertEqual((port / "slice_1.txt").read_text(), "src/sub/func_x.c\nB C _Z5LnkFnv\n")
            for keep in ("decomp_patches/src/func_x.c.patch", "decomp_overlay_files/src/y.cpp",
                         "third_party/z.h", "blob.bin"):
                self.assertEqual((port / keep).read_bytes(), before[port / keep], keep)
            self.assertTrue((port / cm.APPLY_STAMP).is_file())
            after = snapshot(port)
            cm.main(["apply", "--maps", str(maps), "--port", str(port), "--report", str(rep)])
            self.assertEqual(after, snapshot(port))
            text = rep.read_text()
            self.assertIn("# files_changed 0", text)
            self.assertIn("# stamp_before matched", text)
            self.assertIn("hand\thal/h.cpp\t1\tLnkFn\t_Z5LnkFnv", text)

    def test_parked_patches_and_marked_fixtures_untouched(self):
        rows = [("func_x", "Foo", "")]
        with tempfile.TemporaryDirectory() as d:
            port, maps = pathlib.Path(d) / "port", pathlib.Path(d) / "maps"
            for sub in ("catchup_parked/src", "tools", "hal"):
                (port / sub).mkdir(parents=True)
            parked = "--- a/src/func_x.c\n+++ b/src/func_x.c\n@@ -1 +1 @@\n func_x();\n"
            (port / "catchup_parked/src/func_x.c.patch").write_text(parked)
            fixture = '"""fixture (%s)"""\nOLD = "func_x"\n' % cm.APPLY_SKIP_MARKER
            (port / "tools/test_fixture.py").write_text(fixture)
            (port / "hal/h.cpp").write_text("int func_x;\n")
            write_maps(maps, rows, [])
            rep = pathlib.Path(d) / "rep.tsv"
            before = snapshot(port)
            cm.main(["apply", "--maps", str(maps), "--port", str(port), "--report", str(rep)])
            self.assertEqual((port / "hal/h.cpp").read_text(), "int Foo;\n")
            for keep in ("catchup_parked/src/func_x.c.patch", "tools/test_fixture.py"):
                self.assertEqual((port / keep).read_bytes(), before[port / keep], keep)
            self.assertIn("# files_changed 1", rep.read_text())

    def test_hand_and_decided_names_are_never_swapped(self):
        m = apply_maps([("X", "Y", []), ("overlay_100", "Q", [])],
                       hand={"data_023c0000": "ambiguous"}, decided={"overlay_100"})
        out, _, items = cm.apply_text("X data_023c0000 overlay_100\n", "list", m)
        self.assertEqual(out, "Y data_023c0000 overlay_100\n")
        self.assertEqual([(i[0], i[2]) for i in items], [("hand_listed", "data_023c0000")])
        # one old name with two different new names is a conflict, reported
        full = [("arm9", "0x1", "D", "E", set()), ("ov002", "0x1", "D", "F", set())]
        m = cm.ApplyMaps(full)
        out, _, items = cm.apply_text("D\n", "list", m)
        self.assertEqual(out, "D\n")
        self.assertEqual(items[0][4], ("conflict:E|F",))

    def test_python_contexts(self):
        m = apply_maps([("data_020876d4", "@12", ["not_identifier"])])
        src = 'T = {"data_020876d4": 1}  # data_020876d4\nx = data_020876d4\n'
        out, _, items = cm.apply_text(src, "py", m)
        self.assertEqual(out, 'T = {"@12": 1}  # @12\nx = data_020876d4\n')
        self.assertEqual([i[0] for i in items], ["tagged_in_string", "hand"])

    def test_word_rows_are_never_swapped(self):
        m = apply_maps([("main", "_Z9NitroMainv", ["linkage_c_to_cxx"]), ("Mod", "SVC_DivRem", [])])
        src = "main  # decomp main\nMod\n"
        out, changes, items = cm.apply_text(src, "list", m)
        self.assertEqual(out, src)
        self.assertEqual([(i[0], i[2]) for i in items], [("hand_word", "main"), ("hand_word", "Mod")])
        out, _, items = cm.apply_text("// main branch\nstruct Mod *m; int main();\n", "c", m)
        self.assertEqual([(i[0], i[2]) for i in items], [("hand_word", "Mod"), ("hand_word", "main")])
        out, _, items = cm.apply_text("main and Mod\n", "list", m, doc=True)
        self.assertEqual(items, [])


class ParseTests(unittest.TestCase):
    def test_parse_symbols_line(self):
        rows = cm.parse_symbols(
            "Div kind:function(thumb,size=0x4) addr:0x0200406a\n"
            ".p__sinit_02073a24 kind:data(word) addr:0x02073a24\n\n")
        self.assertEqual(rows, [("Div", 0x0200406a), (".p__sinit_02073a24", 0x02073a24)])


if __name__ == "__main__":
    unittest.main()
