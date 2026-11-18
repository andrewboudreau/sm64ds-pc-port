/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN15MaterialChangerD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/model/MaterialChanger.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dExtAnmMaterial_c.h"

extern "C" dExtAnmMaterial_c *_ZN17dExtAnmMaterial_cD0Ev(dExtAnmMaterial_c *thiz)
{
    thiz->dExtAnmMaterial_c::~dExtAnmMaterial_c();    /* the D1 body, through MSVC's one destructor */
    ::operator delete(thiz);  /* D0 ends with _ZdlPv (0x0203cbcc); main retired the class-specific forwarder */
    return thiz;
}
