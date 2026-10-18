/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: the arpeggiator, one per track, on the ARP pad (param.c ARP_P). It sits between the keys and a SYNTH or
 * POLY track's source: the keys go in as notes, one note comes out at a time, in time with the tempo.
 *
 * How it plays: chain.c hands it the keys held on the track each block (a mask over key_base, bryo.h) and gets back
 * the mask the source plays: bit 0 while a step's note sounds, with key_base set to that note. Between two steps
 * there is always at least one silent block (GATE stops a block short of the next step at 100 %), so the source sees
 * every step as a new key press: its envelope starts again, and so do the modulators' KEY triggers (mod.c). The
 * source needs nothing new, which is the point.
 *
 * The notes it holds, in the order they came (PLAY's order; the others sort them):
 *   KEY   the keys held, as they're held; let them all go and it stops
 *   LTCH  the same while any key is down; let them all go and the last chord goes on playing, until a key pressed
 *         after that starts a new one
 *   ADD   every key pressed joins for good (past ARP_MAXN, the oldest leaves): pile up an endless pattern. POLY held
 *         on the ARP page clears it (ui_input.c), and so does MODE turned to OFF
 * A note that plays on without its key (latched, piled up, fading) leaves when its key is pressed again (2026-10-15:
 * "being able to remove keys from arps"); in LTCH a key that isn't in the chord still starts a new one, in ADD it
 * joins. FADE (2026-10-15, "arps could fade out instead of just on or off"): let every key go and the notes go on
 * for FADE, their level falling to silence (arp_level: chain.c scales a SYNTH or POLY source by it; TAPE's slices
 * and DRUM's drums can't fade under the reel or the pattern, so there the steps just stop at the end of FADE). At the
 * end KEY and LTCH forget the notes, ADD keeps its pile quiet until a key; any key brings the level straight back.
 * At FADE 0, KEY stops the moment the keys are let go and LTCH and ADD hold, as before.
 * GATE past 100 % (2026-10-17, "notes longer than a step"): a note rings on into the steps after it, up to 400 %
 * (four steps), so the notes overlap. SYNTH and POLY know a voice by its bit in the mask, so the mask can't move
 * under held notes: with a long gate it's taken over the pattern's lowest note (tbase) and each note still ringing
 * keeps its bit there (tnote, tleft: up to ARP_NT of them). A step whose note is still ringing ends that one and
 * strikes it a block later (the source sees a press); a note more than 31 semitones over the base, or the base
 * moving (the notes changed), ends the overlap and plays that step as before. TAPE and DRUM stop at 100 % (one
 * slice, one drum at a time). With one voice (VOIC 1) the overlap is a legato. At 100 % and under, as it was.
 * HOLD (2026-10-18, "smooth evolving sounds that don't rapidly repeat"): the notes held as LTCH holds them, but no
 *   steps: they sound together, a sustained chord, struck when it starts and again only where it changes (the mask
 *   is taken over the chord's lowest note, so a note that stays keeps its bit and its voice). A pad from a latched
 *   chord, following the transport as LTCH does
 * RNGE spreads them over 1 to 3 octaves (the notes, then the notes an octave up, ...), ORDR walks that UP, DOWN,
 * UPDN (up and back down, the top and bottom once), PLAY (the order pressed, then again an octave up) or RND.
 *
 * The clock is its own: the first note sounds the moment a key goes down (no waiting for the next beat), and each
 * step is RATE long at the tempo, SWNG lengthening the first of each pair of steps and shortening the second. Keys
 * held play whether the transport runs or not, like the keys. Notes it holds with no key down (latched, piled up,
 * or put back by a project, arp_seed) follow the transport: STOP quiets them, PLAY starts them from the first step,
 * a key pressed brings them back at once.
 *
 * Audio ISR, except arp_clear (main loop: a request the ISR takes) and what the screen reads (arp_rt, torn reads
 * are only a frame's drawing). */

#define ARP_MAXN 32u                 /* the notes it holds */
#define ARP_FD_FULL 8388607          /* a fade all the way down (Q23) */
#define ARP_NT 4u                    /* notes still ringing past their step (GATE up to 400 %) */
#define ARP_GMAX 400                 /* GATE's top, % of a step */

typedef struct {
    uint8_t note[ARP_MAXN];          /* the notes held, absolute (0..127), in the order they came */
    uint8_t n;
    uint8_t latched;                 /* LTCH: every key was let go, so the next press starts a new chord */
    uint8_t on;                      /* a step's note is sounding */
    uint8_t cur;                     /* .. which (what key_base is set to) */
    uint8_t gap;                     /* a silent block owed (the arpeggiator just turned off with a note sounding) */
    uint8_t base_in;                 /* the keys' key_base last block */
    uint8_t mode;                    /* MODE as of last block */
    uint8_t waiting;                 /* notes held with no key down, waiting for PLAY (or a key) */
    uint8_t playing;                 /* the transport as of last block */
    uint8_t fading;                  /* every key let go with FADE on: the notes go on, their level falling */
    uint8_t faded;                   /* ADD: the fade ended, the pile kept, quiet until a key */
    uint32_t held;                   /* the keys held last block */
    int32_t fd, fstep;               /* how far the fade has gone, Q23 (0: full level, ARP_FD_FULL: silent), and a
                                      * block's step of it */
    int32_t phase, len, gate;        /* samples into this step, its length, how long its note sounds */
    uint8_t tnote[ARP_NT];           /* GATE past 100 %: the notes still ringing from earlier steps, */
    int32_t tleft[ARP_NT];           /* .. the samples each has left, past this block's start */
    uint8_t nt;                      /* .. how many */
    uint8_t tbase;                   /* .. the mask's base while they ring (the pattern's lowest note) */
    uint8_t lng;                     /* this step's note is long (its gate past the step) */
    uint8_t defer;                   /* this step's note was still ringing: silent for a block, then struck */
    uint32_t pos;                    /* steps played since the notes started */
    uint32_t rng;                    /* RND's generator */
    uint32_t grng;                   /* GRND's */
} arp_rt_t;

static arp_rt_t arp_rt[NTRK];
static volatile uint8_t arp_clear_req[NTRK];   /* main loop -> ISR: forget the notes (POLY held on the ARP page) */
static volatile uint8_t arp_seed_req[NTRK];    /* main loop -> ISR: hold these notes (a project, the factory scene) */
static uint8_t arp_seed_note[NTRK][ARP_MAXN];
static uint8_t arp_seed_n[NTRK];

/* main loop: track t's arpeggiator holds notes[0..n) (absolute, as pressed), as if latched, waiting for PLAY */
static void arp_seed(uint32_t t, const uint8_t *notes, uint32_t n)
{
    uint32_t i;
    if (t >= NTRK)
        return;
    n = n < ARP_MAXN ? n : ARP_MAXN;
    for (i = 0; i < n; i++)
        arp_seed_note[t][i] = notes[i] < 128u ? notes[i] : 127u;
    arp_seed_n[t] = (uint8_t)n;
    RING_PUBLISH();                                      /* (the notes before the request) */
    arp_seed_req[t] = 1;
}

/* main loop: forget track t's notes (they stop at the next block) */
static void arp_clear(uint32_t t)
{
    if (t < NTRK)
        arp_clear_req[t] = 1;
}

/* track t's arpeggiator is on (every source: SYNTH and POLY notes, TAPE's slices, DRUM's drums) */
static int arp_on(uint32_t t) { return tp[t].arp[AR_MODE] != ARM_OFF; }

static arp_rt_t reso_arp[NTRK];      /* RIPPLE's own arpeggio, one a track (its knobs: RP_AMODE.., reso.c) */

static void arp_add(arp_rt_t *r, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < r->n; i++)
        if (r->note[i] == note)
            return;
    if (r->n == ARP_MAXN) {                              /* (full: the oldest leaves) */
        for (i = 1; i < ARP_MAXN; i++)
            r->note[i - 1u] = r->note[i];
        r->n--;
    }
    r->note[r->n++] = (uint8_t)note;
}

static void arp_remove(arp_rt_t *r, uint32_t note)
{
    uint32_t i, j;
    for (i = j = 0; i < r->n; i++)
        if (r->note[i] != note)
            r->note[j++] = r->note[i];
    r->n = (uint8_t)j;
}

static int arp_has(const arp_rt_t *r, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < r->n; i++)
        if (r->note[i] == note)
            return 1;
    return 0;
}

/* the level track t's arpeggiated notes play at, Q15 (32767 but for a fade in progress, or ended: chain.c) */
static int32_t HOT_CODE arp_level(uint32_t t) { return 32767 - (arp_rt[t].fd >> 8); }

/* the keys let go with FADE: the fade starts (its step a block, from the time; one divide, at the let-go) */
static void HOT_CODE arp_fade_start(arp_rt_t *r, int32_t fade)
{
    int32_t samples = (int32_t)TIME_MS_X10[fade & 127] * 441 / 100;
    r->fading = 1;
    r->fd = 0;
    r->fstep = (ARP_FD_FULL / (samples > 0 ? samples : 1)) * (int32_t)CTL;
    if (r->fstep > ARP_FD_FULL)
        r->fstep = ARP_FD_FULL;
}

static void arp_fade_reset(arp_rt_t *r) { r->fading = r->faded = 0; r->fd = 0; }

/* a step's length at the tempo, in samples: RATE's division of a beat (1/4 = a beat), SWNG's long or short half */
static int32_t HOT_CODE arp_len(const int16_t *a, uint32_t step)
{
    static const uint8_t DIV[6] = {1, 2, 3, 4, 6, 8};   /* 1/4 1/8 8T 1/16 16T 1/32: steps a beat */
    int32_t bpm = clamp((int32_t)sys.bpm, 40, 240), d = DIV[clamp(a[AR_RATE], 0, 5)];
    int32_t len = (int32_t)MOD_BEAT_DEN / (bpm * d), sw   /* (60 x 44100: samples a minute) */ = len * clamp(a[AR_SWNG], 0, 100) / 300;
    return step & 1u ? len - sw : len + sw;
}

/* the notes held into s, low to high (PLAY: in the order pressed); their count */
static uint32_t HOT_CODE arp_notes(const int16_t *a, const arp_rt_t *r, uint8_t *s)
{
    uint32_t n = r->n, i, j;
    for (i = 0; i < n; i++)
        s[i] = r->note[i];
    if (a[AR_ORDR] != ARO_PLAY)
        for (i = 1; i < n; i++)
            for (j = i; j > 0u && s[j - 1u] > s[j]; j--) {
                uint8_t x = s[j];
                s[j] = s[j - 1u];
                s[j - 1u] = x;
            }
    return n;
}

/* the steps before ORDR's walk repeats over len places (UPDN: up and back, the ends once) */
static uint32_t HOT_CODE arp_period(uint32_t ord, uint32_t len) { return ord == ARO_UPDN && len > 1u ? 2u * len - 2u : len; }

/* the place step pos takes among len (n notes, then each an octave up, ...), walked by ORDR (RND: rnd) */
static uint32_t HOT_CODE arp_place(uint32_t ord, uint32_t len, uint32_t pos, uint32_t rnd)
{
    uint32_t k = pos % arp_period(ord, len);
    switch (ord) {
    case ARO_DOWN: return len - 1u - k;
    case ARO_UPDN: return k >= len ? 2u * len - 2u - k : k;
    case ARO_RND: return rnd % len;
    default: return k;                                   /* UP, PLAY */
    }
}

/* the note at place k: note k mod n, k / n octaves up */
static uint32_t HOT_CODE arp_at(const uint8_t *s, uint32_t n, uint32_t k)
{
    return (uint32_t)clamp((int32_t)s[k % n] + 12 * (int32_t)(k / n), 0, 127);
}

/* the note of the next step: RNGE octaves of the notes held, walked by ORDR */
static uint32_t HOT_CODE arp_pick(const int16_t *a, arp_rt_t *r)
{
    uint8_t s[ARP_MAXN];
    uint32_t n = arp_notes(a, r, s), len = n * (uint32_t)clamp(a[AR_RNGE], 1, 3);
    r->rng = r->rng * 1664525u + 1013904223u;
    return arp_at(s, n, arp_place((uint32_t)a[AR_ORDR], len, r->pos, r->rng >> 8));
}

static uint32_t HOT_CODE arp_mask(arp_rt_t *r, uint32_t *nbase);

/* audio ISR: one block of an arpeggiator (state r, knobs a: AR_*; on: it plays). keys: the keys held (a mask over
 * base); returns the mask that plays, and in *nbase its key_base (a step's note: bit 0 at that base) */
static uint32_t HOT_CODE arp_run(arp_rt_t *r, const int16_t *a, int on, uint32_t keys, uint32_t base, uint32_t n,
                        uint32_t *nbase, uint32_t salt)
{
    uint32_t mode = (uint32_t)clamp(a[AR_MODE], 0, 4), press = keys & ~r->held, k;
    int32_t fade = clamp(a[AR_FADE], 0, 127);
    if (!r->rng)
        r->rng = 0x9E3779B9u ^ (salt * 0x85EBCA6Bu);
    if (mode != r->mode) {
        if (mode == ARM_OFF || r->mode == ARM_OFF || mode == ARM_KEY)
            r->n = 0;                                    /* (turned on, off, or back to KEY: from the keys as they
                                                          * are) */
        arp_fade_reset(r);
        if (!((mode == ARM_LTCH || mode == ARM_HOLD) && (r->mode == ARM_LTCH || r->mode == ARM_HOLD)))
            r->latched = 0;                              /* (LTCH and HOLD to each other: the chord stays latched) */
    }
    r->mode = (uint8_t)mode;
    if (sys.playing != r->playing) {                     /* PLAY: notes held without keys start over; STOP: they wait */
        r->playing = sys.playing;
        r->waiting = !sys.playing && !keys;
        r->len = 0;
    }
    if (mode == ARM_OFF || !on) {                        /* off: the keys straight through (a silent block first if a
                                                          * step's note was sounding, so it ends) */
        r->held = keys;
        r->base_in = (uint8_t)base;
        r->pos = 0;
        r->nt = r->lng = r->defer = 0;
        arp_fade_reset(r);
        if (r->on) {
            r->on = 0;
            r->gap = 1;
        }
        *nbase = base;
        if (r->gap) {
            r->gap = 0;
            return 0;
        }
        return keys;
    }
    if (mode == ARM_KEY && base != r->base_in) {         /* (the octave moved under held keys: the notes as they are) */
        r->n = 0;
        r->latched = 0;
        arp_fade_reset(r);
        press = keys;
    }
    if (press && (mode == ARM_ADD || r->latched)) {      /* a key while the notes play on without keys (latched, piled
                                                          * up, fading): one of theirs leaves; a new one in ADD joins,
                                                          * in KEY and LTCH starts a new chord */
        uint32_t fresh = 0;
        for (k = 0; k < NKEYBIT; k++)
            if ((press >> k) & 1u && base + k <= 127u && !arp_has(r, base + k))
                fresh = 1;
        if (fresh && mode != ARM_ADD) {
            r->n = 0;
            r->latched = 0;
        } else {
            for (k = 0; k < NKEYBIT; k++)
                if ((press >> k) & 1u && base + k <= 127u && arp_has(r, base + k)) {
                    arp_remove(r, base + k);
                    press &= ~(1u << k);
                }
        }
    }
    if (!keys && r->held && fade && r->n && !r->fading)   /* every key let go with FADE: the notes go on, fading; KEY
                                                          * and LTCH hold them meanwhile as a latched chord */
        arp_fade_start(r, fade);
    if (mode == ARM_KEY && !r->fading)                   /* KEY: the keys let go leave */
        for (k = 0; k < NKEYBIT; k++)
            if (((r->held & ~keys) >> k) & 1u)
                arp_remove(r, base + k);
    for (k = 0; k < NKEYBIT; k++)                        /* the keys pressed join, lowest first */
        if ((press >> k) & 1u && base + k <= 127u)
            arp_add(r, base + k);
    if ((mode == ARM_LTCH || mode == ARM_HOLD || r->fading) && !keys && r->held)
        r->latched = 1;
    if (keys & ~r->held) {                               /* (a key: the notes play now, transport or not, at full) */
        r->waiting = 0;
        arp_fade_reset(r);
    }
    r->held = keys;
    r->base_in = (uint8_t)base;
    if (r->fading) {                                     /* the fade runs on, keys or not (a key reset it above) */
        r->fd += (r->fstep * (int32_t)n) >> CTL_LOG2;
        if (r->fd >= ARP_FD_FULL) {                      /* silent: KEY and LTCH forget the notes, ADD keeps its pile
                                                          * quiet until a key */
            r->fd = ARP_FD_FULL;
            r->fading = 0;
            if (mode == ARM_ADD) {
                r->faded = 1;
            } else {
                r->n = 0;
                r->latched = 0;
            }
        }
    }
    if (!r->n || (r->waiting && !keys) || r->faded) {    /* nothing to play (or waiting for PLAY, or faded out):
                                                          * quiet, and the next note starts at once */
        r->on = 0;
        r->pos = 0;
        r->phase = r->len = 0;
        r->nt = r->lng = r->defer = 0;
        *nbase = r->cur;
        return 0;
    }
    if (mode == ARM_HOLD) {                              /* HOLD: every note at once, over the lowest (no steps) */
        uint32_t lo = 127, m = 0;
        for (k = 0; k < r->n; k++)
            lo = r->note[k] < lo ? r->note[k] : lo;
        for (k = 0; k < r->n; k++)
            if (r->note[k] - lo < 32u)
                m |= 1u << (r->note[k] - lo);
        r->on = 0;
        r->nt = r->lng = r->defer = 0;
        r->cur = (uint8_t)lo;
        *nbase = lo;
        return m;
    }
    if (r->len)                                          /* (len 0: nothing played yet, the first note starts now) */
        r->phase += (int32_t)n;
    for (k = 0; k < r->nt; k++)                          /* (the notes ringing on: a block less each) */
        r->tleft[k] -= (int32_t)n;
    while (r->phase >= r->len) {                         /* a step starts */
        int32_t gmax = clamp(a[AR_GATE], 10, ARP_GMAX) > 100 ? ARP_GMAX : 100;
        if (r->on && r->lng && r->gate > r->phase && r->nt < ARP_NT) {   /* the last note rings on past its step */
            r->tnote[r->nt] = r->cur;
            r->tleft[r->nt++] = r->gate - r->phase;
        }
        r->phase -= r->len;
        r->len = arp_len(a, r->pos);
        r->cur = (uint8_t)arp_pick(a, r);
        r->pos++;
        {
            int32_t g = clamp(a[AR_GATE], 10, gmax), gr = clamp(a[AR_GRND], 0, 100);
            if (gr) {                                    /* GRND: this step's gate GATE +- up to GRND points, at
                                                          * random (its own generator: RND's order stays as it was) */
                r->grng = r->grng * 1664525u + 1013904223u;
                g = clamp(g + (int32_t)((r->grng >> 8) % (uint32_t)(2 * gr + 1)) - gr, 10, gmax);
            }
            r->lng = g > 100;
            r->gate = r->lng ? r->len * g / 100 : clamp(r->len * g / 100, (int32_t)CTL, r->len - (int32_t)CTL);
        }
        r->on = 1;
        r->defer = 0;
        for (k = 0; k < r->nt; k++)                      /* its note still ringing: that one ends, this one strikes
                                                          * a block later */
            if (r->tnote[k] == r->cur && r->tleft[k] > 0) {
                r->tleft[k] = 0;
                r->defer = 1;
            }
    }
    if (r->phase >= r->gate)
        r->on = 0;
    return arp_mask(r, nbase);
}

/* audio ISR: what an arpeggiator plays this block, its notes as a mask over *nbase: the step's note alone (bit 0
 * at its note), or with GATE past 100 % the notes still ringing too, over the pattern's lowest note */
static uint32_t HOT_CODE arp_mask(arp_rt_t *r, uint32_t *nbase)
{
    uint32_t k, j, m = 0, lo = 127;
    for (k = j = 0; k < r->nt; k++)                      /* (the ones done leave) */
        if (r->tleft[k] > 0) {
            r->tnote[j] = r->tnote[k];
            r->tleft[j++] = r->tleft[k];
        }
    r->nt = (uint8_t)j;
    if (!r->nt && !r->lng) {                             /* (no overlap: as it was) */
        r->defer = 0;
        *nbase = r->cur;
        return r->on;
    }
    for (k = 0; k < r->n; k++)                           /* the base: the pattern's lowest note */
        lo = r->note[k] < lo ? r->note[k] : lo;
    if (lo != r->tbase || r->cur < lo || r->cur > lo + 31u) {   /* the notes changed, or out of reach: the
                                                                 * overlap ends, this step as before */
        r->tbase = (uint8_t)lo;
        r->nt = 0;
        if (r->cur < lo || r->cur > lo + 31u) {
            *nbase = r->cur;
            return r->on;
        }
    }
    for (k = 0; k < r->nt; k++)
        if (r->tnote[k] >= lo && r->tnote[k] <= lo + 31u)
            m |= 1u << (r->tnote[k] - lo);
    if (r->on && !r->defer)
        m |= 1u << (r->cur - lo);
    if (r->defer)                                        /* (the struck-again note silent this block, on the next) */
        r->defer = 0;
    *nbase = lo;
    return m;
}

/* audio ISR: one block of track t's arpeggiator (the ARP pad's), between its keys and its source. SYNTH and POLY:
 * notes (keys a mask over base; a step plays bit 0 at *nbase). TAPE and DRUM: the keys are slices or drums (0..15),
 * a step plays that one's key, and RNGE has no octaves to spread over. Returns the mask the source plays */
static uint32_t HOT_CODE arp_block(uint32_t t, uint32_t keys, uint32_t base, uint32_t n, uint32_t *nbase)
{
    arp_rt_t *r = &arp_rt[t];
    int16_t a[NARP];
    uint32_t i, out, nb;
    int notes;
    if (arp_clear_req[t]) {
        arp_clear_req[t] = 0;
        r->n = 0;
    }
    if (arp_seed_req[t]) {                               /* notes put back (a project): latched, waiting for PLAY */
        for (i = 0; i < arp_seed_n[t]; i++)
            r->note[i] = arp_seed_note[t][i];
        r->n = arp_seed_n[t];
        r->latched = 1;
        r->waiting = !sys.playing;
        r->len = 0;
        r->held = keys;
        r->mode = (uint8_t)clamp(tp[t].arp[AR_MODE], 0, 4);   /* (as the mode: arp_run's turned-on clear passes them) */
        arp_seed_req[t] = 0;
    }
    notes = tp[t].src == SRC_SYNTH || tp[t].src == SRC_POLY;
    if (tp[t].arp[AR_MODE] == ARM_OFF && r->mode == ARM_OFF && !r->on && !r->gap) {   /* off and quiet (most tracks,
                                                         * most blocks): the keys straight through, keeping what
                                                         * arp_run's off path keeps (the keys, the transport's edge) */
        if (sys.playing != r->playing) {
            r->playing = sys.playing;
            r->waiting = !sys.playing && !keys;
            r->len = 0;
        }
        r->held = notes ? keys : keys & 0xFFFFu;
        r->base_in = (uint8_t)(notes ? base : 0u);
        r->pos = 0;
        *nbase = base;
        return keys;
    }
    if (notes)
        return arp_run(r, tp[t].arp, arp_on(t), keys, base, n, nbase, t);
    *nbase = base;
    if (tp[t].arp[AR_MODE] == ARM_OFF)                   /* (off: the keys straight through; no copy, RNGE unread) */
        return arp_run(r, tp[t].arp, 0, keys & 0xFFFFu, 0u, n, &nb, t);
    for (i = 0; i < NARP; i++)                           /* TAPE, DRUM: the slices or drums as they are */
        a[i] = tp[t].arp[i];
    a[AR_RNGE] = 1;
    a[AR_GATE] = a[AR_GATE] < 100 ? a[AR_GATE] : 100;   /* (one slice or drum at a time: no overlap) */
    out = arp_run(r, a, 1, keys & 0xFFFFu, 0u, n, &nb, t);
    if (a[AR_MODE] == ARM_HOLD)                          /* (HOLD: the slices or drums held, all of them) */
        return (out << (nb & 15u)) & 0xFFFFu;
    return out ? 1u << (nb & 15u) : 0u;                  /* (a step: that slice's or drum's key) */
}
