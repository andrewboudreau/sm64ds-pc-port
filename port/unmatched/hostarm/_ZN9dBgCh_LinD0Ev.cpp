/* HOST ARM recovered by port/tools/hostarm_recover.py.
 * The port line carried this in src/_ZN9dBgCh_LinD0Ev.cpp under #ifdef _MSC_VER; the
 * 2026-10 main merge (749f5799a) took main's src/, where that code
 * now lives in src/engine/collision/d_bg_ch_lin.cpp without the arm. mwccarm never saw the arm,
 * so nothing here moves a ROM byte. */
#include "dBgCh_Lin.h"

extern "C" dBgCh_Lin *_ZN9dBgCh_LinD0Ev(dBgCh_Lin *thiz)
{
    thiz->dBgCh_Lin::~dBgCh_Lin();    /* the D1 body, through MSVC's one destructor */
    ::operator delete(thiz);  /* D0 ends with _ZdlPv (0x0203cbcc); main retired the class-specific forwarder */
    return thiz;
}
