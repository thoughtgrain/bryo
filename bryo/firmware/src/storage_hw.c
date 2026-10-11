/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The firmware side of storage.c (FELUCCA_FLASH): flash reads in short IRQ-off windows so the
 * audio keeps up, erases and writes only inside the storage areas, the audio buffer silenced
 * first. The host tests (storage_test.c) provide their own st_* on a simulated NOR. */
static uint8_t flash_ok;                 /* JEDEC id matched at boot (persist_boot) */
static int st_read(uint32_t off, void *dst, uint32_t n)   /* 256-byte IRQ-off windows: audio keeps up */
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc)
            return rc;
        off += k;
        d += k;
        n -= k;
    }
    return 0;
}
static void audio_silence(void)                 /* IRQs off: the DMA would loop stale audio (buzz) */
{
    uint32_t i;
    for (i = 0; i < sizeof abuf / sizeof abuf[0]; i++)
        abuf[i] = 0;
}
/* Erases stop the audio (the code runs from the flash): before one, the output fades out (master.c out_fade) and
 * this waits until the DMA's buffers hold only silence (30 ms at most: before the audio runs, at boot, nothing
 * comes); the gap stays open over a run of erases (a reel's) and st_poll lets it go 40 ms after the last, the sound
 * fading back in. One soft gap, not a click at each end and a stutter between. Programming a page (well under a
 * half buffer) doesn't need it. */
static uint8_t st_gap;                  /* the output is held silent for flash work */
static uint32_t st_gap_t;               /* fm1_ms at the last erase */
static void st_quiet(void)
{
    uint32_t t0 = fm1_ticks(), h0 = audio_halves, dt;
    out_mute_req = 1;
    if (!st_gap)                                        /* (TIMER4: it counts with the interrupts off and before
                                                         * they're on; no half played in 8 ms: no audio running) */
        while (out_quiet < OUT_QUIET_BLOCKS && (dt = fm1_ticks() - t0) < 30000u * FM1_TICKS_PER_US &&
               (audio_halves != h0 || dt < 8000u * FM1_TICKS_PER_US))
            ;
    st_gap = 1;
    st_gap_t = fm1_ms;
}
/* main loop: the gap let go once the flash work has stopped */
static void st_poll(void)
{
    if (st_gap && fm1_ms - st_gap_t >= 40u) {
        st_gap = 0;
        out_mute_req = 0;
    }
}
static int st_erase(uint32_t off)
{
    uint32_t took, f;
    int rc;
    if (!FL_STORE_OK(off, 0x1000u))
        return -8;
    st_quiet();
    f = irq_save();
    audio_silence();
    rc = FL_FAR(fl_erase4k_ram)(off, &took);
    irq_restore(f);
    return rc;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    if (!FL_STORE_OK(off, n))                   /* only inside the storage areas */
        return -8;
    return fl_write(off, src, n);
}
