#include <stdint.h>
#include "large_monster.h"

static uintptr_t reference(uintptr_t address) {
    uintptr_t value = *(volatile const uint32_t *)address;
    return value >= 0x10000 && !(value & 3) ? value : 0;
}

extern void trace_line(const char *, unsigned);

extern void trace_mesh_depth(uintptr_t, uintptr_t, unsigned);

static unsigned local_part(uintptr_t model, uintptr_t base, unsigned *local_slot) {
    uintptr_t manager = reference(base + 0x1885b58);
    uintptr_t session = reference(base + 0x188ca48);
    if (!manager || !session) return 10;
    unsigned slot = *(volatile const uint32_t *)(session + 0x30);
    if (slot >= 4) return 10;
    uintptr_t hunter = reference(manager + 0x1c + 4 * slot);
    if (!hunter) return 10;
    // Prowlers and village actors are outside the fourteen hunter classes.
    uintptr_t type = reference(hunter) - base;
    if (type < 0x1749a88 || type > 0x1750d20) return 10;
    *local_slot = slot;
    unsigned part = 9;
    if (model != hunter) {
        if (reference(model + 0x30) != hunter) return 10;
        for (part = 0; part < 9; ++part) {
            if (reference(hunter + 0x2384 + 4 * part) == model) break;
        }
        if (part == 9) return 10;
    }
    return part;
}

unsigned mask_role(uintptr_t model, uintptr_t base) {
    if (!model || !reference(model + 0xf0)) return 0;
    if (large_monster_type(reference(model) - base)) return 2;
    unsigned slot;
    return local_part(model, base, &slot) <= 9;
}

// The caller supplies a live uModel on the model update thread. Compare the
// owner before following player fields. Scene transitions can clear either
// singleton, so all gameplay pointers are resolved anew on each invocation.
void trace_local_model(uintptr_t model, uintptr_t base, unsigned draw, uintptr_t context) {
    if (!model || !reference(model + 0xf0)) return;
    if (draw) {
        unsigned type = large_monster_type(reference(model) - base);
        if (type) {
            trace_mesh_depth(base, context, 2);
            static unsigned char seen[100];
            if (type < sizeof(seen) && !seen[type]) {
                seen[type] = 1;
                char line[] = "MHGU large monster 000 draw";
                line[19] = '0' + type / 100;
                line[20] = '0' + (type / 10) % 10;
                line[21] = '0' + type % 10;
                trace_line(line, sizeof(line) - 1);
            }
            return;
        }
    }
    unsigned slot;
    unsigned part = local_part(model, base, &slot);
    if (part > 9) return;
    if (draw) trace_mesh_depth(base, context, 1);
    static unsigned logged[2];
    unsigned mask = 1u << part;
    if (logged[draw] & mask) return;
    logged[draw] |= mask;
    char message[] = "MHGU local model slot 0 part 0 update";
    message[22] = '0' + slot;
    message[29] = '0' + part;
    if (draw) {
        message[31] = 'd';
        message[32] = 'r';
        message[33] = 'a';
        message[34] = 'w';
    }
    trace_line(message, draw ? 35 : sizeof(message) - 1);
}
