/* boot.S */
.section .text.boot
.global _start

_start:
    /* 
     * Job 1: Set up the Stack Pointer.
     * In the Xtensa architecture, register 'a1' is the stack pointer (sp).
     * We point it to the very end of the DRAM block we defined in kernel.ld.
     * DRAM starts at 0x3FFB0000 and has a length of 0x28000 (160KB).
     * 0x3FFB0000 + 0x28000 = 0x3FFD8000.
     * We align it just below the boundary to be safe.
     */
    movi a1, 0x3FFD7FF0  

    /* 
     * Job 2: Jump to the C kernel.
     * Xtensa uses "Register Windows" for function calls.
     * 'call4' rotates the register window by 4 and jumps to the function.
     */
    call4 kernel_main

.halt:
    /* 
     * Job 3: The safety net.
     * An operating system kernel should never "return".
     * If kernel_main accidentally finishes, we trap the CPU in an infinite loop.
     */
    j .halt