/* Flat Itanium spellings of decomp FREE functions and STATIC members, bound
 * onto the MSVC names main's C++ translation units now define.
 *
 * Why this file exists (catchup1 Stage C round 3, lane FACE4). Port code
 * written before main turned these into real C++ calls them by their flat
 * Itanium name at C linkage (`extern "C" int _ZN14Absorb...(...)`). facegen
 * covers member functions (port/faces_sync.txt); it refuses free functions,
 * namespace functions and static members on purpose, because there is no
 * receiver to move. Those are __cdecl on both sides, so one /alternatename
 * per name is the whole bridge: "if the flat name is still undefined, use
 * the decorated one".
 *
 * Every row was checked four ways before it went in:
 *   1. the flat name has exactly one ROM address in config/arm9 symbols.txt files;
 *   2. that address lies inside the .text range the config/arm9 delinks.txt files
 *      give the source file named on the row;
 *   3. dumpbin of that file's host object (port_slice_shared) DEFINES the
 *      decorated name;
 *   4. undname of the decorated name gives the same qualified name, a
 *      __cdecl free or static function, and the same parameter count as the
 *      Itanium mangle.
 *
 * Do not add a row whose flat name some port file still DEFINES (a host copy
 * under unmatched/, a hostgen output): the alias would be defeated and
 * port/tools/alternatename_guard.py fails the build on it. A row with the
 * same left-hand side and a DIFFERENT target
 * anywhere else in the link is a link conflict, so grep port/hal for the
 * flat name before adding one.
 */

/* 0x0203b438 Absorb0203b438::func_0203b438 (N *, N *, N *) -> int
 * src/runtime/actor/ActorBase_SceneNode.cpp, the scene-tree link. Callers:
 * unmatched/hostarm/_ZN7fBase_cC2Ev.cpp. */
#pragma comment(linker, "/alternatename:__ZN14Absorb0203b43813func_0203b438EPNS_1NES1_S1_=?func_0203b438@Absorb0203b438@@YAHPAUN@1@00@Z")

/* 0x0203b3c0 Absorb0203b3c0::func_0203b3c0 (int *, Node *) -> int
 * src/runtime/actor/ActorBase_SceneNode.cpp, the scene-tree unlink. Callers:
 * unmatched/hostarm/_ZN7fBase_c21AfterCleanupResourcesEj.cpp and
 * hal/level_change.cpp. */
#pragma comment(linker, "/alternatename:__ZN14Absorb0203b3c013func_0203b3c0EPiPNS_4NodeE=?func_0203b3c0@Absorb0203b3c0@@YAHPAHPAUNode@1@@Z")

/* 0x02042ffc fBase_c::createChild (u32, fBase_c *, u32, int) -> fBase_c *,
 * a static member in src/runtime/actor/ActorBase.cpp. Callers: the two
 * Spawn copies in unmatched/ActorDerived_Spawn.cpp, which declare an int
 * return (a pointer and an int come back in the same register). Main's body
 * forwards all four arguments, so the four-argument host copy that used to
 * define this flat name is retired. */
#pragma comment(linker, "/alternatename:__ZN7fBase_c11createChildEjPS_ji=?createChild@fBase_c@@SAPAU1@IPAU1@IH@Z")

/* ---- catchup1 Stage C round 3: free functions, namespace functions and
 * static members INTEG4's FOLD_2 probe link left unresolved under their
 * flat names. Grouped by the source file that defines them; each row
 * carries its ROM address. Same four checks as the rows above. */

/* src/KuppaScript.cpp */
#pragma comment(linker, "/alternatename:__ZN11KuppaScript5ResetEv=?Reset@KuppaScript@@YAXXZ")  /* 0x0200f04c */

/* src/Message.cpp */
#pragma comment(linker, "/alternatename:__Z13func_0201f32ci=?func_0201f32c@@YAXH@Z")  /* 0x0201f32c */

/* src/Message2.cpp */
#pragma comment(linker, "/alternatename:__ZN7Message9AddChoiceEv=?AddChoice@Message@@SAXXZ")  /* 0x0201ab6c */
#pragma comment(linker, "/alternatename:__Z13func_0201aca4v=?func_0201aca4@@YAXXZ")  /* 0x0201aca4 */
#pragma comment(linker, "/alternatename:__ZN7Message11AddCharPairEv=?AddCharPair@Message@@SAXXZ")  /* 0x0201adac */

/* src/SharedFilePtr.cpp */
#pragma comment(linker, "/alternatename:__Z13func_020178b4i=?func_020178b4@@YAHH@Z")  /* 0x020178b4 */
#pragma comment(linker, "/alternatename:__Z13func_020178ccPvj=?func_020178cc@@YAPAXPAXI@Z")  /* 0x020178cc */
#pragma comment(linker, "/alternatename:__Z13func_02017a0ci=?func_02017a0c@@YAHH@Z")  /* 0x02017a0c */
#pragma comment(linker, "/alternatename:__Z13func_02017a24Pvj=?func_02017a24@@YAPAXPAXI@Z")  /* 0x02017a24 */
#pragma comment(linker, "/alternatename:__Z13func_02017ab4i=?func_02017ab4@@YAHH@Z")  /* 0x02017ab4 */
#pragma comment(linker, "/alternatename:__Z13func_02017accPvj=?func_02017acc@@YAPAXPAXI@Z")  /* 0x02017acc */
#pragma comment(linker, "/alternatename:__Z27SharedFilePtr_Destruct_Clsni=?SharedFilePtr_Destruct_Clsn@@YAHH@Z")  /* 0x02017b34 */
#pragma comment(linker, "/alternatename:__Z13func_02017b4cPvj=?func_02017b4c@@YAPAXPAXI@Z")  /* 0x02017b4c */

/* src/engine/collision/d_bg_ch_sph_crr_query.cpp */
#pragma comment(linker, "/alternatename:__Z13func_02037dc4i=?func_02037dc4@@YAHH@Z")  /* 0x02037dc4 */

/* src/engine/particle/Particle.cpp */
#pragma comment(linker, "/alternatename:__ZN8Particle6System13NewCheckWaterEjjiiiPK11Vector3_16f=?NewCheckWater@System@Particle@@SAIIIHHHPBUVector3_16f@@@Z")  /* 0x02022d44 */

/* src/game/actors/d_a_demo.cpp */
#pragma comment(linker, "/alternatename:__Z18daDemo_c_classInitv=?daDemo_c_classInit@@YAPAUdaDemo_c@@XZ")  /* 0x020f8808 */

/* src/game/stages/d_s_boot.cpp */
#pragma comment(linker, "/alternatename:__Z19dScBoot_c_classInitv=?dScBoot_c_classInit@@YAPAUdScBoot_c@@XZ")  /* 0x02023624 */

/* src/game/stages/d_s_boot_methods.cpp */
#pragma comment(linker, "/alternatename:__Z13func_02005324Pv=?func_02005324@@YAHPAX@Z")  /* 0x02005324 */

/* src/game/stages/d_s_dsmt.cpp */
#pragma comment(linker, "/alternatename:__Z17DSMT_MenuCallbacki=?DSMT_MenuCallback@@YAEH@Z")  /* 0x020cc600 */
#pragma comment(linker, "/alternatename:__Z20DSMT_MenuReleaseFilePv=?DSMT_MenuReleaseFile@@YAXPAX@Z")  /* 0x020cca68 */
#pragma comment(linker, "/alternatename:__Z17DSMT_MenuLoadFilei=?DSMT_MenuLoadFile@@YAPAXH@Z")  /* 0x020cca74 */
#pragma comment(linker, "/alternatename:__Z21DSMT_MenuMaxAllocSizev=?DSMT_MenuMaxAllocSize@@YAXXZ")  /* 0x020cca80 */
#pragma comment(linker, "/alternatename:__Z13DSMT_MenuFreePv=?DSMT_MenuFree@@YAXPAX@Z")  /* 0x020cca98 */
#pragma comment(linker, "/alternatename:__Z14DSMT_MenuAllocj=?DSMT_MenuAlloc@@YAPAXI@Z")  /* 0x020ccab4 */
#pragma comment(linker, "/alternatename:__Z19dScDSMT_c_classInitv=?dScDSMT_c_classInit@@YAPAUdScDSMT_c@@XZ")  /* 0x020ccad0 */

/* src/game/stages/d_s_game_over.cpp */
#pragma comment(linker, "/alternatename:__Z23dScGameOver_c_classInitv=?dScGameOver_c_classInit@@YAPAUdScGameOver_c@@XZ")  /* 0x020b1118 */

/* src/game/stages/d_s_mb.cpp */
#pragma comment(linker, "/alternatename:__Z17dScMB_c_classInitv=?dScMB_c_classInit@@YAPAXXZ")  /* 0x020352b4 */

/* src/game/stages/d_s_stage_methods.cpp */
#pragma comment(linker, "/alternatename:__Z9ExitLevelv=?ExitLevel@@YAXXZ")  /* 0x02029bd4 */
#pragma comment(linker, "/alternatename:__Z21CollectStarInCurLeveli=?CollectStarInCurLevel@@YAXH@Z")  /* 0x0202a6ac */
#pragma comment(linker, "/alternatename:__Z25IsStarCollectedInCurLeveli=?IsStarCollectedInCurLevel@@YAHH@Z")  /* 0x0202a6c8 */
#pragma comment(linker, "/alternatename:__Z8NumCoinsv=?NumCoins@@YAFXZ")  /* 0x0202a770 */
#pragma comment(linker, "/alternatename:__Z9GiveCoinsii=?GiveCoins@@YAXHH@Z")  /* 0x0202a7b8 */
#pragma comment(linker, "/alternatename:__Z18InitControllerModeih=?InitControllerMode@@YAXHE@Z")  /* 0x0202a87c */
#pragma comment(linker, "/alternatename:__Z17StartMinigameMenuh=?StartMinigameMenu@@YAXE@Z")  /* 0x0202adf4 */
#pragma comment(linker, "/alternatename:__Z28IsLevelTinyHugeIslandOutsidei=?IsLevelTinyHugeIslandOutside@@YAHH@Z")  /* 0x0202ddd4 */
#pragma comment(linker, "/alternatename:__Z19IsLevelInsideCastlei=?IsLevelInsideCastle@@YAHH@Z")  /* 0x0202ddf4 */
#pragma comment(linker, "/alternatename:__Z15GetSoundGroupIDii=?GetSoundGroupID@@YAHHH@Z")  /* 0x0202de24 */
#pragma comment(linker, "/alternatename:__Z20dScStage_c_classInitv=?dScStage_c_classInit@@YAPAUdScStage_c@@XZ")  /* 0x0202e088 */

/* src/game/stages/d_s_star_sel_methods.cpp */
#pragma comment(linker, "/alternatename:__Z22dScStarSel_c_classInitv=?dScStarSel_c_classInit@@YAPAUdScStarSel_c@@XZ")  /* 0x020b04f0 */

/* src/game/stages/d_s_title.cpp */
#pragma comment(linker, "/alternatename:__Z20dScTitle_c_classInitv=?dScTitle_c_classInit@@YAPAUdScTitle_c@@XZ")  /* 0x020adc10 */

/* src/minigames/d_mg3d_esp_anim_set.cpp */
#pragma comment(linker, "/alternatename:__Z22dScMg3DEsp_c_classInitv=?dScMg3DEsp_c_classInit@@YAPAUdScMg3DEsp_c@@XZ")  /* 0x020ea1f0 */

/* src/minigames/d_s_mg_amida.cpp */
#pragma comment(linker, "/alternatename:__Z22dScMgAmida_c_classInitv=?dScMgAmida_c_classInit@@YAPAXXZ")  /* 0x020d5974 */

/* src/minigames/d_s_mg_bomroom.cpp */
#pragma comment(linker, "/alternatename:__Z24dScMgBomroom_c_classInitv=?dScMgBomroom_c_classInit@@YAPAXXZ")  /* 0x020d9574 */

/* src/minigames/d_s_mg_bsc.cpp */
#pragma comment(linker, "/alternatename:__Z20dScMgBSC_c_classInitv=?dScMgBSC_c_classInit@@YAPAXXZ")  /* 0x021255f8 */

/* src/minigames/d_s_mg_card.cpp */
#pragma comment(linker, "/alternatename:__Z21dScMgCard_c_classInitv=?dScMgCard_c_classInit@@YAPAXXZ")  /* 0x020dbd54 */

/* src/minigames/d_s_mg_coin.cpp */
#pragma comment(linker, "/alternatename:__Z21dScMgCoin_c_classInitv=?dScMgCoin_c_classInit@@YAPAXXZ")  /* 0x020de940 */

/* src/minigames/d_s_mg_cup.cpp */
#pragma comment(linker, "/alternatename:__Z20dScMgCup_c_classInitv=?dScMgCup_c_classInit@@YAPAXXZ")  /* 0x020e0574 */

/* src/minigames/d_s_mg_curling.cpp */
#pragma comment(linker, "/alternatename:__Z24dScMgCurling_c_classInitv=?dScMgCurling_c_classInit@@YAPAXXZ")  /* 0x020e3820 */

/* src/minigames/d_s_mg_curling2.cpp */
#pragma comment(linker, "/alternatename:__Z25dScMgCurling2_c_classInitv=?dScMgCurling2_c_classInit@@YAPAXXZ")  /* 0x020e6bf4 */

/* src/minigames/d_s_mg_flower.cpp */
#pragma comment(linker, "/alternatename:__Z23dScMgFlower_c_classInitv=?dScMgFlower_c_classInit@@YAPAXXZ")  /* 0x0212b7f8 */

/* src/minigames/d_s_mg_hanachan.cpp */
#pragma comment(linker, "/alternatename:__Z25dScMgHanachan_c_classInitv=?dScMgHanachan_c_classInit@@YAPAXXZ")  /* 0x020ede18 */

/* src/minigames/d_s_mg_jump.cpp */
#pragma comment(linker, "/alternatename:__Z21dScMgJump_c_classInitv=?dScMgJump_c_classInit@@YAPAUdScMgJump_c@@XZ")  /* 0x020eeafc */

/* src/minigames/d_s_mg_jump2.cpp */
#pragma comment(linker, "/alternatename:__Z22dScMgJump2_c_classInitv=?dScMgJump2_c_classInit@@YAPAUdScMgJump2_c@@XZ")  /* 0x020efaf0 */

/* src/minigames/d_s_mg_luigi.cpp */
#pragma comment(linker, "/alternatename:__Z22dScMgLuigi_c_classInitv=?dScMgLuigi_c_classInit@@YAPAUdScMgLuigi_c@@XZ")  /* 0x020f3800 */

/* src/minigames/d_s_mg_m_carlo.cpp */
#pragma comment(linker, "/alternatename:__Z23dScMgMCarlo_c_classInitv=?dScMgMCarlo_c_classInit@@YAPAXXZ")  /* 0x020f8e44 */

/* src/minigames/d_s_mg_m_carlo2.cpp */
#pragma comment(linker, "/alternatename:__Z24dScMgMCarlo2_c_classInitv=?dScMgMCarlo2_c_classInit@@YAPAXXZ")  /* 0x020fa6ac */

/* src/minigames/d_s_mg_memory.cpp */
#pragma comment(linker, "/alternatename:__Z23dScMgMemory_c_classInitv=?dScMgMemory_c_classInit@@YAPAUdScMgMemory_c@@XZ")  /* 0x020f5504 */

/* src/minigames/d_s_mg_memory2.cpp */
#pragma comment(linker, "/alternatename:__Z24dScMgMemory2_c_classInitv=?dScMgMemory2_c_classInit@@YAPAXXZ")  /* 0x020f75d4 */

/* src/minigames/d_s_mg_pachinko.cpp */
#pragma comment(linker, "/alternatename:__Z25dScMgPachinko_c_classInitv=?dScMgPachinko_c_classInit@@YAPAXXZ")  /* 0x020ff3ec */

/* src/minigames/d_s_mg_pachinko2.cpp */
#pragma comment(linker, "/alternatename:__Z26dScMgPachinko2_c_classInitv=?dScMgPachinko2_c_classInit@@YAPAUdScMgPachinko2_c@@XZ")  /* 0x02104258 */

/* src/minigames/d_s_mg_panel.cpp */
#pragma comment(linker, "/alternatename:__Z22dScMgPanel_c_classInitv=?dScMgPanel_c_classInit@@YAPAUdScMgPanel_c@@XZ")  /* 0x02107858 */

/* src/minigames/d_s_mg_roulette.cpp */
#pragma comment(linker, "/alternatename:__Z25dScMgRoulette_c_classInitv=?dScMgRoulette_c_classInit@@YAPAXXZ")  /* 0x0210a400 */

/* src/minigames/d_s_mg_roulette_methods.cpp */
#pragma comment(linker, "/alternatename:__Z23RouletteBoard_StateSlowP13RouletteBoard=?RouletteBoard_StateSlow@@YAXPAURouletteBoard@@@Z")  /* 0x02107ea8 */
#pragma comment(linker, "/alternatename:__Z23RouletteBoard_StateSpinP13RouletteBoard=?RouletteBoard_StateSpin@@YAXPAURouletteBoard@@@Z")  /* 0x021082fc */
#pragma comment(linker, "/alternatename:__Z23RouletteRacer_GetPayoutP13RouletteRaceri=?RouletteRacer_GetPayout@@YAHPAURouletteRacer@@H@Z")  /* 0x02108b90 */

/* src/minigames/d_s_mg_slots.cpp */
#pragma comment(linker, "/alternatename:__Z15Slot3_StateDonev=?Slot3_StateDone@@YAXXZ")  /* 0x0210ab90 */
#pragma comment(linker, "/alternatename:__Z19Slot3_StateGameOverPc=?Slot3_StateGameOver@@YAXPAD@Z")  /* 0x0210ab94 */
#pragma comment(linker, "/alternatename:__Z15Slot3_StateWaitv=?Slot3_StateWait@@YAXXZ")  /* 0x0210ac38 */
#pragma comment(linker, "/alternatename:__Z17Slot3_StatePayoutPc=?Slot3_StatePayout@@YAXPAD@Z")  /* 0x0210ac3c */
#pragma comment(linker, "/alternatename:__Z16Slot3_StateJudgePc=?Slot3_StateJudge@@YAXPAD@Z")  /* 0x0210adac */
#pragma comment(linker, "/alternatename:__Z15Slot3_StateSpinPc=?Slot3_StateSpin@@YAXPAD@Z")  /* 0x0210af64 */
#pragma comment(linker, "/alternatename:__Z16Slot3_StateIntroPc=?Slot3_StateIntro@@YAXPAD@Z")  /* 0x0210b1fc */
#pragma comment(linker, "/alternatename:__Z22dScMgSlot3_c_classInitv=?dScMgSlot3_c_classInit@@YAPAXXZ")  /* 0x0210c120 */
#pragma comment(linker, "/alternatename:__Z22dScMgSlot1_c_classInitv=?dScMgSlot1_c_classInit@@YAPAXXZ")  /* 0x0210d6b8 */

/* src/minigames/d_s_mg_smartball.cpp */
#pragma comment(linker, "/alternatename:__Z26dScMgSmartball_c_classInitv=?dScMgSmartball_c_classInit@@YAPAXXZ")  /* 0x02119824 */

/* src/minigames/d_s_mg_snowball.cpp */
#pragma comment(linker, "/alternatename:__Z25dScMgSnowball_c_classInitv=?dScMgSnowball_c_classInit@@YAPAUdScMgSnowball_c@@XZ")  /* 0x0212a520 */

/* src/minigames/d_s_mg_sound.cpp */
#pragma comment(linker, "/alternatename:__Z22dScMgSound_c_classInitv=?dScMgSound_c_classInit@@YAPAXXZ")  /* 0x0211cb70 */

/* src/minigames/d_s_mg_teresa.cpp */
#pragma comment(linker, "/alternatename:__Z23dScMgTeresa_c_classInitv=?dScMgTeresa_c_classInit@@YAPAXXZ")  /* 0x021207a8 */

/* src/minigames/d_s_mg_trampoline.cpp */
#pragma comment(linker, "/alternatename:__Z27dScMgTrampoline_c_classInitv=?dScMgTrampoline_c_classInit@@YAPAUdScMgTrampoline_c@@XZ")  /* 0x02122490 */

/* src/minigames/d_s_mg_trampoline2.cpp */
#pragma comment(linker, "/alternatename:__Z28dScMgTrampoline2_c_classInitv=?dScMgTrampoline2_c_classInit@@YAPAUdScMgTrampoline2_c@@XZ")  /* 0x0212471c */

/* src/minigames/d_s_mini_gm.cpp */
#pragma comment(linker, "/alternatename:__Z21dScMiniGm_c_classInitv=?dScMiniGm_c_classInit@@YAPAUdScMiniGm_c@@XZ")  /* 0x020c21ec */

/* src/minigames/unit020b2c84.cpp */
#pragma comment(linker, "/alternatename:__Z13Banner_UpdateP8MgBanner=?Banner_Update@@YAXPAUMgBanner@@@Z")  /* 0x020b321c */
#pragma comment(linker, "/alternatename:__Z22Banner_DrawLevelNumberP8MgBanner=?Banner_DrawLevelNumber@@YAXPAUMgBanner@@@Z")  /* 0x020b35d8 */
#pragma comment(linker, "/alternatename:__Z16Banner_SetupNonev=?Banner_SetupNone@@YAXXZ")  /* 0x020b3698 */
#pragma comment(linker, "/alternatename:__Z15Banner_DrawSpinP8MgBanner=?Banner_DrawSpin@@YAXPAUMgBanner@@@Z")  /* 0x020b369c */
#pragma comment(linker, "/alternatename:__Z17Banner_UpdateSpinP8MgBanner=?Banner_UpdateSpin@@YAXPAUMgBanner@@@Z")  /* 0x020b37c4 */
#pragma comment(linker, "/alternatename:__Z17Banner_UpdateDropP8MgBanner=?Banner_UpdateDrop@@YAXPAUMgBanner@@@Z")  /* 0x020b3834 */
#pragma comment(linker, "/alternatename:__Z16Banner_SetupDropP8MgBanner=?Banner_SetupDrop@@YAXPAUMgBanner@@@Z")  /* 0x020b3888 */
#pragma comment(linker, "/alternatename:__Z15Banner_DrawWaveP8MgBanner=?Banner_DrawWave@@YAXPAUMgBanner@@@Z")  /* 0x020b38ac */
#pragma comment(linker, "/alternatename:__Z17Banner_UpdateWaveP8MgBanner=?Banner_UpdateWave@@YAXPAUMgBanner@@@Z")  /* 0x020b3978 */
#pragma comment(linker, "/alternatename:__Z18Banner_DrawSqueezeP8MgBanner=?Banner_DrawSqueeze@@YAXPAUMgBanner@@@Z")  /* 0x020b3b38 */
#pragma comment(linker, "/alternatename:__Z17Banner_UpdateFallP8MgBanner=?Banner_UpdateFall@@YAXPAUMgBanner@@@Z")  /* 0x020b3c58 */
#pragma comment(linker, "/alternatename:__Z16Banner_SetupFallP8MgBanner=?Banner_SetupFall@@YAXPAUMgBanner@@@Z")  /* 0x020b3c9c */
#pragma comment(linker, "/alternatename:__Z19Banner_DrawPopSpinAP8MgBanner=?Banner_DrawPopSpinA@@YAXPAUMgBanner@@@Z")  /* 0x020b3cb8 */
#pragma comment(linker, "/alternatename:__Z19Banner_DrawPopSpinBP8MgBanner=?Banner_DrawPopSpinB@@YAXPAUMgBanner@@@Z")  /* 0x020b3e9c */
#pragma comment(linker, "/alternatename:__Z20Banner_UpdatePopSpinP8MgBanner=?Banner_UpdatePopSpin@@YAXPAUMgBanner@@@Z")  /* 0x020b4080 */
#pragma comment(linker, "/alternatename:__Z19Banner_SetupPopSpinP8MgBanner=?Banner_SetupPopSpin@@YAXPAUMgBanner@@@Z")  /* 0x020b40ac */
#pragma comment(linker, "/alternatename:__Z18Banner_DrawPressedP8MgBanner=?Banner_DrawPressed@@YAXPAUMgBanner@@@Z")  /* 0x020b410c */
#pragma comment(linker, "/alternatename:__Z20Banner_UpdatePressedP8MgBanner=?Banner_UpdatePressed@@YAXPAUMgBanner@@@Z")  /* 0x020b4214 */
#pragma comment(linker, "/alternatename:__Z16Banner_SetupZoomP8MgBanner=?Banner_SetupZoom@@YAXPAUMgBanner@@@Z")  /* 0x020b433c */
#pragma comment(linker, "/alternatename:__Z16Banner_DrawZoomAP8MgBanner=?Banner_DrawZoomA@@YAXPAUMgBanner@@@Z")  /* 0x020b4360 */
#pragma comment(linker, "/alternatename:__Z16Banner_DrawZoomBP8MgBanner=?Banner_DrawZoomB@@YAXPAUMgBanner@@@Z")  /* 0x020b45c0 */
#pragma comment(linker, "/alternatename:__Z17Banner_UpdateZoomP8MgBanner=?Banner_UpdateZoom@@YAXPAUMgBanner@@@Z")  /* 0x020b4820 */
#pragma comment(linker, "/alternatename:__Z21Banner_SetupZoomBurstP8MgBanner=?Banner_SetupZoomBurst@@YAXPAUMgBanner@@@Z")  /* 0x020b484c */
#pragma comment(linker, "/alternatename:__Z27Banner_UpdateSlideFromRightP8MgBanner=?Banner_UpdateSlideFromRight@@YAXPAUMgBanner@@@Z")  /* 0x020b49b8 */
#pragma comment(linker, "/alternatename:__Z26Banner_SetupSlideFromRightP8MgBanner=?Banner_SetupSlideFromRight@@YAXPAUMgBanner@@@Z")  /* 0x020b49e4 */
#pragma comment(linker, "/alternatename:__Z26Banner_UpdateSlideFromLeftP8MgBanner=?Banner_UpdateSlideFromLeft@@YAXPAUMgBanner@@@Z")  /* 0x020b49f0 */
#pragma comment(linker, "/alternatename:__Z25Banner_SetupSlideFromLeftP8MgBanner=?Banner_SetupSlideFromLeft@@YAXPAUMgBanner@@@Z")  /* 0x020b4a1c */
#pragma comment(linker, "/alternatename:__Z28Banner_UpdateSlideFromBottomP8MgBanner=?Banner_UpdateSlideFromBottom@@YAXPAUMgBanner@@@Z")  /* 0x020b4a28 */
#pragma comment(linker, "/alternatename:__Z27Banner_SetupSlideFromBottomP8MgBanner=?Banner_SetupSlideFromBottom@@YAXPAUMgBanner@@@Z")  /* 0x020b4a40 */
#pragma comment(linker, "/alternatename:__Z25Banner_UpdateSlideFromTopP8MgBanner=?Banner_UpdateSlideFromTop@@YAXPAUMgBanner@@@Z")  /* 0x020b4a4c */
#pragma comment(linker, "/alternatename:__Z24Banner_SetupSlideFromTopP8MgBanner=?Banner_SetupSlideFromTop@@YAXPAUMgBanner@@@Z")  /* 0x020b4a64 */

/* src/minigames/unit020b4aa4.cpp */
#pragma comment(linker, "/alternatename:__Z17Coin_FlyToCounterP6Sprite=?Coin_FlyToCounter@@YAXPAUSprite@@@Z")  /* 0x020b4aa4 */
#pragma comment(linker, "/alternatename:__Z19Coin_MoveThenRefundP6Sprite=?Coin_MoveThenRefund@@YAXPAUSprite@@@Z")  /* 0x020b4c30 */
#pragma comment(linker, "/alternatename:__Z14Coin_RiseSpentP6Sprite=?Coin_RiseSpent@@YAXPAUSprite@@@Z")  /* 0x020b4d50 */
#pragma comment(linker, "/alternatename:__Z22Coin_MoveThenRiseSpentP6Sprite=?Coin_MoveThenRiseSpent@@YAXPAUSprite@@@Z")  /* 0x020b4dfc */
#pragma comment(linker, "/alternatename:__Z9Coin_RiseP6Sprite=?Coin_Rise@@YAXPAUSprite@@@Z")  /* 0x020b4f44 */
#pragma comment(linker, "/alternatename:__Z17Coin_MoveThenRiseP6Sprite=?Coin_MoveThenRise@@YAXPAUSprite@@@Z")  /* 0x020b4ff0 */
#pragma comment(linker, "/alternatename:__Z17Coin_MoveThenBankP6Sprite=?Coin_MoveThenBank@@YAXPAUSprite@@@Z")  /* 0x020b51f0 */
#pragma comment(linker, "/alternatename:__Z9Coin_MoveP6Sprite=?Coin_Move@@YAXPAUSprite@@@Z")  /* 0x020b5288 */

/* src/minigames/unit020bfec0.cpp */
#pragma comment(linker, "/alternatename:__Z21Camera_UpdateMatricesP8MgCamera=?Camera_UpdateMatrices@@YAXPAUMgCamera@@@Z")  /* 0x020c0134 */

/* src/runtime/actor/ActorBase_SceneNode.cpp */
#pragma comment(linker, "/alternatename:__Z13func_0203b394PN7fBase_c9SceneNodeE=?func_0203b394@@YAPAUSceneNode@fBase_c@@PAU12@@Z")  /* 0x0203b394 */

/* src/ui/hud/d_cap_icon.cpp */
#pragma comment(linker, "/alternatename:__Z19func_ov001_020ab2e4v=?func_ov001_020ab2e4@@YAXXZ")  /* 0x020ab2e4 */

/* src/ui/menus/unit020c1d78.cpp */
#pragma comment(linker, "/alternatename:__Z19func_ov007_020c1da0i=?func_ov007_020c1da0@@YAHH@Z")  /* 0x020c1da0 */

/* src/ui/menus/unit020c3ce4.cpp */
#pragma comment(linker, "/alternatename:__Z19MenuMem_DefaultFreePv=?MenuMem_DefaultFree@@YAXPAX@Z")  /* 0x020c3e4c */
#pragma comment(linker, "/alternatename:__Z20MenuMem_DefaultAllocj=?MenuMem_DefaultAlloc@@YAPAXI@Z")  /* 0x020c3e64 */

/* src/unnamed/arm9/0200/func_02005000.cpp */
#pragma comment(linker, "/alternatename:__Z13func_02005000P9dCamera_c=?func_02005000@@YAXPAUdCamera_c@@@Z")  /* 0x02005000 */

/* src/unnamed/arm9/0200/func_02005060.cpp */
#pragma comment(linker, "/alternatename:__Z13func_02005060P9dCamera_c=?func_02005060@@YAHPAUdCamera_c@@@Z")  /* 0x02005060 */

/* src/unnamed/arm9/0200/func_02005098.cpp */
#pragma comment(linker, "/alternatename:__Z13func_02005098P9dCamera_c=?func_02005098@@YAHPAUdCamera_c@@@Z")  /* 0x02005098 */

/* src/unnamed/arm9/0200/func_020050dc.cpp */
#pragma comment(linker, "/alternatename:__Z13func_020050dcP9dCamera_c=?func_020050dc@@YAHPAUdCamera_c@@@Z")  /* 0x020050dc */

/* src/unnamed/arm9/0200/func_02005110.cpp */
#pragma comment(linker, "/alternatename:__Z13func_02005110P9dCamera_c=?func_02005110@@YAHPAUdCamera_c@@@Z")  /* 0x02005110 */

/* src/unnamed/arm9/0201/func_02019390.cpp */
#pragma comment(linker, "/alternatename:__Z13func_02019390v=?func_02019390@@YAXXZ")  /* 0x02019390 */

/* src/unnamed/arm9/0201/func_02019404.cpp */
#pragma comment(linker, "/alternatename:__Z13func_02019404v=?func_02019404@@YAXXZ")  /* 0x02019404 */

/* src/unnamed/arm9/0202/func_0202345c.cpp */
#pragma comment(linker, "/alternatename:__Z13func_02023498v=?func_02023498@@YAXXZ")  /* 0x02023498 */

/* src/unnamed/ov007/func_ov007_020b6134.cpp */
#pragma comment(linker, "/alternatename:__Z19func_ov007_020b6134i=?func_ov007_020b6134@@YAXH@Z")  /* 0x020b6134 */
