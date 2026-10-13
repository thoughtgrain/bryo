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
 * step is RATE long at the tempo, SWNG lengthening the first of each pair of steps and shortening the second. Keys
 * held play whether the transport runs or not, like the keys. Notes it holds with no key down (latched, piled up,
 * or put back by a project, arp_seed) follow the transport: STOP quiets them, PLAY starts them from the first step,
 * a key pressed brings them back at once.
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
    uint8_t waiting;                 /* notes held with no key down, waiting for PLAY (or a key) */
    uint8_t playing;                 /* the transport as of last block */
    uint32_t held;                   /* the keys held last block */
    int32_t phase, len, gate;        /* samples into this step, its length, how long its note sounds */
    uint32_t pos;                    /* steps played since the notes started */
    uint32_t rng;                    /* RND's generator */
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

static arp_rt_t reso_arp[NTRK];      /* RESONATOR's own arpeggio, one a track (its knobs: RP_AMODE.., reso.c) */

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
static uint32_t arp_period(uint32_t ord, uint32_t len) { return ord == ARO_UPDN && len > 1u ? 2u * len - 2u : len; }

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

/* audio ISR: one block of an arpeggiator (state r, knobs a: AR_*; on: it plays). keys: the keys held (a mask over
 * base); returns the mask that plays, and in *nbase its key_base (a step's note: bit 0 at that base) */
static uint32_t HOT_CODE arp_run(arp_rt_t *r, const int16_t *a, int on, uint32_t keys, uint32_t base, uint32_t n,
                        uint32_t *nbase, uint32_t salt)
{
    uint32_t mode = (uint32_t)clamp(a[AR_MODE], 0, 3), press = keys & ~r->held, k;
    if (!r->rng)
        r->rng = 0x9E3779B9u ^ (salt * 0x85EBCA6Bu);
    if (mode != r->mode && (mode == ARM_OFF || r->mode == ARM_OFF || mode == ARM_KEY))
        r->n = 0;                                        /* (turned on, off, or back to KEY: from the keys as they are) */
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
    if (press)                                           /* (a key: the notes play now, transport or not) */
        r->waiting = 0;
    r->held = keys;
    r->base_in = (uint8_t)base;
    if (!r->n || (r->waiting && !keys)) {             /* nothing to play (or waiting for PLAY): quiet, and the next
                                                         * note starts at once */
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
        r->len = arp_len(a, r->pos);
        r->cur = (uint8_t)arp_pick(a, r);
        r->pos++;
        r->gate = clamp(r->len * clamp(a[AR_GATE], 10, 100) / 100, (int32_t)CTL, r->len - (int32_t)CTL);
        r->on = 1;
    }
    if (r->phase >= r->gate)
        r->on = 0;
    *nbase = r->cur;
    return r->on;
}

/* audio ISR: one block of track t's arpeggiator (the ARP pad's), between its keys and its source. SYNTH and POLY:
 * notes (keys a mask over base; a step plays bit 0 at *nbase). TAPE and DRUM: the keys are slices or drums (0..15),
 * a step plays that one's key, and RNGE has no octaves to spread over. Returns the mask the source plays */
static uint32_t arp_block(uint32_t t, uint32_t keys, uint32_t base, uint32_t n, uint32_t *nbase)
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
        r->mode = (uint8_t)clamp(tp[t].arp[AR_MODE], 0, 3);   /* (as the mode: arp_run's turned-on clear passes them) */
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
    out = arp_run(r, a, 1, keys & 0xFFFFu, 0u, n, &nb, t);
    return out ? 1u << (nb & 15u) : 0u;                  /* (a step: that slice's or drum's key) */
}
