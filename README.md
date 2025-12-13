todo:
- use XSAVEOPT in inline asm
- io_uring on blocking io
- growable stacks and stack splitting
- rand() % thread_count in the stealing loop is risky because rand() often involves a global lock in libc. A thread-local PRNG would be faster.
- double check atomic memory ordering
- dynamic stacks are optional
