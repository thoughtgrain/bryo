/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Shared DSP building blocks for Bryo's devices (all fixed point).
 * Voice output convention: add sample * amp to out[], where a full-scale
 * oscillator at amp = 1.0 (Q15 32767) contributes VOICE_FS. */
#define VOICE_FS 24000           /* per-voice level: one voice peaks near -6 dBFS before the master */

static inline __attribute__((always_inline)) int32_t mulq15(int32_t a, int32_t b) { return (a * b) >> 15; }
/* a * k / 65536 without 64-bit: a up to 2^31, k Q16 */
static inline __attribute__((always_inline)) int32_t mulq16(int32_t a, uint32_t k)
{
    return (int32_t)(((a >> 16) * (int32_t)k) + (int32_t)(((uint32_t)(a & 0xFFFF) * k) >> 16));
}
static inline __attribute__((always_inline)) int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

/* sine, linearly interpolated between the 1024 table points (plain lookup: THD -55 dB) */
static inline __attribute__((always_inline)) int32_t sine_i(uint32_t ph)
{
    uint32_t i = ph >> 22;
    int32_t a = SINE[i], b = SINE[(i + 1u) & 1023u];
    return a + (((b - a) * (int32_t)((ph >> 7) & 0x7FFFu)) >> 15);
}
static inline __attribute__((always_inline)) int32_t osc_sine(uint32_t ph) { return sine_i(ph); }

/* polyBLEP residual (Q15) around a wrap of a phase accumulator */
static inline __attribute__((always_inline)) int32_t blep(uint32_t ph, uint32_t inc)
{
    uint32_t d = inc >> 15;
    int32_t x;
    if (!d)
        return 0;
    if (ph < inc) {
        x = (int32_t)(ph / d);                       /* 0..32767 */
        return x + x - ((x * x) >> 15) - 32768;
    }
    if (ph > 0xFFFFFFFFu - inc) {
        x = -(int32_t)((0xFFFFFFFFu - ph) / d);       /* -32767..0 */
        return ((x * x) >> 15) + x + x + 32768;
    }
    return 0;
}

static inline __attribute__((always_inline)) int32_t osc_saw(uint32_t ph, uint32_t inc)
{
    return ((int32_t)(ph >> 16) - 32768) - blep(ph, inc);
}

static inline __attribute__((always_inline)) int32_t osc_pulse(uint32_t ph, uint32_t inc, uint32_t pw)
{
    return (osc_saw(ph, inc) - osc_saw(ph + pw, inc)) >> 1;
}

static inline __attribute__((always_inline)) int32_t osc_tri(uint32_t ph)
{
    int32_t s = (int32_t)(ph >> 15);                 /* 0..131071 */
    return s < 65536 ? s - 32768 : 98303 - s;
}

/* tanh(x / 32768) * 32767, linearly interpolated (a plain lookup gives
 * 256-unit steps, audible on quiet signals) */
static inline __attribute__((always_inline)) int32_t softclip(int32_t x)
{
    int32_t a = x < 0 ? -x : x, i = a >> 8, y;
    if (i >= 256)
        y = TANH_Q15[256];                           /* continuous with the table */
    else
        y = TANH_Q15[i] + (((TANH_Q15[i + 1] - TANH_Q15[i]) * (a & 255)) >> 8);
    return x < 0 ? -y : y;
}

/* xorshift32 */
static inline __attribute__((always_inline)) uint32_t noise32(int32_t *st)
{
    uint32_t s = (uint32_t)*st;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    *st = (int32_t)s;
    return s;
}

/* Trapezoidal SVF (A. Simper), unconditionally stable. Coefficients per
 * block in Q13; signals stay within +-150000 so products fit in 32 bits. */
typedef struct { int32_t a1, a2, a3, k; } tsvf_t;
static inline __attribute__((always_inline)) void tsvf_coef_k(tsvf_t *c, int32_t cut, int32_t k)   /* cut: 0..127 << 8, k: damping Q12 */
{
    int32_t i, g, den;
    cut = clamp(cut, 0, 127 << 8);
    i = cut >> 8;
    g = SVF_G[i];
    if (i < 127)                                       /* between table points: sweeps without 128 steps */
        g += ((SVF_G[i + 1] - g) * (cut & 255)) >> 8;
    c->k = k;
    den = 4096 + ((g * (g + k)) >> 12);
    c->a1 = (int32_t)((4096u << 13) / (uint32_t)den);
    c->a2 = (c->a1 * g) >> 12;
    c->a3 = (c->a2 * g) >> 12;
}
static inline __attribute__((always_inline)) void tsvf_coef(tsvf_t *c, int32_t cut, int32_t reso)   /* reso 0..127: damping 2.0 .. ~0.15 */
{
    tsvf_coef_k(c, cut, 8192 - reso * 7600 / 127);
}

static inline __attribute__((always_inline)) int32_t tsvf_lp(const tsvf_t *c, int32_t in, int32_t *ic1, int32_t *ic2)
{
    int32_t v3 = in - *ic2;
    int32_t v1 = (c->a1 * *ic1 + c->a2 * v3) >> 13;
    int32_t v2 = *ic2 + ((c->a2 * *ic1 + c->a3 * v3) >> 13);
    *ic1 = clamp(2 * v1 - *ic1, -150000, 150000);
    *ic2 = clamp(2 * v2 - *ic2, -150000, 150000);
    return v2;
}

/* the same filter, one of its three outputs: 0 low-pass, 1 band-pass, 2 high-pass (in - k band - low). Inlined
 * (as tsvf_lp and mulq15): a call a sample with the state through memory was a third of a POLY voice's cost */
static inline __attribute__((always_inline)) int32_t tsvf_mode(const tsvf_t *c, int32_t in, int32_t *ic1, int32_t *ic2,
                                                               uint32_t mode)
{
    int32_t v3 = in - *ic2;
    int32_t v1 = (c->a1 * *ic1 + c->a2 * v3) >> 13;
    int32_t v2 = *ic2 + ((c->a2 * *ic1 + c->a3 * v3) >> 13);
    *ic1 = clamp(2 * v1 - *ic1, -150000, 150000);
    *ic2 = clamp(2 * v2 - *ic2, -150000, 150000);
    return mode == 1u ? v1 : mode == 2u ? in - ((c->k * v1) >> 12) - v2 : v2;
}

/* RES 0..100 as the damping (Q12) for SYNTH's and POLY's filter (2026-10-15: "it would be great if the filters
 * actually would self-resonate; I can't really get much"). To 80 as it was (2.0 down to 0.52); then on to zero at 92
 * and past it, to -0.13 at 100: the loop's gain over one, so from about 93 it rings on its own at the cutoff, a sine
 * that sings over whatever goes in. Past 70 the caller runs tsvf_sat, whose soft limit on the band's state holds
 * that ring at about -6 dBFS whatever the cutoff (and squashes a loud input into it, as an analog filter does) */
static inline __attribute__((always_inline)) int32_t tsvf_kres(int32_t res)
{
    res = clamp(res, 0, 100);
    return res <= 80 ? 8192 - (res * 127 / 100) * 7600 / 127   /* (bit for bit tsvf_coef's: the sounds as they were) */
                     : res <= 92 ? 2148 - (res - 80) * 179 : -(res - 92) * 65;
}
#define TSVF_SAT_RES 70          /* (RES over this: tsvf_sat) */
#define TSVF_KNEE 8000           /* (the band's state: linear to this, a quarter of the rest past it) */
/* tsvf_mode with the soft limit on the band's state: a negative damping's ring grows until it reaches the knee and
 * no further (measured: about 16,500 at the output's << 1, CUT 20 to 120). Two compares a sample over tsvf_mode */
static inline __attribute__((always_inline)) int32_t tsvf_sat(const tsvf_t *c, int32_t in, int32_t *ic1, int32_t *ic2,
                                                              uint32_t mode)
{
    int32_t v3 = in - *ic2;
    int32_t v1 = (c->a1 * *ic1 + c->a2 * v3) >> 13;
    int32_t v2 = *ic2 + ((c->a2 * *ic1 + c->a3 * v3) >> 13);
    int32_t b = clamp(2 * v1 - *ic1, -150000, 150000);
    *ic1 = b > TSVF_KNEE ? TSVF_KNEE + ((b - TSVF_KNEE) >> 2) : b < -TSVF_KNEE ? -TSVF_KNEE + ((b + TSVF_KNEE) >> 2) : b;
    *ic2 = clamp(2 * v2 - *ic2, -150000, 150000);
    return mode == 1u ? v1 : mode == 2u ? in - ((c->k * v1) >> 12) - v2 : v2;
}

/* SYNTH's and POLY's envelopes, a step a block (as Felucca's): e Q24, st 0 silent, 1 attack, 2 decay (and sustain),
 * 3 release. ty, ETYP (2026-10-15): ET_ADSR, as it always was, bit for bit; ET_AD, the decay to silence whatever the
 * key (a release decays at DEC too), then done; ET_LOOP, while held, decay to 1/64 and attack again */
enum { ET_ADSR, ET_AD, ET_LOOP };
static inline __attribute__((always_inline)) void env_step(int32_t *e, uint8_t *st, int32_t atk, int32_t dec, int32_t sus,
                                                           int32_t rel, uint32_t ty)
{
    switch (*st) {
    case 1:
        *e += (int32_t)ENV_LIN[atk & 127];
        if (*e >= 1 << 24) {
            *e = 1 << 24;
            *st = 2;
        }
        break;
    case 2:
        if (ty == ET_ADSR) {
            *e += mulq16(sus * ((1 << 24) / 100) - *e, ENV_EXP[dec & 127]);
            break;
        }
        *e -= mulq16(*e, ENV_EXP[dec & 127]);
        if (ty == ET_LOOP && *e < 1 << 18) {
            *st = 1;
        } else if (*e < 1 << 12) {
            *e = 0;
            *st = 0;
        }
        break;
    case 3:
        *e -= mulq16(*e, ENV_EXP[(ty == ET_AD ? dec : rel) & 127]);
        if (*e < 1 << 12) {
            *e = 0;
            *st = 0;
        }
        break;
    default:
        *e = 0;
        break;
    }
}

/* TRAN, a transient (SYNTH, POLY, DRUM, 2026-10-14): 3 ms of noise that falls as it goes, through a one-pole low-pass
 * at about 9 kHz (bright, not harsh), at level g (Q15: up to about -4 dBFS at the start, so it can stand out of a
 * sound at its own level). n counts the samples left (CLICK_N at a note's start), rng and lp are its noise and its
 * filter; the caller adds the result where the source's own filter can shape it. About 8 instructions a sample
 * while it lasts, nothing after */
#define CLICK_N 132u
static inline __attribute__((always_inline)) int32_t click_step(uint32_t *n, int32_t *rng, int32_t *lp, int32_t g)
{
    int32_t x = (int32_t)(noise32(rng) >> 17) - 16384, e = (int32_t)*n * 248;   /* (132 x 248: full at the start) */
    (*n)--;
    *lp += ((x - *lp) * 24000) >> 15;
    return (((*lp * e) >> 15) * g) >> 14;
}

/* x / 2^k rounded toward zero, as `/` rounds, without the divide: the target compiler keeps a signed division by a
 * constant as a hardware divide, even a power of two (x86 at -Os does too, so the host counts them). For the
 * per-sample paths; a plain >> k rounds toward minus infinity, which a decaying tail never leaves (-1 >> k is -1) */
static inline __attribute__((always_inline)) int32_t sdiv2(int32_t x, uint32_t k)
{
    return (x + ((x >> 31) & (int32_t)((1u << k) - 1u))) >> k;
}

/* Two divides the knobs ask for every block, as tables (the same integer arithmetic, so the same values):
 * x * 32767 / 100 for a 0..100 knob (a WET, a share, a level as Q15), and db_q10's gain for -24..24 dB (2x per
 * 6 dB, straight between: tape.c db_q10 reads it). Each divide is 20-odd cycles on the target, and these ran a
 * dozen or more times a block across the devices, for values that never move. */
static const int16_t PCT_Q15[101] = {
    0, 327, 655, 983, 1310, 1638, 1966, 2293, 2621, 2949, 3276, 3604, 3932, 4259, 4587, 4915,
    5242, 5570, 5898, 6225, 6553, 6881, 7208, 7536, 7864, 8191, 8519, 8847, 9174, 9502, 9830, 10157,
    10485, 10813, 11140, 11468, 11796, 12123, 12451, 12779, 13106, 13434, 13762, 14089, 14417, 14745, 15072, 15400,
    15728, 16055, 16383, 16711, 17038, 17366, 17694, 18021, 18349, 18677, 19004, 19332, 19660, 19987, 20315, 20643,
    20970, 21298, 21626, 21953, 22281, 22609, 22936, 23264, 23592, 23919, 24247, 24575, 24902, 25230, 25558, 25885,
    26213, 26541, 26868, 27196, 27524, 27851, 28179, 28507, 28834, 29162, 29490, 29817, 30145, 30473, 30800, 31128,
    31456, 31783, 32111, 32439, 32767};
static const int8_t PCT_127[101] = {   /* x * 127 / 100: a 0..100 RES knob as the SVF's damping */
    0, 1, 2, 3, 5, 6, 7, 8, 10, 11, 12, 13, 15, 16, 17, 19, 20, 21, 22, 24, 25, 26, 27, 29, 30, 31, 33, 34, 35, 36,
    38, 39, 40, 41, 43, 44, 45, 46, 48, 49, 50, 52, 53, 54, 55, 57, 58, 59, 60, 62, 63, 64, 66, 67, 68, 69, 71, 72,
    73, 74, 76, 77, 78, 80, 81, 82, 83, 85, 86, 87, 88, 90, 91, 92, 93, 95, 96, 97, 99, 100, 101, 102, 104, 105, 106,
    107, 109, 110, 111, 113, 114, 115, 116, 118, 119, 120, 121, 123, 124, 125, 127};
static const int16_t DB_Q10[49] = {
    64, 75, 86, 96, 107, 118, 128, 150, 171, 192, 214, 235,
    256, 299, 342, 384, 427, 470, 512, 598, 683, 768, 854, 939,
    1024, 1194, 1365, 1536, 1706, 1877, 2048, 2389, 2730, 3072, 3413, 3754,
    4096, 4778, 5461, 6144, 6826, 7509, 8192, 9557, 10922, 12288, 13653, 15018,
    16384};

/* Blocks are always CTL long, so x / CTL is a shift (the compiler at -Os keeps even constant divisions as a
 * hardware divide) */
#define CTL_LOG2 5
#if (1 << CTL_LOG2) != CTL
#error "CTL_LOG2 does not match CTL"
#endif
/* linear up to k, then a soft knee: only the peaks above k saturate (|y| - k must stay < 2^30) */
static inline __attribute__((always_inline)) int32_t soft_knee(int32_t y, int32_t k)
{
    int32_t a = y < 0 ? -y : y;
    if (a <= k)
        return y;
    a = k + (softclip((a - k) * 2) >> 1);
    return y < 0 ? -a : a;
}

/* phase increment of pitch16 (1/16 semitone) plus ct cents: whole 1/16 semitones from the table,
 * the rest (and fine, in 1/4096) as a factor (1 ct = 2.367 / 4096) */
static inline uint32_t cents_inc(int32_t pitch16, int32_t ct, int32_t fine)
{
    int32_t d16 = ct * 16 / 100, rem = ct * 16 - d16 * 100;          /* rem: 1/1600 semitone */
    uint32_t inc = pitch_inc(clamp(pitch16 + d16, 0, 2047));
    return inc + (uint32_t)((int32_t)(inc >> 12) * (rem * 2367 / 16000 + fine));
}
