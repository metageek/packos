/* boot.S - bootstrap the kernel */
/* Copyright (C) 1999, 2001  Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#define ASM     1
#include "multiboot.h"

        .text

        .globl  start, _start, data_start
start:
_start:
        jmp     multiboot_entry

        /* Align 32 bits boundary. */
        .align  4

        /* Multiboot header. */
multiboot_header:
        /* magic */
        .long   MULTIBOOT_HEADER_MAGIC
        /* flags */
        .long   MULTIBOOT_HEADER_FLAGS
        /* checksum */
        .long   -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
#ifndef MULTIBOOT_ELF
        /* header_addr */
        .long   multiboot_header
        /* load_addr */
        .long   _start
        /* load_end_addr */
        .long   _edata
        /* bss_end_addr */
        .long   _end
        /* entry_addr */
        .long   multiboot_entry
#endif /* ! MULTIBOOT_ELF */

multiboot_entry:
        /* Initialize the stack pointer. */
        movl    $(stack + STACK_SIZE), %esp

        /* Reset EFLAGS. */
        pushl   $0
        popfl

        /* Push the pointer to the Multiboot information structure. */
        pushl   %ebx
        /* Push the magic value. */
        pushl   %eax

        call    EXT_C(kcls)
        pushl   $booting_message
        call    EXT_C(kputs)

        /* Now enter the C main function... */
        call    EXT_C(kernelMain)

        /* Halt. */
        pushl   $halt_message
        call    EXT_C(kputs)

        cli
loop:   hlt
        jmp     loop

        /* Our stack area. */
        .comm   stack, STACK_SIZE, 4096

booting_message:
        .asciz "Booting.\n"
        
halt_message:
        .asciz  "Halted.\n"

        .data
data_start:
        .asciz "data_start"
