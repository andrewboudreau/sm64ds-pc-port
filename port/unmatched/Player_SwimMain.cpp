/* HOST COPY of src/game/player/Player.cpp -- Player swimming state, per frame.
 *
 * This file is BYTE-IDENTICAL to the byte-locked source except for ONE call:
 * the health check `daPly_c::GetHealth()`. It was produced by copying the source
 * verbatim and applying exactly two string edits (the extern declaration and the
 * single call site), so it cannot carry a transcription error against the 300-line
 * original. See the header of unmatched/KingBobOmb_HeldHealthCheck.cpp for the
 * companion GetHealth caller whose receiver is DIFFERENT.
 *
 * THE CALLING-CONVENTION SEAM (this is the JRB SIG-SWIM crash):
 *
 * daPly_c::GetHealth reads `this` at +0x6d8 (mPlayerNo). St_Swim_Main is a Player
 * member; its `this` is in r0, and on the path to the GetHealth call r0 is NOT
 * reloaded:
 *
 *     020cd94c: push {r4,r5,r6,lr}
 *     020cd954: mov  r5, r0            ; r5 = this  (r0 left holding this)
 *     020cd958: ldrb r1, [r5, #0x6f9]
 *     020cd960: beq  0x20cd97c         ; the metal-cap branch that WOULD clobber r0 is skipped
 *     020cd97c: bl   daPly_c::GetHealth ; r0 still = this
 *
 * The matched C spells GetHealth() with no argument; byte-identical on ARM
 * because `this` is already in r0. On the host the GetHealth bridge
 * (hal/player_bridges.cpp) reads `self` off the stack; the zero-argument call
 * pushes nothing, so GetHealth indexes the health table with a garbage mPlayerNo
 * and the swim state faults -- the SIG-SWIM signature in the JRB matrix. THE FIX
 * passes `this` explicitly, exactly the value the ROM leaves in r0.
 *
 * src/game/player/Player.cpp is dropped from slice_gate10.txt in favour
 * of this file; the byte-locked source is unchanged.
 */
//cpp
extern "C" {
extern void _ZN7daPly_c11ChangeStateERNS_5StateE(void*, void*);
extern int _ZN7daPly_c9GetHealthEv(void* self);
extern void _ZN7daPly_c19func_ov002_020c5decEi(void*, int);
extern int _ZN7daPly_c19func_ov002_020cede0Ev(void*);
extern int _ZN7daPly_c19func_ov002_020cec2cEv(void*);
extern void _ZN7daPly_c19func_ov002_020cd71cEv(void*);
extern void _ZN7daPly_c19func_ov002_020cd550Ev(void*);
extern unsigned int _ZN8Particle6System13NewCheckWaterEjjiiiPK11Vector3_16f(unsigned int, unsigned int, int, int, int, void*);
extern int _ZNK10dBgCh_Actr10IsOnGroundEv(void*);
extern void* _ZNK10dBgCh_Actr14GetFloorResultEv(void*);
extern void* _Z13func_02037dc4i(void*);
extern int _ZN4cstd5atan2Eii(int, int);
extern void Vec3_RotateYAndTranslate(int*, void*, short, int*);
extern void _Z14ApproachLinearRiii(int*, int, int);
extern int _ZN7daPly_c6IsAnimEj(void*, unsigned int);
extern int _ZN7daPly_c12FinishedAnimEv(void*);
extern void _ZN7daPly_c7SetAnimEjiit(void*, unsigned int, int, int, unsigned int);
extern int _ZNK7daPly_c14GetBodyModelIDEjb(void*, unsigned char, int);
extern int _ZN7daPly_c19func_ov002_020ce324Ev(void*);
extern void _ZN7daPly_c19func_ov002_020cd364Ev(void*);
extern void _ZN7daPly_c13StartAnim0xA9Ev(void*);
extern int _ZN7daPly_c19func_ov002_020cedb0Ei(void*, int);
extern void _ZN7daPly_c13StartAnim0xAAEv(void*);
extern int _ZNK15dExtFrameCtrl_c12WillHitFrameEi(void*, int);
extern void _ZN5Sound9PlayBank0EjRK7Vector3(unsigned int, void*);
extern void _ZN7daPly_c13StartAnim0xABEv(void*);
extern void _ZN7daPly_c13StartAnim0xAEEv(void*);
extern void _ZN7daPly_c19func_ov002_020dbf4cEv(void*);
extern void _ZN5dCc_c5ClearEv(void*);
extern void _ZN5dCc_c6UpdateEv(void*);
extern void _ZN7daPly_c15StartStateMusicEj(void*, unsigned int);
extern void _ZN7daPly_c13EndStateMusicEj(void*, unsigned int);
extern void _ZN7daPly_c16ReleaseHeldActorEv(void*);
extern void _ZN7daPly_c19func_ov002_020daa74Ev(void*);
extern void _ZN7daPly_c19func_ov002_020cd218EiPiS0_(void*, int, void*, void*);
extern int _ZN7daPly_c19func_ov002_020ceaf4Ev(void*);
extern int _ZN7daPly_c19func_ov002_020ce5f8Ev(void*);
extern void _ZN7daPly_c12AdvanceAnimsEv(void*);

extern char data_ov002_021106ac[];
extern char data_ov002_021101b4[];
extern unsigned char data_020a0e40;
extern unsigned short data_0209f49c;
extern unsigned short data_0209f49e;
extern char data_0209ee90[];

// PORT_HOST_ABI: implicit-register-arg (GetHealth's receiver rode r0; the host passes the state's player c).
int _ZN7daPly_c12St_Swim_MainEv(char* c)
{
    int t4;
    volatile int pos[3];
    int* fr;
    int ang;
    int out[3];
    int vec[3];
    short yaw;
    int var_r2;
    unsigned short a4;
    int var_r6;
    int var_r1;
    int has;
    int* p;
    int* sub;
    unsigned int t5;


    if (*(unsigned char*)(c+0x6f9) != 0) {
        _ZN7daPly_c11ChangeStateERNS_5StateE(c, data_ov002_021106ac);
        return 1;
    }
    if (_ZN7daPly_c9GetHealthEv(c) == 0) {
        _ZN7daPly_c19func_ov002_020c5decEi(c, 8);
        return 1;
    }
    if (_ZN7daPly_c19func_ov002_020cede0Ev(c) == 0) {
        *(unsigned char*)(c+0x706) = 0;
        _ZN7daPly_c11ChangeStateERNS_5StateE(c, data_ov002_021101b4);
        return 1;
    }
    *(int*)(c+0x684) = *(int*)(c+0x60);
    if (_ZN7daPly_c19func_ov002_020cec2cEv(c) != 0) return 1;

    t4 = (*(unsigned short*)((char*)&data_0209f49c + data_020a0e40 * 0x18)) & 0x802;
    *(short*)(c+0x69e) = *(short*)(c+0x90);
    _ZN7daPly_c19func_ov002_020cd71cEv(c);
    _ZN7daPly_c19func_ov002_020cd550Ev(c);

    if (*(unsigned short*)(c+0x6a8) != 0) {
        int eid = 0xcd;
        int y, z;
        pos[0] = *(int*)(c+0x5c);
        y = *(int*)(c+0x60);
        pos[1] = y;
        z = *(int*)(c+0x64);
        pos[2] = z;
        y = y + 0x32000;
        pos[1] = y;
        *(unsigned int*)(c+0x630) = _ZN8Particle6System13NewCheckWaterEjjiiiPK11Vector3_16f(*(volatile unsigned int*)(c+0x630), eid, pos[0], pos[1], z, 0);
    }

    if (*(int*)(c+0x640) == 0 && _ZNK10dBgCh_Actr10IsOnGroundEv(c+0x380) != 0) {
        fr = (int*)_Z13func_02037dc4i((char*)_ZNK10dBgCh_Actr14GetFloorResultEv(c+0x380) + 4);
        ang = fr[1];
        if (ang < 0x720 && ang > 0x400 && *(int*)(c+0x60) >= *(int*)(c+0x64c) - 0xc8000) {
            int at;
            *(int*)(c+0x60) = *(int*)(c+0x54c) - 0x8000;
            at = _ZN4cstd5atan2Eii(fr[0], fr[2]);
            vec[0] = 0;
            vec[1] = 0;
            at = at + 0x8000;
            yaw = (short)at;
            vec[2] = (*(int*)((char*)data_0209ee90 + 0x244)) << 0xc;
            Vec3_RotateYAndTranslate(out, c+0x5c, yaw, vec);
            *(int*)(c+0x5c) = out[0];
            *(int*)(c+0x60) = out[1];
            *(int*)(c+0x64) = out[2];
        }
    }

    switch (*(unsigned char*)(c+0x6e3)) {
    case 0:
        *(short*)(c+0x92) = -0x4000;
        *(short*)(c+0x96) = 0;
        *(short*)(c+0x8c) = 0;
        *(short*)(c+0x90) = 0;
        *(short*)(c+0x69c) = 0;
        _Z14ApproachLinearRiii((int*)(c+0x640), 0, 0x5000);
        if (_ZN7daPly_c6IsAnimEj(c, 0xa8) == 0 && _ZN7daPly_c12FinishedAnimEv(c) != 0) {
            _ZN7daPly_c7SetAnimEjiit(c, 0xa8, 0, 0x1000, 0);
        }
        if (*(int*)(c+0x640) == 0) {
            *(unsigned char*)(c+0x6e3) = 1;
            *(short*)(c+0x92) = 0;
            *(unsigned short*)(c+0x6a4) = 0x1e;
        }
        break;
    case 1:
        var_r2 = 0x800;
        if (*(unsigned short*)(c+0x6a4) != 0) var_r2 = 0x200;
        _Z14ApproachLinearRiii((int*)(c+0x640), 0, var_r2);
        if (_ZN7daPly_c6IsAnimEj(c, 0xa8) == 0) {
            if (_ZN7daPly_c12FinishedAnimEv(c) != 0) {
                _ZN7daPly_c7SetAnimEjiit(c, 0xa8, 0, 0x1000, 0);
            }
        } else if (*(short*)(c+0x8c) >= 0x1000) {
            sub = (int*)((char*)(*(int*)(c + _ZNK7daPly_c14GetBodyModelIDEjb(c, (unsigned char)*(int*)(c+8), 0)*4 + 0xdc)) + 0x50);
            if (*(int*)((char*)sub + 0xc) != 0x3000) {
                sub = (int*)((char*)(*(int*)(c + _ZNK7daPly_c14GetBodyModelIDEjb(c, (unsigned char)*(int*)(c+8), 0)*4 + 0xdc)) + 0x50);
                *(int*)((char*)sub + 0xc) = 0x3000;
            }
        } else if (*(short*)(c+0x69c) != 0) {
            sub = (int*)((char*)(*(int*)(c + _ZNK7daPly_c14GetBodyModelIDEjb(c, (unsigned char)*(int*)(c+8), 0)*4 + 0xdc)) + 0x50);
            if (*(int*)((char*)sub + 0xc) != 0x2000) {
                sub = (int*)((char*)(*(int*)(c + _ZNK7daPly_c14GetBodyModelIDEjb(c, (unsigned char)*(int*)(c+8), 0)*4 + 0xdc)) + 0x50);
                *(int*)((char*)sub + 0xc) = 0x2000;
            }
        } else {
            sub = (int*)((char*)(*(int*)(c + _ZNK7daPly_c14GetBodyModelIDEjb(c, (unsigned char)*(int*)(c+8), 0)*4 + 0xdc)) + 0x50);
            if (*(int*)((char*)sub + 0xc) != 0x1000) {
                sub = (int*)((char*)(*(int*)(c + _ZNK7daPly_c14GetBodyModelIDEjb(c, (unsigned char)*(int*)(c+8), 0)*4 + 0xdc)) + 0x50);
                *(int*)((char*)sub + 0xc) = 0x1000;
            }
        }
        if (_ZN7daPly_c19func_ov002_020ce324Ev(c) != 0) return 1;
        if ((*(unsigned short*)((char*)&data_0209f49e + data_020a0e40 * 0x18)) & 1) {
            _ZN7daPly_c19func_ov002_020cd364Ev(c);
        } else if (t4 != 0) {
            _ZN7daPly_c13StartAnim0xA9Ev(c);
        }
        break;
    case 2:
        a4 = *(unsigned short*)(c+0x6a4);
        if (a4 >= 0xa) var_r6 = 0x800;
        else if (a4 <= 4) var_r6 = 0x1800;
        else var_r6 = 0;
        _Z14ApproachLinearRiii((int*)(c+0x640), _ZN7daPly_c19func_ov002_020cedb0Ei(c, 0x10000), var_r6);
        if (*(unsigned short*)(c+0x6a4) < 0xa && t4 == 0) {
            *(unsigned char*)(((unsigned int)c + 0x6e5) & 0xFFFFFFFFFFFFFFFFULL) |= 1;
        }
        if (*(unsigned short*)(c+0x6a4) == 0 && _ZN7daPly_c6IsAnimEj(c, 0xa9) != 0 && _ZN7daPly_c12FinishedAnimEv(c) != 0) {
            if (!(*(unsigned char*)(c+0x6e5) & 1)) {
                *(unsigned char*)(c+0x6e3) = 3;
            } else {
                _ZN7daPly_c13StartAnim0xAAEv(c);
            }
        }
        break;
    case 3:
        _Z14ApproachLinearRiii((int*)(c+0x640), _ZN7daPly_c19func_ov002_020cedb0Ei(c, 0xc000), 0x200);
        if (_ZN7daPly_c19func_ov002_020ce324Ev(c) != 0) return 1;
        if (t4 != 0) {
            _ZN7daPly_c7SetAnimEjiit(c, 0xa6, 0, 0x1000, 0);
            if (_ZNK15dExtFrameCtrl_c12WillHitFrameEi((void*)(*(int*)(c + _ZNK7daPly_c14GetBodyModelIDEjb(c, (unsigned char)*(int*)(c+8), 0)*4 + 0xdc) + 0x50), 0) != 0) {
                _ZN5Sound9PlayBank0EjRK7Vector3(0x15, c+0x74);
            }
        } else {
            _ZN7daPly_c13StartAnim0xAAEv(c);
        }
        break;
    case 4:
        _Z14ApproachLinearRiii((int*)(c+0x640), 0, 0x400);
        if (_ZN7daPly_c19func_ov002_020ce324Ev(c) != 0) return 1;
        if (*(unsigned short*)(c+0x6a4) == 0 && t4 != 0) {
            _ZN7daPly_c13StartAnim0xA9Ev(c);
        }
        if (_ZN7daPly_c12FinishedAnimEv(c) != 0 && _ZN7daPly_c6IsAnimEj(c, 0xaa) != 0) {
            _ZN7daPly_c13StartAnim0xABEv(c);
        }
        break;
    case 5:
        if (*(unsigned short*)(c+0x6a4) == 0) {
            _ZN7daPly_c13StartAnim0xAEEv(c);
        }
        break;
    case 6:
        _ZN7daPly_c19func_ov002_020dbf4cEv(c);
        var_r1 = _ZN7daPly_c19func_ov002_020cedb0Ei(c, 0x10000);
        a4 = *(unsigned short*)(c+0x6a4);
        if (a4 >= 5) var_r2 = 0x1000;
        else if (a4 == 0) { var_r2 = 0x800; var_r1 = 0; }
        else var_r2 = 0;
        _Z14ApproachLinearRiii((int*)(c+0x640), var_r1, var_r2);
        if (_ZN7daPly_c12FinishedAnimEv(c) != 0) {
            has = (*(int*)(c+0x358) != 0) ? 1 : 0;
            if (has != 0) {
                *(unsigned char*)(c+0x6e3) = 8;
                *(unsigned short*)(c+0x6a4) = 0xf0;
                p = (int*)(((*(int*)(c+0x358) + 0xb0) & 0xFFFFFFFFFFFFFFFFULL));
                *p |= 0x4000;
                _ZN7daPly_c7SetAnimEjiit(c, 0xb0, 0, 0x1000, 0);
            } else {
                _ZN7daPly_c13StartAnim0xABEv(c);
            }
        }
        _ZN5dCc_c5ClearEv(c+0x314);
        sub = (int*)((char*)(*(int*)(c + _ZNK7daPly_c14GetBodyModelIDEjb(c, (unsigned char)*(int*)(c+8), 0)*4 + 0xdc)) + 0x50);
        t5 = ((unsigned int)(*(int*)((char*)sub + 8)) << 4) >> 0x10;
        if ((int)t5 >= 5 && (int)t5 <= 7) {
            _ZN5dCc_c6UpdateEv(c+0x314);
        }
        break;
    case 7:
        if (_ZN7daPly_c12FinishedAnimEv(c) != 0) {
            *(unsigned char*)(c+0x6e3) = 8;
            *(unsigned short*)(c+0x6a4) = 0xf0;
            _ZN7daPly_c7SetAnimEjiit(c, 0xb0, 0, 0x1000, 0);
            p = (int*)(((*(int*)(c+0x358) + 0xb0) & 0xFFFFFFFFFFFFFFFFULL));
            *p |= 0x4000;
            _ZN7daPly_c15StartStateMusicEj(c, 0x33);
            *(unsigned char*)(c+0x6f7) = 1;
        }
        break;
    case 8:
        _Z14ApproachLinearRiii((int*)(c+0x640), _ZN7daPly_c19func_ov002_020cedb0Ei(c, 0x1e000), 0x400);
        if ((*(unsigned short*)((char*)&data_0209f49e + data_020a0e40 * 0x18)) & 1) {
            _ZN7daPly_c7SetAnimEjiit(c, 0xb1, 0x40000000, 0x1000, 0);
            *(unsigned char*)(c+0x6e3) = 9;
            _ZN7daPly_c13EndStateMusicEj(c, 0x33);
        }
        if (*(unsigned short*)(c+0x6a4) != 0) {
            has = (*(int*)(c+0x358) != 0) ? 1 : 0;
            if (has == 0) goto case8_block107;
        } else {
        case8_block107:
            _ZN7daPly_c16ReleaseHeldActorEv(c);
            _ZN7daPly_c13StartAnim0xABEv(c);
            _ZN7daPly_c13EndStateMusicEj(c, 0x33);
            *(unsigned char*)(c+0x6f7) = 0;
        }
        if (_ZNK15dExtFrameCtrl_c12WillHitFrameEi((void*)(*(int*)(c + _ZNK7daPly_c14GetBodyModelIDEjb(c, (unsigned char)*(int*)(c+8), 0)*4 + 0xdc) + 0x50), 0) != 0) {
            _ZN5Sound9PlayBank0EjRK7Vector3(0x15, c+0x74);
        }
        break;
    case 9:
        if (_ZNK15dExtFrameCtrl_c12WillHitFrameEi((void*)(*(int*)(c + _ZNK7daPly_c14GetBodyModelIDEjb(c, (unsigned char)*(int*)(c+8), 0)*4 + 0xdc) + 0x50), 0xd) != 0) {
            _ZN7daPly_c19func_ov002_020daa74Ev(c);
        }
        if (_ZN7daPly_c12FinishedAnimEv(c) != 0) {
            _ZN7daPly_c13StartAnim0xABEv(c);
        }
        break;
    }

    _ZN7daPly_c19func_ov002_020cd218EiPiS0_(c, *(int*)(c+0x640), c+0x98, c+0xa8);
    {
        int* pa8 = (int*)(((unsigned int)(c + 0xa8)) & 0xFFFFFFFFFFFFFFFFULL);
        *pa8 = *pa8 + _ZN7daPly_c19func_ov002_020ceaf4Ev(c);
    }
    if (_ZN7daPly_c19func_ov002_020ce5f8Ev(c) != 0) return 1;
    _ZN7daPly_c12AdvanceAnimsEv(c);
    return 1;
}
}
