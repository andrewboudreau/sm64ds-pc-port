/* THE ModelBase, Model AND BlendModelAnim PART OF port/unmatched/ModelFamily_Dtors_HostCopy.c,
 * for smoke_modelanim, which hosts those three classes and no other member of
 * the model family.
 *
 * Lane SMOKELINK3, run link100 wave 10 round 2.
 *
 * READ THE PARENT FILE'S HEADER FIRST.  It carries the whole ruling: why the
 * fourteen bodies are host copies at all, what the quarantine is, what it
 * costs, and when it retires.  Nothing here changes any of it.
 *
 * WHY A SUBSET.  The parent covers SIX classes, and the three this target does
 * not host -- CommonModel, ModelAnim2, ShadowModel -- reach _ZN9AnimationD1Ev,
 * _ZTV15dExtAnm2Model_c, _ZTV11ShadowModel, VTable_dExtFrameCtrl_c_dExtAnm2Model_cThunk and
 * data_0209cef4.  Taking the parent whole on smoke_modelanim was tried and
 * traded six unresolved names for six different ones.  This is the
 * same subset argument port/faces_sync_smoke_heap.txt makes for the ledger and
 * port/hal/smoke_aliases_heap.cpp makes for the alias rows: take the rows the
 * target needs, verbatim, and leave the rest where it is.
 *
 * EVERY LINE BELOW IS COPIED CHARACTER FOR CHARACTER from the parent -- the
 * two structs it declares for these classes, the externs these bodies call,
 * and the eight bodies themselves (ModelBase's and Model's D2, D1 and D0, and
 * BlendModelAnim's D1 and D0; ModelBase's D2 is here because the other bodies
 * call it).  Nothing is
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

struct BlendMdlAnim {
    void *vtable;            /* 0x00 */
    char pad0[0x50 - 4];
    void *animVtable;        /* 0x50 Animation sub-object */
    char pad1[0x6c - 0x54];
    void *unk6c;             /* 0x6c owned pointer */
};

extern void *_ZTV11dExtModel_c[];                 /* ModelBase's own table */
extern void *_ZTV17dExtSimpleModel_c[];
extern void *_ZTV19dExtBlendAnmModel_c[];
extern void *VTable_dExtFrameCtrl_c_dExtBlendAnmModel_cThunk[];

extern void Deallocate(void *ptr);                      /* 0x02018144 */
extern void _ZdlPv(void *ptr);                          /* the same entry */
/* catchup1 Stage C round 4: main's src/engine/heap/Heap.cpp names 0x0203cbc0
   and 0x0203cbcc the global operator delete[] (_ZdaPv) and operator delete
   (_ZdlPv) and compiles them only off the host (#ifndef _MSC_VER): on the PC
   port every new and delete is the CRT's. The flat names this copy used are
   defined nowhere, so the bodies release through the CRT's two operators,
   reached by names private to this file (the ModelBaseModel copy's fix). */
extern void ModelBlend_HostDeleteArray(void *ptr);  /* 0x0203cbc0, delete[] */
extern void ModelBlend_HostDelete(void *p);         /* 0x0203cbcc, delete */
#pragma comment(linker, "/alternatename:_ModelBlend_HostDeleteArray=??_V@YAXPAX@Z")
#pragma comment(linker, "/alternatename:_ModelBlend_HostDelete=??3@YAXPAX@Z")
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
    ModelBlend_HostDelete(thiz);
    return thiz;
}



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
        ModelBlend_HostDeleteArray(thiz->unk4c);
    _ZN11dExtModel_cD2Ev((struct MdlBase *)thiz);
    ModelBlend_HostDelete(thiz);
    return thiz;
}

/* ---- BlendModelAnim, 0x02016690 (D1), 0x02016644 (D0) ------------------- */

struct BlendMdlAnim *_ZN19dExtBlendAnmModel_cD1Ev(struct BlendMdlAnim *thiz)
{
    thiz->vtable = (void *)_ZTV19dExtBlendAnmModel_c;
    thiz->animVtable = (void *)VTable_dExtFrameCtrl_c_dExtBlendAnmModel_cThunk;
    if (thiz->unk6c)
        _ZdlPv(thiz->unk6c);
    _ZN14dExtAnmModel_cD2Ev(thiz);
    return thiz;
}

struct BlendMdlAnim *_ZN19dExtBlendAnmModel_cD0Ev(struct BlendMdlAnim *thiz)
{
    thiz->vtable = (void *)_ZTV19dExtBlendAnmModel_c;
    thiz->animVtable = (void *)VTable_dExtFrameCtrl_c_dExtBlendAnmModel_cThunk;
    if (thiz->unk6c)
        ModelBlend_HostDeleteArray(thiz->unk6c);
    _ZN14dExtAnmModel_cD2Ev(thiz);
    ModelBlend_HostDelete(thiz);
    return thiz;
}

