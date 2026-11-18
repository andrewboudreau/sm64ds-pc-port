// PORT_HOST_ABI. dScMgMemory2_c's own faces and its one named trap.
// Run mg6, lane MEM.
//
// Everything here was named by a LINK of the seat, not predicted. Wave 1
// produced 41 unresolved externals on walk_window, and the same set on
// walk_window_hires and smoke_player. port/tools/facegen.py classified them
// (with an ABSOLUTE --out, which is finding 2 of port/mg_fanout_costs.txt
// section 10 -- a relative one FALSE-FAILS) and this file carries the rows the
// classification and the hand rulings landed on. The rest were slice lines and
// two host copies; port/slice_mem.txt says which.
//
//   19  ALIAS      mechanical, generated, copied in below
//    2  FACE       RETIRED at catchup1 (the main -> port sync, Stage B), see section 2
//   10  REFUSED    ruled by hand below; nine become aliases, one is the trap
//    9  UNKNOWN    six are slice lines, three are the g0/g1/g2 host copy
//
// WAVE 3 ADDED TWO MORE and neither was predictable from wave 1: one more P2
// pair (0x0213ac48) and one more arm9 slice line (src/func_02045ec4.c), each
// the closure of something wave 2 brought in. Waves converge here the way
// port/mg_fanout_costs.txt section 10 says they do, 41 then 2 then 0.
//
// ---- 1. THE NINE HAND RULINGS ---------------------------------------------
//
// facegen refuses a struct-typed global on purpose, because a struct wrapper
// hides a member pointer -- ?sCurlingSceneTable@@3PAUEntry@@A was
// dScMgCurling_c's twenty-five-slot state table and slipped both of its guards
// (finding 1 of section 10). So every one of these was read at its consumer
// before it became an alias, and the test is the rulebook corollary section 4
// states: A PAIR WHOSE CONSUMER SPELLS IT AS TWO INTS IS SAFE AS AN ALIAS; A
// PAIR WHOSE CONSUMER NAMES A MEMBER-POINTER TYPE NEEDS A HOST COPY.
//
//   ?pmf_Dealer_StateDeal@@3UP2@@A   src/Dealer_PlayIdle.cpp declares
//   ?pmf_Dealer_StateIdle@@3UP2@@A   `struct P2 { int w[2]; };` and
//   ?pmf_MascotWalker_StateWalk_3@@3UP2@@A   src/MascotWalker_StatePause.cpp and
//                                   src/Dealer_StartCollect.cpp the same, and
//                                   copies each with `*(P2 *)c = ...`. ALL
//                                   THREE ARE REAL mwcc PAIRS -- {0x020c14bc,
//                                   0}, {0x020c11c0, 0} and {0x020c0364, 0} in
//                                   the image -- and all three are SAFE as
//                                   aliases for exactly the reason the
//                                   corollary gives: two ints is eight bytes on
//                                   both machines, so the copy moves what the
//                                   ROM moves. What consumes the pair
//                                   afterwards is the field dispatcher in
//                                   unmatched/MgMemory2_FieldPmf.cpp, which is
//                                   host-copied, and all three code words are
//                                   routed in its switch.
//                                   0x0213ac48 ARRIVED IN WAVE 3 and is the
//                                   correction to that file's pair-run range,
//                                   which had been written 0x0213ac50 up.
//   ?gMgScene@@3PAUG@@A  src/MgCoins_AddBetCoin.cpp declares
//                                   `struct G { int v; };` and reads `g->v`.
//                                   A pointer to a one-int struct; the global
//                                   is dScMgBase_c's own "current minigame
//                                   scene" pointer, which
//                                   src/_ZN11dScMgBase_cC2Ev.cpp writes.
//   ?func_020beb68@@3PAUV@@A        THE SAME GLOBAL under a name that exists in
//                                   no config, and at C++ linkage on top of it.
//                                   src/Dealer_StateDeal.cpp declares
//                                   `extern V* func_020beb68;` where V is a
//                                   22-virtual dummy class -- a POINTER, not a
//                                   pair. Same defect class as
//                                   src/_ZN14dScMgCurling_c13InitResourcesEv.cpp's bare
//                                   `func_020adc74`, which
//                                   port/mg_fanout_costs.txt section 6 records
//                                   and hal/scene_mg_faces.cpp already aliases,
//                                   and the same one MgCoin_Faces.cpp carries
//                                   as the plain-C spelling.
//   ?data_020a0ebc@@3UVector3@@A    src/MascotWalker_Load.cpp reads .x/.y/.z
//   ?data_020a0edc@@3UVector3_16@@A off both and copies them into the object at
//                                   +0xc8 and +0xe8. Three ints and three
//                                   shorts of plain arm9 data; no pointer of
//                                   any kind.
//   ?sMgCoinsStep@@3P6AXXZA  a FUNCTION-POINTER global, which facegen
//                                   refuses because P6AXXZA is outside its
//                                   proven scalar subset. Ruled by reading its
//                                   writers: ten src TUs assign it, and every
//                                   one assigns a HOST function by name
//                                   (`sMgCoinsStep = (void*)func_ov004_
//                                   020b5368;`) or zero. No DS address ever
//                                   reaches it, so _Z14MgCoins_Updatev's
//                                   `if (p) p();` calls a host address. The
//                                   same global also arrives spelled @@3HA and
//                                   @@3PAXA from two other TUs; all three
//                                   alias onto the one mount definition.
//   ?LoadBGPltt@GXS@@YAXPBXII@Z     src/minigames/d_s_mg_single3d_base.cpp declares
//                                   `namespace GXS { void LoadBGPltt(const
//                                   void*, u32, u32); }` while the matched TU
//                                   src/_ZN3GXS10LoadBGPlttEPKvjj.cpp defines the
//                                   plain C name with the identical three
//                                   arguments. Both __cdecl, so a plain alias
//                                   is right and facegen's refusal is the right
//                                   OUTCOME for the wrong reason -- finding 4
//                                   of section 10, on Memory::Allocate.
//   ?PlayBank2_2D@Sound@@SAXI@Z     the STATIC-MEMBER spelling of
//                                   _ZN5Sound12PlayBank2_2DEj, which four TUs
//                                   in this slice reach. Both __cdecl, one
//                                   unsigned argument. hal/scene_mg_flower.cpp
//                                   already aliases the same target from a
//                                   third spelling (_func_02012754).
//   ?RenderAll@ShadowModel@@YAXXZ   NOT a missing body. src/_ZN11ShadowModel9-
//                                   RenderAllEv.cpp is already in the build
//                                   through port/slice_w5d.txt and defines
//                                   ?RenderAll@ShadowModel@@SAXXZ, because
//                                   include/ShadowModel.h makes it a STATIC
//                                   MEMBER. src/Dealer_Draw.cpp
//                                   declares it as a NAMESPACE function, so the
//                                   two mangles differ by S versus Y and
//                                   nothing else: same __cdecl, same void(void).
//
// ---- 2. RETIRED at catchup1 (the main -> port sync, Stage B) ---------------------
//
// This section held two method faces, dExtBlendAnmModel_c::SetAnim and
// SysTracker::~SysTracker.  Main's src/minigames/d_s_mg_memory2.cpp no longer
// declares either shadow class, and main's src/engine/model/d_ext_model.cpp
// defines dExtBlendAnmModel_c::SetAnim with the very signature the face spelled,
// so keeping the face would be a second definition.  Both are deleted.
//
// ---- 3. THE TRAP THAT USED TO BE HERE, AND WHAT REPLACED IT ----------------
//
// _ZN14dScMgMemory2_c9DrawCardsEv is the sixth call vtable slot 9 (Render) makes:
// src/minigames/d_s_mg_memory2.cpp line 16 calls it on `this`.
// config/arm9/overlays/ov006/symbols.txt names it and sizes it 0xa8, there is
// NO delink block covering it -- the block before it ends exactly at
// 0x020f5b98 and the next starts after -- and, until run mg7 lane MEMCARDS,
// no src file in either extension.
// src/func_ov002_020f5b98.c exists and is a DIFFERENT function: ov002 and ov006
// share an address window, which is the module-residency trap
// port/slice_ccn.txt names, and that file's body takes a `struct Ent *` and
// belongs to another overlay entirely. IT IS STILL NOT A SUBSTITUTE and must
// never be the file that satisfies this symbol.
//
// The trap that stood here counted itself and returned 0, which is the honest
// thing for a body nobody has read. Reading it out of the overlay image ended
// that: it is the ONLY code in dScMgMemory2_c that draws a card, and with it
// stubbed the whole minigame deals sixteen cards into an empty green table.
// src/minigames/d_s_mg_memory2.cpp is the decompilation and port/slice_mem.txt carries
// it, so this file no longer defines the symbol at all.
//
// hal/scene_mg_memory2.cpp reports the card records themselves now, which is
// the measurement the trap count was standing in for: sixteen live records
// spread over x 80..176 and y 24..168 with three sprites in sub OAM was the
// whole bug, stated in numbers.

// ---- the nineteen generated aliases ----------------------------------------
//
// GENERATED by port/tools/facegen.py -- the mechanical rows only, every one
// through the exact-name join and the arity gate.

#pragma comment(linker, "/alternatename:?gMainScreenLayers@@3EA=_gMainScreenLayers")
#pragma comment(linker, "/alternatename:?sMgCoinsBetCap@@3HA=_sMgCoinsBetCap")
#pragma comment(linker, "/alternatename:?gMgCoinsStatus@@3FA=_gMgCoinsStatus")
#pragma comment(linker, "/alternatename:?sMgCoinsBetPlaced@@3HA=_sMgCoinsBetPlaced")
#pragma comment(linker, "/alternatename:?sMgCoinsSoundPending@@3HA=_sMgCoinsSoundPending")
#pragma comment(linker, "/alternatename:?sMgCoinsLastLostAlt@@3HA=_sMgCoinsLastLostAlt")
#pragma comment(linker, "/alternatename:?sMgCoinsPayoutLeft@@3HA=_sMgCoinsPayoutLeft")
#pragma comment(linker, "/alternatename:?sMgCoinsToStep@@3HA=_sMgCoinsToStep")
#pragma comment(linker, "/alternatename:?sMgCoinsIndex@@3HA=_sMgCoinsIndex")
#pragma comment(linker, "/alternatename:?sMgCoinsWait@@3HA=_sMgCoinsWait")
#pragma comment(linker, "/alternatename:?sMgCoinsLaunchPending@@3HA=_sMgCoinsLaunchPending")
#pragma comment(linker, "/alternatename:?gMgCoinsInPlay@@3HA=_gMgCoinsInPlay")
#pragma comment(linker, "/alternatename:?sMgCoinsStep@@3HA=_sMgCoinsStep")
#pragma comment(linker, "/alternatename:?sMgCoinsStep@@3PAXA=_sMgCoinsStep")
#pragma comment(linker, "/alternatename:?sMgCoins@@3DA=_sMgCoins")
#pragma comment(linker, "/alternatename:?sMgCoins@@3PADA=_sMgCoins")
#pragma comment(linker, "/alternatename:?sMgCoinsVisible@@3PAEA=_sMgCoinsVisible")
/* RETIRED at ALIAS2 (wave 8, the main -> port sync). DEAD RHS and an UNREFERENCED left hand side: nothing in the build defines _data_ov006_0213d4d4, and nothing references ?_ZTV14dScMgMemory2_c@@3PAXA, so the row can never fire and nothing wants it to. */
// #pragma comment(linker, "/alternatename:?_ZTV14dScMgMemory2_c@@3PAXA=_data_ov006_0213d4d4")
/* RE-POINTED at ALIAS2 (wave 8, the main -> port sync), lane ALIAS's
   derivation: the right hand side this row carried is defined nowhere in
   the link any more. data
   was: #pragma comment(linker, "/alternatename:?_ZTV19dScMgSingle3DBase_c@@3PAXA=_data_ov006_0213e448") */
#pragma comment(linker, "/alternatename:?_ZTV19dScMgSingle3DBase_c@@3PAXA=__ZTV19dScMgSingle3DBase_c")

// ---- the nine hand rulings, section 1 --------------------------------------

/* catchup1 Stage C: 0x0213ac48 is pmf_Dealer_StateDeal at TARGET (was StateCollect). */
#pragma comment(linker, "/alternatename:?pmf_Dealer_StateDeal@@3UP2@@A=_pmf_Dealer_StateDeal")
#pragma comment(linker, "/alternatename:?pmf_Dealer_StateIdle@@3UP2@@A=_pmf_Dealer_StateIdle")
#pragma comment(linker, "/alternatename:?pmf_MascotWalker_StateWalk_3@@3UP2@@A=_pmf_MascotWalker_StateWalk_3")
#pragma comment(linker, "/alternatename:?gMgScene@@3PAUG@@A=_gMgScene")
#pragma comment(linker, "/alternatename:?func_020beb68@@3PAUV@@A=_gMgScene")
#pragma comment(linker, "/alternatename:?data_020a0ebc@@3UVector3@@A=_data_020a0ebc")
#pragma comment(linker, "/alternatename:?data_020a0edc@@3UVector3_16@@A=_data_020a0edc")
#pragma comment(linker, "/alternatename:?sMgCoinsStep@@3P6AXXZA=_sMgCoinsStep")
/* RETIRED at ALIAS2 (wave 8, the main -> port sync). DEFEATED: the left hand side is a real definition in this link now (GXS_LoadBGPltt.cpp.obj), so the directive is inert and alternatename_guard fails on it. */
// #pragma comment(linker, "/alternatename:?LoadBGPltt@GXS@@YAXPBXII@Z=_GXS_LoadBGPltt")
/* RE-POINTED at ALIAS2 (wave 8, the main -> port sync), lane ALIAS's
   derivation: the right hand side this row carried is defined nowhere in
   the link any more. same member, same convention, same 1 argument slots, types spelled differently
   was: #pragma comment(linker, "/alternatename:?PlayBank2_2D@Sound@@SAXI@Z=__ZN5Sound12PlayBank2_2DEj") */
#pragma comment(linker, "/alternatename:?PlayBank2_2D@Sound@@SAXI@Z=?PlayBank2_2D@Sound@@YAII@Z")
#pragma comment(linker, "/alternatename:?RenderAll@dExtShadowModel_c@@YAXXZ=?RenderAll@dExtShadowModel_c@@SAXXZ")

/* THE NAME-SPELLING ALIAS ONTO A DATA WORD. src/minigames/d_s_mg_memory2.cpp (slot 18)
   and src/minigames/d_s_mg_memory2.cpp (slot 0) both store through a symbol they
   spell `func_020bc7d4`, a name that exists in no config; the reloc's module is
   ov004 and gMgCoinsShowDelta is the co-resident answer, a plain int the ov004
   mount defines. Twelve ov004 src TUs already spell it the right way, which is
   what makes this a spelling defect rather than a convention. */
#pragma comment(linker, "/alternatename:_func_020bc7d4=_gMgCoinsShowDelta")

// ---- section 3: the card draw is a real body now ---------------------------
//
// Nothing is defined here for 0x020f5b98 any more. src/minigames/d_s_mg_memory2.cpp
// defines it and port/slice_mem.txt compiles it, so a tree that loses that
// slice line fails to LINK rather than quietly drawing nothing.
