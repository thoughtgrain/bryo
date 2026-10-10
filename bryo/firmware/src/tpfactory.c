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
    {DEV_SPACE, SP_VERB, 35}, {DEV_SPACE, SP_SIZE, 75}, {DEV_SPACE, SP_DLY, 20}, {DEV_SPACE, SP_TIME, 330},
    {DEV_SPACE, SP_FDBK, 35}, {DEV_SRC, TK_LVL, 3}, {TPF_S(0), 0, 48}};   /* (LFO RATE 48: a bar a cycle) */
static const tpf_dep_t TPF_DRIFT_D[] = {{0, DEV_GRAIN, GP_SPRY, 30}};

/* 2 RINGS: the tape rings strings tuned to a major chord; slot 3's SEQ steps their root (on semitones) */
static const tpf_set_t TPF_RINGS_S[] = {
    {DEV_RESO, RP_WET, 45}, {DEV_RESO, RP_PTCH, 45}, {DEV_RESO, RP_DEC, 85}, {DEV_RESO, RP_TONE, 70},
    {DEV_RESO, RP_SCAL, 1}, {DEV_RESO, RP_RES, 20}, {DEV_SPACE, SP_VERB, 30}, {DEV_SPACE, SP_SIZE, 60}};
static const tpf_dep_t TPF_RINGS_D[] = {{2, DEV_RESO, RP_PTCH, 25}};

/* 3 BASS: one saw voice, a low filter its envelope opens, a short glide, drive, an octave down */
static const tpf_set_t TPF_BASS_S[] = {
    {MA_SYN, SY_WAVE, 3}, {MA_SYN, SY_DTUN, 4}, {MA_SYN, SY_MIX, 30}, {MA_SYN, SY_CUT, 40}, {MA_SYN, SY_RES, 35},
    {MA_SYN, SY_ENV, 60}, {MA_SYN, SY_ATK, 0}, {MA_SYN, SY_DEC, 60}, {MA_SYN, SY_SUS, 30}, {MA_SYN, SY_REL, 40},
    {MA_SYN, SY_VOIC, 1}, {MA_SYN, SY_GLID, 50}, {MA_SYN, SY_DRV, 25}, {MA_SYN, SY_TUNE, -12},
    {MA_SYN, SY_LVL, -6}, {DEV_COLOR, CP_DRIV, 15}};

/* 4 PAD: detuned saws, a slow swell and a long release, the cutoff breathing over four bars, a big room */
static const tpf_set_t TPF_PAD_S[] = {
    {MA_SYN, SY_WAVE, 3}, {MA_SYN, SY_DTUN, 22}, {MA_SYN, SY_MIX, 50}, {MA_SYN, SY_NOIS, 5}, {MA_SYN, SY_CUT, 80},
    {MA_SYN, SY_RES, 15}, {MA_SYN, SY_ENV, 20}, {MA_SYN, SY_ATK, 96}, {MA_SYN, SY_DEC, 100}, {MA_SYN, SY_SUS, 80},
    {MA_SYN, SY_REL, 104}, {DEV_SPACE, SP_VERB, 55}, {DEV_SPACE, SP_SIZE, 85}, {DEV_SPACE, SP_DEC, 70},
    {DEV_SPACE, SP_DLY, 15}, {TPF_S(0), 0, 16}, {TPF_S(0), 1, 1}};   /* (LFO: four bars, a triangle) */
static const tpf_dep_t TPF_PAD_D[] = {{0, MA_SYN, SY_CUT, 15}};

/* 5 KEYS: a plucked envelope, the filter a little closed and following it, a small room */
static const tpf_set_t TPF_KEYS_S[] = {
    {MA_POL, PL_ATK, 0}, {MA_POL, PL_DEC, 72}, {MA_POL, PL_SUS, 40}, {MA_POL, PL_REL, 64}, {MA_POL, PL_CUT, 100},
    {MA_POL, PL_RES, 10}, {MA_POL, PL_ENV, 30}, {DEV_SPACE, SP_VERB, 25}, {DEV_SPACE, SP_SIZE, 50}};

/* 6 SWELL: notes that fade in and hang, through grains and a large room, the cutoff drifting over four bars */
static const tpf_set_t TPF_SWELL_S[] = {
    {MA_POL, PL_ATK, 100}, {MA_POL, PL_DEC, 100}, {MA_POL, PL_SUS, 90}, {MA_POL, PL_REL, 108}, {MA_POL, PL_CUT, 70},
    {MA_POL, PL_RES, 25}, {MA_POL, PL_ENV, 40}, {DEV_GRAIN, GP_WET, 30}, {DEV_GRAIN, GP_SIZE, 300},
    {DEV_GRAIN, GP_SPRY, 40}, {DEV_SPACE, SP_VERB, 60}, {DEV_SPACE, SP_SIZE, 90}, {DEV_SPACE, SP_DEC, 80},
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

/* main loop: factory preset f into track t (a snapshot first, for SAVE held). 0, or -1 */
static int tpf_load(uint32_t f, uint32_t t)
{
    const tpf_t *F;
    track_params_t *P;
    int16_t reel, preel, patn, len, inst, bar, mode;
    uint32_t d, k, s, i;
    if (f >= TPF_N || t >= NTRK)
        return -1;
    F = &TPF[f];
    P = &tp[t];
    reel = P->dev[DEV_SRC][TK_REEL];
    preel = P->pol[PL_REEL];
    patn = P->drm[DM_PATN], len = P->drm[DM_LEN], inst = P->drm[DM_INST], bar = P->drm[DM_BAR];
    mode = P->drm[DM_MODE];
    trk_snap(t);
    if (mod_undo_buf.trk == t)
        mod_undo_buf.valid = 0;
    for (d = 1; d < NDEV; d++)                           /* the sound from its defaults: the devices, the channel */
        for (k = 0; k < NPK; k++)
            P->dev[d][k] = pdesc_empty(&DEV_P[d][k]) ? 0 : DEV_P[d][k].def;
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
    P->dev[DEV_SRC][TK_REEL] = reel;                     /* (what stays with the track) */
    P->pol[PL_REEL] = preel;
    P->drm[DM_PATN] = patn;
    P->drm[DM_LEN] = len;
    P->drm[DM_INST] = inst;
    P->drm[DM_BAR] = bar;
    P->drm[DM_MODE] = mode;
    P->src = F->src;
    mod_rebuild(t);
    return 0;
}
