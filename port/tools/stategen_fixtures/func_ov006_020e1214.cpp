//cpp
struct C;
typedef void (C::*PMF)(int);
struct Entry { PMF pmf; };
extern "C" Entry sCurlingBitBStateTable[];

#define F1E(b,i) (*(unsigned char*)((char*)(b) + 0x47aa + (i)*0x24))

extern "C" void func_ov006_020e1214(char *base, int idx)
{
    unsigned char state = F1E(base, idx);
    (((C*)base)->*sCurlingBitBStateTable[state].pmf)(idx);
}
