.section .text.boot
.global _start
.global kernel_main

_start:
    /* * Job 1: Set up the Stack Pointer.
     * In the Xtensa architecture, register 'a1' is the stack pointer (sp).
     * We point it to the very end of the DRAM block we defined in kernel.ld.
     * DRAM starts at 0x3FFB0000 and has a length of 0x28000 (160KB).
     * 0x3FFB0000 + 0x28000 = 0x3FFD8000.
     * We align it just below the boundary to be safe.
     */
    movi a1, 0x3FFD7FF0  

    /* 2. THE PERMANENT BSS FIX: Zero out uninitialized data */
    movi a2, _bss_start      /* a2 = Start of BSS */
    movi a3, _bss_end        /* a3 = End of BSS */
    movi a4, 0               /* a4 = 0 (The value we will write) */

clear_bss_loop:
    bgeu a2, a3, clear_bss_done  /* If Start >= End, exit the loop  */
    s32i a4, a2, 0               /* Store 32-bit Zero (a4) at address (a2)  */
    addi a2, a2, 4               /* Move to the next 32-bit block (+4 bytes)  */
    j clear_bss_loop             /* Loop back  */

clear_bss_done:
    /* * Job 2: Jump to the C kernel.
     * Xtensa uses "Register Windows" for function calls.
     * 'call4' rotates the register window by 4 and jumps to the function.
     */
    call4 kernel_main

.halt:
    /* * Job 3: The safety net.
     * An operating system kernel should never "return".
     * If kernel_main accidentally finishes, we trap the CPU in an infinite loop.
     */
    j .halt