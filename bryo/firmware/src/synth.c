/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: SYNTH, a source engine (docs/bryo-architecture.md, "Source engines"). A small subtractive voice taken from
 * Felucca's ANALOG engine: two band-limited oscillators of one wave (the second DTUN cents up), white noise, drive,
 * a resonant trapezoidal low-pass, and one ADSR that sets the level and, by ENV, moves the cutoff. Up to SYN_NV
 * voices per track (VOIC); at 1 it plays legato, gliding by GLID.
 *
 * Keys: the white keys of the focused track, read by the audio ISR once per block (chain.c), so a note starts
 * within one block. With the black keys as shortcuts, white key k is the semitone k above C of the track's octave
 * (OCT-/OCT+): sixteen keys, sixteen semitones. With SETUP's KEYS on NOTE the 27 keys are a keyboard from F, and with
 * the arpeggiator on (arp.c) its one note plays: either way a mask of keys over key_base (bryo.h).
 *
 * A voice for a new key: a silent one, else the quietest releasing one, else the oldest. A stolen voice keeps its
 * phases and filter state and attacks from its current level, so taking it over doesn't click. Every block's
 * level is ramped from the last block's, so the envelope never steps.
 *
 * WAVE FM (2026-10-13): two operators instead, a modulator sine bending a carrier sine's phase (the DX's 2-op
 * algorithm). RATI (DTUN's place) the modulator's frequency over the carrier's, 0.5 to 14 (SYN_RATIO8); INDX (MIX's)
 * how far it bends it, to two cycles; IENV the envelope's push on INDX (bright at the strike, then mellow); FDBK
 * (NOIS's) the modulator fed into itself through the mean of its last two outputs (sine, then saw-ish, then noise).
 * The filter, drive, envelope, voices and glide as for every wave.
 *
 * WAVE PD (2026-10-13): the CZ's phase distortion. One cosine read through a bent phase (syn_pd): SHAP the shape
 * (DTUN's place), AMT how far the phase bends, 0 a sine (MIX's), AENV the envelope's push on AMT (IENV's place);
 * NOIS as it is. Five shapes bend the phase toward a full bend (a knee, a hold); the three resonant ones read the
 * cosine 1 + 15 x AMT times faster under a window (a saw's, a triangle's, a trapezoid's) that ends at 0, the CZ's
 * resonance.
 *
 * WAVE VOX (2026-10-13): a band-limited saw (and BRTH's noise) through three band-passes at a vowel's formants
 * (VOX_F8, the cutoff scale's index: no logarithm a block), each at unity gain at its peak and a narrower Q up the
 * spectrum (5, 8, 10), weighted 1, 0.7, 0.4. VOWL morphs A E I O U, VENV pushes it with the envelope, SHFT moves all
 * three by a ratio of 0.7 to 1.5. Coefficients a block, three tsvf_coef a voice.
 *
 * WAVE WT (2026-10-13): a wavetable (wt_t below): the phase reads a frame, linearly between its samples, and POS
 * (DTUN's place) + PENV x the envelope a point between two frames, so a turn morphs the cycle. Silent while its table
 * is being built or has no memory.
 *
 * Cost: per voice per sample, two oscillators, one noise step when NOIS is up, one filter; coefficients per block.
 * FM: two sine reads and their phase offsets, about what the band-limited oscillators cost.
 * Everything runs in the audio ISR; the main loop only writes the knobs (tp[t].syn) and the source (tp[t].src). */

typedef struct {
    uint32_t ph[2];              /* the oscillators' phases */
    int32_t ic1, ic2;            /* the filter's state */
    int32_t nst;                 /* the noise's state */
    int32_t env;                 /* the envelope, Q24 */
    int32_t amp;                 /* the last block's level, Q15 (the next block ramps from it) */
    int32_t p16, to16;           /* the pitch now and where it glides to, 1/16 semitone */
    int32_t fm1, fm2;            /* FM: the modulator's last two outputs (its feedback) */
    int32_t vs1[3], vs2[3];      /* VOX: the three formants' filter states */
    int32_t gi;                  /* FM: the index last block, 0..1024; PD: the bend, 0..16384 (the next block ramps
                                  * from it) */
    uint32_t age;                /* when its note started (the oldest is stolen first) */
    uint32_t tr;                 /* TRAN: the click's samples left (dsp.c click_step) */
    int32_t trng, tlp;           /* .. its noise and its low-pass */
    uint8_t key, stage;          /* the white key; 0 silent, 1 attack, 2 decay and sustain, 3 release */
    uint8_t st2;                 /* ENV 2's stage (2026-10-15: dsp.c env_step), */
    int32_t e2;                  /* .. its level, Q24 */
    uint32_t age2;               /* .. the note it started on (a new age: a new note, it starts again) */
    uint32_t phs;                /* SUB's phase */
    int32_t tgq;                 /* TUNE as the voice has it, 1/16 semitone Q14: with GLID, a modulated TUNE (a SEQ's
                                  * chord moves) glides there, an octave in GLID's time, instead of jumping
                                  * (2026-10-18) */
    uint8_t tgo;                 /* .. set since the voice started (0: take TUNE as it is) */
} syn_voice_t;

typedef struct {
    syn_voice_t v[SYN_NV];
    uint32_t keys;               /* the white keys held last block */
    uint32_t age;
    int32_t last16;              /* the last note started (a glide starts from it) */
    uint32_t keys_was;           /* CHRD: the keys held before this block's (a new key with others down: legato) */
    uint8_t croot;               /* CHRD: the chord's root key */
} syn_t;

static syn_t syn[NTRK];

/* WAVE WT's tables (2026-10-13): eight frames of 256 samples, a single cycle each, four to a chunk of the shared
 * memory (a chunk read as 1,056 samples, as RIPPLE's strings read theirs), two chunks a track while WAVE is WT and
 * the track is in use (wt_poll), given back otherwise. The main loop builds a table when TABL changes: ready drops
 * first (the ISR plays silence while it's 0), the frames are written, ready comes back */
#define WT_FR 256u                   /* a frame's samples: one cycle */
#define WT_NF 8u                     /* frames a table */
typedef struct {
    uint8_t map[2];                  /* the two chunks (the main loop's) */
    volatile uint8_t nch;
    volatile uint8_t ready;          /* the frames hold table `built` */
    int16_t built;                   /* the table built (TABL; -1: none) */
} wt_t;
static wt_t wt[NTRK];

/* frame f of track t's table */
static inline __attribute__((always_inline)) int16_t *wt_frame(uint32_t t, uint32_t f)
{
    return (int16_t *)(void *)mem_at(wt[t].map[f >> 2]) + (f & 3u) * WT_FR;
}

/* main loop: frame f of a built table from harmonic amplitudes a[1..WT_NH] (Q8), its peak at 28,000 */
#define WT_NH 24u
static void wt_additive(int16_t *d, const int32_t *a)
{
    static int32_t acc[WT_FR];
    int32_t pk = 1, i, h;
    for (i = 0; i < (int32_t)WT_FR; i++) {
        int32_t x = 0;
        for (h = 1; h <= (int32_t)WT_NH; h++)
            if (a[h])
                x += (sine_i((uint32_t)(h * i) << 24) * a[h]) >> 8;
        acc[i] = x;
        pk = x > pk ? x : -x > pk ? -x : pk;
    }
    for (i = 0; i < (int32_t)WT_FR; i++)
        d[i] = (int16_t)((acc[i] >> 4) * 28000 / ((pk >> 4) + 1));   /* (a sixteenth first: under 2^31) */
}

/* main loop: track t's table `tab` into its frames. BASC: sine, triangle, saw, square and a 25 % pulse, each shape a
 * frame with the halfway blends between (band-limited: 24 harmonics); HARM: the fundamental with one harmonic
 * swept up through the frames (2nd .. 23rd); FORM: a saw's harmonics under a resonance swept up. A reel (TAPE, a
 * factory reel, one of yours): eight windows evenly through it, each made to loop (its last 32 samples crossfaded
 * into the 32 before its start) and centred */
static void wt_build(uint32_t t, uint32_t tab)
{
    uint32_t f, i;
    int32_t a[WT_NH + 1u];
    if (tab < WT_NBUILT) {
        for (f = 0; f < WT_NF; f++) {
            uint32_t h;
            for (h = 0; h <= WT_NH; h++)
                a[h] = 0;
            for (h = 1; h <= WT_NH; h++) {
                int32_t sin1 = h == 1u ? 256 : 0, tri = h & 1u ? ((h & 2u) ? -256 : 256) / (int32_t)(h * h) : 0;
                int32_t saw = 256 / (int32_t)h, sq = h & 1u ? 256 / (int32_t)h : 0;
                int32_t pls = (sine_i(h * 0x20000000u) >> 7) / (int32_t)h;   /* (sin(pi h / 4) / h: 25 %) */
                if (tab == 0u) {
                    static const uint8_t SH[8][2] = {{0, 0}, {0, 1}, {1, 1}, {1, 2}, {2, 2}, {2, 3}, {3, 3}, {4, 4}};
                    int32_t v[5] = {sin1, tri, saw, sq, pls};
                    a[h] = (v[SH[f][0]] + v[SH[f][1]]) / 2;
                } else if (tab == 1u) {
                    a[h] = h == 1u ? 256 : h == 2u + f * 3u ? 220 : h == 3u + f * 3u ? 110 : 0;
                } else {
                    int32_t c = 2 + (int32_t)f * 2, d = (int32_t)h - c;
                    a[h] = saw * 4 / (4 + d * d);
                }
            }
            wt_additive(wt_frame(t, f), a);
        }
        return;
    }
    {
        tape_view_t vw;
        static tape_rd_t rd;
        tape_view_of(t, tab - WT_NBUILT, &vw);
        rd.ok = 0;
        for (f = 0; f < WT_NF; f++) {
            int16_t *d = wt_frame(t, f);
            int32_t st = vw.len > WT_FR + 64u ? (int32_t)(32u + (vw.len - WT_FR - 64u) / 7u * f) : 32, sum = 0, pk = 1, x;
            for (i = 0; i < WT_FR; i++) {
                x = vw.len ? tape_read(&vw, &rd, (st + (int32_t)i) << 12) : 0;
                if (i >= WT_FR - 32u) {                  /* (the loop: the end becomes what leads into the start) */
                    int32_t pre = vw.len ? tape_read(&vw, &rd, (st - 32 + (int32_t)(i - (WT_FR - 32u))) << 12) : 0;
                    int32_t w = (int32_t)(i - (WT_FR - 32u));
                    x = (x * (32 - w) + pre * w) / 32;
                }
                d[i] = (int16_t)clamp(x, -32767, 32767);
                sum += d[i];
            }
            sum /= (int32_t)WT_FR;
            for (i = 0; i < WT_FR; i++) {
                x = d[i] - sum;
                pk = x > pk ? x : -x > pk ? -x : pk;
            }
            for (i = 0; i < WT_FR; i++)
                d[i] = (int16_t)((d[i] - sum) * 28000 / pk);
        }
    }
}

static int trk_live(uint32_t t);
/* main loop, every pass: each track's wavetable memory (two chunks while its SYNTH plays WT and it's in use, none
 * otherwise), and its table built when TABL moves */
static void wt_poll(void)
{
    uint32_t t;
    for (t = 0; t < NTRK; t++) {
        wt_t *W = &wt[t];
        int on = trk_live(t) && tp[t].src == SRC_SYNTH && tp[t].syn[SY_WAVE] == SY_WT;
        int16_t tab = (int16_t)clamp(tp[t].syn[SY_TABL], 0, (int32_t)(WT_NBUILT + NREEL + USLOT_N));
        while (on && W->nch < 2u) {
            int32_t c = tape_alloc(MEM_WT + t, NTRK, 1);
            if (c < 0)
                break;
            W->map[W->nch] = (uint8_t)c;
            RING_PUBLISH();
            W->nch++;
            W->built = -1;
        }
        if (on && W->nch == 2u && W->built != tab) {
            W->ready = 0;
            RING_PUBLISH();
            wt_build(t, (uint32_t)tab);
            W->built = tab;
            RING_PUBLISH();
            W->ready = 1;
        }
        if (!on && W->nch) {
            W->ready = 0;
            RING_PUBLISH();
            while (W->nch) {
                W->nch--;
                mem_free(W->map[W->nch]);
            }
            W->built = -1;
        }
    }
}

/* key bit k of track t as a pitch, 1/16 semitone, before TUNE (added as the voice plays, so a modulated TUNE
 * moves a held note too: mod.c). The bit's note is key_base's (bryo.h), held to the project's scale (key_snap) */
static int32_t syn_note16(uint32_t t, uint32_t k)
{
    return key_snap(clamp((int32_t)key_base[t] + (int32_t)k, 0, 127)) * 16;
}

static void syn_start(uint32_t t, uint32_t k, uint32_t nv)
{
    syn_t *s = &syn[t];
    syn_voice_t *v = &s->v[0];
    int32_t note = syn_note16(t, k), glide = TPD(t, MA_SYN)[SY_GLID] > 0;
    uint32_t j;
    if (nv == 1u && (v->stage == 1u || v->stage == 2u)) {   /* one voice, a key still down: legato */
        v->key = (uint8_t)k;
        v->to16 = note;
        if (!glide)
            v->p16 = note;
        s->last16 = note;
        return;
    }
    for (j = 0; j < nv && s->v[j].stage; j++)              /* a silent voice */
        ;
    if (j == nv) {
        uint32_t best = 0xFFFFFFFFu;
        for (j = 0; j < nv; j++)                            /* the quietest releasing one */
            if (s->v[j].stage == 3u && (uint32_t)s->v[j].env < best) {
                best = (uint32_t)s->v[j].env;
                v = &s->v[j];
            }
        if (best == 0xFFFFFFFFu)                            /* the oldest */
            for (j = 0, v = &s->v[0]; j < nv; j++)
                if (s->v[j].age - v->age > 0x80000000u)
                    v = &s->v[j];
    } else {
        v = &s->v[j];
    }
    if (!v->stage && !v->amp) {                             /* from silence: a clean start */
        v->ph[0] = 0;
        v->ph[1] = 0x40000000u;
        v->ic1 = v->ic2 = 0;
        v->env = 0;
        v->fm1 = v->fm2 = 0;
        v->vs1[0] = v->vs1[1] = v->vs1[2] = v->vs2[0] = v->vs2[1] = v->vs2[2] = 0;
        if (TPD(t, MA_SYN)[SY_WAVE] == SY_FM)
            v->ph[1] = 0;                               /* (FM: the modulator from 0 with the carrier) */
        if (!v->nst)
            v->nst = 0x1234567 + (int32_t)(t * 7919u + k);
    }
    v->key = (uint8_t)k;
    v->stage = 1;
    v->age = ++s->age;
    v->to16 = note;
    v->tgo = 0;
    v->p16 = glide && s->last16 ? s->last16 : note;
    s->last16 = note;
    v->tr = TPD(t, MA_SYN)[SY_TRAN] > 0 ? CLICK_N : 0u;   /* (TRAN: a click at every start, legato aside) */
    v->tlp = 0;
    if (!v->trng)
        v->trng = 0x3C6EF372 + (int32_t)(t * 104729u + k);
}

/* PD: the voice's sample at phase ph, the bend d (0..16384: 0 a sine, 16384 the whole shape), shape sh (N_PDSH);
 * Q15. The full bend of each phase shape is a map T of the phase (Q16; its constants the divides done ahead), and the
 * phase read is p + d x (T(p) - p) */
static inline __attribute__((always_inline)) int32_t syn_pd(uint32_t ph, int32_t d, uint32_t sh)
{
    int32_t p = (int32_t)(ph >> 16), q;
    if (sh >= 5u) {                                     /* resonant: the cosine 1..16 times faster under a window */
        uint32_t r = 65536u + (uint32_t)d * 60u, w;      /* (Q16: d 16384 x 60 is 15 x 65536) */
        int32_t c = sine_i((uint32_t)p * r + 0xC0000000u);
        w = sh == 5u ? 65535u - (uint32_t)p : sh == 6u ? (p < 32768 ? (uint32_t)p * 2u : (65535u - (uint32_t)p) * 2u)
          : (p < 32768 ? 65535u : (65535u - (uint32_t)p) * 2u);
        return (c * (int32_t)(w >> 1)) >> 15;
    }
    switch (sh) {
    case 0: q = p < 655 ? p * 50 : 32768 + (((p - 655) * 33099) >> 16); break;   /* SAW: a knee at 1 % */
    case 1: q = (p & 32768) + ((p & 32767) < 655 ? (p & 32767) * 50 : 32768); break;   /* SQR: each half a sweep
                                                                                     * then a hold */
    case 2: q = p < 3277 ? p * 20 : 65535; break;        /* PLS: one sweep in 5 %, then the hold */
    case 3: q = p * 2; break;                            /* DSIN: two cycles */
    default: q = p < 32768 ? (p < 655 ? p * 50 : 32768) : p; break;   /* SPLS: a sweep and a hold, then as it was */
    }
    q = p + (((q - p) * d) >> 14);
    return sine_i(((uint32_t)q << 16) + 0xC0000000u);  /* (-cos: a sine from its trough, so 0 is a sine) */
}

/* CHRD: voice j onto note16 (a chord's tone): from silence a clean start, sounding a retune (legato, gliding by GLID) */
static void syn_chord_voice(uint32_t t, uint32_t j, uint32_t k, int32_t note16, int legato)
{
    syn_t *s = &syn[t];
    syn_voice_t *v = &s->v[j];
    int glide = TPD(t, MA_SYN)[SY_GLID] > 0;
    if (legato && (v->stage == 1u || v->stage == 2u)) {
        v->to16 = note16;
        if (!glide)
            v->p16 = note16;
        v->key = (uint8_t)k;
        return;
    }
    if (!v->stage && !v->amp) {
        v->ph[0] = 0;
        v->ph[1] = TPD(t, MA_SYN)[SY_WAVE] == SY_FM ? 0 : 0x40000000u;
        v->ic1 = v->ic2 = 0;
        v->env = 0;
        v->fm1 = v->fm2 = 0;
        v->vs1[0] = v->vs1[1] = v->vs1[2] = v->vs2[0] = v->vs2[1] = v->vs2[2] = 0;
        if (!v->nst)
            v->nst = 0x1234567 + (int32_t)(t * 7919u + k + j);
    }
    v->key = (uint8_t)k;
    v->stage = 1;
    v->age = ++s->age;
    v->p16 = glide && v->p16 ? v->p16 : note16;
    v->to16 = note16;
    if (!v->stage || !glide)
        v->tgo = 0;
}

/* the voice's filter on x: TYPE's output (LP the plain one), through tsvf_sat when RES is high (fs) */
#define SY_FLT(x) (fs ? tsvf_sat(&flt, (x), &ic1, &ic2, ft) : ft ? tsvf_mode(&flt, (x), &ic1, &ic2, ft) \
                      : tsvf_lp(&flt, (x), &ic1, &ic2))
/* one block of one voice, added into out */
static void syn_voice(uint32_t t, syn_voice_t *v, int32_t *out, uint32_t n)
{
    const int16_t *p = TPD(t, MA_SYN);
    tsvf_t flt;
    uint32_t wave = (uint32_t)p[SY_WAVE], i, inc1, inc2, ph0 = v->ph[0], ph1 = v->ph[1];
    int32_t e15, a0 = v->amp, a1, cut, m2 = p[SY_MIX] * 327, m1 = 32767 - m2, nz = p[SY_NOIS] * 254;
    int32_t drv = p[SY_DRV], dg, ic1 = v->ic1, ic2 = v->ic2, nst = v->nst, pp, e2, ed = p[SY_ENV];
    uint32_t ety = (uint32_t)clamp(p[SY_ETYP], 0, 2), dst = (uint32_t)clamp(p[SY_EDST], 0, 2), phs = v->phs;
    int32_t sg = clamp(p[SY_SUB], 0, 100) * 229;            /* (SUB: the square at up to 0.7 of the oscillators') */
    uint32_t tr = v->tr;                                    /* (TRAN's click: into s before the filter) */
    int32_t trng = v->trng, tlp = v->tlp, tg = clamp(p[SY_TRAN], 0, 100) * 327;
    uint32_t ft, pw;
    int fs;                                             /* (RES high: the filter's soft limit, tsvf_sat) */
    env_step(&v->env, &v->stage, p[SY_ATK], p[SY_DEC], p[SY_SUS], p[SY_REL], ety);   /* the envelope: a step a
                                                                                        * block (as Felucca's) */
    if (!v->stage) {                                        /* ENV 2: with the note; done with the voice */
        v->st2 = 0;
        v->e2 = 0;
    } else if (v->age != v->age2) {
        v->age2 = v->age;
        v->st2 = 1;
    } else if (v->stage == 3u && v->st2 && v->st2 != 3u) {
        v->st2 = 3;
    }
    env_step(&v->e2, &v->st2, p[SY_ATK2], p[SY_DEC2], p[SY_SUS2], p[SY_REL2], ety);
    if (v->p16 != v->to16) {                                /* the glide: GLID's time for an octave and a half */
        int32_t st = p[SY_GLID] ? (int32_t)(ENV_LIN[p[SY_GLID] & 127] >> 14) : 1 << 16, d = v->to16 - v->p16;
        if (st < 1)
            st = 1;
        v->p16 += d > st ? st : d < -st ? -st : d;
    }
    {                                                       /* TUNE: as it is, or with GLID gliding there */
        int32_t tu = mod_pitch16(t, FN_SYN, p[SY_TUNE]);
        if (!p[SY_GLID] || !v->tgo) {
            v->tgq = tu * 16384;
            v->tgo = 1;
        } else if (v->tgq != tu * 16384) {               /* (ENV_LIN's full scale, 1 << 24, is 192 sixteenths here) */
            int32_t st = (int32_t)(ENV_LIN[p[SY_GLID] & 127] * 3u >> 4), d = tu * 16384 - v->tgq;
            v->tgq += d > st ? st : d < -st ? -st : d;
        }
    }
    e15 = v->env >> 9;
    e2 = v->e2 >> 9;                                        /* (ENV 2, Q15: IENV's, and ENV's at DEST) */
    a1 = e15;
    if (dst == 1u)                                          /* DEST DRV: the drive's envelope */
        drv = clamp(drv + ed * (e2 >> 7) / 256, 0, 100);
    dg = (100 + 2 * drv) * 4096 / 100;
    pp = clamp(v->p16 + (v->tgq >> 14) + (dst == 2u ? ed * 384 * (e2 >> 7) / 25600 : 0), 0, 2047);
                                                            /* (DEST PTCH: ENV 100 at its top, +24 semitones) */   /* (TUNE as it is now, to 1/16 semitone
                                                                           * when it's modulated: mod.c) */
    inc1 = p[SY_FINE] ? cents_inc(pp, p[SY_FINE], 0) : pitch_inc((uint32_t)pp);   /* (FINE: cents, exactly) */
    if (p[SY_INTV] && wave < SY_FM)                     /* INTV: the second oscillator semitones off, DTUN on that */
        inc2 = cents_inc(clamp(pp + p[SY_INTV] * 16, 0, 2047), p[SY_DTUN] + p[SY_FINE], 0);
    else
        inc2 = p[SY_DTUN] ? cents_inc(pp, p[SY_DTUN] + p[SY_FINE], 0) : inc1;
    cut = mod_fine8(t, FN_SCUT, p[SY_CUT]) + (dst ? 0 : ed * 96 * (e2 >> 7) / 100) + p[SY_KTRK] * (pp - 60 * 16) * 16 / 100;
                                                            /* (CUT finer than its semitones when modulated: mod.c) */
    tsvf_coef_k(&flt, cut, tsvf_kres(p[SY_RES]));       /* (RES past 92: it sings on its own, dsp.c) */
    fs = p[SY_RES] > TSVF_SAT_RES;
    ft = (uint32_t)clamp(p[SY_FTYP], 0, 2);             /* (TYPE: the filter's output; LP the loop as it was) */
    pw = p[SY_PW] == 50 ? 0x80000000u : (uint32_t)clamp(p[SY_PW], 5, 95) * 42949673u;   /* (PW: SQR's width) */
    if (wave == SY_WT) {                                /* WT: the two frames POS falls between, this block */
        int32_t fp = clamp(p[SY_DTUN] + p[SY_IENV] * (e2 >> 7) / 255, 0, 100) * (int32_t)((WT_NF - 1u) * 256u) / 100;
        uint32_t fi = (uint32_t)fp >> 8, ff = (uint32_t)fp & 255u;
        const int16_t *fa, *fb;
        if (!wt[t].ready || wt[t].nch < 2u) {           /* (no table: silence, the envelope still runs) */
            n = 0;
        } else {
            fa = wt_frame(t, fi);
            fb = wt_frame(t, fi < WT_NF - 1u ? fi + 1u : fi);
            for (i = 0; i < n; i++) {
                uint32_t j = ph0 >> 24, j1 = (j + 1u) & 255u, fr = (ph0 >> 16) & 255u;
                int32_t xa = fa[j] + (((fa[j1] - fa[j]) * (int32_t)fr) >> 8), xb = fb[j] + (((fb[j1] - fb[j]) * (int32_t)fr) >> 8);
                int32_t s = xa + (((xb - xa) * (int32_t)ff) >> 8), y, k;
                ph0 += inc1;
                if (nz)
                    s += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
                if (drv)
                    s = softclip((s * dg) >> 12);
                if (tr)
                    s += click_step(&tr, &trng, &tlp, tg);
                y = SY_FLT(s >> 1);
                k = y < 0 ? -y : y;
                if (k > 16000) {
                    k = 16000 + (softclip((k - 16000) * 2) >> 1);
                    y = y < 0 ? -k : k;
                }
                out[i] += mulq15(mulq15(y << 1, a0 + (((a1 - a0) * (int32_t)i) >> CTL_LOG2)), VOICE_FS);
            }
            n = 0;
        }
    }
    if (wave == SY_VOX) {                               /* VOX: the vowel now, its three bands' coefficients */
        tsvf_t fb[3];
        static const int16_t QK[3] = {820, 512, 410};   /* (damping Q12: Q 5, 8, 10) */
        static const int16_t GW[3] = {32767, 22938, 13107};   /* (1, 0.7, 0.4) */
        int32_t vw = clamp(p[SY_DTUN] + p[SY_IENV] * (e2 >> 7) / 255, 0, 100) * 1024 / 100, vi = vw >> 8, vf = vw & 255;
        int32_t sh = (p[SY_MIX] - 50) * 40, f, s1[3], s2[3];   /* (SHFT: +-2,000 of the scale, a ratio 0.7..1.5) */
        for (f = 0; f < 3; f++) {
            int32_t c = VOX_F8[vi > 4 ? 4 : vi][f];
            if (vi < 4)
                c += ((VOX_F8[vi + 1][f] - c) * vf) >> 8;
            tsvf_coef_k(&fb[f], c + sh, QK[f]);
            s1[f] = v->vs1[f];
            s2[f] = v->vs2[f];
        }
        for (i = 0; i < n; i++) {
            int32_t x = osc_saw(ph0, inc1) >> 1, s = 0, y, k;
            ph0 += inc1;
            if (nz)
                x += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
            for (f = 0; f < 3; f++)                     /* (k x the band: unity at the peak) */
                s += mulq15((tsvf_mode(&fb[f], x, &s1[f], &s2[f], 1u) * fb[f].k) >> 12, GW[f]);
            s <<= 1;
            if (drv)
                s = softclip((s * dg) >> 12);
            if (tr)
                    s += click_step(&tr, &trng, &tlp, tg);
                y = SY_FLT(s >> 1);
            k = y < 0 ? -y : y;
            if (k > 16000) {
                k = 16000 + (softclip((k - 16000) * 2) >> 1);
                y = y < 0 ? -k : k;
            }
            out[i] += mulq15(mulq15(y << 1, a0 + (((a1 - a0) * (int32_t)i) >> CTL_LOG2)), VOICE_FS);
        }
        for (f = 0; f < 3; f++) {
            v->vs1[f] = s1[f];
            v->vs2[f] = s2[f];
        }
        n = 0;
    }
    if (wave == SY_PD) {                                /* PD: the bend ramped from last block's, the shape */
        int32_t d1 = clamp(p[SY_MIX] + p[SY_IENV] * (e2 >> 7) / 255, 0, 100) * 16384 / 100, d0 = v->gi;
        int32_t dd = (d1 - d0) >> CTL_LOG2;
        uint32_t sh = syn_pdsh(p[SY_DTUN]);
        v->gi = d1;
        for (i = 0; i < n; i++, d0 += dd) {
            int32_t s, y, k;
            s = syn_pd(ph0, d0, sh);
            ph0 += inc1;
            if (nz)
                s += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
            if (drv)
                s = softclip((s * dg) >> 12);
            if (tr)
                    s += click_step(&tr, &trng, &tlp, tg);
                y = SY_FLT(s >> 1);
            k = y < 0 ? -y : y;
            if (k > 16000) {
                k = 16000 + (softclip((k - 16000) * 2) >> 1);
                y = y < 0 ? -k : k;
            }
            out[i] += mulq15(mulq15(y << 1, a0 + (((a1 - a0) * (int32_t)i) >> CTL_LOG2)), VOICE_FS);
        }
        n = 0;
    }
    if (wave == SY_FM) {                                /* FM: the modulator's increment, the index ramped from last
                                                         * block's, the feedback's gain */
        uint32_t r8 = SYN_RATIO8[syn_ratio(p[SY_DTUN])], ib = inc1 >> 8;
        int32_t g1 = clamp(p[SY_MIX] + p[SY_IENV] * (e2 >> 7) / 255, 0, 100) * 1024 / 100, g0 = v->gi;
        int32_t fb8 = p[SY_NOIS] * 256 / 100, m1 = v->fm1, mm2 = v->fm2, dgi = ((g1 - g0) * 2) >> CTL_LOG2;
        uint32_t incm = ib > 0x7FFFFFFFu / r8 ? 0x7FFFFFFFu : ib * r8;   /* (past half the rate: held there) */
        g0 <<= 1;                                       /* (Q1: the ramp's step keeps its half) */
        v->gi = g1;
        inc2 = incm;
        for (i = 0; i < n; i++, g0 += dgi) {
            int32_t m, a, s, y, k;
            m = sine_i(ph1 + ((uint32_t)(((m1 + mm2) >> 1) * fb8) << 8));   /* (the feedback: up to half a cycle) */
            mm2 = m1;
            m1 = m;
            a = sine_i(ph0 + ((uint32_t)(m * (g0 >> 1)) << 8));   /* (the index: up to two cycles; the phases wrap) */
            ph0 += inc1;
            ph1 += incm;
            s = a;
            if (drv)
                s = softclip((s * dg) >> 12);
            if (tr)
                    s += click_step(&tr, &trng, &tlp, tg);
                y = SY_FLT(s >> 1);
            k = y < 0 ? -y : y;
            if (k > 16000) {
                k = 16000 + (softclip((k - 16000) * 2) >> 1);
                y = y < 0 ? -k : k;
            }
            out[i] += mulq15(mulq15(y << 1, a0 + (((a1 - a0) * (int32_t)i) >> CTL_LOG2)), VOICE_FS);
        }
        v->fm1 = m1;
        v->fm2 = mm2;
        n = 0;                                          /* (done: the loop below runs no samples) */
    }
    for (i = 0; i < n; i++) {
        int32_t a, b, s, y, k;
        switch (wave) {
        case 0:
            a = osc_sine(ph0);
            b = osc_sine(ph1);
            break;
        case 1:
            a = osc_tri(ph0);
            b = osc_tri(ph1);
            break;
        case 2:
            a = osc_pulse(ph0, inc1, pw);
            b = osc_pulse(ph1, inc2, pw);
            break;
        case 4:                                             /* a 25 % pulse: thinner, nasal */
            a = osc_pulse(ph0, inc1, pw >> 1);
            b = osc_pulse(ph1, inc2, pw >> 1);
            break;
        default:
            a = osc_saw(ph0, inc1);
            b = osc_saw(ph1, inc2);
            break;
        }
        ph0 += inc1;
        ph1 += inc2;
        s = mulq15(a, m1) + mulq15(b, m2);
        if (sg) {                                           /* SUB: a square an octave under oscillator 1 */
            s += mulq15(osc_pulse(phs, inc1 >> 1, 0x80000000u), sg);
            phs += inc1 >> 1;
        }
        if (nz)
            s += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
        if (drv)                                            /* drive: up to 3x into the soft clip */
            s = softclip((s * dg) >> 12);
        if (tr)
                    s += click_step(&tr, &trng, &tlp, tg);
                y = SY_FLT(s >> 1);             /* linear to half scale, then a soft knee */
        k = y < 0 ? -y : y;
        if (k > 16000) {
            k = 16000 + (softclip((k - 16000) * 2) >> 1);
            y = y < 0 ? -k : k;
        }
        out[i] += mulq15(mulq15(y << 1, a0 + (((a1 - a0) * (int32_t)i) >> CTL_LOG2)), VOICE_FS);
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->phs = phs;
    v->ic1 = ic1;
    v->tr = tr;
    v->trng = trng;
    v->tlp = tlp;
    v->ic2 = ic2;
    v->nst = nst;
    v->amp = a1;
}

/* audio ISR: one block of track t's synth into out; keys: the keys held (bit k: key_base + k); rec: unused (the
 * synth records nothing; REC prints it onto the tape, chain.c) */
static void syn_block(uint32_t t, uint32_t keys, const int32_t *rec, int32_t *out, uint32_t n)
{
    syn_t *s = &syn[t];
    uint32_t on = keys & ~s->keys, off = s->keys & ~keys, nv = (uint32_t)clamp(TPD(t, MA_SYN)[SY_VOIC], 1, SYN_NV), j, k;
    (void)rec;
    for (j = 0; j < n; j++)
        out[j] = 0;
    s->keys = keys;
    {
        uint32_t ch = (uint32_t)clamp(TPD(t, MA_SYN)[SY_CHRD], 0, 11);
        if (ch && !src_sleep) {                         /* CHRD: the lowest key held is the chord's root, its three
                                                         * tones on the three voices (VOIC aside); a key down from
                                                         * nothing strikes it, a new lowest key with others held
                                                         * moves it there legato; none held: every voice lets go */
            if (!keys) {
                for (j = 0; j < SYN_NV; j++)
                    if (s->v[j].stage && s->v[j].stage != 3u)
                        s->v[j].stage = 3;
            } else if (on || off) {
                uint32_t r = 0;
                int legato = (s->keys_was & keys) != 0u;
                while (!((keys >> r) & 1u))
                    r++;
                if (r != s->croot || !legato)
                    for (j = 0; j < SYN_NV; j++)
                        syn_chord_voice(t, j, r, syn_note16(t, r) + SYN_CHORD[ch][j] * 16, legato);
                s->croot = (uint8_t)r;
            }
            s->keys_was = keys;
            for (j = 0; j < SYN_NV; j++)
                if (s->v[j].stage || s->v[j].amp)
                    syn_voice(t, &s->v[j], out, n);
            return;
        }
        s->keys_was = keys;
    }
    for (k = 0; off && k < NKEYBIT; k++)                     /* keys let go: their voices release */
        if ((off >> k) & 1u)
            for (j = 0; j < SYN_NV; j++) {
                syn_voice_t *v = &s->v[j];
                if (v->key != k || !v->stage || v->stage == 3u)
                    continue;
                if (nv == 1u && keys) {                     /* legato: back to the highest key still down */
                    uint32_t h = NKEYBIT - 1u;
                    while (!((keys >> h) & 1u))
                        h--;
                    v->key = (uint8_t)h;
                    v->to16 = syn_note16(t, h);
                    if (!TPD(t, MA_SYN)[SY_GLID])
                        v->p16 = v->to16;
                } else {
                    v->stage = 3;
                }
            }
    for (j = nv; j < SYN_NV; j++)                           /* VOIC turned down: the voices above it let go */
        if (s->v[j].stage && s->v[j].stage != 3u)
            s->v[j].stage = 3;
    if (src_sleep) {                                    /* asleep (muted): no voice sounds; at waking the next keys
                                                         * start from silence */
        for (j = 0; j < SYN_NV; j++)
            s->v[j].stage = 0, s->v[j].amp = 0, s->v[j].env = 0;
        s->keys = s->keys_was = 0;                      /* (2026-10-18: and the keys forgotten, so keys still held at
                                                         * waking strike again, a CHRD chord too) */
        return;
    }
    for (k = 0; on && k < NKEYBIT; k++)                      /* new keys, lowest first */
        if ((on >> k) & 1u)
            syn_start(t, k, nv);
    for (j = 0; j < SYN_NV; j++)
        if (s->v[j].stage || s->v[j].amp)
            syn_voice(t, &s->v[j], out, n);
}
