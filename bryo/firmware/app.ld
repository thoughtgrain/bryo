/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Bryo memory map.
 * Code + constants execute in place from XIP (app.bin offset 0 = 0x02000120).
 * 0x01C00000..0x01C07F00 holds .ram_text (code that must run while the flash
 * is busy: the flash driver), then .lowpool. (0x01C06000.. was kept for RAM-run debug until 2026-10-12; the
 * last 256 B, the boot probe crt0 writes at 0x01C07F00, stay out of it.)
 * The rest of RAMTEXT holds .lowpool: zero-initialised buffers moved out of the pool (2026-10-12), so the pool
 * could give its top to .hot (56 KiB since 2026-10-13).
 * RAM  : .data/.bss (96 KiB); .data holds the constant tables the audio path reads (HOT_TABLE, felucca_tables.h)
 * POOL : big zero-initialised buffers (FX lines, canvases), section .pool
 * HOT  : code run from RAM that calls flash code and is called from it (.hot): the audio path, which from flash
 *        missed the cache every block. High in RAM on purpose: a call reaches 4 MiB either way (fm1_flash.h) and
 *        calls only go to code, the start of .text (its constants come after); from here all of it is in reach both
 *        ways; RAMTEXT, 0x01C00000, isn't. 72 KiB since 2026-10-18 (SYNTH's voice moved in): the pool gave 16 KiB,
 *        its grain slots moved to main RAM
 * STACKS with 256-byte guard bands; NOINIT survives resets and UBOOT entry.
 * 0x01C7FD50.. is reserved: boot_info, boot-state mailbox, vector table. */
MEMORY
{
    XIP    (rx)  : ORIGIN = 0x02000120, LENGTH = 0x8DFBC
    RAMTEXT (rwx): ORIGIN = 0x01C00000, LENGTH = 0x7F00
    RAM    (rwx) : ORIGIN = 0x01C08000, LENGTH = 96K
    POOL   (rw)  : ORIGIN = 0x01C20000, LENGTH = 0x42000
    HOT    (rwx) : ORIGIN = 0x01C62000, LENGTH = 0x12000
    NOINIT (rw)  : ORIGIN = 0x01C7C000, LENGTH = 0x3D50
}
SECTIONS
{
    .text : { KEEP(*(.text.crt0)) *(.text*) *(.rodata*) . = ALIGN(4); _etext = .; } > XIP
    .ram_text : { _rt_start = .; KEEP(*(.ram_text*)) . = ALIGN(4); _rt_end = .; } > RAMTEXT AT > XIP
    _rt_load = LOADADDR(.ram_text);
    .lowpool (NOLOAD) : { . = ALIGN(4); _lp_start = .; *(.lowpool*) . = ALIGN(4); _lp_end = .; } > RAMTEXT
    .hot : { . = ALIGN(4); _hot_start = .; KEEP(*(.hot*)) . = ALIGN(4); _hot_end = .; } > HOT AT > XIP
    _hot_load = LOADADDR(.hot);
    .data : { . = ALIGN(4); _data_start = .; *(.data*) *(.sdata*) . = ALIGN(4); _data_end = .; } > RAM AT > XIP
    _data_load = LOADADDR(.data);
    .bss (NOLOAD) : { _bss_start = .; *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(4); _bss_end = .; } > RAM
    .pool (NOLOAD) : { _pool_start = .; *(.pool*) . = ALIGN(4); _pool_end = .; } > POOL
    .noinit (NOLOAD) : { *(.noinit*) } > NOINIT
    _guard0 = 0x01C74000; _ustack_lo = 0x01C74100; _ustack_top = 0x01C7A000;
    _guard1 = 0x01C7A000; _sstack_lo = 0x01C7A100; _sstack_top = 0x01C7C000;
    /DISCARD/ : { *(.comment) *(.note*) }
}
