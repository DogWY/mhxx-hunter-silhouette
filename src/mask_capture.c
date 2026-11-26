// ARMv7 retail command packets for MHGU 1.4.0. Frame-owned objects use
// the game's command arena so playback may lag submission.
#include <stdint.h>
#include <stdatomic.h>

typedef uint32_t Word;
typedef void (*Bind)(Word, unsigned, const Word *, const Word *, Word, Word);
extern unsigned mask_role(uintptr_t, uintptr_t);
extern void trace_line(const char *, unsigned);
extern void clear_mask_depth(Word, Word, Word);

static Word at(Word p, unsigned offset) {
    return *(const Word *)(uintptr_t)(p + offset);
}

typedef struct {
    Word object, surface, texture;
} Mask;

static Mask masks[2];
static Word dimensions;
static atomic_uint init_state, scene_texture;
static atomic_uint cleared_scene, scene_descriptor;
// Retail increments this counter once per produced frame. Keep a short
// overlap for the renderer consuming the preceding frame's commands.
static atomic_uint last_monster_frame;
static int monster_recent(Word frame) {
    return frame - atomic_load_explicit(&last_monster_frame, memory_order_relaxed) <= 2;
}
static Word frame_roles, frame_scene, frame_clear;
void begin_mask_frame(void) {
    frame_roles = frame_scene = 0;
}

int sampled_masks(Word cmds, Word width, Word height, Word ids[3], Word *clear_one);

static Mask create_mask(Word base, unsigned w, unsigned h) {
    typedef Word (*Allocate)(unsigned, unsigned);
    typedef Word (*Construct)(Word, unsigned, unsigned, unsigned, unsigned,
                              unsigned, unsigned, unsigned, unsigned, unsigned, Word);
    Word obj = ((Allocate)(uintptr_t)(base + 0xb0804c))(528, 16);
    if (!obj) return (Mask){0};
    ((Construct)(uintptr_t)(base + 0xb09098))(obj, w, h, 1, 1, 15, 5, 0, 0, 0, 0);
    Word surfaces = at(obj, 520);
    Word surface = surfaces && at(obj, 524) ? at(surfaces, 0) : 0;
    return (Mask){obj, surface, at(obj, 60)};
}

static int select_scene(Word base, Word surface) {
    Word renderer = at(base, 0x2126998);
    if (!renderer || !surface || at(surface, 20) || at(surface, 36) != 53) return 0;
    Word size = at(surface, 24), w = size & 65535, h = size >> 16;
    if (!w || !h || w != at(renderer, 176) || h != at(renderer, 180)) return 0;
    Word owner = at(surface, 16), texture = owner ? at(owner, 60) : 0;
    if (!texture) return 0;
    unsigned state = atomic_load_explicit(&init_state, memory_order_acquire);
    if (!state) {
        unsigned expected = 0;
        if (!atomic_compare_exchange_strong(&init_state, &expected, 1)) return 0;
        masks[0] = create_mask(base, w, h);
        masks[1] = create_mask(base, w, h);
        dimensions = size;
        state = masks[0].surface && masks[1].surface && masks[0].texture &&
                masks[1].texture && masks[0].texture != masks[1].texture ? 2 : 3;
        atomic_store_explicit(&init_state, state, memory_order_release);
        if (state == 2) {
            static const char msg[] = "MHGU private depth pair allocated";
            trace_line(msg, sizeof msg - 1);
        } else {
            static const char msg[] = "MHGU private depth allocation failed";
            trace_line(msg, sizeof msg - 1);
        }
    }
    if (state != 2 || dimensions != size) return 0;
    atomic_store(&scene_descriptor, at(owner, 52));
    atomic_store(&scene_texture, texture);
    return atomic_load(&cleared_scene) == texture;
}

// The original bind helper passes arrays local to its stack frame. Copy the
// entries while they are live, to restore exactly the same color/depth views.
static struct {
    Word commands, count, colors[4], views[4], depth, view;
} bound;

void remember_targets(const Word *args) {
    bound.commands = args[0];
    bound.count = args[1];
    bound.depth = args[6];
    bound.view = args[7];
    if (frame_scene && bound.depth) {
        if (bound.depth == masks[0].texture) frame_roles |= 1;
        if (bound.depth == masks[1].texture) frame_roles |= 2;
    }
    if (bound.count > 4) return;
    for (unsigned i = 0; i < bound.count; ++i) {
        bound.colors[i] = at(args[2], i * 4);
        bound.views[i] = args[3] ? at(args[3], i * 4) : 0;
    }
}

void clear_private_targets(Word cmds, unsigned depth_enable, Word bits, Word base) {
    if (!depth_enable || !bound.depth || bound.view || bound.commands != cmds ||
        bound.count > 4 || bound.depth != atomic_load(&scene_texture)) return;
    frame_roles = frame_scene = 0;
    if (!monster_recent(at(base, 0x2124120))) return;
    Bind bind = (Bind)(uintptr_t)at(base, 0x191fdb4);
    Word clear = at(base, 0x191fd00);
    for (unsigned i = 0; i < 2; ++i) {
        bind(cmds, 0, 0, 0, masks[i].texture, 0);
        clear_mask_depth(cmds, bits, clear);
    }
    bind(cmds, bound.count, bound.colors, bound.views, bound.depth, bound.view);
    frame_roles = 0;
    frame_scene = bound.depth;
    frame_clear = bits;
    atomic_store(&cleared_scene, bound.depth);
}

int sampled_masks(Word cmds, Word width, Word height, Word ids[3], Word *clear_one) {
    if (frame_roles != 3 || cmds != bound.commands || frame_scene != atomic_load(&scene_texture) ||
        (frame_clear != 0 && frame_clear != 0x3f800000) ||
        (dimensions & 65535) != width || (dimensions >> 16) != height) return 0;
    ids[0] = at(masks[0].object, 52);
    ids[1] = at(masks[1].object, 52);
    ids[2] = atomic_load(&scene_descriptor);
    *clear_one = frame_clear != 0;
    return ids[0] && ids[1] && ids[2];
}

typedef struct {
    Word list[4], entries[4], draw[8], target[23], padding;
} Replay;

Word capture_indexed(Word *ctx, Word count, Word first, Word vertex, Word model, Word base) {
    typedef Word (*Draw)(Word *, Word, Word, Word);
    unsigned index = ctx[16];
    Word result = ((Draw)(uintptr_t)(base + 0x893ca8))(ctx, count, first, vertex);
    if (ctx[16] != index + 1 || (ctx[89] & 31) != 6) return result;
    unsigned role = mask_role(model, base);
    if (!role) return result;
    Word *entry = (Word *)(uintptr_t)ctx[15] + 2 * index;
    Word *draw = (Word *)(uintptr_t)entry[1];
    if (!draw || ((draw[0] >> 16) & 15) != 1 || !draw[1]) return result;
    Word *target = (Word *)(uintptr_t)draw[1];
    Word frame = at(base, 0x2124120);
    if (role == 2) {
        atomic_store_explicit(&last_monster_frame, frame, memory_order_relaxed);
    } else if (!monster_recent(frame)) {
        return result;
    }
    if (!select_scene(base, target[4])) return result;
    if (!ctx[5] || ctx[5] > ctx[6]) return result;
    if (ctx[6] - ctx[5] < sizeof(Replay)) {
        Word old_cursor = ctx[5], old_end = ctx[6];
        typedef Word (*Reserve)(Word *, unsigned);
        ((Reserve)(uintptr_t)(base + 0x882674))(ctx, sizeof(Replay));
        if (!ctx[5]) {
            ctx[5] = old_cursor;
            ctx[6] = old_end;
            return result;
        }
    }
    Replay *r = (Replay *)(uintptr_t)ctx[5];
    ctx[5] += sizeof *r;
    for (unsigned i = 0; i < 23; ++i) r->target[i] = target[i];
    for (unsigned i = 0; i < 4; ++i) r->target[i] = 0;
    r->target[4] = masks[role - 1].surface;
    r->target[5] &= ~7u;
    for (unsigned i = 0; i < 8; ++i) r->draw[i] = draw[i];
    r->draw[1] = (Word)(uintptr_t)r->target;
    // Type 6 is a retail sub-list. Keep the original sort key and queue
    // length. Draw the mask first so the retail draw restores its own state.
    r->list[0] = (draw[0] & ~0xf0000u) | 0x60000u;
    r->list[1] = 0;
    r->list[2] = (Word)(uintptr_t)r->entries;
    r->list[3] = 2;
    r->entries[0] = r->entries[2] = entry[0];
    r->entries[1] = (Word)(uintptr_t)r->draw;
    r->entries[3] = entry[1];
    entry[1] = (Word)(uintptr_t)r->list;
    static atomic_uint roles_logged;
    unsigned bit = 1u << role;
    if (!(atomic_load(&roles_logged) & bit) && !(atomic_fetch_or(&roles_logged, bit) & bit)) {
        static const char hunter[] = "MHGU hunter depth replay queued";
        static const char monster[] = "MHGU monster depth replay queued";
        trace_line(role == 1 ? hunter : monster, role == 1 ? sizeof hunter - 1 : sizeof monster - 1);
    }
    return result;
}
