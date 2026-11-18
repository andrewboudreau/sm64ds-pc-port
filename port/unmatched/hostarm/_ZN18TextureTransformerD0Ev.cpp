/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN18TextureTransformerD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/model/TextureTransformer.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dExtAnmTexSRT_c.h"

extern "C" dExtAnmTexSRT_c *_ZN15dExtAnmTexSRT_cD0Ev(dExtAnmTexSRT_c *thiz)
{
    thiz->dExtAnmTexSRT_c::~dExtAnmTexSRT_c();    /* the D1 body, through MSVC's one destructor */
    ::operator delete(thiz);  /* D0 ends with _ZdlPv (0x0203cbcc); main retired the class-specific forwarder */
    return thiz;
}
