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
    {MA_DRM, DM_LVL, -10},                               /* (-7 until the bass drum got its body, 2026-10-14) */
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

/* The scene Bryo starts with when nothing's saved, and what FACTORY loads in the project view (project.c pj_factory),
 * 2026-10-14: one groove in A minor and three scenes of it, AMBIENT, TECHNO and GLITCH (scene 4 left empty for you),
 * so morphing on the mixer's SCENES page takes it from one style to the next. What the scenes share (they don't hold
 * it, scene.c): the notes and rhythms (the arpeggiators' latched notes, M4's chord steps, T1's pattern), the reels,
 * and the modulation: M1 on T2 T3 T4 is a FOLLOW of T1 with a depth below 0 on the source's level, so they duck on
 * T1's kick wherever it's heard (silent in AMBIENT: nothing ducks there). What each scene sets, every track's sound
 * from its defaults:
 *   T1 DRUM   a 4/4 kick, claps, open hats on the off-beats, closed sixteenths (one bar). AMBIENT: level 0; TECHNO:
 *             tight and driven; GLITCH: varied, filled, crushed, stuttered by grains, a sixteenth echo
 *   T2 SYNTH  its arpeggiator, Am7 up in sixteenths, the chord a bar at a time (M4: Am Am Dm Em). AMBIENT: VOX, each
 *             note a MIN7 chord with a slow attack, so the steps smear into a choir; TECHNO: a saw bass an octave down
 *             with a sub, its filter snapping; GLITCH: PD's resonant saw, crushed, into a sixteenth echo
 *   T3 TAPE   DRON. AMBIENT: a grain wash with strings, the large room; TECHNO: high-passed, pumping under the kick;
 *             GLITCH: stuttering grains through the 12-band bank
 *   T4 POLY   BELL, its arpeggiator A minor pentatonic up and down, swung. AMBIENT: long bells into a dotted echo;
 *             TECHNO: short stabs; GLITCH: TWIN RND, each note BELL or BEAT at random, chopped
 * The arpeggiators' notes wait for PLAY (arp.c). M4 is the SEQ (slot 2), M3 the random LFO (slot 3), M1 the FOLLOW
 * (slot 0) */
#define TPS_BELL_NOTE 57             /* BELL's first strike (tools/gen_reels.py): POLY's ROOT for it */
#define TPS_FOL {ME_FOLLOW, ME_ADSR, ME_SEQ, ME_WAVE}
#define TPS_DUCK {TPF_S(0), 0, 1}, {TPF_S(0), 2, 1}, {TPF_S(0), 3, 12}   /* M1: FOLLOW T1, a quick rise, a fall back
                                                                          * before the next kick */
/* the base: the slots, the depths, what plays */
static const tpf_set_t TPS_T1_S[] = {{TPF_S(3), 0, 32}};
static const tpf_set_t TPS_T2_S[] = {TPS_DUCK, {TPF_S(2), 0, 4}, {TPF_S(2), 1, 0}, {TPF_S(3), 0, 32}};
static const tpf_dep_t TPS_T2_D[] = {{0, MA_SYN, SY_LVL, -30}, {2, MA_SYN, SY_TUNE, 25}, {3, MA_SYN, SY_CUT, 10}};
static const tpf_set_t TPS_T3_S[] = {TPS_DUCK, {TPF_S(2), 0, 8}, {TPF_S(2), 1, 0}, {TPF_S(3), 0, 32}};
static const tpf_dep_t TPS_T3_D[] = {{0, DEV_SRC, TK_LVL, -40}, {2, DEV_RESO, RP_PTCH, 25}, {3, DEV_GRAIN, GP_SIZE, 15}};
static const tpf_set_t TPS_T4_S[] = {TPS_DUCK, {MA_POL, PL_ROOT, TPS_BELL_NOTE}, {MA_POL, PL_RLB, 1},
                                     {TPF_S(2), 0, 4}, {TPF_S(2), 1, 0}, {TPF_S(3), 0, 48}};
static const tpf_dep_t TPS_T4_D[] = {{0, MA_POL, PL_LVL, -25}, {2, MA_POL, PL_TUNE, 25}, {3, MA_POL, PL_CUT, 15}};
static const tpf_t TPS[NTRK] = {
    {"T1", SRC_DRUM, TPF_ENG_DEF, TPF_ARR(TPS_T1_S), 0, 0},
    {"T2", SRC_SYNTH, TPS_FOL, TPF_ARR(TPS_T2_S), TPF_ARR(TPS_T2_D)},
    {"T3", SRC_TAPE, TPS_FOL, TPF_ARR(TPS_T3_S), TPF_ARR(TPS_T3_D)},
    {"T4", SRC_POLY, TPS_FOL, TPF_ARR(TPS_T4_S), TPF_ARR(TPS_T4_D)},
};
/* M4's steps on each track */
static const int8_t TPS_STEPS[NTRK][16] = {
    {0},
    {0, 0, 43, 59},                                                          /* T2: Am Am Dm Em, a bar each */
    {0, 26, 43, 59, 84, 59, 43, 26},                                         /* T3: the strings' walk, A C D E G E D C */
    {0, 0, 43, 59}};                                                         /* T4: with T2 */
/* the arpeggiators: MODE ORDR RNGE RATE GATE SWNG GRND (MODE OFF: none), and the notes they hold */
static const int8_t TPS_ARP[NTRK][7] = {
    {ARM_OFF}, {ARM_LTCH, ARO_UP, 1, 3, 45, 0, 30}, {ARM_OFF}, {ARM_LTCH, ARO_UPDN, 1, 3, 40, 20, 20}};
static const uint8_t TPS_ARPN[NTRK][5] = {{0}, {45, 48, 52, 55}, {0}, {57, 60, 62, 64, 67}};   /* Am7 low; A pent. */
static const uint8_t TPS_ARPC[NTRK] = {0, 4, 0, 5};

/* a scene's knobs: track, array, knob, value (the arrays as tpf_set_t's, TPF_S slots aside) */
typedef struct {
    uint8_t t, a, k;
    int16_t v;
} tps_k_t;
#define TPS_NSC 3u
static const tps_k_t TPS_AMB[] = {
    {1, MA_SYN, SY_WAVE, SY_VOX}, {1, MA_SYN, SY_DTUN, 80}, {1, MA_SYN, SY_MIX, 45}, {1, MA_SYN, SY_IENV, 30},
    {1, MA_SYN, SY_NOIS, 20}, {1, MA_SYN, SY_ATK, 95}, {1, MA_SYN, SY_DEC, 100}, {1, MA_SYN, SY_SUS, 85},
    {1, MA_SYN, SY_REL, 110}, {1, MA_SYN, SY_CUT, 100}, {1, MA_SYN, SY_RES, 10}, {1, MA_SYN, SY_ENV, 0},
    {1, MA_SYN, SY_CHRD, 7}, {1, MA_SYN, SY_TUNE, 12}, {1, MA_SYN, SY_LVL, 4},
    {1, DEV_SPACE, SP_VERB, 60}, {1, DEV_SPACE, SP_ROOM, 1},   /* (everything in room B: room A rests) */
    {2, DEV_GRAIN, GP_WET, 60}, {2, DEV_GRAIN, GP_SIZE, 200}, {2, DEV_GRAIN, GP_RATE, 12}, {2, DEV_GRAIN, GP_SPRY, 70},
    {2, DEV_GRAIN, GP_SPRD, 80}, {2, DEV_GRAIN, GP_SCAL, 3}, {2, DEV_GRAIN, GP_PRND, 12}, {2, DEV_GRAIN, GP_REV, 40},
    {2, DEV_RESO, RP_WET, 45}, {2, DEV_RESO, RP_PTCH, 45}, {2, DEV_RESO, RP_DEC, 90}, {2, DEV_RESO, RP_TONE, 55},
    {2, DEV_RESO, RP_SCAL, 2}, {2, DEV_RESO, RP_MODE, RM_RESO}, {2, DEV_SPACE, SP_VERB, 70}, {2, DEV_SPACE, SP_ROOM, 1},
    {3, MA_POL, PL_ATK, 5}, {3, MA_POL, PL_DEC, 100}, {3, MA_POL, PL_SUS, 30},
    {3, MA_POL, PL_CUT, 90}, {3, MA_POL, PL_ENV, 15},
    {3, MA_POL, PL_REL, 120}, {3, DEV_SPACE, SP_VERB, 60}, {3, DEV_SPACE, SP_ROOM, 1}};   /* (no echo: the room) */
static const tps_k_t TPS_TEC[] = {
    {0, MA_DRM, DM_DECY, -30}, {0, MA_DRM, DM_TONE, 20}, {0, MA_DRM, DM_DRV, 10}, {0, MA_DRM, DM_ACNT, 70},
    {0, MA_DRM, DM_TRAN, 40}, {0, MA_DRM, DM_LVL, -3}, {0, MA_CH, CH_LOW, 3}, {0, DEV_SPACE, SP_VERB, 10},
    {1, MA_SYN, SY_WAVE, 3}, {1, MA_SYN, SY_INTV, -12}, {1, MA_SYN, SY_DTUN, 6}, {1, MA_SYN, SY_MIX, 40},
    {1, MA_SYN, SY_CUT, 45}, {1, MA_SYN, SY_RES, 50}, {1, MA_SYN, SY_ENV, 60}, {1, MA_SYN, SY_KTRK, 40},
    {1, MA_SYN, SY_ATK, 0}, {1, MA_SYN, SY_DEC, 55}, {1, MA_SYN, SY_SUS, 20}, {1, MA_SYN, SY_REL, 25},
    {1, MA_SYN, SY_DRV, 35}, {1, MA_SYN, SY_TUNE, -12}, {1, MA_SYN, SY_LVL, 6},
    {2, MA_CH, CH_FILT, 25}, {2, DEV_SRC, TK_LVL, 6}, {2, DEV_GRAIN, GP_WET, 25}, {2, DEV_GRAIN, GP_SIZE, 120}, {2, DEV_GRAIN, GP_RATE, 60},
    {2, DEV_RESO, RP_WET, 25}, {2, DEV_RESO, RP_MODE, RM_COMB}, {2, DEV_RESO, RP_DEC, 50},
    {2, DEV_SPACE, SP_VERB, 35}, {2, DEV_SPACE, SP_DLY, 25}, {2, DEV_SPACE, SP_SYNC, 1}, {2, DEV_SPACE, SP_TIME, 191},
    {3, MA_POL, PL_ATK, 0}, {3, MA_POL, PL_DEC, 70}, {3, MA_POL, PL_SUS, 10}, {3, MA_POL, PL_REL, 30},
    {3, MA_POL, PL_CUT, 85}, {3, MA_POL, PL_RES, 35}, {3, MA_POL, PL_ENV, 45}, {3, MA_POL, PL_LVL, 4},
    {3, DEV_SPACE, SP_DLY, 35}, {3, DEV_SPACE, SP_SYNC, 1}, {3, DEV_SPACE, SP_TIME, 251}, {3, DEV_SPACE, SP_FDBK, 40},
    {3, DEV_SPACE, SP_VERB, 15}};
static const tps_k_t TPS_GLI[] = {
    {0, MA_DRM, DM_SEED, 9}, {0, MA_DRM, DM_VARY, 85}, {0, MA_DRM, DM_FILL, 70}, {0, MA_DRM, DM_EVOL, 1},
    {0, MA_DRM, DM_DECY, -50}, {0, MA_DRM, DM_TONE, 40}, {0, MA_DRM, DM_TRAN, 60}, {0, MA_DRM, DM_LVL, -8},
    {0, DEV_COLOR, CP_CRSH, 45}, {0, DEV_COLOR, CP_CMOD, CMOD_RATE}, {0, DEV_COLOR, CP_DRIV, 30},
    {0, DEV_GRAIN, GP_WET, 45}, {0, DEV_GRAIN, GP_SIZE, 30}, {0, DEV_GRAIN, GP_RATE, 85}, {0, DEV_GRAIN, GP_PATN, 3},
    {0, DEV_GRAIN, GP_REV, 30}, {0, DEV_GRAIN, GP_SYNC, 1},
    {0, DEV_SPACE, SP_DLY, 30}, {0, DEV_SPACE, SP_SYNC, 1}, {0, DEV_SPACE, SP_TIME, 101}, {0, DEV_SPACE, SP_FDBK, 55},
    {1, MA_SYN, SY_WAVE, SY_PD}, {1, MA_SYN, SY_DTUN, 70}, {1, MA_SYN, SY_MIX, 30}, {1, MA_SYN, SY_IENV, 70},
    {1, MA_SYN, SY_ATK, 0}, {1, MA_SYN, SY_DEC, 35}, {1, MA_SYN, SY_SUS, 10}, {1, MA_SYN, SY_REL, 20},
    {1, MA_SYN, SY_CUT, 90}, {1, MA_SYN, SY_RES, 30}, {1, MA_SYN, SY_TUNE, -12}, {1, MA_SYN, SY_DRV, 25},
    {1, MA_SYN, SY_LVL, 6},
    {1, DEV_COLOR, CP_CRSH, 30}, {1, DEV_SPACE, SP_DLY, 35}, {1, DEV_SPACE, SP_SYNC, 1}, {1, DEV_SPACE, SP_TIME, 101},
    {1, DEV_SPACE, SP_FDBK, 50},
    {2, DEV_GRAIN, GP_WET, 75}, {2, DEV_GRAIN, GP_SIZE, 40}, {2, DEV_GRAIN, GP_RATE, 90}, {2, DEV_GRAIN, GP_PATN, 3},
    {2, DEV_GRAIN, GP_REV, 50}, {2, DEV_GRAIN, GP_SPRY, 90}, {2, DEV_GRAIN, GP_SCAL, 3}, {2, DEV_GRAIN, GP_PRND, 12},
    {2, DEV_RESO, RP_WET, 40}, {2, DEV_RESO, RP_MODE, RM_BANK}, {2, DEV_RESO, RP_RIPL, 60}, {2, DEV_RESO, RP_DEC, 60},
    {2, DEV_COLOR, CP_CRSH, 25}, {2, MA_CH, CH_FILT, 20}, {2, DEV_SRC, TK_LVL, 6},
    {3, MA_POL, PL_TWIN, TW_RND}, {3, MA_POL, PL_BLND, 50}, {3, MA_POL, PL_ATK, 0}, {3, MA_POL, PL_DEC, 40},
    {3, MA_POL, PL_SUS, 0}, {3, MA_POL, PL_REL, 20}, {3, MA_POL, PL_CUT, 110}, {3, MA_POL, PL_LVL, 4},
    {3, DEV_COLOR, CP_CRSH, 40}, {3, DEV_COLOR, CP_CMOD, CMOD_BOTH},
    {3, DEV_SPACE, SP_DLY, 30}, {3, DEV_SPACE, SP_SYNC, 1}, {3, DEV_SPACE, SP_TIME, 101}, {3, DEV_SPACE, SP_FDBK, 30}};
static const struct {
    const tps_k_t *k;
    uint8_t n;
    uint8_t lvl[NTRK];
    int8_t room[NROOM][4];           /* each room's SIZE DEC TYPE MOD */
} TPS_SC[TPS_NSC] = {
    {TPS_AMB, (uint8_t)(sizeof TPS_AMB / sizeof TPS_AMB[0]), {0, 102, 108, 88}, {{50, 40, 0, 0}, {90, 80, 2, 50}}},
    {TPS_TEC, (uint8_t)(sizeof TPS_TEC / sizeof TPS_TEC[0]), {100, 106, 100, 102}, {{35, 30, 1, 0}, {60, 45, 0, 0}}},
    {TPS_GLI, (uint8_t)(sizeof TPS_GLI / sizeof TPS_GLI[0]), {100, 116, 114, 118}, {{25, 20, 1, 0}, {50, 40, 3, 70}}}};

/* main loop: scene n's sound into the tracks: every track's devices, channel and source knobs from their defaults
 * (what plays aside: the reels, POLY's ROOT and RLB, DRUM's pattern knobs), then the scene's list, the levels, the
 * rooms */
static void tps_sound(uint32_t n)
{
    uint32_t t, d, k, i;
    for (t = 0; t < NTRK; t++) {
        track_params_t *P = &tp[t];
        for (d = 1; d < NDEV; d++)
            for (k = 0; k < NPK; k++)
                P->dev[d][k] = pdesc_empty(&DEV_P[d][k]) || (d == DEV_SPACE && SP_RMK[k]) ? 0 : DEV_P[d][k].def;
        for (k = 0; k < NCH; k++)
            P->ch[k] = CH_P[k].def;
        for (k = 0; k < NPK; k++) {
            if (P->src == SRC_SYNTH)
                P->syn[k] = pdesc_empty(&SYN_P[k]) ? 0 : SYN_P[k].def;
            else if (P->src == SRC_POLY && k != PL_REEL && k != PL_ROOT && k != PL_RLB)
                P->pol[k] = pdesc_empty(&POL_P[k]) ? 0 : POL_P[k].def;
            else if (P->src == SRC_DRUM && k != DM_PATN && k != DM_LEN && k != DM_INST && k != DM_BAR && k != DM_MODE)
                P->drm[k] = pdesc_empty(&DRM_P[k]) ? 0 : DRM_P[k].def;
            else if (P->src == SRC_TAPE && k != TK_REEL)
                P->dev[DEV_SRC][k] = pdesc_empty(&DEV_P[DEV_SRC][k]) ? 0 : DEV_P[DEV_SRC][k].def;
        }
        track[t].level = TPS_SC[n].lvl[t];
    }
    for (i = 0; i < TPS_SC[n].n; i++) {
        const tps_k_t *e = &TPS_SC[n].k[i];
        const pdesc_t *ds = 0;
        int16_t *v = tpf_arr(e->t, e->a, &ds);
        if (v && ds && e->k < NPK && !pdesc_empty(&ds[e->k]))
            v[e->k] = (int16_t)clamp(e->v, ds[e->k].min, ds[e->k].max);
    }
    for (i = 0; i < NROOM; i++) {
        rmp[i][RM_SIZE] = TPS_SC[n].room[i][0];
        rmp[i][RM_DEC] = TPS_SC[n].room[i][1];
        rmp[i][RM_TYPE] = TPS_SC[n].room[i][2];
        rmp[i][RM_MOD] = TPS_SC[n].room[i][3];
        rmp[i][RM_QUAL] = RQ_LO;                         /* (the rooms at half the work: the scenes' load) */
    }
}

/* T1's bar: a 4/4 kick (accented), claps on two and four, open hats on the off-beats, closed sixteenths between */
static void tps_beat(uint32_t t)
{
    uint32_t b, s;
    for (b = 0; b < DRM_NBAR; b++) {
        tp[t].dacc[b] = 0;
        for (s = 0; s < 16u; s++)
            tp[t].dpat[b][s] = 0;
    }
    for (s = 0; s < 16u; s++) {
        uint16_t w = 0;
        if (!(s & 3u))
            w |= 1u << DI_BD;
        if (s == 4u || s == 12u)
            w |= 1u << DI_CP;
        if ((s & 3u) == 2u)
            w |= 1u << DI_OH;
        else if (s & 1u)
            w |= 1u << DI_HH;
        tp[t].dpat[0][s] = w;
    }
    tp[t].dacc[0] = 0x1111u;
    tp[t].drm[DM_LEN] = 1;
}

/* main loop: TEST (the project view, past FACTORY), after the scene: GLITCH, the scene with every stage of the chain
 * at work (grains on two tracks, the 12-band bank, three echoes, COLOR's crush), to read the load on (SETUP's lines)
 * and compare one build with the next */
static void tpf_test(void);

/* main loop: the scene into the four tracks (on the defaults: param_defaults first): the base, then the three
 * scenes stored, then AMBIENT, scene 1, as it plays */
static void tpf_scene(void)
{
    uint32_t t, k, n;
    keyp[KY_KEY] = 9;                                    /* A MIN: the scene's key (the default too, but the scene */
    keyp[KY_SCAL] = 2;                                   /* .. says so itself), so a key played over it fits */
    sys.bpm = 124;
    for (t = 0; t < NTRK; t++) {
        int16_t reel = tp[t].dev[DEV_SRC][TK_REEL];
        tpf_apply(t, &TPS[t]);
        tp[t].dev[DEV_SRC][TK_REEL] = reel;              /* (T3 its reel, DRON, as at power-on) */
        if (t == 3u)
            tp[t].pol[PL_REEL] = 2;                      /* (T4 POLY on BELL; its TWIN's B, BEAT) */
        if (TPS[t].src == SRC_DRUM)
            tps_beat(t);
        for (k = 0; k < 16u; k++)
            tp[t].steps[SLOT_SEQ][k] = TPS_STEPS[t][k];
        for (k = 0; k < 7u; k++)                         /* the arpeggiator, and its notes (waiting for PLAY) */
            tp[t].arp[k] = TPS_ARP[t][0] == ARM_OFF ? ARP_P[k].def : TPS_ARP[t][k];
        if (TPS_ARPC[t])
            arp_seed(t, TPS_ARPN[t], TPS_ARPC[t]);
        else
            arp_clear(t);
        mod_rebuild(t);
    }
    for (n = TPS_NSC; n-- > 0u;) {                      /* (GLITCH, TECHNO, AMBIENT: the last one stored plays) */
        tps_sound(n);
        scn_store(n);
    }
}

static void tpf_test(void)
{
    scn_take(&scn_from);                                 /* GLITCH at once (no glide) */
    scn_mix(&scn_from, &scn[2], SCN_FULL);
    scn_cur = 3;
}
