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
 * TWIN (2026-10-13, page 4): a second reel, B. MIX and XFD play both in each voice (B read at its own head, TUNB
 * semitones off A's pitch, from STRT of its own length; BLND, or the note in XFD, sets the balance), so two voices
 * at most, B's reader the spare voice's (pol_rd[t][j + 2]); ALT and RND give each note one reel, A or B (in turn, or
 * B by BLND's chance), four voices as ever. OFF is POLY as it was, to the bit.
 *
 * LOOP (2026-10-13, page 2): a held note (and its release) loops LSTR..END instead of ending at END. FWD jumps back
 * to LSTR, the level faded LXF ms down into the seam and up out of it (one reader a voice: no second head to
 * crossfade with); PING turns round at each end, seamless. The heads are read sample by sample then (tape_read).
 * TWIN's B loops its own LSTR..END the same way, without the fade.
 *
 * Audio ISR only, like synth.c: the main loop writes tp[t].pol and the octave. */

typedef struct {
    int32_t pos, inc;            /* the head and its step, Q12 tape samples */
    int32_t ic1, ic2;            /* the filter's state */
    int32_t env;                 /* Q24 */
    int32_t amp;                 /* the last block's level, Q15 */
    uint32_t age;
    uint8_t key, stage;          /* 0 silent, 1 attack, 2 decay and sustain, 3 release */
    uint8_t st2;                 /* ENV 2, the filter's (2026-10-15: dsp.c env_step): its stage, */
    int32_t e2;                  /* .. its level, Q24 */
    uint32_t age2;               /* .. the note it started on */
    uint8_t note;                /* the key's note at its octave when it went down (TUNE is added as it plays) */
    uint8_t endf;                /* END passed: blocks of its fade left (POL_ENDF..1; 0: not ending) */
    int32_t tn16, fcut;          /* what inc and flt were last worked out from (a divide each, a block, a voice:
                                  * only when they move; tok, fok: worked out at all) */
    int16_t tfine, fres;
    uint8_t tok, fok;
    tsvf_t flt;                  /* the filter's coefficients for fcut and fres */
    uint8_t b;                   /* TWIN ALT, RND: this note plays reel B */
    uint8_t tokb;                /* TWIN MIX, XFD: B's step worked out (for tn16b) */
    int32_t pos2, inc2, tn16b;   /* .. B's head, its step, the pitch it was worked out from */
    uint8_t dir, dir2;           /* LOOP PING: the head (and B's) running backward */
    uint8_t wrapped;             /* LOOP FWD: the head has come round the seam once (the fade-in at LSTR is due) */
    uint32_t tr;                 /* TRAN: the click's samples left (dsp.c click_step) */
    int32_t trng, tlp;           /* .. its noise and its low-pass */
} pol_voice_t;
#define POL_ENDF 6u              /* END's fade, blocks (4.4 ms: one block, 0.7 ms, stepped when END was modulated) */

typedef struct {
    pol_voice_t v[POL_NV];
    uint32_t keys, age;
    uint8_t alt;                 /* TWIN ALT: the next note's reel */
    int32_t rng;                 /* TWIN RND */
} pol_t;

static pol_t pol[NTRK];
static tape_rd_t pol_rd[NTRK][POL_NV] __attribute__((section(".lowpool")));   /* (RAMTEXT: app.ld) */   /* each voice's block (8 KB in all) */

/* the head's step for voice v's note on track t, Q12 tape samples per output sample: 0.5 at the sound's own pitch
 * (its ROOT's key, FINE cents off). TUNE is added here, every block, so a modulated TUNE moves a held note too, to
 * 1/16 semitone (mod.c); FINE to the cent (cents_inc). The divide runs only when the pitch or FINE moved */
static void HOT_CODE pol_tune(uint32_t t, pol_voice_t *v)
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

/* TWIN MIX, XFD: B's step, TUNB semitones off A's (the divide only when the pitch moved) */
static void HOT_CODE pol_tune_b(uint32_t t, pol_voice_t *v)
{
    const int16_t *p = TPD(t, MA_POL);
    int32_t n16 = clamp(((int32_t)v->note + 48 - p[PL_ROOT] + p[PL_TUNB]) * 16 + mod_pitch16(t, FN_POL, p[PL_TUNE]), 0,
                        127 * 16);
    if (v->tokb && n16 == v->tn16b)
        return;
    {
        uint32_t inc = p[PL_FINE] ? cents_inc(n16, p[PL_FINE], 0) : pitch_inc((uint32_t)n16);
        v->inc2 = (int32_t)(inc / (pitch_inc(48u * 16u) >> 11));
    }
    v->tn16b = n16;
    v->tokb = 1;
}

/* the voice's filter for a cutoff and RES: tsvf_coef's divides only when they moved */
static void HOT_CODE pol_filter(uint32_t t, pol_voice_t *v, int32_t cut)
{
    int16_t res = TPD(t, MA_POL)[PL_RES];
    if (v->fok && cut == v->fcut && res == v->fres)
        return;
    tsvf_coef_k(&v->flt, cut, tsvf_kres(res));         /* (RES past 92: it sings on its own, dsp.c) */
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

static void HOT_CODE pol_start(uint32_t t, uint32_t k, uint32_t nv, uint32_t len, uint32_t b, uint32_t lenb)
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
    v->b = (uint8_t)b;
    v->pos = pct100_of(b ? lenb : len, TPD(t, MA_POL)[PL_STRT]) << 12;
    v->pos2 = pct100_of(lenb, TPD(t, MA_POL)[PL_STRT]) << 12;   /* (TWIN MIX, XFD: B's head, from STRT of B) */
    v->tokb = 0;
    v->dir = v->dir2 = 0;
    v->wrapped = 0;
    v->note = (uint8_t)key_snap(clamp((int32_t)key_base[t] + (int32_t)k, 0, 127));   /* (the project's scale) */
    v->tr = TPD(t, MA_POL)[PL_TRAN] > 0 ? CLICK_N : 0u;   /* (TRAN: a click at every start) */
    v->tlp = 0;
    if (!v->trng)
        v->trng = 0x2545F491 + (int32_t)(t * 7919u + k);
    v->tok = 0;
    pol_tune(t, v);
    v->key = (uint8_t)k;
    v->stage = 1;
    v->endf = 0;
    v->age = ++s->age;
    pol_rd[t][v - s->v].ok = 0;
    if (nv <= 2u)
        pol_rd[t][(uint32_t)(v - s->v) + 2u].ok = 0;    /* (its B reader, when TWIN reads two reels) */
}

/* the voice's filter on x: TYPE's output, through tsvf_sat when RES is high (fs) */
#define PL_FLT(x) (fs ? tsvf_sat(&v->flt, (x), &ic1, &ic2, mode) : tsvf_mode(&v->flt, (x), &ic1, &ic2, mode))
static void HOT_CODE pol_voice(uint32_t t, pol_voice_t *v, tape_rd_t *rd, const tape_view_t *vw, int32_t *out, uint32_t n,
                               const tape_view_t *vwb, tape_rd_t *rdb, uint32_t tw)
{
    const int16_t *p = TPD(t, MA_POL);
    uint32_t i, mode = (uint32_t)clamp(p[PL_TYPE], 0, 2);
    int kill = (tw == TW_MIX || tw == TW_XFD) && !rdb;      /* (2026-10-18: a voice past the two that TWIN's two reels
                                                             * allow, still sounding when they came on: its reader is
                                                             * B's for another voice, so it fades out this block
                                                             * instead of reading both reels through one reader, which
                                                             * decoded a block every sample: 50 times the load) */
    int fs = p[PL_RES] > TSVF_SAT_RES;                  /* (RES high: the filter's soft limit, tsvf_sat) */
    uint32_t ety = (uint32_t)clamp(p[PL_ETYP], 0, 2);   /* (ETYP: both envelopes') */
    int32_t a0 = v->amp, a1, e15, pos = v->pos, ic1 = v->ic1, ic2 = v->ic2, c8;
    uint32_t tr = v->tr;                                    /* (TRAN's click: into x before the filter) */
    int32_t trng = v->trng, tlp = v->tlp, tg = clamp(p[PL_TRAN], 0, 100) * 327;
    env_step(&v->env, &v->stage, p[PL_ATK], p[PL_DEC], p[PL_SUS], p[PL_REL], ety);   /* the envelope, a step
                                                                                        * a block (as SYNTH's) */
    if (!v->stage) {                                        /* ENV 2: with the note; done with the voice */
        v->st2 = 0;
        v->e2 = 0;
    } else if (v->age != v->age2) {
        v->age2 = v->age;
        v->st2 = 1;
    } else if (v->stage == 3u && v->st2 && v->st2 != 3u) {
        v->st2 = 3;
    }
    env_step(&v->e2, &v->st2, p[PL_ATK2], p[PL_DEC2], p[PL_SUS2], p[PL_REL2], ety);
    e15 = v->env >> 9;
    a1 = e15;
    if (kill)                                               /* (two reels and no reader for B: out over this block) */
        a1 = 0;
    pol_tune(t, v);                                         /* (TUNE as it is now) */
    {
        uint32_t lpm = (uint32_t)clamp(tp[t].pol[PL_LOOP], 0, LP_PING), le = pol_end(t, vw->len), ls;
        ls = lpm ? (uint32_t)pct100_of(vw->len, p[PL_LSTR]) : 0u;
        if (lpm && le > ls + 64u) {                         /* LOOP: the head kept in LSTR..END, sample by sample */
            int32_t lo = (int32_t)(ls << 12), hi = (int32_t)(le << 12), span = hi - lo, xf = p[PL_LXF] * 22, da;
            int32_t los = 0, his = 0, span2 = 0, pos2 = v->pos2, y;
            int two = (tw == TW_MIX || tw == TW_XFD) && !kill, filt;
            int32_t gb = 0, ga = 32767;
            if (xf > (int32_t)((le - ls) / 2u))
                xf = (int32_t)((le - ls) / 2u);
            if (two) {
                int32_t bl = tw == TW_MIX ? p[PL_BLND] : clamp(((int32_t)v->note - 60) * 4 + p[PL_BLND], 0, 100);
                uint32_t le2 = (uint32_t)pct100_of(vwb->len, p[PL_END]), ls2 = (uint32_t)pct100_of(vwb->len, p[PL_LSTR]);
                gb = bl * 32767 / 100;
                ga = 32767 - gb;
                pol_tune_b(t, v);
                if (le2 > ls2 + 64u) {
                    los = (int32_t)(ls2 << 12);
                    his = (int32_t)(le2 << 12);
                    span2 = his - los;
                }
            }
            c8 = mod_fine8(t, FN_PCUT, p[PL_CUT]);
            pol_filter(t, v, c8 + (p[PL_ENV] ? p[PL_ENV] * 96 * (v->e2 >> 16) / 100 : 0));
            filt = mode || c8 < 127 << 8 || p[PL_RES] || p[PL_ENV];
            da = a1 - a0;
            v->endf = 0;
            for (i = 0; i < n; i++) {
                int32_t x = tape_read(vw, rd, pos), at = pos >> 12;
                if (lpm == LP_FWD && xf > 0) {               /* (the seam's fade: down into END, up out of LSTR) */
                    int32_t d = hi > pos ? (hi - pos) >> 12 : 0;
                    if (v->wrapped && at >= (int32_t)ls && at - (int32_t)ls < d)
                        d = at - (int32_t)ls;
                    if (d < xf)
                        x = x * d / xf;
                }
                if (two) {
                    x = mulq15(x, ga) + mulq15(tape_read(vwb, rdb, pos2), gb);
                    pos2 += v->dir2 ? -v->inc2 : v->inc2;
                    if (span2) {
                        if (lpm == LP_FWD && pos2 >= his) {
                            pos2 -= span2;
                        } else if (lpm == LP_PING) {
                            if (pos2 >= his) {
                                pos2 = 2 * his - pos2;
                                v->dir2 = 1;
                            } else if (v->dir2 && pos2 <= los) {
                                pos2 = 2 * los - pos2;
                                v->dir2 = 0;
                            }
                        }
                    }
                }
                pos += v->dir ? -v->inc : v->inc;
                if (lpm == LP_FWD && pos >= hi) {
                    pos -= span;
                    v->wrapped = 1;
                } else if (lpm == LP_PING) {
                    if (pos >= hi) {
                        pos = 2 * hi - pos;
                        v->dir = 1;
                    } else if (v->dir && pos <= lo) {
                        pos = 2 * lo - pos;
                        v->dir = 0;
                    }
                }
                if (tr)
                    x += click_step(&tr, &trng, &tlp, tg);
                y = filt ? PL_FLT(x >> 1) << 1 : x;
                out[i] += mulq15(y, a0 + ((da * (int32_t)i) >> CTL_LOG2));
            }
            v->pos = pos;
            v->pos2 = pos2;
            v->ic1 = ic1;
    v->tr = tr;
    v->trng = trng;
    v->tlp = tlp;
            v->ic2 = ic2;
            v->amp = a1;
            if (kill)
                v->stage = 0, v->env = 0;
            return;
        }
    }
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
    pol_filter(t, v, c8 + (p[PL_ENV] ? p[PL_ENV] * 96 * (v->e2 >> 16) / 100 : 0));   /* (ENV 0: no divide) */
    if (kill)
        a1 = 0;
    if ((tw == TW_MIX || tw == TW_XFD) && !kill) {          /* TWIN, both reels: B at its own head, the balance */
        int32_t bl = tw == TW_MIX ? p[PL_BLND] : clamp(((int32_t)v->note - 60) * 4 + p[PL_BLND], 0, 100);
        int32_t gb = bl * 32767 / 100, ga = 32767 - gb, pos2 = v->pos2, y, ba, bb;
        int filt = mode || c8 < 127 << 8 || p[PL_RES] || p[PL_ENV], da = a1 - a0;
        pol_tune_b(t, v);
        ba = tape_run(vw, rd, pos, v->inc, n);
        bb = ba >= 0 ? tape_run(vwb, rdb, pos2, v->inc2, n) : -1;
        if (bb >= 0) {                                      /* (both heads' runs inside one decoded block each: read
                                                             * straight from them, as the single reel does) */
            for (i = 0; i < n; i++) {
                int32_t o = (pos >> 12) - ba, o2 = (pos2 >> 12) - bb, a = rd->s[o], b = rd->s[o + 1];
                int32_t a2 = rdb->s[o2], b2 = rdb->s[o2 + 1];
                int32_t x = mulq15(a + (((b - a) * (pos & 4095)) >> 12), ga) +
                            mulq15(a2 + (((b2 - a2) * (pos2 & 4095)) >> 12), gb);
                pos += v->inc;
                pos2 += v->inc2;
                if (tr)
                    x += click_step(&tr, &trng, &tlp, tg);
                y = filt ? PL_FLT(x >> 1) << 1 : x;
                out[i] += mulq15(y, a0 + ((da * (int32_t)i) >> CTL_LOG2));
            }
        } else {
            for (i = 0; i < n; i++) {
                int32_t x = mulq15(tape_read(vw, rd, pos), ga) + mulq15(tape_read(vwb, rdb, pos2), gb);
                pos += v->inc;
                pos2 += v->inc2;
                if (tr)
                    x += click_step(&tr, &trng, &tlp, tg);
                y = filt ? PL_FLT(x >> 1) << 1 : x;
                out[i] += mulq15(y, a0 + ((da * (int32_t)i) >> CTL_LOG2));
            }
        }
        v->pos2 = pos2;
    } else {
        int filt = mode || c8 < 127 << 8 || p[PL_RES] || p[PL_ENV];   /* (decided once a block, not a sample) */
        int32_t base = tape_run(vw, rd, pos, v->inc, n), da = a1 - a0, y;
        if (base >= 0) {                                    /* the block's run inside one decoded block (tape.c): the
                                                             * samples read straight from it, as tape_read reads them */
            for (i = 0; i < n; i++) {
                int32_t o = (pos >> 12) - base, a = rd->s[o], b = rd->s[o + 1];
                int32_t x = a + (((b - a) * (pos & 4095)) >> 12);
                pos += v->inc;
                if (tr)
                    x += click_step(&tr, &trng, &tlp, tg);
                y = filt ? PL_FLT(x >> 1) << 1 : x;
                out[i] += mulq15(y, a0 + ((da * (int32_t)i) >> CTL_LOG2));
            }
        } else {
            for (i = 0; i < n; i++) {
                int32_t x = tape_read(vw, rd, pos);
                pos += v->inc;
                if (tr)
                    x += click_step(&tr, &trng, &tlp, tg);
                y = filt ? PL_FLT(x >> 1) << 1 : x;
                out[i] += mulq15(y, a0 + ((da * (int32_t)i) >> CTL_LOG2));
            }
        }
    }
    v->pos = pos;
    v->ic1 = ic1;
    v->tr = tr;
    v->trng = trng;
    v->tlp = tlp;
    v->ic2 = ic2;
    v->amp = a1;
    if (kill)
        v->stage = 0, v->env = 0;
}

/* audio ISR: one block of track t's POLY into out; keys: the keys held (bit k: key_base + k); rec: unused (REC prints POLY onto the
 * tape, chain.c) */
static void HOT_CODE pol_block(uint32_t t, uint32_t keys, const int32_t *rec, int32_t *out, uint32_t n)
{
    pol_t *s = &pol[t];
    tape_view_t vw;
    tape_view_t vwb;
    uint32_t on = keys & ~s->keys, off = s->keys & ~keys, nv = (uint32_t)clamp(TPD(t, MA_POL)[PL_VOIC], 1, POL_NV), j, k;
    uint32_t tw = (uint32_t)clamp(tp[t].pol[PL_TWIN], 0, TW_RND);
    (void)rec;
    for (j = 0; j < n; j++)
        out[j] = 0;
    tape_view_of(t, (uint32_t)clamp(tp[t].pol[PL_REEL], 0, (int32_t)(NREEL + USLOT_N)), &vw);
    vwb = vw;
    if (tw) {
        tape_view_of(t, (uint32_t)clamp(tp[t].pol[PL_RLB], 0, (int32_t)(NREEL + USLOT_N)), &vwb);
        if ((tw == TW_MIX || tw == TW_XFD) && nv > 2u)
            nv = 2;                                     /* (two reels a voice: two voices, their spares' readers) */
    }
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
        s->keys = 0;                                    /* (2026-10-18: and the keys forgotten, so keys still held at
                                                         * waking strike again: an arpeggiator's HOLD chord, a key held
                                                         * through the mute) */
        return;
    }
    for (k = 0; on && (vw.len || (tw >= TW_ALT && vwb.len)) && k < NKEYBIT; k++)   /* new keys, lowest first
                                                         * (nothing to play: none) */
        if ((on >> k) & 1u) {
            uint32_t b = 0;
            if (tw == TW_ALT)                           /* (ALT: A, B, A ..; RND: B by BLND's chance) */
                b = s->alt ^= 1u;
            else if (tw == TW_RND && (s->rng || (s->rng = 0x2545F49 + (int32_t)t)))
                b = (uint32_t)(noise32(&s->rng) % 100u) < (uint32_t)clamp(TPD(t, MA_POL)[PL_BLND], 0, 100);
            if (!(b ? vwb.len : vw.len))
                continue;
            pol_start(t, k, nv, vw.len, b, vwb.len);
        }
    for (j = 0; j < POL_NV; j++)
        if (s->v[j].stage || s->v[j].amp)
            pol_voice(t, &s->v[j], &pol_rd[t][j], s->v[j].b && tw >= TW_ALT ? &vwb : &vw, out, n, &vwb,
                      j < 2u ? &pol_rd[t][j + 2u] : 0, tw);   /* (B's reader: the spare voices', so 2 voices) */
}
