// Host storage for the heap registry, and the one heap asm primitive the
// host has to supply.
//
// The NitroSystem heap registry -- NNS_FND_heapcommon.c's sRootList and
// sRootListInitialized -- is .bss the decomp TU only declares
// (src/engine/heap/NestedHeapIterator.cpp says why: no arm9 .bss range is
// enrolled for it). On the NDS the link places sRootListInitialized at
// 0x020a4d34 and sRootList at 0x020a4d38; on the host this file is the
// definition. Both are declared extern "C" in that TU (include/nnsys/fnd.h's
// types), so the C spellings are the only ones anything references.
//
// Before the bump (decomp pin 03a6d6aea41) the registry was the coined
// NestedHeapIterator class, hosted here as a 0x20-byte blob named
// _ZN6Memory16rootHeapIteratorE with a family of /alternatename rows and
// __cdecl -> __thiscall forwarders for its methods (AddLast, AddFirst, Next,
// Init, FindNested, the C1 bridges) and for ExpandingHeapAllocator and
// MemoryNode. Main replaced all of that with NitroSystem's NNS_FND list and
// heap-head code (63e1097b379 and the heap NNS rework, shifts.md section 7):
// every one of those functions is now an extern "C" NNS_Fnd* function the
// decomp TUs define and call by one name, so the forwarders have nothing
// left to convert and are gone with the types they named.
//
// LAYOUT, by offset (the host models only these two objects; the asserts
// below hold every offset the heap TUs and this file depend on):
//
//   old (pin)                                   new (TARGET)                    off   size
//   _ZN6Memory25isRootHeapIterInitializedE int  sRootListInitialized int        -     4
//   _ZN6Memory16rootHeapIteratorE char[0x20]    sRootList NNSFndList            -     0xc
//     +0x00 head                                  headObject                    0x0   4
//     +0x04 tail                                  tailObject                    0x4   4
//     +0x08 count (u16)                           numObjects                    0x8   2
//     +0x0a link offset (u16, 4)                  offset                        0xa   2
//   heap head (NNSiFndHeapHead), what the list threads through:
//     signature 0x0, link 0x4 (prevObject 0x4, nextObject 0x8), childList 0xc,
//     heapStart 0x18, heapEnd 0x1c, attribute 0x20, size 0x24.
//
// The pin blob was 0x20 "with room"; the ROM object is 0xc (0x020a4d38 + 0xc
// is the next symbol in the arm9 .bss run), so the host now holds exactly that.

#include <stddef.h>
#include "dsstate_seg.h"
#include "nnsys/fnd.h"

static_assert(sizeof(NNSFndLink) == 0x8, "NNSFndLink is two words");
static_assert(offsetof(NNSFndLink, prevObject) == 0x0, "NNSFndLink.prevObject");
static_assert(offsetof(NNSFndLink, nextObject) == 0x4, "NNSFndLink.nextObject");
static_assert(sizeof(NNSFndList) == 0xc, "sRootList is 0xc bytes at 0x020a4d38");
static_assert(offsetof(NNSFndList, headObject) == 0x0, "NNSFndList.headObject");
static_assert(offsetof(NNSFndList, tailObject) == 0x4, "NNSFndList.tailObject");
static_assert(offsetof(NNSFndList, numObjects) == 0x8, "NNSFndList.numObjects");
static_assert(offsetof(NNSFndList, offset) == 0xa, "NNSFndList.offset");
static_assert(offsetof(NNSiFndHeapHead, signature) == 0x0, "heap head signature");
static_assert(offsetof(NNSiFndHeapHead, link) == 0x4, "heap head link (NNS_FndInitList(&sRootList, 4))");
static_assert(offsetof(NNSiFndHeapHead, childList) == 0xc, "heap head childList");
static_assert(offsetof(NNSiFndHeapHead, heapStart) == 0x18, "heap head heapStart");
static_assert(offsetof(NNSiFndHeapHead, heapEnd) == 0x1c, "heap head heapEnd");
static_assert(offsetof(NNSiFndHeapHead, attribute) == 0x20, "heap head attribute");
static_assert(sizeof(NNSiFndHeapHead) == 0x24, "heap head size");

extern "C" {
// Hosted DS globals the save state must roll back (the list threads INTO the
// arena), so they live in the .dsstate capture section, as the pin's two
// names did.
DSSTATE_BEGIN
int sRootListInitialized;
NNSFndList sRootList;
DSSTATE_END

// SDK asm primitive (stmia burst fill) -> plain word fill on host. The
// frontier classifies its TU as HAL-owned; this is the HAL half.
// PORT_HOST_ABI: ARM asm primitive (stmia burst fill), MSVC cannot assemble.
void MIi_CpuClear32(int val, int *dst, int len)
{
    for (int i = 0; i < len / 4; ++i)
        dst[i] = val;
}
}

// The two pin storage-name rows that stood here are retired: at TARGET
// src/engine/heap/NestedHeapIterator.cpp declares sRootList and
// sRootListInitialized inside extern "C", so every reference is the C name
// defined above, and the pin names are referenced by nothing.
