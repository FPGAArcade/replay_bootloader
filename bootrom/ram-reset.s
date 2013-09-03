.extern Bootrom
    
.text
.code 32
.align 0

.global start
start:
    ldr     sp,     = 0x00203ff8
    bl      Bootrom
