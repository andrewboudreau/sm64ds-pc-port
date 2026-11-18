// ---------------------------------------------------------------------------
// boot_globals.cpp -- the DS globals the ROM's boot chain reads, hosted at
// their ROM sizes and with their ROM contents.
//
// WHY THIS FILE EXISTS. port/slice_gate214.txt seats 100-odd matched
// translation units off the ROM's own boot chain (see hal/boot_os.cpp), and
// those bodies address globals no host file had ever needed: the OS tick and
// alarm words, the IRQ handler table, the assert strings MSL prints, the
// build-time string _Z13func_0203ad84v copies. Every one below was an unresolved
// external in the first link of that slice.
//
// TWO RULES, and they are the ones this port keeps re-learning:
//
//   SIZED BY ROM SPAN, NOT BY THE FIELDS A CALLER TOUCHES. Each array's length
//   is the distance from its own address to the NEXT symbol in
//   config/arm9/symbols.txt. Sizing a hosted global to the widest field some
//   caller happens to write is how the port ends up with an object that a
//   later caller writes past -- OSi_ThreadInfo is exactly that bug today, and
//   it is why hal/boot_os.cpp refuses the ROM's thread-system init.
//
//   .data KEEPS ITS ROM CONTENTS. Anything below 0x0209b000 is initialised
//   data, and its bytes are read straight out of extracted/arm9_dec.bin at
//   its base of 0x02004000 by port/tools/romdata.py (the player's own dump;
//   see the .data section below) -- not invented, not zeroed. Zeroing an
//   initialised global is a silent behaviour change: data_0208ee4c ships as
//   -1 ("no overlay resident"), and zero there means "overlay 0 is resident".
//
// NO POINTER-BEARING GLOBAL IS HOSTED WITH ITS RAW ROM WORD. config/arm9/
// relocs.txt says which words in .data are relocated, and a relocated word is
// a DS ADDRESS that means nothing in this process. Two of the globals the
// slice wanted carry one:
//   data_0209a03c  a function pointer to 0x02057e30, which func_0201a490
//                  CALLS. Hosting the raw word would call into nothing, so
//                  func_0201a490 is not seated at all -- see hal/boot_os.cpp.
//   fsi_digest_key_ptr  a data pointer to fsi_def_digest_key. That one is hosted below
//                  and the pointer is bound to the HOST object, which is the
//                  same technique hal/ptr_tables.cpp uses for the ROM's
//                  function tables.
//
// The .dsstate bracket is not decoration: dsstate_guard fails the build if a
// hosted DS global lands outside the section the save state captures.
// ---------------------------------------------------------------------------

#include "dsstate_seg.h"

DSSTATE_BEGIN
extern "C" {

// ---- .data: the ROM's own bytes, from the player's ROM --------------------
// The initialised arm9 .data the boot chain reads (data_020867bc .. the overlay
// digest and fsi_def_digest_key, 23 objects) is emitted by port/tools/romdata.py
// into romdata.c from extracted/arm9_dec.bin, at these same names, the same
// delta-to-next-symbol spans and inside .dsstate; a ROM-clean build fills it
// from romdata.bin at the top of main(), before the boot chain runs. This file
// used to carry those bytes as literals; the repo carries none.
extern unsigned char fsi_def_digest_key[64];

// fsi_digest_key_ptr is a POINTER, and the ROM word in it is the DS address of
// fsi_def_digest_key. Bound to the host object instead, so the one reader
// (FSi_CompareDigest, on the overlay-load path) follows it to the same 64 bytes
// the ROM would have. Only the address is taken here, so it does not matter
// that a ROM-clean build fills the bytes later, at boot.
void *fsi_digest_key_ptr = (void *)fsi_def_digest_key;

// ---- .bss: zero, at the ROM's span ----------------------------------------
unsigned char data_0209d4dc[4];   /* 0x0209d4dc, .bss, ROM span 4 */
unsigned char data_0209d4e0[4];   /* 0x0209d4e0, .bss, ROM span 4 */
unsigned char data_0209d4f0[4];   /* 0x0209d4f0, .bss, ROM span 4 */
unsigned char data_0209d500[4];   /* 0x0209d500, .bss, ROM span 4 */
unsigned char data_0209d514[4];   /* 0x0209d514, .bss, ROM span 4 */
unsigned char data_020a0db4[4];   /* 0x020a0db4, .bss, ROM span 4 */
unsigned char data_020a63a0[4];   /* 0x020a63a0, .bss, ROM span 4 */
unsigned char data_020a63a4[4];   /* 0x020a63a4, .bss, ROM span 4 */
unsigned char data_020a642c[4];   /* 0x020a642c, .bss, ROM span 4 */
unsigned char data_020a6430[4];   /* 0x020a6430, .bss, ROM span 4 */
unsigned char data_020a6434[4];   /* 0x020a6434, .bss, ROM span 4 */
unsigned char data_020a644c[4];   /* 0x020a644c, .bss, ROM span 4 */
unsigned char data_020a6450[8];   /* 0x020a6450, .bss, ROM span 8 */
/* FifoRecvCallbackTable (0x020a7fc8, .bss, 128): hosted by hal/boot2_ipc.cpp as the PXI
   receive-callback table inside the save-state bracket (run link100 IPC fold). */
unsigned char data_020a80c4[4];   /* 0x020a80c4, .bss, ROM span 4 */
unsigned char data_020a89a4[4];   /* 0x020a89a4, .bss, ROM span 4 */
unsigned char data_020a9ec0[8];   /* 0x020a9ec0, .bss, ROM span 8 */

}  // extern "C"
DSSTATE_END

/* The seven overlay-resident words the ROM's loader bookkeeping and
   GetSceneOverlayID read (moved from hal/boot_hw.cpp at the SCENE fold). */
extern "C" int overlay_2, overlay_3, overlay_4, overlay_5, overlay_6, overlay_7, overlay_75;
int overlay_2, overlay_3, overlay_4, overlay_5, overlay_6, overlay_7, overlay_75;
