#ifndef MHGU_LARGE_MONSTER_H
#define MHGU_LARGE_MONSTER_H
#include <stdint.h>

// Exact Switch 1.4.0 primary vtables for the numbered uEmNNN_00 actors.
// Unknown classes, small-monster uEms actors and detached effects stay unmarked.
static inline unsigned large_monster_type(uintptr_t vtable_rva) {
    switch (vtable_rva) {
    case 0x179acf8: return 1;
    case 0x179c9c0: return 3;
    case 0x179ed5c: return 4;
    case 0x17a059c: return 7;
    case 0x17a1d24: return 8;
    case 0x17a36a8: return 9;
    case 0x17a5388: return 10;
    case 0x17a6810: return 11;
    case 0x17a76fc: return 12;
    case 0x17acaf4: return 13;
    case 0x17af20c: return 14;
    case 0x17b0e50: return 17;
    case 0x17b2c08: return 18;
    case 0x17b44e8: return 19;
    case 0x17b5588: return 20;
    case 0x17b6eb4: return 21;
    case 0x17b8324: return 22;
    case 0x17b9378: return 23;
    case 0x17ba7a8: return 24;
    case 0x17bc0cc: return 25;
    case 0x17bd69c: return 27;
    case 0x17bec10: return 30;
    case 0x17bf818: return 32;
    case 0x17c1294: return 33;
    case 0x17c20c4: return 36;
    case 0x17c354c: return 37;
    case 0x17c4a7c: return 38;
    case 0x17c5a70: return 42;
    case 0x17c6dec: return 43;
    case 0x17c8504: return 44;
    case 0x17c964c: return 45;
    case 0x17cab40: return 46;
    case 0x17cc110: return 47;
    case 0x17ccf84: return 49;
    case 0x17ce324: return 50;
    case 0x17cf838: return 55;
    case 0x17d05b8: return 56;
    case 0x17d19bc: return 57;
    case 0x17d3094: return 58;
    case 0x17dc3c8: return 60;
    case 0x17dea90: return 61;
    case 0x17e0394: return 63;
    case 0x17e17ec: return 65;
    case 0x17e32bc: return 66;
    case 0x17e4800: return 67;
    case 0x17e6250: return 68;
    case 0x17e7f88: return 69;
    case 0x17e92fc: return 70;
    case 0x17eb25c: return 71;
    case 0x17ecf54: return 76;
    case 0x17eecd8: return 77;
    case 0x17325ec: return 79;
    case 0x17f1d44: return 80;
    case 0x17f3280: return 81;
    case 0x17f5548: return 82;
    case 0x17f6b78: return 83;
    case 0x17f82cc: return 84;
    case 0x17fe578: return 85;
    case 0x17ff380: return 86;
    case 0x1732984: return 87;
    case 0x17329c0: return 88;
    default: return 0;
    }
}
#endif
