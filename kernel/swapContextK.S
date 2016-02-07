#define ASM     1

#include "swapContextK.h"
#ifndef SWAP_VIA_TSS
#include "multiboot.h"
#include <packos/sys/contextP.h>

/*#define DEBUG*/

.text
        .globl swapContext, loadContext

/* What do we do about saving CS? */

swapContext:

        pushfl
        popl PACKOS_CONTEXT_TSS_OFFSET_EFLAGS(%eax)

        movl 4(%esp),%eax
        movl 8(%esp),%ebx
        movl (%esp),%ecx

#ifdef DEBUG
        pushl %eax
        pushl %ecx
        pushl %ebx
        pushl %eax
        pushl $swapContextMsg1
        call EXT_C(kprintf)
        popl %eax
        popl %eax
        popl %ebx
        popl %ecx
        popl %eax

        pushl %eax
        pushl PACKOS_CONTEXT_TSS_OFFSET_ESP(%eax)
        pushl %eax
        pushl $swapContextMsg2
        call EXT_C(kprintf)
        popl %eax
        popl %eax
        popl %eax
        popl %eax
#endif

        movl %esp, PACKOS_CONTEXT_TSS_OFFSET_ESP(%eax)
        movl $swapContextRet, PACKOS_CONTEXT_TSS_OFFSET_EIP(%eax)
        movw %ss, PACKOS_CONTEXT_TSS_OFFSET_SS(%eax)
        movw %ds, PACKOS_CONTEXT_TSS_OFFSET_DS(%eax)
        movw %es, PACKOS_CONTEXT_TSS_OFFSET_ES(%eax)
        movw %cs, PACKOS_CONTEXT_TSS_OFFSET_CS(%eax)
        movl %eax, PACKOS_CONTEXT_TSS_OFFSET_EAX(%eax)
        movl %ebx, PACKOS_CONTEXT_TSS_OFFSET_EBX(%eax)
        movl %ecx, PACKOS_CONTEXT_TSS_OFFSET_ECX(%eax)
        movl %edx, PACKOS_CONTEXT_TSS_OFFSET_EDX(%eax)
        movl %esi, PACKOS_CONTEXT_TSS_OFFSET_ESI(%eax)
        movl %edi, PACKOS_CONTEXT_TSS_OFFSET_EDI(%eax)
        movl %ebp, PACKOS_CONTEXT_TSS_OFFSET_EBP(%eax)

loadContext:
        movl 8(%esp),%eax

#ifdef DEBUG
        movl PACKOS_CONTEXT_TSS_OFFSET_ESP(%eax),%ecx
        pushl %eax
        pushl (%ecx)
        pushl %ecx
        pushl %eax
        pushl $loadContextMsg1
        call EXT_C(kprintf)
        popl %eax
        popl %eax
        popl %eax
        popl %eax
        popl %eax
#endif
        
swapContext4:
#ifdef DEBUG
        pushl %eax
        pushl %ss
        pushl %esp
        pushl $loadContextMsg5
        call EXT_C(kprintf)
        popl %eax
        popl %eax
        popl %eax
        popl %eax
        
        pushl %eax
        pushl PACKOS_CONTEXT_TSS_OFFSET_EIP(%eax)
        pushl PACKOS_CONTEXT_TSS_OFFSET_CS(%eax)
        pushl PACKOS_CONTEXT_TSS_OFFSET_ESP(%eax)
        pushl PACKOS_CONTEXT_TSS_OFFSET_SS(%eax)
        pushl $loadContextMsg2
        call EXT_C(kprintf)
        popl %eax
        popl %eax
        popl %eax
        popl %eax
        popl %eax
        popl %eax
#endif

swapContext3:
#ifdef DEBUG
        pushl %eax
        pushl %es
        pushl %ds
        pushl %cs
        pushl %ss
        pushl $loadContextMsg3
        call EXT_C(kprintf)
        popl %eax
        popl %eax
        popl %eax
        popl %eax
        popl %eax
        popl %eax

        pushl %eax
        pushl PACKOS_CONTEXT_TSS_OFFSET_ES(%eax)
        pushl PACKOS_CONTEXT_TSS_OFFSET_DS(%eax)
        pushl PACKOS_CONTEXT_TSS_OFFSET_CS(%eax)
        pushl PACKOS_CONTEXT_TSS_OFFSET_SS(%eax)
        pushl $loadContextMsg3
        call EXT_C(kprintf)
        popl %eax
        popl %eax
        popl %eax
        popl %eax
        popl %eax
        popl %eax
#endif

swapContext2cs:
        movw PACKOS_CONTEXT_TSS_OFFSET_CS(%eax), %bx
        movw %bx, badFarJumpDest+4
swapContext2eip:
        movl PACKOS_CONTEXT_TSS_OFFSET_EIP(%eax), %ebx
        movl %ebx, badFarJumpDest

        movl PACKOS_CONTEXT_TSS_OFFSET_EBX(%eax), %ebx
        movl PACKOS_CONTEXT_TSS_OFFSET_ECX(%eax), %ecx
        movl PACKOS_CONTEXT_TSS_OFFSET_EDX(%eax), %edx
        movl PACKOS_CONTEXT_TSS_OFFSET_ESI(%eax), %esi
        movl PACKOS_CONTEXT_TSS_OFFSET_EDI(%eax), %edi
        movl PACKOS_CONTEXT_TSS_OFFSET_EAX(%eax), %ebp
        movl %eax, currentContext

swapContext2ds:
        movw PACKOS_CONTEXT_TSS_OFFSET_DS(%eax), %ds
swapContext2es:
        movw PACKOS_CONTEXT_TSS_OFFSET_ES(%eax), %es
swapContext2fs:
        movw PACKOS_CONTEXT_TSS_OFFSET_FS(%eax), %fs
swapContext2gs:
        movw PACKOS_CONTEXT_TSS_OFFSET_GS(%eax), %gs
swapContext2ss:
        movw PACKOS_CONTEXT_TSS_OFFSET_SS(%eax), %ss
swapContext2esp:
        movl PACKOS_CONTEXT_TSS_OFFSET_ESP(%eax), %esp

swapContext2flags:
        pushl PACKOS_CONTEXT_TSS_OFFSET_EFLAGS(%eax)
        popfl
swapContext2eax:
        movl PACKOS_CONTEXT_TSS_OFFSET_EAX(%eax), %eax
swapContext1:
        ljmp *badFarJumpDest
swapContextRet:
#ifdef DEBUG
        pushl %eax
        pushl 4(%esp)
        pushl $swapContextRetMsg
        call EXT_C(kprintf)
        popl %eax
        popl %eax
        popl %eax
#endif
        ret

        .align 32
badFarJumpDest:
        .word 0,0,0

swapContextMsg1:
        .asciz "swapContext(%p,%p): return addr %p\n"
swapContextMsg2:
        .asciz "\tstored esp in %p: %p\n"
loadContextMsg1:
        .asciz "loadContext(%p): stack: %p; top-of-stack: %p\n" 
loadContextMsg2:
        .asciz "ctx->ss: %x, ctx->esp: %p, ctx->cs: %x, ctx->eip: %p\n"
loadContextMsg3:
        .asciz "ss: %x cs: %x ds: %x es: %x\n"
loadContextMsg5:
        .asciz "esp: %p ss: %x\n"
swapContextRetMsg:
        .asciz "swapContextRet: %p\n"
#endif
