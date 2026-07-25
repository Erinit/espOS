/* kernel.ld */
ENTRY(_start)

MEMORY
{
    /* IRAM: 128KB mapped for executable code */
    iram0_0_seg (RX) : ORIGIN = 0x40080000, LENGTH = 0x20000 
    
    /* DRAM: 160KB mapped for read/write data and the stack */
    dram0_0_seg (RW) : ORIGIN = 0x3FFB0000, LENGTH = 0x28000 
}

SECTIONS
{
    /* Group executable code and literal pools together in IRAM */
    .text : ALIGN(4)
    {
        *(.literal.boot)
        *(.text.boot)
        *(.literal .text .literal.* .text.*)
    } > iram0_0_seg

    /* .rodata contains constant variables and string literals */
    .rodata : ALIGN(4)
    {
        *(.rodata)
        *(.rodata.*)
    } > dram0_0_seg

    /* .data contains initialized variables */
    .data : ALIGN(4)
    {
        *(.data)
        *(.data.*)
    } > dram0_0_seg

    /* .bss contains uninitialized variables */
    .bss : ALIGN(4)
    {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    } > dram0_0_seg
}