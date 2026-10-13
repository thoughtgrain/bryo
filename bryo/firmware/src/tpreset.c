/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: track presets, and the track snapshot that SAVE held puts back.
 *
 * A track preset is one track's sound: its source and that source's knobs (DRUM's: with each instrument's own), the
 * four devices, the channel strip, the four modulator slots (engines, knobs, a SEQ's steps) and its depths. The
 * sources it isn't on are left out (the track it's loaded into keeps its own values there), which roughly halves a
 * preset. Not the music or the mix: the DRUM pattern (and the PATTERN page's PATN and LEN, which belong to it), the
 * level, mute, octave, REC IN and the room SPACE sends to (A or B: the rooms belong to the mix) stay with the track
 * it's loaded into. A tape can't fit, so a preset names the reel the track played; one that played its own tape
 * names nothing, and the track it's loaded into keeps the reel it's on.
 *
 * Where: the eight presets (A..H) are one bank, a byte stream in the project format (project.c's packed writer and
 * reader, its commit record and A/B copies: a save writes the other copy and commits it last, so a power cut leaves
 * the previous bank in charge). The bank is the projects' seventh sector pair, the only two sectors left free. A save
 * or a delete copies the other presets forward from the copy in charge, a record at a time and unparsed (each is
 * stored behind its unpacked length), so no RAM holds the bank.
 *
 * The stream: 'B' 'K', the version, the count, then each preset as its length (0: empty) and its record. A record
 * starts with what the view shows (source, the four engines, the depth count), then the arrays as projects store
 * them (each its count and values). */

#define TPR_N 8u                     /* presets A..H */
#define TPR_BANK PJ_N                /* the bank's sector pair (pj_sector) */
#define TPR_VERSION 4u               /* one format: a bank of another version reads as empty (nothing older is kept
                                      * up with: presets started with this one) */

typedef struct {                     /* what the view shows of a preset without loading it */
    uint8_t used, src, ndep;
    uint8_t eng[NSLOT];
} tpr_sum_t;
static tpr_sum_t tpr_sum[TPR_N];

/* ------------------------------------------------------------------------------------ records --- */
/* track t's record into the writer (pw.dry: counted, not written) */
static void tpr_write_rec(uint32_t t)
{
    const track_params_t *P = &tp[t];
    uint32_t s, i;
    pw_byte(P->src);
    for (s = 0; s < NSLOT; s++)
        pw_byte(P->engine[s]);
    pw_byte(mdl[t].n);
    pw_u16(NDEV);                                        /* (the sources the track isn't on: counted 0, so the
                                                          * track it's loaded into keeps its own there) */
    for (s = 0; s < NDEV; s++)
        pw_a16(pj_dev_out(P, s), s != DEV_SRC || P->src == SRC_TAPE ? pdesc_len(DEV_P[s]) : 0u);
    pw_u16(NSLOT);
    for (s = 0; s < NSLOT; s++) {
        pw_byte(P->engine[s]);
        pw_a16(P->mod[s], pdesc_len(ME_P[P->engine[s] % NME]));
        pw_a8(P->steps[s], P->engine[s] == ME_SEQ ? 16u : 0u);
    }
    pw_a16(P->ch, NCH);
    pw_a16(P->syn, P->src == SRC_SYNTH ? pdesc_len(SYN_P) : 0u);
    pw_a16(P->pol, P->src == SRC_POLY ? pdesc_len(POL_P) : 0u);
    pw_a16(P->drm, P->src == SRC_DRUM ? pdesc_len(DRM_P) : 0u);
    pw_u16(P->src == SRC_DRUM ? DRM_NINST : 0u);
    for (s = 0; P->src == SRC_DRUM && s < DRM_NINST; s++)
        pw_a16(P->dins[s], NDIN);
    pw_u16(mdl[t].n);                                    /* the depths: slot, array, knob, depth */
    for (i = 0; i < mdl[t].n; i++) {
        uint32_t k, a = mod_tarr(mdl[t].e[i].g, &k);
        pw_byte(mdl[t].e[i].s);
        pw_byte((uint8_t)a);
        pw_byte((uint8_t)k);
        pw_byte((uint8_t)mdl[t].e[i].d);
    }
}

/* the record under the reader into track t (the music, the mix and an own-tape reel kept: see the top) */
static void tpr_read_rec(uint32_t t)
{
    track_params_t *P = &tp[t];
    uint32_t s, n, i;
    int16_t reel = P->dev[DEV_SRC][TK_REEL], preel = P->pol[PL_REEL], patn = P->drm[DM_PATN], len = P->drm[DM_LEN];
    int16_t inst = P->drm[DM_INST], bar = P->drm[DM_BAR], mode = P->drm[DM_MODE], room = P->dev[DEV_SPACE][SP_ROOM];
    uint8_t src = pr_byte();
    for (s = 0; s < NSLOT + 1u; s++)                     /* (the summary: the engines and the depth count) */
        (void)pr_byte();
    n = pr_u16();
    for (s = 0; s < n && !pr.bad; s++) {
        int16_t junk[NPK];
        if (s < NDEV)
            pr_a16(P->dev[s], NPK, DEV_P[s]);
        else
            pr_a16(junk, 0, 0);
    }
    n = pr_u16();
    for (s = 0; s < n && !pr.bad; s++) {
        int16_t junk[NPK];
        int8_t junk8[16];
        uint8_t e = pr_byte();
        if (s < NSLOT) {
            P->engine[s] = (uint8_t)(e < NME ? e : ME_WAVE);
            pr_a16(P->mod[s], NPK, ME_P[P->engine[s]]);
            pr_a8(P->steps[s], 16, 0, 100);
        } else {
            pr_a16(junk, 0, 0);
            pr_a8(junk8, 0, 0, 0);
        }
    }
    pr_a16(P->ch, NCH, CH_P);
    pr_a16(P->syn, NPK, SYN_P);
    pr_a16(P->pol, NPK, POL_P);
    pr_a16(P->drm, NPK, DRM_P);
    n = pr_u16();
    for (s = 0; s < n && !pr.bad; s++) {
        int16_t junk[NDIN];
        pr_a16(s < DRM_NINST ? P->dins[s] : junk, s < DRM_NINST ? NDIN : 0u, DRI_P);
    }
    mdl[t].n = 0;
    n = pr_u16();
    for (i = 0; i < n && !pr.bad; i++) {
        uint32_t ss = pr_byte(), a = pr_byte(), k = pr_byte(), g = pj_target(a, k);
        int32_t dp = (int8_t)pr_byte();
        if (ss < NSLOT && g < MOD_NTGT && mod_tdesc(g))
            mod_dep_set(t, ss, g, clamp(dp, -100, 100));
    }
    if (P->dev[DEV_SRC][TK_REEL] == 0)                   /* (its own tape: nothing named, the reel it's on stays) */
        P->dev[DEV_SRC][TK_REEL] = reel;
    if (P->pol[PL_REEL] == 0)
        P->pol[PL_REEL] = preel;
    P->drm[DM_PATN] = patn;                              /* (the pattern's own: they stay with the pattern) */
    P->drm[DM_LEN] = len;
    P->drm[DM_INST] = inst;
    P->drm[DM_BAR] = bar;
    P->drm[DM_MODE] = mode;
    P->dev[DEV_SPACE][SP_ROOM] = room;                   /* (the room it sends to is the mix's) */
    P->src = (uint8_t)(src < NSRC ? src : SRC_TAPE);     /* (last: the new source's knobs are in place first) */
    mod_rebuild(t);
}

/* ------------------------------------------------------------------------------------- the bank --- */
/* the reader on the bank's copy in charge, past its header: the count, or -1 (none, or not one this Bryo reads) */
static int tpr_open(void)
{
    pj_hdr_t h;
    int c = pj_current(TPR_BANK, &h);
    uint32_t n;
    if (c < 0)
        return -1;
    pr.src = rf_ptr(pj_sector(TPR_BANK, (uint32_t)c) + PJ_PAYLOAD);
    pr.len = h.len;
    pr.at = 0;
    pr.lit = pr.run = 0;
    pr.bad = 0;
    if (pr_byte() != 'B' || pr_byte() != 'K' || pr_byte() != TPR_VERSION)
        return -1;
    n = pr_byte();
    return pr.bad ? -1 : (int)n;
}

/* the view's summaries, from the copy in charge */
static void tpr_scan(void)
{
    int n = tpr_open();
    uint32_t i, k;
    memset(tpr_sum, 0, sizeof tpr_sum);
    for (i = 0; n > 0 && i < (uint32_t)n && !pr.bad; i++) {
        uint32_t len = pr_u16();
        if (len < NSLOT + 2u) {                          /* (empty) */
            for (k = 0; k < len; k++)
                (void)pr_byte();
            continue;
        }
        if (i < TPR_N) {
            tpr_sum[i].used = 1;
            tpr_sum[i].src = (uint8_t)(pr_byte() % NSRC);
            for (k = 0; k < NSLOT; k++)
                tpr_sum[i].eng[k] = (uint8_t)(pr_byte() % NME);
            tpr_sum[i].ndep = pr_byte();
            len -= NSLOT + 2u;
        }
        for (k = 0; k < len; k++)
            (void)pr_byte();
    }
    if (pr.bad)
        memset(tpr_sum, 0, sizeof tpr_sum);
}

/* the bank rewritten into the other copy with preset p from track t (t >= NTRK: p emptied), the rest copied
 * forward. 0; -1 the flash refused; -2 the bank would be over a sector (nothing changed either way) */
static int tpr_write(uint32_t p, uint32_t t)
{
    pj_hdr_t h, cur;
    int c = pj_current(TPR_BANK, &cur), n = tpr_open();
    uint32_t i, k, copy = c == 0 ? 1u : 0u, len = 0;
    if (p >= TPR_N)
        return -1;
    if (t < NTRK) {                                      /* (its length first: a dry run) */
        memset(&pw, 0, sizeof pw);
        pw.dry = 1;
        tpr_write_rec(t);
        len = pw.raw;
    }
    memset(&pw, 0, sizeof pw);
    pw.base = pj_sector(TPR_BANK, copy);
    pw.crc = 0xFFFFFFFFu;
    if (rf_erase(pw.base))
        return -1;
    pw_byte('B');
    pw_byte('K');
    pw_byte(TPR_VERSION);
    pw_byte(TPR_N);
    for (i = 0; i < TPR_N; i++) {
        uint32_t old = n >= 0 && i < (uint32_t)n && !pr.bad ? pr_u16() : 0u;
        if (i == p) {
            for (k = 0; k < old; k++)                    /* (the one it replaces: passed over) */
                (void)pr_byte();
            pw_u16(len);
            if (len)
                tpr_write_rec(t);
        } else {
            pw_u16(old);
            for (k = 0; k < old; k++)
                pw_byte(pr_byte());
        }
    }
    if (pw_finish())
        return pw.err == -2 ? -2 : -1;
    memset(&h, 0, sizeof h);
    h.magic = PJ_MAGIC;
    h.slot = (uint16_t)TPR_BANK;
    h.copy = (uint16_t)copy;
    if (c >= 0 && cur.seq - pj_seq < 0x80000000u)
        pj_seq = cur.seq;
    h.seq = ++pj_seq;
    h.len = pw.off;
    h.crc = ~pw.crc;
    h.raw = pw.raw;
    h.hcrc = pj_crc(&h, sizeof h - 4u);
    if (rf_prog(pw.base, &h, sizeof h) || pj_head(TPR_BANK, copy, &cur))   /* the commit record, last; read back */
        return -1;
    tpr_scan();
    return 0;
}

/* main loop: track t saved as preset p. 0, -1 the flash refused, -2 no room in the bank */
static int tpr_save(uint32_t p, uint32_t t) { return t < NTRK ? tpr_write(p, t) : -1; }

/* main loop: preset p emptied. 0, or -1 */
static int tpr_delete(uint32_t p) { return tpr_write(p, NTRK) == 0 ? 0 : -1; }

/* ------------------------------------------------------------------------------- the snapshot --- */
/* What SAVE held puts back after a preset load or a reset (SCL): the track as it was, its depths, and what the
 * other views' knobs set (the levels, REC IN, the master, the key), since a reset can be on those pages too */
static struct {
    track_params_t p;
    mod_deps_t dep;
    int16_t mst[NMS], key[NKY];
    uint8_t lv[NTRK], rin[NTRK];
    uint8_t trk, valid;
} trk_undo;

static void trk_snap(uint32_t t)
{
    uint32_t i;
    if (t >= NTRK)
        return;
    trk_undo.p = tp[t];
    trk_undo.dep = mdl[t];
    for (i = 0; i < NMS; i++)
        trk_undo.mst[i] = mst[i];
    for (i = 0; i < NKY; i++)
        trk_undo.key[i] = keyp[i];
    for (i = 0; i < NTRK; i++) {
        trk_undo.lv[i] = track[i].level;
        trk_undo.rin[i] = tp[i].recin;
    }
    trk_undo.trk = (uint8_t)t;
    trk_undo.valid = 1;
}

/* main loop: the snapshot back. -1: none */
static int trk_restore(void)
{
    uint32_t t = trk_undo.trk, i;
    if (!trk_undo.valid || t >= NTRK)
        return -1;
    tp[t] = trk_undo.p;
    mdl[t] = trk_undo.dep;
    for (i = 0; i < NMS; i++)
        mst[i] = trk_undo.mst[i];
    for (i = 0; i < NKY; i++)
        keyp[i] = trk_undo.key[i];
    for (i = 0; i < NTRK; i++) {
        track[i].level = trk_undo.lv[i];
        tp[i].recin = trk_undo.rin[i];
    }
    trk_undo.valid = 0;
    mod_rebuild(t);
    return 0;
}

/* main loop: preset p into track t (a snapshot first, for SAVE held). 0, or -1: empty or unreadable (nothing
 * changed) */
static int tpr_load(uint32_t p, uint32_t t)
{
    int n = tpr_open();
    uint32_t i, k;
    if (p >= TPR_N || t >= NTRK || n <= (int)p)
        return -1;
    for (i = 0; i < p && !pr.bad; i++) {
        uint32_t len = pr_u16();
        for (k = 0; k < len; k++)
            (void)pr_byte();
    }
    if (pr.bad || pr_u16() < NSLOT + 2u || pr.bad)
        return -1;
    trk_snap(t);
    if (mod_undo_buf.trk == t)                           /* (MONO's undo was this track's: not over the new sound) */
        mod_undo_buf.valid = 0;
    tpr_read_rec(t);
    return 0;
}

/* boot: the summaries */
static void tpr_boot(void) { tpr_scan(); }
