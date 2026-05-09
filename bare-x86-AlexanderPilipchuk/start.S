.intel_syntax noprefix

.section .note.GNU-stack,"",@progbits

.section .text
.align 4
.set MAGIC_NUMBER, 0x1BADB002
.set FLAGS, 0x0
.set CHECKSUM, -(MAGIC_NUMBER + FLAGS)
.long MAGIC_NUMBER
.long FLAGS
.long CHECKSUM


.global start
.extern entry
start:
    cli
    cld
    lea esp, [stack_top]
    call entry


.hang:
    cli
    hlt
    jmp .hang


.global timer_vector_8_stub
.global yield_vector_stub
.extern handle_trap

timer_vector_8_stub:
    pusha
    cld
    mov eax, esp
    push 0
    push eax
    call handle_trap
    add esp, 8
    mov esp, eax
    popa
    iretd

yield_vector_stub:
    pusha
    cld
    mov eax, esp
    push 1
    push eax
    call handle_trap
    add esp, 8
    mov esp, eax
    popa
    iretd

.global restore_context
restore_context:
    mov esp, [esp + 4]
    popa
    iretd


.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:
