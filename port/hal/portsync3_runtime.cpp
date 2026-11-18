/* hal/portsync3_runtime.cpp -- the few runtime symbols the main sync made the
 * port define by hand. Includes nothing.
 *
 * __register_global_object: mwccarm's C++ runtime call that records a
 * function-local static's destructor so the DS runtime can run it at exit.
 * Main's TUs spell the call out (src/game/actors/d_a_door.cpp, daTrs_c.cpp,
 * dMg3DHeyhoObjAdapter_c.cpp). The DS game never exits and the host never
 * runs these destructors (the objects are file-scope data the .dsstate span
 * owns), so the registration has nothing to do. MSVC's own thread-safe-static
 * machinery is not involved: the TUs construct the object by hand and only
 * call this to register it.
 */
/* __register_global_object: src/__register_global_object.c defines it now
   (catchup1 Stage C round 2); the decomp copy wins, so the no-op is retired. */
