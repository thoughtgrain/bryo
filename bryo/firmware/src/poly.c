/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: POLY, a source engine (docs/bryo-architecture.md, "Source engines"): a sound played across the keys, up to
 * POL_NV notes at once. The sound is any reel choice (REEL): a factory reel or one of yours in flash, read through
 * the XIP window as TAPE reads them, or the track's own RAM tape, so you can record a phrase and play it as an
 * instrument. Nothing is copied, so POLY costs no tape RAM: each voice has a block reader (tape.c's) and a little
 * state.
 *
 * A voice: its head starts at STRT and runs at the key's pitch against the sound's own (the key ROOT names, A3 at
 * first: white key 1 at OCT 3, plus TUNE and FINE, is the sound as it was recorded: 22.05 kHz read at 0.5 a sample,
 * like TAPE at 100 %). With the project's SCAL on, a key outside it plays the nearest note in it. Its ADSR shapes the
 * level and, by ENV, the cutoff of its state-variable filter (TYPE: low-, band- or high-pass). At END (or the
 * sound's end) it fades over six blocks and frees itself. Voices are taken as SYNTH's are: a silent one, the quietest
 * releasing one, else the oldest (which keeps its filter state and ramps from its level, so a steal doesn't click).
 *
 * Audio ISR only, like synth.c: the main loop writes tp[t].pol and the octave. */

typedef struct {
    int32_t pos, inc;            /* the head and its step, Q12 tape samples */
    int32_t ic1, ic2;            /* the filter's state */
    int32_t env;                 /* Q24 */
    int32_t amp;                 /* the last block's level, Q15 */
    uint32_t age;
    uint8_t key, stage;          /* 0 silent, 1 attack, 2 decay and sustain, 3 release */
    uint8_t note;                /* the key's note at its octave when it went down (TUNE is added as it plays) */
    uint8_t endf;                /* END passed: blocks of its fade left (POL_ENDF..1; 0: not ending) */
    int32_t tn16, fcut;          /* what inc and flt were last worked out from (a divide each, a block, a voice:
                                  * only when they move; tok, fok: worked out at all) */
    int16_t tfine, fres;
    uint8_t tok, fok;
    tsvf_t flt;                  /* the filter's coefficients for fcut and fres */
} pol_voice_t;
#define POL_ENDF 6u              /* END's fade, blocks (4.4 ms: one block, 0.7 ms, stepped when END was modulated) */

typedef struct {
    pol_voice_t v[POL_NV];
    uint32_t keys, age;
} pol_t;

static pol_t pol[NTRK];
static tape_rd_t pol_rd[NTRK][POL_NV] __attribute__((section(".lowpool")));   /* (RAMTEXT: app.ld) */   /* each voice's block (8 KB in all) */

/* the head's step for voice v's note on track t, Q12 tape samples per output sample: 0.5 at the sound's own pitch
 * (its ROOT's key, FINE cents off). TUNE is added here, every block, so a modulated TUNE moves a held note too, to
 * 1/16 semitone (mod.c); FINE to the cent (cents_inc). The divide runs only when the pitch or FINE moved */
static void pol_tune(uint32_t t, pol_voice_t *v)
{
    const int16_t *p = TPD(t, MA_POL);
    int32_t n16 = clamp(((int32_t)v->note + 48 - p[PL_ROOT]) * 16 + mod_pitch16(t, FN_POL, p[PL_TUNE]), 0, 127 * 16);
    if (v->tok && n16 == v->tn16 && p[PL_FINE] == v->tfine)
        return;
    {
        uint32_t inc = p[PL_FINE] ? cents_inc(n16, p[PL_FINE], 0) : pitch_inc((uint32_t)n16);   /* (cents_inc's
                                                                                   * divides only when FINE is off 0) */
        v->inc = (int32_t)(inc / (pitch_inc(48u * 16u) >> 11));   /* (C3 here: the sound's own) */
    }
    v->tn16 = n16;
    v->tfine = p[PL_FINE];
    v->tok = 1;
}

/* the voice's filter for a cutoff and RES: tsvf_coef's divides only when they moved */
static void pol_filter(uint32_t t, pol_voice_t *v, int32_t cut)
{
    int16_t res = TPD(t, MA_POL)[PL_RES];
    if (v->fok && cut == v->fcut && res == v->fres)
        return;
    tsvf_coef(&v->flt, cut, PCT_127[clamp(res, 0, 100)]);
    v->fcut = cut;
    v->fres = res;
    v->fok = 1;
}

/* where a note stops in a sound len samples long: END, never past the sound. Audio ISR only (the screen calls
 * pct100_of): each voice asks every block, so the answer is kept per track (pct100_of's three divides are hardware
 * divides on the target: only when len or END moved) */
static struct {
    uint32_t len, end;
    int16_t knob;
} pol_endc[NTRK];

static uint32_t pol_end(uint32_t t, uint32_t len)
{
    int16_t k = TPD(t, MA_POL)[PL_END];
    t %= NTRK;
    if (pol_endc[t].len != len || pol_endc[t].knob != k || !len) {
        pol_endc[t].len = len;
        pol_endc[t].knob = k;
        pol_endc[t].end = (uint32_t)pct100_of(len, k);
    }
    return pol_endc[t].end;
}

static void pol_start(uint32_t t, uint32_t k, uint32_t nv, uint32_t len)
{
    pol_t *s = &pol[t];
    pol_voice_t *v = &s->v[0];
    uint32_t j, best = 0xFFFFFFFFu;
    for (j = 0; j < nv && s->v[j].stage; j++)              /* a silent voice */
        ;
    if (j < nv) {
        v = &s->v[j];
    } else {
        for (j = 0; j < nv; j++)                            /* the quietest releasing one */
            if (s->v[j].stage == 3u && (uint32_t)s->v[j].env < best) {
                best = (uint32_t)s->v[j].env;
                v = &s->v[j];
            }
        if (best == 0xFFFFFFFFu)                            /* the oldest */
            for (j = 0, v = &s->v[0]; j < nv; j++)
                if (s->v[j].age - v->age > 0x80000000u)
                    v = &s->v[j];
    }
    if (!v->stage && !v->amp) {                             /* from silence: a clean start */
        v->ic1 = v->ic2 = 0;
        v->env = 0;
    }
    v->pos = pct100_of(len, TPD(t, MA_POL)[PL_STRT]) << 12;
    v->note = (uint8_t)key_snap(clamp((int32_t)key_base[t] + (int32_t)k, 0, 127));   /* (the project's scale) */
    v->tok = 0;
    pol_tune(t, v);
    v->key = (uint8_t)k;
    v->stage = 1;
    v->endf = 0;
    v->age = ++s->age;
    pol_rd[t][v - s->v].ok = 0;
}

static void pol_voice(uint32_t t, pol_voice_t *v, tape_rd_t *rd, const tape_view_t *vw, int32_t *out, uint32_t n)
{
    const int16_t *p = TPD(t, MA_POL);
    uint32_t i, mode = (uint32_t)clamp(p[PL_TYPE], 0, 2);
    int32_t a0 = v->amp, a1, e15, pos = v->pos, ic1 = v->ic1, ic2 = v->ic2, c8;
    switch (v->stage) {                                     /* the envelope, a step per block (as SYNTH's) */
    case 1:
        v->env += (int32_t)ENV_LIN[p[PL_ATK] & 127];
        if (v->env >= 1 << 24) {
            v->env = 1 << 24;
            v->stage = 2;
        }
        break;
    case 2:
        v->env += mulq16(p[PL_SUS] * ((1 << 24) / 100) - v->env, ENV_EXP[p[PL_DEC] & 127]);
        break;
    case 3:
        v->env -= mulq16(v->env, ENV_EXP[p[PL_REL] & 127]);
        if (v->env < 1 << 12) {
            v->env = 0;
            v->stage = 0;
        }
        break;
    default:
        v->env = 0;
        break;
    }
    e15 = v->env >> 9;
    a1 = e15;
    pol_tune(t, v);                                         /* (TUNE as it is now) */
    if (!v->endf && (uint32_t)((pos + v->inc * (int32_t)n) >> 12) >= pol_end(t, vw->len))
        v->endf = POL_ENDF;                                 /* END passed: a short fade from here, then done */
    if (v->endf) {                                          /* (reading on a little past END: the sound's end itself
                                                             * reads as silence) */
        a1 = e15 * (int32_t)(v->endf - 1u) / (int32_t)POL_ENDF;
        if (--v->endf == 0u) {
            a1 = 0;
            v->stage = 0;
            v->env = 0;
        }
    }
    c8 = mod_fine8(t, FN_PCUT, p[PL_CUT]);                  /* (CUT finer than its semitones when modulated: mod.c) */
    pol_filter(t, v, c8 + (p[PL_ENV] ? p[PL_ENV] * 96 * (e15 >> 7) / 100 : 0));   /* (ENV 0: no divide) */
    {
        int filt = mode || c8 < 127 << 8 || p[PL_RES] || p[PL_ENV];   /* (decided once a block, not a sample) */
        int32_t base = tape_run(vw, rd, pos, v->inc, n), da = a1 - a0, y;
        if (base >= 0) {                                    /* the block's run inside one decoded block (tape.c): the
                                                             * samples read straight from it, as tape_read reads them */
            for (i = 0; i < n; i++) {
                int32_t o = (pos >> 12) - base, a = rd->s[o], b = rd->s[o + 1];
                int32_t x = a + (((b - a) * (pos & 4095)) >> 12);
                pos += v->inc;
                y = filt ? tsvf_mode(&v->flt, x >> 1, &ic1, &ic2, mode) << 1 : x;
                out[i] += mulq15(y, a0 + ((da * (int32_t)i) >> CTL_LOG2));
            }
        } else {
            for (i = 0; i < n; i++) {
                int32_t x = tape_read(vw, rd, pos);
                pos += v->inc;
                y = filt ? tsvf_mode(&v->flt, x >> 1, &ic1, &ic2, mode) << 1 : x;
                out[i] += mulq15(y, a0 + ((da * (int32_t)i) >> CTL_LOG2));
            }
        }
    }
    v->pos = pos;
    v->ic1 = ic1;
    v->ic2 = ic2;
    v->amp = a1;
}

/* audio ISR: one block of track t's POLY into out; keys: the keys held (bit k: key_base + k); rec: unused (REC prints POLY onto the
 * tape, chain.c) */
static void pol_block(uint32_t t, uint32_t keys, const int32_t *rec, int32_t *out, uint32_t n)
{
    pol_t *s = &pol[t];
    tape_view_t vw;
    uint32_t on = keys & ~s->keys, off = s->keys & ~keys, nv = (uint32_t)clamp(TPD(t, MA_POL)[PL_VOIC], 1, POL_NV), j, k;
    (void)rec;
    for (j = 0; j < n; j++)
        out[j] = 0;
    tape_view_of(t, (uint32_t)clamp(tp[t].pol[PL_REEL], 0, (int32_t)(NREEL + USLOT_N)), &vw);
    s->keys = keys;
    for (k = 0; off && k < NKEYBIT; k++)                     /* keys let go: their voices release */
        if ((off >> k) & 1u)
            for (j = 0; j < POL_NV; j++)
                if (s->v[j].key == k && s->v[j].stage && s->v[j].stage != 3u)
                    s->v[j].stage = 3;
    for (j = nv; j < POL_NV; j++)                           /* VOIC turned down: the voices above it let go */
        if (s->v[j].stage && s->v[j].stage != 3u)
            s->v[j].stage = 3;
    if (src_sleep) {                                    /* asleep (muted): no voice sounds; at waking the next keys
                                                         * start from silence */
        for (j = 0; j < POL_NV; j++)
            s->v[j].stage = 0, s->v[j].amp = 0, s->v[j].env = 0;
        return;
    }
    for (k = 0; on && vw.len && k < NKEYBIT; k++)            /* new keys, lowest first (nothing to play: none) */
        if ((on >> k) & 1u)
            pol_start(t, k, nv, vw.len);
    for (j = 0; j < POL_NV; j++)
        if (s->v[j].stage || s->v[j].amp)
            pol_voice(t, &s->v[j], &pol_rd[t][j], &vw, out, n);
}
