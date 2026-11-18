// GATE 50: the castle interior's PAINTING (daPicGate_c, actor 307), ov080.
//
// Peach's portrait gate: six on level 2's object table. NOT PeachPainting (38,
// ov010) -- a different class in a different overlay, with no cross-overlay
// spawn: its InitResources reads its own tuning tables, allocates a Memory
// buffer and calls Actor::SetRanges and Actor::ClosestPlayer, nothing more.
//
// The class is native C++ now (src/game/actors/d_a_pic_gate.cpp and
// d_a_pic_gate_methods.cpp): daPicGate_c_classInit runs the real constructor,
// so a painting carries its compiled table ??_7daPicGate_c@@6B@ and the
// registry row has no fill. The host table this file used to fill
// (_ZTV11daPicGate_c, 31 slots of thunks onto the same bodies, with the
// "[paint] init" line, the SM64DS_PAINT_TEX init/render probe and a refuse
// guard on Behavior) was never dispatched on this line and is gone.
//
// What is left is the two things the compiled class still needs from the
// port: the C++-linkage data aliases and the texture cache the overlay load
// clears.
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* ---- C++-linkage data aliases, the gate-33 actor_faces_bob treatment -------
   Four of the painting's globals are declared with C++ linkage in the .cpp
   TUs (func_ov080_02125630 and _ZN8Painting13InitResourcesEv spell them outside their
   extern "C" blocks), so a reference comes out decorated -- ?...@@3PAUDisp@@A
   and the like -- while the storage the mount and auto_bss emit is the plain C
   name. Data has no calling convention, so an alias onto the one object is
   exact. data_ov080_0212775c is the SharedFilePtr-pointer table from the mount
   (ov080_syms.txt); the other three are BSS (hal/auto_bss.cpp). */
#pragma comment(linker, "/alternatename:?data_ov080_02128628@@3PAUDisp@@A=_data_ov080_02128628")
#pragma comment(linker, "/alternatename:?data_ov080_0212869c@@3PAEA=_data_ov080_0212869c")
#pragma comment(linker, "/alternatename:?data_ov080_02128688@@3PAEA=_data_ov080_02128688")
#pragma comment(linker, "/alternatename:?data_ov080_0212775c@@3PAPAUSharedFilePtr@@A=_data_ov080_0212775c")

/* SM64DS_PAINT_TEX=1: says when the overlay load clears the painting's texture
   cache, off unless set. daPicGate_c::InitResources stores
   func_ov080_02125630's five-word material record at this+0x1a8. That function
   is a CACHE keyed on an ov080 bss byte (data_ov080_0212869c[0x18*i]): on the
   first call it uploads and writes the record, on every later call it hands
   the record back untouched. Word 0 of the record is
   dExtSimpleModel_c::LoadCompressedTextureToVram's return, which is the
   texture VRAM bump cursor data_020a4bc8 at upload time -- an offset into a
   cursor every level boot rewinds. */
static int pt_tex_probe(void)
{
    static int on = -1;
    if (on < 0) on = std::getenv("SM64DS_PAINT_TEX") != 0;
    return on;
}
extern "C" unsigned char data_ov080_02128688[];

/* THE CACHE BLOCK, sized from the ROM's own symbol table rather than guessed.
   config/arm9/overlays/ov080/symbols.txt has data_ov080_02128688 and
   data_ov080_0212869c as kind:bss, 0x14 apart and both indexed by 0x18*i, so
   each element is 0x18 bytes: twenty bytes of record then the loaded flag at
   +0x14. The slot count is the length of the SharedFilePtr array
   func_ov080_02125630 indexes with the same i -- data_ov080_0212775c runs to
   data_ov080_021277a8, 0x4c bytes, 19 pointers. */
enum { PT_TEX_SLOTS = 19, PT_TEX_STRIDE = 0x18 };

/* WHAT THE CARTRIDGE'S OVERLAY LOAD DOES TO THAT BLOCK, which this port was
   not doing. UnloadLevelOverlays then LoadLevelOverlays run on every level
   change, level 2's object overlays are 63/80/85/89/94/100 and Bob-omb
   Battlefield's are 62/69/78/84/91/95/100, so ov080 comes off and goes back
   on around a course -- and FS_LoadOverlay clears the overlay's bss. The
   cartridge therefore starts every castle entry with the flag at 0 and
   re-uploads the picture to the current cursor. The port's LoadOverlay face
   is empty and is never called at all, so the flag stayed 1 for the life of
   the process and the second visit re-bound the first visit's dead offset:
   the frame drew right and the picture was whatever the course had left in
   that VRAM. Called from hal/level_boot.cpp's port_stage_boot_body, which is
   this port's Stage::InitResources, at the ROM's own position for the call. */
extern "C" void port_painting_texcache_overlay_load(int id)
{
    if (id != 80) return;
    std::memset(data_ov080_02128688, 0, PT_TEX_SLOTS * PT_TEX_STRIDE);
    if (pt_tex_probe()) {
        std::fprintf(stderr, "[painttex] overlay load %d: texture cache "
                     "cleared (%d slots)\n", id, (int)PT_TEX_SLOTS);
        std::fflush(stderr);
    }
}
