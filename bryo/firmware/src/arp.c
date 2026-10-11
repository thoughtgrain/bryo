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
 * RNGE spreads them over 1 to 3 octaves (the notes, then the notes an octave up, ...), ORDR walks that UP, DOWN,
 * UPDN (up and back down, the top and bottom once), PLAY (the order pressed, then again an octave up) or RND.
 *
 * The clock is its own: the first note sounds the moment a key goes down (no waiting for the next beat), and each
 * step is RATE long at the tempo, SWNG lengthening the first of each pair of steps and shortening the second. It
 * runs whether the transport plays or not, like the keys.
 *
 * Audio ISR, except arp_clear (main loop: a request the ISR takes) and what the screen reads (arp_rt, torn reads
 * are only a frame's drawing). */

#define ARP_MAXN 32u                 /* the notes it holds */

typedef struct {
    uint8_t note[ARP_MAXN];          /* the notes held, absolute (0..127), in the order they came */
    uint8_t n;
    uint8_t latched;                 /* LTCH: every key was let go, so the next press starts a new chord */
    uint8_t on;                      /* a step's note is sounding */
    uint8_t cur;                     /* .. which (what key_base is set to) */
    uint8_t gap;                     /* a silent block owed (the arpeggiator just turned off with a note sounding) */
    uint8_t base_in;                 /* the keys' key_base last block */
    uint8_t mode;                    /* MODE as of last block */
    uint32_t held;                   /* the keys held last block */
    int32_t phase, len, gate;        /* samples into this step, its length, how long its note sounds */
    uint32_t pos;                    /* steps played since the notes started */
    uint32_t rng;                    /* RND's generator */
} arp_rt_t;

static arp_rt_t arp_rt[NTRK];
static volatile uint8_t arp_clear_req[NTRK];   /* main loop -> ISR: forget the notes (POLY held on the ARP page) */

/* main loop: forget track t's notes (they stop at the next block) */
static void arp_clear(uint32_t t)
{
    if (t < NTRK)
        arp_clear_req[t] = 1;
}

/* track t's arpeggiator is on and its source is one it plays (SYNTH, POLY) */
static int arp_on(uint32_t t)
{
    return tp[t].arp[AR_MODE] != ARM_OFF && (tp[t].src == SRC_SYNTH || tp[t].src == SRC_POLY);
}

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

/* a step's length at the tempo, in samples: RATE's division of a beat (1/4 = a beat), SWNG's long or short half */
static int32_t arp_len(uint32_t t, uint32_t step)
{
    static const uint8_t DIV[6] = {1, 2, 3, 4, 6, 8};   /* 1/4 1/8 8T 1/16 16T 1/32: steps a beat */
    int32_t bpm = clamp((int32_t)sys.bpm, 40, 240), d = DIV[clamp(tp[t].arp[AR_RATE], 0, 5)];
    int32_t len = (int32_t)MOD_BEAT_DEN / (bpm * d), sw   /* (60 x 44100: samples a minute) */ = len * clamp(tp[t].arp[AR_SWNG], 0, 100) / 300;
    return step & 1u ? len - sw : len + sw;
}

/* the notes held into s, low to high (PLAY: in the order pressed); their count */
static uint32_t arp_notes(uint32_t t, const arp_rt_t *r, uint8_t *s)
{
    uint32_t n = r->n, i, j;
    for (i = 0; i < n; i++)
        s[i] = r->note[i];
    if (tp[t].arp[AR_ORDR] != ARO_PLAY)
        for (i = 1; i < n; i++)
            for (j = i; j > 0u && s[j - 1u] > s[j]; j--) {
                uint8_t x = s[j];
                s[j] = s[j - 1u];
                s[j - 1u] = x;
            }
    return n;
}

/* the steps before ORDR's walk repeats over len places (UPDN: up and back, the ends once) */
static uint32_t arp_period(uint32_t ord, uint32_t len) { return ord == ARO_UPDN && len > 1u ? 2u * len - 2u : len; }

/* the place step pos takes among len (n notes, then each an octave up, ...), walked by ORDR (RND: rnd) */
static uint32_t arp_place(uint32_t ord, uint32_t len, uint32_t pos, uint32_t rnd)
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
static uint32_t arp_at(const uint8_t *s, uint32_t n, uint32_t k)
{
    return (uint32_t)clamp((int32_t)s[k % n] + 12 * (int32_t)(k / n), 0, 127);
}

/* the note of the next step: RNGE octaves of the notes held, walked by ORDR */
static uint32_t arp_pick(uint32_t t, arp_rt_t *r)
{
    uint8_t s[ARP_MAXN];
    uint32_t n = arp_notes(t, r, s), len = n * (uint32_t)clamp(tp[t].arp[AR_RNGE], 1, 3);
    r->rng = r->rng * 1664525u + 1013904223u;
    return arp_at(s, n, arp_place((uint32_t)tp[t].arp[AR_ORDR], len, r->pos, r->rng >> 8));
}

/* audio ISR: one block of track t's arpeggiator. keys: the keys held on the track (a mask over base); returns the
 * mask the source plays, and in *nbase its key_base */
static uint32_t arp_block(uint32_t t, uint32_t keys, uint32_t base, uint32_t n, uint32_t *nbase)
{
    arp_rt_t *r = &arp_rt[t];
    uint32_t mode = (uint32_t)clamp(tp[t].arp[AR_MODE], 0, 3), press = keys & ~r->held, k;
    if (!r->rng)
        r->rng = 0x9E3779B9u ^ (t * 0x85EBCA6Bu);
    if (arp_clear_req[t]) {
        arp_clear_req[t] = 0;
        r->n = 0;
    }
    if (mode != r->mode && (mode == ARM_OFF || r->mode == ARM_OFF || mode == ARM_KEY))
        r->n = 0;                                        /* (turned on, off, or back to KEY: from the keys as they are) */
    r->mode = (uint8_t)mode;
    if (mode == ARM_OFF || !arp_on(t)) {                 /* off: the keys straight through (a silent block first if a
                                                          * step's note was sounding, so it ends) */
        r->held = keys;
        r->base_in = (uint8_t)base;
        r->pos = 0;
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
        press = keys;
    }
    if (mode == ARM_LTCH && press && r->latched) {       /* a new chord after letting go */
        r->n = 0;
        r->latched = 0;
    }
    if (mode == ARM_KEY)                                 /* KEY: the keys let go leave */
        for (k = 0; k < NKEYBIT; k++)
            if (((r->held & ~keys) >> k) & 1u)
                arp_remove(r, base + k);
    for (k = 0; k < NKEYBIT; k++)                        /* the keys pressed join, lowest first */
        if ((press >> k) & 1u && base + k <= 127u)
            arp_add(r, base + k);
    if (mode == ARM_LTCH && !keys && r->held)
        r->latched = 1;
    r->held = keys;
    r->base_in = (uint8_t)base;
    if (!r->n) {                                         /* nothing to play: quiet, and the next note starts at once */
        r->on = 0;
        r->pos = 0;
        r->phase = r->len = 0;
        *nbase = r->cur;
        return 0;
    }
    if (r->len)                                          /* (len 0: nothing played yet, the first note starts now) */
        r->phase += (int32_t)n;
    while (r->phase >= r->len) {                         /* a step starts */
        r->phase -= r->len;
        r->len = arp_len(t, r->pos);
        r->cur = (uint8_t)arp_pick(t, r);
        r->pos++;
        r->gate = clamp(r->len * clamp(tp[t].arp[AR_GATE], 10, 100) / 100, (int32_t)CTL, r->len - (int32_t)CTL);
        r->on = 1;
    }
    if (r->phase >= r->gate)
        r->on = 0;
    *nbase = r->cur;
    return r->on;
}
