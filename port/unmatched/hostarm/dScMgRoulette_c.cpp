/* HOST ARM recovered by PORTSYNC2.
 * Old path: src/minigames/d_s_mg_roulette_methods.cpp (port line). Its _MSC_VER arm passed
 * the payout to _Z19MgCoins_StartPayouti from OnTurnIntoEgg case 7; the 2026-10 main
 * merge (749f5799a) took main's src/, which keeps the file (now
 * dScMgRoulette_c::OnTurnIntoEgg with named members) but calls the callee with
 * no argument and dropped the arm. This file defines the member again: MAIN'S
 * BODY, statement for statement, with only the arm's change applied. The same
 * @symbol body must be parked in the TU's hostgen copy. mwccarm never sees this
 * file, so nothing here moves a ROM byte. */
#include "common.h"
#include "types.h"
#include "dScMgRoulette_c.h"
#include "dExtSimpleModel_c.h"
#include "decl_common.h"
#include "PlayerInput.h"

/* Declared as main's TU (src/minigames/d_s_mg_roulette_methods.cpp) declares them.
   At the catchup1 bump main's body passes the payout itself
   (MgCoins_StartPayout(mScore)), which is what the arm added: this copy is now
   main's body unchanged, a candidate for retirement with its hostgen park. */
extern "C" {
void FreeGfxSlotsById(int a);
extern u16 gMgCoinsStatus;
}
extern "C++" void MgCoins_StartPayout(int x);

// @symbol _ZN15dScMgRoulette_c11CanEndStateEi
/* Runs the end of the round, phases 5 to 9. Returns 1 when it is done. */
int dScMgRoulette_c::CanEndState(int /* mode */)
{

    switch (mPhase) {
    case 5:
        FreeGfxSlotsById(0x1d);
        if (RouletteResults_Tick() != 0)
            (mPhase)++;
        break;
    case 6:
        if (mDealer.IsIdle() != 0) {
            mPhaseTimer = 0x3c;
            mDealIndex = 0;
            (mPhase)++;
        }
        break;
    case 7:
        (mPhaseTimer)--;
        if (mPhaseTimer == 0) {
            if (mScore != 0)
                MgCoins_StartPayout(mScore);
            (mPhase)++;
        }
        break;
    case 8:
        (mPhase)++;
        /* fall through */
    case 9:
    default:
        if (gMgCoinsStatus <= 1)
            return 1;
        break;
    }
    return 0;
}
