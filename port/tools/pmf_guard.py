#!/usr/bin/env python
"""Refuse a build that seats a stack-receiver body in a pointer-to-member cell.

THE DEFECT THIS CATCHES, which nine lanes have now each found at a different
class (5ae983797 FlameChomp, 27a24ff5a ChainChomp, 651b5e853 daPgDfdr_c,
f9936e798 daGmch_c, 45ce69707 daPgMthr_c, 00732a5ab SnowmanHead and
daBgSnmBdy_c, then this lane at TreasureChest, Bird, PrincessPeach, Butterfly
and KingBobOmb):

  A ROM state record holds a pointer to member.  MSVC calls one by putting
  `this + delta` in ECX and pushing NOTHING, whether it reaches the record
  through a `call` or through a tail `jmp`.  A port seat that writes a __cdecl
  word into that record -- one of hal/faces_sync_gen.cpp's flat C faces, or a
  matched body taken raw -- gives the body a receiver it has to read off the
  caller's stack, and nobody put one there.  It shows up as a junk receiver, a
  null receiver, or control landing on a data word.

  The seat header is usually the thing that is wrong and it is wrong the same
  way each time: it reasons that the dispatcher is a tail jump, so the caller's
  own argument is still in place.  That is true of a FLAT C dispatcher whose
  first stack argument is the receiver (BabyPenguin's host copies, the path
  lift's _ZN16dPathLiftActor_c8SetStateEi, the puzzle piece's _ZN18daWater_Hakidasi_c8SetStateEPNS_5StateE), and it
  is false as soon as the dispatcher is a __thiscall member, and false again as
  soon as /O2 inlines it into a caller in the same translation unit.

THE CRITERION, and why it is mechanical.  Every seat in port/ writes its cells
out of one static table in its own object's .rdata, so the code word that ends
up in each cell is IN THE BUILT IMAGE and can be read back.  For each seat
table this file names, the guard reads every code word out of the image, walks
the body's prologue, and asks where the receiver comes from:

  ECX      the body reads ecx before anything overwrites it: a __thiscall
           member, or the __fastcall thunk this campaign seats over a face.
  STACK    the body never reads ecx and does read [ebp+8] / [esp+4].
  NEITHER  neither: the body ignores its receiver.

A __fastcall face that ALSO takes a stack argument reads both, and ECX wins:
what decides is where the RECEIVER comes from.  A STACK word in a table this
file marks ECX is the defect, and the guard refuses.

The tables marked CDECL are the adjudicated exceptions, each with the flat C
dispatcher that does push the receiver named beside it.  A thunk on one of
those would break what works, so they are recorded here rather than left for
the next lane to rediscover.

THE LEDGER IS NOT THE WHOLE CHECK ANY MORE.  A table nobody added to it was
not checked at all, which is how the Amp's six cells sat raw in a /vmg /vmm
dispatcher's records through five folds while the headline below read OK.  The
second half of this file (THE DERIVED CENSUS) asks the image which records are
dispatched with the receiver in ECX instead of asking a list, and checks every
seat table that reaches one.  It needs no maintenance when a new class is
seated.

usage: pmf_guard.py [--root <tree>] [--exe <exe>] [--map <map>] [--selftest]
                    [--list]
"""
import argparse
import bisect
import os
import re
import struct
import sys
import decomp_root  # noqa: E402

try:
    import capstone
except ImportError:
    sys.stderr.write("pmf_guard: capstone is not installed\n")
    raise SystemExit(2)


# ---- the ledger -----------------------------------------------------------
# One row per seat table, matched against the map by REGEX because MSVC
# decorates a file-scope static with its own unnamed struct tag.  ECX rows are
# checked; CDECL rows are the adjudicated exceptions and are only reported.
LEDGER = [
    ("ECX", r"^\?g_treasure_chest_states@@",
     "TreasureChest: ?SetState@daObjTbox_c@@QAEXH@Z calls through "
     "_data_ov064_0211c98c with ecx = this + delta, ?CallStateBehavior tail "
     "jumps the same way; both are __thiscall members, so no receiver is ever "
     "on the stack"),
    ("ECX", r"^\?g_princess_peach_states@@",
     "PrincessPeach: ?CallStateInit and ?CallStateBehavior both tail jump "
     "through the record at +0x350 with ecx = this + delta, the shape "
     "00732a5ab fixed for SnowmanHead one overlay over"),
    ("ECX", r"^\?g_butterfly_states@@",
     "Butterfly: ?Behavior@daBtfly_c@@UAEHXZ carries the call inline through "
     "_data_ov100_02148628 with ecx = this + delta and nothing pushed"),
    ("ECX", r"^\?g_bird@@",
     "Bird: ?Behavior@daSBird_c@@UAEHXZ calls through _data_ov009_02113c48 with "
     "ecx = this + delta and nothing pushed"),
    ("ECX", r"^\?g_king_states@@",
     "KingBobOmb: five of the class's own matched bodies inline the state "
     "change and call through _data_ov078_02126ffc with ecx = this + delta; "
     "__ZN12daBombking_c19KingBobOmb_SetStateEPv carries ecx on its tail-jump path too"),
    ("ECX", r"^\?g_smb_cells@@",
     "daBgSnmBdy_c: ?Behavior and ?InitResources both carry the call inline "
     "with ecx = this + delta (00732a5ab)"),
    ("ECX", r"^\?g_smh_cells@@",
     "SnowmanHead: ?SetState calls ?CallStateInit with nothing pushed and "
     "both Call* helpers tail jump with ecx = this + delta (00732a5ab)"),

    # ---- the minigame framework base, run link100 lane PMFSWEEP2 -----------
    ("ECX", r"^\?seats_ecx@\?1\?\?port_mg_base_writer_seat@@",
     "dMgState_c: these twenty-nine pair globals are copied by the state "
     "bodies into the framework message object at +0x08 and +0x10, and those "
     "two fields have exactly two readers in the image, "
     "?Behavior@dMgState_c@@QAEXXZ (+0x16 mov eax,[esi+8]; +0x20 mov ecx,"
     "[esi+0xc]; add ecx,esi; jmp eax) and ?Render@dMgState_c@@QAEXXZ (+0x8 "
     "mov edx,[eax+0x10]; +0xf mov ecx,[eax+0x14]; add ecx,eax; jmp edx). "
     "Both are __thiscall members with NO stack argument and their callers "
     "push nothing (?BeforeBehavior@dScMgBase_c@@UAEHXZ+0x96 lea ecx,"
     "[esi+0xcc]; call), so the word at [esp+4] is the caller's saved edi"),
    ("ECX", r"^\?cells_490@\?1\?\?port_mg_framework_tables_seat@@",
     "sBannerSetupTable: src/Banner_Init.cpp dispatches this table "
     "itself with a real call, ecx = this and nothing pushed (lane MGWRITER "
     "measured it off the TU's own listing and seated __fastcall faces)"),

    ("CDECL", r"^\?seats_cdecl@\?1\?\?port_mg_base_writer_seat@@",
     "dMgState_c: the seven cells that land in the 020b3278 object, whose "
     "two readers are the FLAT C dispatchers __Z13Banner_UpdateP8MgBanner and "
     "__Z11Banner_DrawP8MgBanner, f(self) tail jumps that leave the caller's own "
     "pushed argument at [esp+4]. (Group A, the setter's own twenty, left "
     "this table in run linkfull lane PMFMG1 for seats_ecx_a above: the "
     "matched setter dispatches them with the receiver in ecx.)"),
    ("CDECL", r"^\?cells_field@\?1\?\?port_mg_framework_tables_seat@@",
     "sBannerUpdateTable and _020bf4f8: Banner_Init copies these "
     "into the 020b3278 object at +0x00 and +0x08, which __Z13Banner_UpdateP8MgBanner "
     "and __Z11Banner_DrawP8MgBanner read, and both are flat f(self) tail jumps"),
    ("CDECL", r"^\?seats@\?1\?\?port_mg_framework_states_seat@@",
     "sMgFxUpdateTable and _020beb98: the readers are the matched TUs "
     "src/Mg_DrawFx.cpp and src/Mg_UpdateFx.cpp, each a "
     "flat f(self) that loads the pair out of the table and tail jumps with "
     "the frame restored (lane PMFB3 measured both listings)"),

    ("CDECL", r"^\?g_bp_cells@@",
     "BabyPenguin: the dispatchers are the host copies "
     "BabyPenguin_StateEnter.cpp and BabyPenguin_StateTick.cpp, which call "
     "the cell as a plain function pointer and DO push the receiver, so a "
     "thunk here breaks what works (00732a5ab left these alone on purpose)"),
    ("CDECL", r"^\?seats@\?1\?\?port_pathlift_states_seat@@",
     "dPathLiftActor_c: the three TICK halves already carry __fastcall faces; "
     "the three ENTER halves are read by NOTHING but _ZN16dPathLiftActor_c8SetStateEi, a "
     "flat C dispatcher f(self, state) that tail jumps, so [esp+4] really is "
     "the receiver there (proved by a byte scan of .text for every reference "
     "to _data_ov002_0210af2c+0/+4)"),
    ("CDECL", r"^\?g_piece_states@@",
     "BowserPuzzlePiece: the .b half already carries a __fastcall face; the "
     ".a half's only reader is _ZN18daWater_Hakidasi_c8SetStateEPNS_5StateE, a flat C dispatcher "
     "f(self, state) that tail jumps"),

    # ---- run link100 lane PMFSWEEP3 ---------------------------------------
    ("ECX", r"^\?g_rabbit_states@@",
     "daMip_c (MIPS the rabbit): the class dispatches its own state record and "
     "every one of its sixteen dispatch sites is a __thiscall member that "
     "pushes nothing -- ?Behavior@daMip_c@@UAEHXZ +0x2f6 mov [edi+0x364],"
     "0x18a6b2c; +0x300 mov eax,[0x18a6b2c]; +0x309 mov ecx,edi; +0x30b add "
     "ecx,[0x18a6b30]; +0x311 call eax, and the same three moves again in "
     "?InitResources +0x398, ?Render +0x22a, ?StateCaughtInit, "
     "?StateCaughtMain, ?StateFleeInit, ?StateFleeMain, ?StateReleasedMain, "
     "?StateRestInit, ?StateRestMain, ?StateSaveTalkMain, ?StateStartleInit, "
     "?StateStartleMain and ?StateTalkMain. __sinit_ov085_0212f5ec copies the "
     "seat's source records into the runtime ones those sites read, so the "
     "seat's word is the word they call"),
    ("ECX", r"^\?g_ov077_seats@",
     "ov077 HeaveHo's per-frame halves: ?Behavior@daPopoi_c@@UAEHXZ +0x51 mov "
     "ecx,[edi+0x3fc]; +0x5a mov eax,[ecx+8]; +0x61 mov ecx,[ecx+0xc]; +0x64 "
     "add ecx,edi; +0x66 call eax -- the dispatcher is INLINE in a member and "
     "pushes nothing. The rest of this table is the adjudicated __cdecl set "
     "below: the enter halves and Lakitu's and Spiny's tick halves are reached "
     "only by the flat f(self) tail jumps __ZN7daJgm_c10EnterStateEv (va 00529ab0), "
     "_02124718 (00529a90), _02125e5c (00529af0), _02125e20 (00529ad0) and "
     "_02126d5c (0052adc0), each `mov eax,[ebp+8] / mov ecx,[rec+4] / add "
     "ecx,eax / pop ebp / jmp`, so [esp+4] really is the receiver there",
     r"^(?:_func_ov077_|__ZN7daJgm_c|__ZN9daPopoi_c19func_ov077_)"
     r"(?!02126640|0212679c|02126ad0|02126a50|021269a8)"),
    ("ECX", r"^\?g_ov060_states@",
     "ov060's Bowser pack: the five tables dispatched by a matched TU's own "
     "pointer-to-member call (?Behavior and ?InitResources of BowserFire, "
     "?Behavior of BowserSkyPlatform, the Bowser tail, and since run linkfull "
     "lane SEATS3 retired HOST COPY 1, Bowser's 0211aeb4 through the matched "
     "__ZN7daKpa_c10BowserLoopEv, receiver in ecx) carry the ov60_* __fastcall faces "
     "and must keep them. The raw matched bodies in this table are the "
     "adjudicated __cdecl set below: __ZN7daKpa_c10FreeUpdateEv decodes 0211aed4's "
     "record by hand and dispatches it `push edx / call eax`, PUSHING the "
     "receiver, and ?Behavior@SpikeBomb reads 0211b1d8 as int[] and does the "
     "same",
     r"^(?:_func_ov060_|__ZN\d+\w+?_c\d+func_ov060_)(?!02115c1c|02115d50|02115d68|021167c8|021167ec|"
     r"021168c4|021169b0|021169f8|02116b18|02116b68|02116c68|02116d78|"
     r"02116f74|02116f90|021171e8|0211722c|0211747c|02117db8|021180e0|"
     r"021181b4|021128c0|02112724|021125f0)"),

    # g_scuttlebug_sources: row retired with f4ce6e510bb, which deleted the
    # Scuttlebug pointer-to-member seat (port/unmatched/Scuttlebug_StateDispatch
    # .cpp); the native PMF constants cover the table now and it is not in the
    # map. INT093F.
    ("CDECL", r"^\?g_crate_states@@",
     "Crate: the readers are __Z14Crate_SetStateP13daObjBlockS_ci (va 0052ce50, +0x21 jmp eax) and "
     "__ZN13daObjBlockS_c15CallStateUpdateEv (va 0052ce80, +0x1e jmp eax), both flat f(self) "
     "tail jumps that load the pair out of the table with the receiver in "
     "[ebp+8] and leave it at [esp+4]"),
    ("ECX", r"^\?g_ukiki_cells@@",
     "daMky_c: all twenty-two cells take their receiver in ecx. The eleven "
     "TICK cells go through ?Behavior@daMky_c@@UAEHXZ +0x1f5 mov eax,"
     "[edi+0x3a4]; +0x1fb mov ecx,[eax+0xc]; +0x1fe mov eax,[eax+8]; +0x201 "
     "add ecx,edi; +0x203 call eax, and the flat tail jumps "
     "__ZN7daMky_c11ChangeStateEi and _02114134, _02113324, _02113d20, _02113ff0 "
     "ride the receiver through while also setting ecx. The eleven ENTER "
     "cells were excused here as __cdecl on the reading that those flat "
     "jumps are their only readers. They are not: run link100 lane UKIKI2 "
     "read every absolute reference into data_ov030_02115e0c back out of "
     "the image and found ?InitResources@daMky_c@@UAEHXZ +0x1d0 and +0x205 "
     "inline two more dispatches, `lea ecx,[ecx+this]; call dword ptr "
     "[cell]` with nothing pushed -- the Scuttlebug shape 65deff04d fixed "
     "at the sibling class the same night. Both the flat dispatcher and the "
     "inlined calls set ecx, so the enter half is thunked too and this "
     "table leaves the exception list"),

    # ---- run linkfull lane PMFMG1: the minigame member-pointer rows -------
    ("ECX", r"^\?seats_ecx_a@\?1\?\?port_mg_base_writer_seat@@",
     "dMgState_c group A: the twenty pair globals the matched "
     "?SetState@dMgState_c@@QAEXH@Z builds sEnterTable from; it copies the "
     "chosen pair into the message object at +0x00 and dispatches it itself "
     "with `lea ecx,[edi+esi]; call edx`, nothing pushed, and +0x00 has no "
     "other reader (lane PMFMG1, run linkfull)"),
    ("ECX", r"^\?rec@\?1\?\?mem2_record_seat@@",
     "dScMgMemory2_c's record field: the ten ov004 pairs are copied into "
     "sMgCoins[i]+0 and read back by src/Coin_Update.cpp, "
     "`mov ecx,[eax+4]; add ecx,eax; mov eax,[eax]; pop ebp; jmp eax` -- a "
     "tail jump with the receiver in ecx and nothing pushed"),
    ("CDECL", r"^\?seats@\?1\?\?port_mg_flower_sub_seat@@",
     "dScMgFlower_c's object-head field: the five .data pairs (the sentinel "
     "0213aee0 included) are read by src/FlowerFace_Update.cpp, which "
     "open-codes the decode and calls `push ecx; call eax`, caller-cleaned"),
    ("CDECL", r"^\?g_trmpln_seats@@",
     "the trampoline Mario element field (+0x70 in dScMgTrmpln2Mario_c, +0x64 "
     "in dMgTrmpln3DMario_c): the thirty-eight .data pairs are read by the "
     "open-coded src/Trmp2Mario_Update.cpp and src/Trmp3DMario_Update.cpp, "
     "both `push ecx; call edx; add esp,4`"),

    # ---- run linkfull lane PMF2: the member-pointer rows, second wave ------
    # (Trampoline Time's g_tti_seats row went with its seat table in
    # catchup1 76c2448ec3b: the TU assigns &dScMgTrampoline_c::State members
    # directly, so there is no host record table left to check)
    ("ECX", r"^\?g_objcmd_seats@@",
     "the Kuppa script's second dispatcher: the twenty-four ov002 records are "
     "read only by src/func_ov002_020f7d74.cpp, which copies them into its "
     "function-local table on the first call and dispatches `push a3; push "
     "a2; push p+7; mov ecx,[tab+i*8+4]; add ecx,self; call [tab+i*8]`, "
     "callee-cleaned -- the first dispatcher's shape (port_kuppa_cmd_seat)"),
    ("ECX", r"^\?g_mb_cells@@",
     "MrBlizzard: the ten cells at data_ov081_02128e14, pointed to from "
     "[self+0x3f8]. ?Behavior@daSnowman_c@@UAEHXZ (src/_ZN10MrBlizzard8Behavior"
     "Ev.cpp) calls the tick half at +8 with ecx = this + delta and nothing "
     "pushed; __ZN11daSnowman_c8SetStateEPNS_5StateE tail jumps into the enter half with `mov "
     "ecx,[ecx+4]; add ecx,eax; pop ebp; jmp edx`. Every other reader "
     "compares the cell pointer by address"),

    # ---- run linkfull lane PMF3: the last member-pointer rows --------------
    ("ECX", r"^_g_pmf3_list_cells$",
     "the five actor list heads (data_020a4b6c/78/88/98/a8): port_actor_lists_"
     "seat writes these words into the callback pairs, and their only readers "
     "are the matched walks src/func_02043fdc.cpp and src/func_020441cc.cpp, "
     "which call `(node->obj->*thiz->callback)()` with ecx = obj + delta and "
     "nothing pushed"),
    ("ECX", r"^_data_02099e(74|7c|84|8c|94|9c|a4|ac|b4|bc|c4|cc)$",
     "the twelve virtual member-pointer records the four ActorBase::Process "
     "wrappers pass by value into src/_ZN7fBase_c7ProcessEMS_FivEMS_FbvEMS_"
     "FvjE.cpp, which calls each `(self->*pmf)()` / `(self->*pmf)(code)` with "
     "ecx = self + delta, the code pushed and callee-popped; hal/pmf3_cells.cpp"
     " hosts each as {face, 0}"),
    ("ECX", r"^_g_pmf3_player_state_faces$",
     "the 78 Player::State objects (ov002 0x0210ffec..0x02110724): "
     "port_player_states_seat writes these faces into their 185 cells, and "
     "the readers are src/_ZN6Player8BehaviorEv.cpp (`(this->*(st->mMain))()`)"
     " and src/_ZN6Player11ChangeStateERNS_5StateE.cpp (Cleanup and Init), "
     "both ecx = this + delta with nothing pushed; every other reader compares "
     "the State pointer by address"),
]


# ---- image ----------------------------------------------------------------
MAPROW = re.compile(
    r"\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8})\s+(\S*)\s*(\S+)?\s*$")


def sections(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    base = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    off = pe + 24 + optsz
    out = []
    for i in range(nsec):
        s = off + i * 40
        out.append((struct.unpack_from("<I", data, s + 12)[0],
                    struct.unpack_from("<I", data, s + 8)[0],
                    struct.unpack_from("<I", data, s + 20)[0],
                    struct.unpack_from("<I", data, s + 16)[0]))
    return base, out


class Image(object):
    def __init__(self, exe, mp):
        self.data = open(exe, "rb").read()
        self.base, self.secs = sections(self.data)
        self.syms = []
        base = 0x400000
        started = False
        with open(mp, encoding="utf-8", errors="replace") as f:
            for line in f:
                if "Preferred load address is" in line:
                    base = int(line.split()[-1], 16)
                if "Publics by Value" in line:
                    started = True
                    continue
                if not started:
                    continue
                m = MAPROW.match(line)
                if m:
                    obj = m.group(4) or m.group(3)
                    self.syms.append((int(m.group(2), 16) - base, m.group(1), obj))
        self.syms.sort()
        self.keys = [s[0] for s in self.syms]
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        va, vsz, raw, rsz = self.secs[0]
        self.text = (va, va + max(vsz, rsz))

    def off(self, rva):
        for va, vsz, raw, rsz in self.secs:
            if va <= rva < va + max(vsz, rsz):
                d = rva - va
                if d < rsz:
                    return raw + d
        return None

    def code(self, rva, n):
        o = self.off(rva)
        return b"" if o is None else self.data[o:o + n]

    def name_at(self, rva):
        i = bisect.bisect_right(self.keys, rva) - 1
        if i < 0:
            return ("?", "?", 0)
        r, n, o = self.syms[i]
        return (n, o, rva - r)


# ---- the classifier -------------------------------------------------------
STACKOP = re.compile(r"\[ebp \+ 8\]|\[esp \+ 4\]")
ECXWORD = re.compile(r"\becx\b")
READS_ITS_OPERAND = ("push", "jmp", "call", "test", "cmp")
KILLS_ITS_FIRST = ("mov", "lea", "pop", "movzx", "movsx", "xor")


def receiver_of_bytes(md, blob, rva, limit=60):
    stack_read = False
    ecx_dead = False
    saw_call = False
    for i in list(md.disasm(blob, rva))[:limit]:
        ops = i.op_str
        if STACKOP.search(ops):
            stack_read = True
        if ECXWORD.search(ops) and not ecx_dead:
            if i.mnemonic in READS_ITS_OPERAND:
                return "ECX"
            if ops.split(",")[0].strip() != "ecx":
                return "ECX"
        if ops.split(",")[0].strip() == "ecx" and i.mnemonic in KILLS_ITS_FIRST:
            ecx_dead = True
        if i.mnemonic == "ret":
            break
        # A TAIL JUMP ENDS THE BODY (run link100 lane PMFSWEEP2). Without this
        # the walk ran off the end of a short forwarder, through the int3
        # padding and into whatever function the linker put next, and read that
        # one's prologue instead. It was not academic: every one of the six
        # counting wrappers in port/unmatched/MgBase_StateSetter.cpp is
        # `push ebp; mov ebp,esp; inc <counter>; pop ebp; jmp <body>`, reads
        # nothing itself, and classified as ECX purely because the NEXT wrapper
        # in the image opens with `push ecx`. A forwarder like that in an ECX
        # table is the defect this guard exists for -- it hands the body a
        # stack nobody wrote -- so reading it as ECX is exactly the wrong
        # answer. The caller re-walks at the target, which is what a tail jump
        # means.
        #
        # ONLY WHEN THE BODY IS A PURE FORWARDER, though, and that qualifier is
        # a measured one rather than caution: ?port_bird_state2@@YIXPAX0@Z is a
        # __fastcall refusal stub that ignores its receiver, prints, and tail
        # jumps into abort() -- and abort's own prologue reads [esp+4], so
        # following that jump reported the stub as a stack reader and refused a
        # build that is correct. A body that has already made a call has used
        # its frame and is not handing a receiver on, so the walk stops there
        # with what it saw, which for that stub is NEITHER.
        if i.mnemonic == "call":
            saw_call = True
        if i.mnemonic == "jmp":
            if re.match(r"^0x[0-9a-f]+$", ops) and not saw_call:
                return ("STACK" if stack_read else "TAILJUMP:" + ops)
            break   # an indirect or post-call tail jump: nothing to follow
    return "STACK" if stack_read else "NEITHER"


def receiver_of(img, rva, hops=4):
    """The receiver class of the body at rva, following tail jumps.

    A tail jump is a continuation: the body that ends in one takes its receiver
    however the body it jumps to takes it, because the frame is handed over
    untouched. Bounded at four hops so a jump cycle cannot spin."""
    seen = set()
    for _ in range(hops):
        if rva in seen:
            break
        seen.add(rva)
        k = receiver_of_bytes(img.md, img.code(rva, 760), rva)
        if not k.startswith("TAILJUMP:"):
            return k
        nxt = int(k.split(":", 1)[1], 16)
        if not (img.text[0] <= nxt < img.text[1]):
            return "NEITHER"
        rva = nxt
    return "NEITHER"


# ---- the census -----------------------------------------------------------
def cells_of(img, sym_rva, end_rva):
    """Every code word stored in [sym_rva, end_rva)."""
    out = []
    off = img.off(sym_rva)
    if off is None:
        return out
    n = end_rva - sym_rva
    blob = img.data[off:off + n]
    for k in range(0, len(blob) - 3, 4):
        w = struct.unpack_from("<I", blob, k)[0]
        if w < img.base:
            continue
        r = w - img.base
        if img.text[0] <= r < img.text[1]:
            out.append((sym_rva + k, r))
    return out



def ledger():
    """The ledger rows, normalised to (kind, pattern, why, cdecl_ok).

    cdecl_ok is the fourth field an ECX row may carry (run link100 lane
    PMFSWEEP3): a regex over BODY symbol names whose cells are the adjudicated
    __cdecl exceptions INSIDE a checked table.  Several ROM tables mix the two
    conventions because two dispatchers read the same array -- a flat f(self)
    tail jump for one half, an inlined member for the other -- and a table with
    no way to say so could only be left out of the ledger entirely or added as
    a blanket CDECL row, which stops checking the half that must stay in ECX.
    The exception is spelled as EXACT body names (or a negative lookahead over
    them) rather than a family prefix on purpose: dropping a __fastcall face
    back to its raw body renames the cell's body to something the list does not
    allow, so that regression still refuses."""
    for row in LEDGER:
        kind, pat, why = row[0], row[1], row[2]
        yield kind, pat, why, (row[3] if len(row) > 3 else None)


def cdecl_ok_for(name):
    for kind, pat, why, ok in ledger():
        if re.match(pat, name):
            return ok
    return None


def refusals(rows):
    """Split census rows into (refused, excused-by-name, adjudicated tables).

    Factored out of main so the self-test can drive it: gutting the criterion
    is caught by the prologue fixtures, and gutting THIS is caught by the
    verdict fixtures below."""
    bad, excused = [], []
    for r in rows:
        if r[0] != "ECX" or r[6] != "STACK":
            continue
        ok = cdecl_ok_for(r[1])
        if ok and re.match(ok, r[5][0]):
            excused.append(r)
        else:
            bad.append(r)
    return bad, excused

def census(img):
    datasyms = [(r, n, o) for r, n, o in img.syms
                if not (img.text[0] <= r < img.text[1])]
    datasyms.sort()
    rows = []
    for idx, (rva, name, obj) in enumerate(datasyms):
        for kind, pat, why, ok in ledger():
            if re.match(pat, name):
                end = rva + 0x400
                for j in range(idx + 1, len(datasyms)):
                    if datasyms[j][0] > rva:
                        end = datasyms[j][0]
                        break
                for at, target in cells_of(img, rva, end):
                    rows.append((kind, name, obj, at, target,
                                 img.name_at(target), receiver_of(img, target),
                                 why))
                break
    return rows


# ---- the self-test --------------------------------------------------------
# Three real prologues, as bytes, so the check is proved without the image.
FLAT_FACE = bytes(bytearray([
    0x55,                          # push ebp
    0x8B, 0xEC,                    # mov ebp, esp
    0x8B, 0x4D, 0x08,              # mov ecx, dword ptr [ebp+8]
    0x5D,                          # pop ebp
    0xE9, 0x00, 0x00, 0x00, 0x00,  # jmp <member>
]))
CDECL_BODY = bytes(bytearray([
    0x55,                          # push ebp
    0x8B, 0xEC,                    # mov ebp, esp
    0x56,                          # push esi
    0x8B, 0x75, 0x08,              # mov esi, dword ptr [ebp+8]
    0xC3,                          # ret
]))
FASTCALL_THUNK = bytes(bytearray([
    0x51,                          # push ecx
    0xE8, 0x00, 0x00, 0x00, 0x00,  # call <face>
    0x83, 0xC4, 0x04,              # add esp, 4
    0xC3,                          # ret
]))
FASTCALL_WITH_STACK_ARG = bytes(bytearray([
    0x55,                          # push ebp
    0x8B, 0xEC,                    # mov ebp, esp
    0xFF, 0x75, 0x08,              # push dword ptr [ebp+8]   the int argument
    0x51,                          # push ecx                 the receiver
    0xE8, 0x00, 0x00, 0x00, 0x00,  # call <body>
    0xC2, 0x04, 0x00,              # ret 4
]))
THISCALL_MEMBER = bytes(bytearray([
    0x8B, 0xC1,                    # mov eax, ecx
    0x8B, 0x50, 0x10,              # mov edx, dword ptr [eax+0x10]
    0xC3,                          # ret
]))
# A counting wrapper that reads NOTHING of its own and tail jumps, the shape
# every bw_ row in port/unmatched/MgBase_StateSetter.cpp has. Its receiver is
# whatever the body it jumps to takes, so the classifier must report the jump
# rather than walk off the end of it into the next function in the image.
TAILJUMP_WRAPPER = bytes(bytearray([
    0x55,                          # push ebp
    0x8B, 0xEC,                    # mov ebp, esp
    0xFF, 0x05, 0x00, 0x10, 0x40, 0x00,   # inc dword ptr [0x401000]
    0x5D,                          # pop ebp
    0xE9, 0xFB, 0x0F, 0x00, 0x00,  # jmp 0x402010
]))

SELFTESTS = [
    ("a flat C face from hal/faces_sync_gen.cpp", FLAT_FACE, "STACK"),
    ("a raw matched cdecl body", CDECL_BODY, "STACK"),
    ("the __fastcall thunk this campaign seats", FASTCALL_THUNK, "ECX"),
    ("a __fastcall face that also takes a stack argument",
     FASTCALL_WITH_STACK_ARG, "ECX"),
    ("a __thiscall member", THISCALL_MEMBER, "ECX"),
    ("a counting wrapper that tail jumps", TAILJUMP_WRAPPER, "TAILJUMP:0x40200a"),
]



# ---- the verdict fixtures -------------------------------------------------
# The prologue fixtures above prove the CRITERION.  These prove the WIRING:
# that a stack-receiver cell in a checked table actually reaches the refusal,
# that the fourth-field exception excuses only the body it names, and that a
# CDECL table is reported rather than refused.  Without them, emptying the
# refusal list in main() left every prologue fixture green and shipped a build
# with a raw cell in it (measured, run link100 lane PMFSWEEP3).
def _row(kind, table, body, klass):
    return (kind, table, "fixture.obj", 0x1000, 0x2000, (body, "fixture.obj", 0),
            klass, "fixture")


VERDICTS = [
    ("a stack-receiver cell in a checked table refuses",
     [_row("ECX", "?g_rabbit_states@@3QBU", "_func_ov085_0212b4b4", "STACK")], 1, 0),
    ("an ECX cell in a checked table passes",
     [_row("ECX", "?g_rabbit_states@@3QBU", "?rb_0212b4b4@@YIXPAX0@Z", "ECX")], 0, 0),
    ("the named __cdecl exception inside a checked table is excused",
     [_row("ECX", "?g_ov077_seats@?A0x1@@3QBU", "__ZN7daJgm_c15UpdateSpitStateEv", "STACK")], 0, 1),
    ("a body the exception does NOT name still refuses",
     [_row("ECX", "?g_ov077_seats@?A0x1@@3QBU", "__ZN9daPopoi_c15UpdateGrabStateEv", "STACK")], 1, 0),
    ("a stack-receiver cell in an adjudicated table is only reported",
     [_row("CDECL", "?g_bp_cells@@3QBU", "__ZN9daPgBby_c22StateKeepDistanceEnterEv", "STACK")], 0, 0),
]


def verdict_selftest():
    bad = 0
    for what, rows, want_bad, want_excused in VERDICTS:
        got_bad, got_excused = refusals(rows)
        ok = len(got_bad) == want_bad and len(got_excused) == want_excused
        print("  %s %-58s want %d/%d got %d/%d" % (
            "ok " if ok else "FAIL", what, want_bad, want_excused,
            len(got_bad), len(got_excused)))
        if not ok:
            bad += 1
    if bad:
        print("pmf_guard --selftest: %d of %d verdicts are WRONG, so the guard "
              "would not refuse what it classifies" % (bad, len(VERDICTS)))
    return bad


def selftest():
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    bad = 0
    for what, blob, want in SELFTESTS:
        got = receiver_of_bytes(md, blob, 0x401000)
        ok = "ok " if got == want else "FAIL"
        print("  %s %-52s want %-7s got %s" % (ok, what, want, got))
        if got != want:
            bad += 1
    if bad:
        print("pmf_guard --selftest: %d of %d classifications are WRONG, so the "
              "guard cannot see the defect it exists for" % (bad, len(SELFTESTS)))
        return 1
    if verdict_selftest():
        return 1
    if dispatch_selftest(md):
        return 1
    nbad = 0
    for name, want in ECX_NAMES:
        got = bool(ECX_BY_NAME.search(name))
        print("  %s %-52s want %-5s got %s" % ("ok " if got == want else "FAIL",
                                              name[:52], want, got))
        nbad += got != want
    if nbad:
        print("pmf_guard --selftest: %d of %d calling-convention names are "
              "WRONG" % (nbad, len(ECX_NAMES)))
        return 1
    print("pmf_guard --selftest OK: %d prologues, %d verdicts, %d dispatch "
          "shapes and %d convention names, every one as the campaign's own "
          "evidence says" % (len(SELFTESTS), len(VERDICTS), len(DISPATCHES),
                             len(ECX_NAMES)))
    return 0


# names whose decoration alone says ECX (see ECX_BY_NAME), and the flat and
# cdecl spellings that must still be judged by their prologue
ECX_NAMES = [
    ("?Pop12_Finish@dScMgSound_c@@QAEXH@Z", True),          # public __thiscall
    ("?func_ov002_020f2d70@StaffRoll@daDemo_c@@QAEXH@Z", True),
    ("?Behavior@daPly_c@@UAEHXZ", True),                    # virtual __thiscall
    ("@func_ov002_020e200c@4", True),                       # C __fastcall
    ("?f@@YIXPAX@Z", True),                                 # free __fastcall
    ("_func_ov002_020e200c", False),                        # C __cdecl
    ("?InitStateSwim@daShark_c@@SAHPAU1@@Z", False),        # static __cdecl
    ("?func_ov002_020f9468@@YAHPAUdaSoundObj_c@@PAG@Z", False),
    ("__ZN12daSnowball_c10InitState0Ev", False),            # flat face
]


# ---- THE DERIVED CENSUS (run link100 lane PMFCELLS1) ----------------------
#
# The ledger above is a hand-written list, and a seat table nobody thought to
# add to it is not checked at all.  That is not hypothetical: the Amp's six
# state cells (hal/actor_classes_ov070.cpp) held plain cdecl bodies in a
# /vmg /vmm dispatcher's records through five folds while this file walked
# fourteen tables, none of them that one, and the headline read OK every time.
#
# So the check below does not ask which tables to walk.  It asks the IMAGE
# which records are dispatched with the receiver in ECX, and then whether the
# port seats a stack-receiver body into any of them:
#
#   1. one pass over .text finds every MSVC pointer-to-member dispatch whose
#      record sits at a fixed address --
#          mov  <r>, [REC]        (test <r>,<r> / je)
#          mov  ecx, [REC+4]
#          add/lea ecx, <this>
#          call/jmp <r>           with NOTHING pushed
#      -- and, in the same pass, every static-initialiser copy
#          mov <r>, [SRC] ... mov [LIVE], <r>
#      so a live cell can be traced back to the source record a seat writes;
#   2. a data symbol holding a pointer to one of those records is a seat table;
#   3. a face in such a table whose prologue reads its receiver off the stack
#      is the defect, and the guard refuses.
#
# Two more shapes since catchup1 Stage D (PLY4's row; ENG4/PLY4 fix
# a9b9efbad08). Four free cdecl player handlers sat in daPly_c's member-pointer
# tables through every fold with this check reading OK, because neither of the
# two steps above could see them:
#
#   1b. the INDEXED dispatch, `(this->*table[i])()` --
#          mov  ecx, [i*8 + TABLE+4]
#          mov  <r>, [i*8 + TABLE]
#          add  ecx, <this>
#          call <r>                 with nothing pushed
#       (St_Jump_Main, St_LevelEnter_Main, St_NoControl_Init, St_WallJump_Main
#       on data_ov002_0211073c / 0211075c / 02110884). The whole table is
#       ECX-dispatched: every 8-byte record in its extent is checked.
#   2b. a live cell the TU's OWN static initialiser fills, from its own static
#       data rather than a port seat table: _PlayerInitStates copies the
#       PlayerFreeVoidPmf/PlayerFreeCharPmf unions (`mov eax,[data_ov002_02109f0c]
#       / mov [data_ov002_0211073c],eax`), and a store of an immediate code
#       address counts the same. The word the copy chain ends at, or the
#       live cell's own word when the image already holds one, is the body.
#
# ONCE A TABLE HAS ONE ECX-DISPATCHED RECORD, EVERY CELL IN IT IS CHECKED.  A
# seat writes one array and the class dispatches all of it the same way, and
# the campaign's own rule -- the note over the FlyGuy block in
# hal/actor_classes_ov070.cpp -- is that the face goes on every row rather than
# on the row that happened to fault, because the thunk is right whether the
# transfer is a call or a tail jump.  Escalating to the table is also what lets
# the check see the cells whose dispatch computes the record in a register
# (`lea eax,[eax*16+table]; call [eax]`), which no fixed-address scan can
# attribute on its own.
#
# The exceptions are the ledger's own: a CDECL row, or an ECX row's fourth
# field, excuses a table here too, plus the list below for what neither covers.
# A row here is a KNOWN stack-receiver face that some other branch already
# fixes, kept out of the refusal so this branch still builds while the guard
# keeps refusing anything NEW.  It is a baseline, not an adjudication: the
# reason has to name where the real fix lives, and when a baseline row stops
# firing the guard says the baseline can be tightened.  Same shape as
# alternatename_guard's and inferred_stub_guard's baselines.
DERIVED_BASELINE = [
    # (the Amp's ov70_seat_state_pmfs row went once the fold carried its six
    # __fastcall thunks: it had stopped firing by p4-fold7)
    #
    # (the three tables the indexed / own-initialiser shapes first found went
    # once their fixes landed: daSoundObj_c data_ov002_0211110c fa9da79fa7c and
    # daShark_c's SharkStateTable 380589abb60 (ACT4a), daSnowball_c
    # data_ov081_02128eb4 b468baca8f3 (ACT4b))
]

DERIVED_OK = [
    (r"^\?(?:g_states@@|g_chuckya_states@@|rows@\?1\?\?port_koopa_shell_states_seat@@|"
     r"g_ov090_seats@|g_mri_sources@@|g_rabbit_key_states@@|g_ov091_seats@|"
     r"g_water_ring_states@@|g_ov064_gate178_sources@@|g_unchained_chomp_states@@)",
     r".*",
     "SOURCE tables of seats that write PORT_PMF_TC(host) into the live record "
     "(hal/pmf_tc.h, PORTSYNC3 d87ca866f19): main's class TUs dispatch their "
     "state records natively, ECX = this and nothing pushed, so a seat that "
     "wrote the flat cdecl body into a record would read garbage; since "
     "PORTSYNC3 every seat below writes a thiscall stub over the flat body "
     "instead (or, where the mount word is not a DS code address, skips the "
     "record because the class TU owns the live table). The cells this scan "
     "reads are the flat bodies those stubs wrap, never a live record cell. "
     "Read per file, INT093F: LakituBro_Behavior, SignPost_StateDispatch, "
     "KoopaChuckya_StateDispatch, KoopaShell_States, MrI_StateDispatch, "
     "Ov064_WaterRing, Ov064Gate178_States and UnchainedChomp_Behavior each "
     "have one PORT_PMF_TC seat; hal/actor_classes_ov090.cpp and "
     "actor_classes_ov091.cpp the same in their seat loops; hal/actor_overlays"
     ".cpp seats g_rabbit_key_states through PORT_PMF_TC at its loop"),
    (r"^_vs_data_patch$", r".*",
     "ov075's patch table is GENERATED and carries the ROM's own words.  The "
     "three of them that are pointer-to-member records are overwritten at boot "
     "with vs_pmf_grid_init / vs_pmf_grid_render / vs_pmf_entry_walk in "
     "hal/scene_vs_menu.cpp, so the word this scan reads is the first writer's "
     "and not the one that is live"),
]

DATASKIP = re.compile(r"^\?\?_C@|^__ZTV|^\?\?_7|^\?\?_R|\$initializer\$")
ABSMEM = re.compile(r"^(e[a-z][a-z]), dword ptr \[(0x[0-9a-f]+)\]$")
ABSSTORE = re.compile(r"^dword ptr \[(0x[0-9a-f]+)\], (e[a-z][a-z])$")
ABSCALL = re.compile(r"^dword ptr \[(0x[0-9a-f]+)\]$")
# the indexed record read: `mov <r>, dword ptr [<i>*8 + TABLE]` (8 = one MSVC
# pointer-to-member record, code word then this-adjustment)
IDXMEM = re.compile(r"^(e[a-z][a-z]), dword ptr \[(e[a-z][a-z])\*8 \+ (0x[0-9a-f]+)\]$")
IDXCALL = re.compile(r"^dword ptr \[(e[a-z][a-z])\*8 \+ (0x[0-9a-f]+)\]$")
IMMSTORE = re.compile(r"^dword ptr \[(0x[0-9a-f]+)\], (0x[0-9a-f]+)$")
WRITES_FIRST = ("mov", "lea", "pop", "xor", "movzx", "movsx", "add", "sub",
                "and", "or", "shl", "shr", "sar", "imul", "inc", "dec",
                "neg", "not", "adc", "sbb", "setne", "sete")


def derived_ok(table, body):
    for tp, bp, why in DERIVED_OK:
        if re.match(tp, table) and re.match(bp, body):
            return why
    return None


def derived_baseline(table, body):
    for n, (tp, bp, why) in enumerate(DERIVED_BASELINE):
        if re.match(tp, table) and re.match(bp, body):
            return n, why
    return None, None


def dispatch_scan(md, blob, rva, ecx_sites, copies, ecx_tables=None,
                  consts=None):
    """One function's worth of instructions, folded into the answers.

    ecx_sites[record] gains the address of every transfer whose target came out
    of [record] while ecx carried [record+4] and nothing had been pushed;
    ecx_tables[table] the same for the indexed form, [i*8+table] and
    [i*8+table+4] with one index register; copies[dest] gains the source of
    every `mov <r>,[SRC] ... mov [DEST],<r>`, which is how a static initialiser
    fills a live cell from a seat's (or its own TU's) source record; and
    consts[dest] the value of every `mov dword ptr [DEST], imm`."""
    held, ecxrec, idxheld, ecxidx = {}, set(), {}, set()
    for i in md.disasm(blob, rva):
        ops = i.op_str
        m = ABSMEM.match(ops)
        if i.mnemonic == "mov" and m:
            reg, addr = m.group(1), int(m.group(2), 16)
            if reg == "ecx":
                ecxrec.add(addr - 4)
            else:
                held[reg] = addr
                idxheld.pop(reg, None)
            continue
        m = IDXMEM.match(ops)
        if i.mnemonic == "mov" and m:
            reg, idx, addr = m.group(1), m.group(2), int(m.group(3), 16)
            if reg == "ecx":
                ecxidx.add((idx, addr - 4))
            else:
                idxheld[reg] = (idx, addr)
                held.pop(reg, None)
            continue
        m = ABSSTORE.match(ops)
        if i.mnemonic == "mov" and m and m.group(2) in held:
            copies[int(m.group(1), 16)] = held[m.group(2)]
            continue
        m = IMMSTORE.match(ops)
        if i.mnemonic == "mov" and m and consts is not None:
            consts[int(m.group(1), 16)] = int(m.group(2), 16)
            continue
        if i.mnemonic in ("call", "jmp"):
            tgt = ops.strip()
            rec = held.get(tgt)
            if rec is None:
                mm = ABSCALL.match(tgt)
                rec = int(mm.group(1), 16) if mm else None
            if rec is not None and rec in ecxrec:
                ecx_sites.setdefault(rec, []).append(i.address)
            ix = idxheld.get(tgt)
            if ix is None:
                mm = IDXCALL.match(tgt)
                ix = (mm.group(1), int(mm.group(2), 16)) if mm else None
            if ix is not None and ix in ecxidx and ecx_tables is not None:
                ecx_tables.setdefault(ix[1], []).append(i.address)
        if i.mnemonic in ("push", "call"):
            # a push between the ecx write and the transfer means the receiver
            # went on the stack after all; a call ends the window either way
            held, ecxrec, idxheld, ecxidx = {}, set(), {}, set()
            continue
        # ONLY an instruction that WRITES its first operand kills the register.
        # The ROM's own null guard is `test <r>,<r> / je`, and counting test as
        # a write let the Amp's and Eyerok's dispatches walk straight past this
        # check while Goomboss's, which has no null guard, was caught: the
        # fixture below is that regression.
        if i.mnemonic in WRITES_FIRST:
            dst = ops.split(",")[0].strip()
            held.pop(dst, None)
            idxheld.pop(dst, None)


def text_pass(img):
    """One walk of .text: (ECX-dispatched records, live record -> source,
    ECX-dispatched indexed tables, live cell -> immediate).

    Decoding restarts at every symbol the map names, so the instruction stream
    is the real one rather than a resynchronisation out of mid-operand."""
    lo, hi = img.text
    blob = img.data[img.off(lo):img.off(lo) + (hi - lo)]
    starts = sorted(set(r for r, n, o in img.syms if lo <= r < hi))
    ecx_sites, copies, ecx_tables, consts = {}, {}, {}, {}
    for k, s in enumerate(starts):
        e = starts[k + 1] if k + 1 < len(starts) else hi
        if e - s > 0x4000:
            e = s + 0x4000
        dispatch_scan(img.md, blob[s - lo:e - lo], s, ecx_sites, copies,
                      ecx_tables, consts)
    return ecx_sites, copies, ecx_tables, consts


# ---- the dispatch fixtures ------------------------------------------------
# Four real shapes, as bytes, so the scan above is proved without an image.
# The record is 0x900000 and up; the fixtures are what the classes in this
# file's comments actually emit.
EYEROK_CALL = bytes(bytearray([
    0x8B, 0x15, 0x00, 0x00, 0x90, 0x00,   # mov edx, dword ptr [0x900000]
    0x85, 0xD2,                           # test edx, edx     the ROM null guard
    0x74, 0x08,                           # je  +8
    0x8B, 0x0D, 0x04, 0x00, 0x90, 0x00,   # mov ecx, dword ptr [0x900004]
    0x03, 0xC8,                           # add ecx, eax      this + delta
    0xFF, 0xD2,                           # call edx          nothing pushed
]))
GOOMBOSS_TAILJUMP = bytes(bytearray([
    0x8B, 0x0D, 0x04, 0x01, 0x90, 0x00,   # mov ecx, dword ptr [0x900104]
    0x03, 0xCE,                           # add ecx, esi
    0xFF, 0x25, 0x00, 0x01, 0x90, 0x00,   # jmp dword ptr [0x900100]
]))
PUSHED_RECEIVER = bytes(bytearray([
    0x8B, 0x05, 0x00, 0x02, 0x90, 0x00,   # mov eax, dword ptr [0x900200]
    0x8B, 0x0D, 0x04, 0x02, 0x90, 0x00,   # mov ecx, dword ptr [0x900204]
    0x03, 0xCE,                           # add ecx, esi
    0x56,                                 # push esi          THE RECEIVER
    0xFF, 0xD0,                           # call eax
]))
SINIT_COPY = bytes(bytearray([
    0x8B, 0x05, 0x00, 0x03, 0x90, 0x00,   # mov eax, dword ptr [0x900300]
    0x89, 0x05, 0x00, 0x03, 0x91, 0x00,   # mov dword ptr [0x910300], eax
]))

# daPly_c::St_Jump_Main's (this->*data_ov002_0211073c[i])(), as MSVC emits it
PLAYER_INDEXED = bytes(bytearray([
    0x8B, 0x0C, 0xC5, 0x04, 0x04, 0x90, 0x00,   # mov ecx, dword ptr [eax*8 + 0x900404]
    0x8B, 0x04, 0xC5, 0x00, 0x04, 0x90, 0x00,   # mov eax, dword ptr [eax*8 + 0x900400]
    0x03, 0xCE,                                 # add ecx, esi
    0xFF, 0xD0,                                 # call eax          nothing pushed
]))
INDEXED_PUSHED = bytes(bytearray([
    0x8B, 0x0C, 0xC5, 0x04, 0x05, 0x90, 0x00,   # mov ecx, dword ptr [eax*8 + 0x900504]
    0x8B, 0x04, 0xC5, 0x00, 0x05, 0x90, 0x00,   # mov eax, dword ptr [eax*8 + 0x900500]
    0x56,                                       # push esi          THE RECEIVER
    0xFF, 0xD0,                                 # call eax
]))
INDEXED_TWO_INDICES = bytes(bytearray([
    0x8B, 0x0C, 0xDD, 0x04, 0x06, 0x90, 0x00,   # mov ecx, dword ptr [ebx*8 + 0x900604]
    0x8B, 0x04, 0xC5, 0x00, 0x06, 0x90, 0x00,   # mov eax, dword ptr [eax*8 + 0x900600]
    0x03, 0xCE,                                 # add ecx, esi
    0xFF, 0xD0,                                 # call eax          not one record
]))
# _PlayerInitStates: a TU's own union copied into the live table, and the
# same filled from an immediate
OWN_SINIT = bytes(bytearray([
    0x8B, 0x05, 0x00, 0x07, 0x90, 0x00,   # mov eax, dword ptr [0x900700]
    0x83, 0xC4, 0x18,                     # add esp, 0x18
    0x89, 0x05, 0x00, 0x04, 0x90, 0x00,   # mov dword ptr [0x900400], eax
    0xC7, 0x05, 0x08, 0x04, 0x90, 0x00,   # mov dword ptr [0x900408], 0x401234
    0x34, 0x12, 0x40, 0x00,
]))

# (what, bytes, ECX records, ECX tables, copies, immediates)
DISPATCHES = [
    ("a call through the record with the ROM's null guard", EYEROK_CALL,
     [0x900000], [], {}, {}),
    ("a tail jump through the record", GOOMBOSS_TAILJUMP, [0x900100], [], {},
     {}),
    ("a dispatcher that pushes the receiver", PUSHED_RECEIVER, [], [], {}, {}),
    ("a static initialiser filling a live cell", SINIT_COPY, [], [],
     {0x910300: 0x900300}, {}),
    ("the indexed (this->*table[i])() dispatch", PLAYER_INDEXED, [],
     [0x900400], {}, {}),
    ("an indexed dispatch that pushes the receiver", INDEXED_PUSHED, [], [],
     {}, {}),
    ("code and adjustment read with two indices", INDEXED_TWO_INDICES, [], [],
     {}, {}),
    ("the TU's own initialiser: a copy and an immediate", OWN_SINIT, [], [],
     {0x900400: 0x900700}, {0x900408: 0x401234}),
]


def dispatch_selftest(md):
    bad = 0
    for what, blob, want_recs, want_tabs, want_copies, want_consts in DISPATCHES:
        sites, copies, tabs, consts = {}, {}, {}, {}
        dispatch_scan(md, blob, 0x401000, sites, copies, tabs, consts)
        got = sorted(sites) + sorted(tabs)
        want = sorted(want_recs) + sorted(want_tabs)
        ok = (sorted(sites) == sorted(want_recs) and sorted(tabs) == sorted(want_tabs)
              and copies == want_copies and consts == want_consts)
        print("  %s %-52s want %-18s got %s" % (
            "ok " if ok else "FAIL", what,
            [hex(x) for x in want] or "no record",
            [hex(x) for x in got] or "no record"))
        if not ok:
            bad += 1
    if bad:
        print("pmf_guard --selftest: %d of %d dispatch shapes are WRONG, so "
              "the derived census cannot see the records it exists to find"
              % (bad, len(DISPATCHES)))
    return bad


def data_extents(img):
    ds = [(r, n, o) for r, n, o in img.syms
          if not (img.text[0] <= r < img.text[1])]
    ds.sort()
    out = []
    for k, (rva, name, obj) in enumerate(ds):
        end = rva + 0x800
        for j in range(k + 1, len(ds)):
            if ds[j][0] > rva:
                end = min(end, ds[j][0])
                break
        out.append((rva, end, name, obj))
    return out


def seat_tables(img, extents):
    """{pointer value: [(table, obj)]} for every data symbol that also holds at
    least one code word -- the shape every seat table in port/ has."""
    out = {}
    for rva, end, name, obj in extents:
        if DATASKIP.search(name):
            continue
        # romdata.c.obj holds the ROM's own DS-side bytes, never a seat: its
        # words are packed script fields, DS addresses (0x02xxxxxx) and plain
        # numbers. One of them equal to an x86 text address (or to the data
        # address of an ECX-dispatched record) in some build layout is a
        # coincidence, and would refuse a good build (sEndingPeachDescentCamPosPoints+0x10 ==
        # ppu_compose_stacked+0x472 in the TWODGPU1 layout).
        if obj and obj.startswith("romdata.c"):
            continue
        o = img.off(rva)
        if o is None:
            continue
        blob = img.data[o:o + (end - rva)]
        words = [struct.unpack_from("<I", blob, x)[0]
                 for x in range(0, len(blob) - 3, 4)]
        if not any(w >= img.base and img.text[0] <= w - img.base < img.text[1]
                   for w in words):
            continue
        for w in words:
            if w >= img.base and not (img.text[0] <= w - img.base < img.text[1]):
                out.setdefault(w, []).append((name, obj))
    return out


# A body whose own MSVC name says __thiscall or __fastcall takes its receiver in
# ECX by the compiler's contract, whatever its prologue reads: a member that
# ignores `this` and reads its first argument at [ebp+8] looks like a stack
# receiver to receiver_of (StaffRoll's QAEXH states, dScMgSound_c's PopNN
# states under their _port_pmft_ aliases). Only the derived census meets real
# members; the ledger's tables hold flat faces.
ECX_BY_NAME = re.compile(r"@@[ABEFIJMNQRUV][A-D][EI]|@@[CDKLSTYZ]I|^@[^@]+@\d+$")


def ecx_by_name(img, rva):
    """The name(s) the map gives rva, when one of them is a decorated
    __thiscall/__fastcall function; else None."""
    lo = bisect.bisect_left(img.keys, rva)
    hi = bisect.bisect_right(img.keys, rva)
    for r, n, o in img.syms[lo:hi]:
        if ECX_BY_NAME.search(n):
            return n
    return None


def code_word(img, va):
    """The code address held in the image at va, as an rva, or None."""
    rva = va - img.base
    o = img.off(rva)
    if o is None or o + 4 > len(img.data):
        return None
    w = struct.unpack_from("<I", img.data, o)[0]
    if w >= img.base and img.text[0] <= w - img.base < img.text[1]:
        return w - img.base
    return None


def live_cell_bodies(img, cell, copies, consts):
    """[(body rva, via)] for one ECX-dispatched live cell (a VA): its own word
    when the image holds code there, the immediate an initialiser stores into
    it, and the code word at the end of its copy chain (catchup1 shape 2b)."""
    out = []
    w = code_word(img, cell)
    if w is not None:
        out.append((w, "the image's own word"))
    v = consts.get(cell)
    if v is not None and v >= img.base and img.text[0] <= v - img.base < img.text[1]:
        out.append((v - img.base, "an initialiser's immediate store"))
    chain, cur = [cell], cell
    for _ in range(4):
        nxt = copies.get(cur)
        if nxt is None or nxt in chain:
            break
        chain.append(nxt)
        cur = nxt
        w = code_word(img, cur)
        if w is not None:
            src = img.name_at(cur - img.base)
            out.append((w, "copied from %s+0x%x [%s]" % (src[0], src[2], src[1])))
    return out


def derived_refusals(img):
    """Stack-receiver faces in the tables the image says are ECX-dispatched."""
    ecx_sites, copies, ecx_tables, consts = text_pass(img)
    extents = data_extents(img)
    seats = seat_tables(img, extents)
    flagged = {}
    for rec, sites in ecx_sites.items():
        chain, cur = [rec], rec
        for _ in range(4):
            nxt = copies.get(cur)
            if nxt is None or nxt in chain:
                break
            chain.append(nxt)
            cur = nxt
        for a in chain:
            for name, obj in seats.get(a, []):
                t = flagged.setdefault(name, {"obj": obj, "recs": set(),
                                              "sites": []})
                t["recs"].add(rec)
                for at in sites[:2]:
                    if len(t["sites"]) < 4:
                        t["sites"].append(at)
    # the live cells themselves (shapes 1b and 2b): every record of an indexed
    # table's extent, and each fixed record, read through its own initialiser
    live = {}
    by_start = dict((img.base + rva, (img.base + end, name, obj))
                    for rva, end, name, obj in extents)
    for tab, sites in ecx_tables.items():
        start = tab - img.name_at(tab - img.base)[2]
        if start not in by_start:
            continue
        end, name, obj = by_start[start]
        for cell in range(tab, end - 3, 8):
            live.setdefault(cell, (name, obj, sites, "indexed"))
    for rec, sites in ecx_sites.items():
        nm = img.name_at(rec - img.base)
        live.setdefault(rec, (nm[0], nm[1], sites, "fixed"))
    bad, excused, known, checked = [], [], [], 0
    fired = set()
    cells = []
    for rva, end, name, obj in extents:
        t = flagged.get(name)
        if t is None:
            continue
        for at, target in cells_of(img, rva, end):
            cells.append((name, obj, at, target, t, None))
    seen = set((c[2], c[3]) for c in cells)
    for cell, (name, obj, sites, how) in sorted(live.items()):
        t = flagged.setdefault(name, {"obj": obj, "recs": set(), "sites": []})
        t["recs"].add(cell)
        for at in sites[:2]:
            if len(t["sites"]) < 4 and at not in t["sites"]:
                t["sites"].append(at)
        for target, via in live_cell_bodies(img, cell, copies, consts):
            if (cell - img.base, target) in seen:
                continue
            seen.add((cell - img.base, target))
            cells.append((name, obj, cell - img.base, target, t, via))
    for name, obj, at, target, t, via in cells:
        checked += 1
        if receiver_of(img, target) != "STACK" or ecx_by_name(img, target):
            continue
        body = img.name_at(target)
        if via:
            body = (body[0], body[1], body[2], via)
        why = derived_ok(name, body[0])
        if not why:
            ok = cdecl_ok_for(name)
            if ok and re.match(ok, body[0]):
                why = "the ledger's own cell-by-cell __cdecl exception"
        if not why:
            for kind, pat, _w, _o in ledger():
                if kind == "CDECL" and re.match(pat, name):
                    why = "an adjudicated __cdecl table in the ledger"
                    break
        row = (name, obj, img.base + at, body, t, why)
        if why:
            excused.append(row)
            continue
        n, bwhy = derived_baseline(name, body[0])
        if n is not None:
            fired.add(n)
            known.append((name, obj, img.base + at, body, t, bwhy))
            continue
        bad.append(row)
    stale = [i for i in range(len(DERIVED_BASELINE)) if i not in fired]
    return bad, excused, checked, flagged, known, stale


# ---- main -----------------------------------------------------------------
def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=None, type=decomp_root.root_arg)
    ap.add_argument("--exe")
    ap.add_argument("--map")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args(argv)
    if a.selftest:
        return selftest()

    exe = a.exe or decomp_root.join(a.root, "build", "port", "walk_window.exe")
    mp = a.map or decomp_root.join(a.root, "build", "port", "walk_window.map")
    if not os.path.exists(exe) or not os.path.exists(mp):
        print("pmf_guard: no built image at %s -- nothing to check" % exe)
        return 0

    rc = selftest()
    if rc:
        return rc

    img = Image(exe, mp)
    rows = census(img)
    seen = set(name for _, name, _, _, _, _, _, _ in rows)
    missing = []
    for kind, pat, why, ok in ledger():
        if not any(re.match(pat, n) for n in seen):
            missing.append(pat)
    if missing:
        print("pmf_guard REFUSES: %d seat table(s) in the ledger are not in the "
              "map, so the ledger has gone stale and the check is not running "
              "on them:" % len(missing))
        for m in missing:
            print("    %s" % m)
        return 1

    checked = [r for r in rows if r[0] == "ECX"]
    adjudicated = [r for r in rows if r[0] == "CDECL"]
    bad, excused = refusals(rows)

    if a.list:
        for kind, name, obj, at, target, tn, klass, why in rows:
            print("%-6s %08x  %-44s -> %-8s %s+0x%x [%s]" % (
                kind, img.base + at, name[:44], klass, tn[0], tn[2], tn[1]))

    if bad:
        print("pmf_guard REFUSES: %d cell(s) in a pointer-to-member table hold "
              "a body that reads its receiver off the caller's stack. The call "
              "site passes it in ECX and pushes nothing, so the body will read "
              "whatever the caller last spilled." % len(bad))
        for kind, name, obj, at, target, tn, klass, why in bad:
            print("")
            print("  %s  [%s]" % (name, obj))
            print("    cell at %08x holds %08x = %s+0x%x [%s]" % (
                img.base + at, img.base + target, tn[0], tn[2], tn[1]))
            print("    its prologue reads [ebp+8]/[esp+4] and never reads ecx")
            print("    why this table needs ECX: %s" % why)
        print("")
        print("Fix: seat a __fastcall thunk that names the body, the shape "
              "5ae983797 / 27a24ff5a / 651b5e853 / f9936e798 / 45ce69707 / "
              "00732a5ab all use. If the cell is really reached only by a flat "
              "C dispatcher that pushes the receiver, move its table to a "
              "CDECL row in this file's ledger WITH the dispatcher named.")
        return 1

    print("pmf_guard OK: %d cell(s) across %d pointer-to-member table(s) all "
          "take their receiver in ECX; %d cell(s) across %d table(s) are the "
          "adjudicated __cdecl exceptions, %d of them named cell by cell inside "
          "a checked table." % (
              len(checked), len(set(r[1] for r in checked)),
              len(adjudicated) + len(excused),
              len(set(r[1] for r in adjudicated)) +
              len(set(r[1] for r in excused)), len(excused)))

    # ---- the derived half: the tables nobody put in the ledger ------------
    dbad, dexcused, dchecked, flagged, dknown, dstale = derived_refusals(img)
    for name, obj, at, body, t, why in dknown:
        print("pmf_guard: baseline-known -- %s holds %s+0x%x, which reads its "
              "receiver off the stack: %s" % (name[:44], body[0], body[2], why))
    for i in dstale:
        print("pmf_guard: note -- baseline row %d (%s) no longer fires, so the "
              "baseline can be tightened by deleting it"
              % (i, DERIVED_BASELINE[i][0]))
    if a.list:
        for name in sorted(flagged):
            t = flagged[name]
            print("DERIV  %-52s %d record(s) dispatched with the receiver in "
                  "ECX [%s]" % (name[:52], len(t["recs"]), t["obj"]))
    if dbad:
        print("")
        print("pmf_guard REFUSES: %d cell(s) in %d seat table(s) hold a body "
              "that reads its receiver off the caller's stack, and the image "
              "says the record that cell is written into is dispatched with "
              "the receiver in ECX and nothing pushed." % (
                  len(dbad), len(set(r[0] for r in dbad))))
        for name, obj, at, body, t, why in dbad:
            print("")
            print("  %s  [%s]" % (name, obj))
            print("    cell at %08x holds %s+0x%x [%s]" % (
                at, body[0], body[2], body[1]))
            if len(body) > 3:
                print("    the cell is filled at run time: %s" % body[3])
            print("    its prologue reads [ebp+8]/[esp+4] and never reads ecx")
            for site in t["sites"][:2]:
                nm = img.name_at(site)
                print("    the dispatch is at %08x, in %s+0x%x" % (
                    img.base + site, nm[0], nm[2]))
        print("")
        print("Fix: seat a __fastcall thunk that names the body, the shape "
              "this file's ledger comments describe. If the cell really is "
              "reached only by a flat C dispatcher that pushes the receiver, "
              "add it to DERIVED_OK WITH the dispatcher named and the reading "
              "written out.")
        return 1
    print("pmf_guard OK (derived): %d cell(s) across %d seat table(s) whose "
          "records the image shows dispatched with the receiver in ECX all "
          "take it in ECX too; %d excused by name, %d baseline-known." % (
              dchecked, len(flagged), len(dexcused), len(dknown)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
