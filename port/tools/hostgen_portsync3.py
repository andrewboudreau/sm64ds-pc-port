"""hostgen_portsync3 -- hostgen patch rows added at the 2026-10-14 main sync
(lane PORTSYNC3), on top of hostgen_portsync2.py. Same format: ROWS maps
(TABLE, stem) to the whole row. ASCII only."""
import decomp_root  # noqa: E402

ROWS = {}

# _ZN9dCamera_c13func_02009e70Ev (the camera follow state). Main declares `Vector3 sp70 =
# line.GetClsnPos();` in the middle of the body and jumps past it with
# `goto L_A534` (twice). mwccarm allows it; MSVC refuses it (C2362: the jump
# skips an initialisation). sp70 is never read, so the declaration is scoped
# to its own block; the GetClsnPos() call stays.
# catchup1 (SYM4, needs row from SLICE4): main's d_camera.cpp has no goto any
# more and dCamera_c::func_02009e70 declares no skipped local.
ROWS[("MWCC_SPELLING", "_ZN9dCamera_c13func_02009e70Ev")] = None

# ---- PARK_SYMBOL: folded bodies the port line never compiled ---------------
# Main's folds pulled these functions into TUs the port compiles wholesale.
# The 0.9.2 line (port/h8-rel092b) compiled none of them from src/ (checked
# against its own slice enumeration, tools/inferred_stub_guard.build_tu_set),
# and each one reaches DS-only runtime that the host never defines (OS mutex
# and interrupt helpers, the DS sound driver's hardware tables, ROM data in
# overlays the port does not mount). Parking restores exactly what ran before.
# src/ApproachAngle.cpp keeps ApproachAngle and UpdateAngle behind `#ifndef
# _MSC_VER` because they call __aeabi_idiv and "the host smoke build has no
# divider". The window targets call them (Eyerok, dMgJump3DMario, d_a_pg_rcer),
# so the guard is lifted for the generated copy and the divide goes through the
# port's own ds_idiv (hal/cstd_div.c), the DS divider with its x/0 = x rule.
ROWS[("MWCC_SPELLING", "ApproachAngle")] = [
    ("#ifndef _MSC_VER\n/* The host smoke build has no divider. These two call __aeabi_idiv. */\nextern \"C\" int __aeabi_idiv(int a, int b);",
     "/* PORTSYNC3: guard lifted for the host; the divide is ds_idiv (hal/cstd_div.c). */\nextern \"C\" int ds_idiv(int a, int b);\n#define __aeabi_idiv ds_idiv"),
    ("        angle = angle + step;\n    }\n}\n#endif\n",
     "        angle = angle + step;\n    }\n}\n"),
]
ROWS[("PARK_SYMBOL", "cstd4")] = ["__exit", "exit", "func_0206de14", "abort", "__flush_all", "fflush"]
ROWS[("PARK_SYMBOL", "cstd6")] = ["raise"]
ROWS[("PARK_SYMBOL", "SoundEngine")] = ["func_020506fc", "func_020503a4", "func_020500a4",
                                        "func_0204f194", "func_020507e0"]
ROWS[("PARK_SYMBOL", "Sound")] = ["_Z13func_020133bcv", "_Z13func_020134c8v"]
# dExtShadowModel_c::InitCuboid / InitCylinder are NOT parked: the old
# per-function files (src/_ZN11ShadowModel10InitCuboidEv.cpp) were compiled, and
# their ov000 data is bound by linkalias.py's same-address rule.
# daFRing_c / daObjFl_UkiKi_c InitResources were parked on the premise that the old
# build did not compile them; it did (as BowserShockwaves / LavaPlank), so they run.

# daDoor_c::InitResources and daTree_c::Behavior: PORTSYNC2 parked main's bodies
# on the premise of a flat host copy that does not exist in the tree (only
# daTor_c's, port/unmatched/Tornado_HostSites.cpp, does). The hostgen repairs the
# old per-function stems carried are keyed on the TU stem now, so main's member
# IS the repaired body: it is un-parked, and the two ledger rows are R faces
# (flat name -> member) instead of F faces (member -> flat).
ROWS[("PARK_SYMBOL", "daDoor_c")] = None
ROWS[("PARK_SYMBOL", "d_a_tree")] = None

# G3i.cpp (main's fold of the G3i matrix writers). LookAt_ binds a local
# `mtx` to the MTX_MULT_3x3 port and writes the view matrix through it; the
# fold put PerspectiveW_ in the same TU, whose `int *mtx` OUTPUT parameter is
# tested (`if (mtx)`) and indexed. hostgen's pointer rewrite works on names
# per TU text and gives up on a name with any non-deref use, so the whole
# `mtx` family was left plain: the title's LookAt_ latched into mapped memory
# and the geometry engine never got the view matrix (title bottom screen drew
# 8.0 units off, the buttons and wrench fell out of the picture). The row
# gives the register-bound local a name of its own.
# catchup1 (SYM4, needs row from ENG4): main binds both locals, each named `port`,
# with `port = &reg_G3_MTX_LOAD_4x4` / `&reg_G3_MTX_LOAD_4x3`. hostgen turns the
# reg_ macros into NTR_MMIO temporaries, so the address-of would bind a host
# pointer to nothing and every *port store would miss the geometry engine, the
# defect this row always cured. The local holds the register's DS address
# instead and each store goes through NTR_MMIO on it: the same stores, in the
# same order, to the same register.
ROWS[("MWCC_SPELLING", "G3i")] = [
    ("    volatile s32 *port;\n",
     "    unsigned port;  /* hostgen MWCC_SPELLING: the register's DS address */\n"),
    ("        port = &reg_G3_MTX_LOAD_4x4;\n",
     "        port = 0x04000458;\n"),
    ("        port = &reg_G3_MTX_LOAD_4x3;\n",
     "        port = 0x0400045c;\n"),
    ("        *port = ",
     "        NTR_MMIO(s32, port) = "),
]

# cstd.cpp (main's fold of the division / square-root unit routines). Three
# functions bind the SQRT unit's registers through locals of the SAME names
# (`sqrtcnt`, `sqrtParam`), once each per function and several times per TU.
# hostgen's pointer rewrite needs a name bound exactly once in the text it
# sees, so the whole family was left plain: the stores that start the unit
# latched in mapped memory and only the result read was routed. Each
# function's two names get a suffix of their own so every one is bound once.
#
# The rows are cut from the text hostgen itself transforms: the cstd.cpp under
# the --decomp root hostgen was given. When the decomp patch layer patches
# src/cstd.cpp, port/cmake/hostgen_overlay.cmake runs hostgen with
# --decomp <build>/decomp-overlay, so the rows come from the patched copy; read
# from the raw file they would describe text hostgen never sees.
def _hostgen_decomp_root(argv=None):
    import pathlib
    import sys
    argv = sys.argv[1:] if argv is None else argv
    root = None
    for i, a in enumerate(argv):
        if a == "--decomp" and i + 1 < len(argv):
            root = argv[i + 1]
        elif a.startswith("--decomp="):
            root = a.split("=", 1)[1]
    if root is None:
        return decomp_root.path(decomp_root.find())
    return decomp_root.path(root).expanduser().resolve()


def _cstd_sqrt_rows(argv=None):
    import re
    p = _hostgen_decomp_root(argv) / "src" / "cstd.cpp"
    if not p.is_file():
        return []
    text = p.read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")
    rows = []
    # Each function is looked up under its TARGET name and its old pin name, and
    # its suffix is fixed, so a rename cannot change the generated locals
    # (catchup1: VEC_Distance was func_02053274, and fn[-4:] became "ance").
    # Main spells the return fx32 and opens the brace on the same line.
    for names, sfx in ((("VEC_Distance", "func_02053274"), "3274"),
                       (("FX_Sqrt",), "Sqrt"),
                       (("FX_SqrtAsync",), "sync")):
        m = None
        for fn in names:
            m = re.search(r"(?:int|void|fx32) " + fn + r"\([^)]*\)\s*\{.*?\n\}\n", text, re.S)
            if m:
                break
        if not m:
            continue
        blk = m.group(0)
        new = re.sub(r"\bsqrtcnt\b", "sqrtcnt_" + sfx, blk)
        new = re.sub(r"\bsqrtParam\b", "sqrtParam_" + sfx, new)
        if new != blk:
            rows.append((blk, new))
    return rows


_cstd_rows = _cstd_sqrt_rows()
if _cstd_rows:
    ROWS[("MWCC_SPELLING", "cstd")] = _cstd_rows

# ---- lane MGSYNC1 (hunt8) ----------------------------------------------------

# dMg3DHeyhoObjAdapter_c: the shy-guy hit tests (_ZN5Heyho9CheckHitBEv and
# _ZN5Heyho9CheckHitAEv) read the three Mario objects' vtable words through plain
# C function-pointer types, (*(VT*)O)[0..2](O). On the ARM that is the ROM's
# call: the object in r0. Main's dMgJump3DMario_c now has a real constructor,
# so the objects carry MSVC's compiled vtable whose three slots are thiscall
# members (this in ECX). A cdecl call through them hands the member whatever ECX
# held: in the first dispatch of each hit test the compiler happened to load the
# object into ECX for its own reasons, in the other two ECX held the function
# pointer itself, so mVel was read from the code address and every shy guy
# counted as hit (scenes 372 and 373 cleared with no input). Spelling the
# pointer types thiscall passes the receiver the way the members expect it.
ROWS[("MWCC_SPELLING", "dMg3DHeyhoObjAdapter_c")] = [
    ("typedef int (**VT)(void*);\n",
     "typedef int (__thiscall **VT)(void*);  /* MGSYNC1: the slots are thiscall members */\n"),
    ("    Vec3 *(*f0)(Obj *);\n"
     "    Vec3 *(*f1)(Obj *);\n"
     "    int   (*f2)(Obj *);\n",
     "    Vec3 *(__thiscall *f0)(Obj *);   /* MGSYNC1: thiscall members */\n"
     "    Vec3 *(__thiscall *f1)(Obj *);\n"
     "    int   (__thiscall *f2)(Obj *);\n"),
]

# ---- lane MG4 (catchup1 Phase 4): the minigame TUs main folded or rewrote ----
# Rows re-derived by MG4 against decomp main bddb9d36a60, keyed on the TU stems
# main uses; MG4 round 2, appended verbatim. Only the rows whose stale override
# here would still block a TU after SLICE4's 526d471a3fd; the minigame TUs SLICE4
# re-derived in hostgen.py itself carry no row here, so nothing pops them.

ROWS[('MG_PMF_CALL', 'd_s_mg_pachinko')] = None

ROWS[('MG_PMF_CALL', 'd_s_mg_panel')] = None

ROWS[('MG_PMF_CALL', 'd_s_mg_hanachan_methods')] = None

# PMF_SEAM guards kept as EMPTY lists (pmf_seam_residue still checks the TU) with every typed
# ->* the TU keeps listed in PMF_SEAM_ALLOW. The declaration blocks only rode in with a seam
# patch, so they go. The panel's first ALLOW entry is the header comment's own `->*`.
ROWS[('PMF_SEAM_ALLOW', 'd_s_mg_panel')] = [
    '(this->*table[mKind[i]])(i)',
    '    (this->*sPanelShakeStateTable[state])(0);',
    '    (this->*sPanelBannerStateTable[mBanner.state])();',
    '    (this->*sPanelIntroCardStateTable[mSlideStep])();',
    '    (this->*sPanelAnswerStateTable[mInputState])();',
    '        (this->*sPanelPanelStateTable[mKind[i]])(i);',
    '        (this->*sPanelPanelStateTable[index])(i);',
    '    (this->*sPanelSceneStateTable[mState])();',
]

ROWS[('PMF_SEAM', 'd_s_mg_hanachan_methods')] = []

ROWS[('PMF_SEAM_DECL', 'd_s_mg_hanachan_methods')] = None

ROWS[('PMF_SEAM_ALLOW', 'd_s_mg_hanachan_methods')] = [
    '    (this->*sub)();',
    '        (this->*state)();',
    '    (this->*mPhase)();',
]

ROWS[('PARK_SYMBOL', 'unit020cd744')] = None

ROWS[('HOST_COPY_PARK', 'unit020cd744')] = None

# MEMBER_REDECL d_mg_trmpln2_mario: RETIRED. Main calls Particle::System::New through the
# real header; the out-of-class redeclaration is gone.
ROWS[('MEMBER_REDECL', 'd_mg_trmpln2_mario')] = None

# MWCC_SPELLING d_mg3d_heyho_obj_adapter: RETIRED. Main's Heyho::CheckHitA/B no longer read
# the Mario objects' vtable words through plain C pointer types (no `VT` typedef, no f0..f2
# struct); the calls are typed members.
ROWS[('MWCC_SPELLING', 'd_mg3d_heyho_obj_adapter')] = None

# EXTERN_C_DATA d_mg3d_heyho_obj_adapter: re-anchored. Main spells the global
# `extern fBase_c *gCurrentScene;` at C++ linkage; the arm9 mount (hal/actor_vtables.cpp)
# defines the C name. Same fix as before on main's text.
ROWS[('EXTERN_C_DATA', 'd_mg3d_heyho_obj_adapter')] = [
    ('/* The running scene. */\nextern fBase_c *gCurrentScene;\n', '/* The running scene. */\nextern "C" fBase_c *gCurrentScene;  /* hostgen EXTERN_C_DATA */\n'),
]

# LEDGER_PARK d_s_mg_d3d_base: re-anchored. port/faces_sync.txt still carries the F row for
# dScMgD3DBase_c::OnGraphCallback2 (the face defines the member and forwards to the flat body in
# port/unmatched/MgD3DBase_DeclConflict.cpp), so the TU's body stays parked. Main dropped the
# `raw` local and the @symbol line after the body.
ROWS[('LEDGER_PARK', 'd_s_mg_d3d_base')] = [
    ('int dScMgD3DBase_c::OnGraphCallback2()\n{\n    if (dScMgBase_c::OnGraphCallback2() == 0) return 0;\n', '/* hostgen LEDGER_PARK: port/faces_sync.txt already defines this\n   member as a face forwarding to the flat ROM name, so a\n   second definition here is a duplicate symbol. Parked, not deleted.\n   See the LEDGER_PARK table in tools/hostgen.py. */\n#if 0\nint dScMgD3DBase_c::OnGraphCallback2()\n{\n    if (dScMgBase_c::OnGraphCallback2() == 0) return 0;\n'),
    ('    return 1;\n}\n\n/* Slot 25. */\nint dScMgD3DBase_c::OnGraphCallback3()\n', '    return 1;\n}\n#endif  /* hostgen LEDGER_PARK: dScMgD3DBase_c::OnGraphCallback2 */\n\n/* Slot 25. */\nint dScMgD3DBase_c::OnGraphCallback3()\n'),
]

# MWCC_SPELLING d_s_mg_base: main's three router namespaces (s20af770, s20afb20, s20aff38) each
# forward-declare `struct OamAttr;` inside their extern "C" block. NS_C_ALIAS (this TU is on it)
# renames every name such a block declares, so OamAttr becomes OamAttr__hg_<ns> by #define and the
# bodies' `(::OamAttr *)` casts stop naming a type (C2039). The declaration is dropped; the
# elaborated `struct OamAttr *` parameters then find the global type from OamAttr.h.
ROWS[('MWCC_SPELLING', 'd_s_mg_base')] = [
    ('namespace s20af770 {\nextern "C" {\nextern "C" {\nstruct OamAttr;\n}\n', 'namespace s20af770 {\nextern "C" {\nextern "C" {\n/* hostgen MWCC_SPELLING: namespace-local `struct OamAttr;` dropped. NS_C_ALIAS would\n   rename it and break the body\'s ::OamAttr casts; include/OamAttr.h declares the same type. */\n}\n'),
    ('namespace s20afb20 {\nextern "C" {\nextern "C" {\nstruct OamAttr;\n}\n', 'namespace s20afb20 {\nextern "C" {\nextern "C" {\n/* hostgen MWCC_SPELLING: namespace-local `struct OamAttr;` dropped. NS_C_ALIAS would\n   rename it and break the body\'s ::OamAttr casts; include/OamAttr.h declares the same type. */\n}\n'),
    ('namespace s20aff38 {\nextern "C" {\nextern "C" {\nstruct OamAttr;\nextern int sMgTopScreenYOffset;\n', 'namespace s20aff38 {\nextern "C" {\nextern "C" {\n/* hostgen MWCC_SPELLING: namespace-local `struct OamAttr;` dropped. NS_C_ALIAS would\n   rename it and break the body\'s ::OamAttr casts; include/OamAttr.h declares the same type. */\nextern int sMgTopScreenYOffset;\n'),
]

# HOST_COPY_PARK d_s_mg_base: the five screen-gap routers, re-derived on main's text. Each body
# is renamed to its __rom name (hal/screen_gap.cpp's wrapper owns the real name and calls it)
# and its OAM::Render calls go to the routed shim, exactly the portsync2 transformation; main
# now reads mDrawSubScreen through D3DBaseView and calls OAM::Render qualified.
ROWS[('HOST_COPY_PARK', 'd_s_mg_base')] = [
    ('void Mg_RenderSpriteBoth(void *a0, int a1, int a2, int a3, int a4) {\n    dScMgBase_c *scene = gMgScene;\n    if (scene == 0)\n        return;\n    if (scene->mMenuOpen == 0) {\n        if (scene->GetDisplayMode() == 2) {\n            if (((D3DBaseView *)gMgScene)->mDrawSubScreen == 1) {\n                OAM::Render(0, (OamAttr *)a0, a1, a2 + 0xc0 + sMgTopScreenYOffset, a3, a4, 0x1000,\n                            0x1000, 0, -1);\n                return;\n            }\n            OAM::Render(0, (OamAttr *)a0, a1, a2, a3, a4, 0x1000, 0x1000, 0, -1);\n            return;\n        }\n    }\n    OAM::Render(0, (OamAttr *)a0, a1, a2 + 0xc0 + sMgTopScreenYOffset, a3, a4, 0x1000, 0x1000, 0,\n                -1);\n    OAM::RenderSub((OamAttr *)a0, a1, a2, a3, a4);\n}\n', 'void *port_oam_render_routed_10(int, void *, int, int, int, int, int, int, int, int);  /* hostgen: hal/screen_gap.cpp shim */\nvoid func_ov004_020afdd0__rom(void *a0, int a1, int a2, int a3, int a4) {\n    dScMgBase_c *scene = gMgScene;\n    if (scene == 0)\n        return;\n    if (scene->mMenuOpen == 0) {\n        if (scene->GetDisplayMode() == 2) {\n            if (((D3DBaseView *)gMgScene)->mDrawSubScreen == 1) {\n                port_oam_render_routed_10(0, (OamAttr *)a0, a1, a2 + 0xc0 + sMgTopScreenYOffset, a3, a4, 0x1000,\n                            0x1000, 0, -1);\n                return;\n            }\n            port_oam_render_routed_10(0, (OamAttr *)a0, a1, a2, a3, a4, 0x1000, 0x1000, 0, -1);\n            return;\n        }\n    }\n    port_oam_render_routed_10(0, (OamAttr *)a0, a1, a2 + 0xc0 + sMgTopScreenYOffset, a3, a4, 0x1000, 0x1000, 0,\n                -1);\n    OAM::RenderSub((OamAttr *)a0, a1, a2, a3, a4);\n}\n'),
    ('extern "C" int Mg_RenderSpriteBothScaled(struct OamAttr *a0, int a1, int a2, int a3, int a4,\n                                         fx32 a5, int a6) {\n    dScMgBase_c *scene = gMgScene;\n    if (scene == 0)\n        return 0;\n    if (scene->mMenuOpen == 0 && scene->GetDisplayMode() == 2) {\n        if (((D3DBaseView *)gMgScene)->mDrawSubScreen == 1) {\n            int hp = sMgTopScreenYOffset;\n            if (a2 >= -0x100 - hp && a2 < -hp) {\n                return OAM::Render(0, (::OamAttr *)a0, a1, a2 + 0xc0 + hp, a3, a4, a5, a6);\n            }\n            if (a2 >= -0x40 && a2 < 0xc0) {\n                return OAM::Render(0, (::OamAttr *)a0, a1, a2, a3, a4, a5, a6);\n            }\n        }\n    } else {\n        int hp = sMgTopScreenYOffset;\n        if (a2 >= -0x100 - hp && a2 < -hp) {\n            return OAM::Render(0, (::OamAttr *)a0, a1, a2 + 0xc0 + hp, a3, a4, a5, a6);\n        }\n        if (a2 >= -0x40 && a2 < 0xc0) {\n            return OAM::Render(1, (::OamAttr *)a0, a1, a2, a3, a4, a5, a6);\n        }\n    }\n    return 0;\n}\n', 'extern "C" void *port_oam_render_routed_8(int, void *, int, int, int, int, int, int);  /* hostgen: hal/screen_gap.cpp shim */\nextern "C" int func_ov004_020aff38__rom(struct OamAttr *a0, int a1, int a2, int a3, int a4,\n                                         fx32 a5, int a6) {\n    dScMgBase_c *scene = gMgScene;\n    if (scene == 0)\n        return 0;\n    if (scene->mMenuOpen == 0 && scene->GetDisplayMode() == 2) {\n        if (((D3DBaseView *)gMgScene)->mDrawSubScreen == 1) {\n            int hp = sMgTopScreenYOffset;\n            if (a2 >= -0x100 - hp && a2 < -hp) {\n                return (int)(size_t)port_oam_render_routed_8(0, (::OamAttr *)a0, a1, a2 + 0xc0 + hp, a3, a4, a5, a6);\n            }\n            if (a2 >= -0x40 && a2 < 0xc0) {\n                return (int)(size_t)port_oam_render_routed_8(0, (::OamAttr *)a0, a1, a2, a3, a4, a5, a6);\n            }\n        }\n    } else {\n        int hp = sMgTopScreenYOffset;\n        if (a2 >= -0x100 - hp && a2 < -hp) {\n            return (int)(size_t)port_oam_render_routed_8(0, (::OamAttr *)a0, a1, a2 + 0xc0 + hp, a3, a4, a5, a6);\n        }\n        if (a2 >= -0x40 && a2 < 0xc0) {\n            return (int)(size_t)port_oam_render_routed_8(1, (::OamAttr *)a0, a1, a2, a3, a4, a5, a6);\n        }\n    }\n    return 0;\n}\n'),
    ('void RenderOamBothScreens(void *attr, int x, int y, int palette, int priority, void *mtx) {\n    dScMgBase_c *scene = gMgScene;\n    if (scene == 0)\n        return;\n    if (scene->mMenuOpen == 0) {\n        if (scene->GetDisplayMode() == 2) {\n            if (((D3DBaseView *)gMgScene)->mDrawSubScreen == 1) {\n                OAM::Render(false, (OamAttr *)attr, x, y + 0xc0 + sMgTopScreenYOffset, palette,\n                            priority, (Matrix2x2 *)mtx);\n                return;\n            }\n            OAM::Render(false, (OamAttr *)attr, x, y, palette, priority, (Matrix2x2 *)mtx);\n            return;\n        }\n    }\n    OAM::Render(false, (OamAttr *)attr, x, y + 0xc0 + sMgTopScreenYOffset, palette, priority,\n                (Matrix2x2 *)mtx);\n    OAM::Render(true, (OamAttr *)attr, x, y, palette, priority, (Matrix2x2 *)mtx);\n}\n', 'void *port_oam_render_routed_mtx(int, void *, int, int, int, int, void *);  /* hostgen: hal/screen_gap.cpp shim */\nvoid RenderOamBothScreens__rom(void *attr, int x, int y, int palette, int priority, void *mtx) {\n    dScMgBase_c *scene = gMgScene;\n    if (scene == 0)\n        return;\n    if (scene->mMenuOpen == 0) {\n        if (scene->GetDisplayMode() == 2) {\n            if (((D3DBaseView *)gMgScene)->mDrawSubScreen == 1) {\n                port_oam_render_routed_mtx(false, (OamAttr *)attr, x, y + 0xc0 + sMgTopScreenYOffset, palette,\n                            priority, (Matrix2x2 *)mtx);\n                return;\n            }\n            port_oam_render_routed_mtx(false, (OamAttr *)attr, x, y, palette, priority, (Matrix2x2 *)mtx);\n            return;\n        }\n    }\n    port_oam_render_routed_mtx(false, (OamAttr *)attr, x, y + 0xc0 + sMgTopScreenYOffset, palette, priority,\n                (Matrix2x2 *)mtx);\n    port_oam_render_routed_mtx(true, (OamAttr *)attr, x, y, palette, priority, (Matrix2x2 *)mtx);\n}\n'),
    ('void Mg_RenderSpriteBothPalMtx(void *a0, int a1, int a2, int a3, void *a4) {\n    dScMgBase_c *scene = gMgScene;\n    if (scene == 0)\n        return;\n    if (scene->mMenuOpen == 0) {\n        if (scene->GetDisplayMode() == 2) {\n            if (((D3DBaseView *)gMgScene)->mDrawSubScreen == 1) {\n                OAM::Render(0, (OamAttr *)a0, a1, a2 + 0xc0 + sMgTopScreenYOffset, a3, -1,\n                            (Matrix2x2 *)a4);\n                return;\n            }\n            OAM::Render(0, (OamAttr *)a0, a1, a2, a3, -1, (Matrix2x2 *)a4);\n            return;\n        }\n    }\n    OAM::Render(0, (OamAttr *)a0, a1, a2 + 0xc0 + sMgTopScreenYOffset, a3, -1, (Matrix2x2 *)a4);\n    OAM::Render(1, (OamAttr *)a0, a1, a2, a3, -1, (Matrix2x2 *)a4);\n}\n', 'void *port_oam_render_routed_mtx(int, void *, int, int, int, int, void *);  /* hostgen: hal/screen_gap.cpp shim */\nvoid func_ov004_020b023c__rom(void *a0, int a1, int a2, int a3, void *a4) {\n    dScMgBase_c *scene = gMgScene;\n    if (scene == 0)\n        return;\n    if (scene->mMenuOpen == 0) {\n        if (scene->GetDisplayMode() == 2) {\n            if (((D3DBaseView *)gMgScene)->mDrawSubScreen == 1) {\n                port_oam_render_routed_mtx(0, (OamAttr *)a0, a1, a2 + 0xc0 + sMgTopScreenYOffset, a3, -1,\n                            (Matrix2x2 *)a4);\n                return;\n            }\n            port_oam_render_routed_mtx(0, (OamAttr *)a0, a1, a2, a3, -1, (Matrix2x2 *)a4);\n            return;\n        }\n    }\n    port_oam_render_routed_mtx(0, (OamAttr *)a0, a1, a2 + 0xc0 + sMgTopScreenYOffset, a3, -1, (Matrix2x2 *)a4);\n    port_oam_render_routed_mtx(1, (OamAttr *)a0, a1, a2, a3, -1, (Matrix2x2 *)a4);\n}\n'),
    ('void Mg_RenderSpriteBothMtx(void *a0, int a1, int a2, void *a3) {\n    dScMgBase_c *scene = gMgScene;\n    if (scene == 0)\n        return;\n    if (scene->mMenuOpen == 0) {\n        if (scene->GetDisplayMode() == 2) {\n            if (((D3DBaseView *)gMgScene)->mDrawSubScreen == 1) {\n                OAM::Render(0, (OamAttr *)a0, a1, a2 + 0xc0 + sMgTopScreenYOffset, -1, -1,\n                            (Matrix2x2 *)a3);\n                return;\n            }\n            OAM::Render(0, (OamAttr *)a0, a1, a2, -1, -1, (Matrix2x2 *)a3);\n            return;\n        }\n    }\n    OAM::Render(0, (OamAttr *)a0, a1, a2 + 0xc0 + sMgTopScreenYOffset, -1, -1, (Matrix2x2 *)a3);\n    OAM::Render(1, (OamAttr *)a0, a1, a2, -1, -1, (Matrix2x2 *)a3);\n}\n', 'void *port_oam_render_routed_mtx(int, void *, int, int, int, int, void *);  /* hostgen: hal/screen_gap.cpp shim */\nvoid func_ov004_020b0380__rom(void *a0, int a1, int a2, void *a3) {\n    dScMgBase_c *scene = gMgScene;\n    if (scene == 0)\n        return;\n    if (scene->mMenuOpen == 0) {\n        if (scene->GetDisplayMode() == 2) {\n            if (((D3DBaseView *)gMgScene)->mDrawSubScreen == 1) {\n                port_oam_render_routed_mtx(0, (OamAttr *)a0, a1, a2 + 0xc0 + sMgTopScreenYOffset, -1, -1,\n                            (Matrix2x2 *)a3);\n                return;\n            }\n            port_oam_render_routed_mtx(0, (OamAttr *)a0, a1, a2, -1, -1, (Matrix2x2 *)a3);\n            return;\n        }\n    }\n    port_oam_render_routed_mtx(0, (OamAttr *)a0, a1, a2 + 0xc0 + sMgTopScreenYOffset, -1, -1, (Matrix2x2 *)a3);\n    port_oam_render_routed_mtx(1, (OamAttr *)a0, a1, a2, -1, -1, (Matrix2x2 *)a3);\n}\n'),
]

# ---- lane SYM4 (catchup1 Stage C round 2): the per-player arrays stay port-sized
# Main's d_s_stage_methods.cpp defines the stage globals itself now, and the
# decomp TU's copy wins (mount_reconcile, hal/auto_bss.cpp retired its other
# stand-ins). Five of them the port widens on purpose to its body-slot count
# (hal/auto_bss.cpp, hal/cxxname_bridge.cpp): a four-seat ROM array would be
# written past its end by every seat from four up. The TU keeps them as
# declarations so the one storage is the port's widened one.
ROWS[("MWCC_SPELLING", "d_s_stage_methods")] = [
    ("s8 gNumRedCoins[4];\n",
     "extern s8 gNumRedCoins[];  /* SYM4: hosted widened in hal/auto_bss.cpp */\n"),
    ("void *data_0209f33c;\n",
     "extern void *data_0209f33c;  /* SYM4: hosted widened in hal/auto_bss.cpp */\n"),
    ("u8 data_0209f350[4];\n",
     "extern u8 data_0209f350[];  /* SYM4: hosted widened in hal/auto_bss.cpp */\n"),
    ("daPly_c *gPlayers[4];\n",
     "extern daPly_c *gPlayers[];  /* SYM4: hosted widened in hal/cxxname_bridge.cpp */\n"),
    ("ControllerInput data_0209f498[4];\n",
     "extern ControllerInput data_0209f498[];  /* SYM4: hosted widened in hal/auto_bss.cpp */\n"),
]
