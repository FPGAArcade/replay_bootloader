.extern CMain
    
.text
.code 32
.align 0

.global start
start:
    b       Reset
    b       0x00102008
    b       0x00102010
    b       0x00102018
    b       0x00102020
    b       0x00102028
    b       0x00102030
    b       0x00102038

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
