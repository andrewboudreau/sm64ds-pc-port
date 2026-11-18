/* Host copy of ov023's static initializer at 0x02111aa8..0x02111b14 (mwccarm's
 * __sinit_d_a_obj_fm_battan.cpp). It constructs the Battan's model and
 * collision SharedFilePtrs, data_ov023_02112088 (file 0x616) and
 * data_ov023_02112080 (file 0x617), and pushes each onto the global destructor
 * chain, in the cartridge's order. hal/actor_classes_ov023.cpp calls it at the
 * overlay's bring-up, where the cartridge runs it.
 *
 * WHY THE HOST NEEDS IT. Main's src/game/actors/d_a_obj_fm_battan.cpp defines
 * both handles as C++ objects, but hal/s4ov0_overlay_sinit.h (force-included
 * into that TU) puts its dynamic initialisers in a section the CRT never walks,
 * because before main the ov0 handle table they look the ids up in does not
 * exist yet. Nothing else constructs them, so with an empty body here both
 * handles stayed zero and the first InitResources asked the FAT for entry 0 on
 * level 15. hal/link21_rows.cpp does the same for the other two TUs under that
 * header (ov044 and ov100).
 *
 * The helpers are main's C++ functions (src/SharedFilePtr.cpp); this
 * file is C, so it reaches them through file-private C names. */
#pragma comment(linker, "/alternatename:_port_ov023_model_ctor=?func_02017acc@@YAPAXPAXI@Z")
#pragma comment(linker, "/alternatename:_port_ov023_clsn_ctor=?func_02017b4c@@YAPAXPAXI@Z")
#pragma comment(linker, "/alternatename:_port_ov023_model_dtor=?func_02017ab4@@YAHH@Z")
#pragma comment(linker, "/alternatename:_port_ov023_clsn_dtor=?SharedFilePtr_Destruct_Clsn@@YAHH@Z")
extern void *port_ov023_model_ctor(void *self, unsigned int fileID);
extern void *port_ov023_clsn_ctor(void *self, unsigned int fileID);
extern int port_ov023_model_dtor(int self);
extern int port_ov023_clsn_dtor(int self);

void __register_global_object(void *object, void *destructor, void *node);

extern int data_ov023_02112088[];
extern int data_ov023_02112080[];
extern int data_ov023_0211209c[];   /* the model handle's chain node */
extern int data_ov023_02112090[];   /* the collision handle's chain node */

void __sinit_ov023_02111aa8(void)
{
    port_ov023_model_ctor(data_ov023_02112088, 0x616u);
    __register_global_object(data_ov023_02112088, (void *)port_ov023_model_dtor,
                             data_ov023_0211209c);
    port_ov023_clsn_ctor(data_ov023_02112080, 0x617u);
    __register_global_object(data_ov023_02112080, (void *)port_ov023_clsn_dtor,
                             data_ov023_02112090);
}
