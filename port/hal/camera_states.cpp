// The camera state machine, host side.
//
// The ROM keeps 19 State objects in arm9 bss at 0x0209b008..0x0209b128, step
// 0x10, each {PMF onEnter, PMF main} -- two mwcc member-function pointers,
// which are {code address, this-delta} pairs. __sinit_d_camera.cpp fills them by
// copying 38 relocated pairs out of 0x02086bc8..0x02086e48. That sinit is not
// run on host (its 38 source pairs are ROM pointer data that would each need
// its own symbol); the objects are synthesized here with the same addresses,
// read out of the arm9 image once and baked in below.
//
// The addresses stay DS addresses on purpose. Nothing calls through them:
// _ZN9dCamera_c10EnterStateEv (onEnter) and _ZN9dCamera_c11UpdateStateEv (main), the matched TUs through
// hostgen's CALL_STATE_FN patch (lane shadow-A retired their host copies),
// read the word and hand it to hal_call_camera_state_fn, which translates it
// to a host call through the table in camera_states.inc. That is the same
// shape the player state machine uses (hal_call_state_fn / player_states.inc)
// and it is why an MSVC build can drive a mwcc PMF table at all.
//
// Two more things depend on these objects being ONE contiguous array:
// Camera::InitResources enters &data_0209b008[0], and Camera::ChangeState
// compares the current state against &data_0209b0c8. Both are just symbol
// addresses, so ordinary separate definitions would do -- but every state
// past 0 is reached by an explicit &data_0209bXXX, so each one is named.
#include <cstdio>
#include "dsstate_seg.h"

extern "C" {

/* the 29 distinct state functions, all matched in src/ and all in the
   gate-13 slice. Declared through void* here: they are C-linkage cdecl
   one-argument functions, and their own TUs spell the parameter every way
   from `int` to a local struct pointer. */
int _ZN9dCamera_c13func_0200af0cEv(void *);
int _ZN9dCamera_c13func_02009e70Ev(void *);
int _ZN9dCamera_c13func_02009e50Ev(void *);
int func_02009e30(void *);
int func_02009e10(void *);
int func_02009df0(void *);
int func_02009dd0(void *);
int func_02009db0(void *);
int func_02009d90(void *);
int func_02009d70(void *);
int _ZN9dCamera_c13func_02009d30Ev(void *);
int func_02009aa8(void *);
int func_020098b8(void *);
void func_0200987c(void *);
int func_0200985c(void *);
int func_020097ec(void *);
int func_020095e4(void *);
int _Z13func_02005324Pv(void *);
int _Z13func_02005110P9dCamera_c(void *);
int _Z13func_020050dcP9dCamera_c(void *);
int _Z13func_02005098P9dCamera_c(void *);
int func_020095c4(void *);
int func_02009540(void *);
int _ZN9dCamera_c13func_02009a8cEv(void *);
int _ZN9dCamera_c13func_020098e0Ev(void *);
int func_020094e4(void *);
void func_020094c0(void *);
int _Z13func_02005060P9dCamera_c(void *);
void _Z13func_02005000P9dCamera_c(void *);

int hal_call_camera_state_fn(void *self, unsigned ds_addr)
{
    switch (ds_addr) {
#include "camera_states.inc"
    }
    std::fprintf(stderr, "  [cam] unhosted camera state fn 0x%08x (no-op)\n",
                 ds_addr);
    return 1;
}

/* The 19 State objects. Layout: {onEnter fn, onEnter delta, main fn, main
   delta}. The deltas are all zero in the ROM's pairs.

   These are routed into the save state's captured section even though nothing
   writes them after boot (the ROM's __sinit_d_camera.cpp fills them once; the port
   bakes the same values in). Capturing 304 bytes that never change is free;
   exempting them would put back the hand-asserted "this one cannot diverge"
   list that the 0.2.0 restore crash came out of. See hal/dsstate_seg.h. */
/* The 19 State objects are src/engine/view/d_camera.cpp's own now (catchup1
   round 3): its initialisers copy ctor_runner's DS-address pairs, so each
   word is still the DS address hal_call_camera_state_fn translates. */

}  /* extern "C" */

/* catchup1 round 3: src/engine/view/d_camera.cpp defines the camera State objects. The C name reaches that storage: */
#pragma comment(linker, "/alternatename:_data_0209b008=?data_0209b008@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b018=?data_0209b018@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b028=?data_0209b028@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b038=?data_0209b038@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b048=?data_0209b048@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b058=?data_0209b058@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b068=?data_0209b068@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b078=?data_0209b078@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b088=?data_0209b088@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b098=?data_0209b098@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b0a8=?data_0209b0a8@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b0b8=?data_0209b0b8@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b0c8=?data_0209b0c8@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b0d8=?data_0209b0d8@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b0e8=?data_0209b0e8@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b0f8=?data_0209b0f8@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b108=?data_0209b108@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b118=?data_0209b118@@3UState@dCamera_c@@A")
#pragma comment(linker, "/alternatename:_data_0209b128=?data_0209b128@@3UState@dCamera_c@@A")
