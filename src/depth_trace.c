// MHGU 1.4.0. Compare the texture selected at mesh preparation with the
// texture passed to NVN when the sorted command stream is recorded.
#include <stdint.h>
#include <stdatomic.h>

extern void trace_line(const char *, unsigned);

typedef struct {
    uintptr_t texture, view;
    unsigned stage, pass;
} Binding;

static Binding observed[192];
static unsigned used;
static atomic_flag trace_lock = ATOMIC_FLAG_INIT;

static int first_binding(Binding key) {
    // A contended draw can be observed on a later frame.
    if (atomic_flag_test_and_set_explicit(&trace_lock, memory_order_acquire)) return 0;
    unsigned i = 0;
    while (i < used && (observed[i].texture != key.texture ||
           observed[i].view != key.view || observed[i].stage != key.stage ||
           observed[i].pass != key.pass)) ++i;
    int fresh = i == used && used < sizeof observed / sizeof observed[0];
    if (fresh) observed[used++] = key;
    atomic_flag_clear_explicit(&trace_lock, memory_order_release);
    return fresh;
}

static char *hex_field(char *p, unsigned x) {
    *p++ = ' ';
    for (int shift = 28; shift >= 0; shift -= 4)
        *p++ = "0123456789abcdef"[(x >> shift) & 15];
    return p;
}

static uintptr_t field(uintptr_t addr, unsigned offset) {
    return *(const uint32_t *)(addr + offset);
}

void trace_depth_binding(uintptr_t base, uintptr_t texture, uintptr_t view,
                         unsigned stage, unsigned pass) {
    if (!first_binding((Binding){texture, view, stage, pass})) return;
    char line[112];
    char *p = line;
    for (const char *s = "MHGU depth"; *s; ++s) *p++ = *s;
    p = hex_field(p, stage);
    p = hex_field(p, pass);
    p = hex_field(p, texture);
    p = hex_field(p, view);
    if (texture && stage == 0) {
        // Invoke the getters only on the NVN thread with a bound NVNtexture.
        typedef int (*GetInt)(const void *);
        const unsigned getters[] = {0x191f9e0, 0x191f9e4, 0x191f9f0, 0x191f9f4};
        for (unsigned i = 0; i < 4; ++i) {
            GetInt get = (GetInt)field(base, getters[i]);
            if (!get) break;
            p = hex_field(p, (unsigned)get((const void *)texture));
        }
    }
    trace_line(line, (unsigned)(p - line));
}

void trace_mesh_depth(uintptr_t base, uintptr_t context, unsigned stage) {
    // uModel submission context -> depth surface view -> engine texture -> NVNtexture.
    uintptr_t surface = field(context, 188);
    uintptr_t texture = surface ? field(surface, 16) : 0;
    uintptr_t view = surface ? field(surface, 20) : 0;
    trace_depth_binding(base, texture ? field(texture, 60) : 0, view, stage,
                        field(context, 356) & 31);
}
