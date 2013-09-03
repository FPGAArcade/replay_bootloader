.extern CMain
    
.text
.code 32
.align 0

.global start
start:
    b       Reset
    b       0x00102004
    b       0x00102008
    b       0x0010200C
    b       0x00102010
    b       0x00102014
    b       0x00102018
    b       0x0010201C

Reset:
    ldr     sp,     = 0x00203ff8
    bl      CMain

Fiq:
    b       Fiq
UndefinedInstruction:
    b       UndefinedInstruction
SoftwareInterrupt:
    b       SoftwareInterrupt
PrefetchAbort:
    b       PrefetchAbort
DataAbort:
    b       DataAbort
Reserved:
    b       Reserved
Irq:
    b       Irq

.global CallRam
CallRam:
    ldr     r3,     = 0x00200000
    bx      r3
