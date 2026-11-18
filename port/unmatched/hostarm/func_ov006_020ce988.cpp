/* HOST ARM RETIRED (run catchup1, ACT4b round 2, MG4's row). This file
 * re-defined TrmpItem_UpdateMatrix as a C function with the pin arm's
 * __thiscall slot-5 Render call. Main's TrmpItem::UpdateMatrix is now a real
 * member that calls mModel->Render(&mScale), which is already correct on the
 * host, and MG4 retires the park of main's body, so this copy has no caller
 * and nothing to replace. The file stays, empty, until its slice row goes. */
