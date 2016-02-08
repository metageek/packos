# packos
PackOS is a microkernel where all IPC is IPv6.

I developed it around 2006-2007.  It started as a class project, in
which I was developing a pseudo-OS that lived inside a Linux process;
later, I ported it to i486.  The catch was that I never got memory
protection working--and, when I went to implement it, I found that,
back at the beginning, I had written code that shared data across
process boundaries.  So now I'm dusting it off and fixing that.

To build PackOS, you'll need gcc with support for compiling 32-bit
binaries.

To run PackOS under a virtual machine on a Linux system, you'll need
QEMU, and you'll need to make the disk image mountable as /mnt/packos.
Let's say your packos Git checkout is at directory $PACKOS (so that
this README.md is $PACKOS/README.md).  Then, on my system, I use the
following line in my /etc/fstab:

$PACKOS/kernel/floppy.img /mnt/packos ext2 loop,noauto,user

...although, of course, I have to have the actual value of $PACKOS in
the file.

Then, in $PACKOS, type:

make

which will build the kernel and some utilities.  Then you can go into,
say, $PACKOS/kernel and type ./run (note that you have to be in the
directory with the run script in order for it to work; it's not smart
enough to find its own directory); the run script will mount
/mnt/packos, copy the kernel into place, unmount /mnt/packos, and
start QEMU.  You'll get a window looking like a PC's console, with a
GRUB prompt with one choice: "PackOS on floppy" (the virtual disk with
the kernel is pretending to be a floppy).  Press Return and you can
watch the program run.

There are run scripts in three of the subdirectories under
$PACKOS/samples, too.  One of them, pci, requires some extra setup:
you'll have to set up a TUN/TAP device named tap0.  Some commands to
do this:

tunctl -u $SELF # where $SELF is your username
ifconfig tap0 up add fe80::3602:86ff:fe68:bcef/64 # arbitrarily chosen
                                                  # IPv6 address, with
                                                  # link scope.

Unfortunately, this one currently doesn't work (it used to, before I
let PackOS go fallow for 9 years...); it gives an error message:
"can't find IRQ router.  No IRQs will be available." -- and, without
any IRQs, it can't register to talk to the virtual Ethernet card QEMU
is providing, so it quits.
