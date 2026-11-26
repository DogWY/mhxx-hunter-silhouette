// Called after MHGU's sorted command stream has finished recording and
// before the two HUD mods append their draws to the acquired window image.
#include <stdint.h>

typedef uint32_t Word;
typedef uint64_t Handle;
extern void begin_mask_frame(void);
extern int sampled_masks(Word, Word, Word, Word[3], Word *);
extern void trace_line(const char *, unsigned);
extern __attribute__((visibility("hidden"))) const unsigned char vertex_control[], edge_control[], vertex_code[],
                           vertex_code_end[], edge_code[], edge_code_end[];

static Word read_word(Word base, unsigned offset) {
    return *(const Word *)(uintptr_t)(base + offset);
}

// Retail NVN dispatch table. These entries use integer/pointer arguments only.
#define PROC(offset, result, ...) ((result (*)(__VA_ARGS__))(uintptr_t)read_word(base, offset))

static _Alignas(4096) unsigned char storage[8192]; // includes NVN shader prefetch padding
static _Alignas(16) unsigned char pool[256], program[192];
static int program_status;
static unsigned configured_width;
// The four independent cheat sections set one byte each in module-owned RAM.
_Alignas(4) volatile Word width_cheat_flags;

static unsigned selected_width(Word flags, unsigned fallback) {
    switch (flags) {
    case 0x00000001: return 1;
    case 0x00000100: return 3;
    case 0x00010000: return 5;
    case 0x01000000: return 8;
    default: return fallback;
    }
}

static unsigned read_outline_width(Word base) {
    typedef Word (*Open)(Word *, const char *, int);
    typedef Word (*Read)(unsigned *, Word, int64_t, void *, unsigned);
    typedef void (*Close)(Word);
    Word handle;
    if (((Open)(uintptr_t)(base + 0x13f04b4))(&handle, "app:/mhgu-outline/outline-width.txt", 1))
        return 3;
    char text[32];
    unsigned length = 0;
    Word error = ((Read)(uintptr_t)(base + 0x13f0448))(&length, handle, 0, text, sizeof text);
    ((Close)(uintptr_t)(base + 0x13f046c))(handle);
    if (error || length >= sizeof text) return 3;
    unsigned width = 0;
    for (unsigned i = 0; i < length; ++i) {
        char c = text[i];
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
        if (width || c < '1' || c > '8') return 3;
        width = (unsigned)(c - '0');
    }
    return width ? width : 3;
}

typedef struct { Handle code; const void *control; } Shader;
_Static_assert(sizeof(Shader) == 16, "NVN shader data ABI");

static int make_program(Word base, Word device) {
    unsigned vs = (uintptr_t)vertex_code_end - (uintptr_t)vertex_code;
    unsigned fs = (uintptr_t)edge_code_end - (uintptr_t)edge_code;
    unsigned offset = (vs + 255) & ~255u;
    if (offset > sizeof storage - 1024 || fs > sizeof storage - 1024 - offset) return 0;
    static _Alignas(16) unsigned char builder[64];
    PROC(0x191f820, void, void *, Word)(builder, device);
    PROC(0x191f824, void, void *)(builder);
    PROC(0x191f828, void, void *, void *, unsigned)(builder, storage, sizeof storage);
    PROC(0x191f82c, void, void *, unsigned)(builder, 0x64); // CPU_CACHED | GPU_CACHED | SHADER_CODE
    if (!PROC(0x191f83c, uint8_t, void *, void *)(pool, builder)) return 0;
    for (unsigned i = 0; i < vs; ++i) storage[i] = vertex_code[i];
    for (unsigned i = 0; i < fs; ++i) storage[offset + i] = edge_code[i];
    PROC(0x191f84c, void, void *, int, unsigned)(pool, 0, sizeof storage);
    Handle gpu = PROC(0x191f854, Handle, void *)(pool);
    Shader shaders[2] = {{gpu, vertex_control}, {gpu + offset, edge_control}};
    if (!PROC(0x191f810, uint8_t, void *, Word)(program, device)) return 0;
    return PROC(0x191f81c, uint8_t, void *, unsigned, const Shader *)(program, 2, shaders) != 0;
}

static void composite(Word renderer, Word base) {
    Word window = renderer + 213248, commands = read_word(renderer, 148);
    if (!commands || commands != read_word(window, 376)) return;
    Word acquired = read_word(window, 4660 * 4);
    Word swap = read_word(window, 4659 * 4);
    if (acquired >= 3 || !swap || swap != read_word(window, (4656 + acquired) * 4)) return;
    Word width = read_word(renderer, 176), height = read_word(renderer, 180);
    Word ids[3], clear_one;
    if (!sampled_masks(commands, width, height, ids, &clear_one)) return;
    Word device = read_word(renderer, 144), color = read_word(swap, 60);
    if (!device || !color) return;
    if (!program_status) {
        configured_width = read_outline_width(base);
        program_status = make_program(base, device) ? 1 : -1;
        if (program_status < 0) {
            static const char msg[] = "MHGU outline shader initialization failed";
            trace_line(msg, sizeof msg - 1);
        }
    }
    if (program_status < 0) return;
    Handle handles[3];
    for (unsigned i = 0; i < 3; ++i)
        handles[i] = PROC(0x191f72c, Handle, Word, int)(device, ids[i]);
    Word view = 0;
    PROC(0x191fdb4, void, Word, unsigned, const Word *, const Word *, Word, Word)
        (commands, 1, &color, &view, 0, 0);
    PROC(0x191fdd0, void, Word, int)(commands, 0x53); // order primitives/fragments and invalidate texture caches
    PROC(0x191fddc, void, Word, Word)(commands, window + 392 + 1344);
    PROC(0x191fc78, void, Word, void *, unsigned)(commands, program, 31);
    PROC(0x191fca0, void, Word, unsigned, unsigned, unsigned, const Handle *)
        (commands, 1, 0, 3, handles);
    // Set complete fixed-function state for the full-screen triangle.
    const Word defaults[] = {0x191fb10, 0x191fb2c, 0x191fb38, 0x191fb94, 0x191fbb8};
    const Word bind[] = {0x191fc60, 0x191fc5c, 0x191fc64, 0x191fc68, 0x191fc6c};
    _Alignas(16) Word state[8];
    for (unsigned i = 0; i < 5; ++i) {
        PROC(defaults[i], void, void *)(state);
        PROC(bind[i], void, Word, void *)(commands, state);
    }
    PROC(0x191fd0c, void, Word, int, int, int, int)(commands, 0, 0, width, height);
    PROC(0x191fd18, void, Word, int, int, int, int)(commands, 0, 0, width, height);
    unsigned outline_width = selected_width(width_cheat_flags, configured_width);
    static unsigned reported_width;
    if (outline_width != reported_width) {
        reported_width = outline_width;
        char message[] = "MHGU outline width 0 px";
        message[19] = '0' + outline_width;
        trace_line(message, sizeof message - 1);
    }
    PROC(0x191fcd0, void, Word, unsigned, int, int)(commands, 4, (outline_width - 1) * 8 + clear_one * 4, 3);
    static unsigned reported;
    if (!reported) {
        reported = 1;
        static const char msg[] = "MHGU occluded outline composite recorded";
        trace_line(msg, sizeof msg - 1);
    }
}

Word draw_frame(Word renderer, Word base) {
    begin_mask_frame();
    Word result = ((Word (*)(Word))(uintptr_t)(base + 0xbbe9c8))(renderer);
    composite(renderer, base);
    return result;
}
