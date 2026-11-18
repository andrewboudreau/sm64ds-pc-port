/* HOST COPIES of src/runtime/actor/ActorDerived.cpp and
 * src/func_02042ffc.c -- the actor-spawn entry pair with its arguments
 * spelled out.
 *
 * The ROM's ActorDerived::Spawn is a two-instruction veneer:
 *
 *     ldr ip, [pc] ; bx ip ; .word _ZN7fBase_c11createChildEjPS_ji
 *
 * and _ZN7fBase_c11createChildEjPS_ji names only the first two of the four arguments it was
 * handed, leaving param1 and flags riding in r2/r3 across the call into
 * _ZN7fBase_c10fBase_makeEiPvji. Both are correct ARM and both are matched, which is why the
 * matched sources have those shapes. Under the MSVC cdecl ABI a veneer that
 * declares no parameters forwards nothing: _ZN7fBase_c11createChildEjPS_ji would read its two
 * arguments off the veneer's own frame, and _ZN7fBase_c10fBase_makeEiPvji would take an actor
 * id out of stack garbage and call through data_020a4bb8[garbage].
 *
 * Stage::LoadClsnAndObjects ends in exactly that call (the minimap and HUD
 * actors, ids 0x14f / 0x14e), so the level boot is the first host code that
 * reaches it. The copies below carry all four arguments the whole way; the
 * behaviour -- null parent means no spawn, the SceneNode is parent + 0x14 --
 * is the matched source's, unchanged.
 *
 * 2026-09-14, run link100 lane CRASH4: THE MSVC-DECORATED NAME IS DEFINED HERE
 * TOO NOW, at the bottom of this file. Until today only the Itanium spelling
 * lived here, which is what the port's C-spelled callers reach. Commit
 * 5116945a1 folded the deleted src/_ZN7dBase_c5SpawnEjP7fBase_cii.cpp into
 * src/runtime/actor/ActorDerived.cpp, a real C++ TU, so the two-argument decomp body
 * started carrying ?Spawn@dBase_c@@SAPAUfBase_c@@IPAU2@HH@Z and every C++
 * caller bound to that instead of to this file. The gate-14 filter in
 * port/CMakeLists.txt still names the file that no longer exists, so nothing
 * was unresolved and nothing was duplicated: the ride-through fix simply
 * stopped applying, and the level boot spawned every actor with a stack
 * address in fBase_c::mParam. port/unmatched/dBase_c_SpawnHostAbi.h carries
 * the whole measurement and renames the decomp copy out of the way.
 */
#include "dBase_c.h"

extern "C" {

int _ZN7fBase_c10fBase_makeEiPvji(unsigned id, void *node, unsigned param1, int flags);

/* RETIRED at catchup1 round 5: the four-argument host copy of
 * fBase_c::createChild that stood here existed only because the matched body
 * named two of its four arguments. Main's src/runtime/actor/ActorBase.cpp now
 * defines fBase_c::createChild(u32, fBase_c *, u32, int) and forwards all four,
 * so this copy is gone; FACE4 adds the face that gives the flat name below its
 * body. The two Spawn copies here still call it by that flat name. */
int _ZN7fBase_c11createChildEjPS_ji(unsigned id, void *parent, unsigned param1, int flags);

/* PORT_HOST_ABI: ARM register ride-through: the ROM veneer leaves param1
 * and flags in r2/r3 across a call that names two arguments. See the
 * header. */
void *_ZN7dBase_c5SpawnEjP7fBase_cii(unsigned id, void *parent,
                                             unsigned param1, int flags)
{
    return (void *)(size_t)_ZN7fBase_c11createChildEjPS_ji(id, parent, param1, flags);
}

}

/* PORT_HOST_ABI: the same ruling under the name C++ callers actually use.
 * LoadEntranceObjects packs the spawn flags and reaches dActor_c::Spawn, which
 * reaches this; on the cartridge the flags and the trailing 2 ride r2/r3
 * through two functions that name neither. Same body as the Itanium spelling
 * above, so the linker's identical-COMDAT folding may well give both one
 * address. */
fBase_c *dBase_c::Spawn(u32 actorID, fBase_c *parent, int param1, int flags)
{
    return (fBase_c *)(void *)(size_t)_ZN7fBase_c11createChildEjPS_ji(actorID, parent,
                                                    (unsigned)param1, flags);
}

/* catchup1 round 3 (FACE4 rows): main defines these as C++ statics or
   namespace-scope functions, which facegen does not face. Both sides are
   __cdecl with the same arguments, so a name bridge is the whole fix
   (FACE4 round2_done.tsv; each right hand side is in the probe link map). */
#pragma comment(linker, "/alternatename:__ZN7fBase_c10fBase_makeEiPvji=?fBase_make@fBase_c@@SAPAU1@HPAXIH@Z")
