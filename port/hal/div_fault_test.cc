// hal/div_fault_test.cc -- host unit test for hal/div_fault.h (catchup1
// Stage D round 4, ENG4). x86 (32-bit) MSVC only: every case is an inline-asm
// div or idiv with a known encoding, run under the real vectored handler, and
// checked against the cartridge's answer (ds_idiv / ds_imod: x / 0 = x,
// x % 0 = 0; INT_MIN / -1 = INT_MIN rem 0, read from src/_s32_div_f.c).
// Cases the handler must refuse (a double-width dividend, a fault outside the
// game predicate) must reach the frame's __except unchanged.
//
// Not a CMake target (.cc so the hal/*.cpp seat scan skips it). Build from
// port/: cl /nologo /EHa /O1 hal/div_fault_test.cc /I hal ; run: exit 0 = pass.
#include "div_fault.h"
#include <stdio.h>

static int g_in_game = 1;
static int in_game(uintptr_t) { return g_in_game; }
static int g_logged;
static void log_hit(uintptr_t, DWORD code, const div_fault::Insn &in, uint32_t d)
{
    ++g_logged;
    printf("    logged: code %08lx %s%d len %d divisor %u\n", (unsigned long)code,
           in.is_signed ? "idiv" : "div", in.width, in.len, d);
}

static int g_fail, g_n;
static void check(const char *name, unsigned got_a, unsigned got_d, unsigned want_a, unsigned want_d, int caught)
{
    ++g_n;
    const int ok = !caught && got_a == want_a && got_d == want_d;
    printf("%s %-40s eax=%08x edx=%08x (want %08x %08x)%s\n", ok ? "PASS" : "FAIL", name, got_a, got_d,
           want_a, want_d, caught ? " [reached __except]" : "");
    if (!ok) ++g_fail;
}
static void check_refused(const char *name, int caught, DWORD code, DWORD want)
{
    ++g_n;
    const int ok = caught && code == want;
    printf("%s %-40s refused, __except saw %08lx (want %08lx)\n", ok ? "PASS" : "FAIL", name,
           (unsigned long)code, (unsigned long)want);
    if (!ok) ++g_fail;
}

static DWORD g_code;
static int filt(DWORD c) { g_code = c; return EXCEPTION_EXECUTE_HANDLER; }

#define CASE(NAME, WANT_A, WANT_D, ...)                                         \
    do {                                                                        \
        unsigned ra = 0xdeadbeef, rd = 0xdeadbeef; int caught = 0;               \
        __try { __asm { __VA_ARGS__ } __asm { mov ra, eax } __asm { mov rd, edx } }       \
        __except (filt(GetExceptionCode())) { caught = 1; }                     \
        check(NAME, ra, rd, WANT_A, WANT_D, caught);                           \
    } while (0)

#define REFUSE(NAME, WANT_CODE, ...)                                            \
    do {                                                                        \
        int caught = 0; g_code = 0;                                             \
        __try { __asm { __VA_ARGS__ } }                                                  \
        __except (filt(GetExceptionCode())) { caught = 1; }                     \
        check_refused(NAME, caught, g_code, WANT_CODE);                         \
    } while (0)

static int zero32 = 0;
static short zero16 = 0;
static unsigned char zero8 = 0;
static int table[0x48];

int main()
{
    div_fault::install(in_game, log_hit);
    table[0x40 + 3] = 0;   // [ebx + ecx*4 + 0x100] with ebx=&table, ecx=3

    // ---- 32-bit, c0000094 -------------------------------------------------
    CASE("idiv ecx  (F7 F9)  -77 / 0", 0xffffffb3u, 0u,
         mov eax, -77 __asm cdq __asm xor ecx, ecx __asm idiv ecx);
    CASE("div ecx   (F7 F1)  0xfffffff0 / 0", 0xfffffff0u, 0u,
         mov eax, 0xfffffff0 __asm xor edx, edx __asm xor ecx, ecx __asm div ecx);
    CASE("idiv [esi] (F7 3E) 1234 / 0", 1234u, 0u,
         lea esi, zero32 __asm mov eax, 1234 __asm cdq __asm idiv dword ptr [esi]);
    CASE("idiv [mem32] (F7 3D disp32) 99 / 0", 99u, 0u,
         mov eax, 99 __asm cdq __asm idiv zero32);
    CASE("idiv [esp+4] (F7 7C 24 04 SIB+disp8)", 5u, 0u,
         push 0 __asm push 0 __asm mov eax, 5 __asm cdq __asm idiv dword ptr [esp+4] __asm add esp, 8);
    CASE("idiv [ebx+ecx*4+0x10c] (SIB+disp32)", 0x7fffffffu, 0u,
         lea ebx, table __asm mov ecx, 3 __asm mov eax, 0x7fffffff __asm cdq
         __asm idiv dword ptr [ebx+ecx*4+0x100]);
    CASE("idiv ebp-based [ebp+disp8] via push", 42u, 0u,
         push ebp __asm push 0 __asm mov ebp, esp __asm mov eax, 42 __asm cdq
         __asm idiv dword ptr [ebp+0] __asm pop eax __asm mov eax, 42 __asm pop ebp);
    // ---- 32-bit, c0000095 (INT_MIN / -1) ------------------------------------
    CASE("idiv ecx  INT_MIN / -1 (c0000095)", 0x80000000u, 0u,
         mov eax, 0x80000000 __asm cdq __asm mov ecx, -1 __asm idiv ecx);

    // ---- 16-bit (66 F7) ----------------------------------------------------
    CASE("idiv cx   (66 F7 F9) -300 / 0", 0x1111fed4u, 0x22220000u,
         mov eax, 0x1111fed4 __asm mov edx, 0x2222ffff __asm xor ecx, ecx __asm idiv cx);
    CASE("div cx    (66 F7 F1) 0xabcd / 0", 0x3333abcdu, 0x44440000u,
         mov eax, 0x3333abcd __asm mov edx, 0x44440000 __asm xor ecx, ecx __asm div cx);
    CASE("idiv word [mem] (66 F7 3D) 7 / 0", 0x00000007u, 0u,
         mov eax, 7 __asm xor edx, edx __asm idiv zero16);
    CASE("idiv cx   -32768 / -1 (c0000095)", 0x55558000u, 0x6666ffffu & 0x66660000u,
         mov eax, 0x55558000 __asm mov edx, 0x6666ffff __asm mov ecx, 0xffff __asm idiv cx);

    // ---- 8-bit (F6) --------------------------------------------------------
    CASE("idiv cl   (F6 F9) -5 / 0", 0x777700fbu, 0x99u,
         mov eax, 0x7777fffb __asm mov edx, 0x99 __asm xor ecx, ecx __asm idiv cl);
    CASE("div cl    (F6 F1) 200 / 0", 0x000000c8u, 0x99u,
         mov eax, 0xc8 __asm mov edx, 0x99 __asm xor ecx, ecx __asm div cl);
    CASE("div ch    (F6 F5, rm=5 is ch)", 0x00000011u, 0x99u,
         mov eax, 0x11 __asm mov edx, 0x99 __asm mov ecx, 0x00ff __asm div ch);
    CASE("div byte [esi] (F6 36)", 0x00000023u, 0x99u,
         lea esi, zero8 __asm mov eax, 0x23 __asm mov edx, 0x99 __asm div byte ptr [esi]);
    CASE("idiv cl   -128 / -1 (c0000095)", 0x0000ff80u & 0x80u, 0x99u,
         mov eax, 0xff80 __asm mov edx, 0x99 __asm mov ecx, 0xff __asm idiv cl);

    // ---- refusals: must reach the frame's own handler ------------------------
    REFUSE("idiv ecx with edx not cdq (64-bit dividend)", EXCEPTION_INT_DIVIDE_BY_ZERO,
           mov eax, 5 __asm mov edx, 1 __asm xor ecx, ecx __asm idiv ecx);
    REFUSE("div ecx  quotient overflow (edx >= divisor)", EXCEPTION_INT_OVERFLOW,
           mov eax, 0 __asm mov edx, 2 __asm mov ecx, 1 __asm div ecx);
    g_in_game = 0;
    REFUSE("idiv ecx outside the game predicate", EXCEPTION_INT_DIVIDE_BY_ZERO,
           mov eax, 9 __asm cdq __asm xor ecx, ecx __asm idiv ecx);
    REFUSE("INT_MIN / -1 outside the game predicate", EXCEPTION_INT_OVERFLOW,
           mov eax, 0x80000000 __asm cdq __asm mov ecx, -1 __asm idiv ecx);
    g_in_game = 1;

    printf("%d/%d passed, %d first-hit log lines, handler hits %ld\n", g_n - g_fail, g_n, g_logged,
           (long)div_fault::state().hits);
    return g_fail ? 1 : 0;
}
