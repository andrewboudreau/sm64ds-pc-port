/* hal/div_fault.h -- the cartridge's answer for a hardware integer divide fault
 * in game code (catchup1 Stage D round 4, ENG4, from ACT4a's proposal).
 *
 * WHY. mwccarm compiles every int `/` and `%` to the ITCM helper _s32_div_f
 * (0x01ffabe4, src/_s32_div_f.c; unsigned twin _u32_div_f at 0x01ffadf0). It
 * never faults: a zero divisor branches straight to the sign-restore tail
 *
 *     cmp r1, #0 ; rsblt r1, r1, #0 ; beq _L1f8
 *     _L1f8: ands r3, r12, #0x80000000 ; rsbne r0, r0, #0
 *            ands r3, r12, #1          ; rsbne r1, r1, #0 ; bx lr
 *
 * with r0 still |numerator| and r1 = 0, so QUOTIENT = NUMERATOR, REMAINDER = 0
 * (hal/cstd_div.c's ds_idiv / ds_imod / ds_udiv / ds_umod). INT_MIN / -1 runs
 * the same path: the sign word r12 is (INT_MIN ^ -1) & 0x80000000 = 0 plus
 * bit 0 for the negative numerator, |INT_MIN| is 0x80000000 unsigned, the
 * kernel divides it by 1, and the tail negates only the (zero) remainder, so
 * QUOTIENT = INT_MIN, REMAINDER = 0. x86's div / idiv raise #DE for both
 * (c0000094 for a zero divisor, c0000095 for the overflow).
 *
 * Main's translation units spell plain `/` and `%` where 1.0.7's per-function
 * files went through __aeabi_idiv, so every non-constant divisor in game code
 * is a raw idiv on the host again (ACT4a: 156 candidate sites in 58 files; VS
 * map 0 died at f1401 in the chain chomp). One handler covers all of them.
 *
 * WHAT IT DOES. A vectored handler for c0000094 and c0000095 that acts only
 * when the faulting EIP is game code (the caller's predicate: hal/os_arena.cpp
 * uses the game code segment, hal/game_code_seg.h). It decodes the instruction
 * (div / idiv, r/m8, r/m16 with 66, r/m32; any ModRM / SIB / displacement),
 * reads the divisor, and only when the dividend is the plain extension of the
 * low half (cdq / xor edx, cbw / cwd: what a 32-bit C `/` compiles to) and the
 * divisor is 0, or the signed case is MIN / -1, writes the cartridge's
 * quotient and remainder, steps over the instruction and continues. Anything
 * else (another code, another instruction, a 64-bit dividend, an overflow the
 * ARM helper cannot produce, a FS/GS operand) is CONTINUE_SEARCH, so the crash
 * reporter still fires. It keeps no state the game can see: only a diagnostic
 * first-hit-per-site table for the log, so save states are unaffected.
 */
#pragma once

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace div_fault {

struct Insn {
    int len;          /* whole instruction, prefixes included */
    int width;        /* 8, 16 or 32 */
    int is_signed;    /* idiv */
    int mod, rm;      /* ModRM operand */
    int sib, has_sib;
    int32_t disp;
};

/* Decode a div / idiv at p. Returns 0 when it is anything else. */
inline int decode(const uint8_t *p, Insn *in)
{
    int i = 0, opsize16 = 0;
    for (;; ++i) {
        if (i >= 4) return 0;
        const uint8_t b = p[i];
        if (b == 0x66) { opsize16 = 1; continue; }
        if (b == 0x2e || b == 0x3e || b == 0x26 || b == 0x36) continue;  /* flat segments */
        break;                                   /* 64 / 65 (fs / gs) fall out below */
    }
    const uint8_t op = p[i++];
    if (op != 0xf6 && op != 0xf7) return 0;
    const uint8_t modrm = p[i++];
    const int reg = (modrm >> 3) & 7;
    if (reg != 6 && reg != 7) return 0;
    in->is_signed = reg == 7;
    in->width = op == 0xf6 ? 8 : (opsize16 ? 16 : 32);
    in->mod = modrm >> 6;
    in->rm = modrm & 7;
    in->has_sib = 0;
    in->sib = 0;
    in->disp = 0;
    if (in->mod != 3) {
        int base = in->rm;
        if (in->rm == 4) {
            in->has_sib = 1;
            in->sib = p[i++];
            base = in->sib & 7;
        }
        if (in->mod == 1) {
            in->disp = (int8_t)p[i];
            i += 1;
        } else if (in->mod == 2 || (in->mod == 0 && base == 5)) {
            int32_t d;
            memcpy(&d, p + i, 4);
            in->disp = d;
            i += 4;
        }
    }
    in->len = i;
    return 1;
}

inline DWORD *reg32(CONTEXT *c, int r)
{
    switch (r) {
    case 0: return &c->Eax;
    case 1: return &c->Ecx;
    case 2: return &c->Edx;
    case 3: return &c->Ebx;
    case 4: return &c->Esp;
    case 5: return &c->Ebp;
    case 6: return &c->Esi;
    default: return &c->Edi;
    }
}

/* The divisor, zero-extended to 32 bits from its width. */
inline uint32_t divisor(CONTEXT *c, const Insn &in)
{
    if (in.mod == 3) {
        if (in.width == 8) {
            const DWORD v = *reg32(c, in.rm & 3);
            return in.rm < 4 ? (v & 0xff) : ((v >> 8) & 0xff);   /* al cl dl bl / ah ch dh bh */
        }
        const DWORD v = *reg32(c, in.rm);
        return in.width == 16 ? (v & 0xffff) : v;
    }
    uint32_t ea;
    if (in.has_sib) {
        const int base = in.sib & 7, index = (in.sib >> 3) & 7, scale = in.sib >> 6;
        ea = (in.mod == 0 && base == 5) ? 0 : *reg32(c, base);
        if (index != 4) ea += *reg32(c, index) << scale;
    } else {
        ea = (in.mod == 0 && in.rm == 5) ? 0 : *reg32(c, in.rm);
    }
    ea += (uint32_t)in.disp;
    const uint8_t *m = (const uint8_t *)(uintptr_t)ea;
    if (in.width == 8) return m[0];
    if (in.width == 16) { uint16_t v; memcpy(&v, m, 2); return v; }
    uint32_t v;
    memcpy(&v, m, 4);
    return v;
}

/* Rewrite the context with the cartridge's answer. Returns 0 (and leaves the
   context alone) when this is not a case the ARM helper defines. */
inline int apply(CONTEXT *c, const Insn &in)
{
    const uint32_t d = divisor(c, in);
    const uint32_t mask = in.width == 32 ? 0xffffffffu : ((1u << in.width) - 1u);
    const uint32_t sign = 1u << (in.width - 1);
    uint32_t lo, hi;                             /* dividend halves at this width */
    if (in.width == 8) {
        lo = c->Eax & 0xff;
        hi = (c->Eax >> 8) & 0xff;
    } else {
        lo = c->Eax & mask;
        hi = c->Edx & mask;
    }
    const uint32_t ext = (in.is_signed && (lo & sign)) ? mask : 0u;
    if (hi != ext)
        return 0;                                /* a double-width dividend: not a C `/` */
    uint32_t q;
    if (d == 0)
        q = lo;                                  /* x / 0 = x, x % 0 = 0 */
    else if (in.is_signed && d == mask && lo == sign)
        q = lo;                                  /* MIN / -1 = MIN, remainder 0 */
    else
        return 0;
    if (in.width == 8) {
        c->Eax = (c->Eax & ~0xffffu) | q;        /* al = quotient, ah = 0 */
    } else if (in.width == 16) {
        c->Eax = (c->Eax & ~0xffffu) | q;
        c->Edx = c->Edx & ~0xffffu;
    } else {
        c->Eax = q;
        c->Edx = 0;
    }
    c->Eip += in.len;
    return 1;
}

typedef int (*InGameFn)(uintptr_t eip);

struct State {
    InGameFn in_game;
    PVOID handle;
    volatile LONG nsites;
    uintptr_t sites[256];
    volatile LONG hits;
    void (*log)(uintptr_t eip, DWORD code, const Insn &in, uint32_t div_value);
};

inline State &state()
{
    static State s;
    return s;
}

/* First hit per site (the caller logs); later hits only count. */
inline int first_hit(uintptr_t eip)
{
    State &s = state();
    const LONG n = s.nsites < 256 ? s.nsites : 256;
    for (LONG k = 0; k < n; ++k)
        if (s.sites[k] == eip) return 0;
    const LONG at = InterlockedIncrement(&s.nsites) - 1;
    if (at < 256) s.sites[at] = eip;
    return 1;
}

inline LONG handle(EXCEPTION_POINTERS *ep, InGameFn in_game)
{
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_INT_OVERFLOW)
        return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT *c = ep->ContextRecord;
    const uintptr_t eip = c->Eip;
    if (!in_game || !in_game(eip))
        return EXCEPTION_CONTINUE_SEARCH;
    Insn in;
    if (!decode((const uint8_t *)eip, &in))
        return EXCEPTION_CONTINUE_SEARCH;
    const uint32_t d = divisor(c, in);
    if (!apply(c, in))
        return EXCEPTION_CONTINUE_SEARCH;
    InterlockedIncrement(&state().hits);
    if (first_hit(eip) && state().log)
        state().log(eip, code, in, d);
    return EXCEPTION_CONTINUE_EXECUTION;
}

inline LONG WINAPI veh(EXCEPTION_POINTERS *ep)
{
    return handle(ep, state().in_game);
}

inline void install(InGameFn in_game,
                    void (*log)(uintptr_t, DWORD, const Insn &, uint32_t))
{
    State &s = state();
    s.in_game = in_game;
    s.log = log;
    if (!s.handle)
        s.handle = AddVectoredExceptionHandler(1, veh);
}

} // namespace div_fault
