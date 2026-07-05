.global _start
.section .multiboot, "a", @progbits
.align 4
.long 0x1BADB002
.long 0x00000007
.long -(0x1BADB002 + 0x00000007)

.long 0
.long 0
.long 0
.long 0
.long 0

.long 0
.long 1024
.long 768
.long 32

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.type _start, @function
_start:
    movl $stack_top, %esp

    pushl %ebx
    pushl %eax

    call start_demidos_pro

    cli
1:
    hlt
    jmp 1b

.size _start, .- _start
