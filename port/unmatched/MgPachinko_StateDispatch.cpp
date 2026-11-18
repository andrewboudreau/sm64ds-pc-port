// PORT_HOST_ABI. The mwcc POINTER-TO-MEMBER WALL, per-class half:
// dScMgPachinko_c's five dispatching TUs and its twenty-five state addresses.
// Actor id 0x170, scene 368. Run mg5, lane PCH.
//
// Read unmatched/MgBase_StateDispatch.cpp's header first, then
// unmatched/MgCurling_StateDispatch.cpp. The first carries the ROM disassembly
// of the dispatch sequence and the framework's own four states; the second is
// the per-class template this file follows. Neither is repeated here.
//
// ---- THE TWENTY-FIVE ADDRESSES, AND WHERE THEY COME FROM ------------------
//
// Not guessed and not read out of a running table. src/__sinit_ov006_02131a38.c
// names, in source order, which .data pair fills which .bss slot, and
// port/tools/stategen.py --sinit src/__sinit_ov006_02131a38.c --overlay 6
// --arity 1 reprints the join. That constructor is attributed to THIS class by
// address containment: its copied pairs run 0x0213d878..0x0213d940 and that
// range brackets g_profile_MG_PACHINKO at 0x0213d910.
//
//   table                n  arity  state addresses (in slot order)
//   -------------------  -  -----  ----------------------------------------
//   sPachinkoBgStateTable  4    1    020fac48 020fab70 020faac8 020fa9c8
//   sPachinkoPipeStateTable  4    1    020fc718 020fc500 020fc2ec 020fc1f8
//   sPachinkoBobOmbStateTable  5    1    020fe2e4 020fe394 020fe750 020fe90c
//                                  020fea54
//   sPachinkoFlyerStateTable  5    1    020fb4e0 020fb45c 020fb230 020fb1c4
//                                  020fb0fc
//   sPachinkoBallStateTable  7    1    020fd9cc 020fd894 020fd2d8 020fd17c
//                                  020fd088 020fcec4 020fce04
//
// 4+4+5+5+7 = 25. EVERY TABLE IS ARITY 1 and there is no arity-0 table in this
// class, which is why this file has one switch where curling has two. Every
// adjustment word in the twenty-five pairs is zero, measured the same way
// MgBase_StateDispatch measured curling's.
//
// STATEGEN REFUSED FOUR OF THE TWENTY-FIVE AND THE REFUSALS ARE CARRIED, NOT
// GUESSED. Three are genuine decomp holes and one is a join miss, and the tool
// distinguishes them:
//
//   0x020fe394  slot 1 of sPachinkoBobOmbStateTable   NO delink block, NO src file
//   0x020fb230  slot 2 of sPachinkoFlyerStateTable   NO delink block, NO src file
//   0x020fd2d8  slot 2 of sPachinkoBallStateTable   NO delink block, NO src file
//   0x020fc718  slot 0 of sPachinkoPipeStateTable   no delink block, src EXISTS
//
// ALL FOUR REFUSALS ARE NOW SEATED and this paragraph is kept as the
// derivation rather than rewritten. 0x020fc718 went in with lane PCH as the
// ordinary slice line it always was, run mg5 lane INTEG seated 0x020fe394, and
// run mg5 lane PCOLL seated 0x020fb230 and 0x020fd2d8 from origin/main, where
// both are byte-matched. The switch below therefore has twenty-five real
// dispatches and no reporting case. The three-floors argument that follows is
// what those seats were checked against and is left standing for that reason.
//
// THREE HARD FLOORS WHERE CURLING HAD ONE. Each of the three is the
// Aim_Swing shape as it stood BEFORE run link60 lane CT1 hosted it:
// the case below reports which state was wanted, by address, and calls
// nothing. No symbol is invented for any of them -- inventing a definition for
// a body with no source is the guess port/tools/inferred_stub_guard exists to
// refuse -- and no port_ body is transcribed here either, because transcribing
// three ROM bodies is a second lane's work and not this seat's.
//
// 0x020fc718 IS NOT A FLOOR and is treated as the ordinary slice line it is.
// src/_ZN15dScMgPachinko_c14Pipe_StatePickEi.c exists; only the delinks join misses it. It is in
// port/slice_pch.txt and this switch calls it by its real symbol.
//
// ---- WHY ALL FIVE DISPATCHERS ARE HOSTED, AND THE SPLIT INSIDE THE FIVE ----
//
// port/mg_fanout_costs.txt section 4's rulebook corollary keys on the
// SPELLING, and a `::*` sweep of this class's whole 623-TU closure returns
// exactly two ov006 TUs. Both are dispatchers of this class:
//
//   _ZN15dScMgPachinko_c9Bg_UpdateEv  `typedef void (C::*PMF)(int); struct Entry{PMF pmf;};`
//        declared INSIDE extern "C". The global therefore mangles to the plain
//        C name the mount already defines and THE LINK IS SATISFIED -- section
//        4's silent extern-"C" class, the one a link can never surface.
//        sizeof(Entry) is 4 on MSVC where the ROM strides 8, so index 1 would
//        read half of pair 0 and half of pair 1.
//   _ZN15dScMgPachinko_c12Flyer_UpdateEv  `extern PMF sPachinkoFlyerStateTable[];` at file scope
//        OUTSIDE extern "C", so this one does emit a mangled member-pointer
//        symbol and a link would name it.
//
// THE OTHER THREE ARE ON THE CORROLARY'S SAFE SIDE AND ARE HOSTED ANYWAY, for
// a reason that is about the ADDRESS rather than the layout.
// _ZN15dScMgPachinko_c14Pipe_UpdateAllEv, _020fe248 and _020fda7c spell the pair as
// `struct Ent { int a; int b; }` and perform the Itanium adjustment arithmetic
// in C by hand -- eight bytes on both machines, every field after it in the
// right place, exactly the shape the corollary calls safe as an alias. What
// they then do is `((void(*)(void*,int))fn)(obj, i)` where fn is the pair's
// code word, and in this port that word is a RAW DS ADDRESS that no host
// symbol names. Being safe as an alias is not being routable, and the three
// would each jump into DS address space on their first dispatch. Routing them
// costs the same host copy as the wall does, so they take it.
//
// ---- THE CHAIN, AND WHY IT RUNS THIS WAY ---------------------------------
//
// unmatched/MgBase_StateDispatch.cpp's port_mg_call1 chains
// mg_try_ov004 -> port_mg_try_ov006_1 -> report, and port_mg_try_ov006_1 is
// dScMgCurling_c's. A second class cannot define that symbol without a
// duplicate, and this lane does not edit either of those two files.
//
// So the host copies below call pch_call1, which tries THIS class's switch
// first and falls through to port_mg_call1 for anything it does not know. That
// ordering is correct rather than merely convenient: the framework's two state
// tables hold four ov004 addresses and none of this class's twenty-five, so
// the two sets are disjoint by construction and neither switch can shadow the
// other. The fall-through also keeps ONE reporting site for a genuinely
// unknown address, which is MgBase_StateDispatch's mg_unhandled.

#include <cstdio>
#include <cstdlib>   /* std::abort, for the boot installer below (lane PMFB4) */

/* The eight-byte mwcc member pointer, in the only spelling that is true on
   both machines: two words, no member-pointer type anywhere. */
struct MgPmf { unsigned code; int adj; };

extern "C" {

/* the framework's entry point; see unmatched/MgBase_StateDispatch.cpp */
void port_mg_call1(void *self, unsigned code, int adj, int a);

/* ---- ALL TWENTY-FIVE state bodies, in address order ---------------------
   Twenty-two when lane PCH wrote this, then 0x020fe394 (lane INTEG) and
   0x020fb230 and 0x020fd2d8 (lane PCOLL), which is every address in the five
   tables above and leaves the switch below with no reporting case.
   All twenty-five take (self, index) in their own src, so none of them is the
   ARM ride-through curling met at Scene_Idle and _020e285c. They are
   declared with void* here rather than with each src's own struct type: these
   are C-linkage symbols, the declaration cannot change the mangling, and a
   host copy that repeated five different private struct definitions would be
   five more places for a layout to drift. */
void _ZN15dScMgPachinko_c15Bg_StateSinkOutEi(void *c, int i);
void _ZN15dScMgPachinko_c14Bg_StateScrollEi(void *c, int i);
void _ZN15dScMgPachinko_c14Bg_StateRiseInEi(void *c, int i);
void _ZN15dScMgPachinko_c15Bg_StateWaitingEi(void *c, int i);
void _ZN15dScMgPachinko_c16Flyer_StateLeaveEi(void *c, int i);
void _ZN15dScMgPachinko_c16Flyer_StatePauseEi(void *c, int i);
/* Run mg5, lane PCOLL: state 2 of sPachinkoFlyerStateTable, seated from
   origin/main where it is byte-matched. Was a reported floor AND a trap. Its
   ROM prologue at 0x020fb230 reads `mov r7, r0` and `mul r5, r1, #0x1c`, so it
   takes (self, index) over the 0x1c-stride array and the arity is the table's,
   not an assumption carried over from the trap's declaration. */
void _ZN15dScMgPachinko_c16Flyer_StateCrossEi(void *c, int i);
void _ZN15dScMgPachinko_c18Flyer_StateDescendEi(void *c, int i);
void _ZN15dScMgPachinko_c18Flyer_StateWaitingEi(void *c, int i);
void _ZN15dScMgPachinko_c14Pipe_StateIdleEi(void *c, int i);
void _ZN15dScMgPachinko_c14Pipe_StateSinkEi(void *c, int i);
void _ZN15dScMgPachinko_c14Pipe_StateRiseEi(void *c, int i);
void _ZN15dScMgPachinko_c14Pipe_StatePickEi(void *c, int i);
void _ZN15dScMgPachinko_c13Ball_StatePopEi(void *c, int i);
void _ZN15dScMgPachinko_c18Ball_StateSpinAwayEi(void *c, int i);
void _ZN15dScMgPachinko_c15Ball_StateGlideEi(void *c, int i);
void _ZN15dScMgPachinko_c14Ball_StateDiveEi(void *c, int i);
/* Run mg5, lane PCOLL: state 2 of sPachinkoBallStateTable, the IN-FLIGHT state,
   seated from origin/main where it is byte-matched. Its ROM prologue at
   0x020fd2d8 reads `mov r7, r0`, `mov r6, r1` and `mul r8, r6, #0x38`, so it
   takes (self, index) over this table's own 0x38-stride array -- the same
   stride _ZN15dScMgPachinko_c14Ball_UpdateAllEv walks below, which is the cross-check that the
   index this switch passes is the index the body expects. */
void _ZN15dScMgPachinko_c15Ball_StateDriftEi(void *c, int i);
void _ZN15dScMgPachinko_c14Ball_StateFallEi(void *c, int i);
void _ZN15dScMgPachinko_c17Ball_StateWaitingEi(void *c, int i);
void _ZN15dScMgPachinko_c20BobOmb_StateWaitGrabEi(void *c, int i);
/* Run mg5, lane INTEG: state 1 of sPachinkoBobOmbStateTable, recovered on branch
   decomp/pach-bodies; was a floor, now sliced and dispatched below. */
void _ZN15dScMgPachinko_c15BobOmb_StateAimEi(void *c, int i);
void _ZN15dScMgPachinko_c18BobOmb_StateFlyingEi(void *c, int i);
void _ZN15dScMgPachinko_c20BobOmb_StateFallAwayEi(void *c, int i);
void _ZN15dScMgPachinko_c18BobOmb_StateRemoveEi(void *c, int i);

/* the five mount tables, re-typed to the ROM's eight-byte pair. The mount
   defines the storage; __sinit_ov006_02131a38 fills it at scene load. */
extern MgPmf sPachinkoBgStateTable[];
extern MgPmf sPachinkoPipeStateTable[];
extern MgPmf sPachinkoBobOmbStateTable[];
extern MgPmf sPachinkoFlyerStateTable[];
extern MgPmf sPachinkoBallStateTable[];

/* the ordinary callees the host copies below keep, from their own src */
void PachinkoDeco_UpdateAll(void *c);
void PachinkoFlyer_CheckHits(void *c, int a);

/* the boot installer at the end of this file; hal/scene_mg.cpp calls it after
   the ov006 constructors have filled the tables. */
void port_mg_pachinko_states_seat(void);

}  /* extern "C" */

// ---- the class's address switch --------------------------------------------

static unsigned g_pachinko_state_hits;

/* THE FLOOR COUNTER IS KEPT AT A MEASURED ZERO AND ITS PRINTER IS GONE.
   Run mg5 lane PCOLL seated the last two floor addresses, so this file has no
   caller left for the one-line-per-distinct-address reporter that used to sit
   here and it is deleted rather than left unreferenced. The counter itself
   stays because hal/scene_mg.cpp reads it through port_mg_pachinko_state_counts
   and prints it every run: a reported 0 is evidence that the switch is complete,
   where a removed line would just be a line nobody looks for any more.

   NOTHING IS UNREPORTED AS A RESULT. An address this switch has never seen --
   what a mount rebase or a stategen drift would actually produce, and the only
   remaining way to get one -- takes the default arm, returns 0, and reaches
   port_mg_call1's own mg_unhandled in unmatched/MgBase_StateDispatch.cpp, which
   is the single reporting site this file's header says the fall-through exists
   to preserve. The class-level reporter was only ever the more specific of the
   two, and it was specific to three addresses that are now bodies. */
static const unsigned g_pachinko_state_missing = 0;

extern "C" int port_mg_try_pachinko_1(void *self, unsigned code, int a)
{
    ++g_pachinko_state_hits;
    switch (code) {
    /* sPachinkoBgStateTable */
    case 0x020fac48u: _ZN15dScMgPachinko_c15Bg_StateWaitingEi(self, a); return 1;
    case 0x020fab70u: _ZN15dScMgPachinko_c14Bg_StateRiseInEi(self, a); return 1;
    case 0x020faac8u: _ZN15dScMgPachinko_c14Bg_StateScrollEi(self, a); return 1;
    case 0x020fa9c8u: _ZN15dScMgPachinko_c15Bg_StateSinkOutEi(self, a); return 1;
    /* sPachinkoPipeStateTable -- slot 0 is the delinks JOIN MISS, not a floor */
    case 0x020fc718u: _ZN15dScMgPachinko_c14Pipe_StatePickEi(self, a); return 1;
    case 0x020fc500u: _ZN15dScMgPachinko_c14Pipe_StateRiseEi(self, a); return 1;
    case 0x020fc2ecu: _ZN15dScMgPachinko_c14Pipe_StateSinkEi(self, a); return 1;
    case 0x020fc1f8u: _ZN15dScMgPachinko_c14Pipe_StateIdleEi(self, a); return 1;
    /* sPachinkoBobOmbStateTable */
    case 0x020fe2e4u: _ZN15dScMgPachinko_c20BobOmb_StateWaitGrabEi(self, a); return 1;
    /* slot 1: run mg5 lane INTEG seated this, so it is a real dispatch now */
    case 0x020fe394u: _ZN15dScMgPachinko_c15BobOmb_StateAimEi(self, a); return 1;
    case 0x020fe750u: _ZN15dScMgPachinko_c18BobOmb_StateFlyingEi(self, a); return 1;
    case 0x020fe90cu: _ZN15dScMgPachinko_c20BobOmb_StateFallAwayEi(self, a); return 1;
    case 0x020fea54u: _ZN15dScMgPachinko_c18BobOmb_StateRemoveEi(self, a); return 1;
    /* sPachinkoFlyerStateTable */
    case 0x020fb4e0u: _ZN15dScMgPachinko_c18Flyer_StateWaitingEi(self, a); return 1;
    case 0x020fb45cu: _ZN15dScMgPachinko_c18Flyer_StateDescendEi(self, a); return 1;
    /* slot 2: run mg5 lane PCOLL seated this, so it is a real dispatch now */
    case 0x020fb230u: _ZN15dScMgPachinko_c16Flyer_StateCrossEi(self, a); return 1;
    case 0x020fb1c4u: _ZN15dScMgPachinko_c16Flyer_StatePauseEi(self, a); return 1;
    case 0x020fb0fcu: _ZN15dScMgPachinko_c16Flyer_StateLeaveEi(self, a); return 1;
    /* sPachinkoBallStateTable */
    case 0x020fd9ccu: _ZN15dScMgPachinko_c17Ball_StateWaitingEi(self, a); return 1;
    case 0x020fd894u: _ZN15dScMgPachinko_c14Ball_StateFallEi(self, a); return 1;
    /* slot 2: run mg5 lane PCOLL seated this, so it is a real dispatch now.
       THIS ONE IS THE CHAIN'S FRAME DRIVER -- it is the state a bob-omb sits in
       while it is in flight, and it is what calls the proximity scan and tail-
       calls the hit reaction. With it reported instead of called, seating the
       other three would have changed nothing observable. */
    case 0x020fd2d8u: _ZN15dScMgPachinko_c15Ball_StateDriftEi(self, a); return 1;
    case 0x020fd17cu: _ZN15dScMgPachinko_c14Ball_StateDiveEi(self, a); return 1;
    case 0x020fd088u: _ZN15dScMgPachinko_c15Ball_StateGlideEi(self, a); return 1;
    case 0x020fcec4u: _ZN15dScMgPachinko_c18Ball_StateSpinAwayEi(self, a); return 1;
    case 0x020fce04u: _ZN15dScMgPachinko_c13Ball_StatePopEi(self, a); return 1;

    /* NO FLOOR REMAINS. All twenty-five of this class's state addresses are
       now real dispatches: run mg5 lane INTEG seated 0x020fe394 and lane PCOLL
       seated the last two, 0x020fb230 and 0x020fd2d8. The two `case` arms that
       used to report an address and call nothing are gone with them; see the
       note above g_pachinko_state_missing for where an unknown address is
       reported now. */

    default: --g_pachinko_state_hits;          return 0;
    }
}

/* THIS CLASS FIRST, THEN THE FRAMEWORK. See the header for why the order is
   safe in both directions. */
static void pch_call1(void *self, unsigned code, int adj, int a)
{
    if (code == 0)
        return;                       /* the ROM's own null-pmf guard */
    if (adj == 0 && port_mg_try_pachinko_1(self, code, a))
        return;
    port_mg_call1(self, code, adj, a);
}

/* Lane LINKMG: the C-name face on pch_call1 for the three dispatchers that now
   compile FROM src (_ZN15dScMgPachinko_c14Pipe_UpdateAllEv, _020fe248, _020fda7c). Their src TUs
   open-code the {code,adj} decode in plain ints and compile under MSVC as
   they stand; the one thing MSVC cannot do is call the DS code address the
   decode produces, so tools/hostgen.py's MG_PMF_CALL table swaps exactly that
   one call line for a call to this face. Same order as pch_call1: this class's
   switch first, then the framework's. */
extern "C" void port_mg_pachinko_call1(void *self, unsigned code, int adj, int a)
{
    pch_call1(self, code, adj, a);
}

extern "C" void port_mg_pachinko_state_counts(unsigned *hits, unsigned *missing)
{
    if (hits)    *hits    = g_pachinko_state_hits;
    if (missing) *missing = g_pachinko_state_missing;
}

// ---- the two host copies ----------------------------------------------------
//
// (Five until lane LINKMG. _ZN15dScMgPachinko_c14Pipe_UpdateAllEv, _020fe248 and _020fda7c
// open-code the pair decode in plain ints and compile FROM src now, with the
// one call line routed to port_mg_pachinko_call1 by tools/hostgen.py's
// MG_PMF_CALL table; see the CMake block after SCENE_MG_SOURCES.)
//
// Each is its src TU verbatim with the member-pointer or raw-address call site
// replaced by pch_call1, and with each src's private struct spelled as the
// byte offsets it resolves to. The offsets below are the src's own, not
// re-derived: these five TUs are matched, so their field arithmetic is what
// the byte gate already checks.

// ---- TWO TABLES SEATED, AND NINE FACES -------------------------------------
//
// Run link100 lane PMFB4. Two of dScMgPachinko_c's tables now hold HOST
// addresses, written at boot by port_mg_pachinko_states_seat below after every
// cell has been compared against the ROM's own code word and a zero adjustment
// word, and BOTH of this file's host copies are gone:
//
//   _ZN15dScMgPachinko_c9Bg_UpdateEv  sPachinkoBgStateTable   4 slots  arity 1
//   _ZN15dScMgPachinko_c12Flyer_UpdateEv  sPachinkoFlyerStateTable   5 slots  arity 1
//
// THE SWITCH STAYS LIVE. sPachinkoPipeStateTable's eleven addresses are still routed
// through port_mg_try_pachinko_1 by the three dispatchers that compile from src
// with hostgen's MG_PMF_CALL swap (_ZN15dScMgPachinko_c14Pipe_UpdateAllEv, _020fe248, _020fda7c),
// which reach it through port_mg_pachinko_call1. Nothing about that path moves.
//
// THE STRIDE, BOTH SIDES (lane PMFB4, run link100, emit_gate3.txt):
//   020fad34  ROM add r3,r1,r0,lsl #3 at 020fad54, pool 020fad8c = 02142604
//   020fb60c  ROM add r3,r1,r0,lsl #3 at 020fb630, pool 020fb66c = 0214266c
// emitted [eax*8] and [eax*8+4] in both listings; /Zp4 a no-op on both.
//
// THE NINE SOURCE PAIRS all read {code, 0} in overlay_0006.bin at the addresses
// src/__sinit_ov006_02131a38.c copies each slot from, all whole-pair copies:
//   02142604[0] <- 0213d928 020fac48/0    0214266c[0] <- 0213d878 020fb4e0/0
//   02142604[1] <- 0213d918 020fab70/0    0214266c[1] <- 0213d8e8 020fb45c/0
//   02142604[2] <- 0213d8a0 020faac8/0    0214266c[2] <- 0213d8e0 020fb230/0
//   02142604[3] <- 0213d900 020fa9c8/0    0214266c[3] <- 0213d890 020fb1c4/0
//                                         0214266c[4] <- 0213d888 020fb0fc/0
//
// THE DISPATCH SHAPE: both TUs emit `push 0 / mov ecx, tab[i*8+4] /
// mov eax, tab[i*8] / add ecx, <this> / call eax` with no caller cleanup -- the
// pushed argument is each src's own literal `(0)`, which is what the ROM does
// too, so both tables take one-argument faces.
//
// ONE /alternatename: ?sPachinkoFlyerStateTable@@3PAP8C@@AEXH@ZA, read off the object
// with dumpbin /symbols. src/PachinkoBg_Update.cpp declares its table inside
// extern "C" and needs none.
#pragma comment(linker, "/alternatename:?sPachinkoFlyerStateTable@@3PAP8C@@AEXH@ZA=_sPachinkoFlyerStateTable")

/* EVERY FACE COUNTS. g_pachinko_state_hits keeps counting exactly the dispatches
   that happen, which is what hal/scene_mg.cpp reads through
   port_mg_pachinko_state_counts. */
#define PC_FACE(sym)                                                          \
    static void __fastcall pc_##sym(void *self, void *dead_edx, int i)        \
    {                                                                         \
        (void)dead_edx;                                                       \
        ++g_pachinko_state_hits;                                              \
        sym(self, i);                                                         \
    }

/* sPachinkoBgStateTable */
PC_FACE(_ZN15dScMgPachinko_c15Bg_StateWaitingEi)
PC_FACE(_ZN15dScMgPachinko_c14Bg_StateRiseInEi)
PC_FACE(_ZN15dScMgPachinko_c14Bg_StateScrollEi)
PC_FACE(_ZN15dScMgPachinko_c15Bg_StateSinkOutEi)
/* sPachinkoFlyerStateTable */
PC_FACE(_ZN15dScMgPachinko_c18Flyer_StateWaitingEi)
PC_FACE(_ZN15dScMgPachinko_c18Flyer_StateDescendEi)
PC_FACE(_ZN15dScMgPachinko_c16Flyer_StateCrossEi)
PC_FACE(_ZN15dScMgPachinko_c16Flyer_StatePauseEi)
PC_FACE(_ZN15dScMgPachinko_c16Flyer_StateLeaveEi)

extern "C" void port_mg_pachinko_states_seat(void)
{
    static int done;
    if (done)
        return;
    done = 1;

    static const struct {
        MgPmf *table;
        const char *name;
        unsigned slot;
        unsigned rom;
        void *face;
    } seats[] = {
        {sPachinkoBgStateTable, "02142604", 0, 0x020fac48u, (void *)pc__ZN15dScMgPachinko_c15Bg_StateWaitingEi},
        {sPachinkoBgStateTable, "02142604", 1, 0x020fab70u, (void *)pc__ZN15dScMgPachinko_c14Bg_StateRiseInEi},
        {sPachinkoBgStateTable, "02142604", 2, 0x020faac8u, (void *)pc__ZN15dScMgPachinko_c14Bg_StateScrollEi},
        {sPachinkoBgStateTable, "02142604", 3, 0x020fa9c8u, (void *)pc__ZN15dScMgPachinko_c15Bg_StateSinkOutEi},

        {sPachinkoFlyerStateTable, "0214266c", 0, 0x020fb4e0u, (void *)pc__ZN15dScMgPachinko_c18Flyer_StateWaitingEi},
        {sPachinkoFlyerStateTable, "0214266c", 1, 0x020fb45cu, (void *)pc__ZN15dScMgPachinko_c18Flyer_StateDescendEi},
        {sPachinkoFlyerStateTable, "0214266c", 2, 0x020fb230u, (void *)pc__ZN15dScMgPachinko_c16Flyer_StateCrossEi},
        {sPachinkoFlyerStateTable, "0214266c", 3, 0x020fb1c4u, (void *)pc__ZN15dScMgPachinko_c16Flyer_StatePauseEi},
        {sPachinkoFlyerStateTable, "0214266c", 4, 0x020fb0fcu, (void *)pc__ZN15dScMgPachinko_c16Flyer_StateLeaveEi},
    };

    for (unsigned i = 0; i < sizeof seats / sizeof seats[0]; ++i) {
        MgPmf *p = &seats[i].table[seats[i].slot];
        if ((unsigned)(p->code) < 0x02000000u || (unsigned)(p->code) >= 0x02400000u) continue;  /* PORTSYNC3: a mount word that is not a DS code address: the class TU owns the live table */
        if (p->code != seats[i].rom || p->adj != 0) {
            std::fprintf(stderr, "FATAL: dScMgPachinko_c state table %s slot %u: "
                         "the sinit left %08x/%d, the ROM's own pairs say "
                         "%08x/0 -- WRONG BYTES\n", seats[i].name,
                         seats[i].slot, p->code, p->adj, seats[i].rom);
            std::abort();
        }
        p->code = (unsigned)(size_t)seats[i].face;
    }
}
