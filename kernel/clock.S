.text
        .globl init_clock, init_8259s, tick, oldidt, noop_handler, PackosKernelIRQ10HandlerS, PackosKernelIRQ16HandlerS, enable_irq_10

#define PIC_MASTER_IMR $0x21
#define PIC_SLAVE_IMR $0xa1
#define PIC_MASTER_CMD $0x20
#define PIC_SLAVE_CMD $0xa0
#define PIC_CASCADE_IR 2

#if 1
#define CLOCK_TIMER_1 $0xae
#define CLOCK_TIMER_2 $0x4d
#else
#define CLOCK_TIMER_1 $1
#define CLOCK_TIMER_2 $1
#endif

init_8259s:
        pushfl
        cli

        pushl %eax

        mov $0xff, %al
        outb %al, PIC_MASTER_IMR
        mov $0xff, %al
        outb %al, PIC_SLAVE_IMR

        mov $0x11, %al
        out %al, PIC_MASTER_CMD
        mov $0x20, %al
        out %al, PIC_MASTER_IMR
        mov (1 << PIC_CASCADE_IR), %al
        outb %al, PIC_MASTER_IMR
        mov 6, %al
        outb %al, PIC_MASTER_IMR

        mov $0x11, %al
        out %al, PIC_SLAVE_CMD
        mov $0x28, %al
        out %al, PIC_SLAVE_IMR
        mov PIC_CASCADE_IR, %al
        outb %al, PIC_SLAVE_IMR
        mov 1, %al
        outb %al, PIC_SLAVE_IMR

        mov $0xfe, %al
        outb %al, PIC_MASTER_IMR

        sidt oldidt
        lidt idt48

        popl %eax
        popfl
        ret

init_clock:
        pushfl
        pushl %eax

        mov $0x34, %al
        outb %al, $0x43

        /* 60 Hz */
        mov CLOCK_TIMER_1, %al
        outb %al,$0x40
        mov CLOCK_TIMER_2, %al
        outb %al,$0x40

        inb PIC_MASTER_IMR,%al
        andb $0xfe,%al
        outb %al,PIC_MASTER_IMR
        
        popl %eax
        popfl
        ret

enable_irq_10:
        inb PIC_SLAVE_IMR,%al
        andb $0xfd,%al
        outb %al,PIC_SLAVE_IMR
        ret        
        
PackosKernelIRQ10HandlerS:
        pushl %eax
        mov $0x20, %al
        out %al, $0x20

        pushw $10
        call PackosKernelIRQHandler
        popw %ax

        popl %eax
        iret

PackosKernelIRQ16HandlerS:
        pushl %eax
        mov $0x20, %al
        out %al, $0x20

        pushw $16
        call PackosKernelIRQHandler
        popw %ax

        popl %eax
        iret

noop_handler:
        mov $0x20, %al
        out %al, $0x20
        iret

tick:
        pushl %eax

        mov $0x60, %al
        out %al, PIC_MASTER_CMD

        movb ticks, %al
        addb $1, %al
        movb %al, ticks
        cmp $10, %al
        jnz tickExit

        movb $0, ticks

#ifdef DEBUG
        pushl %ebx
#endif

        movl $0xb, %eax
        outb %al, PIC_MASTER_CMD	/* isr register */
        movl $0, %eax
        inb PIC_MASTER_CMD, %al
#ifdef DEBUG
        movl %eax, %ebx
#endif
        movl $0xa, %eax
        outb %al, PIC_MASTER_CMD	/* back to the irr register */
        movl $0, %eax
        inb PIC_MASTER_CMD, %al

#ifdef DEBUG
        pushl %ecx
        pushl %edx
        pushl %esi
        pushl %edi
        pushl %ebp

        pushl %eax
        pushl %ebx
        pushl $tickMsg
        call kprintf
        popl %eax
        popl %eax
        popl %eax
        
        popl %ebp
        popl %edi
        popl %esi
        popl %edx
        popl %ecx
        popl %ebx
#endif

        call sendTickPacket

tickExit:               
        popl %eax
tickExit2:
        iret

        .data        
        .align 16
idt48:
        .word 0x800
        .word 0,0

ticks:
        .byte 0

oldidt:
        .word 0,0,0
        
tickMsg:
        .asciz "tick ISR=%x IRR=%x\n"
