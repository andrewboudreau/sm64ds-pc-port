"""Tests for port/tools/hook_anchor_check.py (catch-up plan Task 4.4).

Run: python -m pytest port/tools/test_hook_anchor_check.py -v

The fixtures are spelled in the pin's names on purpose (catchup_maps: no-apply).
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import decomp_overlay as dov  # noqa: E402
import hook_anchor_check as hac  # noqa: E402


# ---------------------------------------------------------------------------
# fixture: a BEFORE and an AFTER decomp tree, their patch dirs, a maps dir

def write(path, text):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(text.encode("utf-8") if isinstance(text, str) else text)


class Side:
    def __init__(self, root, name):
        self.root = Path(root) / (name + "_decomp")  # decomp-root-check: explicit: temp fixture tree
        self.patches = Path(root) / (name + "_patches")  # decomp-root-check: explicit: temp fixture tree
        self.root.mkdir(parents=True)
        self.patches.mkdir(parents=True)

    def symbols(self, module, rows):
        sub = {"arm9": "arm9"}.get(module, "arm9/overlays/" + module)
        lines = ["%s kind:function(arm,size=0x10) addr:%s" % (name, addr) for name, addr in rows]
        write(self.root / "config" / sub / "symbols.txt", "\n".join(lines) + "\n")

    def file(self, rel, text, edited):
        write(self.root / rel, text)
        patch = dov.make_patch(rel, text.encode("utf-8"), edited.encode("utf-8"))
        assert patch, "fixture edit is empty"
        write(self.patches / (rel + ".patch"), patch)


class Case:
    def __init__(self, tmp_path):
        self.tmp = Path(tmp_path)
        self.before = Side(tmp_path, "before")
        self.after = Side(tmp_path, "after")
        self.maps = self.tmp / "maps"
        self.maps.mkdir()
        self.edits = self.tmp / "decomp_edits.tsv"
        self.kinds = {}
        self.allow = None

    def kind(self, rel, kind="HOOK"):
        self.kinds[rel] = kind

    def renames(self, rows):
        text = "#module\taddr\told_name\tnew_name\n" + "".join("%s\t%s\t%s\t%s\n" % r for r in rows)
        write(self.maps / "renames.tsv", text)

    def patchmap(self, rows):
        text = "#path\tstatus\tnew_paths\tunowned\n" + "".join("%s\t%s\t%s\t0\n" % r for r in rows)
        write(self.maps / "patchmap.tsv", text)

    def allow_list(self, lines):
        self.allow = self.tmp / "allow.tsv"
        write(self.allow, "".join(line + "\n" for line in lines))

    def run(self, maps=True):
        write(self.edits, "path\tstatus\tkind\tside\tlines_added\tlines_removed\tnote\n"
              + "".join("%s\tM\t%s\tplain\t1\t0\t\n" % kv for kv in sorted(self.kinds.items())))
        report = self.tmp / "report.tsv"
        argv = ["--before-root", str(self.before.root), "--after-root", str(self.after.root),
                "--before-patches", str(self.before.patches), "--after-patches", str(self.after.patches),
                "--report", str(report), "--edits", str(self.edits)]
        if maps:
            argv += ["--maps", str(self.maps)]
        if self.allow:
            argv += ["--allow", str(self.allow)]
        rc = hac.main(argv)
        rows = []
        for line in report.read_text(encoding="utf-8").splitlines():
            if line.startswith("#") or line.startswith("result\t"):
                continue
            rows.append(line.split("\t"))
        return rc, rows


HOOK = "#ifdef _MSC_VER\n    port_hook();\n#endif\n"

C_FILE = """#include "types.h"

void Foo(int a)
{
    a += 1;
    a += 2;
    a += 3;
    a += 4;
}

void Bar(int b)
{
    b += 1;
    b += 2;
    b += 3;
    b += 4;
}
"""


def insert_after(text, marker, add):
    k = text.index(marker) + len(marker)
    return text[:k] + add + text[k:]


# ---------------------------------------------------------------------------
# the six required cases

def test_same_function_passes(tmp_path):
    c = Case(tmp_path)
    rel = "src/foo.c"
    c.kind(rel)
    c.before.symbols("ov002", [("Foo", "0x02100000"), ("Bar", "0x02100040")])
    c.after.symbols("ov002", [("Foo", "0x02100000"), ("Bar", "0x02100040")])
    c.before.file(rel, C_FILE, insert_after(C_FILE, "    a += 2;\n", HOOK))
    # main added lines above the function: the re-cut hunk sits at another line, same function
    after_src = C_FILE.replace('#include "types.h"\n', '#include "types.h"\n#include "other.h"\n\nint g;\n')
    c.after.file(rel, after_src, insert_after(after_src, "    a += 2;\n", HOOK))
    rc, rows = c.run()
    assert rc == 0
    assert [r[0] for r in rows] == ["SAME"]
    assert "function Foo {ov002:0x02100000}" in rows[0][4]
    assert "addr" in rows[0][7]


def test_function_renamed_on_main_matches_by_address(tmp_path):
    c = Case(tmp_path)
    rel = "src/foo.cpp"
    c.kind(rel)
    c.before.symbols("ov021", [("func_ov021_02112544", "0x02112544")])
    c.after.symbols("ov021", [("_ZN9daGrock_c19HandleMeshCollisionEv", "0x02112544")])
    before_src = 'extern "C" void func_ov021_02112544(int a)\n{\n    a += 1;\n    a += 2;\n    a += 3;\n    a += 4;\n}\n'
    after_src = ("void daGrock_c::HandleMeshCollision()\n{\n    int a = 0;\n    a += 1;\n    a += 2;\n"
                 "    a += 3;\n    a += 4;\n}\n")
    c.before.file(rel, before_src, insert_after(before_src, "    a += 2;\n", HOOK))
    c.after.file(rel, after_src, insert_after(after_src, "    a += 2;\n", HOOK))
    rc, rows = c.run()
    assert rc == 0, rows
    assert rows[0][0] == "SAME"
    assert "daGrock_c::HandleMeshCollision {ov021:0x02112544}" in rows[0][5]


def test_renamed_class_header_inline_needs_the_map(tmp_path):
    """A header inline method has no symbol: only the rename map ties Player:: to daPly_c::."""
    c = Case(tmp_path)
    rel_b, rel_a = "include/Player.h", "include/daPly_c.h"
    c.kind(rel_b)
    c.patchmap([(rel_b, "renamed", rel_a)])
    c.renames([("ov002", "0x020e30a0", "_ZN6Player11ChangeStateERNS_5StateE",
                "_ZN7daPly_c11ChangeStateERNS_5StateE")])
    body = "    {\n        x += 1;\n        x += 2;\n        x += 3;\n        x += 4;\n    }\n"
    before_src = "#pragma once\nclass Player\n{\npublic:\n    int x;\n    void Tick()\n" + body + "};\n"
    after_src = "#pragma once\nclass daPly_c\n{\npublic:\n    int x;\n    void Tick()\n" + body + "};\n"
    c.before.file(rel_b, before_src, insert_after(before_src, "        x += 2;\n", HOOK))
    c.after.file(rel_a, after_src, insert_after(after_src, "        x += 2;\n", HOOK))
    rc, rows = c.run()
    assert rc == 0, rows
    assert rows[0][0] == "SAME"
    assert "function Player::Tick" in rows[0][4] and "function daPly_c::Tick" in rows[0][5]
    assert "name via map" in rows[0][7]
    # without the map the same pair is not provably the same function
    rc, rows = c.run(maps=False)
    assert rc == 1


def test_hook_moved_into_another_function_fails(tmp_path):
    c = Case(tmp_path)
    rel = "src/foo.c"
    c.kind(rel)
    c.before.symbols("ov002", [("Foo", "0x02100000"), ("Bar", "0x02100040")])
    c.after.symbols("ov002", [("Foo", "0x02100000"), ("Bar", "0x02100040")])
    c.before.file(rel, C_FILE, insert_after(C_FILE, "    a += 2;\n", HOOK))
    c.after.file(rel, C_FILE, insert_after(C_FILE, "    b += 2;\n", HOOK))
    rc, rows = c.run()
    assert rc == 1
    assert rows[0][0] == "MOVED"
    assert "function Foo" in rows[0][4] and "function Bar" in rows[0][5]


def test_moved_hook_on_the_allow_list_passes(tmp_path):
    c = Case(tmp_path)
    rel = "src/foo.c"
    c.kind(rel)
    c.before.symbols("ov002", [("Foo", "0x02100000"), ("Bar", "0x02100040")])
    c.after.symbols("ov002", [("Foo", "0x02100000"), ("Bar", "0x02100040")])
    c.before.file(rel, C_FILE, insert_after(C_FILE, "    a += 2;\n", HOOK))
    c.after.file(rel, C_FILE, insert_after(C_FILE, "    b += 2;\n", HOOK))
    c.allow_list(["# intentional", "%s\t1\tmain inlined Foo into Bar" % rel])
    rc, rows = c.run()
    assert rc == 0
    assert rows[0][0] == "MOVED_LISTED"
    assert "main inlined Foo into Bar" in rows[0][7]


def test_header_inline_method(tmp_path):
    c = Case(tmp_path)
    rel = "include/Thing.h"
    c.kind(rel)
    src = ("#pragma once\nnamespace ns {\ntemplate <typename T>\nstruct Thing : public Base<T>\n{\n"
           "    T v;\n    inline T Get() const\n    {\n        T r = v;\n        r += 1;\n        r += 2;\n"
           "        return r;\n    }\n    int other;\n};\n}\n")
    edited = insert_after(src, "        r += 1;\n", HOOK)
    c.before.file(rel, src, edited)
    shifted = src.replace("#pragma once\n", "#pragma once\n#include \"a.h\"\n")
    c.after.file(rel, shifted, insert_after(shifted, "        r += 1;\n", HOOK))
    rc, rows = c.run()
    assert rc == 0, rows
    assert "function ns::Thing::Get" in rows[0][4]
    assert rows[0][0] == "SAME"


def test_file_scope_hook_classified_by_preceding_declaration(tmp_path):
    c = Case(tmp_path)
    rel = "src/foo.c"
    c.kind(rel)
    c.before.symbols("arm9", [("Foo", "0x02010000"), ("Bar", "0x02010040")])
    c.after.symbols("arm9", [("Foo", "0x02010000"), ("Bar", "0x02010040")])
    host_fn = "\n#ifdef _MSC_VER\nvoid HostThing(void)\n{\n    Foo(1);\n}\n#endif\n"
    c.before.file(rel, C_FILE, insert_after(C_FILE, "    a += 4;\n}\n", host_fn))
    c.after.file(rel, C_FILE, insert_after(C_FILE, "    a += 4;\n}\n", host_fn))
    rc, rows = c.run()
    assert rc == 0, rows
    assert rows[0][4].startswith("filescope[file] after Foo {arm9:0x02010000}")
    # the same host function placed after Bar instead is a move
    c2 = Case(tmp_path / "moved")
    c2.kind(rel)
    c2.before.symbols("arm9", [("Foo", "0x02010000"), ("Bar", "0x02010040")])
    c2.after.symbols("arm9", [("Foo", "0x02010000"), ("Bar", "0x02010040")])
    c2.before.file(rel, C_FILE, insert_after(C_FILE, "    a += 4;\n}\n", host_fn))
    c2.after.file(rel, C_FILE, C_FILE + host_fn)
    rc, rows = c2.run()
    assert rc == 1
    assert rows[0][0] == "MOVED" and "after Bar" in rows[0][5]


# ---------------------------------------------------------------------------
# further traps

def test_same_name_at_another_address_is_a_move(tmp_path):
    """The Dealer trap: the name the hook sits in now labels a different address."""
    c = Case(tmp_path)
    rel = "src/foo.c"
    c.kind(rel)
    c.before.symbols("ov006", [("Foo", "0x0213ac60"), ("Bar", "0x0213ac48")])
    c.after.symbols("ov006", [("Foo", "0x0213ac48"), ("Bar", "0x0213ac60")])
    c.before.file(rel, C_FILE, insert_after(C_FILE, "    a += 2;\n", HOOK))
    c.after.file(rel, C_FILE, insert_after(C_FILE, "    a += 2;\n", HOOK))
    rc, rows = c.run()
    assert rc == 1
    assert rows[0][0] == "MOVED" and "addr differs" in rows[0][7]


def test_patch_moved_into_tu_follows_patchmap(tmp_path):
    c = Case(tmp_path)
    rel_b, rel_a = "src/unnamed/func_02000000.c", "src/engine/unit02000000.cpp"
    c.kind(rel_b)
    c.patchmap([(rel_b, "moved", rel_a)])
    c.before.symbols("arm9", [("func_02000000", "0x02000000")])
    c.after.symbols("arm9", [("func_02000000", "0x02000000"), ("func_02000100", "0x02000100")])
    before_src = C_FILE.replace("Foo", "func_02000000")
    after_src = 'extern "C" {\n' + before_src.replace("Bar", "func_02000100") + "}\n"
    c.before.file(rel_b, before_src, insert_after(before_src, "    a += 2;\n", HOOK))
    c.after.file(rel_a, after_src, insert_after(after_src, "    a += 2;\n", HOOK))
    rc, rows = c.run()
    assert rc == 0, rows
    assert rows[0][6].startswith(rel_a + "#1")


def test_change_only_after_is_unknown(tmp_path):
    c = Case(tmp_path)
    rel = "src/foo.c"
    c.kind(rel)
    c.before.symbols("arm9", [("Foo", "0x02010000"), ("Bar", "0x02010040")])
    c.after.symbols("arm9", [("Foo", "0x02010000"), ("Bar", "0x02010040")])
    c.before.file(rel, C_FILE, insert_after(C_FILE, "    a += 2;\n", HOOK))
    two = insert_after(insert_after(C_FILE, "    a += 2;\n", HOOK), "    b += 2;\n", "    extra_hook();\n")
    c.after.file(rel, C_FILE, two)
    rc, rows = c.run()
    assert rc == 1
    res = sorted((r[0], r[2]) for r in rows)
    assert res == [("SAME", "1"), ("UNKNOWN", "after:2")]


def test_non_hook_patches_are_not_reported(tmp_path):
    c = Case(tmp_path)
    rel = "src/foo.c"
    c.kind(rel, "SRCFIX")
    c.before.file(rel, C_FILE, insert_after(C_FILE, "    a += 2;\n", HOOK))
    c.after.file(rel, C_FILE, insert_after(C_FILE, "    b += 2;\n", HOOK))
    rc, rows = c.run()
    assert rc == 0 and rows == []


def test_patch_that_does_not_apply_is_unknown(tmp_path):
    c = Case(tmp_path)
    rel = "src/foo.c"
    c.kind(rel)
    c.before.file(rel, C_FILE, insert_after(C_FILE, "    a += 2;\n", HOOK))
    c.after.file(rel, C_FILE, insert_after(C_FILE, "    a += 2;\n", HOOK))
    write(c.after.root / rel, C_FILE.replace("a += 1", "a += 9"))  # main changed the context
    rc, rows = c.run()
    assert rc == 1
    assert rows[0][0] == "UNKNOWN" and "does not apply" in rows[0][7]


def test_preprocessor_branches_with_unbalanced_braces():
    text = ("void Foo(int a)\n{\n#ifdef _MSC_VER\n    if (a) {\n#else\n    if (!a) {\n#endif\n        a++;\n"
            "    }\n    a--;\n}\n\nvoid Bar(void)\n{\n    x();\n}\n")
    res = hac.scan(text)
    idx = hac.SymbolIndex("does-not-exist")
    assert hac.anchor_at(res, 9, set(), idx).qual == "Foo"    # a--;
    assert hac.anchor_at(res, 14, set(), idx).qual == "Bar"   # x();
    assert res.warnings == []


def test_signature_edit_keeps_the_function_as_anchor():
    """A hook that edits the function's own header line still anchors in that function."""
    text = "void Foo(int a)\n{\n    a++;\n}\n"
    res = hac.scan(text)
    idx = hac.SymbolIndex("does-not-exist")
    a = hac.anchor_at(res, 0, {0}, idx)
    assert a.kind == "function" and a.qual == "Foo"


def test_demangle_qualified_names():
    assert hac.demangle_qualname("_ZN7daPly_c11ChangeStateERNS_5StateE") == "daPly_c::ChangeState"
    assert hac.demangle_qualname("_ZN6PlayerD0Ev") == "Player::~Player"
    assert hac.demangle_qualname("_ZN5Fix12IiEaSERKS0_") == "Fix12::operator="
    assert hac.demangle_qualname("_Z3fooi") == "foo"
    assert hac.demangle_qualname("_ZTV6Player") is None
    assert hac.class_of_special("_ZTV6Player") == "Player"


CLASS_H = ("#pragma once\nclass Thing\n{\npublic:\n    int a;\n    int b;\n    int c;\n    void Run();\n"
           "    int d;\n};\n")


def test_class_member_hook_survives_neighbour_changes(tmp_path):
    """Main deleted the member the hook followed, and added another one: the hook did not move."""
    c = Case(tmp_path)
    rel = "include/Thing.h"
    c.kind(rel)
    member = "#ifdef _MSC_VER\n    int host_only;\n#endif\n"
    c.before.file(rel, CLASS_H, insert_after(CLASS_H, "    int c;\n", member))
    after_h = CLASS_H.replace("    int c;\n", "    int c2;\n    int c3;\n")
    c.after.file(rel, after_h, insert_after(after_h, "    int c3;\n", member))
    rc, rows = c.run()
    assert rc == 0, rows
    assert "after Thing::b on both sides" in rows[0][7]
    # the same member moved below Run() is a move
    c2 = Case(tmp_path / "moved")
    c2.kind(rel)
    c2.before.file(rel, CLASS_H, insert_after(CLASS_H, "    int c;\n", member))
    c2.after.file(rel, CLASS_H, insert_after(CLASS_H, "    void Run();\n", member))
    rc, rows = c2.run()
    assert rc == 1 and rows[0][0] == "MOVED"


def test_brace_slide_after_a_function(tmp_path):
    """A host function added after Foo, cut with Foo's closing brace as the first added line (the
    original '}' then closes the host function), still anchors 'after Foo', not inside Foo."""
    c = Case(tmp_path)
    rel = "src/foo.c"
    c.kind(rel)
    c.before.symbols("arm9", [("Foo", "0x02010000"), ("Bar", "0x02010040")])
    c.after.symbols("arm9", [("Foo", "0x02010000"), ("Bar", "0x02010040")])
    host_fn = "\nvoid HostThing(void)\n{\n    Foo(1);\n}\n"
    edited = insert_after(C_FILE, "    a += 4;\n}\n", host_fn)
    c.before.file(rel, C_FILE, edited)
    slid = ("--- a/{0}\n+++ b/{0}\n@@ -6,6 +6,11 @@\n     a += 2;\n     a += 3;\n     a += 4;\n+}}\n+\n"
            "+void HostThing(void)\n+{{\n+    Foo(1);\n }}\n \n void Bar(int b)\n").format(rel)
    write(c.after.root / rel, C_FILE)
    write(c.after.patches / (rel + ".patch"), slid)
    assert dov.apply_patch(rel, C_FILE.encode(), slid.encode()) == edited.encode()
    rc, rows = c.run()
    assert rc == 0, rows
    assert rows[0][5].startswith("filescope[file] after Foo {arm9:0x02010000}"), rows


def test_hooks_merged_into_one_after_group(tmp_path):
    """Main deleted the lines between two hooks: two BEFORE groups become one AFTER group."""
    c = Case(tmp_path)
    rel = "src/foo.c"
    c.kind(rel)
    c.before.symbols("arm9", [("Foo", "0x02010000"), ("Bar", "0x02010040")])
    c.after.symbols("arm9", [("Foo", "0x02010000"), ("Bar", "0x02010040")])
    two = insert_after(insert_after(C_FILE, "    a += 1;\n", "    hook_one();\n"), "    a += 2;\n",
                       "    hook_two();\n")
    c.before.file(rel, C_FILE, two)
    after_src = C_FILE.replace("    a += 2;\n", "")
    merged = insert_after(after_src, "    a += 1;\n", "    hook_one();\n    hook_two();\n")
    c.after.file(rel, after_src, merged)
    rc, rows = c.run()
    assert rc == 0, rows
    assert all(r[0] == "SAME" for r in rows) and len(rows) == 2
