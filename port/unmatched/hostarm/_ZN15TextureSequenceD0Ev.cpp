/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN15TextureSequenceD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/model/TextureSequence.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dExtAnmTexPat_c.h"

extern "C" dExtAnmTexPat_c *_ZN15dExtAnmTexPat_cD0Ev(dExtAnmTexPat_c *thiz)
{
    thiz->dExtAnmTexPat_c::~dExtAnmTexPat_c();    /* the D1 body, through MSVC's one destructor */
    ::operator delete(thiz);  /* D0 ends with _ZdlPv (0x0203cbcc); main retired the class-specific forwarder */
    return thiz;
}
