# packos
PackOS is a microkernel where all IPC is IPv6.

I developed it around 2006-2007.  It started as a class project, in
which I was developing a pseudo-OS that lived inside a Linux process;
later, I ported it to i486.  The catch was that I never got memory
protection working--and, when I went to implement it, I found that,
back at the beginning, I had written code that shared data across
process boundaries.  So now I'm dusting it off and fixing that.
