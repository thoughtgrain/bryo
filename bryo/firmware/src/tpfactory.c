/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: the factory track presets, two a source, after the eight of your own on the PRESETS page (1..8).
 *
 * Each is a few knobs away from the defaults, so it's a short list here (in the program, not the presets' flash):
 * loading one puts the track's sound back to its defaults (the four devices, the channel strip, the slots, the
 * source's knobs) and then sets the list. What stays with the track is what stays for a preset of your own
 * (tpreset.c): the pattern and its PATN and LEN, the level, octave, REC IN, and the reel each source plays. They
 * can't be saved over or deleted; save a changed one into A..H. Each source's LVL is set so a preset's peak lands
 * within a few dB of the default sound it replaces (tests/bryo_host.c, test_tpfactory, measures it), so stepping
 * through them doesn't jump in level. */

#define TPF_N 8u
#define TPF_SLOT MOD_NARR            /* an "array" past the knob arrays: slot s's own knobs are TPF_SLOT + s */

typedef struct {
    uint8_t a, k;                    /* the array (DEV_*, MA_*, TPF_SLOT + s) and the knob */
    int16_t v;
} tpf_set_t;
typedef struct {
    uint8_t s, a, k;                 /* slot, the target's array and knob */
    int8_t d;                        /* depth, % */
} tpf_dep_t;
typedef struct {
    const char *name;
    uint8_t src;
    uint8_t eng[NSLOT];
    const tpf_set_t *set;
    uint8_t nset;
    const tpf_dep_t *dep;
    uint8_t ndep;
} tpf_t;

#define TPF_ENG_DEF {ME_WAVE, ME_ADSR, ME_SEQ, ME_WAVE}
#define TPF_S(n) (TPF_SLOT + (n))

/* 1 DRIFT: the tape through soft grains, warmed, a long tail; slot 1 sways the grains' spray over a bar */
static const tpf_set_t TPF_DRIFT_S[] = {
    {DEV_GRAIN, GP_WET, 40}, {DEV_GRAIN, GP_SIZE, 220}, {DEV_GRAIN, GP_RATE, 30}, {DEV_GRAIN, GP_SPRD, 60},
    {DEV_GRAIN, GP_SPRY, 50}, {DEV_GRAIN, GP_CONT, 80}, {DEV_COLOR, CP_TILT, -25},
    {DEV_SPACE, SP_VERB, 35}, {DEV_SPACE, SP_DLY, 20}, {DEV_SPACE, SP_TIME, 330},
    {DEV_SPACE, SP_FDBK, 35}, {DEV_SRC, TK_LVL, 3}, {TPF_S(0), 0, 48}};   /* (LFO RATE 48: a bar a cycle) */
static const tpf_dep_t TPF_DRIFT_D[] = {{0, DEV_GRAIN, GP_SPRY, 30}};

/* 2 RINGS: the tape rings strings tuned to a major chord; slot 3's SEQ steps their root (on semitones) */
static const tpf_set_t TPF_RINGS_S[] = {
    {DEV_RESO, RP_WET, 45}, {DEV_RESO, RP_PTCH, 45}, {DEV_RESO, RP_DEC, 85}, {DEV_RESO, RP_TONE, 70},
    {DEV_RESO, RP_SCAL, 1}, {DEV_RESO, RP_RES, 20}, {DEV_SPACE, SP_VERB, 30}};
static const tpf_dep_t TPF_RINGS_D[] = {{2, DEV_RESO, RP_PTCH, 25}};

/* 3 BASS: one saw voice, a low filter its envelope opens, a short glide, drive, an octave down */
static const tpf_set_t TPF_BASS_S[] = {
    {MA_SYN, SY_WAVE, 3}, {MA_SYN, SY_DTUN, 4}, {MA_SYN, SY_MIX, 30}, {MA_SYN, SY_CUT, 40}, {MA_SYN, SY_RES, 35},
    {MA_SYN, SY_ENV, 60}, {MA_SYN, SY_ATK, 0}, {MA_SYN, SY_DEC, 60}, {MA_SYN, SY_SUS, 30}, {MA_SYN, SY_REL, 40},
    {MA_SYN, SY_VOIC, 1}, {MA_SYN, SY_GLID, 50}, {MA_SYN, SY_DRV, 25}, {MA_SYN, SY_TUNE, -12},
    {MA_SYN, SY_LVL, -6}, {DEV_COLOR, CP_DRIV, 15}};

/* 4 PAD: detuned saws, a slow swell and a long release, the cutoff breathing over four bars, well into its room */
static const tpf_set_t TPF_PAD_S[] = {
    {MA_SYN, SY_WAVE, 3}, {MA_SYN, SY_DTUN, 22}, {MA_SYN, SY_MIX, 50}, {MA_SYN, SY_NOIS, 5}, {MA_SYN, SY_CUT, 80},
    {MA_SYN, SY_RES, 15}, {MA_SYN, SY_ENV, 20}, {MA_SYN, SY_ATK, 96}, {MA_SYN, SY_DEC, 100}, {MA_SYN, SY_SUS, 80},
    {MA_SYN, SY_REL, 104}, {DEV_SPACE, SP_VERB, 55}, {DEV_SPACE, SP_DLY, 15}, {MA_SYN, SY_LVL, -1},
    {TPF_S(0), 0, 16}, {TPF_S(0), 1, 1}};   /* (LFO: four bars, a triangle) */
static const tpf_dep_t TPF_PAD_D[] = {{0, MA_SYN, SY_CUT, 15}};

/* 5 KEYS: a plucked envelope, the filter a little closed and following it, a little of its room */
static const tpf_set_t TPF_KEYS_S[] = {
    {MA_POL, PL_ATK, 0}, {MA_POL, PL_DEC, 72}, {MA_POL, PL_SUS, 40}, {MA_POL, PL_REL, 64}, {MA_POL, PL_CUT, 100},
    {MA_POL, PL_RES, 10}, {MA_POL, PL_ENV, 30}, {DEV_SPACE, SP_VERB, 25}};

/* 6 SWELL: notes that fade in and hang, through grains and well into its room, the cutoff drifting over four bars */
static const tpf_set_t TPF_SWELL_S[] = {
    {MA_POL, PL_ATK, 100}, {MA_POL, PL_DEC, 100}, {MA_POL, PL_SUS, 90}, {MA_POL, PL_REL, 108}, {MA_POL, PL_CUT, 70},
    {MA_POL, PL_RES, 25}, {MA_POL, PL_ENV, 40}, {DEV_GRAIN, GP_WET, 30}, {DEV_GRAIN, GP_SIZE, 300},
    {DEV_GRAIN, GP_SPRY, 40}, {DEV_SPACE, SP_VERB, 60},
    {MA_POL, PL_LVL, 4},
    {TPF_S(0), 0, 16}, {TPF_S(0), 1, 1}};
static const tpf_dep_t TPF_SWELL_D[] = {{0, MA_POL, PL_CUT, 10}};

/* 7 TIGHT: short, bright drums with a little drive and more accent */
static const tpf_set_t TPF_TIGHT_S[] = {
    {MA_DRM, DM_TONE, 25}, {MA_DRM, DM_DECY, -40}, {MA_DRM, DM_DRV, 15}, {MA_DRM, DM_ACNT, 70},
    {DEV_COLOR, CP_DRIV, 20}, {MA_CH, CH_LOW, 2}, {MA_DRM, DM_LVL, -3}};

/* 8 DUB: longer, darker drums that vary, into a feedback delay whose feedback swells over a bar */
static const tpf_set_t TPF_DUB_S[] = {
    {MA_DRM, DM_DECY, 30}, {MA_DRM, DM_TONE, -20}, {MA_DRM, DM_SEED, 7}, {MA_DRM, DM_VARY, 50},
    {MA_DRM, DM_FILL, 40}, {DEV_SPACE, SP_DLY, 35}, {DEV_SPACE, SP_TIME, 370}, {DEV_SPACE, SP_FDBK, 55},
    {DEV_SPACE, SP_VERB, 20}, {DEV_SPACE, SP_TONE, -30}, {DEV_COLOR, CP_DRIV, 25}, {DEV_COLOR, CP_TILT, -20},
    {MA_DRM, DM_LVL, -7},
    {TPF_S(0), 0, 48}};
static const tpf_dep_t TPF_DUB_D[] = {{0, DEV_SPACE, SP_FDBK, 15}};

#define TPF_ARR(x) x, (uint8_t)(sizeof x / sizeof x[0])
static const tpf_t TPF[TPF_N] = {
    {"DRIFT", SRC_TAPE, TPF_ENG_DEF, TPF_ARR(TPF_DRIFT_S), TPF_ARR(TPF_DRIFT_D)},
    {"RINGS", SRC_TAPE, TPF_ENG_DEF, TPF_ARR(TPF_RINGS_S), TPF_ARR(TPF_RINGS_D)},
    {"BASS", SRC_SYNTH, TPF_ENG_DEF, TPF_ARR(TPF_BASS_S), 0, 0},
    {"PAD", SRC_SYNTH, TPF_ENG_DEF, TPF_ARR(TPF_PAD_S), TPF_ARR(TPF_PAD_D)},
    {"KEYS", SRC_POLY, TPF_ENG_DEF, TPF_ARR(TPF_KEYS_S), 0, 0},
    {"SWELL", SRC_POLY, TPF_ENG_DEF, TPF_ARR(TPF_SWELL_S), TPF_ARR(TPF_SWELL_D)},
    {"TIGHT", SRC_DRUM, TPF_ENG_DEF, TPF_ARR(TPF_TIGHT_S), 0, 0},
    {"DUB", SRC_DRUM, TPF_ENG_DEF, TPF_ARR(TPF_DUB_S), TPF_ARR(TPF_DUB_D)},
};

/* knob array a of track t (slot knobs included) and its descriptors (0: no such array) */
static int16_t *tpf_arr(uint32_t t, uint32_t a, const pdesc_t **d)
{
    if (a >= TPF_SLOT && a < TPF_SLOT + NSLOT) {
        *d = ME_P[tp[t].engine[a - TPF_SLOT] % NME];
        return tp[t].mod[a - TPF_SLOT];
    }
    if (a >= MOD_NARR)
        return 0;
    *d = a == DEV_SRC ? DEV_P[DEV_SRC] : a < NDEV ? DEV_P[a] : a == MA_SYN ? SYN_P : a == MA_POL ? POL_P :
         a == MA_DRM ? DRM_P : CH_P;
    return mod_base(t, a);
}

/* main loop: the sound of F into track t: its devices, channel strip, source knobs and slots from their defaults,
 * then F's knobs and depths (what the track keeps across a preset, the caller puts back) */
static void tpf_apply(uint32_t t, const tpf_t *F)
{
    track_params_t *P = &tp[t];
    uint32_t d, k, s, i;
    int16_t room = P->dev[DEV_SPACE][SP_ROOM];          /* (the room it sends to stays: the rooms are the mix's) */
    for (d = 1; d < NDEV; d++)                           /* the sound from its defaults: the devices, the channel */
        for (k = 0; k < NPK; k++)
            P->dev[d][k] = pdesc_empty(&DEV_P[d][k]) || (d == DEV_SPACE && SP_RMK[k]) ? 0 : DEV_P[d][k].def;
    P->dev[DEV_SPACE][SP_ROOM] = room;
    for (k = 0; k < NCH; k++)
        P->ch[k] = CH_P[k].def;
    for (k = 0; k < NPK; k++) {                          /* .. the preset's source (the others: as they are) */
        if (F->src == SRC_TAPE)
            P->dev[DEV_SRC][k] = pdesc_empty(&DEV_P[DEV_SRC][k]) ? 0 : DEV_P[DEV_SRC][k].def;
        else if (F->src == SRC_SYNTH)
            P->syn[k] = SYN_P[k].def;
        else if (F->src == SRC_POLY)
            P->pol[k] = pdesc_empty(&POL_P[k]) ? 0 : POL_P[k].def;
        else
            P->drm[k] = DRM_P[k].def;
    }
    if (F->src == SRC_DRUM)
        for (i = 0; i < DRM_NINST; i++)
            for (k = 0; k < NDIN; k++)
                P->dins[i][k] = 0;
    for (s = 0; s < NSLOT; s++) {                        /* .. the slots, as at power-on */
        param_engine(t, s, F->eng[s] % NME);
        if (s == 3u && F->eng[s] == ME_WAVE) {
            P->mod[s][1] = LFO_RND;
            P->mod[s][6] = 20;
        }
        for (k = 0; k < 16u; k++)
            P->steps[s][k] = SEQ_DEF[k];
    }
    mdl[t].n = 0;
    for (i = 0; i < F->nset; i++) {                      /* the preset's knobs, each held to its range */
        const pdesc_t *ds = 0;
        int16_t *v = tpf_arr(t, F->set[i].a, &ds);
        uint32_t kk = F->set[i].k;
        if (v && ds && kk < NPK && !pdesc_empty(&ds[kk]))
            v[kk] = (int16_t)clamp(F->set[i].v, ds[kk].min, ds[kk].max);
    }
    for (i = 0; i < F->ndep; i++) {                      /* its depths */
        uint32_t g = pj_target(F->dep[i].a, F->dep[i].k);
        if (F->dep[i].s < NSLOT && g < MOD_NTGT && mod_tdesc(g))
            mod_dep_set(t, F->dep[i].s, g, clamp(F->dep[i].d, -100, 100));
    }
    P->src = F->src;
}

/* main loop: factory preset f into track t (a snapshot first, for SAVE held). 0, or -1 */
static int tpf_load(uint32_t f, uint32_t t)
{
    track_params_t *P;
    int16_t reel, preel, patn, len, inst, bar, mode;
    if (f >= TPF_N || t >= NTRK)
        return -1;
    P = &tp[t];
    reel = P->dev[DEV_SRC][TK_REEL];
    preel = P->pol[PL_REEL];
    patn = P->drm[DM_PATN], len = P->drm[DM_LEN], inst = P->drm[DM_INST], bar = P->drm[DM_BAR];
    mode = P->drm[DM_MODE];
    trk_snap(t);
    if (mod_undo_buf.trk == t)
        mod_undo_buf.valid = 0;
    tpf_apply(t, &TPF[f]);
    P->dev[DEV_SRC][TK_REEL] = reel;                     /* (what stays with the track) */
    P->pol[PL_REEL] = preel;
    P->drm[DM_PATN] = patn;
    P->drm[DM_LEN] = len;
    P->drm[DM_INST] = inst;
    P->drm[DM_BAR] = bar;
    P->drm[DM_MODE] = mode;
    mod_rebuild(t);
    return 0;
}

/* The scene Bryo starts with when nothing's saved, and what FACTORY loads in the project view (project.c pj_factory):
 * four tracks that keep changing by themselves, the modulators and two arpeggiators doing the work. Everything is in
 * A minor with the reels (tools/gen_reels.py), and every pitch a modulator moves lands on a semitone (a SEQ's steps
 * on a pitch do, mod.c) or on a scale (GRAIN's PRND, held to SCAL).
 *   T1 DRUM   FUNK, VARY and FILL up, EVOL moving to a new version every two times round; M3, the random LFO, picks
 *             SEED's versions every two bars, M4's steps retune the kit
 *   T2 POLY   BELL, struck by its arpeggiator: Am7 latched, at random over two octaves, eighths; M4 moves the
 *             chord a bar at a time, Am7 Dm7 Em7 Am7 (TUNE +0 +5 +7 +0); M3 drifts the filter; a few grains, an echo
 *   T3 TAPE   DRON through strings M4 walks from A (A C D E G, a bar a step) and a shimmer of grains, each pitched
 *             at random within A minor (PRND 12, SCAL MIN); room B, the large one
 *   T4 POLY   FOLD, plucked by its arpeggiator: A minor pentatonic up and down, swung sixteenths, moving with T2's
 *             chords (its M4 the same steps); M3 drifts the filter; a delay whose feedback M1 breathes over four bars
 * The arpeggiators' notes wait for PLAY (arp.c). M4 is the SEQ (slot 2), M3 the random LFO (slot 3), M1 the LFO
 * (slot 0); depths are % of a knob's range: a pitch knob of 48 semitones at 25 % reaches an octave, and a step's
 * value v lands v x 12 / 100 semitones up (0 3 5 7 10 12 are 0 26 43 59 84 100). */
#define TPS_BELL_NOTE 57             /* BELL's first strike (tools/gen_reels.py): POLY's ROOT for it */
#define TPS_FOLD_NOTE 45             /* FOLD's first pluck */
static const tpf_set_t TPS_T1_S[] = {
    {MA_DRM, DM_SEED, 5}, {MA_DRM, DM_VARY, 70}, {MA_DRM, DM_FILL, 50}, {MA_DRM, DM_EVOL, 2}, {MA_DRM, DM_LVL, 2},
    {DEV_SPACE, SP_VERB, 15},
    {TPF_S(3), 0, 32}, {TPF_S(3), 6, 40},              /* M3: RND, two bars a step, drifting */
    {TPF_S(2), 0, 8}, {TPF_S(2), 1, 1}};               /* M4: 8 steps, half a bar each */
static const tpf_dep_t TPS_T1_D[] = {{3, MA_DRM, DM_SEED, 45}, {2, MA_DRM, DM_TUNE, 20}};
static const tpf_set_t TPS_T2_S[] = {
    {MA_POL, PL_ROOT, TPS_BELL_NOTE}, {MA_POL, PL_ATK, 0}, {MA_POL, PL_DEC, 76}, {MA_POL, PL_SUS, 20},
    {MA_POL, PL_REL, 70}, {MA_POL, PL_CUT, 110}, {MA_POL, PL_RES, 10}, {MA_POL, PL_ENV, 20},
    {DEV_GRAIN, GP_WET, 30}, {DEV_GRAIN, GP_SIZE, 200}, {DEV_GRAIN, GP_RATE, 35}, {DEV_GRAIN, GP_SPRY, 40},
    {DEV_GRAIN, GP_SPRD, 70}, {DEV_SPACE, SP_DLY, 25}, {DEV_SPACE, SP_TIME, 370}, {DEV_SPACE, SP_FDBK, 35},
    {DEV_SPACE, SP_VERB, 25},
    {TPF_S(2), 0, 4}, {TPF_S(2), 1, 0},                 /* M4: 4 steps, a bar each: the chords */
    {TPF_S(3), 0, 32}};                                /* M3: two bars a step */
static const tpf_dep_t TPS_T2_D[] = {{2, MA_POL, PL_TUNE, 25}, {3, MA_POL, PL_CUT, 20}};
static const tpf_set_t TPS_T3_S[] = {
    {DEV_RESO, RP_WET, 45}, {DEV_RESO, RP_PTCH, 45}, {DEV_RESO, RP_DEC, 85}, {DEV_RESO, RP_TONE, 65},
    {DEV_GRAIN, GP_WET, 40}, {DEV_GRAIN, GP_SIZE, 280}, {DEV_GRAIN, GP_RATE, 22}, {DEV_GRAIN, GP_SCAL, 3},
    {DEV_GRAIN, GP_PRND, 12}, {DEV_GRAIN, GP_REV, 40}, {DEV_GRAIN, GP_SPRY, 60}, {DEV_GRAIN, GP_SPRD, 80},
    {DEV_SPACE, SP_VERB, 45}, {DEV_SPACE, SP_ROOM, 1},
    {TPF_S(2), 0, 8}, {TPF_S(2), 1, 0},                 /* M4: 8 steps, a bar each */
    {TPF_S(3), 0, 32}};
static const tpf_dep_t TPS_T3_D[] = {{2, DEV_RESO, RP_PTCH, 25}, {3, DEV_GRAIN, GP_SIZE, 20}};
static const tpf_set_t TPS_T4_S[] = {
    {MA_POL, PL_ROOT, TPS_FOLD_NOTE}, {MA_POL, PL_ATK, 0}, {MA_POL, PL_DEC, 60}, {MA_POL, PL_SUS, 0},
    {MA_POL, PL_REL, 50}, {MA_POL, PL_CUT, 100}, {MA_POL, PL_RES, 20}, {MA_POL, PL_ENV, 30},
    {DEV_SPACE, SP_DLY, 30}, {DEV_SPACE, SP_TIME, 250}, {DEV_SPACE, SP_FDBK, 45}, {DEV_SPACE, SP_TONE, -20},
    {TPF_S(2), 0, 4}, {TPF_S(2), 1, 0},                 /* M4: T2's chords */
    {TPF_S(3), 0, 48},                                 /* M3: a bar a step */
    {TPF_S(0), 0, 16}, {TPF_S(0), 1, 1}};              /* M1: four bars, a triangle */
static const tpf_dep_t TPS_T4_D[] = {{2, MA_POL, PL_TUNE, 25}, {3, MA_POL, PL_CUT, 20}, {0, DEV_SPACE, SP_FDBK, 20}};
static const tpf_t TPS[NTRK] = {
    {"T1", SRC_DRUM, TPF_ENG_DEF, TPF_ARR(TPS_T1_S), TPF_ARR(TPS_T1_D)},
    {"T2", SRC_POLY, TPF_ENG_DEF, TPF_ARR(TPS_T2_S), TPF_ARR(TPS_T2_D)},
    {"T3", SRC_TAPE, TPF_ENG_DEF, TPF_ARR(TPS_T3_S), TPF_ARR(TPS_T3_D)},
    {"T4", SRC_POLY, TPF_ENG_DEF, TPF_ARR(TPS_T4_S), TPF_ARR(TPS_T4_D)},
};
/* M4's steps on each track */
static const int8_t TPS_STEPS[NTRK][16] = {
    {0, 59, 26, 84, 0, 43, 100, 26},                                         /* T1: the kit's tuning, 8 steps */
    {0, 43, 59, 0},                                                          /* T2: Am7 Dm7 Em7 Am7, a bar each */
    {0, 26, 43, 59, 84, 59, 43, 26},                                         /* T3: A C D E G E D C, a bar each */
    {0, 43, 59, 0}};                                                         /* T4: with T2 */
/* the arpeggiators: MODE ORDR RNGE RATE GATE SWNG (MODE OFF: none), and the notes they hold */
static const int8_t TPS_ARP[NTRK][6] = {
    {ARM_OFF}, {ARM_LTCH, ARO_RND, 2, 1, 70, 0}, {ARM_OFF}, {ARM_LTCH, ARO_UPDN, 1, 3, 40, 20}};
static const uint8_t TPS_ARPN[NTRK][5] = {{0}, {57, 60, 64, 67}, {0}, {45, 48, 50, 52, 55}};   /* Am7; A minor pent. */
static const uint8_t TPS_ARPC[NTRK] = {0, 4, 0, 5};

/* main loop: the scene into the four tracks (on the defaults: param_defaults first) */
static void tpf_scene(void)
{
    uint32_t t, k;
    rmp[0][RM_SIZE] = 45;                                /* the rooms: A a small one (T1's kit, T2's bells), B a */
    rmp[1][RM_SIZE] = 80;                                /* .. large, long one (T3's strings); both QUAL LO, half */
    rmp[1][RM_DEC] = 65;                                 /* .. the work (the scene at ~75 % on the FM-1, not ~78) */
    rmp[0][RM_QUAL] = RQ_LO;
    rmp[1][RM_QUAL] = RQ_LO;
    for (t = 0; t < NTRK; t++) {
        int16_t reel = tp[t].dev[DEV_SRC][TK_REEL], preel = tp[t].pol[PL_REEL];
        tpf_apply(t, &TPS[t]);
        if (TPS[t].src == SRC_DRUM) {                    /* T1's rhythm: FUNK, its length and swing */
            drm_load(t, 2);
            tp[t].drm[DM_PATN] = 2;
        }
        tp[t].dev[DEV_SRC][TK_REEL] = reel;              /* (each track its reel, as at power-on: POLY's too) */
        tp[t].pol[PL_REEL] = preel;
        for (k = 0; k < 16u; k++)
            tp[t].steps[SLOT_SEQ][k] = TPS_STEPS[t][k];
        for (k = 0; k < 6u; k++)                         /* the arpeggiator, and its notes (waiting for PLAY) */
            tp[t].arp[k] = TPS_ARP[t][0] == ARM_OFF ? ARP_P[k].def : TPS_ARP[t][k];
        if (TPS_ARPC[t])
            arp_seed(t, TPS_ARPN[t], TPS_ARPC[t]);
        else
            arp_clear(t);
        mod_rebuild(t);
    }
}
