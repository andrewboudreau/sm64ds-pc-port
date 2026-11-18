/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN5dCc_cD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/collision/d_cc.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dCc_c.h"

extern "C" dCc_c *_ZN5dCc_cD0Ev(dCc_c *thiz)
{
    thiz->dCc_c::~dCc_c();    /* the D1 body, through MSVC's one destructor */
    ::operator delete(thiz);  /* D0 ends with _ZdlPv (0x0203cbcc); main retired the class-specific forwarder */
    return thiz;
}
