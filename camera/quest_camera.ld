ENTRY(patch_start)
read_int = 0x826780;
read_float = 0x8269f4;
read_vec3 = 0x8273ac;
SECTIONS {
    . = 0x3cbc9c;
    .text : { *(.text) }
    ASSERT(SIZEOF(.text) == 196, "camera patch overlaps retail cleanup")
}
