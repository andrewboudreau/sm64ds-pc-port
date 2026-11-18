// GATE 176: SCUTTLEBUG (255, ov071), the spider-legged Scuttlebug daSpd_c that
// skitters after Mario in a few caves and grounds. First class hosted out of
// ov071 (the eyeball MrI, MrI_Projectile and Coffin share the overlay); this is
// its FIRST mount, so ov071 joins PORT_ACTOR_OVERLAYS and its four sinits run.
//
// Same law as hal/actor_classes_flame.cpp and hal/actor_classes_montymole.cpp
// -- ROM slot order, __fastcall thunks that call QUALIFIED or by C name,
// unhosted slots trap by name -- and Scuttlebug is an Enemy subclass, so its
// vtable is the 31-slot shape: the reloc span [0x02122c2c, 0x02122ca8) is 31
// words, D1 slot 16 and D0 slot 17. Every slot below was read out of
// _ZTV7daSpd_c (= _ZTV7daSpd_c, 0x02122c2c) with its 31 relocations applied
// and cross-checked against BlueFlame's table: the shared Actor/ActorBase halves
// (1/2/4/5/7/8/10/11/13/14/15/20..28/30) hold the identical arm9 addresses.
//
// The class owns 0 (Init), 3 (Cleanup), 6 (Behavior), 9 (Render), 12
// (OnPendingDestroy, an empty body of its OWN -- unlike the flame, which uses
// ActorBase's), 16 (D1), 17 (D0), and the interaction overrides 18
// (OnYoshiTryEat _ZN7daSpd_c13OnYoshiTryEatEv, returns 6), 19 (OnTurnIntoEgg
// _ZN7daSpd_c13OnTurnIntoEggER7daPly_c) and 29 (OnAimedAtWithEgg _ZN7daSpd_c16OnAimedAtWithEggEv, returns
// 0x32000). Slots 13/14 (ActorBase::Virtual34/38, 0x0204357c/0x0204349c) trap
// by name the way every sibling fill traps that pair, and slot 30
// (OnAimedAtWithEggReturnVec, arm9 0x020100dc) traps because its matched body
// is SRET -- a hidden return-slot pointer before self -- which no fill's thunk
// shape models. Everything else binds to Actor/ActorBase's own default half,
// all in the map (the montymole binding set).
//
// BEHAVIOR IS MATCHED SRC, but the two STATE DISPATCHERS it and SetState reach
// are host copies: func_ov071_021202b4 (ENTER) and func_ov071_02120278 (MAIN,
// called by Behavior) form mwcc pointer-to-member reads MSVC mis-strides, so
// they live in port/unmatched/Scuttlebug_StateDispatch.cpp with the state seat.
// Init/Behavior/Render are real MSVC members against include/Scuttlebug.h
// (faces below); Cleanup and D0 are plain C; OnPendingDestroy is an empty .c.
//
// D1 (slot 16) is a HOST THUNK, the flame/HauntedChair treatment. The matched
// D1 (_ZN10ScuttlebugD1Ev.cpp) is the auto-emitted-member-dtor form -- an
// empty ~daSpd_c() whose compiler-emitted member and base dtor calls resolve
// to MSVC-mangled names that do not exist in this build -- so it is NOT
// compiled; the thunk runs the D0 chain (ScuttlebugD0's own order:
// WithMeshClsn +0x194, MovingCylinderClsn +0x160, ShadowModel +0x138,
// ModelAnim +0xd4, ~Actor) minus the final Memory::Deallocate, because the
// slot-16 caller (ActorBase::AfterCleanupResources) deallocates itself.
//
// The id was cross-checked from the relocated overlay image: g_profile_SPIDER
// (0x02122c08) +0 word = _Z17daSpd_c_classInitv (0x02120618), +4 halfword = 0x00ff =
// 255, and ACTOR_SPAWN_TABLE[255] (ACTOR_SPAWN_TABLE + 255*4 = 0x02090c60) points at
// that record. _Z17daSpd_c_classInitv's own vtable-store site (Spawn.c: p[0] =
// _ZTV7daSpd_c) names the table.
#include "port_d16.h"

#include <cstdio>

/* hal/actor_slot30_seat.cpp -- the shared seat for vtable slot 30,
   Actor::OnAimedAtWithEggReturnVec. The ROM word in slot 30 of every vtable
   this file fills IS the arm9 base body 0x020100dc (checked against
   config/<module>/relocs.txt at vtable+30*4), and that body is now in the
   link from src/_ZN8dActor_c25OnAimedAtWithEggReturnVecEv.cpp on slice_gate50.
   The three-parameter __fastcall is the sret contract MSVC uses for a
   thiscall member returning a 12-byte struct: this in ecx, the hidden result
   pointer the one (callee-popped) stack argument. Same shape as whomp_s30. */
/* types main's C++ declarations below name (catchup1 linkage flip) */
struct daSpd_c;
extern "C" void *__fastcall port_actor_s30_base(void *self, void *, void *out);
#include <cstdlib>

#include "dActor_c.h"
#include "dtor_faces_cpp.h"
#include "fBase_c.h"
#include "daSpd_c.h"

extern "C" {
/* the shared lifecycle halves, the same functions every fill writes */
int _ZN8dActor_c19BeforeInitResourcesEv(void *self);            /* slot 1  */
void _ZN8dActor_c18AfterInitResourcesEj(void *self, unsigned a); /* slot 2  */
int _ZN8dActor_c14BeforeBehaviorEv(void *self);                 /* slot 7  */
int _ZN8dActor_c12BeforeRenderEv(void *self);                   /* slot 10 */
int _ZN8dActor_c9Virtual50Ev(void *self);                       /* slot 20 */
void _ZN8dActor_c15OnGroundPoundedERS_(void *self, void *o);    /* slot 21 */
void _ZN8dActor_c11OnAttacked1ERS_(void *self, void *o);        /* slot 22 */
void _ZN8dActor_c11OnAttacked2ERS_(void *self, void *o);        /* slot 23 */
void _ZN8dActor_c8OnKickedERS_(void *self, void *o);            /* slot 24 */
void _ZN8dActor_c8OnPushedERS_(void *self, void *o);            /* slot 25 */
void _ZN8dActor_c24OnHitByCannonBlastedCharERS_(void *self, void *o); /* slot 26 */
void _ZN8dActor_c15OnHitByMegaCharER7daPly_c(void *self, void *p);     /* slot 27 */
void _ZN8dActor_c19OnHitFromUnderneathERS_(void *self, void *o);      /* slot 28 */

extern int data_02099f24[];          /* the frame phase the lists are in */
extern unsigned char data_020a4b4c;  /* the spawn spine's own step */
const char *port_actor_class_name(unsigned id);   /* hal/actor_registry */
  void port_actor_slot_decline(const char *what);  /* func_02043fdc_hostcopy.cpp */
void port_actor_render_probe(const char *cls, void *model); /* actor_classes */

/* Scuttlebug's own C-linkage bodies. Behavior is matched src; the two state
   dispatchers it reaches are the host copies in
   port/unmatched/Scuttlebug_StateDispatch.cpp. */
int _ZN7daSpd_c16CleanupResourcesEv(void);    /* slot 3,  .c C linkage */
int _ZN7daSpd_c16OnPendingDestroyEv(void);    /* slot 12, .c C linkage */
int *_ZN7daSpd_cD0Ev(int *self);              /* slot 17, .c C linkage */
int _ZN7daSpd_c13OnYoshiTryEatEv(void);                    /* slot 18, OnYoshiTryEat */
void _ZN7daSpd_c13OnTurnIntoEggER7daPly_c(void *self, void *p);    /* slot 19, OnTurnIntoEgg */
int _ZN7daSpd_c16OnAimedAtWithEggEv(void);                    /* slot 29, OnAimedAtWithEgg */
extern "C++" daSpd_c *daSpd_c_classInit(void);

/* the D1 chain's sub-object destructors, all C-linkage in the build (the same
   node dtors ScuttlebugD0 calls) */
void _ZN10dBgCh_ActrD1Ev(void *);         /* the WithMeshClsn at +0x194 */
void _ZN7dCcAc_cD1Ev(void *);   /* the MovingCylinderClsn at +0x160 */
void _ZN17dExtShadowModel_cD1Ev(void *);          /* the ShadowModel at +0x138 */
void _ZN14dExtAnmModel_cD1Ev(void *);             /* the ModelAnim at +0xd4 */
void *_ZN8dActor_cD2Ev(void *);                /* the Actor base */

}

/* dsd mislabeled the anim SharedFilePtr in func_ov071_0211fbf4: the src spells
   data_ov073_02122f88 but the load site's reloc (0x0211fc5c) targets 0x02122f88
   module:overlay(71) -- ov071's own anim pointer, mounted in ov071_syms.txt.
   The config rename goes upstream as its own by-address PR.

   THE ALIAS THAT USED TO SIT HERE IS GONE (run linkw wave 12, lane w12). It
   was
       /alternatename:_data_ov073_02122f88=_gSpiderWalkAnimationFile
   and it DIED the moment ov073 was mounted: 0x02122f88 is a real ov073 address
   too -- ChiefChilly's tenth {handler, adj} state record -- so port/ov073_syms.
   txt defines the LHS and a defined LHS defeats /alternatename silently. That
   is the wave-5 R1/R2 arrival shape and tools/alternatename_guard.py caught it
   at the first link. The routing moved to the R1/R2 remedy, a per-source
   rename on the one TU that needs it, in port/CMakeLists.txt:

       set_source_files_properties(src/game/actors/d_a_spd.cpp PROPERTIES
           COMPILE_DEFINITIONS "data_ov073_02122f88=gSpiderWalkAnimationFile")

   Scuttlebug's behaviour is unchanged: that TU still reads
   gSpiderWalkAnimationFile, by name now instead of through the alias. Leaving the
   pragma in place would have been the silent failure -- the reference would
   have bound to ChiefChilly's state record and handed Animation::LoadFile a
   code address. */
/* __sinit_ov071_02122a64 names a second view of data_ov071_02122ecc with a
   dsd _d suffix (a struct overlay on the same address); one storage, two
   spellings. */
#pragma comment(linker, "/alternatename:_data_ov071_02122ecc_d=_data_ov071_02122ecc")

/* The C++/C linkage bridge, the gate-172..175 reading:
   - AnimLoadFile is decl_common's friendly C name for the STATIC
     Animation::LoadFile (cdecl both sides, one arg; the matched method rides
     the slice) -- the UpdatePosWithTransform static-method alias precedent.
   - InitResources spells WithMeshClsn::Init with a void* fourth param; the
     compiled bridge (hal/actor_class_faces.cpp) is the Vector3_16* spelling.
     Same __thiscall body, same widths.
   - func_ov071_0211f7d4 spells GetFloorResult with a void* return; the
     compiled body (slice_gate10) is the int-return spelling. eax either way.
   - func_ov071_0211f524 declares the arm9 identity matrix as a C++ Mtx43;
     romdata.c defines the C symbol. */
#pragma comment(linker, "/alternatename:_AnimLoadFile=?LoadFile@dExtFrameCtrl_c@@SAPADAAUSharedFilePtr@@@Z")
/* RETIRED at ALIAS2 (wave 8, the main -> port sync). DEAD RHS and an UNREFERENCED left hand side: nothing in the build defines ?Init@dBgCh_Actr@@QAEXPAUActor@@HHPAUVector3_16@@H@Z, and nothing references ?Init@dBgCh_Actr@@QAEXPAUActor@@HHPAXH@Z, so the row can never fire and nothing wants it to. */
// #pragma comment(linker, "/alternatename:?Init@dBgCh_Actr@@QAEXPAUActor@@HHPAXH@Z=?Init@dBgCh_Actr@@QAEXPAUActor@@HHPAUVector3_16@@H@Z")
#pragma comment(linker, "/alternatename:?data_020a0e68@@3UMtx43@@A=_data_020a0e68")

/* func_ov071_0211f7d4 names its second cylinder collider CylinderClsn2 and
   calls Clear/Update as methods; the bodies are the one CylinderClsn's own
   C-linkage forms, already linked. A local shadow bridges the __thiscall
   spellings -- an alias cannot cross the __thiscall/cdecl seam. */
extern "C" void _ZN5dCc_c5ClearEv(void *c);
extern "C" void _ZN5dCc_c6UpdateEv(void *c);
struct CylinderClsn2 { void Clear(); void Update(); };
void CylinderClsn2::Clear() { _ZN5dCc_c5ClearEv(this); }
void CylinderClsn2::Update() { _ZN5dCc_c6UpdateEv(this); }

// ---- no host table ----------------------------------------------------------
// daSpd_c is a native class: _Z17daSpd_c_classInitv runs the real constructor,
// so a Scuttlebug carries its compiled table ??_7daSpd_c@@6B@ and the registry
// row has no fill. The host table _ZTV7daSpd_c this file used to fill with
// thunks onto the same bodies was never dispatched and is gone.
