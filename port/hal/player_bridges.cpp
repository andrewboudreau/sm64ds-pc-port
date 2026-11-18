// Gate-10 method bridges: C-named references -> MSVC method definitions.
//
// The Player closure's callers reference these at C linkage while the
// defining TUs compile them as real methods against the shared headers.
// Same hop as gate 9 (cxxname_bridge.cpp), split into its own TU because
// Player.h drags a wider include surface than the gate-9 file wants.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "dExtFrameCtrl_c.h"
#include "dBgCh.h"
#include "nnsys/fnd.h"   /* decomp main 63e1097b379: NNS_Fnd list calls replace NestedHeapIterator */
#include "daPly_c.h"
#include "player_fields.h"   /* run mg16 lane MP4: the one place field offsets live */
#include "host_settings.h"   /* the host settings readers */
#include "comms_seam.h"      /* port::sync_stats(): the local-write witness */
#include "dExtShadowModel_c.h"
#include "dExtAnmTexPat_c.h"
#include "Heap.h"
#include "dExtAnmModel_c.h"  /* was ModelAnim.h (decomp main renamed the class) */

#include "dsstate_seg.h"
/* SYNC4: see the case for 0x020e0d28 inside player_states.inc. */
extern "C" int __fastcall port_player_st_crazedcrate_cleanup(void *self, void *);
#pragma comment(linker, "/alternatename:@port_player_st_crazedcrate_cleanup@8=?St_CrazedCrate_Cleanup@daPly_c@@QAEHXZ")


/* how many times hal_call_state_fn fell off the end of its switch this run --
   read by the F3 overlay in port/tests/walk_window.cpp */
extern "C" unsigned g_port_unhosted_hits = 0;

extern "C" unsigned int _ZNK7daPly_c14GetBodyModelIDEjb(char *, unsigned int, char);
extern "C" unsigned _ZN7daPly_c19func_ov002_020becf4Eii(char *self, unsigned j, int b);
extern "C" int _ZN7daPly_c19func_ov002_020d225cEv(char *o);   /* src/_ZN7daPly_c19func_ov002_020d225cEv.c */
extern "C" void func_ov002_020e3e00(void *obj, const void *vec,
                                    unsigned opacity);  /* src/func_ov002_020e3e00.cpp */
extern "C" int _ZN7daPly_c13InitResourcesEv(void *);

/* C++-linkage globals some slice TUs call under Itanium-style names */
int _ZNK15dExtFrameCtrl_c12WillHitFrameEi(void *self, int f)
{ return ((dExtFrameCtrl_c *)self)->dExtFrameCtrl_c::WillHitFrame(f) ? 1 : 0; }
/* GetFlags is C linkage now: main's mangled-declaration sweep gave the Player
   state TUs an extern "C" declaration, so they call the plain name. The alias
   at the head of hal/cxx_aliases.cpp still serves the old C++ mangling. */
extern "C" int _ZN15dExtFrameCtrl_c8GetFlagsEv(void *self)
{ return ((dExtFrameCtrl_c *)self)->dExtFrameCtrl_c::GetFlags(); }
void _ZN7daPly_c4HealEi(daPly_c *p, int amt)
{ p->daPly_c::Heal(amt); }

/* RETIRED at SYNC6: main's #2666 put src/game/player/Player.cpp back in the build
   and it defines ?GetBodyModelID@daPly_c@@QBEII_N@Z itself, so this face was
   the second definition (one LNK2005 row against player_bridges.cpp.obj in
   build_s2.log). The flat body it forwarded to is unchanged.
unsigned int daPly_c::GetBodyModelID(unsigned int a, bool b_) const
{ return _ZNK7daPly_c14GetBodyModelIDEjb((char *)this, a, b_ ? 1 : 0); }      */

/* BALLOON2: THE POWER-UP MODELS A CASTLE FLOOR NEVER BUILDS.
   _ZN7daPly_c19func_ov002_020e5948Ev (src/game/player/Player.cpp, the SetFiles arm) builds
   the balloon models (+0xEC body, +0xF0 balloon, +0x174 wings) only when the
   course is NOT 2, 4 or 5 (the castle floors) and the game is not VS. A Power
   Flower still lies on the castle 2F (four persistent ones for Luigi's vanish
   cap), and a Mario standing near one when it is picked up takes the Mario arm
   of daObjPowerUpItem_c::CheckPickup: InitBalloonMario / InitWingFeathers turn
   the flag on and daPly_c::Behavior (_ZN7daPly_c19func_ov002_020e4768Ev -> UpdateBones on the
   body model the balloon selects) then reads the model that was never built
   and faults at address 0x14. The DS never reaches that read in a retail
   castle; the port can (a character switched after the course loaded, a second
   player nearest), so the models are built here, once, with the very calls and
   files the ROM's own arm makes for every other course, before the first
   Behavior of a player that lacks them. A castle that never uses them draws
   and runs exactly as before. The matrices the ROM seats later are untouched. */
extern "C" { static void port_player_power_models(char *c); }

extern "C" {
/* gate-10 smoke drives the state machine directly (the ChangeState PMF
   dispatch reads DS-baked member-fn bytes MSVC cannot represent) */
int hal_player_init_resources(void *p)
{ return _ZN7daPly_c13InitResourcesEv(p); }
int hal_player_st_wait_init(void *p)
{ return ((daPly_c *)p)->daPly_c::St_Wait_Init(); }
int hal_player_st_wait_main(void *p)
{ return ((daPly_c *)p)->daPly_c::St_Wait_Main(); }
int hal_player_st_walk_init(void *p)
{ return ((daPly_c *)p)->daPly_c::St_Walk_Init(); }
int hal_player_st_walk_main(void *p)
{ return ((daPly_c *)p)->daPly_c::St_Walk_Main(); }
int hal_player_behavior(void *p)
{ port_player_power_models((char *)p); return ((daPly_c *)p)->daPly_c::Behavior(); }
/* the walk demo renders the Player's current body ModelAnim in place:
   identity model matrix, bones posed from the anim Behavior advanced */
/* level model render for the window: identity world matrix (stage models
   are authored in world space; the KCL shares it) */
/* SM64DS_TEX_LOG=1: the BMD's own texture/palette/material tables next to
   the runtime material records _Z13func_020462d0P3DstP4DescP5ElemA built from them -- the
   ground truth a [texbind] line is checked against. One shot per file. */
extern "C" int hal_tex_log(void);
static void hal_dump_model_tables(dExtSimpleModel_c *m)
{
    static BMD_File *done[16];
    static int ndone;
    /* dExtModel_c::modelFile is only filled by LoadAndSetFile; the harness
       drives SetFile directly, so read the file the components carry. */
    BMD_File *f = m->data.modelFile;
    if (!f) return;
    for (int i = 0; i < ndone; ++i)
        if (done[i] == f) return;
    if (ndone < 16) done[ndone++] = f;

    printf("[texmodel] file=%p tex=%u pal=%u mat=%u\n", (void *)f,
           f->numTextures, f->numPalettes, f->numMaterials);
    for (u32 i = 0; i < f->numTextures; ++i) {
        BMD_Texture *t = f->textures + i;
        printf("  tex %2u %-20s flags %08x fmt %u %3dx%-3d size %5u "
               "vramoff %05x\n",
               i, (const char *)t->name, t->flags, (t->flags >> 26) & 7,
               8 << ((t->flags >> 20) & 7), 8 << ((t->flags >> 23) & 7),
               t->size, (t->flags & 0xffff) << 3);
    }
    for (u32 i = 0; i < f->numPalettes; ++i) {
        BMD_Palette *p = f->palettes + i;
        printf("  pal %2u %-20s size %5u vramoff %05x -> pltt %04x\n", i,
               (const char *)p->name, p->size, p->vramOffset,
               p->vramOffset >> 4);
    }
    const unsigned char *mm = (const unsigned char *)m->data.materials;
    for (u32 i = 0; i < f->numMaterials && mm; ++i) {
        const u32 *e = (const u32 *)(mm + i * 0x30);
        printf("  mat %2u %-20s tex %3d pal %3d teximage %08x pltt %04x "
               "attr %08x difamb %08x\n",
               i, (const char *)f->materials[i].name, (int)e[0], (int)e[1],
               e[7], e[8], e[9], e[10]);
    }
}

/* Stage::RenderModel's own scale argument, {125.0, 125.0, 125.0} fx32.
   Emitted by port/tools/romdata.py from the arm9 image. */
extern unsigned char data_020755d4[];

void hal_render_model(void *model, int scaleShift)
{
    dExtSimpleModel_c *m = (dExtSimpleModel_c *)model;
    if (hal_tex_log()) hal_dump_model_tables(m);
    /* THE STAGE RENDERS IN SCENE UNITS, and this is Stage::RenderModel's own
       shape: the model matrix is the Model constructor's identity
       (IDENTITY_MATRIX4X3) and the entire scale travels through dExtSimpleModel_c::Render's
       Vector3 argument -- data_020755d4 -- which the ordinary part walk
       spends as an MTX_SCALE on top of its own 1 << (shift + 12). Scene is
       what everything else in the frame is now in: the view matrix
       Camera::Render hands LookAt_ as (v + 4) >> 3, the model matrices the
       actors' own Render methods fill, and the positions Actor::BeforeBehavior
       clips with. The old world-unit matrix (0x1000 << (shift + 10), x40/41)
       and the R6 view shim that paid for it are both gone.

       NOTHING IS CORRECTED HERE ANY MORE. The old world matrix carried a
       measured x40/41, and that 2.5% was the residue of forcing a power of two
       (0x1000 << (shift + 10) = x2048) onto a scale the ROM writes as 125.0.
       With the part walk's own MTX_SCALE reaching the geometry engine again
       (the hostgen MMIO_PTR hole, port/tools/hostgen.py) the ROM's vector is
       exact: the stage comes out x[-1000..1000] y[-225..953.5] scene, which is
       x[-8000..8000] y[-1799.8..7627.9] world, and the castle grounds' KCL --
       the same terrain, in the world units the Player and the object table use
       -- puts its lowest vertex at -1800.0 and its outer wall at +-8500.
       SM64DS_LEVEL_SCALE=N overrides the whole vector for the A/B. */
    /* This runs per model per frame, and MSVC's getenv scans the whole
       environment block, so both overrides are latched on first use. */
    int s = *(const int *)data_020755d4;
    static int scale_override = 0, scale_value = 0;
    if (!scale_override) {
        const char *e = std::getenv("SM64DS_LEVEL_SCALE");
        scale_override = e ? 1 : -1;
        if (e) scale_value = std::atoi(e);
    }
    if (scale_override > 0) s = scale_value;
    static int probe = -1;
    if (probe < 0) probe = std::getenv("SM64DS_MODEL_PROBE") ? 1 : 0;
    if (probe) {
        const BMD_File *f2 = m->data.modelFile;
        const int *t = (const int *)m->data.transforms;
        int lo[3] = {1 << 30, 1 << 30, 1 << 30},
            hi[3] = {-(1 << 30), -(1 << 30), -(1 << 30)};
        unsigned nb = f2 ? f2->numBones : 0;
        for (unsigned i = 0; i < nb && t; ++i)
            for (int k = 0; k < 3; ++k) {
                int v = t[i * 12 + 9 + k];
                if (v < lo[k]) lo[k] = v;
                if (v > hi[k]) hi[k] = v;
            }
        std::printf("[mprobe] shift %u (arg %d) bones %u scale %d (%.3f) "
                    "part.t x[%.3f..%.3f] y[%.3f..%.3f] z[%.3f..%.3f]\n",
                    f2 ? f2->scaleShift : 0u, scaleShift, nb, s, s / 4096.0,
                    lo[0] / 4096.0, hi[0] / 4096.0, lo[1] / 4096.0,
                    hi[1] / 4096.0, lo[2] / 4096.0, hi[2] / 4096.0);
    }
    Vector3 scale = {s, s, s};
    for (int i = 0; i < 12; ++i) ((int *)&m->mat4x3)[i] = 0;
    ((int *)&m->mat4x3)[0] = 0x1000;
    ((int *)&m->mat4x3)[4] = 0x1000;
    ((int *)&m->mat4x3)[8] = 0x1000;
    m->dExtSimpleModel_c::Render(&scale);
}

void hal_render_player_body_ex(void *player, int with_head);
void hal_render_player_body(void *player)
{ hal_render_player_body_ex(player, 1); }
void hal_render_player_body_only(void *player)
{ hal_render_player_body_ex(player, 0); }
/* the three matrix helpers and the scratch matrix daPly_c::Render uses to seat
   mModelAnim4 (+0x174); all four are matched and already in the link */
extern "C" {
void Matrix4x3_FromTranslation(void *m, int x, int y, int z);
void Matrix4x3_ApplyInPlaceToRotationZ(void *m, int ang);
void MTX_Concat43(const void *a, const void *b, void *out);
extern int data_020a0e68;
/* base dExtSimpleModel_c::Render, non-virtually -- the exact call daPly_c::Render's i==3 arm
   makes on the Yoshi head. C-linkage face over the matched
   src/_ZN5Model6RenderEPK7Vector3.cpp, so the dispatch is non-virtual as in the
   ROM: index 3 is a ModelAnim, and going through its own vtable Render slot
   would re-run UpdateVerts, which is why daPly_c::Render reaches past it to the
   base method and renders the head at the pose the anim system already left. */
void _ZN17dExtSimpleModel_c6RenderEPK7Vector3(void *self, const void *pos);
}

/* THE HEAD-MODEL GROUP, daPly_c::Render's second draw. daPly_c::Render walks one
   model out of the array at Player +0x154, indexed by
   _ZN7daPly_c19func_ov002_020becf4Eii(this, unk_6db, 1), and forks on that index
   (src/_ZN6Player6RenderEv.cpp lines 120-128):

       if (i == 3) {                                    // Yoshi's head
           dExtSimpleModel_c::Render(mdl4, this+0x80);              // base, non-virtual, NO seat
       } else {                                         // Mario/Luigi/Wario heads
           *(M34*)mdl4->bones[0] = *(M34*)(bodyBones + 0x2d0);  // neck bone
           mdl4->Virtual10(&mScale);
       }

   The array is seated in _ZN7daPly_c19func_ov002_020e5948Ev (InitResources): the four
   character heads load into indices 0-3 from data_ov002_0210a69c, and index 3
   -- Yoshi -- is a ModelAnim (0x64 bytes, a base BCA anim) while 0-2 are plain
   Models (0x50). _ZN7daPly_c19func_ov002_020becf4Eii returns 3 for Yoshi in normal play (its
   v==3 branch adds 4 only when unk_714, "something in the mouth", is set), so
   i==3 is Yoshi's EVERY-FRAME head index, not a tongue-only case. That is why
   it is special-cased at all: Mario/Luigi/Wario wear a separate cap model that
   has to be pinned to the body's neck bone, but Yoshi's head is a full model
   authored in body space and drawn at the player root.

   THE MATRIX IS NOT SEATED HERE, and that is the whole correction over take 1.
   _ZN7daPly_c19func_ov002_020e444cEv -- which the port already runs every frame inside
   daPly_c::Behavior (_ZN7daPly_c19func_ov002_020e4bb8Ev -> _ZN7daPly_c19func_ov002_020e444cEv, and via
   _ZN7daPly_c19func_ov002_020e3f90Ev when Yoshi is carrying something) -- has already written
   head+0x1c to the player-root matrix by the time render runs. So the i==3 arm
   dispatches base dExtSimpleModel_c::Render with nothing seated, exactly as the ROM does.

   THE PROBE THAT ONCE READ head+0x1c == body+0x1c == scene ON EVERY FRAME WAS
   READING A STANDING PLAYER, and the claim it left here was wrong in motion.
   The seat's matrix carries the X and Z rotations and a model offset that the
   old hand-built `scene` did not; all of them are zero at rest and none of them
   are at speed. That is where Yoshi's head-off-body separation came from, and
   hal_render_player_world now reads the seated matrix instead of rebuilding
   one -- see the banner there for the measurements. head+0x1c and body+0x1c are
   the same write on the ROM, and they are the same value here now.

   Take 1 seated head+0x1c = scene by hand -- redundant, it already was -- and,
   worse, believed i==3 meant "mid-tongue" and so it read as a new render on top
   of the else arm the port still ran; the real fault the human saw (head gone,
   a model upside down under the feet) is the else arm's neck-bone compose being
   run over Yoshi's index-3 head, which is not neck-parented. Routing i==3 to the
   ROM's own base dExtSimpleModel_c::Render, and leaving every other head on the neck-bone
   compose, is the fix.

   SCOPE: this draws Yoshi's HEAD in the right place every frame. It does not on
   its own make the tongue reach: the head ModelAnim's currFrame does not advance
   in the port (St_YoshiPower_Main's Animation::Advance on +0x160 leaves it at 0
   here), so the mouth/tongue animation stays at frame 0. Extending the tongue is
   a separate open gap (the head anim never being advanced/posed), not this fork. */
static void hal_render_head_group(char *c, char *head, unsigned hid,
                                  dExtAnmModel_c *ma, const int *scene)
{
    if (hid == 3) {
        /* Yoshi's head: mat4x3 already seated by _ZN7daPly_c19func_ov002_020e444cEv this frame;
           dispatch the BASE dExtSimpleModel_c::Render (non-virtual) with c+0x80, exactly the
           ROM's i==3 arm. Base, not the object's own slot 4: index 3 is a
           ModelAnim, whose virtual Render would re-run UpdateVerts, and the ROM
           calls dExtSimpleModel_c::Render directly to render the head at the pose the anim
           system already produced. */
        _ZN17dExtSimpleModel_c6RenderEPK7Vector3(head, c + 0x80);
        return;
    }

    /* EVERY OTHER HEAD -- the cap Mario, Luigi and Wario wear, a model of its
       own. src/_ZN6Player6RenderEv.cpp:123-127 is two statements:

           char* m   = mdl4;
           char* dst = *(char**)(m + 0x14);                  // head bones
           char* src = *(char**)(bodyMdl + 0x14) + 0x2d0;    // body neck bone
           *(M34*)dst = *(M34*)src;                          // 1
           ((VObj*)mdl4)->m14((char*)&mScaleX);              // 2

       and the port had a stand-in for the first and had DROPPED THE SECOND'S
       ARGUMENT. &mScaleX is Player mScaleX/Y/Z at +0x080, the same vector the
       body draw and the i==3 arm above are handed; this call passed NULL,
       which _ZN15ModelComponents13func_0204488cEiP7Vector3 reads as unit. Statement 2 is now written as the ROM
       writes it.

       WHAT THAT DOES AND WHAT IT DOES NOT DO, measured, because the two are
       not the same and the difference is the port's stand-in for statement 1.
       dExtSimpleModel_c::Render loads the model's mat4x3 as the world matrix
       (src/_ZN5Model6RenderEPK7Vector3.cpp -> ModelComponents::Render ->
       data_020a4bd0), and _ZN15ModelComponents13func_0204488cEiP7Vector3 then emits, per part and in this
       order (src/func_0204488c.c:68-78):

           G3_LoadMtx43(data_020a4bd0)   GX 0x17  MTX_LOAD_4x3, the world
           *mtxScale = c0/c1/c2           GX 0x1b  MTX_SCALE, THIS argument
           G3_MultMtx43(partMtx)         GX 0x19  MTX_MULT_4x3, the bone

       -- world, then scale, then the bone's whole 4x3. Anything carried in
       bone 0 is INSIDE the scale; anything folded into the world matrix is
       OUTSIDE it. The ROM puts the neck in bone 0. The port composes it
       through `scene` into head+0x1c and leaves bone 0 at (0,0,0).

       So the scale now multiplies the cap's PARTS, which is what the ROM's
       argument does, and it cannot reach the cap's ANCHOR, because bone 0
       holds nothing for MTX_SCALE to multiply. Castle grounds, Mario, level 1,
       SM64DS_FORCE_STATE=squish, head-minus-neck world Y in Fix12 scene units:

           standing            gap      0    before and after
           crushed, NULL       gap  41378    the cap 80.8 world units HIGH
           crushed, &mScaleX   gap  41378    not one Fix12 of it

       THE POSITION IS NOT CLOSED HERE, deliberately. Closing it needs the
       ROM's statement 1 as well: copy the body's neck bone into the head's
       bone 0, drop the compose, and let _ZN7daPly_c19func_ov002_020e444cEv's player-root
       seat of head+0x1c stand as the ROM leaves it. An earlier pass in this
       lane built that and reported the crushed gap reaching 0 -- and also
       reported it moving normal-play framebuffer hashes with mScale at unit,
       which the table above says THIS change does not do. The emission order
       predicts exactly that: through bone 0 the neck's ROTATION goes inside
       MTX_SCALE and a non-uniform scale shears it, and through the world
       matrix it does not. So the relocation is a rendering change, not a
       no-op. It needs its own commit and its own before/after; it is not
       carried here and it is not re-measured here.

       WHAT WAS MEASURED HERE, castle grounds level 1, Mario, the quiet
       spawner, ten normal-play runs (L1 296/300, L5, L6, L9 x Mario/Yoshi):
       every rc 0, every selftest position unchanged, and all ten framebuffer
       hashes BYTE-IDENTICAL to the same runs without this argument. The
       CRUSHED frame is not byte-identical, which is the point: at frame 80 of
       the squish run the two framebuffers differ in 3435 of 589824 pixel
       bytes (1226 of 196608 pixels, 0.62 percent), all of them inside one
       40x42 box at x 236..275, y 209..250 -- the cap. Frames 78 through 82
       all differ by the same order. This change is not an invisible numeric
       delta; what the box looks like is an owner's call, not this comment's.
       Shots: status_shots/cap/mario_crushed_f0{78,80,82}_{before,after}.png */
    /* THE ROM'S STATEMENT 1, src/_ZN6Player6RenderEv.cpp:123-126: the body's
       neck bone is copied INTO the head model's bone 0, and the head model's
       own mat4x3 is left as _ZN7daPly_c19func_ov002_020e444cEv seated it this frame (the
       player root, the same matrix the body draws through). The port used to
       compose neck * scene into head+0x1c instead and leave bone 0 at
       (0,0,0). The two are the same product while mScale is unit; they are
       not the same product under a crush, because _ZN15ModelComponents13func_0204488cEiP7Vector3 emits
       MTX_SCALE between the world matrix and the bone, so a neck carried in
       the world matrix is outside the scale and cannot shrink with the body.
       Measured: crushed gap 41378 on castle grounds and 22714 on the
       opening's Wario before, 0 after. */
    char *hbones = *(char **)(head + 0x14);
    const char *neck = *(const char *const *)((char *)ma + 0x14) + 0x2d0;
    if (hbones && neck)
        for (int i = 0; i < 12; ++i)
            ((int *)hbones)[i] = ((const int *)neck)[i];
    /* Slot 5, not 4: VObj (src/_ZN6Player6RenderEv.cpp's shadow vtable) names
       every slot by byte offset, so m14 is index 5, and _ZTV17dExtSimpleModel_c[5] is
       dExtSimpleModel_c::Render (hal/cxxname_bridge.cpp:522). This read 4 while
       hal_fill_model_vtable dual-filled _ZTV17dExtSimpleModel_c[4] with Render, until
       0a7f12ee9 removed the dual fill and put Virtual10 back at [4]. */
    ((void(__fastcall *)(void *, void *, const void *))(
        ((void ***)head)[0][5]))(head, 0, c + 0x80);
}

/* run mg16 lane MP3: the two globals daPly_c::Render's own gates read. The
   per-slot Player table and the local-player index are read by the render
   pass below (declared here so hal_render_player_world can tell a remote
   body from the local one; also declared later at their first other use). */
extern "C" {
extern unsigned char gGameMode;   /* VS mode flag */
extern unsigned char data_0209fc5c[]; /* per-slot "this slot is live"; BYTE
                                         stride, the ROM's own width */
extern void *gPlayers[];         /* per-slot daPly_c* */
extern unsigned char gLocalPlayerId;   /* local player index */
}



/* dExtModel_c::ApplyOpacity, the ROM's own one-argument body (method_faces.cpp
   forwards to the matched src/_ZN9ModelBase12ApplyOpacityEj.cpp). It walks the
   model's components and stamps the alpha into each material, the same call
   daPly_c::Render makes on the wings. Re-applied every frame because the
   material flags are rebuilt by the per-frame update. */
extern "C" void _ZN11dExtModel_c12ApplyOpacityEjj(void *self, unsigned a);






extern signed char gCurrentSublevel;


/* TEMP faithful eat/egg-lay reproduction driver (SM64DS_YOSHI_EGG_REPRO=<frame>).
   The headless harness cannot turn Yoshi to face a wandering enemy, so a natural
   tongue-catch is not scriptable. This forces the SAME grab the tongue-collision
   handler performs (func_ov002_020d6998 sets mObjInMouth and the enemy's grabbed
   bit), then hands control to the REAL St_Swallow egg-lay path via ChangeState.
   Everything downstream -- _ZN7daPly_c18DisableInteractionEv, anim 0x70, FinishedAnim,
   _Z18daYegg_c_classInitv -- runs unchanged, so the freeze it exposes is the game's, not
   the driver's. Inert unless the env is set. */
extern "C" {
extern void *port_first_live_actor_of_class(unsigned id);   /* hal/actor_registry.cpp */
extern unsigned port_unique_id_of_actor(void *actor);       /* hal/actor_registry.cpp */
/* the ROM's own cap-pickup entry point; src/game/player/Player.cpp,
   called by the cap actor at src/game/actors/d_a_obj_mario_cap.cpp:51. Used only by the
   SM64DS_YOSHI_CAP repro driver below. */
extern void _ZN7daPly_c18SetNewHatCharacterEjjb(void *self, unsigned a, unsigned b,
                                               bool c);
}

extern "C" void port_adventure_presence_probe(int frame)
{
    static int on = -1;
    if (on < 0) on = std::getenv("SM64DS_ADVENTURE_PRESENCE") ? 1 : 0;
    if (!on) return;
    static bool done = false;
    const int kAt = 100;
    if (frame != kAt || done) return;
    done = true;

    const int me = (int)gLocalPlayerId;
    const int mylev = (int)gCurrentSublevel;
    const int otherlev = mylev + 7;     /* an id that is not ours */

    /* the first two REMOTE live bodies -- the probe's pair */
    int sA = -1, sB = -1;

    int fails = 0;
    const bool not_vs = gGameMode == 0;
    bool visA_same = false, visB_diff = false, drawnA = false, frozenB = false;
    bool visA_after = false, visB_after = false, frozenA2 = false, drawnB2 = false;
    bool local_untouched = false;


    ++fails;
    if (!not_vs) ++fails;
    ++fails;
    if (!visA_same) ++fails;
    if (!visB_diff) ++fails;
    if (!drawnA) ++fails;
    if (!frozenB) ++fails;
    if (!visA_after) ++fails;
    if (!visB_after) ++fails;
    if (!frozenA2) ++fails;
    if (!drawnB2) ++fails;
    if (!local_untouched) ++fails;
}


/* ---- THE ROM'S PER-FRAME TEXTURE-SEQUENCE UPDATES --------------------------
   daPly_c::Render (src/_ZN6Player6RenderEv.cpp:80-83, 129-135) runs three
   dExtAnmTexPat_c::Update calls every frame, and this port's replacement
   render ran none of them, so every material a BTP drives stayed at its
   BMD-authored default forever. For Luigi that default is the WRONG variant:
   luigi_head_cap.bmd's head material is authored pointing at 'luigi_head_2'
   -- the vanish-power stipple -- and only the every-frame Update pulls it
   back to 'luigi_head_1' (L_tx_headtrans.btp frame 0). His body file is
   authored the other way round (default 'luigi_body_tr_1', the normal skin),
   which is why the body looked right while the head rendered see-through
   from the port's first boot. Measured: [btpprep] resolves the trans pairs at
   boot, and no [btp] material patch ever ran before this block existed.

   The three updates, the ROM's own gates and order:
     after the body render,  param1==1 (Luigi):  seq +0x254 -> body model
       (+0xe0, slot 1)'s components, then currFrame(+0x25c) = unk_6fc<<12
       (unk_6fc is the vanish-power progress; 0 in normal play = frame 0 =
       the normal texture)
     after the head render,  param1==1 (Luigi):  seq +0x268 -> head-with-cap
       (+0x158)'s components, then currFrame(+0x270) = unk_6fc<<12
     after the head render,  face timer +0x73c == 0 (_ZN7daPly_c19func_ov002_020bea7cEv):
       the per-character face/blink seq (+0x1dc + char*0x14) -> the CURRENT
       head model's components (the eye-blink everyone gets, all characters)

   The null guards are port hygiene only (the ROM would crash there too); the
   gates and destinations are the matched TU's, byte for byte of intent. */
extern "C" void _ZN15dExtAnmTexPat_c6UpdateER15ModelComponents(void *seq,
                                                               void *mc);
extern "C" int _ZN7daPly_c19func_ov002_020bea7cEv(char *self);
extern "C" int port_player_render_hidden(const void *player);  /* below, hoisted */

static void hal_player_texseq_body(char *c)
{
    if (*(int *)(c + 0x8) != 1) return;            /* param1: Luigi only */
    char *mdl = *(char **)(c + 0xe0);              /* body model, slot 1 */
    if (mdl)
        _ZN15dExtAnmTexPat_c6UpdateER15ModelComponents(c + 0x254, mdl + 8);
    *(int *)(c + 0x25c) = (int)*(unsigned char *)(c + 0x6fc) << 12;
}

static void hal_player_texseq_head(char *c, unsigned hid)
{
    if (*(int *)(c + 0x8) == 1) {                  /* param1: Luigi only */
        char *mdl = *(char **)(c + 0x158);         /* head with cap */
        if (mdl)
            _ZN15dExtAnmTexPat_c6UpdateER15ModelComponents(c + 0x268, mdl + 8);
        *(int *)(c + 0x270) = (int)*(unsigned char *)(c + 0x6fc) << 12;
    }
    if (_ZN7daPly_c19func_ov002_020bea7cEv(c) == 0) {             /* face timer idle */
        char *head = ((char **)(c + 0x154))[hid];
        if (head)
            _ZN15dExtAnmTexPat_c6UpdateER15ModelComponents(
                c + 0x1dc + *(unsigned char *)(c + 0x6db) * 0x14, head + 8);
    }
}

/* THE TEXTURE-SEQUENCE UPDATES RUN IN A REPLAYED FRAME.
   hal_render_player_world is entirely tick-only-skipped and entirely
   conservative-re-sim-skipped (rb_skip_render(), which the actor-render
   switch does not touch -- it is a Stage-level render skip, not
   the actor-Render one). The map0 DET rung showed one arena byte
   drifting on every VS map 0 window: the per-character eye-blink material
   flag (hal_player_texseq_head's third block, "everyone gets, all
   characters") lands in the CURRENT HEAD MODEL's ModelComponents, which
   every player slot on the same character shares (VS map 0's census plays
   four of the same character) -- so the update is shared state a later
   frame's SAME call reads (whether the blink shows), not Render-private
   scratch, exactly the rule the replay's own notes state for
   an actor's Render. It is not an actor, so it is not on the actor replay
   list; it is a Player, and it belongs beside
   the particle submission (tests/walk_window.cpp, "the particle
   SUBMISSION stays in a replayed frame") -- state-mutating, not pixel
   work, so it runs on every tick, replayed or not. What it does NOT do is
   run the frame's other player-render work (UpdateVerts, dExtAnmModel_c::-
   Render, the wing model): those stay under rb_skip_render(), unmeasured
   here, because they write host GX state only.
   The port_player_render_hidden gate is reproduced because the ROM's own
   daPly_c::Render only reaches these updates past its four hide gates
   (comment above hal_render_player_world). */
extern "C" void hal_player_texseq_tick(void *player)
{
    char *c = (char *)player;
    if (port_player_render_hidden(c)) return;
    hal_player_texseq_body(c);
    const unsigned hid = _ZN7daPly_c19func_ov002_020becf4Eii(c, *(unsigned char *)(c + 0x6db), 1);
    if (hid != 8 && hid != 9)
        hal_player_texseq_head(c, hid);
}

/* ---- SM64DS_YHD_PROBE: the head-vs-body matrix separation, per frame --------
   Prints the difference between the two matrices this function is about to draw
   with -- `scene`, the body's, and the head's at +0x1c -- next to the player
   fields _ZN7daPly_c19func_ov002_020e444cEv builds both of them from. On the ROM those two are
   the SAME WRITE (src/func_ov002_020e444c.c lines 55 and 66 both store
   data_020a0e68), so every difference column here reads zero, and a nonzero one
   is the gap the head is being drawn away from the body by. That is the
   regression the banner in hal_render_player_world closed, so the probe stays.

   dtr = head translation - body translation, in scene units (world >> 3).
   drot = max |head rotation element - body rotation element|, Fix12 (0x1000=1).
   spd  = horizontal |dpos| per frame in world Fix12, the "certain speeds" axis. */
static double yhd_sqrt(double v) { return std::sqrt(v); }
static void yhd_probe(char *c, const int *scene, const char *head)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("SM64DS_YHD_PROBE");
        on = e ? atoi(e) : 0;
        if (on)
            printf("[yhd] frame spd sink u690 u694 angY angX angZ "
                   "dtrx dtry dtrz |dtr| drot\n");
    }
    if (!on || !head) return;

    static int frame = 0, px = 0, py = 0, pz = 0, primed = 0;
    const int x = *(const int *)(c + 0x5c);
    const int y = *(const int *)(c + 0x60);
    const int z = *(const int *)(c + 0x64);
    const int vx = primed ? x - px : 0;
    const int vz = primed ? z - pz : 0;
    px = x; py = y; pz = z; primed = 1;
    (void)py;

    const int *hm = (const int *)(head + 0x1c);
    const int dtx = hm[9] - scene[9];
    const int dty = hm[10] - scene[10];
    const int dtz = hm[11] - scene[11];
    int drot = 0;
    for (int i = 0; i < 9; ++i) {
        int d = hm[i] - scene[i];
        if (d < 0) d = -d;
        if (d > drot) drot = d;
    }
    const double spd = (double)vx * vx + (double)vz * vz;
    const double dtr = (double)dtx * dtx + (double)dty * dty + (double)dtz * dtz;

    printf("[yhd] %d %.1f %d %d %d %d %d %d %d %d %d %.1f %d\n", frame++,
           yhd_sqrt(spd), *(const int *)(c + 0x68c),
           *(const int *)(c + 0x690), *(const int *)(c + 0x694),
           (int)*(const short *)(c + 0x8e), (int)*(const short *)(c + 0x8c),
           (int)*(const short *)(c + 0x90), dtx, dty, dtz,
           yhd_sqrt(dtr), drot);
}

/* ---- SM64DS_HSINK_PROBE, and the scale the body draw was missing ------
   Env-gated, inert unless SM64DS_HSINK_PROBE is set. Reads only; writes no
   game state.

   THE ROM PASSES ONE SCALE VECTOR TO BOTH HALVES OF THE PLAYER.
   src/_ZN6Player6RenderEv.cpp:79 draws the body

       _ZN17dExtSimpleModel_c6RenderEPK7Vector3(bodyMdl, ((char*)this) + 0x80);

   and :121 draws Yoshi's head with the SAME ((char*)this) + 0x80. That is
   Player mScaleX/Y/Z (Actor.h +0x080/+0x084/+0x088). This port drew the head
   with it and the body with nothing, until the call site below was corrected:

       body:  ma->dExtAnmModel_c::Render(0)                     -> NULL == unit
       head:  _ZN17dExtSimpleModel_c6RenderEPK7Vector3(head, c + 0x80)  -> mScale

   (that body call was ALSO the wrong method -- see the call site's own banner
   for the re-pose half of the same one-line correction)

   NULL really is unit: the vector reaches _ZN15ModelComponents13func_0204488cEiP7Vector3
   (src/func_0204488c.c:66-72), which writes MTX_SCALE (0x0400046c) only when
   the pointer is non-null, and it writes it AFTER the world matrix multiply
   and BEFORE the per-bone MTX_TRANS -- so the scale multiplies the bone
   translations, i.e. it scales the model about the model ROOT (the player's
   feet), not about the bone.

   With the root, the world rotation and the world translation identical for
   both models, any scale != 1.0 moves one mesh and leaves the other where it
   was. mScaleY is written by exactly two functions, both crush states:
   daPly_c::St_Squish_Main (src/game/player/Player.cpp, case 0 sets
   mScaleY = 0x100 and case 1 walks it back up in 0x100 steps) and
   daPly_c::Unk_020c6a10 (mScaleY = 0x100, 30-frame hold), so in ordinary play
   mScale is unit and this whole difference is invisible.

   COLUMNS, all Fix12 (0x1000 == 1.0). Bone translations are SCENE units, which
   are world >> 3.
     frame   render frame counter
     posY    Player mPosY (+0x60), world Fix12
     sy      the Y the HEAD render was actually handed. Player mScaleY on
             Yoshi (the i==3 arm); 4096 on Mario/Luigi/Wario, whose head is
             a scene-space stand-in that is handed no scale at all -- see
             hal_render_head_group
     bsy     the Y the BODY render was actually handed this frame; it read
             4096 on every frame before the fix, because the call passed
             NULL, and it now carries Player mScaleY
     hb0y    head bone 0 origin, model space Y
     nky     body neck bone (bones + 0x2d0) origin, model space Y
     headY   head bone 0 world Y AS DRAWN, i.e. through the head's scale
     bodyY   neck bone world Y AS DRAWN, i.e. through the body's scale
     gap     headY - bodyY. THIS IS THE ANSWER COLUMN. The head's anchor and
             the body's neck are the same joint; a large negative gap is the
             head drawn far below the neck it hangs from.
     btris   polygons the body draw submitted this frame, so "the fix is a
             no-op in unscaled play" is a measurement and not an argument.

   MEASURED on castle grounds as Yoshi (SM64DS_CHARACTER=3, level 1),
   squished at frame 66 by SM64DS_FORCE_STATE=squish, which calls the ROM's
   own crusher entry daPly_c::Unk_020c6a10(1):

     standing, sy 4096   gap 0        on every frame
     squished, sy 256    gap -41723   pre-fix, flat for the whole hold
     squished, sy 256    gap 0        post-fix

   -41723 Fix12 in scene units is 10.2 scene units, and scene is world >> 3,
   so the head was drawn 81.5 world units below the neck it hangs from --
   a head inside a body that had not shrunk.

   MARIO/LUIGI/WARIO ARE A DIFFERENT SHAPE, and the two figures this
   paragraph used to give -- gap -44136 pre-fix and -2758 post-fix, from
   the row [hsink] 68 ... 130048 132806 -2758 -- ARE WITHDRAWN. They came
   out of the else-arm probe bug described at hsink_probe below: headY was
   pinned at scene[10], the player root, so the column reported the whole
   neck height as a sink even on a standing Mario. Read the corrected
   figures there. Their head is a separate cap model that the ROM parents
   to the body's neck bone and renders with &mScaleX
   (src/_ZN6Player6RenderEv.cpp:127); hal_render_head_group now passes that
   argument, and the cap's POSITION residue is a separate change described
   in that function's banner.                                        */
static int hsink_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = std::getenv("SM64DS_HSINK_PROBE");
        on = e ? std::atoi(e) : 0;
    }
    return on;
}
static int hsink_worldY(const int *m, int px, int py, int pz)
{
    long long v = (long long)px * m[1] + (long long)py * m[4] +
                  (long long)pz * m[7];
    return (int)(v >> 12) + m[10];
}
static int hsink_axisY(const int *m, const int *b, const int *s)
{
    const int px = (int)(((long long)b[9]  * s[0]) >> 12);
    const int py = (int)(((long long)b[10] * s[1]) >> 12);
    const int pz = (int)(((long long)b[11] * s[2]) >> 12);
    return hsink_worldY(m, px, py, pz);
}
/* THE PROBE READ THE WRONG MATRIX FOR EVERY HEAD BUT YOSHI'S, and every
   Mario/Luigi/Wario figure it ever printed -- including the -44136 and -2758
   in the banner above -- carried the error.

   It took head+0x1c to be the matrix the head is drawn through. On the i==3
   arm that is true. On the else arm it is not: head+0x1c is RE-SEATED TO THE
   PLAYER-ROOT MATRIX EVERY FRAME by _ZN7daPly_c19func_ov002_020e444cEv (the same seat the
   i==3 arm rests on), and hal_render_head_group's neck compose overwrites it
   only for the instant of the draw. This probe runs BEFORE that compose, so it
   read the player root -- the feet -- through the head's own bone 0, which the
   else arm never seats and which is (0,0,0).

   MEASURED, and it is not a rounding error: a STANDING Mario, mScale unit,
   nothing wrong with him, read gap -44081 on every frame -- the whole neck
   height, reported as a sink. headY was pinned at scene[10] while bodyY moved
   with the body's scale, so the column was measuring the body's scale twice
   and the head not at all.

   CORRECTED, AGAIN: hal_render_head_group's else arm now seats the body's
   neck bone into the head model's own bone 0 (the ROM's statement 1) instead
   of composing neck * scene into head+0x1c, so head+0x1c is left exactly as
   _ZN7daPly_c19func_ov002_020e444cEv seats it and is the matrix the draw loads on EVERY
   arm, i==3 included. The probe reads it directly, with no separate compose
   of its own. */
static void hsink_probe(char *c, const int *scene, const char *head,
                        const char *ma, const int *bscale,
                        const int *hscale, unsigned hid, int btris)
{
    static int on = -1;
    if (on < 0) {
        on = hsink_on();
        if (on)
            std::printf("[hsink] frame posY sy bsy hb0y nky headY bodyY gap btris\n");
    }
    if (!on || !head || !ma) return;

    static int frame = 0;
    const int unit[3] = { 0x1000, 0x1000, 0x1000 };
    const int *hs = hscale ? hscale : unit;
    const int *bs = bscale ? bscale : unit;

    const char *hb = *(const char *const *)(head + 0x14);
    const char *bb = *(const char *const *)(ma + 0x14);
    if (!hb || !bb) { ++frame; return; }
    const int *b0 = (const int *)hb;              /* head bone 0  */
    const int *nk = (const int *)(bb + 0x2d0);    /* body neck    */

    /* the matrix the draw loads: head+0x1c, on every arm. */
    const int *hm = (const int *)(head + 0x1c);
    const int headY = hsink_axisY(hm, b0, hs);
    const int bodyY = hsink_axisY(scene, nk, bs);

    std::printf("[hsink] %d %d %d %d %d %d %d %d %d %d\n", frame++,
                *(const int *)(c + 0x60), hs[1], bs[1], b0[10], nk[10],
                headY, bodyY, headY - bodyY, btris);
}







/* run mg16 lane MP3: the "should this player body be hidden this frame" test,
   lifted out of hal_render_player_world so another host-side hide path can
   ask the SAME question instead of duplicating daPly_c::Render's gates. Nonzero
   means the ROM's daPly_c::Render (src/_ZN6Player6RenderEv.cpp:44-58) would not
   draw the body this frame, in its own gate order:
     :44-48  VS mode and this slot not live       -> hide
     :49-55  the invincibility blink               -> hide (see the bit note)
     :56     fully transparent, mOpacity==0 (0x6f5) -> hide
     :58     held/eaten, mFlags & 0x10 (Actor +0xb0)-> ROM skips the body block
   Gates :56 and :58 were the two this port used to omit. The mFlags one is the
   ROM's held/eaten hide, and the eaten victim is exactly why it matters: while
   St_InYoshiMouth the player stays LIVE and keeps mOpacity 0x1f, so it passes
   gates 1-3 and rode the eater visibly until mFlags&0x10 got honoured here. */
extern "C" int port_player_render_hidden(const void *player)
{
    const char *c = (const char *)player;
    const unsigned char no = *(const unsigned char *)(c + 0x6d8);
    /* :49-55  blink. The ROM picks bit 1 or bit 0 off IsState(hurt); that state
       object is ov002's and unreachable here, so the port takes the conservative
       half, bit 0, the arm the ROM uses everywhere except that one state. */
    if (port::player::invincible_timer(c) & 1)
        return 1;
    /* :56  mOpacity == 0, offset 0x6f5 (Player.h). */
    if (*(const unsigned char *)(c + 0x6f5) == 0)
        return 1;
    /* :58  mFlags & 0x10, Actor mFlags at +0xb0. The ROM's held/eaten hide. */
    if (*(const unsigned int *)(c + 0xb0) & 0x10)
        return 1;
    return 0;
}

static void hal_render_player_world_body(void *player);


struct MirrorStamp {
    int pal;
};
static MirrorStamp g_mirror_stamp;
static void mirror_restamp()
{
    MirrorStamp &m = g_mirror_stamp;
}

/* BROSTAMP1: THE BALLOON IS THE BODY WHILE IT IS UP. With mIsBalloon
   (+0x6fd) on, the body draw is the inline balloon model at +0xf0
   (hal_render_player_world_body's balloon arm, src/game/player/Player.cpp
   daPly_c::Render's else arm), not mBodyModels[GetBodyModelID], so the
   stamps go on it. It shares the body's palette names (mario_body_pl ...),
   so the picks find it the same way. */
static char *stamp_body_model(char *c, unsigned id)
{
    if (*(const unsigned char *)(c + 0x6fd) != 0) return c + 0xf0;
    return (char *)((dExtAnmModel_c **)(c + 0xdc))[id];
}



/* BALLOON1: THE BALLOON BODY MAY NOT EXIST WHILE THE BALLOON IS ON.
   _ZN7daPly_c19func_ov002_020e5948Ev (src/func_ov002_020e5948.c:330-345) SetFiles the balloon
   body at Player+0xf0 only when the course is NOT one of levels 2, 4 and 5
   (the castle floors). The castle 2F carries four Power Flowers all the same:
   param1 0xffff, "persistent", which InitResources lets live only while the
   closest player is Luigi (the Vanish Luigi flower). A Mario who is in the room
   when one is lying there (the character switched after the course loaded, or a
   second player standing closer) picks it up, CheckPickup takes the Mario arm
   and daPly_c::InitBalloonMario turns +0x6fd on with nothing at +0xf0 but the
   constructed vptr. The first Render then reads a model file that is null
   (ApplyOpacity -> _ZN15ModelComponents8SetAlphaEj, fault address 0x24, crash report c926736d).
   The ROM never checks: on the DS that read lands in the low TCM mirror and the
   draw that follows is undefined. Here the balloon is built on first use, with
   the very SetFile / SetAnim the ROM's SetFiles makes (same file, same args),
   so the balloon looks and flies as it does in every other course. The matrix
   the behaviour pass already seated at +0x10c is kept across the SetFile. */
extern "C" void *_ZN17dExtSimpleModel_c8LoadFileER13SharedFilePtr(void *ptr);
extern "C" int _ZN11dExtModel_c7SetFileEP8BMD_Fileii(void *self, void *file, int a, int b);
extern "C" int _ZN14dExtAnmModel_c7SetAnimEP8BCA_Fileiit(void *, void *, int, int, unsigned int);
extern "C" char data_ov002_0210e6f8[];     /* SharedFilePtr: the balloon body */
extern "C" int data_ov002_0210e750[];      /* [1] = its BCA */
static bool balloon_model_ready(char *c)
{
    char *m3 = c + 0xf0;
    if (*(void **)(m3 + 8) != 0) return true;
    static int logged;
    unsigned char keep[0x30];
    memcpy(keep, c + 0x10c, sizeof keep);
    void *f = _ZN17dExtSimpleModel_c8LoadFileER13SharedFilePtr(data_ov002_0210e6f8);
    if (f) {
        _ZN11dExtModel_c7SetFileEP8BMD_Fileii(m3, f, 1, 1);
        _ZN14dExtAnmModel_c7SetAnimEP8BCA_Fileiit(m3, (void *)(size_t)data_ov002_0210e750[1], 0x40000000, 0x1000, 0);
    }
    memcpy(c + 0x10c, keep, sizeof keep);
    bool ok = *(void **)(m3 + 8) != 0;
    if (logged < 3) {
        ++logged;
        fprintf(stderr, "[balloon] Player %p has the balloon on (+0x6fd) with no balloon body (course %d builds none): %s\n",
                (void *)c, (int)gCurrentSublevel, ok ? "built it on first use" : "could not build it, balloon draw skipped");
    }
    return ok;
}


void hal_render_player_world(void *player)
{
    const int slot = (int)*(const unsigned char *)((const char *)player + 0x6d8);
    int pal = -1;
    if (pal >= 0) {
        char *c = (char *)player;
        const unsigned id =
            _ZNK7daPly_c14GetBodyModelIDEjb(c, *(int *)(c + 8) & 0xff, 0);
        const unsigned hid =
            _ZN7daPly_c19func_ov002_020becf4Eii(c, *(unsigned char *)(c + 0x6db), 1);
    }
    {
        char *c = (char *)player;
        const unsigned chr = (unsigned)*(int *)(c + 8) & 0xff;
    }
    if (pal >= 0) {
        g_mirror_stamp.pal = pal;
    }
    hal_render_player_world_body(player);
    g_mirror_stamp.pal = -1;
}

static void hal_render_player_world_body(void *player)
{
    char *c = (char *)player;

    /* ---- THE GATES daPly_c::Render KEEPS, AND THIS DID NOT ------------------
     *
     * Run mg16 lane MP3, field failure 3. This function draws the body; the
     * SHADOW is a different path entirely -- daPly_c::Behavior registers a
     * ShadowModel node (_ZN7daPly_c19func_ov002_020e4bb8Ev -> _ZN7daPly_c19func_ov002_020e444cEv ->
     * Actor::DropShadowRadHeight) and one global ShadowModel::RenderAll walk
     * draws every node. That path honours its OWN gates. This one honoured
     * none of src/_ZN6Player6RenderEv.cpp's, so the two could disagree about
     * whether a player should be drawn AT ALL -- and a caster whose body is
     * gated out is precisely "a shadow with no body".
     *
     * The four gates, in the ROM's own order (_ZN6Player6RenderEv.cpp:44-58),
     * all four now reproduced through port_player_render_hidden:
     *   VS mode and this slot not live  -> not drawn
     *   the invincibility blink          -> not drawn on alternating frames
     *   fully transparent, mOpacity==0   -> not drawn  (offset 0x6f5, Player.h)
     *   mFlags & 0x10                    -> not drawn  (Actor +0xb0, held/eaten)
     *
     * The blink one matters most here: it is a PER-FRAME alternation, so a
     * body that ignores it while its shadow does not gives a shadow that
     * flickers away from a body that never does.
     *
     * NOT A NEW POLICY -- this is the matched TU's own logic, applied where the
     * port draws instead of it. hal_render_player_world exists because
     * daPly_c::Render's vtable slot is a no-op in this port
     * (hal/level_boot.cpp's ps_render), so every gate that slot would have
     * applied has to be applied here or it is not applied anywhere. */
    /* ALL FOUR gates now, through the one named predicate so another
       host-side hide path can reuse it. The blink gate reads the invincibility
       timer THROUGH THE ACCESSOR: the raw path here once read 0x6a6, which is
       mStateWaitTimer not mInvincibleTimer, so it culled the body on a timer
       that ticks constantly; the accessor is the one place that offset lives.
       mOpacity (0x6f5) and mFlags&0x10 (+0xb0) used to be omitted here and are
       reproduced inside the predicate now. */
    if (port_player_render_hidden(c)) {
        /* SM64DS_EATEN_PROBE: prove the held/eaten hide fires. On the green
           path this is silent; set the env var to log the slot whose body is
           now skipped because Actor mFlags&0x10 is set (the St_InYoshiMouth
           victim that used to ride the eater). */
        if (getenv("SM64DS_EATEN_PROBE") &&
            (*(const unsigned int *)(c + 0xb0) & 0x10))
            std::fprintf(stderr, "[eaten] slot %u body skipped: mFlags&0x10\n",
                         (unsigned)*(const unsigned char *)(c + 0x6d8));
        return;
    }

    /* SMOOTHREND1 (D18, Metal Wario's closing flicker): daPly_c::Render picks the body with GetBodyModelID(.., 1), the form that
       reads the RENDER copy of the metal flag (+0x6fa), not the live one (+0x6f9). The behaviour pass clears +0x6fa on every
       second tick of the last 62 ticks of the cap and sets it back on the next, and that is the whole flicker: the body AND the
       cap model (the head group below already asked with 1) swap between metal and plain Wario together. Asking with 0 drew the
       metal body on every tick and only the cap blinked. Outside the flicker the two flags are equal. */
    unsigned id = _ZNK7daPly_c14GetBodyModelIDEjb(c, *(int *)(c + 8) & 0xff, 1);
    dExtAnmModel_c *ma = ((dExtAnmModel_c **)(c + 0xdc))[id];
    if (!ma) return;
    /* THE BODY MATRIX IS THE ONE THE SEAT ALREADY WROTE, and this function
       must not build its own. That is the whole of the Yoshi head-vs-body
       separation the field report described as the head coming off at speed
       and on hard landings.

       _ZN7daPly_c19func_ov002_020e444cEv (src/func_ov002_020e444c.c) composes ONE matrix in
       data_020a0e68 each frame inside daPly_c::Behavior --

           translation(RotateY(pos, mAngleY) + [0, unk_690 - mSinkDepth,
                                                unk_694])
           then rotation Y (+0x8e), then X (+0x8c), then Z (+0x90)

       -- and stores that same matrix into the BODY model at +0x1c (line 55)
       and into the HEAD model at +0x1c (line 66), in one statement group.
       daPly_c::Render (src/_ZN6Player6RenderEv.cpp:79 and :121) then draws both
       with a bare dExtSimpleModel_c::Render and sets NO matrix at render time. On the ROM
       the two models cannot separate: they are literally the same write.

       This function used to rebuild a matrix here from mAngleY and the
       position alone and store it over the body's, leaving the head on the
       seated one. The two agree while Yoshi stands still -- which is why an
       earlier head-vs-body probe read them identical and the note above
       recorded them as bit-identical -- but the seat also carries the X and Z
       rotations, and +0x8c is the LEAN, driven by speed out of
       _ZN7daPly_c19func_ov002_020cd550Ev. So the faster he ran the further the head pitched
       away from a body that never pitched at all.

       Measured on this tree before the change, castle grounds as Yoshi, the
       largest rotation-element difference between the head's matrix and the
       body's over 300 frames (Fix12, 0x1000 = 1.0):

           idle       maxspeed      0    gap    0
           walk       maxspeed  65520    gap  682   (lean 9.5 deg)
           dash       maxspeed 131040    gap 1350   (lean 19.2 deg)
           longjump   maxspeed 196560    gap 1112

       and gap == 4096 * sin(lean) to within rounding at every sample, so the
       separation was the lean angle exactly. Against the matrix the seat had
       already left in the body, the head measured identical on every frame of
       every run -- the right matrix was there the whole time and was being
       thrown away.

       Both halves of the field report are this one fault. The lean only exists
       above a speed, and it grows with it, which is "at certain speeds". And it
       engages in a SINGLE FRAME at full size around jumps and landings -- 0 to
       1038 at frame 158 of the jump-spam run, 0 to 1050 at frame 98 of the
       dash-jump run, 0 to 1073 at frame 234 of the long-jump run -- so the head
       did not drift off, it popped off and back on, which is "when he hits the
       ground too hard".

       So: read it, do not build it. The scale and units are unchanged, because
       the seat writes the same scene convention this function used to build
       (its translation is `out >> 3`, the same world-to-scene shift, with the
       rotation rows at 1.0 out of Matrix4x3_FromTranslation). `scene` stays the
       name the neck-bone and wing composes below use, and they now compose
       against the body's real matrix rather than a stand-in. */
    int scene[12];
    for (int i = 0; i < 12; ++i) scene[i] = ((const int *)&ma->mat4x3)[i];
    /* THE LOCAL BODY'S OWN OPACITY (Player+0x6f5), the first statement of
       daPly_c::Render's body arm (src/_ZN6Player6RenderEv.cpp:63). It is
       31 in play and runs down to 1 and back while St_Teleport is on (the
       secret warp spots), so the body dissolves out and back in: this copy
       of the render only ever stamped it on the balloon arm, and a warp
       showed the body fully solid until the screen flash took it. */
    _ZN11dExtModel_c12ApplyOpacityEjj(ma, *(const unsigned char *)(c + 0x6f5));
    /* THE ROM'S BODY DRAW, WHOLE. src/_ZN6Player6RenderEv.cpp:79 is one
       statement carrying two facts, and the port used to get both of them
       wrong with `ma->dExtAnmModel_c::Render(0)`:

           _ZN17dExtSimpleModel_c6RenderEPK7Vector3(bodyMdl, ((char*)this) + 0x80);

       BASE dExtSimpleModel_c::Render, not dExtAnmModel_c::Render. dExtAnmModel_c::Render is
       UpdateVerts + dExtSimpleModel_c::Render (src/_ZN9ModelAnim6RenderEPK7Vector3.cpp),
       so dispatching it here re-posed the body a second time at draw
       time, after the UpdateVerts three lines up and after `scene` was
       read. That is the same reach-past-the-anim the head's i==3 arm makes
       for the same reason (see hal_render_head_group's banner).

       AND THE +0x80 SCALE, not NULL. Player mScaleX/Y/Z. See the
       SM64DS_HSINK_PROBE banner above for what NULL cost.

       ONE LINE, BOTH HALVES, DELIBERATELY. They land on the same call and
       either one alone is still not the ROM's statement; written as two
       independent switches, whichever flipped second would silently edit
       what the first one did. */
    /* THE BALLOON IS THE OTHER ARM OF THE SAME STATEMENT. daPly_c::Render
       forks the body draw on mIsBalloon (+0x6fd, the live byte, not the +0x6fe
       latch the head test reads), src/_ZN6Player6RenderEv.cpp:59-99:

           if (mIsBalloon == 0) {
               ... the body, its texture sequence, the wings ...
           } else {
               dExtModel_c::ApplyOpacity(this + 0xf0, mOpacity, 0);
               mModelAnim3.m14(&mScaleX);          slot 5, dExtAnmModel_c::Render
           }

       mModelAnim3 is the balloon body _ZN7daPly_c19func_ov002_020e5948Ev SetFiles at +0xf0
       (src/func_ov002_020e5948.c:347), and the behaviour pass seats ITS matrix
       while the balloon is up, not the body's (src/func_ov002_020e444c.c:52-58
       writes +0x10c instead). This copy had only the first arm, so a Power
       Flower left the normal body standing where the balloon started, frozen,
       with the head skipped (hid 8 is the ROM's own balloon rule): the
       headless Mario the forum report shows. */
    const bool balloon = *(const unsigned char *)(c + 0x6fd) != 0;
    std::size_t hsink_t0 = 0;
    std::size_t hsink_t1 = 0;
    if (balloon) {
        dExtAnmModel_c *m3 = (dExtAnmModel_c *)(c + 0xf0);    /* mModelAnim3 */
        if (!balloon_model_ready(c)) {
            /* no balloon body and none could be built: draw nothing for it
               rather than hand the draw a model with no file behind it */
        } else {
        _ZN11dExtModel_c12ApplyOpacityEjj(m3, *(const unsigned char *)(c + 0x6f5));
        m3->Render((const Vector3 *)(c + 0x80));    /* virtual, as the ROM's */
        }
    } else {
        ma->dExtSimpleModel_c::Render((const Vector3 *)(c + 0x80));
        hsink_t1 = hsink_t0;
        hal_player_texseq_body(c);
        /* the mirror, src/_ZN6Player6RenderEv.cpp:81 (see the head's call) */
        mirror_restamp();               /* BROSTAMP1: the texseq above moved stamped bases */
        func_ov002_020e3e00(ma, c + 0x80, *(const unsigned char *)(c + 0x6f5));
    }

    unsigned hid = _ZN7daPly_c19func_ov002_020becf4Eii(c, *(unsigned char *)(c + 0x6db), 1);
    if (hid != 8 && hid != 9) {
        char *head = ((char **)(c + 0x154))[hid];
        if (head) {
            yhd_probe(c, scene, head);
            hsink_probe(c, scene, head, (const char *)ma,
                        (const int *)(c + 0x80),
                        (const int *)(c + 0x80),
                        hid, (int)(hsink_t1 - hsink_t0));
            /* the head's own line of the same statement block (:104) */
            _ZN11dExtModel_c12ApplyOpacityEjj(head, *(const unsigned char *)(c + 0x6f5));
            hal_render_head_group(c, head, hid, ma, scene);
            hal_player_texseq_head(c, hid);
            /* THE MIRROR, once for the body (above) and once here for the
               head, src/_ZN6Player6RenderEv.cpp:81 and :128:

                   func_ov002_020e3e00(model, this + 0x80, mOpacity);

               src/func_ov002_020e3e00.cpp draws the model a second time
               reflected in X about the room's mirror plane (x = 0 in the
               King Boo arena, level 0x2f, while its ov055 gate allows; x =
               0x1086000 in the castle's mirror room, level 5 area 3), with
               the material flags that flip its winding, and restores the
               model's matrix after. It is linked (port/slice_hostgen4.txt)
               and this copy never called it, so the player had no
               reflection in either mirror. */
            mirror_restamp();           /* BROSTAMP1: the blink above moved the eyes' base */
            func_ov002_020e3e00(head, c + 0x80, *(const unsigned char *)(c + 0x6f5));
            /* THE LAST STATEMENT OF THE ROM'S HEAD BLOCK,
               src/_ZN6Player6RenderEv.cpp:129-131:

                   if (_ZN7daPly_c19func_ov002_020d225cEv(this))
                       ((VObj*)unk_1d8)->m14(&unk_56c);

               +0x1d8 is the Model _ZN7daPly_c19func_ov002_020e5948Ev news and SetFiles
               outside VS (src/func_ov002_020e5948.c:330-336), and
               _ZN7daPly_c19func_ov002_020d225cEv is Yoshi (param1 3) with +0x721 == 2 and
               +0x6e6 == 0: the sleep _ZN7daPly_c19func_ov002_020d228cEv puts him in (ST_WAIT,
               anim 9, +0x56c..+0x574 cleared, +0x721 = 2), which the opening
               does to him on the castle roof (src/func_ov085_0212df84.cpp).
               _ZN7daPly_c19func_ov002_020e444cEv seats that model's matrix in front of his
               face each frame (src/func_ov002_020e444c.c:70-75) and +0x56c is
               the scale that swells it. This copy never drew it. Slot 5 is
               dExtSimpleModel_c::Render, the same dispatch the head's own draw makes. */
            if (_ZN7daPly_c19func_ov002_020d225cEv(c)) {
                char *m = *(char **)(c + 0x1d8);
                if (m)
                    ((void(__fastcall *)(void *, void *, const void *))(
                        ((void ***)m)[0][5]))(m, 0, c + 0x56c);
            }
        }
    }

    /* THE WING MODEL: daPly_c::mModelAnim4, EMBEDDED at +0x174.
       NOT the tongue, which is what an earlier pass here took it for. The gate
       says so outright -- _ZN7daPly_c19func_ov002_020e4bb8Ev ends with the render latches

           *(u8*)(self + 0x6fe) = *(u8*)(self + 0x6fd);
           *(u8*)(self + 0x700) = *(u8*)(self + 0x6ff);   <- mHasWings

       so unk_700 is last frame's mHasWings, and the model it gates is the pair
       of wings. That also explains the Yoshi special case below: his wings
       hang off a different bone, with an offset and a quarter turn.

       daPly_c::Render (src/_ZN6Player6RenderEv.cpp) draws it like this:

           if (unk_700 != 0) {
               dExtModel_c::ApplyOpacity(this + 0x174, mOpacity, 0);
               if (unk_6db == 3) {
                   Matrix4x3_FromTranslation(&data_020a0e68, -0x1b33, -0x666, 0);
                   Matrix4x3_ApplyInPlaceToRotationZ(&data_020a0e68, 0x4000);
                   MTX_Concat43(&data_020a0e68, bodyBones + 0x180, &data_020a0e68);
                   mModelAnim4.Virtual18(&data_020a0e68, &mScale);
               } else {
                   mModelAnim4.Virtual18(bodyBones + 0x2d0, &mScale);
               }
           }

       and all four parts of that matter. An earlier pass here drew
       *(dExtAnmModel_c **)(c + 0x160) -- the pointer _ZN7daPly_c19func_ov002_020d71ecEi SetAnims,
       which is NOT the object Render walks -- with the body's own scene matrix,
       through Render (slot 4), with no gate at all. What Tango saw was a
       model upside down under every character's feet, on Mario as much as on
       Yoshi: the probe said 198 polygons on frame 0 of a plain Mario boot,
       from the same object either way.

       THE TONGUE IS STILL NOT DRAWN. Removing this does not cost anything that
       was working -- the wings were never in the right place -- but it does not
       deliver the tongue either. Whatever draws the tongue, it is not
       mModelAnim4, and unk_700 never opens on a tongue (SM64DS_WINGS_PROBE=1
       through SM64DS_SELFTEST_TONGUE says the gate stays 0 the whole run).

       Faithful now: gated on unk_700, the ROM's own two matrices, and
       Virtual18 -- which is host slot 6 (_ZTV14dExtAnmModel_c is D1 0, D0 1,
       dExtSimpleModel_c::DoSetFile 2, UpdateVerts 3, Virtual10 4, Render 5, Virtual18 6,
       read out of the ROM's own table at 0x0208e980) and takes the matrix and
       the scale, unlike Render.

       THE BONE GOES IN AS IT IS, the ROM's statement word for word. This used
       to compose it through `scene` first, the way the head at +0x154 once
       was; the head path dropped that (hal_render_head_group: the neck is
       copied into the head's bone 0 and the head's own mat4x3 is left as
       _ZN7daPly_c19func_ov002_020e444cEv seated it), and the wings were left behind.
       _ZN7daPly_c19func_ov002_020e444cEv seats the wing model's mat4x3 too (Player+0x190 =
       +0x174 + 0x1c, src/func_ov002_020e444c.c:56), with the same player root
       the body draws through, and Virtual18 is Virtual10 (the matrix into
       transforms[0]) + dExtSimpleModel_c::Render through that mat4x3
       (src/_ZN9ModelAnim9Virtual18EjPK7Vector3.cpp). So the composed matrix
       applied the root twice and the wings drew far off the player: the first
       time a live Wing Cap reached this block (lane FEATHER1, a ? block's
       feather in Bob-omb Battlefield) the gate opened and nothing showed on
       the cap. dExtModel_c::ApplyOpacity is the statement's first line, as in
       the balloon arm above. */
    {
        static int probe = -1;
        if (probe < 0) probe = std::getenv("SM64DS_WINGS_PROBE") ? 1 : 0;
        char *m4 = c + 0x174;
        unsigned char gate = *(unsigned char *)(c + 0x700);
        if (probe) {
            static int said, said_open;
            if (!said) {
                said = 1;
                std::fprintf(stderr, "[wings] +0x174 gate unk_700=%u "
                             "unk_6db=%u vptr=%p\n", gate,
                             *(unsigned char *)(c + 0x6db), *(void **)m4);
            }
            if (gate && !said_open) {
                said_open = 1;
                std::size_t n0 = 0, n1 = 0;
                std::fprintf(stderr, "[wings] gate OPENED (unk_700=%u, "
                             "unk_6db=%u, step=%u)\n", gate,
                             *(unsigned char *)(c + 0x6db),
                             *(unsigned char *)(c + 0x6e3));
                (void)n0; (void)n1;
            }
        }
        /* inside the ROM's `mIsBalloon == 0` arm, like the body draw above */
        if (gate != 0 && !balloon) {
            char *bones = *(char **)((char *)ma + 0x14);
            const int *src;
            _ZN11dExtModel_c12ApplyOpacityEjj(m4, *(const unsigned char *)(c + 0x6f5));
            if (*(unsigned char *)(c + 0x6db) == 3) {
                Matrix4x3_FromTranslation(&data_020a0e68, -0x1b33, -0x666, 0);
                Matrix4x3_ApplyInPlaceToRotationZ(&data_020a0e68, 0x4000);
                MTX_Concat43(&data_020a0e68, bones + 0x180, &data_020a0e68);
                src = (const int *)&data_020a0e68;
            } else {
                src = (const int *)(bones + 0x2d0);
            }
            /* Slot 6, not 5. The ROM's _ZTV14dExtAnmModel_c at 0x0208e980 is seven slots
               -- D1 0, D0 1, dExtSimpleModel_c::DoSetFile 2, UpdateVerts 3, Virtual10 4,
               Render 5, Virtual18 6 (0x0208e99c = 020167c4) -- and
               src/_ZN6Player6RenderEv.cpp's own shadow names every slot by byte
               offset, so the wing statement's m18 is byte 0x18, index 6. MSVC does
               not fold the destructor pair for this family any more
               (hal/cxxname_bridge.cpp:530-539), so ??_7dExtAnmModel_c@@6BdExtSimpleModel_c@@@ is
               seven slots in the same order and slot 6 is
               ?Virtual18@dExtAnmModel_c@@UAEXIPBUVector3@@@Z. This read 5 while the
               fold made Virtual18 the fifth slot; 0a7f12ee9 restored ROM order and
               these dispatches were not renumbered with it. Index 5 is
               dExtAnmModel_c::Render, which takes the scale alone and ends `ret 4`,
               while this site pushes two stack words and cleans none -- so the
               wrong slot also left four bytes on the stack and this function's
               epilogue pops edi, esi and ebx before it restores the frame
               pointer. */
            ((void(__fastcall *)(void *, void *, unsigned, const void *))(
                ((void ***)m4)[0][6]))(m4, 0, (unsigned)(uintptr_t)src,
                                       c + 0x80);
        }
    }


}

void hal_render_player_body_ex(void *player, int with_head)
{
    char *c = (char *)player;
    unsigned id = _ZNK7daPly_c14GetBodyModelIDEjb(c, *(int *)(c + 8) & 0xff, 1);   /* the render form, see hal_render_player_world */
    dExtAnmModel_c *ma = ((dExtAnmModel_c **)(c + 0xdc))[id];
    if (!ma) return;
    for (int i = 0; i < 12; ++i) ((int *)&ma->mat4x3)[i] = 0;
    ((int *)&ma->mat4x3)[0] = 0x1000;
    ((int *)&ma->mat4x3)[4] = 0x1000;
    ((int *)&ma->mat4x3)[8] = 0x1000;
    ma->dExtAnmModel_c::UpdateVerts();
    ma->dExtAnmModel_c::Render(0);
    hal_player_texseq_body(c);

    /* the head is its own model; daPly_c::Render seats it by copying the
       body's neck-bone matrix (+0x2d0 in the bone array) into the head's
       matrix slot, then renders through the object's own vtable */
    unsigned hid = _ZN7daPly_c19func_ov002_020becf4Eii(c, *(unsigned char *)(c + 0x6db), 1);
    if (with_head && hid != 8 && hid != 9) {
        char *head = ((char **)(c + 0x154))[hid];
        if (head) {
            char *src = *(char **)((char *)ma + 0x14) + 0x2d0;
            /* host dExtSimpleModel_c::Render consumes mat4x3; seat the neck transform
               there (the game writes the bone array, but writing both
               double-transforms on host) */
            if (src)
                for (int i = 0; i < 12; ++i)
                    ((int *)(head + 0x1c))[i] = ((const int *)src)[i];
            /* Slot 5, not 4: VObj names every slot by byte offset, so m14 is
               index 5, and _ZTV17dExtSimpleModel_c[5] is dExtSimpleModel_c::Render
               (hal/cxxname_bridge.cpp:522). This read 4 while
               hal_fill_model_vtable dual-filled _ZTV17dExtSimpleModel_c[4] with Render,
               until 0a7f12ee9 removed the dual fill and put Virtual10 back
               at [4]. */
            ((void(__fastcall *)(void *, void *, const void *))(
                ((void ***)head)[0][5]))(head, 0, 0);
            hal_player_texseq_head(c, hid);
        }
    }
}

/* State-machine dispatch: the State objects come from the overlay image
   with DS code addresses baked into their function slots (mwcc PMFs). The
   host ChangeState (port/unmatched/Player_ChangeState.cpp) routes every
   dispatch here; the table grows one line per state the slice hosts.
   Unknown state = loud no-op success so the boot path keeps moving. */
extern "C" int _ZN7daPly_c18St_LevelEnter_MainEv(int *c);
extern "C" int _ZN7daPly_c12St_Jump_InitEv(char *c);
extern "C" void __fastcall func_ov002_020e200c(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c9c4cEv(char *c);
extern "C" void __fastcall func_ov002_020c9de4(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c9d68Ev(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c9d1cEv(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c9cc0Ev(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c9c00Ev(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c9b7cEv(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c9b5cEv(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c9ac0Ev(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c9b10Ev(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c9a04Ev(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c9998Ev(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c990cEv(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c98a4Ev(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c9840Ev(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c97f8Ev(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c97e0Ev(char *c);
extern "C" void __fastcall func_ov002_020e1e70(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020e1c20Ev(char *c);
/* Three state slots the ROM fills with plain ov002 functions rather than
   Player methods: Null's Init, WallJump's Init, InYoshiMouth's Cleanup.
   The community St_ names at those addresses belong to ov006, not ov002. */
extern "C" int _ZN7daPly_c12St_Null_InitEv(void);
extern "C" int _ZN7daPly_c23St_InYoshiMouth_CleanupEv(char *c);
extern "C" int _ZN7daPly_c16St_WallJump_InitEv(void *c);
extern "C" int _ZN7daPly_c16St_BurnLava_MainEv(char *c);
/* gate 14: the level-boot state and the seven entrance-step handlers */
extern "C" int func_ov002_020c6f3c(void *c);
extern "C" void _ZN7daPly_c19func_ov002_020c75f0Ev(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c7350Ev(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c71e0Ev(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c7194Ev(char *c);
extern "C" void __fastcall func_ov002_020c72a4(void *c);
extern "C" void _ZN7daPly_c19func_ov002_020c70acEv(char *c);
extern "C" void _ZN7daPly_c19func_ov002_020c6fe4Ev(char *c);
/* SM64DS_TRACE_STATE: every state function the dispatcher runs. The state
   machine is otherwise opaque from outside -- the Player carries a State* and
   the port switches on the DS address inside it -- and this is how the water
   test reads: walking into the moat has to show 0x020ce550 (St_Swim_Init) and
   then 0x020cd94c (St_Swim_Main), and climbing out has to come back through
   0x020cd1e4 (St_Swim_Cleanup) to St_Walk.
     =1  once per distinct DS address (the quiet census)
     =2  on every CHANGE, with the Player's height, which is what a
         walk-in/swim-across/climb-out run has to be read from */
/* ST_CLIMB's two host entries, port/unmatched/Player_St_Climb.cpp */
extern "C" int port_player_st_climb_init(void *self);
extern "C" int port_player_st_climb_main(void *self);
/* daPly_c::St_EndingFly_Main, under the flat name the ov002 world gives it
   (the sinit's PMF table pairs it with the EndingFly state; see the case). */
extern "C" int _ZN7daPly_c17St_EndingFly_MainEv(char *self);

extern "C" int hal_call_state_fn(void *self, unsigned ds_addr)
{
    {
        static int on = -1;
        if (on < 0) {
            const char *e = std::getenv("SM64DS_TRACE_STATE");
            on = e ? std::atoi(e) : 0;
            if (e && !on) on = 1;
        }
        if (on >= 2) {
            static unsigned prev;
            if (ds_addr != prev) {
                prev = ds_addr;
                std::printf("  [state] 0x%08x y=%.1f\n", ds_addr,
                            *(int *)((char *)self + 0x60) / 4096.0);
            }
            /* =3: the LevelEnter entrance-step spin, byte by byte. The
               step exits on FinishedAnim with the hold byte clear, so
               those two plus the mode/step pair are the whole question. */
            /* =3: the LevelEnter entrance-step spin, whichever step it is.
               Every exit gate the seven step handlers use is here: the hold
               byte, the global countdown, the no-control timer, and the
               animation's cursor against its length. */
            if (on >= 3 &&
                (ds_addr == 0x020c75f0 || ds_addr == 0x020c7350 ||
                 ds_addr == 0x020c71e0 || ds_addr == 0x020c7194 ||
                 ds_addr == 0x020c72a4 || ds_addr == 0x020c70ac ||
                 ds_addr == 0x020c6fe4)) {
                extern unsigned char data_0209f2bc[];
                char *c = (char *)self;
                const unsigned id =
                    _ZNK7daPly_c14GetBodyModelIDEjb(c, *(int *)(c + 8) & 0xff, 0);
                char *anim = *(char **)(c + 0xdc + id * 4) + 0x50;
                std::fprintf(stderr,
                             "  [enter] fn=%08x mode=%u step=%u hold6de=%u "
                             "f2bc=%u hold6a6=%u cur=%d nff=0x%08x fin=%d\n",
                             ds_addr,
                             *(unsigned char *)(c + 0x6e3),
                             *(unsigned char *)(c + 0x6e5),
                             *(unsigned char *)(c + 0x6de),
                             *(unsigned char *)data_0209f2bc,
                             port::player::invincible_timer(c),
                             *(int *)(anim + 8), *(unsigned *)(anim + 4),
                             ((dExtFrameCtrl_c *)anim)->dExtFrameCtrl_c::Finished());
            }
        } else if (on) {
            static unsigned said[64];
            int seen = 0;
            for (int i = 0; i < 64 && said[i]; ++i)
                if (said[i] == ds_addr) seen = 1;
            if (!seen) {
                for (int i = 0; i < 64; ++i)
                    if (!said[i]) { said[i] = ds_addr; break; }
                std::printf("  [state] 0x%08x\n", ds_addr);
            }
        }
    }
    switch (ds_addr) {
#include "player_states.inc"
    /* run linkw / lane l5: the last state-fn addresses in ov002's symbol
       table that the generated .inc does not carry. All three are matched
       src TUs, seated through port/slice_w1l5.txt, and all three are real
       __thiscall methods, so the call goes through the Player class exactly
       the way every method case in the .inc above does.

       Addresses read out of config/arm9/overlays/ov002/symbols.txt, not
       guessed from a neighbouring case:
         _ZN7daPly_c17St_EndingFly_InitEv       0x020c3d6c
         _ZN7daPly_c22St_SwingPlayer_CleanupEv  0x020d9fc4
         _ZN7daPly_c19St_SwingPlayer_InitEv     0x020da3b0

       SwingPlayer's Main is HOSTED from main's matched daPly_c::St_SwingPlayer_Main
       (0x020d9fec; the host transcription that stood here until PORTDROP1 is
       gone). It was the last unhosted half once: Init sets the held partner's pin bit
       (mHeldObj+0xb0 |= 0x800) and only Cleanup clears it, so with Main
       no-op'd the state never advanced, never ChangeState'd out and Cleanup
       never ran -- the partner stayed pinned and the carrier kept a spin
       nothing consumed, the same freeze-then-slide Climb documents.

       EndingFly's Main is NOT the ov007-named body an earlier version of
       this comment blamed. The ov002 sinit's own PMF state table pairs the
       EndingFly state object (0x0211058c) with _ZN7daPly_c17St_EndingFly_MainEv: the
       wave-2 mount lane derived that from the relocation triple, and its
       reviewer re-derived it by reconstructing all 81 state objects from
       the sinit's store sequence. The ov007 body at the same address is a
       different overlay's function that 86 ov007-internal calls reach and
       no PMF cell anywhere names. The case below dispatches the ov002 body;
       its data (the 27-record rising-spiral step table gJumboStarKeyframes
       and the kuppa script sCutsceneGrandStar) rode in with the wave-2 mounts. */
    case 0x020c3d1c: return _ZN7daPly_c17St_EndingFly_MainEv((char *)self);
    case 0x020c3d6c: return ((daPly_c *)self)->daPly_c::St_EndingFly_Init();
    case 0x020d9fc4: return ((daPly_c *)self)->daPly_c::St_SwingPlayer_Cleanup();
    case 0x020da3b0: return ((daPly_c *)self)->daPly_c::St_SwingPlayer_Init();
    case 0x020d9fec: return ((daPly_c *)self)->daPly_c::St_SwingPlayer_Main();
    }
    /* Every miss here is a state the port silently does not run, and the only
       record of it used to be a line in the flight recorder that nobody reads
       until after the glitch. The counter is what the F3 overlay shows live,
       so a state hole announces itself while it is happening. */
    ++g_port_unhosted_hits;
    std::fprintf(stderr, "  [state] unhosted state fn 0x%08x (no-op)\n",
                 ds_addr);
    return 1;
}

/* ---- LIVE CHARACTER SWAP (port mod) ------------------------------------
   The mod is the ENTRY POINT, not the swap. daPly_c::SetRealCharacter is the
   game's own permanent change -- the one the character doors call -- and it is
   already instant: it starts the hat morph and then writes unk_73c = 0, which
   cancels the morph's state machine before a single stage of it runs. So there
   is nothing to reimplement here and nothing to animate away. What the port
   owes it is a caller with guards, because the ROM only ever reaches it from
   one place that has already checked everything.

   All four body and head models are resident from InitResources
   (_ZN7daPly_c19func_ov002_020e5948Ev), so no model loading happens. One thing does load: the
   BCA for (current animation, new character). mCharFileBase is animId * 4, not
   a per-character base, so ANIM_PTRS[base + chr] is a 2D lookup and only the
   current row's Mario column is resident at boot.

   Return codes exist so the caller can print WHY nothing happened rather than
   looking broken: 0 done, 1 no player, 2 out of range, 3 already that one. */
extern void *gPlayers[];        /* per-player Actor*, cxxname_bridge's */
extern short gPlayerHealth[];        /* per-player health, high byte = HP */
extern void *data_ov002_020ff480[];  /* ANIM_PTRS[anim*4 + char] */
extern signed char data_02092114;    /* queued in-level swap, -1 = none */
extern "C" void *_Znwm(unsigned);
extern "C" void *_ZN15dExtAnm2Model_cC1Ev(void *);
extern "C" void _ZN15dExtAnm2Model_c13Func_020162C4EP8BCA_Fileiit(void *, unsigned, int, int, unsigned short);
extern "C" void *_ZN17dExtSimpleModel_c8LoadFileER13SharedFilePtr(void *ptr);
extern "C" int _ZN11dExtModel_c7SetFileEP8BMD_Fileii(void *self, void *file, int a, int b);
extern "C" int _ZN14dExtAnmModel_c7SetAnimEP8BCA_Fileiit(void *, void *, int, int, unsigned int);
extern "C" char data_ov002_0210e780[], data_ov002_0210e6f8[], data_ov002_0210e7b0[];
extern "C" int data_ov002_0210e750[], data_ov002_0210e4c0[];
static void port_player_power_models(char *c)
{
    if (gGameMode == 1) return;                       /* VS builds its own */
    { static int off = -1; if (off < 0) off = std::getenv("SM64DS_NO_POWER_MODELS") ? 1 : 0; if (off) return; }   /* test only: the must-fail row */
    const int lv = (int)gCurrentSublevel;
    if (lv != 2 && lv != 4 && lv != 5) return;            /* every other course built them */
    if (*(void **)(c + 0xEC) != 0) return;
    void *p = _Znwm(0x78);
    if (p) p = _ZN15dExtAnm2Model_cC1Ev(p);
    *(void **)(c + 0xEC) = p;
    if (!p) return;
    *(unsigned char *)(c + 0x718) |= 0x20;
    _ZN11dExtModel_c7SetFileEP8BMD_Fileii(p, _ZN17dExtSimpleModel_c8LoadFileER13SharedFilePtr(data_ov002_0210e780), 1, -1);
    _ZN15dExtAnm2Model_c13Func_020162C4EP8BCA_Fileiit(p, (unsigned)((int *)data_ov002_020ff480[0xC6])[1], 0, 0x1000, 0);
    _ZN11dExtModel_c7SetFileEP8BMD_Fileii(c + 0xF0, _ZN17dExtSimpleModel_c8LoadFileER13SharedFilePtr(data_ov002_0210e6f8), 1, 1);
    _ZN14dExtAnmModel_c7SetAnimEP8BCA_Fileiit(c + 0xF0, (void *)(size_t)data_ov002_0210e750[1], 0, 0x1000, 0);
    _ZN11dExtModel_c7SetFileEP8BMD_Fileii(c + 0x174, _ZN17dExtSimpleModel_c8LoadFileER13SharedFilePtr(data_ov002_0210e7b0), 1, 1);
    _ZN14dExtAnmModel_c7SetAnimEP8BCA_Fileiit(c + 0x174, (void *)(size_t)data_ov002_0210e4c0[1], 0, 0x1000, 0);
    fprintf(stderr, "[balloon] Player %p: course %d builds no balloon / wing models; built them before the first Behavior\n", (void *)c, lv);
}

void _ZN15dExtAnm2Model_c4CopyERKS_PcP8BCA_File(void *self, const void *src, char *nf,
                                  unsigned nof);

/* THE DOOR SWAP, operating on a given player. This is the game's own in-place
   change (daPly_c::SetRealCharacter, the path the character doors take), which
   loads the incoming character's files through data_ov002_020ff480 and arms the
   hat morph BEFORE it moves param1, then cancels the morph so the change is
   instant. It replaces nothing about the Player except who it is -- the model,
   the animation cursor, the state and the position all carry across untouched,
   so there is no cold restart and no stand-freeze. Returns the same codes as
   port_set_character (0 done, 1 no player, 2 out of range, 3 already that one).
   port_set_character wraps this on the local player; port_player_set_character
   routes to it as the default swap. */
static int port_door_swap(char *c, int chr)
{
    if (!c) return 1;
    /* SetRealCharacter masks with & 3 in two places but uses chr RAW in the
       other two (the ANIM_PTRS index and the chr + 0xc4 default-anim index),
       so an out-of-range value would read past the table into whatever ov002
       packed next. Reject it -- masking would silently hand back a different
       character than the caller asked for. */
    if (chr < 0 || chr > 3) return 2;
    /* the body ModelAnim2 slots: null until InitResources has run, and
       SetRealCharacter dereferences one with no check of its own */
    if (!*(void **)(c + 0xdc)) return 1;
    if ((int)(*(unsigned *)(c + 8) & 0xff) == chr) return 3;

    /* A cap collect in flight is CANCELLED, not refused. Cancelling is what an
       instant swap means, and refusing would be a trap: the debug menu pauses
       the tick, so unk_73c can never advance while someone is holding the menu
       open. Say it happened so an odd-looking result is attributable. */
    if (((daPly_c *)c)->daPly_c::IsCollectingCap())
        std::fprintf(stderr, "[chr] swap during a cap collect (unk_73c=%04x);"
                             " the morph is cancelled\n",
                     *(unsigned short *)(c + 0x73c));

    /* SetRealCharacter ends in Heal(0x880) unconditionally. Save the whole
       short, not just the HP byte -- the low byte is a fraction GiveHealth
       works in. Without this every swap visibly refills the HUD hearts, which
       makes the row useless for anything you were testing damage against. */
    /* THE HEALTH TABLE IS SIXTEEN WIDE, so mPlayerNo is read WHOLE. The `& 3`
       that used to be here predated 0.3.2's widening of gPlayerHealth to
       the port's full slot count (hal/cxx_aliases.cpp:70), and it was wrong the moment
       slots 4..15 could exist: a swap in slot 5 saved slot 1's health and then
       wrote slot 1's health back over whatever slot 1 had done in between.
       mPlayerNo is a whole byte and the port writes the TRUE slot into it
       (hal/level_boot.cpp, right after Actor::Spawn returns), so the only
       thing owed here is a bound, not a mask. */
    unsigned pno = *(unsigned char *)(c + 0x6d8);
    const short saved_hp = gPlayerHealth[pno];
    /* which body model is animating RIGHT NOW, before param1 moves */
    const unsigned old_id =
        _ZNK7daPly_c14GetBodyModelIDEjb(c, *(unsigned *)(c + 8) & 0xff, 0);

    ((daPly_c *)c)->daPly_c::SetRealCharacter((unsigned)chr);

    /* KNOWN, LEFT ALONE DELIBERATELY: Luigi's head draws with the toon/flash
       shading the power flower uses. The swap does not turn it on -- unk_73c
       = 0 inside SetRealCharacter cancels func_ov002_020be3b0 before stage 1's
       SetPolygonMode(m, 2) can run, which is what makes the swap instant. It
       means stage 7, the only thing that turns the flash OFF, never runs
       either, so a model already in that polygon mode keeps it. A
       daPly_c::TurnOffToonShading(chr) here clears it, but that is a rendering
       question about Luigi's head model rather than part of the swap, and it
       has not been eyeballed. Chip it separately. */

    /* THE OTHER THING THE ROM'S CALLER ALREADY HAD RIGHT. data_02092114 is the
       QUEUED in-level swap -- the file-select picker writes the character it
       wants and _ZN7daPly_c19func_ov002_020be008Ev performs it on the next landing, spawning
       the cap actor and freezing the player in a no-control state while the
       poof plays. -1 means "nothing queued", and the port never seats it: it
       is plain zero-initialised storage, which reads as "a swap to Mario is
       pending". Inert for as long as you ARE Mario, which is why nothing has
       ever noticed. The moment param1 becomes anything else the poll fires and
       drags you straight back, ~28 frames after the swap, through a state the
       port does not host -- so you end up frozen as the character you asked
       for. SetRealCharacter is a direct swap, so the honest value afterwards is
       the one 020be008 itself writes when it has consumed the request. */
    data_02092114 = -1;

    gPlayerHealth[pno] = saved_hp;
    /* The body renders off param1, which the call above wrote, but the HEAD
       renders off unk_6db, and the only thing that re-syncs unk_6db from
       param1 is the tail of _ZN7daPly_c19func_ov002_020e4bb8Ev -- inside daPly_c::Behavior.
       With the menu open the tick is skipped, so without this the new body
       wears the old head for exactly as long as somebody is looking at it.
       This is that same assignment, one tick early, and it is idempotent once
       the tick resumes. */
    *(unsigned char *)(c + 0x6db) = (unsigned char)chr;

    /* THE ONE THING SetRealCharacter LEAVES TO ITS CALLER, and the reason a
       swap that looked fine faulted on the next tick.

       Its tail is dExtAnm2Model_c::Copy(body[m1], body[m2], newFile, 0) -- but it
       assigns param1 = chr BEFORE computing both ids, so m1 == m2 and the copy
       is a model onto itself. That cannot carry an animation across, and
       body[chr] has no base Animation to begin with: InitResources
       (_ZN7daPly_c19func_ov002_020e5948Ev) seats only otherAnim for all four models and the
       base Animation for whoever you actually are. So the new model comes out
       with numFramesAndFlags == 0, and Animation::Advance's `% len` on the
       very next _ZN7daPly_c12AdvanceAnimsEv is an integer divide by zero.

       On the ROM this is invisible, because SetRealCharacter has exactly one
       caller -- the character door -- and the door's state immediately calls
       daPly_c::SetAnim with a DIFFERENT animation id, which takes SetAnim's
       slow path and seats the frame count off the new file. A swap that does
       not change animation never gets that. Calling SetAnim here would not
       help either: dExtAnmModel_c::SetAnim fast-paths when the file pointer already
       matches, and SetRealCharacter's own Copy already stored it.

       So do what the hat morph does. This is func_ov002_020be3b0's stage-3
       call with the operands it has: dst != src, which carries the live
       cursor, speed and frame count off the character who was walking a moment
       ago onto the one who is now. */
    {
        const unsigned base = *(unsigned *)(c + 0x63c);
        const unsigned new_id =
            _ZNK7daPly_c14GetBodyModelIDEjb(c, (unsigned)chr, 0);
        void *dst = *(void **)(c + 0xdc + new_id * 4);
        void *src = *(void **)(c + 0xdc + old_id * 4);
        /* SharedFilePtr's layout is not recovered; +4 is the file pointer every
           caller in src/ indexes, SetRealCharacter's own Copy argument included.
           It is a real load off the file seam, so it can come back empty. */
        char *bca = *(char **)((char *)data_ov002_020ff480[base + chr] + 4);
        if (dst && src && dst != src && bca)
            _ZN15dExtAnm2Model_c4CopyERKS_PcP8BCA_File(dst, src, bca, 0);
        /* and say so if it still came out unplayable, rather than leaving the
           divide to announce it */
        if (!bca || !dst ||
            (*(unsigned *)((char *)dst + 0x54) & ~0xc0000000u) == 0)
            std::fprintf(stderr, "[chr] character %d came out with no playable "
                                 "body animation (anim base %u, file %p) -- "
                                 "Advance will divide by zero\n",
                         chr, base, (void *)bca);
    }
    return 0;
}

/* the local-player wrapper the debug menu reached for first */
int port_set_character(int chr)
{ return port_door_swap((char *)gPlayers[0], chr); }

void _ZN7daPly_c16InitWingFeathersEb(void *self, unsigned char b_)
{ ((daPly_c *)self)->daPly_c::InitWingFeathers(b_ != 0); }
extern "C++" int ApproachLinear(int &ref, int target, int step);
int _Z14ApproachLinearRiii(int *ref, int target, int step)
{ return ApproachLinear(*ref, target, step); }
int hal_anim_willhit(void *self, int f)
{ return ((dExtFrameCtrl_c *)self)->dExtFrameCtrl_c::WillHitFrame(f) ? 1 : 0; }
int hal_nhi_next(void *self, void *h)
{ return (int)(size_t)NNS_FndGetNextListObject((NNSFndList *)self, h); }

void _ZN15dExtFrameCtrl_c7AdvanceEv(void *self)
{ ((dExtFrameCtrl_c *)self)->dExtFrameCtrl_c::Advance(); }
int _ZN15dExtFrameCtrl_c8FinishedEv(void *self)
{ return ((dExtFrameCtrl_c *)self)->dExtFrameCtrl_c::Finished(); }
char *_ZN15dExtFrameCtrl_c8LoadFileER13SharedFilePtr(void *fp)
{
    if (!fp) {
        std::fprintf(stderr, "  [anim] LoadFile on NULL fileptr (table hole)\n");
        return 0;
    }
    return (char *)dExtFrameCtrl_c::LoadFile(*(SharedFilePtr *)fp);  /* main returns BCA_File * */
}

int _ZN7daPly_c6IsAnimEj(void *self, unsigned a)
{ return ((daPly_c *)self)->daPly_c::IsAnim(a); }
int _ZN7daPly_c12FinishedAnimEv(void *self)
{ return ((daPly_c *)self)->daPly_c::FinishedAnim(); }
int _ZN7daPly_c17SetNoControlStateEhih(void *self, unsigned char a, int b,
                                      unsigned char c)
{ return ((daPly_c *)self)->daPly_c::SetNoControlState(a, b, c); }
int _ZN7daPly_c8HasNoCapEv(void *self)
{ return ((daPly_c *)self)->daPly_c::HasNoCap(); }
int _ZN7daPly_c9GetHealthEv(void *self)
{ return ((daPly_c *)self)->daPly_c::GetHealth(); }

void _ZN5dBgCh19StartDetectingWaterEv(void *self)
{ ((dBgCh *)self)->dBgCh::StartDetectingWater(); }

/* THE CYLINDER SHADOW IS NO LONGER DEFERRED (run linkw wave 4, lane w4-a).
   This was `void _ZN11ShadowModel12InitCylinderEv(void *) {}`, the stub that
   kept the matched body out of the image; it is now the ordinary C-name-to-
   method bridge, the same shape as _ZN11ShadowModel10InitCuboidEv in
   hal/cxxname_bridge.cpp, and the body it reaches is the matched
   src/_ZN11ShadowModel12InitCylinderEv.cpp.

   The three things that had to be true first, and are:
     - the template BMD at data_ov001_020ad560 is static .data in overlay 1, 0x3c
       bytes with one bone at 0x020ad5dc and one material at 0x020ad4c4, and
       wave 3 named that chain in port/ov001_syms.txt;
     - the host zero copy of data_ov001_020ad560 that would have shadowed the mount
       is gone from hal/actor_vtables.cpp, which now aliases the arm9 spelling
       onto the ov001 one;
     - _ZTV11ShadowModel[1] holds ShadowModel::DoSetFile (hal/model_dtor_seat
       .cpp, seated in wave 1 and called from port_stage_a2_seat), so the
       vtable dispatch dExtModel_c::SetFile makes lands on a real body.

   IT RETURNS int, AND THE VALUE IS DERIVED RATHER THAN CHOSEN. The ROM's
   InitCylinder is a TAIL BRANCH into SetFile, which tail-branches into
   DoSetFile, so r0 leaves this function carrying DoSetFile's result -- and
   BobOmb::InitResources tests it (`if (InitCylinder() == 0) return 0;`, see
   the ride-through block in hal/bob_enemy_bridges.cpp). The matched
   ShadowModel::InitCylinder and dExtModel_c::SetFile are both declared void by
   the decomp, so the value cannot ride out of a host call the way it rides out
   of the ARM one. It does not have to be guessed at either: matched
   src/_ZN11ShadowModel9DoSetFileEPcii.cpp returns 0 exactly when it has just
   stored data = 0 and 1 in every other path, so `data != 0` read back off the
   object after the call IS that return value, not a stand-in for it.

   THE SEAT CHECK IS NOT DEFENSIVE PROGRAMMING, IT IS A MEASURED HARNESS GAP,
   and it is the wave-3 abort-vs-no-op lesson (hal/cxxname_bridge.cpp) landing
   in a new place. The matched body's SetFile dispatches DoSetFile through
   _ZTV11ShadowModel[2] -- ROM numbering, as of the destructor respelling in
   include/ModelBase.h; it was [1] while MSVC folded the pair -- which
   hal/model_dtor_seat.cpp seats inside
   hal_seat_model_family_dtors -- and that runs from port_stage_a2_seat, so
   walk_window and walk_window_hires have it from process start. smoke_player
   does NOT: it hand-fills the host vtables and calls hal_fill_shadow_vtable,
   which by its own comment seats slot 0 only. Measured, not assumed -- with
   the bridge unguarded, smoke_player faults c0000005 accessing 0 with
   ?InitCylinder@ShadowModel@@QAEXXZ + 0xe on the stack under
   _ZN7daPly_c19func_ov002_020e5948Ev + 0x6d7 (Mario's own drop shadow, Player + 0x2ac), while
   walk_window runs 1000 frames through that same call.

   So the guard fires only where the host never filled the slot, and there it
   reproduces that harness's baseline exactly: an InitCylinder that installs
   nothing, which is what the stub gave smoke_player for its whole green
   history. It returns 0 for the same reason the line below returns data != 0
   -- no DoSetFile ran, so no data was attached -- and the one caller in that
   target ignores the value anyway (port/unmatched/TexSeq_Caller_ov002_
   020e5948.cpp declares this void). It complains on stderr the first time
   rather than staying silent, because in a target that DOES seat the slot the
   same line would mean the seat had regressed and the shadows had stopped
   drawing. The real fix is one call in tests/smoke_player.cpp
   (hal_seat_model_family_dtors, beside the four hal_fill_*_vtable calls) or
   one line in hal/cxxname_bridge.cpp's hal_fill_shadow_vtable; neither file is
   this lane's. */
int _ZN17dExtShadowModel_c12InitCylinderEv(void *self)
{
    void **vt = *(void ***)self;
    if (!vt || !vt[2]) {
        static int told;
        if (!told) {
            told = 1;
            std::fprintf(stderr, "InitCylinder: _ZTV17dExtShadowModel_c[2] "
                         "(DoSetFile) is null in this target -- no shadow "
                         "installed\n");
        }
        return 0;
    }
    ((dExtShadowModel_c *)self)->dExtShadowModel_c::InitCylinder();
    return ((dExtShadowModel_c *)self)->data != 0;
}

/* PORT_HOST_ABI CORRECTION (run link100, lane CRASH3). dExtAnmTexPat_c::Prepare
   is STATIC, and include/TextureSequence.h carries the cartridge evidence for it:
   the ROM body is a 0xc long-call veneer into func_02046d50, which reads only r0
   and r1, never touches r2, and reads r1 at exactly BTP_File's own field offsets.
   Two words and no this -- that IS the ROM's call surface.

   The three-parameter face this replaces was written against the stale belief
   that Prepare was a non-static method (the same belief is still spelled out in
   port/tools/hostgen.py's REG_RIDE_ARG comment and in the two retired host
   copies). Because the callee is in fact static, MSVC compiled
   `((dExtAnmTexPat_c *)self)->Prepare(*bmd, *btp)` by evaluating self, DISCARDING
   it, and pushing only the 2nd and 3rd parameters. Read out of the shipped
   artifact at 004106e0:

       push ebp / mov ebp,esp
       push dword ptr [ebp+0x10]      <- 3rd parameter
       push dword ptr [ebp+0x0c]      <- 2nd parameter
       call ?Prepare@dExtAnmTexPat_c@@SAXAAUBMD_File@@AAUBTP_File@@@Z
       add esp,8 / pop ebp / ret

   [ebp+8] is never read, and the callee's own mangling ("SAX") says static. So
   the face shifted every argument down by one. All 17 call sites in the image
   pass the BMD as their FIRST argument, so func_02046d50 was handed the BTP file
   where it wanted the BMD; func_020471ac then read [BTP+0x14] as a texture count
   and [BTP+0x18] as a texture array, walked into the BTP's own bytes, and handed
   a run of characters to cstd::strcmp as a pointer.

   Two parameters is the ROM's shape and is right for every caller at this cdecl
   ABI: the fourteen ROM-arity sites pass exactly these two words, and the three
   sites the REG_RIDE_ARG row patched pass (bmd, btp, btp), whose first two words
   are the same two -- the caller cleans up its own extra push. */
void _ZN15dExtAnmTexPat_c7PrepareER8BMD_FileR8BTP_File(void *bmd, void *btp)
{ dExtAnmTexPat_c::Prepare(*(BMD_File *)bmd, *(BTP_File *)btp); }
void *_ZN15dExtAnmTexPat_c8LoadFileER13SharedFilePtr(void *fp)
{ return dExtAnmTexPat_c::LoadFile(*(SharedFilePtr *)fp); }

/* The old flat names of NitroSystem's list calls (decomp main 63e1097b379
   respelled the coined NestedHeapIterator as NNS_FndRemoveListObject and
   NNS_FndGetPrevListObject), kept for callers still spelling them. */
void _ZN18NestedHeapIterator6RemoveEP13HeapAllocator(void *self, void *node)
{ NNS_FndRemoveListObject((NNSFndList *)self, node); }
int _ZN18NestedHeapIterator8PreviousEP13HeapAllocator(void *self, void *h)
{ return (int)(size_t)NNS_FndGetPrevListObject((NNSFndList *)self, h); }

/* SYNC4: main declares Heap::Rescue void. The face keeps its int signature
   because its C callers are declared that way, and returns 0 rather than
   whatever EAX happens to hold -- the ROM body sets no result either, so the
   value is unread; see the falls-off-the-end block in port/CMakeLists.txt for
   the same question asked of the src bodies. */
int _ZN5mHeap6Heap_t6RescueEv(void *self)
{ ((Heap *)self)->Heap::Rescue(); return 0; }
int _ZN5mHeap6Heap_t21MaxAllocationUnitSizeEv(void *self)
{ return ((Heap *)self)->Heap::MaxAllocationUnitSize(); }
int _ZN5mHeap6Heap_t6IntactEv(void *self)
{ return ((Heap *)self)->Heap::Intact() ? 1 : 0; }

/* gate-10 BSS, second ring */
/* data_0209b44c is NOT here: it is the one-byte area id, hosted in
   hal/actor_vtables.cpp so the smoke targets that link no player bridge
   still get it, and so writer and reader share one object. */
DSSTATE_BEGIN
int sCurrentBank[8] = { -1 };   /* gSoundInitialized is src/Sound.cpp's u8; retail sCurrentBank is .data -1 (Sound.cpp: static signed char sCurrentBank = -1), and the first byte is the object */
int data_020a4d60[8], data_020a6438[8], data_020a6488[4], data_020a648c[4];
int data_020a6490[4], data_020a649c[4], data_020a64a0[4], data_020a64a4[4];
/* The sound command queue's SIZED object. A placeholder while sound was
   stubbed; the hosted ARM7 in hal/sdat/ needs its real extent.
   data_020a64a8 is the batch ring that SND_FlushCommand and SND_RecvCommandReply index
   with "idx++; if (idx > 8) idx = 0", so it needs nine slots. At the old
   size, the ring walked off the end of a 32-byte object.

   data_020a6760 IS NOT HERE ANY MORE (run link100, lane SND1, rung R1). It was
   `int data_020a6760[256 * 0x18 / sizeof(int)]` -- the right 0x1800 extent, but
   as a lone object. src/func_0205b358.c, the ROM's own SND_Init, addresses the
   same 0x1800 bytes under THREE dsd names (data_020a6760, data_020a7760 + 0x7e8
   for the last node's link, and data_020a7f48 for the tail pointer), so hosting
   only the first of them left the other two nowhere and the body unrunnable.
   hal/snd_globals.cpp hosts all three as one run with the names at their ROM
   offsets, which is what let the transcription in hal/sdat/consumer.cpp retire.
   Defining it here as well would be a duplicate symbol. */
void *data_020a64a8[16];
int data_020a0f1c[4], data_020a4d54[4], OSi_UseAlarm[4], OSi_AlarmQueue[4];
int data_020a6484[4], data_020a6494[4], data_020a6498[4];
int data_0209cdd0, data_0209cdd4, data_0209cdd8, data_0209cddc, data_0209cde0;
/* gCurrentStarId and gCurrentEntranceId are u8 globals of src/game/stages/d_s_stage_methods.cpp now. */
int data_020a0d90[8], data_020a0f38[8];
int data_020a4b58[4], data_020a4b68[4];
/* data_020a60f4 is not here: it is OSi_IrqCallbackInfo + 0x30 on the DS and
   ntr/runtime.cpp hosts the two as one band (the timer callbacks). */
/* DTCM, AND IT IS 16 KB AND NOT 64 BYTES. This used to read "DTCM scratch the
   timer list walker anchors at" and be sized for that one reader, which is the
   undersized-hosted-global shape: a span decided by the first caller found
   rather than by the ROM.
   THE SPAN IS THE ARM9's DATA TCM, 0x023c0000..0x023c4000, and what fixes the
   size is the BIOS interrupt check flag at the top of it, DTCM_END - 8, the
   NitroSDK OSi_IrqCheckFlag slot. FIVE handlers in src write that word by
   literal offset, and only two of them are in this link:
       _Z18Wipe_HBlankHandlerv            |= 2       slice_fdr, AND REACHED, 191x a frame
       OSi_ArrangeTimer            |= 0x10    slice_gate10, linked and undriven
       _ZN3IRQ13VBlankHandlerEv |= 1       not compiled
       OSi_IrqCallback |= mask    not compiled
       _Z22Luigi_PrepareScanlinesv      |= 2       not compiled
   At 64 bytes that store landed 16,312 bytes past the object, 56 bytes into
   _hal_area_table and so INSIDE .dsstate, which means a save state would have
   captured the corruption and a restore replayed it. It was invisible only
   because no host path had ever RUN one of the five: the ntr layer stored no
   handler for mask 1, 2 or 0x10, so none was ever dispatched. Raising IRQ 2
   drives the first one for real, and the second goes live the day anyone
   models timer interrupts, so the span has to be real before either.
   See port/irq2_map.txt section 8. */
__declspec(align(8)) unsigned char data_023c0000[0x4000];
/* data_02099e94 / ebc / ec4 are NOT here any more: they are three of the
   twelve virtual member-pointer records fBase_c::Process is handed, hosted
   as the ROM's {face, 0} pairs in hal/pmf3_cells.cpp (run linkfull, PMF3). */
int sIsDispOn[4];
/* data_020a6088 is NOT here any more. It is the head of the GX bank-state
   block, whose members the SetBankFor* family reaches by STRUCT OFFSET out to
   +0x18 and which GX_InitGXState clears 26 bytes at a time, and the `int[2]`
   that used to sit here was both too small for that and not adjacent to the
   twelve u16 symbols the ROM has after it. Eight bytes was already generous:
   the symbol's own span in config/arm9/symbols.txt is two. The whole band is
   laid out in ROM order in hal/cxx_aliases.cpp; read the banner there. */
int sDispMode[4], PMi_Work[4];
DSSTATE_END

}  /* extern "C" */

/* ---- THE GAME'S OWN CHARACTER CHANGE ---------------------------------------
   daPly_c::SetRealCharacter is the cap-block path, the thing that runs when you
   break a Luigi or Wario cap, hat animation and all. It is also the only
   correct way to change character in place, because it does the part a bare
   index write cannot: it Releases the outgoing character's file, LoadFiles the
   incoming one, re-points the ModelAnim2 at it, swaps the voice bank through
   _ZN7daPly_c18PlayCharacterSoundEv and writes the save byte at data_0209caa0[0x41] so the
   choice survives. All of it is matched src already in the slices, gate 22 for
   this and gate 10 for the rest.

   THIS WRAPPER EXISTS ONLY FOR THE CALLING CONVENTION. The Itanium name is
   /alternatename'd onto ?SetRealCharacter@daPly_c@@QAEXI@Z, which is
   __thiscall, so calling it through the C name from a TU without Player.h
   passes `this` on the stack and leaves ecx holding whatever was there. That
   is the garbage-ecx trap the port has already been bitten by three times.
   Here the real header is in scope and the call is an ordinary method call. */
extern "C" { extern void *data_ov002_020ff480[]; }

/* THE LEGACY SWAP: rebuild the whole Player by re-running InitResources with a
   rewritten spawn param. It works, but it is a COLD restart -- InitResources
   re-runs _ZN7daPly_c19func_ov002_020e5948Ev (the model/anim resource init), re-seats the
   collision, and re-places the Player at the entrance, so the animation system
   comes back at frame 0 and the Player stands frozen for a second or two while
   it settles. That freeze is the whole reason for the door path below. Kept
   behind SM64DS_SWAP_LEGACY=1 as a fallback while the door path proves out.

   The param's layout is InitResources' own unpacking, read back out of the
   fields it wrote: bits 0-2 character, 3-5 the sub value at +0x6da, 6-7 the
   entrance index at +0x6d8. Bits 8+ feed func_ov002_020c7dd0's entrance type,
   which a mid-level swap has no business re-running, so they go in as 0.
   Position, angle and speed are carried across because InitResources places the
   Player at the entrance and the point is to change who you are, not where. */
static void port_legacy_set_character(void *player, unsigned ch)
{
    char *c = (char *)player;
    int pos[3], spd[4];
    short ang;
    unsigned param;

    ch &= 3;
    pos[0] = *(int *)(c + 0x5c);
    pos[1] = *(int *)(c + 0x60);
    pos[2] = *(int *)(c + 0x64);
    ang = *(short *)(c + 0x8e);
    spd[0] = *(int *)(c + 0x98);
    spd[1] = *(int *)(c + 0xa4);
    spd[2] = *(int *)(c + 0xa8);
    spd[3] = *(int *)(c + 0xac);

    /* run pal16: THE TWO-BIT FIELD, AND THE TRUE SLOT CARRIED ACROSS IT.
       mPlayerNo is a whole byte and the port writes the TRUE slot into it
       (hal/level_boot.cpp), but the flag word's slot field is bits 6-7 and
       nothing else. Shifting a raw mPlayerNo in here was wrong twice over for
       a slot at or above four:

         - `5 << 6` is 0x140, so bit 8 went in SET, and this function's own
           note three paragraphs up promises bits 8+ go in as 0 because they
           feed func_ov002_020c7dd0's entrance type, which a mid-level swap
           has no business re-running. Masking is what makes that note true.
         - InitResources then unpacked `(a >> 6) & 3` back into +0x6d8 and
           recomputed the VS palette word at +0x61C from it, so a legacy swap
           in slot 5 came back as player 1 wearing row 1. That is exactly the
           defect run pal16 fixed at the spawn, reverted here.

       THE PORT HAS THREE CALLERS OF InitResources AND THIS IS THE ONLY ONE
       THAT NEEDED THIS. hal/level_boot.cpp's ps_init is the vtable slot, so it
       runs INSIDE Actor::Spawn and is exactly what the spawn-side repair
       already corrects on the way out. hal_player_init_resources just above is
       the gate-10 smoke's direct entry, one Mario at slot 0 in a harness with
       no VS and no second player, where this delta is zero by construction.
       This one is the only place a LIVE player at an arbitrary slot is rebuilt
       mid-level, which is why it is the only place the invariant could come
       back wrong.

       So the field is masked going in, and both halves of the identity are
       put back afterwards -- the same delta the spawn uses, for the same
       reason: one step of mPlayerNo is one sixteen-colour row. A slot 0..3
       masks to itself and the repair is a no-op, which is every run of the
       legacy path that has ever been taken. */
    const unsigned true_slot = *(unsigned char *)(c + 0x6d8);
    const unsigned packed_slot = true_slot & 3;

    param = ch | ((unsigned)*(unsigned char *)(c + 0x6da) << 3) |
            (packed_slot << 6);
    *(int *)(c + 8) = (int)param;
    *(unsigned char *)(c + 0x6d9) = (unsigned char)ch;

    _ZN7daPly_c13InitResourcesEv(c);

    if (true_slot != packed_slot) {
        *(int *)(c + 0x61c) += ((int)true_slot - (int)packed_slot) << 1;
        *(unsigned char *)(c + 0x6d8) = (unsigned char)true_slot;
    }

    *(int *)(c + 0x5c) = pos[0];
    *(int *)(c + 0x60) = pos[1];
    *(int *)(c + 0x64) = pos[2];
    *(short *)(c + 0x8e) = ang;
    *(int *)(c + 0x98) = spd[0];
    *(int *)(c + 0xa4) = spd[1];
    *(int *)(c + 0xa8) = spd[2];
    *(int *)(c + 0xac) = spd[3];
}

/* CHANGE CHARACTER ON THE SPOT. Every entry point in the port -- F4, the debug
   menu row, SM64DS_SWITCH, SM64DS_SELFTEST_SWAP -- comes through here, so the
   default swap path is chosen in one place.

   THE DEFAULT IS NOW THE DOOR PATH (port_door_swap). It is the game's own
   in-place change: it loads the incoming character's files through
   data_ov002_020ff480 and arms the hat morph engine (func_ov002_020be3b0, which
   daPly_c::Behavior already ticks every frame) BEFORE it moves param1, then
   cancels the morph so the change is instant. The model, the animation cursor,
   the current state and the position all carry across, so there is no cold
   restart and no stand-freeze -- which is the whole point of preferring it.

   The old worry was that the door path could not do Yoshi, because
   SetRealCharacter arms the CAP morph and there is no Yoshi cap, leaving a
   zero-length Animation that divides by zero on the next Advance. port_door_swap
   already closes that: after SetRealCharacter it does the hat morph's own
   stage-3 dExtAnmModel_c::Copy with dst != src, carrying the live cursor and frame
   count off the outgoing character onto the incoming one, so the new body has a
   playable animation whether or not a cap was involved.

   SM64DS_SWAP_LEGACY=1 forces the old InitResources rebuild for an A/B. */
extern "C" unsigned char data_0209cad2[];   /* save block; [0x0f] is caa0[0x41] */
extern "C" unsigned char data_02092128[];   /* per-player character, the spawn's f1 (hal/actor_vtables.cpp) */
extern "C" int sPlayerVoiceGroup[];             /* voice-group byte (hal/actor_vtables.cpp) */
extern "C" void _ZN7daPly_c18PlayCharacterSoundEv(void *self);   /* src/_ZN7daPly_c18PlayCharacterSoundEv.c */
extern "C" void _ZN7daPly_c19func_ov002_020e6350Ev(char *self);   /* src/_ZN7daPly_c19func_ov002_020e6350Ev.c */
extern "C" void port_player_set_character(void *player, unsigned ch)
{
    static int legacy = -1;
    if (legacy < 0) legacy = std::getenv("SM64DS_SWAP_LEGACY") ? 1 : 0;
    if (legacy) { port_legacy_set_character(player, (int)(ch & 3)); return; }

    /* SetRealCharacter is the LOCAL player's operation (the character doors):
       besides the body it writes the SAVE FILE's character byte
       (data_0209caa0[0x41]) and runs the voice-group setup that sets
       sPlayerVoiceGroup. A remote swap (a remote body changing to its sender's
       character) must not touch any of that, or the watcher's own save
       character becomes the last remote character seen, and the next course
       entered through the star select makes the watcher that character
       (field test 4, F3). So for any body that is not the local one: keep
       both bytes across the swap, and re-run the voice setup on the local
       body, exactly what daPly_c::InitResources does for the local slot. */
    char *const localp = (char *)gPlayers[gLocalPlayerId];
    const bool remote = player != 0 && player != (void *)localp;
    unsigned char *const save_chr = &data_0209cad2[0x0f];  /* data_0209caa0[0x41] */
    const unsigned char keep_save = *save_chr;
    const unsigned char keep_voice = *(unsigned char *)sPlayerVoiceGroup;
    /* THE PER-PLAYER TABLE IS THE THIRD THING SetRealCharacter WRITES
       (src/game/player/Player.cpp:1226, data_02092128[mPlayerNo] = param1),
       and for a remote body it has to be kept exactly like the save byte.
       The spawn loop (src/game/stages/LevelObjects.cpp:203-208) packs body i
       as f2 = the save byte (mCharacter, the same for every body) and
       f1 = data_02092128[i] (param1, the look), and outside the castle
       daPly_c::InitResources keeps param1 = f1 (Player.cpp:27022-27037). Left
       at the friend's character, the table built that friend's body at the
       next level load half as the friend (param1) and half as me
       (mCharacter). The cartridge's cap rule (dCapEnemy_c::
       DestroyIfCapNotNeeded) then threw away any enemy wearing my cap whose
       nearest body at load was that one: a Luigi console lost 4 of the 6
       entrance-hall Boos in Big Boo's Haunt. Kept, every body comes up with
       param1 == mCharacter, as the ROM's own loop builds one player, and the
       friend's look is put back by the remote swap as before. */
    const int tbl_slot = remote ? (int)(*((unsigned char *)player + 0x6d8) & 0x3f) : -1;
    const unsigned char keep_tbl = remote ? data_02092128[tbl_slot] : 0;

    int rc = port_door_swap((char *)player, (int)(ch & 3));
    /* the door path can decline (no player, already that character); the codes
       are informational, but on an outright "no player" fall back to the legacy
       rebuild rather than leaving the caller with nothing happening. */
    if (rc == 1)
        port_legacy_set_character(player, (int)(ch & 3));

    if (remote) {
        {
            *save_chr = keep_save;
            data_02092128[tbl_slot] = keep_tbl;
            *(unsigned char *)sPlayerVoiceGroup = keep_voice;
            if (localp && *(void **)(localp + 0xdc)) {
                _ZN7daPly_c18PlayCharacterSoundEv(localp);
                _ZN7daPly_c19func_ov002_020e6350Ev(localp);
            }
        }
    }
}

static int g_vs_chars[4];



/* catchup1 round 3: src/Sound.cpp defines gSoundInitialized. The C name reaches that storage: */
#pragma comment(linker, "/alternatename:_gSoundInitialized=?gSoundInitialized@@3EA")
