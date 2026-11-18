/* Force-included (cl /FI) into every GAME translation unit: the decomp's own
 * src/ files, their decomp-overlay and host-src (hostgen) copies, and the
 * port/unmatched host copies that stand in for them. The list is the GAME CODE
 * SEGMENT block at the end of port/CMakeLists.txt; nothing includes this
 * header by hand.
 *
 * It opens one code section for the whole TU. The linker merges every
 * .text$<suffix> into .text in suffix order, so the game's code lands in one
 * contiguous run between hal/os_arena.cpp's two markers (.text$mnf and
 * .text$mnh), with the compiler's ordinary .text$mn before it.
 *
 * WHY. On the cartridge the global operator new IS the game heap
 * (_Znwm at 0x0203cbe4 -> func_0203cc0c -> Memory::defaultHeapPtr). On the host
 * one program has one global operator new, and the host's own C++ (the
 * renderer, the sound host, every std::vector) must keep the CRT's. Main's
 * src spells ::operator new and new-expressions on classes without a class
 * allocator (the models, the minimap change nodes, the smartball pieces), and
 * MSVC binds all of them to that one global. hal/os_arena.cpp's operator new
 * tells the two apart by its return address: a call from inside this run is
 * game code and gets the game heap, anything else gets the CRT.
 *
 * Templates the TU instantiates and compiler-generated members are not moved
 * by code_seg; they stay host code, which is what a std:: container inside a
 * host patch should be anyway. */
#pragma code_seg(".text$mng")
