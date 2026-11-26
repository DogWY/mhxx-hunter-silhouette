.syntax unified
.arm
.section .text.start,"ax"
.global _start
_start:
    b init_module
    .word mod_header - _start

mod_header:
    .ascii "MOD0"
    .word dynamic_table - mod_header
    .word bss_start - mod_header
    .word bss_end - mod_header
    .word 0, 0
    .word module_object - mod_header

// DT_INIT is called by the guest dynamic linker. The guard also permits
// loaders that invoke the module's entry before its initializer.
init_module:
    ldr r2, guard_offset
guard_pc:
    add r2, pc, r2
    ldr r3, [r2]
    cmp r3, #0
    bxne lr
    mov r3, #1
    str r3, [r2]
    push {r4,lr}
    adr r0, init_message
    movw r1, #:lower16:(init_end-init_message)
    svc #0x27
    pop {r4,pc}
init_message: .ascii "MHGU outline trace initialized"
init_end:
.balign 4
guard_offset:
    .word initialized - (guard_pc + 8)

.section .rodata,"a"
    .word 0, name_end - name
name:
    .ascii "MHGU-Outline-Trace"
name_end:
    .byte 0
    .balign 4
hash_table:
    .word 1, 1, 0, 0
symbols:
    .space 16
strings:
    .byte 0
.section .data,"aw"
.global dynamic_table
dynamic_table:
    .word 4, hash_table
    .word 5, strings
    .word 6, symbols
    .word 10, 1
    .word 11, 16
    .word 12, init_module
    .word 0, 0
.section .bss,"aw",%nobits
    .balign 16
module_object:
    .space 0x200
initialized:
    .space 4


.section .landing.p0,"ax"
    b trace_0
    b trace_1
    b trace_2
    b model_update
    b model_update_alt
    b mesh_static
    b mesh_skinned
    b mesh_extended
    b bind_targets
    b indexed_static
    b indexed_skinned
    b indexed_extended
    b scene_clear
    b worker_composite
    b main_composite
    .ascii "WID1"
    .word width_cheat_flags + 0x2715000 + 0xd000

.section .landing.p1,"ax"
    b trace_0
    b trace_1
    b trace_2
    b model_update
    b model_update_alt
    b mesh_static
    b mesh_skinned
    b mesh_extended
    b bind_targets
    b indexed_static
    b indexed_skinned
    b indexed_extended
    b scene_clear
    b worker_composite
    b main_composite
    .ascii "WID1"
    .word width_cheat_flags + 0x2715000 + 0x8000

.section .landing.p2,"ax"
    b trace_0
    b trace_1
    b trace_2
    b model_update
    b model_update_alt
    b mesh_static
    b mesh_skinned
    b mesh_extended
    b bind_targets
    b indexed_static
    b indexed_skinned
    b indexed_extended
    b scene_clear
    b worker_composite
    b main_composite
    .ascii "WID1"
    .word width_cheat_flags + 0x2715000 + 0x5000

.section .landing.p3,"ax"
    b trace_0
    b trace_1
    b trace_2
    b model_update
    b model_update_alt
    b mesh_static
    b mesh_skinned
    b mesh_extended
    b bind_targets
    b indexed_static
    b indexed_skinned
    b indexed_extended
    b scene_clear
    b worker_composite
    b main_composite
    .ascii "WID1"
    .word width_cheat_flags + 0x2715000 + 0x0

.text
trace_0:
    push {r0-r4,lr}
    mrs r4, APSR
    ldr r2, flag_delta_0
flag_pc_0:
    add r2, pc, r2
    ldrb r3, [r2]
    cmp r3, #0
    bne resume_0
    mov r3, #1
    strb r3, [r2]
    adr r0, label_0
    movw r1, #:lower16:(label_end_0-label_0)
    svc #0x27
resume_0:
    msr APSR_nzcvq, r4
    pop {r0-r4,lr}
    ldr r12, return_delta_0
    add pc, lr, r12
return_delta_0: .word -14288
flag_delta_0: .word flag_0 - (flag_pc_0 + 8)
label_0: .ascii "MHGU native outline pass 008D76D8"
label_end_0:
.balign 4
.section .bss,"aw",%nobits
flag_0: .space 1

.text
trace_1:
    push {r0-r4,lr}
    mrs r4, APSR
    ldr r2, flag_delta_1
flag_pc_1:
    add r2, pc, r2
    ldrb r3, [r2]
    cmp r3, #0
    bne resume_1
    mov r3, #1
    strb r3, [r2]
    adr r0, label_1
    movw r1, #:lower16:(label_end_1-label_1)
    svc #0x27
resume_1:
    msr APSR_nzcvq, r4
    pop {r0-r4,lr}
    ldr r12, return_delta_1
    add pc, lr, r12
return_delta_1: .word -5404
flag_delta_1: .word flag_1 - (flag_pc_1 + 8)
label_1: .ascii "MHGU native outline pass 008D99AC"
label_end_1:
.balign 4
.section .bss,"aw",%nobits
flag_1: .space 1

.text
trace_2:
    push {r0-r4,lr}
    mrs r4, APSR
    ldr r2, flag_delta_2
flag_pc_2:
    add r2, pc, r2
    ldrb r3, [r2]
    cmp r3, #0
    bne resume_2
    mov r3, #1
    strb r3, [r2]
    adr r0, label_2
    movw r1, #:lower16:(label_end_2-label_2)
    svc #0x27
resume_2:
    msr APSR_nzcvq, r4
    pop {r0-r4,lr}
    ldr r12, return_delta_2
    add pc, lr, r12
return_delta_2: .word -5156
flag_delta_2: .word flag_2 - (flag_pc_2 + 8)
label_2: .ascii "MHGU native outline pass 008D9AB4"
label_end_2:
.balign 4
.section .bss,"aw",%nobits
flag_2: .space 1

.text
// Both retail call sites have the model in r0. Keep the pre-call state
// and tail-call the original function using the retail return address.
.macro model_hook name, site, callee, draw
\name:
    push {r0-r4,lr}
    mrs r4, APSR
    mov r3, r1
    ldr r1, =\site + 4
    sub r1, lr, r1
    mov r2, #\draw
    bl trace_local_model
    msr APSR_nzcvq, r4
    pop {r0-r4,lr}
    ldr r12, =(\callee - \site - 4)
    add pc, lr, r12
.endm
model_hook model_update, 0x94e258, 0x890e20, 0
model_hook model_update_alt, 0x94e474, 0x890e20, 0
model_hook mesh_static, 0x8938dc, 0x8926d4, 1
model_hook mesh_skinned, 0x93c780, 0x8926d4, 1
model_hook mesh_extended, 0x99d174, 0x8926d4, 1
.global trace_line
trace_line:
    svc #0x27
    bx lr
.ltorg

// The original instruction is BLX r7. Tail-call it with the original stack
// so NVN sees both stack arguments and returns directly to retail code.
.text
bind_targets:
#ifdef MHGU_TRACE_BINDINGS
    push {r0-r4,lr}
    ldr r0, =0xbc1204
    sub r0, lr, r0
    ldr r1, [sp, #24]
    ldr r2, [sp, #28]
    mov r3, #0
    ldr r4, [sp, #4]
    sub sp, sp, #8
    str r4, [sp]
    bl trace_depth_binding
    add sp, sp, #8
    pop {r0-r4,lr}
#endif
    push {r0-r4,lr}
    // Pass a snapshot of the six original arguments, including the stack pair.
    mov r0, sp
    bl remember_targets
    pop {r0-r4,lr}
    bx r7
.ltorg

.macro indexed_hook name, site, offset
\name:
    push {r4,lr}
    sub sp, sp, #8
.if \offset
    ldr r4, [sp, #(16 + \offset)]
.else
    mov r4, r9
.endif
    str r4, [sp]
    ldr r4, =(\site + 4)
    sub r4, lr, r4
    str r4, [sp, #4]
    bl capture_indexed
    add sp, sp, #8
    pop {r4,pc}
.endm
indexed_hook indexed_static, 0x893c00, 0
indexed_hook indexed_skinned, 0x93c9c8, 0x54
indexed_hook indexed_extended, 0x99d38c, 0x4c

.fpu vfpv3
scene_clear:
    push {r0-r4,lr}
    vpush {d0-d7}
    vmov r2, s0
    ldr r3, =0xbbfa6c
    sub r3, lr, r3
    bl clear_private_targets
    vpop {d0-d7}
    pop {r0-r4,lr}
    bx r7

.global clear_mask_depth
clear_mask_depth: // void (commands, rawFloatBits, nvnProc)
    mov r12, r2
    vmov s0, r1
    mov r1, #1
    mov r2, #0
    mov r3, #0
    bx r12
.ltorg

.macro compose_hook name, site
\name:
    ldr r1, =(\site + 4)
    sub r1, lr, r1
    b draw_frame
.endm
compose_hook worker_composite, 0xbbd8d8
compose_hook main_composite, 0xbbe714
.ltorg

.section .rodata,"a"
.macro shader symbol, path
    .balign 16
    .global \symbol
    .hidden \symbol
\symbol:
    .incbin "\path"
.endm
shader vertex_control, src/shaders/vertex.ctrl
shader edge_control, src/shaders/edge.ctrl
shader vertex_code, src/shaders/vertex.gpu
.global vertex_code_end
.hidden vertex_code_end
vertex_code_end:
shader edge_code, src/shaders/edge.gpu
.global edge_code_end
.hidden edge_code_end
edge_code_end:
