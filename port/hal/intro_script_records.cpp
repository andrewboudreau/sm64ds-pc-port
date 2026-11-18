/* THE OPENING'S SCRIPT ARGUMENT RECORDS, hosted with REAL HOST POINTERS.
 *
 * Run lvled, lane intro-cutscene. Same job and same shape as table 2 of
 * hal/ptr_tables.cpp (the 39 camera-script records): the ROM's own two-word
 * record, redefined here so the pointer word holds a HOST address instead of
 * a DS one.
 *
 * WHY NOT romdata.py. That list byte-copies, and its header banks what that
 * costs on a kuppa table: sDanceCutsceneTable was removed from it because it held
 * six relocated words, so the byte copy handed _ZN11KuppaScript7ProcessEv six raw DS
 * addresses and it faulted walking them. Each record below has exactly ONE
 * relocated word, at +4, verified against config/arm9/relocs.txt
 * (out/intro-cutscene/reloc_audit.txt). Word 0 is a plain value and is
 * carried through from arm9_dec.bin unchanged.
 *
 * The opening's scripts reach these through arguments the sinit patches in,
 * so the pointer IS dereferenced and a DS address here is a fault, not a
 * cosmetic wrong number.
 */
#include "dsstate_seg.h"

/* ---- TWO THINGS THE SINIT NEEDS THAT ARE NOT RECORDS ----------------------
 *
 * @259: BSS, 12 bytes (0x0209b278..0x0209b284 by the symbol table).
 * It is in the 0x0209xxxx WORKING region, not romdata -- byte-copying it there
 * would be a category error and its span comes out negative anyway, because the
 * next symbol is below it. Nothing in the tree defined it yet; the sinit is the
 * first thing to reference it. Zero-initialised, which is what BSS means.
 * (Its neighbour gKuppaScriptFader, the FaderBrightness object _ZN11KuppaScript7ProcessEv
 * drives, is already hosted elsewhere and is deliberately NOT defined here.)
 *
 * _ZN10FaderColorD1Ev: the sinit hands this to __register_global_object, the ROM's
 * global-destructor registrar, as the destructor for a static FaderColor. A
 * no-op host body used to live here under the claim that the symbol had no
 * matched src TU. That was stale: src/engine/fader/_ZN10FaderColorD1Ev.cpp is
 * matched (vptr store, base-subobject dtor _ZN15FaderBrightnessD2Ev, returns self) and
 * is on slice_intro.txt now, so the registrar stores the ROM's own body. The
 * port never runs global destructors, so nothing observable changes; the
 * stand-in is simply gone (lane shadow-A).
 */
extern "C" {

DSSTATE_BEGIN
unsigned char data_0209b278[12];

extern unsigned char sEndingMarioToPeachFocusPoints[];
extern unsigned char sTHIWigglerCreditsSplineFocusPoints[];
extern unsigned char sEndingMarioToPeachPosPoints[];
extern unsigned char sTHIWigglerCreditsSplinePositionsPoints[];
extern unsigned char sCCMOutsideCreditsSplineFocusPoints[];
extern unsigned char sBBHCreditsSplinePositionsPoints[];
extern unsigned char sCCMOutsideCreditsSplinePositionsPoints[];
extern unsigned char sSLCreditsSplineFocusPoints[];
extern unsigned char sSLCreditsSplinePositionsPoints[];
extern unsigned char sWDWCreditsSplineFocusPoints[];
extern unsigned char sWDWCreditsSplinePositionsPoints[];
extern unsigned char sTTCCreditsSplineFocusPoints[];
extern unsigned char sTTCCreditsSplinePositionsPoints[];
extern unsigned char sRRCreditsSplineFocusPoints[];
extern unsigned char sRRCreditsSplinePositionsPoints[];
extern unsigned char sEndingLookAtSkyFocusPoints[];
extern unsigned char sBBHCreditsSplineFocusPoints[];
extern unsigned char sDDDSubCreditsSplineFocusPoints[];
extern unsigned char sJRBCreditsSplinePositionsPoints[];
extern unsigned char sCCMSlideCreditsSplineFocusPoints[];
extern unsigned char sDDDSubCreditsSplinePositionsPoints[];
extern unsigned char sCCMSlideCreditsSplinePositionsPoints[];
extern unsigned char sJRBCreditsSplineFocusPoints[];
extern unsigned char sBoBCreditsSplineFocusPoints[];
extern unsigned char sSACreditsSplineFocusPoints[];
extern unsigned char sSACreditsSplinePositionsPoints[];
extern unsigned char sCotMCCreditsSplineFocusPoints[];
extern unsigned char sBoBCreditsSplinePositionsPoints[];
extern unsigned char sEndingLookUpAtCastlePoints[];
extern unsigned char sWFCreditsSplineFocusPoints[];
extern unsigned char sCotMCCreditsSplinePositionsPoints[];
extern unsigned char sWFCreditsSplinePositionsPoints[];
extern unsigned char sVolcanoCreditsSplineFocusPoints[];
extern unsigned char sVolcanoCreditsSplinePositionsPoints[];
extern unsigned char sSSLCreditsSplineFocusPoints[];
extern unsigned char sSSLCreditsSplinePositionsPoints[];
extern unsigned char sTTMCreditsSplineFocusPoints[];
extern unsigned char sTTMCreditsSplinePositionsPoints[];
extern unsigned char gEndWavingFocusPoints[];
extern unsigned char gEndWavingPosPoints[];
extern unsigned char sHMCCreditsSplineFocusPoints[];
extern unsigned char sHMCCreditsSplinePositionsPoints[];
extern unsigned char sDDDCreditsSplineFocusPoints[];
extern unsigned char sDDDCreditsSplinePositionsPoints[];
extern unsigned char sTHIHugeCreditsSplineFocusPoints[];
extern unsigned char sTHIHugeCreditsSplinePositionsPoints[];
extern unsigned char sEndingFlyToWindowFocusPoints[];
extern unsigned char sEndingFlyToWindowPosPoints[];
extern unsigned char sEndingPeachDescentCamPosPoints[];
extern unsigned char sIntroStartToPipeFocusPoints[];
extern unsigned char sIntroStartToPipePositionPoints[];
extern unsigned char sIntroPipeToRoofFocusPoints[];
extern unsigned char sIntroPipeToRoofPositionPoints[];
extern unsigned char gIntroLakituStartToPipeOffsetFromCameraPoints[];
extern unsigned char gIntroLakituStartToPipeFocusPoints[];

DSSTATE_END

}  /* extern "C" */

/* catchup1 round 3: src/KuppaScript.cpp defines these CutsceneSpline records itself, with host pointers, so the copies that stood here are gone. The C name reaches that storage: */
#pragma comment(linker, "/alternatename:_sSACreditsSplineFocus=?sSACreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_gIntroLakituStartToPipeFocus=?gIntroLakituStartToPipeFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sHMCCreditsSplineFocus=?sHMCCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sRRCreditsSplineFocus=?sRRCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_gIntroLakituStartToPipeOffsetFromCamera=?gIntroLakituStartToPipeOffsetFromCamera@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sVolcanoCreditsSplineFocus=?sVolcanoCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sTHIWigglerCreditsSplinePositions=?sTHIWigglerCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sBBHCreditsSplineFocus=?sBBHCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sCCMSlideCreditsSplinePositions=?sCCMSlideCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sJRBCreditsSplineFocus=?sJRBCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sTTMCreditsSplinePositions=?sTTMCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sSSLCreditsSplineFocus=?sSSLCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sSSLCreditsSplinePositions=?sSSLCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sWDWCreditsSplineFocus=?sWDWCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sEndingFlyToWindowPos=?sEndingFlyToWindowPos@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sTTMCreditsSplineFocus=?sTTMCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sTHIWigglerCreditsSplineFocus=?sTHIWigglerCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sTHIHugeCreditsSplineFocus=?sTHIHugeCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sSACreditsSplinePositions=?sSACreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sTTCCreditsSplinePositions=?sTTCCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sIntroPipeToRoofFocus=?sIntroPipeToRoofFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sEndingFlyToWindowFocus=?sEndingFlyToWindowFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sWFCreditsSplinePositions=?sWFCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sHMCCreditsSplinePositions=?sHMCCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sDDDSubCreditsSplinePositions=?sDDDSubCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sCCMOutsideCreditsSplineFocus=?sCCMOutsideCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sEndingLookAtSkyFocus=?sEndingLookAtSkyFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sVolcanoCreditsSplinePositions=?sVolcanoCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_gEndWavingPos=?gEndWavingPos@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sEndingPeachDescentCamPos=?sEndingPeachDescentCamPos@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sSLCreditsSplineFocus=?sSLCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sTHIHugeCreditsSplinePositions=?sTHIHugeCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sRRCreditsSplinePositions=?sRRCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sBoBCreditsSplineFocus=?sBoBCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sCotMCCreditsSplineFocus=?sCotMCCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sSLCreditsSplinePositions=?sSLCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sBBHCreditsSplinePositions=?sBBHCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sIntroStartToPipeFocus=?sIntroStartToPipeFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sJRBCreditsSplinePositions=?sJRBCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sWDWCreditsSplinePositions=?sWDWCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sEndingLookUpAtCastle=?sEndingLookUpAtCastle@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sIntroStartToPipePosition=?sIntroStartToPipePosition@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sIntroPipeToRoofPosition=?sIntroPipeToRoofPosition@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sDDDCreditsSplinePositions=?sDDDCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sDDDSubCreditsSplineFocus=?sDDDSubCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sCotMCCreditsSplinePositions=?sCotMCCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sBoBCreditsSplinePositions=?sBoBCreditsSplinePositions@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sTTCCreditsSplineFocus=?sTTCCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sEndingMarioToPeachPos=?sEndingMarioToPeachPos@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_gEndWavingFocus=?gEndWavingFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sWFCreditsSplineFocus=?sWFCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sDDDCreditsSplineFocus=?sDDDCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sEndingMarioToPeachFocus=?sEndingMarioToPeachFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sCCMSlideCreditsSplineFocus=?sCCMSlideCreditsSplineFocus@@3UCutsceneSpline@@A")
#pragma comment(linker, "/alternatename:_sCCMOutsideCreditsSplinePositions=?sCCMOutsideCreditsSplinePositions@@3UCutsceneSpline@@A")
