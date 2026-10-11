/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Bryo: the master output stage: DC block, MENU > SPEAKER EQ (LOWCUT, BASS+ for the
 * small speaker), the peak limiter and the soft clip, and MENU > USB LEVEL FIXED. master_block: the four tracks'
 * stereo sum in, Q15 stereo out for audio.c. */
/* master: peak limiter in front of the soft clipper. Fast attack (~0.1 ms),
 * ~150 ms release, threshold where tanh is still nearly linear, so chords
 * get quieter instead of crushed. */
#define LIM_T 18000
static int32_t lim_env = LIM_T;
/* MENU > USB LEVEL FIXED (for #42: record over USB with the speaker turned down): the mix goes to master_out at the
 * full MASTER level, USB audio takes that (audio.c uac_tap), and only then does MASTER scale what the DAC gets
 * (usb_fixed_dac). MASTER (0, the default): MASTER before master_out, as always (USB follows the knob) */
static volatile uint8_t fx_usb_fixed;
static __attribute__((noinline)) void usb_fixed_dac(int32_t *out, uint32_t n)   /* audio ISR, after uac_tap */
{
    uint32_t i;
    int32_t m = (int32_t)sys.master_q12;
    for (i = 0; i < 2u * n; i++)
        out[i] = (out[i] * m) >> 12;                /* (|out| <= 32767 after the soft clip: fits) */
}
static volatile uint8_t fx_lowcut;     /* settings: 1 LOWCUT 12 dB/oct ~110 Hz, 2 BASS+ (the small speaker):
                                        * 12 dB/oct ~220 Hz plus the harmonics of the bass (spk_bass) */
static int32_t lc_l1, lc_l2, lc_r1, lc_r2, dc_l, dc_r, dce_l, dce_r;

/* DC blocker (~2 Hz), always on: a leaky integrator of the input (Q6 state) subtracted from it.
 * The >> 12 step keeps its remainder (error feedback, 0..4095) and adds it to the next one, so no
 * part of the step is lost: the state follows the input exactly, down to 0 after the sound stops.
 * (A rounded step of (x - dc) / 4096 would stop moving at |x - dc| < 2048 and leave an offset of up
 * to +-31 at the output after silence.) */
static inline int32_t dc_block(int32_t x, int32_t *dc, int32_t *err)
{
    int32_t e = (x << 6) - *dc + *err, d = e >> 12;
    *err = e - (d << 12);
    *dc += d;
    return x - ((*dc + 32) >> 6);
}

static int32_t lce[4];
static inline int32_t lowcut1(int32_t x, int32_t *lc, int32_t *err, uint32_t sh)   /* x minus its one-pole low-pass */
{
    int32_t e = x - *lc + *err, d = e >> sh;
    *err = e - (d << sh);
    *lc += d;
    return x - *lc;
}

/* BASS+: what the speaker cannot play, heard through its harmonics. The bass below ~150 Hz is clipped at its
 * own envelope (a level-following trapezoid: odd harmonics), then band-passed ~220 Hz..1 kHz and added.
 * The low-pass has 4 poles (#42: with 2, the trapezoid rebuilt the 300 .. 600 Hz of the mix itself, late,
 * and cancelled up to 6 dB of it; now under 0.5 dB) */
static int32_t sb_lp1, sb_lp2, sb_lp3, sb_lp4, sb_env, sb_h1, sb_h2, sb_hl;
static inline int32_t spk_bass(int32_t m)
{
    int32_t a, t, u;
    sb_lp1 += ((m - sb_lp1) * 692) >> 15;
    sb_lp2 += ((sb_lp1 - sb_lp2) * 692) >> 15;
    sb_lp3 += ((sb_lp2 - sb_lp3) * 692) >> 15;
    sb_lp4 += ((sb_lp3 - sb_lp4) * 692) >> 15;
    a = sb_lp4 < 0 ? -sb_lp4 : sb_lp4;
    if (a > sb_env)
        sb_env += (a - sb_env) >> 2;
    else if (sb_env > 0)
        sb_env -= (sb_env >> 11) + 1;
    t = clamp(sb_lp4 * 8, -sb_env, sb_env);
    sb_h1 += (t - sb_h1) >> 5;
    u = t - sb_h1;
    sb_h2 += (u - sb_h2) >> 5;
    u -= sb_h2;
    sb_hl += (u - sb_hl) >> 3;
    return sb_hl * 3;
}

static inline void master_out(int32_t *l, int32_t *r)
{
    int32_t al, ar, a;
    *l = dc_block(*l, &dc_l, &dce_l);
    *r = dc_block(*r, &dc_r, &dce_r);
    if (fx_lowcut) {                  /* two one-pole high-passes, error feedback as dc_block (the */
        uint32_t sh = fx_lowcut == 2u ? 5u : 6u;    /* rounded step stopped at |x - lc| < 32: an offset) */
        int32_t b = fx_lowcut == 2u ? spk_bass((*l + *r) >> 1) : 0;
        *l = lowcut1(*l, &lc_l1, &lce[0], sh);
        *l = lowcut1(*l, &lc_l2, &lce[1], sh) + b;
        *r = lowcut1(*r, &lc_r1, &lce[2], sh);
        *r = lowcut1(*r, &lc_r2, &lce[3], sh) + b;
    }
    al = *l < 0 ? -*l : *l;
    ar = *r < 0 ? -*r : *r;
    a = al > ar ? al : ar;
    if (a > lim_env)
        lim_env += (a - lim_env) >> 2;
    else if (lim_env > LIM_T)
        lim_env -= ((lim_env - LIM_T) >> 12) + 1;
    if (lim_env > LIM_T) {
        int32_t g = (int32_t)(((uint32_t)LIM_T << 15) / (uint32_t)lim_env);   /* < 32768 */
        *l = ((*l >> 4) * g) >> 11;                      /* >> 4 first: |l| may be far above Q15 */
        *r = ((*r >> 4) * g) >> 11;
    }
    *l = softclip(*l);
    *r = softclip(*r);
}


/* The output's fade for flash work (storage_hw.c): erasing the flash stops everything (the code runs from it), and
 * the audio with it, so the sound has to stop then. Cut, it clicks at both ends (and a run of erases, a reel's,
 * stutters). The main loop asks for silence (out_mute_req 1) and waits until out_quiet says the DMA's buffers hold
 * nothing else (OUT_QUIET_BLOCKS: both halves); the gain slides down over OUT_FADE_FRAMES, and back up the same way
 * when it's let go. Held at full, it costs one compare a block. */
#define OUT_FADE_FRAMES 256              /* 5.8 ms either way */
#define OUT_QUIET_BLOCKS (2u * HALF_FRAMES / CTL + 2u)   /* both of the DMA's halves rendered silent, and a margin */
static volatile uint8_t out_mute_req;   /* main loop: 1 fade out and stay silent, 0 fade back in */
static volatile uint16_t out_quiet;     /* ISR: blocks rendered silent in a row (capped) */
static int32_t out_g = 32767;           /* ISR: the gain now, Q15 */
static void out_fade(int32_t *out, uint32_t n)
{
    int32_t tgt = out_mute_req ? 0 : 32767, g = out_g, step = 32767 / OUT_FADE_FRAMES + 1;
    uint32_t i;
    if (g == tgt && g == 32767) {                       /* (playing: untouched) */
        out_quiet = 0;
        return;
    }
    if (g == 0 && tgt == 0) {                           /* (silent: held there) */
        for (i = 0; i < 2u * n; i++)
            out[i] = 0;
        if (out_quiet < 0xFFFFu)
            out_quiet++;
        return;
    }
    for (i = 0; i < n; i++) {
        g = tgt > g ? (g + step > tgt ? tgt : g + step) : (g - step < tgt ? tgt : g - step);
        out[2u * i] = (out[2u * i] * (g >> 3)) >> 12;
        out[2u * i + 1u] = (out[2u * i + 1u] * (g >> 3)) >> 12;
    }
    out_g = g;
    out_quiet = 0;
}

/* the mix (Q15-ish, summed tracks) -> MASTER -> master_out -> interleaved stereo for audio.c */
static void master_block(const int32_t *l, const int32_t *r, int32_t *out, uint32_t n)
{
    uint32_t i;
    int32_t mg = fx_usb_fixed ? MASTER_FULL : (int32_t)sys.master_q12;   /* (USB LEVEL FIXED: MASTER after) */
    for (i = 0; i < n; i++) {
        int32_t ol = (l[i] * mg) >> 12, or_ = (r[i] * mg) >> 12;
        master_out(&ol, &or_);
        out[2u * i] = ol;
        out[2u * i + 1u] = or_;
    }
    out_fade(out, n);                                   /* (flash work: faded, not cut) */
}
