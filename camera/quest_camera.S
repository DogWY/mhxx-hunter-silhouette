// MHGU 1.4.0, rFreeUseParam::load. At entry r4 is the resource and sp
// points to the initialized reader. All arrays have already been allocated.
.syntax unified
.fpu vfpv3
.text
.arm
.global patch_start
patch_start:
    add ip, pc, #1
    bx ip
.thumb
    movs r0, #1
    mov r8, r0              // original success return value
    ldr r6, [r4, #100]
    cbz r6, floats
    ldr r5, [r4, #112]
ints_loop:
    mov r0, sp
    blx read_int
    stmia r5!, {r0}
    subs r6, #1
    bne ints_loop
floats:
    ldr r6, [r4, #104]
    cbz r6, vectors
    ldr r5, [r4, #116]
floats_loop:
    mov r0, sp
    blx read_float
    vstmia r5!, {s0}
    subs r6, #1
    bne floats_loop
vectors:
    ldr r6, [r4, #108]
    cbz r6, check_camera
    ldr r5, [r4, #120]
    movs r7, #0
vectors_loop:
    mov r0, sp
    blx read_vec3
    vstmia r5!, {s0-s2}
    stmia r5!, {r7}
    subs r6, #1
    bne vectors_loop
check_camera:
    ldr r0, [r4, #104]
    cmp r0, #3
    blo resume
    add.w r0, r4, #8       // cResource::getPath()
    adr r1, camera_path
    movs r3, #24
compare_path:
    ldrb r2, [r0, r3]
    ldrb r5, [r1, r3]
    cmp r2, r5
    bne resume
    subs r3, #1
    bpl compare_path
    ldr r1, selection
    // Each enabled cheat sets one byte. No selection or several choices
    // leaves the values read from the game resource intact.
    movs r2, #1
select_slot:
    cmp r1, #1
    beq apply
    lsls r3, r1, #24
    bne resume
    lsrs r1, #8
    beq resume
    adds r2, #1
    b select_slot
apply:
    adds r2, #5
    movs r0, #98
    muls r0, r2, r0
    movs r1, #72
    muls r1, r2, r1
    vmov s0, r0
    vmov s1, r1
    vcvt.f32.u32 s0, s0
    vcvt.f32.u32 s1, s1
    ldr r0, [r4, #116]
    vstr s0, [r0]
    vstr s1, [r0, #8]
resume:
    adr r0, retail_resume
    bx r0
.balign 4
camera_path:
    .asciz "camera\\param\\questcamera"
camera_path_end:
.balign 4
    .org 188
    .ascii "QCM1"
selection:
    .word 0
retail_resume:
