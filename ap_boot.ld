SECTIONS
{
    . = 0x8000;
    .text : { *(.text) }
    /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame) }
}
