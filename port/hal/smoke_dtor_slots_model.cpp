// port/hal/smoke_dtor_slots_model.cpp -- ModelBase's and Model's two
// vtable-slot virtuals, for the smoke targets that host those two classes and
// nothing else in the model family.
//
// Lane SMOKELINK3, run link100 wave 10 round 2.
//
// WHAT THESE NAMES ARE.  port/hal/int4_rows.cpp's header says it in full and
// include/ModelBase.h:110 is the ruling: mwccarm puts a class's D1 and D0 in
// vtable slots 0 and 1 and MSVC folds them into one entry, so every slot below
// the destructor would sit a word early.  The decomp headers therefore spell
// TWO ORDINARY VIRTUALS in those two places under the host-invented names
// Destructor1 and Destructor0, guarded by _MSC_VER so the ARM side never sees
// them and no ROM byte moves.  Neither name is ever called; they hold the two
// slots the ROM's table holds.  Holding a slot still needs a definition,
// because a constructor emits its class's MSVC vftable and a vftable
// references every slot -- which is why src/_ZN5ModelC1Ev.cpp asks the linker
// for ?Destructor1@dExtSimpleModel_c@@UAEXXZ.
//
// WHY THIS FILE RATHER THAN hal/int4_rows.cpp ITSELF.  That file is where
// these four bodies live for the window targets, and taking it whole was tried
// first: it covers SIX classes (ModelBase, Model, CommonModel, ModelAnim2,
// BlendModelAnim, ShadowModel) and also carries five /alternatename rows and a
// flat ExpandingHeapAllocator face, so on smoke_model it traded four
// unresolved names for eighteen -- the other four classes' flat destructors
// plus dEnemyBase_c::KillByInvincibleChar, Particle::SysTracker::Contents::
// Create, dScene_c::ResetHardwareRegisters, dScMgCurling2_c::SpawnValue,
// dActor_c::OnAimedAtWithEggReturnVec and dBgCh_Lin::GetClsnPos, none of which
// a model harness hosts.  This is the same subset argument
// port/faces_sync_smoke_heap.txt makes for the ledger and
// port/hal/smoke_aliases_heap.cpp makes for the alias rows: take the rows the
// target needs, verbatim, and leave the rest of the window build's scaffolding
// where it is.
//
// THE FOUR BODIES BELOW ARE COPIED CHARACTER FOR CHARACTER from
// hal/int4_rows.cpp lines 58-59, 71-74, together with the two extern "C"
// declarations they call.  Nothing is re-derived: each forwards to the ROM's
// own function for that slot, with the receiver the host ABI put in ecx.  The
// flat names come from port/unmatched/ModelFamily_Dtors_HostCopy.c, which is
// where walk_window gets them too (walk_window.map lines 15562-15570).  No
// target carries both this file and hal/int4_rows.cpp, so the two definitions
// cannot collide.
//
//   slot 0, Destructor1              slot 1, Destructor0
//   _ZN11dExtModel_cD1Ev      0x02017120   _ZN11dExtModel_cD0Ev      0x020170e8
//   _ZN17dExtSimpleModel_cD1Ev          0x02016d20   _ZN17dExtSimpleModel_cD0Ev          0x02016ce0

#include "dExtModel_c.h"
#include "dExtSimpleModel_c.h"

extern "C" {
void _ZN11dExtModel_cD1Ev(void *self);
void _ZN11dExtModel_cD0Ev(void *self);
void _ZN17dExtSimpleModel_cD1Ev(void *self);
void _ZN17dExtSimpleModel_cD0Ev(void *self);
}

void dExtModel_c::Destructor1()      { _ZN11dExtModel_cD1Ev(this); }
void dExtModel_c::Destructor0()      { _ZN11dExtModel_cD0Ev(this); }
void dExtSimpleModel_c::Destructor1()          { _ZN17dExtSimpleModel_cD1Ev(this); }
void dExtSimpleModel_c::Destructor0()          { _ZN17dExtSimpleModel_cD0Ev(this); }

/* catchup1 Stage C round 4 (SLICE4 list): main's merged d_ext_model.cpp builds
   the vtables of dExtCommonModel_c, dExtAnm2Model_c, dExtShadowModel_c,
   dExtBlendAnmModel_c and dExtAnmTexPat_c, so their slots are wanted here too.
   walk_window seats them from hal/int4_rows.cpp and hal/model_dtor_seat.cpp
   over host copies these harnesses do not carry; here they run the TU's own
   destructors instead: slot 0 (D1) destroys in place, slot 1 (D0) destroys
   and frees, the ROM's two halves. */
#include "dExtCommonModel_c.h"
#include "dExtAnm2Model_c.h"
#include "dExtShadowModel_c.h"
#include "dExtAnmTexPat_c.h"
#include <new>
void dExtCommonModel_c::Destructor1() { this->dExtCommonModel_c::~dExtCommonModel_c(); }
void dExtCommonModel_c::Destructor0() { this->dExtCommonModel_c::~dExtCommonModel_c(); ::operator delete(this); }
void dExtAnm2Model_c::Destructor1()   { this->dExtAnm2Model_c::~dExtAnm2Model_c(); }
void dExtAnm2Model_c::Destructor0()   { this->dExtAnm2Model_c::~dExtAnm2Model_c(); ::operator delete(this); }
void dExtShadowModel_c::Destructor1() { this->dExtShadowModel_c::~dExtShadowModel_c(); }
void dExtShadowModel_c::Destructor0() { this->dExtShadowModel_c::~dExtShadowModel_c(); ::operator delete(this); }
void __cdecl dExtAnmTexPat_c::RomSlot1D0() { this->dExtAnmTexPat_c::~dExtAnmTexPat_c(); ::operator delete(this); }
#include "dExtBlendAnmModel_c.h"
void dExtBlendAnmModel_c::Destructor1() { this->dExtBlendAnmModel_c::~dExtBlendAnmModel_c(); }
void dExtBlendAnmModel_c::Destructor0() { this->dExtBlendAnmModel_c::~dExtBlendAnmModel_c(); ::operator delete(this); }
