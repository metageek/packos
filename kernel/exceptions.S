.text
        .globl packosKernelGPFHandler, packosKernelInvalidOpCodeHandler, packosKernelPageFaultHandler

packosKernelGPFHandler:

        cli
        movl 12(%esp),%eax
        movl (%esp),%ebx
        movl 8(%esp),%ecx
        movl 4(%esp),%edx

        pushl %edx
        pushl %ecx
        pushl %ebx
        pushl %eax
        pushl $13
        call packosKernelMiscHandlerWithError
        popl %eax
        popl %eax
        popl %ebx
        popl %ecx
        popl %edx

        iret
        
packosKernelPageFaultHandler:

        cli
        movl 12(%esp),%eax
        movl (%esp),%ebx
        movl 8(%esp),%ecx
        movl 4(%esp),%edx

        pushl %edx
        pushl %ecx
        pushl %ebx
        pushl %eax
        pushl $14
        call packosKernelMiscHandlerWithError
        popl %eax
        popl %eax
        popl %ebx
        popl %ecx
        popl %edx

        iret
        
packosKernelInvalidOpCodeHandler:

        cli
        movl 8(%esp),%eax
        movl 4(%esp),%ecx
        movl (%esp),%edx

        pushl %edx
        pushl %ecx
        pushl %eax
        pushl $6
        call packosKernelMiscHandlerNoError
        popl %eax
        popl %eax
        popl %ecx
        popl %edx

        iret
