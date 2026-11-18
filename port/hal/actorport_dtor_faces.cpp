// ACTORPORT -- out-of-line destructor faces for the nine INLINE-destructor classes.
//
// Companion to hal/actorport_dtor_bridge.cpp; read that file's header first.
//
// WHY THESE NINE ARE DIFFERENT.  The bridge aliases the old folded name
// ??1X@@UAE@XZ and the new slot name ?Destructor1@X@@UAEXXZ onto the class's own
// complete-object destructor ??1X@@QAE@XZ.  That works for every class whose
// destructor has an out-of-line body somewhere in the link.  These nine spell
// theirs INLINE in the header (`~daStarGate_c() {}`), and an inline NON-VIRTUAL
// destructor is emitted only where a TU odr-uses it.  While the same destructor
// was VIRTUAL it was emitted unconditionally, because the compiler vtable
// referenced it -- that is the whole difference, and it is why the nine only
// surface once the host arm stops saying `virtual`.
//
// So the face below is the odr-use.  Each one is a real out-of-line function
// whose body IS the destructor -- the inline body plus the base and member
// destructor chain the compiler synthesises -- and the bridge points both names
// at it.  __fastcall(void *, void *) is the tree's dead-edx face shape: a
// __thiscall caller passes the receiver in ecx and neither side puts anything on
// the stack, so the alias is ABI-exact (the same reasoning as hal's other faces
// and the arity audit in sm64ds-port-fastcall-face-arity).
//
// daChRoom_c and daObjKumo_c are here for a second reason as well: hal/int4_rows.cpp
// aliases ??1CameraTag@@UAE@XZ and ??1Cloud@@UAE@XZ -- shadow names that
// hal/actor_classes_ov_link100.cpp references -- onto those two classes BY ADDRESS,
// and those right-hand sides were the virtual mangles that no longer exist.

#include "daBakubaku_c.h"
#include "daBgSnmBdy_c.h"
#include "daBgSnwmn_c.h"
#include "daChRoom_c.h"
#include "daLuigi_c.h"
#include "daObjFl_Gura_c.h"
#include "daObjKi_Ita_c.h"
#include "daObjKm1_Kuruma_c.h"
#include "daObjKm1_Kurumajiku_c.h"
#include "daObjKm2_Ukishima_c.h"
#include "daObjKumo_c.h"
#include "daStarGate_c.h"

#include "EnemySpawner.h"
#include "EnemySwitchTag.h"
#include "Koopa.h"
#include "dMeter_c.h"
#include "dView_c.h"
#include "daBDonketu_c.h"
#include "daDonketu_c.h"
#include "daDsn_c.h"
#include "daLinelift2_c.h"
#include "daObjCtMecha08_c.h"
#include "daObjCtMecha10_c.h"
#include "daObjFl_Block_c.h"
#include "daObjFl_Ring_c.h"
#include "daObjFl_UkiKi_c.h"
#include "daObjKi_Fune_c.h"
#include "daObjKi_Hasira_c.h"
#include "daObjKm2_Agaru_c.h"
#include "daObjSwitch_c.h"
#include "daObjTh_Fall_Block_c.h"
#include "daObjYajirusi_c.h"
#include "daUdlift_c.h"
#include "daSpd_c.h"
#include "daObjHatenaBlock_c.h"
#include "daKpa_c.h"
#include "daKpaTail_c.h"

#define ACTORPORT_D1_FACE(Class)                                              \
    extern "C" void __fastcall actorport_d1_##Class(void *s, void *unused)    \
    {                                                                         \
        (void)unused;                                                         \
        ((Class *)s)->~Class();                                               \
    }

ACTORPORT_D1_FACE(daBakubaku_c)
ACTORPORT_D1_FACE(daBgSnmBdy_c)
ACTORPORT_D1_FACE(daBgSnwmn_c)
ACTORPORT_D1_FACE(daChRoom_c)
ACTORPORT_D1_FACE(daLuigi_c)
ACTORPORT_D1_FACE(daObjFl_Gura_c)
ACTORPORT_D1_FACE(daObjKi_Ita_c)
ACTORPORT_D1_FACE(daObjKm1_Kuruma_c)
ACTORPORT_D1_FACE(daObjKm1_Kurumajiku_c)
ACTORPORT_D1_FACE(daObjKm2_Ukishima_c)
ACTORPORT_D1_FACE(daObjKumo_c)
ACTORPORT_D1_FACE(daStarGate_c)
ACTORPORT_D1_FACE(daECreate_c)
ACTORPORT_D1_FACE(daESwitch_c)
ACTORPORT_D1_FACE(daNknk_c)
ACTORPORT_D1_FACE(dMeter_c)
ACTORPORT_D1_FACE(dView_c)
ACTORPORT_D1_FACE(daBDonketu_c)
ACTORPORT_D1_FACE(daDonketu_c)
ACTORPORT_D1_FACE(daDsn_c)
ACTORPORT_D1_FACE(daLinelift2_c)
ACTORPORT_D1_FACE(daObjCtMecha08_c)
ACTORPORT_D1_FACE(daObjCtMecha10_c)
ACTORPORT_D1_FACE(daObjFl_Block_c)
ACTORPORT_D1_FACE(daObjFl_Ring_c)
ACTORPORT_D1_FACE(daObjFl_UkiKi_c)
ACTORPORT_D1_FACE(daObjKi_Fune_c)
ACTORPORT_D1_FACE(daObjKi_Hasira_c)
ACTORPORT_D1_FACE(daObjKm2_Agaru_c)
ACTORPORT_D1_FACE(daObjSwitch_c)
ACTORPORT_D1_FACE(daObjTh_Fall_Block_c)
ACTORPORT_D1_FACE(daObjYajirusi_c)
ACTORPORT_D1_FACE(daUdlift_c)

/* catchup1: two more inline destructors. daSpd_c's is VIRTUAL and inline
   (`virtual ~daSpd_c() {}`, no host arm), so the call is QUALIFIED: an
   unqualified one would go back through the vtable slot the bridge reseats.
   daObjHatenaBlock_c's host arm spells `~daObjHatenaBlock_c() {}` (no slot),
   which the bridge used to reach under its old virtual mangle. */
#define ACTORPORT_D1Q_FACE(Class)                                                 extern "C" void __fastcall actorport_d1_##Class(void *s, void *unused)        {                                                                                 (void)unused;                                                                 ((Class *)s)->Class::~Class();                                            }

ACTORPORT_D1Q_FACE(daSpd_c)
ACTORPORT_D1Q_FACE(daObjHatenaBlock_c)

/* catchup1 round 3: the three deleting destructors FACE4 proved against the
   ROM relocs (lane FACE4) and could not ledger, because the
   ledger binds a row only to a destructor the link already defines. Each ROM
   D0 is its D1 plus exactly one call, Memory::Deallocate(self, data_020a0eac),
   so each face below is the facegen D form written by hand:
     daSpd_c      0x0211f048  D1 is inline in main; the qualified call
                              emits it here, as the D1 face above does
     daKpa_c      0x02111950  D1 ??1daKpa_c@@QAE@XZ, defined by d_a_kpa.cpp's
     daKpaTail_c  0x021119e4  host arm (ACT4a 9f79028c009); same for the tail
   They live here and not in port/faces_sync.txt so the D0 sits next to the
   D1 face it depends on. The callers (actor_classes_scuttlebug.cpp and
   actor_classes_ov060.cpp slot 17) read the result as the object pointer, so
   the face returns self. */
extern "C" void _ZN6Memory10DeallocateEPvPN5mHeap6Heap_tE(void *ptr, void *heap);
/* data_020a0eac (the actor heap) comes from dActor_c.h / dEnemyBase_c.h */
#define ACTORPORT_D0_FACE(Flat, Class) extern "C" void *Flat(void *self) { ((Class *)self)->Class::~Class(); _ZN6Memory10DeallocateEPvPN5mHeap6Heap_tE(self, data_020a0eac); return self; }
ACTORPORT_D0_FACE(_ZN7daSpd_cD0Ev, daSpd_c)
ACTORPORT_D0_FACE(_ZN7daKpa_cD0Ev, daKpa_c)
ACTORPORT_D0_FACE(_ZN11daKpaTail_cD0Ev, daKpaTail_c)
