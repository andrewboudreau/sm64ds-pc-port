// Host OS arena: the memory region SetupRootHeap carves the root heap from.
//
// On the DS, data_020a0ea4 is the arena id Heap::SetupRootHeap passes and the
// func_02058* family are the NitroSDK arena accessors (get-low, get-high,
// set-low, allocate-from-low) over one table in the shared system block. On
// host the arena itself is one big block reserved at a fixed base, created on
// first use. The hosting targets run the ROM's own accessors over that table;
// the narrow smoke harnesses keep host stand-ins. THE OS ARENA TABLE block
// below says which is which and why.
#include <stdlib.h>
#include "dsstate_seg.h"
#include <stdio.h>

#if defined(_WIN32)
#include <windows.h>
#endif

typedef unsigned int u32;

enum { HOST_ARENA = 32 << 20 };   /* HUB32: 32 MB (was 8): room for 32 Player bodies on the hub; SM64DS_HOST_ARENA_MB overrides */

static char *g_lo, *g_hi;

/* WHY THE ARENA WANTS A FIXED BASE (lane lk7, save states that outlive the run)
   ---------------------------------------------------------------------------
   The in-memory state capture (a host-side caller) memcpy's the whole arena and
   restores it verbatim, which works because the block never moves for the life
   of the process: a pointer captured at save still addresses the same arena
   offset after restore. A save state written to DISK has to survive the process
   ENDING. On the next launch the arena is a fresh allocation, and a plain calloc
   lands wherever the host allocator puts it, which is a different absolute
   address every run. The disk state is full of raw absolute pointers into the
   arena (actor-list heads, model-matrix pointers, intrusive next-links), so it
   only loads correctly if the arena comes up at the SAME base it had when the
   state was written.

   So the arena is reserved at a fixed host address with VirtualAlloc, the same
   technique ntr/io.cpp uses to pin the DS ranges. The base sits above every DS
   range (main RAM 0x02000000..0x02400000, the shared block at 0x027ff000, and
   the MMIO/VRAM ranges up to 0x07000800) and well below 2GB so the truncating
   32-bit round-trips the loaders do stay exact. If the reservation fails at
   boot (something else holds the range), the arena falls back to calloc and the
   persist layer refuses disk load/save for the run; the in-memory slot still
   works exactly as before. port_arena_is_fixed() reports which path we took. */
#if defined(_WIN32)
enum : size_t { ARENA_FIXED_BASE = 0x30000000u };  /* 768 MB, above the DS ranges */
static int g_fixed_base;                            /* 1 = VirtualAlloc'd at ARENA_FIXED_BASE */
#endif

/* THE ARENA HAS TO BE DETERMINISTIC. It was a plain malloc, which left two
   things different on every run: the contents (whatever the host allocator
   last had there) and the absolute base. Both reach the game. Anything that
   reads a field before writing it picks up the old contents, and the carve in
   OS_AllocFromArenaLo rounds `lo` up against the ABSOLUTE address, so a base that
   is not aligned to at least the largest alignment the game asks for shifts
   every later allocation by a different amount.
   The symptom was a walk_window selftest that produced five different final
   frames in six identical runs -- Mario ended up somewhere else each time,
   which makes any before/after comparison of the renderer meaningless.
   calloc zeroes it (the DS arena is cleared main RAM) and the 64K alignment
   makes every carve offset identical from one run to the next. */
enum { ARENA_ALIGN = 0x10000 };

/* The aligned base of the arena, i.e. g_lo as it stood the instant arena_init
   ran, before any carve advanced it. g_lo itself is the live carve cursor and
   moves; g_base does not. The state-capture code needs the
   fixed base and the fixed ceiling to bound the region it snapshots, plus the
   live cursor as a value it captures and restores like any other allocator
   state. */
static char *g_base;

static void arena_init(void)
{
    if (!g_lo) {
        /* soaks and tools can ask for more than the DS ever had */
        const char *env = getenv("SM64DS_HOST_ARENA_MB");
        size_t mb = env ? (size_t)atoi(env) : 0;
        size_t size = mb ? mb << 20 : (size_t)HOST_ARENA;

#if defined(_WIN32)
        /* Try the fixed base first. ARENA_FIXED_BASE is 64K-aligned, so the
           reservation starts exactly there and no post-alignment is needed --
           the base is deterministic across runs, which is what the disk save
           state leans on. VirtualAlloc zeroes committed pages, matching the
           calloc'd DS arena. */
        void *fixed = VirtualAlloc((void *)ARENA_FIXED_BASE, size,
                                   MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (fixed) {
            g_lo = (char *)fixed;
            g_hi = g_lo + size;
            g_base = g_lo;
            g_fixed_base = 1;
            return;
        }
#endif
        char *base = (char *)calloc(size + ARENA_ALIGN, 1);
        if (!base) return;
        g_lo = (char *)(((size_t)base + (ARENA_ALIGN - 1))
                        & ~(size_t)(ARENA_ALIGN - 1));
        g_hi = g_lo + size;
        g_base = g_lo;
    }
}

#ifdef SM64DS_OS_ARENA_ROM
/* The ROM's OS arena table in the shared system block (NitroSDK OSArenaInfo,
   OS_ARENA_MAX = 9 ids): lo[id] at 0x027ffda0, hi[id] at 0x027ffdc4. The
   addresses are the ROM's own: src/func_02058ea0.c, src/func_02058eb4.c and
   src/func_02058d58.c spell them, and the cartridge's bodies at 0x02058ea0 /
   0x02058eb4 / 0x02058d58 compute 0x027ff000 + 0xda0 / 0xdc4 + id * 4. */
typedef unsigned int os_word;
static os_word *const os_arena_lo_word = (os_word *)(size_t)0x027ffda0u;
static os_word *const os_arena_hi_word = (os_word *)(size_t)0x027ffdc4u;
enum { OS_ARENA_MAIN = 0 };
/* 1 once port_os_arena_seed has run. Host session state, not DS state: it only
   ever goes 0 -> 1, once per process, at OS_InitArena's point. */
static int g_os_seeded;
#endif

extern "C" {
/* Read-only arena window accessors for the save-state layer. base and end are
   fixed for the process lifetime; cursor is the live low-water carve pointer.
   All three return 0 before the first carve forces arena_init, which is why
   the save-state code calls port_arena_base() (it runs arena_init) before
   reading any of them.

   WHERE THE CURSOR LIVES. On the narrow harnesses it is g_lo, which the host
   stand-ins below move. On the hosting targets (SM64DS_OS_ARENA_ROM) the ROM's
   own OS_SetArenaLo / OS_AllocFromArenaLo move the OS_ARENA_MAIN lo word of the
   table instead, so once the table is seeded the cursor IS that word: the
   save state captures and restores it exactly as it captured g_lo before.
   After boot both read the arena end, because Heap::SetupRootHeap carves the
   whole arena into the root heap. */
void *port_arena_base(void)   { arena_init(); return g_base; }
void *port_arena_end(void)    { arena_init(); return g_hi; }

/* 1 if the arena came up at its fixed host base (VirtualAlloc succeeded), 0 if
   it fell back to calloc. The disk save state (hal/lk7_persist.cpp) is only
   loadable and savable when this is 1, because a disk state carries absolute
   pointers into the arena and only a fixed base makes them valid across a
   restart. On non-Windows the arena is always calloc'd, so this is 0 and disk
   states are off (the shipped port is Windows-only). */
int port_arena_is_fixed(void)
{
    arena_init();
#if defined(_WIN32)
    return g_fixed_base;
#else
    return 0;
#endif
}

/* THE INSTALL VERDICT, and why it lives in this file.

   WHAT THE ASSET FOLDER TURNED OUT TO BE is invisible in a crash dump, and the
   dump is the only thing a remote report gives us. The 2026-08-26 burst spent
   real human time on exactly this question -- six dumps read side by side,
   comparing SM64DS_ASSET_ROOT in the env block against the version in the
   launcher metadata -- and the answer that comparison produced was WRONG,
   because a folder's NAME is not its contents. The facts that actually settle
   it are known at BOOT (hal/romdata_loader.cpp knows which blob it loaded and
   whether the exe was built for it; hal/lk7_persist.cpp knows whether a disk
   state was loaded or refused and why), and they are gone by the time anything
   faults. So they are written here, once, into a plain host .bss buffer, and
   tests/fault_probe.h copies it verbatim into the dump.

   Note what this block does and does not settle. `romdata ... MATCH` says the
   game data in the folder is the data this exe was built for, whatever the
   folder is called -- it would have ended that argument in an hour. It says
   nothing about whether a restored world is SOUND; a save state can carry every
   matching stamp and still restore a broken world (see hal/lk7_persist.cpp).

   THIS BUFFER IS HOST SESSION STATE AND MUST NOT BE IN .dsstate. It describes
   the process, not the game: rolling it back on an F9 would make a dump
   describe the install as it was at save time. It sits outside the
   DSSTATE_BEGIN/END bracket below deliberately, and its name carries no DS
   address, so tools/dsstate_guard.py correctly ignores it.

   It lives in os_arena.cpp because that is the one hal file every target that
   can write a dump already links -- lk7_persist needs port_arena_base, and the
   ROM-CLEAN window targets link both. tests/fault_probe.h still carries a weak
   default (the /alternatename trick it already uses for port_build_gittip) so
   the narrow probes that include it without this file keep linking. */
char port_install_verdict[512];

/* WHICH GAME DATA THIS PROCESS BOOTED FROM, for the same reason and with the
   same placement rule. hal/romdata_loader.cpp fills these once, at boot, from
   the blob it verified; hal/lk7_persist.cpp stamps them into a disk save state
   and compares them on load, because a save state is a snapshot of a world that
   this data produced and no EXE-side field can see the difference. The storage
   is here rather than in the loader because the loader is compiled only into
   ROM-CLEAN targets while lk7_persist is compiled into more than that; empty
   and zero is the correct reading for a build that loads no blob.

   Host session state, deliberately outside the .dsstate bracket: an F9 must not
   roll back which folder the process booted from. */
char               port_romdata_sha[65];
unsigned long long port_romdata_bytes;
char               port_asset_root_seen[512];

/* Append one line, bounded, always NUL-terminated. Safe to call before or after
   anything. A line that does not fit is TRUNCATED, not dropped, and a buffer
   with fewer than three bytes left loses the CRLF separator too, so the tail
   would run on -- at 512 bytes with two lines of roughly 200 that cannot
   happen today, and it is written down rather than prevented because this is
   diagnostic text in a process that may be about to die, not a log. */
void port_install_verdict_add(const char *line)
{
    unsigned n = 0, i = 0;
    if (!line) return;
    while (n < sizeof port_install_verdict - 1 && port_install_verdict[n])
        ++n;
    /* CRLF: the dump this ends up in (tests/fault_probe.h) is a Windows text
       file the player opens in Notepad, and every other line in it is CRLF. */
    if (n && n + 2 < sizeof port_install_verdict - 1) {
        port_install_verdict[n++] = '\r';
        port_install_verdict[n++] = '\n';
    }
    while (line[i] && n < sizeof port_install_verdict - 1)
        port_install_verdict[n++] = line[i++];
    port_install_verdict[n] = 0;
}
}

extern "C" {
// The arena id Heap::SetupRootHeap passes to every accessor below
// (ROOT_HEAP_ARENA_ID, bound here by /alternatename in hal/cxx_aliases.cpp).
// Heap::InitializeRootHeap stores 0 into it, OS_ARENA_MAIN. The narrow
// harnesses' stand-ins ignore it; the ROM's accessors index the table by it.
DSSTATE_BEGIN
char data_020a0ea4[4];
DSSTATE_END

/* THE OS ARENA TABLE (run linkfull wave 23, lane OSARENA1; LINK15 batch W23-4)
   -------------------------------------------------------------------------
   The five PORT_HOST_ABI rows this block used to carry all said the DS
   arena words at 0x027ffda0 / 0x027ffdc4 were UNMAPPED. They have not been
   since 08-26: ntr/io.cpp maps the shared system block 0x027ff000..0x027fffff
   as a required region, so the ROM's accessor bodies can run on the table as
   they are. What the port never did was WRITE the table, which on the DS is
   OS_InitArena's job (OS_InitArena, the first arm of OS_Init). This
   file seeds it, from the port's own arena block, and hal/boot_os.cpp calls
   the seed at that arm's point in port_boot_rom_pre_main.

   SM64DS_OS_ARENA_ROM selects the ROM side. port/CMakeLists.txt's W23-4 block
   sets it on the three hosting targets' compile of this file ONLY (the shared
   object library and walk_window, walk_window_hires, smoke_player), the same
   targets port/slice_w23_arena.txt and port/slice_w27_smalls1.txt put the ROM
   bodies on:

     OS_GetArenaHi  OS_GetArenaHi        src/, slice_w23_arena
     OS_SetArenaLo  OS_SetArenaLo        src/, slice_w23_arena
     OS_AllocFromArenaLo  OS_AllocFromArenaLo  src/, slice_w23_arena
     OS_GetArenaLo  OS_GetArenaLo        src/, slice_w27_smalls1 (run linkfull
                                         wave 27, lane SMALLS1)
     OS_InitAlloc  OS_InitAlloc         src/, slice_w27_smalls1 (the same lane):
                                         the root heap starts 0x60 in, see below

   THE SEED is what the ROM's OS_InitArena writes for OS_ARENA_MAIN, with the
   port's block in place of the cartridge's linker-defined bounds:
       hi[OS_ARENA_MAIN] = g_hi   (the end of the arena block)
       lo[OS_ARENA_MAIN] = g_lo   (its 64K-aligned base, nothing carved yet)
   on the default 8 MB fixed-base arena that is lo 0x30000000, hi 0x30800000.
   Every other id stays 0. OS_InitArena also fills MAINEX (with 0), ITCM,
   DTCM, SHARED and WRAM_MAIN from DS ranges this port does not map, and no
   linked ROM code reads any id but 0 (the only linked caller of these five
   is Heap::SetupRootHeap, with id 0); a 0 lo makes OS_AllocFromArenaLo
   refuse, which is the honest answer for an arena that does not exist here.

   WHERE THE SEED RUNS: AT OS_InitArena's OWN POINT IN THE ORDER. Two things
   have to be true when it runs: the arena block exists, and ntr holds the
   shared block. hal/boot_os.cpp's port_boot_rom_pre_main calls it at the
   OS_InitArena line, the first arm of OS_Init, which walk_window
   reaches only after ntr::io_init has succeeded (retry included) and just
   before Heap::InitializeRootHeap; smoke_player's main, which has no pre-main
   span, calls it between io_init and Heap::SetupRootHeap. Both points precede
   the first read of the table, the ROM's own OS_GetArenaLo at the top of
   Heap::SetupRootHeap. It is NOT run from arena_init or a static
   initialiser: arena_init is also reachable from host callers whose timing is
   not tied to io_init (port_arena_base from the GPU device and the save-state
   layer), and a static initialiser runs after ntr's early TLS claim but BEFORE
   io_init's retry, so on a launch where the early claim lost the shared block
   and the retry won it (the rescue ntr/io.cpp exists for) it would find no
   table to write. Until run linkfull wave 27 (lane SMALLS1) the seed ran
   lazily inside a host OS_GetArenaLo at the first read; the values it writes
   are unchanged. */
#ifdef SM64DS_OS_ARENA_ROM
void port_os_arena_seed(void)
{
    if (g_os_seeded)
        return;
    g_os_seeded = 1;
    arena_init();
    if (!g_lo)
        return;   /* no arena block: leave the table 0, the allocator refuses */
    /* OS_InitArena's order: hi first, then lo (OS_SetArenaHi, OS_SetArenaLo) */
    os_arena_hi_word[OS_ARENA_MAIN] = (os_word)(size_t)g_hi;
    os_arena_lo_word[OS_ARENA_MAIN] = (os_word)(size_t)g_lo;
    fprintf(stderr, "[arena] OS arena %d seeded: lo %08x hi %08x\n",
            (int)OS_ARENA_MAIN, os_arena_lo_word[OS_ARENA_MAIN],
            os_arena_hi_word[OS_ARENA_MAIN]);
}

/* OS_InitAlloc RUNS AS THE ROM'S (run linkfull wave 27, lane SMALLS1).
   src/func_02059040.c (arm9 0x02059040, 0xbc) is NitroSDK OS_InitAlloc.
   Heap::SetupRootHeap calls it as (id, lo, hi, 4); it stores lo into
   OSiHeapInfo[id] (OSiHeapInfo, hosted in hal/scene_boot.cpp), builds the
   heap-info header plus four 0xc-byte descriptors at lo, and returns
   round32(lo + 0x14 + 4 * 0xc) = lo + 0x60. So the root heap starts 0x60
   bytes into the arena and is 0x60 bytes smaller, as on the cartridge (where
   lo is 0x0214eaa0), and every allocation carved from the head of the root
   heap sits 0x60 higher than under the old stand-in, which returned lo
   rounded to 4, lo itself. OSARENA1 (wave 23) held the row for exactly that
   move; the coordinator's ruling (09-23, corrected by REVOSARENA1) re-filed
   it as a ROM-faithful layout move. What moved and what it costs is in the
   lane's commit: a disk save state is refused by build (gittip) before any
   layout is looked at, and a world probe tool's fixed
   camera address moved by the same 0x60. */
#else
/* THE NARROW HARNESSES' STAND-INS. Every smoke_* target that links
   Heap::SetupRootHeap links it without port/slice_w23_arena.txt, and two of
   them (smoke_roots, smoke_fs) link no ntr at all, so there is no shared
   block under the table: these five host bodies are the only definitions
   those links can have. They carve the same arena the same way they always
   did. */
int OS_GetArenaLo(void *) { arena_init(); return (int)(size_t)g_lo; }   /* arena lo */
int OS_GetArenaHi(void *) { arena_init(); return (int)(size_t)g_hi; }   /* arena hi */

/* the round-up SetupRootHeap's OS_InitAlloc call returns here; the hosting
   targets run the ROM body instead (src/func_02059040.c), which starts the
   root heap 0x60 in */
int OS_InitAlloc(void *, int lo, int hi, int align)
{
    (void)hi;
    return (lo + align - 1) & ~(align - 1);
}

void OS_SetArenaLo(void *, int newLo) { g_lo = (char *)(size_t)newLo; }  /* set lo */

/* carve `size` bytes aligned `align` from the low side */
void *OS_AllocFromArenaLo(void *, int size, int align)
{
    arena_init();
    char *p = (char *)(((size_t)g_lo + align - 1) & ~(size_t)(align - 1));
    if (p + size > g_hi)
        return 0;
    g_lo = p + size;
    return p;
}
#endif
}

/* ===================================================================== *
 * THE GLOBAL DEALLOCATOR SEAM, AND WHY IT IS DECIDED IN THIS FILE
 * ===================================================================== *
 *
 * This image contains TWO programs with two separate memory systems, and
 * C++ gives a program exactly ONE global operator new / operator delete
 * pair. That is the whole defect.
 *
 *   THE CARTRIDGE'S SYSTEM. On the DS the pair is matched and both halves
 *   are the game heap: _Znwm at 0x0203cbe4 tail-calls _Z13func_0203cc0cj, which
 *   is Heap::Allocate(data_020a0ea0, size), and _Z13func_0203cbf0Pv at 0x0203cbf0 is
 *   Memory::defaultHeapPtr->_Deallocate(ptr). Both are matched and linked
 *   here, and that heap is carved out of THIS FILE'S arena.
 *
 *   THE HOST'S SYSTEM. The ntr graphics emulation, the sound host, the
 *   editor channel and every std::vector / std::string / std::function
 *   inside them allocate from the CRT. There is no DS equivalent of any of
 *   it; it does not exist on a cartridge.
 *
 * WHAT WENT WRONG. src/_ZdlPv.cpp is a real `operator delete(void*)`
 * definition, so MSVC binds it as the program-wide global deallocator --
 * correctly, because ROM code needs it: a census of the linked image finds
 * FIVE ROM call sites reaching it (ModelD1Ev, BlendModelAnimD1Ev,
 * CleanCommonModelDataArr, _ZdaPv, __cxa_vec_delete) plus every ROM
 * class whose MSVC scalar deleting destructor runs. But src/_Znwj.cpp
 * cannot match it on the allocating side: mwccarm 2004/b56 rejects
 * `void* operator new(u32)` outright, so _Znwm stays a hand-spelled flat
 * name and the CRT's operator new wins. Host allocations were therefore
 * malloc'd by Windows and handed back to the cartridge's allocator, which
 * has never seen them, and ExpandingHeapAllocator::UnlinkNode faulted
 * walking a node header that was really somebody else's malloc block.
 *
 * WHY THE SIZED FORM IS THE WHOLE FIX. The census says the host reaches
 * the ROM deallocator through exactly ONE edge: LIBCMT's sized-delete
 * shim ??3@YAXPAXI@Z, which every host deallocation funnels through and
 * which does nothing but forward to the unsized global. Replacing that one
 * shim with a router splits the two systems apart and touches nothing
 * else. src/_ZdlPv.cpp keeps its definition, stays byte-matched and stays
 * linked; no decomp file moves; no TU leaves the link.
 *
 * THE TEST IS EXACT, NOT A HEURISTIC. The arena is one contiguous region
 * reserved at a fixed base above every DS range, and the ROM heap is
 * carved from it, so a pointer belongs to the cartridge if and only if it
 * lies inside [g_base, g_hi). Nothing else can be allocated there: the
 * region is reserved and committed to us for the life of the process.
 *
 * DELIBERATELY NO arena_init() HERE. If the arena does not exist yet then
 * the game has not allocated anything yet, so the pointer is necessarily a
 * host one and free() is the right answer. Reading the bounds without
 * forcing the arena up also keeps arena creation at exactly the moment it
 * happened before, which the determinism this file argues for depends on.
 *
 * NOT DONE, AND ON PURPOSE: the allocating side is left alone. ROM code
 * never calls the global operator new in this link (34 call sites, every
 * one host), so routing it to the game heap would buy nothing and would
 * pour host vertex buffers into the cartridge's 8 MB arena, changing the
 * addresses the ROM's own allocator hands out. That would be a divergence
 * from the cartridge, not a step towards it.
 */
#include <new>
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)

/* SINCE FOLD_1 BOTH GLOBAL DEALLOCATORS ARE DECIDED HERE. Main folded the
 * global allocators into src/engine/heap/Heap.cpp and keeps its four
 * operators off MSVC, so the ROM's operator delete(void *) no longer
 * replaces the CRT's on its own. Two things changed with it, and both make
 * the unsized form need the same router as the sized one:
 *   - TARGET's src spells `::operator new(n)` at ten ROM call sites
 *     (d_ext_model, d_camera, d_a_demo, d_a_mugen_bgm, the stage minimap
 *     nodes, smartball), which on the host is the CRT's operator new, so a
 *     ROM delete can now be handed a Windows pointer;
 *   - the D0 faces and this file's sized form still hand cartridge pointers
 *     to ::operator delete.
 * So the unsized global is defined here with the same exact test: a pointer
 * inside the DS arena goes to func_0203cbf0 (Heap.cpp, the ROM's
 * Memory::defaultHeapPtr->_Deallocate at 0x0203cbf0, built on the host by
 * ENG4's Heap.cpp patch), anything else was malloc'd by the CRT and goes to
 * free(). The allocating side is still left alone (the CRT's operator new,
 * new[]); the ROM's flat callers get the game heap through Heap.cpp's
 * host-arm _Znwm and _Znam, as they got it from src/_Znwj.cpp at the pin. */
void func_0203cbf0(void *ptr);

/* Harnesses that link this file without host-src Heap.cpp (smoke_gpu_raster)
   have no game heap to give an arena pointer back to. The row below binds
   func_0203cbf0 to this no-op only when nothing else defines it; such a
   harness never allocates from the game heap, so there is nothing to free. */
void os_arena_no_game_heap(void *) {}
#pragma comment(linker, "/alternatename:?func_0203cbf0@@YAXPAX@Z=?os_arena_no_game_heap@@YAXPAX@Z")

static void heap_origin_note_free(void *ptr, void *ra, size_t sized);

void operator delete(void *ptr) noexcept
{
    if (!ptr)
        return;
    if ((char *)ptr >= g_base && (char *)ptr < g_hi) {
        /* The cartridge allocated it: the ROM's own deallocator. */
        func_0203cbf0(ptr);
        return;
    }
    /* Windows allocated it, through the CRT's operator new. */
    heap_origin_note_free(ptr, _ReturnAddress(), 0);
    free(ptr);
}

/* THE SIZED DELETE MUST STAY A DIFFERENT BODY FROM THE ONE ABOVE. Host code's
   std::string and std::vector free through this one (MSVC's allocator passes
   the size), and the function-entry trace names a body by its address. If the
   two bodies are byte-identical the linker's identical-code folding gives them
   one address, which the trace symtab calls __ZdlPv, the ROM's operator
   delete, and every host string free then shows up in the trace as game code
   deleting something (fold 11: host path and sdat.cpp strings, 164 calls on
   the opening). Passing the size on to the free note keeps the bodies apart,
   as they were through fold 10, so this one keeps its own name. A game-side
   delete of a cartridge block still reaches func_0203cbf0 from either. */
void operator delete(void *ptr, size_t size) noexcept
{
    if (!ptr)
        return;
    if ((char *)ptr >= g_base && (char *)ptr < g_hi) {
        func_0203cbf0(ptr);
        return;
    }
    heap_origin_note_free(ptr, _ReturnAddress(), size);
    free(ptr);
}

/* ===================================================================== *
 * THE GLOBAL ALLOCATOR SEAM (catchup1 Stage D round 4, TRACE4 family L)
 * ===================================================================== *
 *
 * The comment above says ROM code never called the global operator new. That
 * stopped being true when main folded the per-function files: src now spells
 * ::operator new / ::operator new[] at ten sites (d_ext_model, d_camera,
 * d_a_demo, d_a_mugen_bgm, the minimap change nodes, smartball) and plain
 * new-expressions on classes without a class allocator (every model class,
 * TreeNode, StaffRoll, the wipe array), and MSVC binds every one of them to the
 * single global operator new, the CRT's. Those objects were malloc'd: outside
 * the arena, so outside save states and off the game heap's books, where golden
 * 1.0.7 (per-function files calling the flat _Znwm) had them on the game heap.
 *
 * THE TEST, AGAIN EXACT. Every game translation unit is compiled with
 * hal/game_code_seg.h force-included, which puts its code in .text$mng; the
 * linker orders .text$ pieces by suffix, so all game code lies strictly
 * between the two markers below (.text$mnf and .text$mnh). A new whose return
 * address is in that run was called by game code and gets the game heap
 * through Heap.cpp's host _Znwm / _Znam (Memory::defaultHeapPtr), exactly as on
 * the cartridge; every other caller (the renderer, the sound host, std::
 * containers, whose template code code_seg does not move) gets the CRT, as
 * before. The deallocators above already split by address, so each block goes
 * back to whoever made it.
 *
 * Before the game heap exists (a game TU's static initialiser) there is
 * nowhere on the game side to put it, and it goes to the CRT like any host
 * allocation; the address test frees it with free().
 *
 * SM64DS_GAME_NEW_LOG=1 prints the first 64 game-side allocations (caller,
 * size, block, whether it is inside the arena) and a running count every 1000. */
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)

#pragma code_seg(".text$mnf")
extern "C" __declspec(noinline) int os_arena_game_code_lo(void) { return 0x67636c6f; }
#pragma code_seg(".text$mnh")
extern "C" __declspec(noinline) int os_arena_game_code_hi(void) { return 0x67636869; }
#pragma code_seg()

/* Memory::defaultHeapPtr, by its address name: hal/heap_vtable.cpp defines
   data_020a0ea0 and aliases the C++ spelling onto it. Only the three targets
   that build this file with SM64DS_OS_ARENA_ROM (walk_window,
   walk_window_hires, smoke_player) link host-src Heap.cpp and
   hal/heap_vtable.cpp; the smoke_* harnesses that compile this file without
   it have no game heap, so a game-side new there keeps the CRT. (A
   /alternatename fallback did the same job but alternatename_guard rightly
   counts it as defeated on the three real targets.) */
#ifdef SM64DS_OS_ARENA_ROM
extern "C" void *data_020a0ea0;
extern "C" void *_Znwm(size_t size);
extern "C" void *_Znam(size_t size);
#define OS_ARENA_GAME_HEAP() (data_020a0ea0 != 0)
#else
#define OS_ARENA_GAME_HEAP() false
static void *_Znwm(size_t size) { return malloc(size ? size : 1); }
static void *_Znam(size_t size) { return malloc(size ? size : 1); }
#endif

static int game_new_log()
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("SM64DS_GAME_NEW_LOG");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    return on;
}

static inline bool called_from_game(void *ra)
{
    return (char *)ra > (char *)&os_arena_game_code_lo &&
           (char *)ra < (char *)&os_arena_game_code_hi;
}

/* SM64DS_HEAP_ORIGIN_LOG=1: which code made a block that game code later
   frees to the CRT. Every CRT new remembers its caller in a ring (only while
   the variable is set), and a free() of a CRT block whose delete was called
   from game code prints both callers once per allocating site. A game object
   on the CRT heap is a block the cartridge would have put in its own heap. */
static int heap_origin_log()
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("SM64DS_HEAP_ORIGIN_LOG");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    return on;
}

struct HeapOrigin { void *ptr; void *ra; unsigned size; };
static HeapOrigin s_origin[8192];
static volatile long s_origin_n;

static void heap_origin_note_alloc(void *p, void *ra, size_t size)
{
    if (!heap_origin_log())
        return;
    const long i = InterlockedIncrement(&s_origin_n) - 1;
    HeapOrigin &o = s_origin[i & 8191];
    o.ptr = p;
    o.ra = ra;
    o.size = (unsigned)size;
}

static void *crt_new(size_t size, void *ra = 0)
{
    if (size == 0)
        size = 1;
    for (;;) {
        if (void *p = malloc(size)) {
            heap_origin_note_alloc(p, ra, size);
            return p;
        }
        std::new_handler h = std::get_new_handler();
        if (!h)
            throw std::bad_alloc();
        h();
    }
}

static void div_fault_selftest_hal();

static void *game_new(void *ra, size_t size, bool array)
{
    div_fault_selftest_hal();
    void *p = array ? _Znam(size) : _Znwm(size);
    if (game_new_log()) {
        static unsigned long n;
        ++n;
        if (n <= 64)
            printf("[gamenew] #%lu caller=%p size=0x%x %s=%p arena=%d\n", n, ra,
                   (unsigned)size, array ? "new[]" : "new", p,
                   (char *)p >= g_base && (char *)p < g_hi);
        else if (n % 1000 == 0)
            printf("[gamenew] %lu game-side allocations\n", n);
    }
    return p;
}

void *operator new(size_t size)
{
    void *ra = _ReturnAddress();
    if (OS_ARENA_GAME_HEAP() && called_from_game(ra))
        return game_new(ra, size, false);
    return crt_new(size, ra);
}

void *operator new[](size_t size)
{
    void *ra = _ReturnAddress();
    if (OS_ARENA_GAME_HEAP() && called_from_game(ra))
        return game_new(ra, size, true);
    return crt_new(size, ra);
}

/* THE GAME'S OWN ALLOCATION FOR A HOST HELPER THAT STANDS IN FOR ROM CODE.
   __cxa_vec_new (hal/actor_vtables.cpp, the MSL array-new the DS runs as asm)
   is called only by game code, but it is host code, so its ::operator new
   fails the return-address test and the arrays it makes (the player's
   0x260-byte records, model arrays) landed on the CRT heap; the game then
   freed them to free() where the cartridge frees them to its heap
   (one field probe: _Deallocate and func_0203cbf0 0 of golden's 2). A helper
   like that calls this instead: the game heap when it exists, the CRT before. */
extern "C" void *os_arena_game_alloc(size_t size, int array)
{
    if (OS_ARENA_GAME_HEAP())
        return game_new(_ReturnAddress(), size, array != 0);
    return crt_new(size, _ReturnAddress());
}

static void heap_origin_note_free(void *ptr, void *ra, size_t sized)
{
    if (!heap_origin_log() || !called_from_game(ra))
        return;
    const long n = s_origin_n < 8192 ? s_origin_n : 8192;
    void *alloc_ra = 0;
    unsigned size = 0;
    for (long k = 0; k < n; ++k)
        if (s_origin[k].ptr == ptr) { alloc_ra = s_origin[k].ra; size = s_origin[k].size; }
    const uintptr_t base = (uintptr_t)GetModuleHandleA(0);
    fprintf(stderr, "[heaporigin] game code at +0x%08x frees CRT block %p (size 0x%x) made by +0x%08x%s\n",
            (unsigned)((uintptr_t)ra - base), ptr, size,
            alloc_ra ? (unsigned)((uintptr_t)alloc_ra - base) : 0u,
            alloc_ra ? "" : " (not in the ring: allocated before the log or by malloc)");
    if (sized && size && sized != size)
        fprintf(stderr, "[heaporigin]   sized delete says 0x%x\n", (unsigned)sized);
    fflush(stderr);
}

/* ---- INTEGER DIVIDE FAULTS IN GAME CODE GET THE CARTRIDGE'S ANSWER ---------
 *
 * hal/div_fault.h has the why (the ARM helper never faults: x / 0 = x,
 * x % 0 = 0, INT_MIN / -1 = INT_MIN rem 0) and the decoder. This file owns the
 * game-code range, so the scope test and the install live here: the handler
 * acts only on a fault whose EIP is game code by the same test operator new
 * uses, and leaves every other #DE (hal, ntr, the CRT, SDL) to the crash
 * reporter. Installed by a static initialiser so it is in place before the
 * first game frame on every target that links this file.
 *
 * Each site is logged once on stderr: "[div0] +0x<rva> <symbol>+0x<off> ..."
 * (the symbol from the build's .symtab or .map beside the exe, when present).
 * SM64DS_DIV_FAULT=0 turns the handler off (a raw idiv faults as before).
 * SM64DS_DIV_FAULT_SELFTEST=hal divides by zero in this file (outside the
 * game range) at the first game-side allocation, which must crash normally.
 * x86 Windows only: the decoder rewrites a 32-bit CONTEXT. */
#if defined(_WIN32) && defined(_M_IX86)
#include "div_fault.h"

static int div_fault_in_game(uintptr_t eip)
{
    return called_from_game((void *)eip);
}

/* Nearest symbol at or below rva, from <exe>.symtab ("name\taddr", "# base=")
   or the linker map. Only runs on a site's first hit, so it scans the file. */
static void div_fault_symbol(uintptr_t rva, char *out, size_t cap)
{
    snprintf(out, cap, "?");
    char exe[MAX_PATH];
    DWORD n = GetModuleFileNameA(0, exe, sizeof exe);
    if (!n || n >= sizeof exe - 8)
        return;
    char *dot = strrchr(exe, '.');
    if (!dot)
        return;
    const char *exts[2] = { ".symtab", ".map" };
    for (int e = 0; e < 2; ++e) {
        strcpy(dot, exts[e]);
        FILE *f = fopen(exe, "rb");
        if (!f)
            continue;
        unsigned base = 0x400000, best = 0;
        char line[2048], name[1024], best_name[1024] = "";
        while (fgets(line, sizeof line, f)) {
            unsigned a = 0;
            if (e == 0) {
                if (line[0] == '#') {
                    const char *b = strstr(line, "base=");
                    if (b) sscanf(b + 5, "%x", &base);
                    continue;
                }
                char *tab = strchr(line, '\t');
                if (!tab || (size_t)(tab - line) >= sizeof name || sscanf(tab + 1, "%x", &a) != 1)
                    continue;
                memcpy(name, line, tab - line);
                name[tab - line] = 0;
            } else {
                char sec[32], addr[32];
                const char *pl = strstr(line, "Preferred load address is ");
                if (pl) {
                    sscanf(pl + 26, "%x", &base);
                    continue;
                }
                if (sscanf(line, " %31s %1023s %31s", sec, name, addr) != 3 ||
                    strlen(sec) != 13 || sec[4] != ':' || strlen(addr) != 8 ||
                    sscanf(addr, "%x", &a) != 1)
                    continue;
            }
            if (a < base)
                continue;
            a -= base;
            if (a <= rva && a >= best) {
                best = a;
                snprintf(best_name, sizeof best_name, "%s", name);
            }
        }
        fclose(f);
        if (best_name[0]) {
            snprintf(out, cap, "%s+0x%x", best_name, (unsigned)(rva - best));
            return;
        }
    }
}

static void div_fault_log(uintptr_t eip, DWORD code, const div_fault::Insn &in, uint32_t d)
{
    const uintptr_t rva = eip - (uintptr_t)GetModuleHandleA(0);
    char sym[1200];
    div_fault_symbol(rva, sym, sizeof sym);
    fprintf(stderr, "[div0] +0x%08x %s code %08lx %s%d by %u -> cartridge answer (first hit at this site)\n",
            (unsigned)rva, sym, (unsigned long)code, in.is_signed ? "idiv" : "div", in.width,
            (unsigned)d);
    fflush(stderr);
}

static int div_fault_install()
{
    const char *e = getenv("SM64DS_DIV_FAULT");
    if (e && *e == '0')
        return 0;
    div_fault::install(div_fault_in_game, div_fault_log);
    return 1;
}
static const int s_div_fault_installed = div_fault_install();

static void div_fault_selftest_hal()
{
    static int done;
    if (done)
        return;
    done = 1;
    const char *e = getenv("SM64DS_DIV_FAULT_SELFTEST");
    if (!e || strcmp(e, "hal") != 0)
        return;
    volatile int num = 7, den = 0;
    fprintf(stderr, "[div0] selftest: dividing by zero in hal/os_arena.cpp (outside game code)\n");
    fflush(stderr);
    volatile int q = num / den;
    fprintf(stderr, "[div0] selftest: NOT EXPECTED, survived with %d\n", q);
}
#else
static void div_fault_selftest_hal() {}
#endif /* _WIN32 && _M_IX86 */
