/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: the four tracks, rendered by the audio ISR one control block (CTL samples) at a time.
 *
 * Each track starts with its source (source.c): its TAPE (tape.c: the loop plays while the transport runs, the
 * white keys play its 16 slices), SYNTH or POLY (the white keys play notes) or DRUM (drum.c: its pattern; the
 * white keys play the kit), at the source's LVL. REC records onto
 * the tape either way.
 * Then GRAIN (grain.c: grains of a live buffer of the source, or of the tape, blended by WET), from which the track
 * is stereo. Then RESONATOR (reso.c: four tuned strings the track rings through), COLOR (color.c: drive, crush,
 * noise, tilt) and SPACE (space.c: the delay and the room). Then the track's channel strip, its level and its pan
 * (mixer.c), and the four tracks' sum through the master compressor (mixer.c) to the output stage (master.c).
 *
 * What REC records is each track's REC IN (param.c RIN_*, the routing view): by default (AUTO) the other three
 * tracks' mix on a TAPE track and its own source on a SYNTH or POLY track; or the others' mix, one chosen track, or
 * itself (SELF). A track is heard after its devices, level and mute, from the previous control block (0.7 ms late),
 * so tracks can record each other in a chain (T1 into T2 into T3...) or round in a loop.
 *
 * Keys: the ISR reads the debounced white keys itself (fm1_in.notes, updated by the 10 kHz scan) once per block,
 * so a slice starts within one block instead of waiting for a UI frame; they play the focused track (sys.sel)
 * while sys.keys_live. Main loop -> ISR: the main loop owns `.mute`, `.octave` and `.level` and writes whole
 * bytes; the ISR reads them once per block. The ISR owns everything in track_rt_t. */

typedef struct {                     /* ISR only */
    int32_t peak;                    /* |output| peak, decaying (the UI's meter) */
    int32_t act;                     /* the track switched on (TRACKS), Q15: 2 ms ramps; a parked track at 0 isn't
                                      * rendered at all (its heads wait where they were) */
    int32_t last[CTL];               /* the last block's output, after level and mute (what REC hears) */
    int32_t src_g[NSRC];             /* each source's share of the track, Q15: 2 ms ramps when the source changes */
    int32_t src_lv[NSRC];            /* each source's LVL as last applied, Q10 (1024: 0 dB), ramped block to block */
} track_rt_t;

static track_rt_t track_rt[NTRK];
static uint32_t chain_shed_count;

static void chain_init(void)
{
    uint32_t t;
    for (t = 0; t < NTRK; t++) {
        track[t].level = 100;
        track[t].octave = 3;
        track_rt[t].src_g[SRC_TAPE] = 32767;
        track_rt[t].src_lv[SRC_TAPE] = track_rt[t].src_lv[SRC_SYNTH] = track_rt[t].src_lv[SRC_POLY] = 1024;
        track_rt[t].src_lv[SRC_DRUM] = 1024;
        drm_now[t] = 0xFF;
        track_rt[t].act = 32767;
    }
    mod_init();
    tape_init();
}

/* one track's source into s: the chosen one, and the one before it while it fades out. The tape renders last, so
 * REC on a track whose source isn't TAPE can print that source onto it (rin: the other tracks, for a TAPE track;
 * 0: REC isn't armed) */
static void chain_source(uint32_t t, uint32_t keys, const int32_t *rin, int32_t *s, uint32_t n)
{
    track_rt_t *rt = &track_rt[t];
    uint32_t cur = (uint32_t)tp[t].src & (NSRC - 1u), e, i;   /* (& not %: src is a byte, NSRC an int, so % is a
                                                                  * signed divide on the target) */
    int32_t o[CTL];
    int first = 1;                                      /* the first source heard assigns s, the rest add (no zero
                                                         * pass; the tape, rendered last, finds s assigned when it
                                                         * prints it: the chosen source always renders before it) */
    for (e = NSRC; e-- > 0;) {
        int32_t g0 = rt->src_g[e], g1 = clamp(g0 + (e == cur ? 1 : -1) * 372 * (int32_t)n, 0, 32767);
        const int32_t *rec = cur == SRC_TAPE || tp[t].recin != RIN_AUTO ? rin : rin ? s : 0;   /* (for the tape) */
        if (e != cur && !g0) {                          /* not heard: only the tape, printing */
            if (e == SRC_TAPE && rec)
                SOURCES[e].render(t, 0, rec, o, n);
            continue;
        }
        SOURCES[e].render(t, e == cur ? keys : 0u, e == SRC_TAPE ? rec : 0, o, n);
        rt->src_g[e] = g1;
        {                                               /* the source's LVL (its last page), ramped; 0 dB: untouched */
            int32_t l0 = rt->src_lv[e], l1 = db_q10(e == SRC_TAPE ? TPD(t, DEV_SRC)[TK_LVL] : e == SRC_SYNTH ?
                                                    TPD(t, MA_SYN)[SY_LVL] : e == SRC_POLY ? TPD(t, MA_POL)[PL_LVL]
                                                    : TPD(t, MA_DRM)[DM_LVL]);
            rt->src_lv[e] = l1;
            if (l0 != 1024 || l1 != 1024)
                for (i = 0; i < n; i++)
                    o[i] = (o[i] * (l0 + (((l1 - l0) * (int32_t)i) >> CTL_LOG2))) >> 10;
        }
        if (g0 == g1) {                                 /* steady: the ramp's term is 0 on every sample */
            int32_t k = g0 >> 1;
            if (first)
                for (i = 0; i < n; i++)
                    s[i] = ((o[i] >> 2) * k) >> 12;
            else
                for (i = 0; i < n; i++)
                    s[i] += ((o[i] >> 2) * k) >> 12;
        } else if (first) {
            for (i = 0; i < n; i++)                     /* (in 32 bits: POLY's four voices sum past 16) */
                s[i] = ((o[i] >> 2) * ((g0 + (((g1 - g0) * (int32_t)i) >> CTL_LOG2)) >> 1)) >> 12;
        } else {
            for (i = 0; i < n; i++)
                s[i] += ((o[i] >> 2) * ((g0 + (((g1 - g0) * (int32_t)i) >> CTL_LOG2)) >> 1)) >> 12;
        }
        first = 0;
    }
    if (first)                                          /* (nothing heard: silence) */
        for (i = 0; i < n; i++)
            s[i] = 0;
}

/* The load by stage, for the SETUP page (ui_viz.c viz_setup): TIMER4 ticks (24 MHz) spent in each part of the chain,
 * summed over a second of blocks, then published as tenths of a percent of the time the blocks had. Each track's
 * parts add into the same stage, so SRC is all four sources. Nested interrupts (TIMER5) land in whichever stage they
 * cut into: a little more than the CPU meter, which takes them out. On the host fm1_ticks() is 0: all 0. */
enum { LD_PRE, LD_SRC, LD_GRN, LD_RES, LD_COL, LD_SPC, LD_ROOM, LD_MST, LD_N };
static uint32_t ld_acc[LD_N], ld_blocks, ld_t;
static volatile uint16_t ld_pm[LD_N];               /* ISR -> the screen: per mille of the blocks' time, last second */
static volatile uint16_t ld_ram, ld_xip;            /* main loop, at boot: a fixed loop's speed from RAM and from
                                                     * flash (main.c ld_probe), loops a microsecond x 10 */
static inline void ld_mark(uint32_t k)
{
    uint32_t t1 = fm1_ticks();
    ld_acc[k] += t1 - ld_t;
    ld_t = t1;
}

/* audio ISR: one control block of the whole instrument, interleaved stereo Q15 into out. In the USB record mode
 * (usbrec.c) the computer's sound instead: the tracks fade out over one block as it opens, and come back once the
 * monitor has faded out as it closes. */
static void chain_block(int32_t *out, uint32_t n)
{
    int32_t l[CTL], r[CTL], rin[NTRK][CTL];
    uint32_t i, t, u, white = sys.keys_live ? white_keys(fm1_in.notes) : 0u, sel = sys.sel, zero = 0, play[NTRK];
    int first;
    uint32_t ntrk = sys.ntrk >= 1u && sys.ntrk <= NTRK ? sys.ntrk : NTRK;
    if (ur.isr_in) {                                    /* the record mode: the monitor */
        usbrec_block(l, r, n);
        master_block(l, r, out, n);
        return;
    }
    ld_t = fm1_ticks();
    for (t = 0; t < NTRK; t++) {                       /* what each track's source plays: its keys (the focused
                                                        * track's), through the arpeggiator (every source) */
        uint32_t src = tp[t].src, base = 12u * ((uint32_t)track[t].octave + 1u), in, nb;
        int notes = src == SRC_SYNTH || src == SRC_POLY;
        if (notes && sys.keys_notes) {                 /* KEYS NOTE: the 27 keys as a keyboard, from the F above */
            in = t == sel && sys.keys_live ? fm1_in.notes & 0x7FFFFFFu : 0u;
            base += 5u;
        } else {
            in = t == sel ? white : 0u;
        }
        if (t == sel && ((sys.keys_reso && !notes) || (sys.keys_rarp && tp[t].dev[DEV_RESO][RP_AMODE] != ARM_OFF) ||
                         (sys.keys_grain && src == SRC_TAPE)))
            in = 0;                                    /* (the keys are RESONATOR's, RES ARP's or GRAIN's cursor on
                                                        * this page) */
        nb = base;
        play[t] = arp_block(t, in, base, n, &nb);
        key_base[t] = (uint8_t)clamp((int32_t)nb, 0, 127);
        mod_xkeys[t] = t == sel ? 0u : play[t];
    }
    {                                                  /* the modulators, and the knobs they move (mod.c) */
        const int32_t *last[NTRK] = {track_rt[0].last, track_rt[1].last, track_rt[2].last, track_rt[3].last};
        mod_tick(play[sel % NTRK], sel, last);
    }
    gr_plan();                                         /* the grains each track may sound this block (grain.c) */
    for (u = 0; u < 27u; u++)                          /* the 0 black key held, or tapped (latched): GRAIN frozen */
        if (KEY_BLACK[u] == BK_ZERO)
            zero = (((fm1_in.notes >> u) & 1u) & sys.black_short) | (sys.freeze != 0);   /* (KEYS NOTE: a note) */
    for (t = 0; t < NTRK; t++)                         /* what each track's REC hears (REC IN), last block */
        if ((sys.rec >> t) & 1u) {
            uint32_t from = tp[t].recin;
            for (i = 0; i < n; i++) {
                int32_t a = 0;
                if (from >= RIN_T1 && from < NRIN)     /* one track (its own: itself) */
                    a = track_rt[from - RIN_T1].last[i];
                else                                   /* AUTO, OTHR: the others' mix */
                    for (u = 0; u < NTRK; u++)
                        if (u != t)
                            a += track_rt[u].last[i];
                rin[t][i] = clamp(a, -32767, 32767);
            }
        }
    ld_mark(LD_PRE);
    first = 1;                                          /* the first track assigns l and r, the rest add */
    for (t = 0; t < NTRK; t++) {
        const track_ctl_t *c = &track[t];
        track_rt_t *rt = &track_rt[t];
        int32_t s[CTL], gl[CTL], gr[CTL], g = c->mute ? 0 : (int32_t)LEVEL_Q12[c->level & 127u], pk = rt->peak;
        int32_t a0 = rt->act, a1 = clamp(a0 + (t < ntrk ? 1 : -1) * 372 * (int32_t)n, 0, 32767);
        uint32_t k = play[t], gk = 0, rk = 0, ra = 0, rn = 0;
        int sleep;
        int32_t anote = -1;
        if (tp[t].dev[DEV_RESO][RP_AMODE] == ARM_OFF)
            reso_arp_on[t] = 0;                         /* (its arpeggio off: the next step on is a pluck) */
        rt->act = a1;
        if (!a0 && !a1) {                               /* parked (TRACKS): silent, not rendered */
            grain_kill(t);                              /* (its grain slots go to the tracks still on) */
            for (i = 0; i < n; i++)
                rt->last[i] = 0;
            rt->peak = pk - (pk >> 6);
            continue;
        }
        if (t == sel && white && sys.keys_grain && tp[t].src == SRC_TAPE) {   /* the GRAIN page: the keys move
                                                                               * GRAIN's cursor */
            gk = white;
            k = 0;
        }
        if (tp[t].dev[DEV_RESO][RP_AMODE] != ARM_OFF) {   /* RESONATOR's own arpeggio: the keys on its page into it,
                                                         * a pluck at each step's note (whatever the source plays) */
            ra = reso_arp_block(t, t == sel && sys.keys_rarp ? white : 0u, 12u * ((uint32_t)track[t].octave + 1u), n,
                                &rn);
            if (ra && !reso_arp_on[t])
                anote = (int32_t)rn;
            reso_arp_on[t] = (uint8_t)ra;
        }
        if (t == sel && white && sys.keys_reso) {   /* the RESONATOR page: the white keys set its root; on a
                                                         * TAPE or DRUM track they pluck it alone (no slice, no drum),
                                                         * on SYNTH or POLY they still play the source too */
            rk = white;
            if (tp[t].src == SRC_TAPE || tp[t].src == SRC_DRUM)
                k = 0;
        }
        sleep = c->mute && !((sys.rec >> t) & 1u);   /* asleep: muted and not recording. Its source keeps its time
                                                         * (tape heads, the drum pattern, keys, arpeggios; the
                                                         * modulators run on) and makes no sound, GRAIN neither
                                                         * records nor plays, RES ARP doesn't pluck: a mute is a cut,
                                                         * so at waking notes start fresh and GRAIN's buffer holds the
                                                         * bars from before until it's written over (2026-10-12: a
                                                         * muted track had rendered everything, then been zeroed: a
                                                         * third of the CPU with one track heard) */
        src_sleep = (uint8_t)sleep;
        chain_source(t, k, (sys.rec >> t) & 1u ? rin[t] : 0, s, n);
        src_sleep = 0;
        ld_mark(LD_SRC);
        if (sleep) {
            grain_sleep(t, (int)zero, n);
            anote = -1;
        } else {
            grain_block(t, s, gk, (int)zero, gl, gr, n);   /* from here the track is stereo */
        }
        if (c->mute)                                    /* muted: nothing after GRAIN is heard, so it hears silence
                                                         * too: RESONATOR, COLOR, SPACE and the channel ring down
                                                         * and rest (their work given back: a muted track cost as
                                                         * much as a playing one, 2026-10-11). Unmuted, they start
                                                         * from rest on the track's sound, as a mute is a cut */
            for (i = 0; i < n; i++)
                gl[i] = gr[i] = 0;
        ld_mark(LD_GRN);
        reso_block(t, gl, gr, rk, tp[t].src == SRC_TAPE || tp[t].src == SRC_DRUM, anote, n);
        ld_mark(LD_RES);
        color_block(t, gl, gr, n);
        ld_mark(LD_COL);
        space_block(t, gl, gr, n, g);                   /* (its delay; its send to a room, at its level) */
        if (a0 != 32767 || a1 != 32767)                  /* switching on or off: the 2 ms ramp */
            for (i = 0; i < n; i++) {
                int32_t a = a0 + (((a1 - a0) * (int32_t)i) >> CTL_LOG2);
                gl[i] = (gl[i] >> 3) * (a >> 3) >> 9;
                gr[i] = (gr[i] >> 3) * (a >> 3) >> 9;
            }
        mix_channel(t, gl, gr, n);                      /* the channel strip: LOW HIGH FILT (mixer.c) */
        if (mix_pan(t, gl, gr, g, n))                   /* the level and the pan (centred: the level below) */
            g = 4096;
        for (i = 0; i < n; i++) {
            int32_t vl = (gl[i] * g) >> 12, vr = (gr[i] * g) >> 12, a = vl < 0 ? -vl : vl, b = vr < 0 ? -vr : vr;
            rt->last[i] = sdiv2(vl + vr, 1);            /* (what the other tracks' REC hears: mono) */
            if (first) {
                l[i] = vl;
                r[i] = vr;
            } else {
                l[i] += vl;
                r[i] += vr;
            }
            if (a > pk)
                pk = a;
            if (b > pk)
                pk = b;
        }
        first = 0;
        rt->peak = pk - (pk >> 6);                    /* ~45 ms decay at one block per 0.73 ms */
        ld_mark(LD_SPC);                               /* (SPACE, the channel strip, the level and the sum) */
    }
    if (first)                                          /* (every track parked: silence) */
        for (i = 0; i < n; i++)
            l[i] = r[i] = 0;
    grain_recover();
    reso_recover();
    drm_recover();
    space_rooms(l, r, n);                               /* the rooms A and B on what the tracks sent (space.c) */
    ld_mark(LD_ROOM);
    mix_comp(l, r, n);                                  /* the master compressor on the sum (mixer.c) */
    master_block(l, r, out, n);
    ld_mark(LD_MST);
    if (++ld_blocks >= 1378u) {                         /* a second: per mille of 1378 blocks of 17,408 ticks each
                                                         * (TIMER4's 544 a frame x 32), one divide a stage a second */
        for (i = 0; i < LD_N; i++) {
            ld_pm[i] = (uint16_t)(ld_acc[i] / (1378u * 17408u / 1000u));
            ld_acc[i] = 0;
        }
        ld_blocks = 0;
    }
    if (sys.usbrec) {                                   /* the record mode opening: this block fades the tracks out */
        for (i = 0; i < n; i++) {
            int32_t g = (int32_t)(n - i) * 32767 / (int32_t)n;
            out[2u * i] = (out[2u * i] >> 1) * g >> 14;
            out[2u * i + 1u] = (out[2u * i + 1u] >> 1) * g >> 14;
        }
        ur.isr_in = 1;
    }
}

/* audio ISR, the half after two overloaded halves: take load away. GRAIN's sounding cap goes first (grain.c), down
 * to half; then RESONATOR's strings (reso.c), down to two; then DRUM's voices (drum.c), down to two a track (the
 * voices above the cap fade out in 1.5 ms); the tape is never shed. */
static void chain_shed(void)
{
    chain_shed_count++;
    if (grain_cap > 4u)                                 /* grains first, then strings, then drum tails */
        grain_shed();
    else if (reso_cap > 2u)
        reso_shed();
    else
        drm_shed();
}

/* main loop: TRACKS set to n (GLO held + SELECT). The tracks above it park: they fade out in 2 ms and stop
 * rendering (the CPU they took is free), REC on them is let go, and their tapes are the first taken when memory runs
 * short (tape.c tape_steal); nothing is erased, so raising TRACKS again brings back what's still there. */
static void chain_tracks(int32_t n)
{
    uint32_t t;
    n = clamp(n, 1, (int32_t)NTRK);
    sys.ntrk = (uint8_t)n;
    for (t = (uint32_t)n; t < NTRK; t++)
        if ((sys.rec >> t) & 1u) {
            sys.rec &= (uint8_t)~(1u << t);
            tape_unprepare(t);
        }
    if (sys.sel >= (uint32_t)n)
        sys.sel = (uint8_t)(n - 1);
}

/* main loop, every pass: the memory's bookkeeping (tape.c tape_poll: growing tapes and their ends; grain.c
 * grain_poll: GRAIN's buffers; reso.c reso_poll: the strings; space.c space_poll: the delay's and room's lines) */
static void chain_poll(void)
{
    tape_poll();
    drm_poll();
    grain_poll();
    reso_poll();
    space_poll();
    usbrec_poll();
}

/* all sound off now (an update starting, a panic): the keys stop and the transport stops (the heads fade out) */
static void chain_panic(void)
{
    sys.keys_live = 0;
    sys.playing = 0;
}
