toiletfs
========

A FUSE filesystem to collect and store coredumps.

Why?
-----
Traditional web applications will "pre-fork" in order to improve parallelism
but still support underlying runtimes which do not support multithreading
correctly. It's not uncommon to have more than a dozen php or ruby processes,
each consuming 400+ MB of memory. 

Imagine a situation where all of them crash at the same time. All of the
processes will dump core to disk in parallel. The disk will thrash, seeking
endlessly between a dozen different huge files being written at once.

An easy solution is to simply disable core dumps, but if you want to be able
to fix these bugs, it would be really nice to have a core. Plus if they all
crashed at the same time, you probably only need one core anyway.

So...
-----
toiletfs solves this problem by allowing only a single file to be opened at
a time. When the kernel tries to open a dozen cores on disk, the first will
succeed, and the rest will fail. 

toiletfs also supports a executing a shell script after close(), to create a
bug report, or alert your monitoring infrastructure.

Why can't you just use core_pattern?
------
https://bugzilla.redhat.com/show_bug.cgi?id=566460
At some point I'd like to go back and fix this in the kernel, but for
production servers, I'd like a solution right now. The kernel fix is complicated
enough that it likely won't be eligible for backport into a stable distribution
kernel and I'd prefer to avoid maintaining my own binary kernel packages.
