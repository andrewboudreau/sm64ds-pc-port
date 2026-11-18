/* RETIRED (run catchup1, lane PLY4, decomp main bddb9d36a60).
 *
 * This file used to define six daPly_c members again (the four music-layer
 * bodies, func_ov002_020bdb50 and St_Swim_Main) with host arms main's text had
 * dropped. Main's src/game/player/d_a_ply_methods.cpp now spells each of them
 * the way the arms did, so the host copies have nothing left to fix:
 *
 *   - Sound::EndMusic takes one parameter (include/Sound.h), so the one-word
 *     calls in EndStateMusic, StartStateMusic, EndPowerupMusic and
 *     StartPowerupMusic match the definition on every compiler;
 *   - func_ov002_020bdb50 calls mObjInMouth->OnYoshiTryEat() and
 *     obj->OnTurnIntoEgg(*this) as real dActor_c virtuals (slots 18 and 19)
 *     and passes the held object to func_ov002_020d5cec(obj);
 *   - St_Swim_Main calls this->GetHealth() as a member.
 *
 * The TU's own bodies compile instead (hostgen's PARK_SYMBOL row for the TU
 * keeps only _ZN7daPly_c19func_ov002_020d869cEv, which
 * a host-side file hosts). This empty TU stays only until
 * the slice rows naming it are removed; then the file goes. */
