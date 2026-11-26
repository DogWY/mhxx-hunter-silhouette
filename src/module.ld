ENTRY(_start)
PHDRS {
    text PT_LOAD FLAGS(5);
    ro PT_LOAD FLAGS(4);
    rw PT_LOAD FLAGS(6);
}
SECTIONS {
    . = 0;
    .text : { *(.text.start) . = 0x100; *(.landing.p0) . = 0x5100; *(.landing.p1) . = 0x8100; *(.landing.p2) . = 0xd100; *(.landing.p3) *(.text*) } :text
    . = ALIGN(0x1000);
    .rodata : { *(.rodata*) } :ro
    . = ALIGN(0x1000);
    .data : { *(.data*) } :rw
    .got : { *(.got .got.*) } :rw
    ASSERT(SIZEOF(.got) == 0, "Guest module has an unrelocated pointer in the GOT")
    .bss : { bss_start = .; *(.bss*) . = ALIGN(0x1000); bss_end = .; } :rw
    /DISCARD/ : { *(.ARM.attributes) *(.comment) }
}
