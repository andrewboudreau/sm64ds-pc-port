/* THE ModelBase, Model AND ShadowModel PART OF port/unmatched/ModelFamily_Dtors_HostCopy.c,
 * for the smoke targets that host those three classes and no other member of
 * the model family.
 *
 * Lane SMOKELINK5, run link100 wave 10 round 5, applying lane SMOKELINK4B's
 * remaining.md section A verbatim.
 *
 * READ THE PARENT FILE'S HEADER FIRST.  It carries the whole ruling: why the
 * fourteen bodies are host copies at all, what the quarantine is, what it
 * costs, and when it retires.  Nothing here changes any of it.
 *
 * WHY A SUBSET.  The parent covers SIX classes, and the three this target does
 * not host -- CommonModel, ModelAnim2, BlendModelAnim -- reach
 * _ZN9AnimationD1Ev, _ZTV15dExtAnm2Model_c, _ZTV19dExtBlendAnmModel_c,
 * VTable_dExtFrameCtrl_c_dExtAnm2Model_cThunk and VTable_dExtFrameCtrl_c_dExtBlendAnmModel_cThunk.
 * This is the same subset argument port/faces_sync_smoke_heap.txt makes for
 * the ledger and port/hal/smoke_aliases_heap.cpp makes for the alias rows:
 * take the rows the target needs, verbatim, and leave the rest where it is.
 * port/unmatched/ModelBaseModel_Dtors_HostCopy.c is exactly this shape for
 * ModelBase and Model alone (smoke_model's two-class gate); this file is that
 * same text plus ShadowModel's D1 and D0, which this batch's shadow gate
 * needs and smoke_model's targets do not.
 *
 * EVERY LINE BELOW IS COPIED CHARACTER FOR CHARACTER from the parent -- the
 * three structs it declares for these classes, the externs these bodies call,
 * and the eight bodies themselves (ModelBase's and Model's D2, D1 and D0, and
 * ShadowModel's D1 and D0; ModelBase's D2 is here because ShadowModel's D1 and
 * D0 both call it, the same way Model's does).  Nothing is re-derived, nothing
 * is stubbed, and no target carries both this file and either
 * ModelBaseModel_Dtors_HostCopy.c or ModelFamily_Dtors_HostCopy.c, so the
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

struct ShadowMdl {
    void *vtable;            /* 0x00 */
    char pad[0x20 - 4];
    struct ShadowMdl *prev;  /* 0x20 */
    struct ShadowMdl *next;  /* 0x24 */
};

extern void *_ZTV11dExtModel_c[];                 /* ModelBase's own table */
extern void *_ZTV17dExtSimpleModel_c[];
extern void *_ZTV17dExtShadowModel_c[];
extern struct ShadowMdl *data_0209cef4;       /* the shadow list head */

extern void Deallocate(void *ptr);                      /* 0x02018144 */
extern void _ZdlPv(void *ptr);                          /* the same entry */
/* catchup1 Stage C round 4: main's src/engine/heap/Heap.cpp names 0x0203cbc0
   and 0x0203cbcc the global operator delete[] (_ZdaPv) and operator delete
   (_ZdlPv) and compiles them only off the host (#ifndef _MSC_VER): on the PC
   port every new and delete is the CRT's. The flat names this copy used are
   defined nowhere, so the bodies release through the CRT's two operators,
   reached by names private to this file (the ModelBaseModel copy's fix). */
extern void ModelShadow_HostDeleteArray(void *ptr);  /* 0x0203cbc0, delete[] */
extern void ModelShadow_HostDelete(void *p);         /* 0x0203cbcc, delete */
#pragma comment(linker, "/alternatename:_ModelShadow_HostDeleteArray=??_V@YAXPAX@Z")
#pragma comment(linker, "/alternatename:_ModelShadow_HostDelete=??3@YAXPAX@Z")

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
    ModelShadow_HostDelete(thiz);
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
        ModelShadow_HostDeleteArray(thiz->unk4c);
    _ZN11dExtModel_cD2Ev((struct MdlBase *)thiz);
    ModelShadow_HostDelete(thiz);
    return thiz;
}

/* ---- ShadowModel, 0x02015ff8 (D1) --------------------------------------- */

struct ShadowMdl *_ZN17dExtShadowModel_cD1Ev(struct ShadowMdl *thiz)
{
    struct ShadowMdl *prev;
    struct ShadowMdl *next;

    thiz->vtable = (void *)_ZTV17dExtShadowModel_c;

    prev = thiz->prev;
    if (prev) {
        prev->next = thiz->next;
    } else if (data_0209cef4 == thiz) {
        data_0209cef4 = thiz->next;
    }

    next = thiz->next;
    if (next) {
        next->prev = thiz->prev;
    }

    thiz->prev = 0;
    thiz->next = 0;

    _ZN11dExtModel_cD2Ev((struct MdlBase *)thiz);
    return thiz;
}

struct ShadowMdl *_ZN17dExtShadowModel_cD0Ev(struct ShadowMdl *thiz)
{
    thiz->vtable = (void *)_ZTV17dExtShadowModel_c;

    if (thiz->prev)
        thiz->prev->next = thiz->next;
    else if (data_0209cef4 == thiz)
        data_0209cef4 = thiz->next;

    if (thiz->next)
        thiz->next->prev = thiz->prev;

    thiz->prev = 0;
    thiz->next = 0;

    _ZN11dExtModel_cD2Ev((struct MdlBase *)thiz);
    ModelShadow_HostDelete(thiz);
    return thiz;
}
