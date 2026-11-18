/* THE ModelBase AND Model HALF OF port/unmatched/ModelFamily_Dtors_HostCopy.c,
 * for the smoke targets that host those two classes and no other member of the
 * model family.
 *
 * Lane SMOKELINK3, run link100 wave 10 round 2.
 *
 * READ THE PARENT FILE'S HEADER FIRST.  It carries the whole ruling: why the
 * fourteen bodies are host copies at all, what the quarantine is, what it
 * costs, and when it retires.  Nothing here changes any of it.
 *
 * WHY A SUBSET.  The parent covers SIX classes, and four of them --
 * CommonModel, ModelAnim2, BlendModelAnim, ShadowModel -- reach
 * _ZN9AnimationD1Ev, _ZN14dExtAnmModel_cD2Ev, data_0209cef4, three _ZTV tables and
 * two Animation thunks that a model smoke does not host.  Taking the parent
 * whole on smoke_model traded four unresolved names for eleven.  This is the
 * same subset argument port/faces_sync_smoke_heap.txt makes for the ledger and
 * port/hal/smoke_aliases_heap.cpp makes for the alias rows: take the rows the
 * target needs, verbatim, and leave the rest where it is.
 *
 * EVERY LINE BELOW IS COPIED CHARACTER FOR CHARACTER from the parent -- the
 * two structs it declares for these classes, the externs these bodies call,
 * and the six bodies themselves (D2, D1 and D0 of each class; D2 is here
 * because both classes' other two bodies call ModelBase's).  Nothing is
 * re-derived, nothing is stubbed, and no target carries both files, so the two
 * copies cannot collide.  If the parent's text for one of these ever changes,
 * the two files disagree and the diff is one grep away.
 *
 * C, NOT C++, ON PURPOSE, for the parent's reason: these are the flat
 * Itanium-mangled names the ROM's own tables hold, and a .c TU emits them with
 * no decoration of its own.
 */

struct MdlBase {
    void *vtable;            /* 0x00 */
    void *res;               /* 0x04 owned resource */
};

struct Mdl {
    void *vtable;            /* 0x00 */
    char pad[0x4c - 4];
    void *unk4c;             /* 0x4c owned pointer */
};

extern void *_ZTV11dExtModel_c[];                 /* ModelBase's own table */
extern void *_ZTV17dExtSimpleModel_c[];

extern void Deallocate(void *ptr);                      /* 0x02018144 */
extern void _ZdlPv(void *ptr);                          /* the same entry */

/* catchup1 round 2, the one deliberate divergence from the parent: main's
   src/engine/heap/Heap.cpp now names 0x0203cbc0 and 0x0203cbcc as the global
   operator delete[] and operator delete and compiles them only off the host
   (#ifndef _MSC_VER), because on the PC port every new and delete is the
   CRT's. The two flat names this file used are therefore defined nowhere, so
   the deleting bodies below release through the CRT's two operators instead,
   reached by names private to this file so no other alias row is disturbed. */
extern void ModelBaseModel_HostDeleteArray(void *ptr);  /* 0x0203cbc0, delete[] */
extern void ModelBaseModel_HostDelete(void *p);         /* 0x0203cbcc, delete */
#pragma comment(linker, "/alternatename:_ModelBaseModel_HostDeleteArray=??_V@YAXPAX@Z")
#pragma comment(linker, "/alternatename:_ModelBaseModel_HostDelete=??3@YAXPAX@Z")

extern void _ZN14dExtAnmModel_cD2Ev(void *thiz);              /* 0x0201689c */

/* ---- ModelBase, 0x020170b8 (D2), 0x02017120 (D1), 0x020170e8 (D0) -------- */

struct MdlBase *_ZN11dExtModel_cD2Ev(struct MdlBase *thiz)
{
    thiz->vtable = (void *)_ZTV11dExtModel_c;
    if (thiz->res != 0) {
        Deallocate(thiz->res);
    }
    return thiz;
}

struct MdlBase *_ZN11dExtModel_cD1Ev(struct MdlBase *thiz)
{
    thiz->vtable = (void *)_ZTV11dExtModel_c;
    if (thiz->res != 0) {
        Deallocate(thiz->res);
    }
    return thiz;
}

struct MdlBase *_ZN11dExtModel_cD0Ev(struct MdlBase *thiz)
{
    thiz->vtable = (void *)_ZTV11dExtModel_c;
    if (thiz->res)
        Deallocate(thiz->res);
    ModelBaseModel_HostDelete(thiz);
    return thiz;
}


/* ---- Model, 0x02016ca8 (D2), 0x02016d20 (D1), 0x02016ce0 (D0) ------------ */

struct Mdl *_ZN17dExtSimpleModel_cD2Ev(struct Mdl *thiz)
{
    thiz->vtable = (void *)_ZTV17dExtSimpleModel_c;
    if (thiz->unk4c)
        _ZdlPv(thiz->unk4c);
    _ZN11dExtModel_cD2Ev((struct MdlBase *)thiz);
    return thiz;
}

struct Mdl *_ZN17dExtSimpleModel_cD1Ev(struct Mdl *thiz)
{
    thiz->vtable = (void *)_ZTV17dExtSimpleModel_c;
    if (thiz->unk4c)
        _ZdlPv(thiz->unk4c);
    _ZN11dExtModel_cD2Ev((struct MdlBase *)thiz);
    return thiz;
}

struct Mdl *_ZN17dExtSimpleModel_cD0Ev(struct Mdl *thiz)
{
    thiz->vtable = (void *)_ZTV17dExtSimpleModel_c;
    if (thiz->unk4c)
        ModelBaseModel_HostDeleteArray(thiz->unk4c);
    _ZN11dExtModel_cD2Ev((struct MdlBase *)thiz);
    ModelBaseModel_HostDelete(thiz);
    return thiz;
}
