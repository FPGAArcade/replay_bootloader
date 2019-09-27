.extern Bootrom
    
.text
.code 32
.align 0

.global start
start:
    ldr     sp,     = 0x00203ff8
    bl      Bootrom

  .word     0xB007C0DE
  .word     __bss_start__-0x00200000
  .word	    0x00000000
  .word	    0xfafffaff

