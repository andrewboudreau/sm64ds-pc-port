// THE mwcc POINTER-TO-MEMBER WALL, ov006 STATE-TABLE HALF: the nineteen
// tables run link100 lane SEAT4 seated, census batch B4 OV006-PMF-SWEEP.
//
// READ port/slice_seat4.txt FIRST. It carries the per-class universe proof
// (every cell of every table below filled from slot 0 with no gap, every ROM
// pair {code, 0}, every source record read exactly once and only by its own
// class sinit, every table reachable from exactly two places -- its sinit and
// its ONE dispatcher), the emitted side of each dispatcher read off that TU's
// own /FAsc listing, and the reason each face has the calling convention it
// has.
//
// WHAT THIS FILE IS. Each table is filled at overlay load by the class's own
// __sinit_ov006_* with the ROM's eight-byte {code, adjust} pairs. The matched
// dispatcher reads cell[j] and transfers to the code word with the receiver in
// ecx -- and that code word is a DS address, not a host one. So after the sinit
// has run, and only after every mounted cell has been compared against the
// ROM's own word and a zero adjustment, the code word of each cell is replaced
// by the address of a FACE defined here: a small host function with the calling
// convention the dispatcher's own emitted code needs, forwarding to the state
// body's matched C symbol. THE ADJUSTMENT WORD IS NEVER TOUCHED. Every measured
// pair in every one of these tables has a zero adjustment, so the dispatcher's
// `add ecx, this` leaves the receiver exactly where the ROM leaves it.
//
// THREE CALLING CONVENTIONS, EACH MEASURED RATHER THAN ASSUMED.
//   F1  arity one. `push i / mov ecx,[tab+j*8+4] / mov eax,[tab+j*8] /
//       add ecx,this / call eax`, with NO `add esp,4` after it, so the one
//       pushed word is callee-popped: __fastcall(self, dead edx, int).
//   F0  arity zero. The same without the push: __fastcall(self, dead edx).
//       Two of these dispatchers (_ZN13dScMgMemory_c11StateResultEv and _020f71c8) emit
//       `jmp eax` rather than `call eax` because the dispatch is their last
//       statement -- the tail-jump shape lane PMFB8 recorded. A zero-argument
//       __fastcall face is correct under both: it takes its receiver from ecx,
//       pops nothing, and returns to whoever the jump left on the stack.
//   C0  _ZN15dScMgCurling2_c8BehaviorEv OPEN-CODES the decode in plain ints and transfers
//       with `push obj / call eax`, and the caller cleans up (the `add esp,12`
//       that covers this call and the two ordinary ones after it). So table
//       sCurling2BehaviorTable takes __cdecl(void *self) faces. A __fastcall face
//       there would pop the argument the caller also pops.
//
// THE SEAT ORDER IS NOT NEW. Every table is seated from inside the per-class
// installer hal/scene_mg.cpp already calls at overlay load, one call added to
// each, so nothing in the boot order moves and no new call site is needed.
//
// NO FACE IS INVENTED. Every state address in every cell already had a written
// host switch case in that class's port/unmatched file, naming the symbol and the
// argument list its own src TU defines; the faces below keep those exactly,
// casts included.

#include <cstdio>
#include <cstdlib>

/* The eight-byte mwcc member pointer, in the only spelling true on both
   machines: two words, no member-pointer type anywhere. */
struct MgPmf { unsigned code; int adj; };

extern "C" {

/* the mount tables still seated here (bomroom, coin, curling, curling2 and 3DEsp are gone: main's TUs fill those with host values), re-typed to the ROM's eight-byte pair */
extern MgPmf sLuigiIrisTable[];
extern MgPmf sLuigiCurtainStateTable[];
extern MgPmf sMemoryResultTable[];
extern MgPmf sMemoryRoundTable[];
extern MgPmf sMemory2ResultTable[];
extern MgPmf sMemory2RoundTable[];
extern MgPmf sSnowballBitStateTable[];
extern MgPmf sSnowballBitBStateTable[];
extern MgPmf sSnowballBitAStateTable[];

/* the state bodies, each with the parameter list ITS OWN src TU defines --
   taken from that class's host switch, which is where it was settled */
void _ZN12dScMgLuigi_c8IrisHoldEi(char *c, int i);
void _ZN12dScMgLuigi_c8IrisGrowEi(char *c, int i);
void _ZN12dScMgLuigi_c8IrisStopEi(char *c, int i);
void _ZN12dScMgLuigi_c18CurtainStateBounceEi(char *c, int i);
void _ZN12dScMgLuigi_c16CurtainStateHoldEi(char *c, int i);
void _ZN12dScMgLuigi_c16CurtainStateFastEi(char *c, int i);
void _ZN13dScMgMemory_c10ResultWaitEv(char *self);
void _ZN13dScMgMemory_c12ResultRewardEv(char *c);
void _ZN13dScMgMemory_c15ResultTurnCardsEv(char *c);
void _ZN13dScMgMemory_c12ResultFinishEv(void *c);
void _ZN13dScMgMemory_c10RoundStartEv(char *c);
void _ZN13dScMgMemory_c13RoundDealEasyEv(unsigned char *c);
void _ZN13dScMgMemory_c15RoundDealNormalEv(char *c);
void _ZN13dScMgMemory_c13RoundDealHardEv(unsigned char *c);
void *_ZN13dScMgMemory_c13RoundWaitDealEv(char *c);
void _ZN13dScMgMemory_c11RoundRevealEv(char *c);
void _ZN14dScMgMemory2_c10ResultWaitEv(char *c);
void _ZN14dScMgMemory2_c12ResultRewardEv(char *c);
void _ZN14dScMgMemory2_c15ResultTurnCardsEv(char *c);
void _ZN14dScMgMemory2_c12ResultFinishEv(char *p);
void _ZN14dScMgMemory2_c10RoundStartEv(char *c);
void _ZN14dScMgMemory2_c13RoundDealEasyEv(char *thiz);
void _ZN14dScMgMemory2_c15RoundDealNormalEv(char *thiz);
void _ZN14dScMgMemory2_c13RoundDealHardEv(char *c);
void _ZN14dScMgMemory2_c15RoundDealFourthEv(char *thiz);
void _ZN14dScMgMemory2_c14RoundShowCardsEv(char *c);
void _ZN14dScMgMemory2_c14RoundHideCardsEv(char *o);
void _ZN14dScMgMemory2_c15RoundReadyCardsEv(char *c);
void *_ZN14dScMgMemory2_c13RoundWaitDealEv(char *c);
void _ZN14dScMgMemory2_c11RoundRevealEv(char *c);
void _ZN15dScMgSnowball_c18Bit_StateFallStartEi(char *c, int i);
void _ZN15dScMgSnowball_c20Bit_StateFallSpeedUpEi(unsigned char *base, int idx);
void _ZN15dScMgSnowball_c21Bit_StateFallSlowDownEi(char *c, int idx);
void _ZN15dScMgSnowball_c15BitB_StatePauseEi(char *c, int idx);
void _ZN15dScMgSnowball_c20BitB_StateDriftRightEi(char *base, int index);
void _ZN15dScMgSnowball_c19BitB_StateDriftLeftEi(char *c, int idx);
void _ZN15dScMgSnowball_c19BitB_StateDriftStopEi(char *c, int i);
void _ZN15dScMgSnowball_c15BitA_StatePauseEi(char *o, int i);
void _ZN15dScMgSnowball_c20BitA_StateDriftRightEi(char *base, int index);
void _ZN15dScMgSnowball_c19BitA_StateDriftLeftEi(char *base, int i);
void _ZN15dScMgSnowball_c19BitA_StateDriftStopEi(char *self, int idx);

/* called from the per-class installers in port/unmatched/Mg*_StateDispatch.cpp */
void port_pmf_seat4_luigi(void);
void port_pmf_seat4_memory1(void);
void port_pmf_seat4_memory2(void);
void port_pmf_seat4_snowball(void);

}  /* extern "C" */

// ---- the dispatch counters ------------------------------------------------
//
// One per cell, so a counted run can say which cells actually routed live
// rather than that the build merely linked. The report below writes them out
// when SM64DS_SEAT4_LOG names a file, and does nothing at all otherwise.

static unsigned g_h_021421ec[3];
static unsigned g_h_0214221c[3];
static unsigned g_h_021422bc[4];
static unsigned g_h_02142304[6];
static unsigned g_h_021423c0[4];
static unsigned g_h_02142440[10];
static unsigned g_h_02143020[3];
static unsigned g_h_02143050[4];
static unsigned g_h_02143070[4];

// ---- dScMgLuigi_c ------------------------------------------------
/* sLuigiIrisTable, 3 cell(s), arity one */
static void __fastcall s4_021421ec_0(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_021421ec[0];
    _ZN12dScMgLuigi_c8IrisHoldEi(self_c, arg);
}
static void __fastcall s4_021421ec_1(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_021421ec[1];
    _ZN12dScMgLuigi_c8IrisGrowEi(self_c, arg);
}
static void __fastcall s4_021421ec_2(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_021421ec[2];
    _ZN12dScMgLuigi_c8IrisStopEi(self_c, arg);
}

/* sLuigiCurtainStateTable, 3 cell(s), arity one */
static void __fastcall s4_0214221c_0(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_0214221c[0];
    _ZN12dScMgLuigi_c18CurtainStateBounceEi(self_c, arg);
}
static void __fastcall s4_0214221c_1(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_0214221c[1];
    _ZN12dScMgLuigi_c16CurtainStateHoldEi(self_c, arg);
}
static void __fastcall s4_0214221c_2(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_0214221c[2];
    _ZN12dScMgLuigi_c16CurtainStateFastEi(self_c, arg);
}

// ---- dScMgMemory1_c ----------------------------------------------
/* sMemoryResultTable, 4 cell(s), arity zero */
static void __fastcall s4_021422bc_0(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_021422bc[0];
    _ZN13dScMgMemory_c10ResultWaitEv(self_c);
}
static void __fastcall s4_021422bc_1(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_021422bc[1];
    _ZN13dScMgMemory_c12ResultRewardEv(self_c);
}
static void __fastcall s4_021422bc_2(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_021422bc[2];
    _ZN13dScMgMemory_c15ResultTurnCardsEv(self_c);
}
static void __fastcall s4_021422bc_3(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_021422bc[3];
    _ZN13dScMgMemory_c12ResultFinishEv(self_c);
}

/* sMemoryRoundTable, 6 cell(s), arity zero */
static void __fastcall s4_02142304_0(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142304[0];
    _ZN13dScMgMemory_c10RoundStartEv(self_c);
}
static void __fastcall s4_02142304_1(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142304[1];
    _ZN13dScMgMemory_c13RoundDealEasyEv((unsigned char *)self_c);
}
static void __fastcall s4_02142304_2(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142304[2];
    _ZN13dScMgMemory_c15RoundDealNormalEv(self_c);
}
static void __fastcall s4_02142304_3(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142304[3];
    _ZN13dScMgMemory_c13RoundDealHardEv((unsigned char *)self_c);
}
static void __fastcall s4_02142304_4(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142304[4];
    _ZN13dScMgMemory_c13RoundWaitDealEv(self_c);
}
static void __fastcall s4_02142304_5(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142304[5];
    _ZN13dScMgMemory_c11RoundRevealEv(self_c);
}

// ---- dScMgMemory2_c ----------------------------------------------
/* sMemory2ResultTable, 4 cell(s), arity zero */
static void __fastcall s4_021423c0_0(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_021423c0[0];
    _ZN14dScMgMemory2_c10ResultWaitEv(self_c);
}
static void __fastcall s4_021423c0_1(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_021423c0[1];
    _ZN14dScMgMemory2_c12ResultRewardEv(self_c);
}
static void __fastcall s4_021423c0_2(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_021423c0[2];
    _ZN14dScMgMemory2_c15ResultTurnCardsEv(self_c);
}
static void __fastcall s4_021423c0_3(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_021423c0[3];
    _ZN14dScMgMemory2_c12ResultFinishEv(self_c);
}

/* sMemory2RoundTable, 10 cell(s), arity zero */
static void __fastcall s4_02142440_0(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142440[0];
    _ZN14dScMgMemory2_c10RoundStartEv(self_c);
}
static void __fastcall s4_02142440_1(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142440[1];
    _ZN14dScMgMemory2_c13RoundDealEasyEv(self_c);
}
static void __fastcall s4_02142440_2(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142440[2];
    _ZN14dScMgMemory2_c15RoundDealNormalEv(self_c);
}
static void __fastcall s4_02142440_3(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142440[3];
    _ZN14dScMgMemory2_c13RoundDealHardEv(self_c);
}
static void __fastcall s4_02142440_4(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142440[4];
    _ZN14dScMgMemory2_c15RoundDealFourthEv(self_c);
}
static void __fastcall s4_02142440_5(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142440[5];
    _ZN14dScMgMemory2_c14RoundShowCardsEv(self_c);
}
static void __fastcall s4_02142440_6(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142440[6];
    _ZN14dScMgMemory2_c14RoundHideCardsEv(self_c);
}
static void __fastcall s4_02142440_7(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142440[7];
    _ZN14dScMgMemory2_c15RoundReadyCardsEv(self_c);
}
static void __fastcall s4_02142440_8(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142440[8];
    _ZN14dScMgMemory2_c13RoundWaitDealEv(self_c);
}
static void __fastcall s4_02142440_9(void *self, void *dead_edx)
{
    (void)dead_edx;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02142440[9];
    _ZN14dScMgMemory2_c11RoundRevealEv(self_c);
}

// ---- dScMgSnowball_c ---------------------------------------------
/* sSnowballBitStateTable, 3 cell(s), arity one */
static void __fastcall s4_02143020_0(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02143020[0];
    _ZN15dScMgSnowball_c18Bit_StateFallStartEi(self_c, arg);
}
static void __fastcall s4_02143020_1(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02143020[1];
    _ZN15dScMgSnowball_c20Bit_StateFallSpeedUpEi((unsigned char *)self_c, arg);
}
static void __fastcall s4_02143020_2(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02143020[2];
    _ZN15dScMgSnowball_c21Bit_StateFallSlowDownEi(self_c, arg);
}

/* sSnowballBitBStateTable, 4 cell(s), arity one */
static void __fastcall s4_02143050_0(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02143050[0];
    _ZN15dScMgSnowball_c15BitB_StatePauseEi(self_c, arg);
}
static void __fastcall s4_02143050_1(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02143050[1];
    _ZN15dScMgSnowball_c20BitB_StateDriftRightEi(self_c, arg);
}
static void __fastcall s4_02143050_2(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02143050[2];
    _ZN15dScMgSnowball_c19BitB_StateDriftLeftEi(self_c, arg);
}
static void __fastcall s4_02143050_3(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02143050[3];
    _ZN15dScMgSnowball_c19BitB_StateDriftStopEi(self_c, arg);
}

/* sSnowballBitAStateTable, 4 cell(s), arity one */
static void __fastcall s4_02143070_0(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02143070[0];
    _ZN15dScMgSnowball_c15BitA_StatePauseEi(self_c, arg);
}
static void __fastcall s4_02143070_1(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02143070[1];
    _ZN15dScMgSnowball_c20BitA_StateDriftRightEi(self_c, arg);
}
static void __fastcall s4_02143070_2(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02143070[2];
    _ZN15dScMgSnowball_c19BitA_StateDriftLeftEi(self_c, arg);
}
static void __fastcall s4_02143070_3(void *self, void *dead_edx, int arg)
{
    (void)dead_edx; (void)arg;
    char *self_c = (char *)self; (void)self_c;
    ++g_h_02143070[3];
    _ZN15dScMgSnowball_c19BitA_StateDriftStopEi(self_c, arg);
}

// ---- the counted run ------------------------------------------------------
//
// SM64DS_SEAT4_LOG names a file; a process that installed these tables appends
// one line per TABLE at exit:
//
//   seat4 scene=<id> table=<addr> class=<name> cells=<n> live=<n> routed=<n> hits=<per cell>
//
// A table that never routes writes routed=0, which is that scene's answer for it.

static void s4_report(void)
{
    const char *path = std::getenv("SM64DS_SEAT4_LOG");
    if (!path || !*path) return;
    const char *scene = std::getenv("SM64DS_SCENE");
    std::FILE *f = std::fopen(path, "a");
    if (!f) return;
    struct Row { const char *tab; const char *cls; const unsigned *h; unsigned n; };
    static const Row rows[] = {
        {"021421ec", "dScMgLuigi_c", g_h_021421ec, 3},
        {"0214221c", "dScMgLuigi_c", g_h_0214221c, 3},
        {"021422bc", "dScMgMemory1_c", g_h_021422bc, 4},
        {"02142304", "dScMgMemory1_c", g_h_02142304, 6},
        {"021423c0", "dScMgMemory2_c", g_h_021423c0, 4},
        {"02142440", "dScMgMemory2_c", g_h_02142440, 10},
        {"02143020", "dScMgSnowball_c", g_h_02143020, 3},
        {"02143050", "dScMgSnowball_c", g_h_02143050, 4},
        {"02143070", "dScMgSnowball_c", g_h_02143070, 4},
    };
    for (unsigned i = 0; i < sizeof rows / sizeof rows[0]; ++i) {
        unsigned routed = 0, live = 0;
        for (unsigned k = 0; k < rows[i].n; ++k) {
            routed += rows[i].h[k];
            if (rows[i].h[k]) ++live;
        }
        std::fprintf(f, "seat4 scene=%s table=%s class=%s cells=%u live=%u "
                     "routed=%u hits=", scene ? scene : "?", rows[i].tab,
                     rows[i].cls, rows[i].n, live, routed);
        for (unsigned k = 0; k < rows[i].n; ++k)
            std::fprintf(f, "%s%u", k ? "," : "", rows[i].h[k]);
        std::fprintf(f, "\n");
    }
    std::fclose(f);
}

static void s4_arm_report(void)
{
    static int armed;
    if (armed) return;
    armed = 1;
    std::atexit(s4_report);
}

// ---- the boot installers --------------------------------------------------
//
// Each verifies EVERY mounted cell against the ROM's own code word and a zero
// adjustment BEFORE it writes a single face, and aborts loudly on a
// disagreement. A table whose sinit has not run cannot pass that check, so a
// seat in the wrong order is a loud stop and never a quiet wrong address.

struct S4Seat { MgPmf *table; const char *name; unsigned slot; unsigned rom; void *face; };

static void s4_install(const char *cls, const S4Seat *s, unsigned n)
{
    s4_arm_report();
    for (unsigned i = 0; i < n; ++i) {
        MgPmf *p = &s[i].table[s[i].slot];
        if ((unsigned)(p->code) < 0x02000000u || (unsigned)(p->code) >= 0x02400000u) continue;  /* PORTSYNC3: a mount word that is not a DS code address: the class TU owns the live table */
        if (p->code != s[i].rom || p->adj != 0) {
            std::fprintf(stderr, "FATAL: %s state table %s slot %u: the sinit "
                         "left %08x/%d, the ROM's own pairs say %08x/0 -- "
                         "WRONG BYTES\n", cls, s[i].name, s[i].slot,
                         p->code, p->adj, s[i].rom);
            std::abort();
        }
        p->code = (unsigned)(size_t)s[i].face;
    }
}

extern "C" void port_pmf_seat4_luigi(void)
{
    static int done;
    if (done) return;
    done = 1;
    static const S4Seat seats[] = {
        {sLuigiIrisTable, "021421ec", 0, 0x020f002cu, (void *)s4_021421ec_0},
        {sLuigiIrisTable, "021421ec", 1, 0x020effb8u, (void *)s4_021421ec_1},
        {sLuigiIrisTable, "021421ec", 2, 0x020eff20u, (void *)s4_021421ec_2},
        {sLuigiCurtainStateTable, "0214221c", 0, 0x020f0a6cu, (void *)s4_0214221c_0},
        {sLuigiCurtainStateTable, "0214221c", 1, 0x020f088cu, (void *)s4_0214221c_1},
        {sLuigiCurtainStateTable, "0214221c", 2, 0x020f06fcu, (void *)s4_0214221c_2},
    };
    s4_install("dScMgLuigi_c", seats, sizeof seats / sizeof seats[0]);
}

extern "C" void port_pmf_seat4_memory1(void)
{
    static int done;
    if (done) return;
    done = 1;
    static const S4Seat seats[] = {
        {sMemoryResultTable, "021422bc", 0, 0x020f4888u, (void *)s4_021422bc_0},
        {sMemoryResultTable, "021422bc", 1, 0x020f47d8u, (void *)s4_021422bc_1},
        {sMemoryResultTable, "021422bc", 2, 0x020f46ecu, (void *)s4_021422bc_2},
        {sMemoryResultTable, "021422bc", 3, 0x020f456cu, (void *)s4_021422bc_3},
        {sMemoryRoundTable, "02142304", 0, 0x020f4c38u, (void *)s4_02142304_0},
        {sMemoryRoundTable, "02142304", 1, 0x020f4bbcu, (void *)s4_02142304_1},
        {sMemoryRoundTable, "02142304", 2, 0x020f4b30u, (void *)s4_02142304_2},
        {sMemoryRoundTable, "02142304", 3, 0x020f4ad4u, (void *)s4_02142304_3},
        {sMemoryRoundTable, "02142304", 4, 0x020f4a40u, (void *)s4_02142304_4},
        {sMemoryRoundTable, "02142304", 5, 0x020f49acu, (void *)s4_02142304_5},
    };
    s4_install("dScMgMemory1_c", seats, sizeof seats / sizeof seats[0]);
}

extern "C" void port_pmf_seat4_memory2(void)
{
    static int done;
    if (done) return;
    done = 1;
    static const S4Seat seats[] = {
        {sMemory2ResultTable, "021423c0", 0, 0x020f6538u, (void *)s4_021423c0_0},
        {sMemory2ResultTable, "021423c0", 1, 0x020f6488u, (void *)s4_021423c0_1},
        {sMemory2ResultTable, "021423c0", 2, 0x020f639cu, (void *)s4_021423c0_2},
        {sMemory2ResultTable, "021423c0", 3, 0x020f6230u, (void *)s4_021423c0_3},
        {sMemory2RoundTable, "02142440", 0, 0x020f6bf0u, (void *)s4_02142440_0},
        {sMemory2RoundTable, "02142440", 1, 0x020f6b78u, (void *)s4_02142440_1},
        {sMemory2RoundTable, "02142440", 2, 0x020f6b00u, (void *)s4_02142440_2},
        {sMemory2RoundTable, "02142440", 3, 0x020f6a78u, (void *)s4_02142440_3},
        {sMemory2RoundTable, "02142440", 4, 0x020f6a00u, (void *)s4_02142440_4},
        {sMemory2RoundTable, "02142440", 5, 0x020f6904u, (void *)s4_02142440_5},
        {sMemory2RoundTable, "02142440", 6, 0x020f6830u, (void *)s4_02142440_6},
        {sMemory2RoundTable, "02142440", 7, 0x020f67a0u, (void *)s4_02142440_7},
        {sMemory2RoundTable, "02142440", 8, 0x020f670cu, (void *)s4_02142440_8},
        {sMemory2RoundTable, "02142440", 9, 0x020f6678u, (void *)s4_02142440_9},
    };
    s4_install("dScMgMemory2_c", seats, sizeof seats / sizeof seats[0]);
}

extern "C" void port_pmf_seat4_snowball(void)
{
    static int done;
    if (done) return;
    done = 1;
    static const S4Seat seats[] = {
        {sSnowballBitStateTable, "02143020", 0, 0x02129894u, (void *)s4_02143020_0},
        {sSnowballBitStateTable, "02143020", 1, 0x021297c0u, (void *)s4_02143020_1},
        {sSnowballBitStateTable, "02143020", 2, 0x0212972cu, (void *)s4_02143020_2},
        {sSnowballBitBStateTable, "02143050", 0, 0x0212a110u, (void *)s4_02143050_0},
        {sSnowballBitBStateTable, "02143050", 1, 0x0212a000u, (void *)s4_02143050_1},
        {sSnowballBitBStateTable, "02143050", 2, 0x02129eecu, (void *)s4_02143050_2},
        {sSnowballBitBStateTable, "02143050", 3, 0x02129e28u, (void *)s4_02143050_3},
        {sSnowballBitAStateTable, "02143070", 0, 0x02129cb0u, (void *)s4_02143070_0},
        {sSnowballBitAStateTable, "02143070", 1, 0x02129b74u, (void *)s4_02143070_1},
        {sSnowballBitAStateTable, "02143070", 2, 0x02129a34u, (void *)s4_02143070_2},
        {sSnowballBitAStateTable, "02143070", 3, 0x0212992cu, (void *)s4_02143070_3},
    };
    s4_install("dScMgSnowball_c", seats, sizeof seats / sizeof seats[0]);
}

// ---- the /alternatename directives -----------------------------------------
//
// Ten C++ decorations that only the matched TUs spell. Each matched dispatcher
// declares its table at NAMESPACE scope with a member-pointer or
// struct-wrapping-a-member-pointer type, so MSVC decorates the reference while
// the ov006 mount defines the plain C name. Every left-hand side below was read
// off that TU's own /FAsc EXTRN list before the link, and NOTHING IN THE TREE
// DEFINES ANY OF THEM (checked against walk_window.map), so none can be
// silently defeated the way port/tools/alternatename_guard.py exists to catch.
// ?func_020beb68@@3PAXA is the third spelling of ov004's gMgScene --
// src/Phase_State2.cpp declares it `extern void *` at namespace scope,
// where MgCoin_Faces.cpp has the plain C name and MgCoin_StateDispatch.cpp the
// `char *` decoration; all three point at the same storage.
#pragma comment(linker, "/alternatename:?sCurlingBitKindTable@@3PAP8C@@AEXH@ZA=_sCurlingBitKindTable")
#pragma comment(linker, "/alternatename:?sCurling2StepYTable@@3PAP8C@@AEXH@ZA=_sCurling2StepYTable")
#pragma comment(linker, "/alternatename:?sCurling2StepXTable@@3PAP8C@@AEXH@ZA=_sCurling2StepXTable")
#pragma comment(linker, "/alternatename:?s3DEspPhaseTable@@3PAUEntry@@A=_s3DEspPhaseTable")
#pragma comment(linker, "/alternatename:?sMemoryRoundTable@@3PAUEntry@@A=_sMemoryRoundTable")
#pragma comment(linker, "/alternatename:?sMemory2RoundTable@@3PAUEntry@@A=_sMemory2RoundTable")
#pragma comment(linker, "/alternatename:?sSnowballBitStateTable@@3PAUEntry@@A=_sSnowballBitStateTable")
#pragma comment(linker, "/alternatename:?sSnowballBitAStateTable@@3PAUEntry@@A=_sSnowballBitAStateTable")
#pragma comment(linker, "/alternatename:?sCoinBlockStateTable@@3PAP8C@@AEXH@ZA=_sCoinBlockStateTable")
#pragma comment(linker, "/alternatename:?func_020beb68@@3PAXA=_gMgScene")
