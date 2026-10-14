/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo on the host: the app's own sources (the chain, the parameter table, the UI and its input) on stubs of the
 * display and the input scan.
 *
 *   bryo_host OUT_DIR        writes OUT_DIR/ppm/<palette>_<screen>.ppm and prints one line per check
 *
 * Audio: silence at rest, a held key's pitch, mute, a click-free release, load shedding, the master stage's
 *        ceiling. Input: what each pad, key and knob does (PRD 2, phase 1). Screens: every phase 1 screen in
 *        the one ink, for tests/ui_golden.py (pixel fingerprints) and the eye (tests/bryo_ui.sh makes PNGs). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define __attribute__(x)
#define memset felucca_memset            /* the firmware's own (libc.c), not the host's */
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#define FELUCCA_FLASH 0
#define FELUCCA_CDC 0
#define FELUCCA_VERSION "TEST"
#include "felucca_tables.h"
#include "../firmware/src/libc.c"

/* ------------------------------------------------------------ HAL stubs --- */
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {         /* (hal/fm1_input.h's: where each LED is) */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, {5, 11, 4, 10, 3, 9, 2, 8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13, 7, 6, 12}, {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    {0, 1, 15, 14, 17, 16, 19, 18, 20, 21, 22}, {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL];
static void fm1_led_dim_level(uint32_t lo) { (void)lo; }
#define FM1_TICKS_PER_US 1u
static struct { uint32_t notes, buttons; } fm1_in;
static uint32_t host_pressed, host_note_edges;
static int32_t host_enc[7];
static uint32_t fm1_ticks(void) { return 0; }
static uint32_t fm1_input_edges(uint32_t *r) { uint32_t p = host_pressed; if (r) *r = 0; host_pressed = 0; return p; }
static uint32_t fm1_input_note_edges(void) { uint32_t n = host_note_edges; host_note_edges = 0; return n; }
static int32_t fm1_enc_take(uint32_t e) { int32_t s = host_enc[e % 7u]; host_enc[e % 7u] = 0; return s; }
static void fm1_wdt_feed(void) {}
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
static uint16_t host_screen[240 * 240];
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_screen[(y + j) * 240u + x + i] = (uint16_t)((c >> 8) | (c << 8));
}
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            host_screen[(y + j) * 240u + x + i] = p[j * w + i];
}

/* ------------------------------------------------------- Bryo sources --- */
#define GFX_DOT_TEXT 1
#include "../firmware/src/gfx.c"
#include "../firmware/src/bryo.h"
static void ui_message(const char *s);
static void ui_redraw(void);
#include "../firmware/src/dsp.c"
#include "../firmware/src/master.c"
#include "../firmware/src/param.c"
#include "../firmware/src/mod.c"
#include "../firmware/src/mem.c"
#include "../firmware/src/tape.c"
#include "../firmware/src/synth.c"
#include "../firmware/src/poly.c"
#include "../firmware/src/drum_voice.c"
#include "../firmware/src/drum.c"
#include "../firmware/src/arp.c"
#include "../firmware/src/source.c"
#include "../firmware/src/grain.c"
#include "../firmware/src/reso.c"
#include "../firmware/src/color.c"
#include "../firmware/src/space.c"
#include "../firmware/src/mixer.c"
#include "../firmware/src/usbrec.c"
#include "../firmware/src/chain.c"
#include "../firmware/src/icons.c"
#include "../firmware/src/panel.c"
/* the flash under the user reels: a NOR model (erase to 0xFF, program clears bits) */
#define RF_HOST
static uint8_t host_nor[0x100000];
static uint32_t host_prog_limit = 0xFFFFFFFFu;           /* bytes a save may program before the "power cut" */
static const uint8_t *rf_ptr(uint32_t off) { return host_nor + off; }
static uint32_t host_nerase;                             /* erases so far (each one stops the device's sound) */
static int rf_erase(uint32_t off) { memset(host_nor + off, 0xFF, 4096); host_nerase++; return 0; }
static int rf_prog(uint32_t off, const void *src, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        if (!host_prog_limit)
            return -1;
        host_prog_limit--;
        host_nor[off + i] &= ((const uint8_t *)src)[i];
    }
    return 0;
}
#include "../firmware/src/reel.c"
#include "../firmware/src/scene.c"
#include "../firmware/src/project.c"
#include "../firmware/src/tpreset.c"
#include "../firmware/src/tpfactory.c"
#include "../firmware/src/settings.c"
#include "../firmware/src/ui_px.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/ui_viz.c"
#include "../firmware/src/ui_input.c"
#define BRYO_MSC 1                                       /* (vdisk.c's hooks for msc.c: tested without USB) */
#include "../firmware/src/vdisk.c"

static int fails;
static void check(const char *what, int ok)
{
    printf("bryo: %-78s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

static void power_on(void)
{
    memset(&ui, 0, sizeof ui);
    memset(track, 0, sizeof track);
    memset(track_rt, 0, sizeof track_rt);
    memset(tape_rt, 0, sizeof tape_rt);
    memset(tape_ctl, 0, sizeof tape_ctl);
    memset(&cap, 0, sizeof cap);
    memset(&capm, 0, sizeof capm);
    memset(reso, 0, sizeof reso);
    reso_cap = RS_N;
    memset(color, 0, sizeof color);
    memset(space, 0, sizeof space);
    memset(&ur, 0, sizeof ur);
    ur_mw = ur_mr = ur_rw = ur_rr = 0;
    memset(chan, 0, sizeof chan);
    comp.env = comp.gr_q8 = 0;
    comp.g = 4096;
    memset(gbuf, 0, sizeof gbuf);                        /* (GRAIN's buffers and grains: a fresh start, as the */
    memset(grain, 0, sizeof grain);                      /* device's power-on gives) */
    gr_used[0] = gr_used[1] = 0;
    memset(gr_trk, 0, sizeof gr_trk);                    /* (the slots by track, kept in step with gr_used) */
    sys.freeze = 0;
    vd_sp_w = vd_sp_r = 0;
    memset(&tape_undo, 0, sizeof tape_undo);
    memset(syn, 0, sizeof syn);
    memset(pol, 0, sizeof pol);
    drm_cap = DRM_NV;                                    /* (DRUM's voices: none shed) */
    drm_calm = 0;
    memset(arp_rt, 0, sizeof arp_rt);                    /* (the arpeggiators, the keys' notes) */
    memset((void *)arp_clear_req, 0, sizeof arp_clear_req);
    memset(key_base, 0, sizeof key_base);
    memset(mod_xkeys, 0, sizeof mod_xkeys);
    bryo_prefs = 0;                                      /* (SETUP's preferences: the defaults) */
    memset(drm_rt, 0, sizeof drm_rt);
    memset(&drm_undo_buf, 0, sizeof drm_undo_buf);
    memset(drm_dirty, 0, sizeof drm_dirty);
    drm_rq_w = drm_rq_r = 0;
    memset(pj_sum, 0, sizeof pj_sum);                    /* (the projects' RAM side: as at boot, before pj_boot) */
    memset(tpr_sum, 0, sizeof tpr_sum);                  /* (the track presets' list, and the snapshot) */
    memset(&trk_undo, 0, sizeof trk_undo);
    pj_cur = 0;
    pj_seq = 0;
    pj_hash_at = 0;
    pj_pre_off = 0xFFFFFFFFu;                            /* (no erase ahead pending) */
    memset(room, 0, sizeof room);                        /* (the shared rooms: their chunks go with the pool) */
    lim_env = LIM_T;                                     /* (the master's states: the limiter, the DC blocks, the
                                                          * speaker EQ, the output gain: as at boot. Left over from
                                                          * a run before, they bled its last tenth of a second into
                                                          * the next run's fingerprint, 2026-10-13) */
    lc_l1 = lc_l2 = lc_r1 = lc_r2 = dc_l = dc_r = dce_l = dce_r = 0;
    memset(lce, 0, sizeof lce);
    sb_lp1 = sb_lp2 = sb_lp3 = sb_lp4 = sb_env = sb_h1 = sb_h2 = sb_hl = 0;
    out_g = 32767;
    reso_cap = RS_N;                                     /* (shedding's caps, back to full) */
    reso_calm = 0;
    grain_cap = GR_CAP;
    grain_calm = 0;
    drm_cap = DRM_NV;
    chain_shed_count = 0;
    out_mute_req = 0;                                    /* (the output's fade: playing) */
    out_g = 32767;
    out_quiet = 0;
    memset(&sys, 0, sizeof sys);
    memset(&fm1_in, 0, sizeof fm1_in);
    panel = PANEL_DEFAULT;
    param_defaults();
    uslot_init();
    chain_init();
    vdisk_mount();
    ui_init();
    sys.bpm = 120;
    sys.ntrk = NTRK;
    sys.master_q12 = 2048;
    sys.keys_live = 1;
    sys.black_short = 1;
}

/* block b of track t's RAM tape: its peak, its data */
static uint32_t tpk(uint32_t t, uint32_t b)
{
    tape_view_t v = {0};
    v.map = tape_ctl[t].map;
    return tv_peak(&v, b);
}
static const uint8_t *tdata(uint32_t t, uint32_t b)
{
    tape_view_t v = {0};
    v.map = tape_ctl[t].map;
    return tv_data(&v, b);
}

/* ---------------------------------------------------------------- audio --- */
static int32_t out[2 * CTL];
static void render(uint32_t blocks, int32_t *buf)        /* buf: blocks * CTL left samples, or 0 */
{
    uint32_t b, i;
    for (b = 0; b < blocks; b++) {
        chain_block(out, CTL);
        if (buf)
            for (i = 0; i < CTL; i++)
                buf[b * CTL + i] = out[2u * i];
    }
}

static uint32_t note_bit_of_white(uint32_t w)
{
    uint32_t n;
    for (n = 0; n < 27u; n++)
        if (KEY_WHITE[n] == w)
            return 1u << n;
    return 0;
}

static uint32_t note_of_white(uint32_t w)              /* white key w's note (fm1_in.notes bit) */
{
    uint32_t n;
    for (n = 0; n < 27u && KEY_WHITE[n] != w; n++)
        ;
    return n;
}

static int32_t peak_of(const int32_t *x, uint32_t n)
{
    int32_t p = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        p = abs(x[i]) > p ? abs(x[i]) : p;
    return p;
}

static void test_tape(void)
{
    enum { NB = 1378 };                                  /* 1 s */
    static int32_t s[NB * CTL];
    char b[112];
    uint32_t i;
    int32_t step = 0, pk;
    {   /* the codec: a 440 Hz sine through the encoder and back */
        int16_t in[TAPE_BLK], o[TAPE_BLK];
        uint8_t data[TAPE_BLK / 2];
        int64_t es = 0, ee = 0;
        for (i = 0; i < TAPE_BLK; i++)
            in[i] = (int16_t)(16000.0 * sin(2 * M_PI * 440.0 * i / TAPE_SR));
        ima_enc(in, 0, 0, data, TAPE_BLK);
        ima_dec(data, 0, 0, o, TAPE_BLK);
        for (i = 32; i < TAPE_BLK; i++) {                /* (after the step size has settled) */
            es += (int64_t)in[i] * in[i];
            ee += (int64_t)(in[i] - o[i]) * (in[i] - o[i]);
        }
        snprintf(b, sizeof b, "ADPCM round trip of a sine: %.1f dB SNR (> 25 dB)", 10 * log10((double)es / (double)(ee + 1)));
        check(b, 10 * log10((double)es / (double)(ee + 1)) > 25.0);
    }
    {   /* the factory reels decode into sound */
        uint32_t r, ok = 1;
        for (r = 0; r < NREEL; r++)
        {
            uint32_t k, mx = 0;
            for (k = 0; k < REELS[r].nblk; k++)
                mx = REELS[r].peak[k] > mx ? REELS[r].peak[k] : mx;
            ok &= REELS[r].nblk > 100u && REELS[r].nblk <= TAPE_NBLK && mx > 100u;
        }
        check("every factory reel fits a tape and holds sound", ok);
    }
    power_on();
    render(NB, s);
    check("silence at rest (the transport stopped): every sample 0", peak_of(s, NB * CTL) == 0);

    sys.playing = 1;                                     /* PLAY: track 1 plays reel BEAT */
    render(NB, s);
    pk = peak_of(s, NB * CTL);
    snprintf(b, sizeof b, "PLAY: the reels play, under the limiter: peak %d (> 2000, <= 32767)", (int)pk);
    check(b, pk > 2000 && pk <= 32767);
    check("..track 1's head moves forward inside its loop", tape_rt[0].running && tape_rt[0].pos > 0 &&
          (tape_rt[0].pos >> 12) < (int32_t)(REELS[0].nblk * TAPE_BLK));

    sys.playing = 0;                                     /* STOP: a fade, then silence */
    render(NB / 4, s);
    for (i = 1; i < NB / 4 * CTL; i++)
        step = abs(s[i] - s[i - 1]) > step ? abs(s[i] - s[i - 1]) : step;
    check("STOP fades out within 250 ms (every track's last block silent)",
          peak_of(track_rt[0].last, CTL) == 0 && peak_of(track_rt[1].last, CTL) == 0 &&
          peak_of(track_rt[2].last, CTL) == 0 && peak_of(track_rt[3].last, CTL) == 0);

    {   /* the loop wraps: STRT 50, LEN 10 on track 1, 1 s of play stays inside [50 %, 60 %) */
        int32_t lo, hi, len = (int32_t)(REELS[0].nblk * TAPE_BLK), inside = 1;
        tp[0].dev[DEV_SRC][TK_STRT] = 5000;
        tp[0].dev[DEV_SRC][TK_LEN] = 1000;
        lo = len * 50 / 100;
        hi = lo + len * 10 / 100;
        sys.playing = 1;
        for (i = 0; i < NB; i++) {
            render(1, 0);
            inside &= (tape_rt[0].pos >> 12) >= lo && (tape_rt[0].pos >> 12) < hi;
        }
        check("the loop window holds the head (STRT 50, LEN 10: 1 s of play, never outside)", inside);
        tp[0].dev[DEV_SRC][TK_REV] = 1;                  /* reverse: the head runs back */
        {
            int32_t p0 = tape_rt[0].pos, back;
            render(10, 0);
            back = tape_rt[0].pos < p0 || tape_rt[0].xf > 0;
            check("REV (OP5): the head runs backwards", back);
        }
        tp[0].dev[DEV_SRC][TK_REV] = 0;
        tp[0].dev[DEV_SRC][TK_STRT] = 0;
        tp[0].dev[DEV_SRC][TK_LEN] = 10000;
        sys.playing = 0;
        render(NB / 4, 0);
    }

    {   /* a slice while stopped: white key 5 plays slice 5 of 16, then stops */
        int32_t len = (int32_t)(REELS[0].nblk * TAPE_BLK), s0 = len * 4 / 16, ran = 0;
        fm1_in.notes = note_bit_of_white(4);
        render(1, 0);
        check("stopped, white key 5 starts slice 5 (the head at its start)", tape_rt[0].running &&
              abs((tape_rt[0].pos >> 12) - s0) < 64);
        fm1_in.notes = 0;
        for (i = 0; i < NB && tape_rt[0].running; i++, ran++)
            render(1, 0);
        snprintf(b, sizeof b, "..and stops at its end: %d blocks (a slice is %d)", (int)ran, (int)(len / 16 * 2 / CTL));
        check(b, abs(ran - len / 16 * 2 / CTL) <= 2);
    }

    {   /* REC: track 2 records track 1 (its reel copied onto its tape first), DUB 0: a new take */
        uint32_t k, same = 0, diff = 0;
        check("REC arms: track 2's reel is copied onto its tape, REEL turns to TAPE",
              tape_prepare(1) && tape_src(1) == 0u && tape_ctl[1].nblk == REELS[1].nblk);
        sys.rec = 1u << 1;
        tp[1].dev[DEV_SRC][TK_DUB] = -100;                   /* (DUB -100: the input replaces the loop) */
        track[1].mute = 1;                               /* (only track 1 sounds into it) */
        track[2].mute = track[3].mute = 1;
        sys.playing = 1;
        render(NB * 2, 0);                               /* 2 s: a whole pass over the 2 s loop */
        sys.rec = 0;
        render(2, 0);                                    /* (the last block commits) */
        tape_unprepare(1);
        for (k = 0; k < REELS[1].nblk; k++) {           /* the take against what was there and what went in */
            same += abs((int)tpk(1, k) - (int)REELS[0].peak[k % REELS[0].nblk] * 100 / 127) < 24;
            diff += tpk(1, k) != REELS[1].peak[k];
        }
        snprintf(b, sizeof b, "..recording replaces it with track 1's beat (%u of %u blocks changed, %u follow the beat)",
                 diff, (unsigned)REELS[1].nblk, same);
        check(b, diff > REELS[1].nblk * 9u / 10u && same > REELS[1].nblk * 3u / 4u);
        sys.playing = 0;
        track[1].mute = track[2].mute = track[3].mute = 0;
        render(NB / 4, 0);
    }

    {   /* clear (POLY held) and undo (SAVE held) */
        tape_view_t v;
        tape_clear(1);
        tape_view(1, &v);
        check("a clear empties the tape (it plays nothing)", v.len == 0u && tape_ctl[1].empty);
        check("..and SAVE held brings it back", tape_undo_clear() == 1 && !tape_ctl[1].empty);
        check("..once", tape_undo_clear() == -1);
        tp[1].dev[DEV_SRC][TK_REEL] = 3;                 /* a reel over a tape with a take: REC refuses */
        check("REC on a reel refuses while the tape holds a take (no take is lost)", !tape_prepare(1) &&
              tape_ctl[1].nblk == REELS[1].nblk && !tape_ctl[1].empty);
        tape_clear(1);
        tp[1].dev[DEV_SRC][TK_REEL] = 3;
        check("..after a clear it copies the reel in", tape_prepare(1) && tape_ctl[1].nblk == REELS[2].nblk);
        check("..and the clear can't be undone any more (the reel is over it)", tape_undo_clear() == -1);
        tape_unprepare(1);
    }

    {   /* the master stage holds the ceiling: four reels at full level */
        uint32_t t;
        for (t = 0; t < NTRK; t++)
            track[t].level = 127;
        sys.master_q12 = MASTER_FULL;
        sys.playing = 1;
        render(NB, s);
        check("four reels at full level and MASTER full stay under full scale (soft clip)", peak_of(s, NB * CTL) <= 32767);
        sys.playing = 0;
    }
    {
        uint32_t before = chain_shed_count;
        chain_shed();
        check("shedding is counted, and never takes the tape", chain_shed_count == before + 1u);
    }
}

/* ---------------------------------------------------------------- input --- */
static void press(uint32_t b) { host_pressed |= 1u << panel.btn[b]; ui_input(); }
static void hold(uint32_t b) { fm1_in.buttons |= 1u << panel.btn[b]; host_pressed |= 1u << panel.btn[b]; ui_input(); }
static void let_go(uint32_t b) { fm1_in.buttons &= ~(1u << panel.btn[b]); ui_input(); }
static void tap(uint32_t b) { hold(b); let_go(b); }
static void key_edge(uint32_t note) { host_note_edges |= 1u << note; ui_input(); }
static uint32_t black_note(uint32_t k)
{
    uint32_t n;
    for (n = 0; n < 27u; n++)
        if (KEY_BLACK[n] == k)
            return n;
    return 0;
}

static void test_input(void)
{
    power_on();
    check("power-on: TRACK 1, the source (TAPE) focused", sys.sel == 0 && ui.kind == FOCUS_DEV && ui.dev == DEV_SRC);
    press(B_EDIT);
    check("EDIT: GRAIN", ui.kind == FOCUS_DEV && ui.dev == DEV_GRAIN);
    check("..GRAIN 1 opens with its mix: KNOB 3 ROUT, KNOB 4 WET", !strcmp(dev_p(0, DEV_GRAIN)[pdesc_at(dev_p(0, DEV_GRAIN), 2u)].label, "ROUT") && !strcmp(dev_p(0, DEV_GRAIN)[pdesc_at(dev_p(0, DEV_GRAIN), 3u)].label, "WET"));
    press(B_EDIT);
    press(B_EDIT);
    press(B_EDIT);
    press(B_EDIT);
    check("EDIT again: GRAIN 5 (BODY MODE ROOT TONE)", ui.dev == DEV_GRAIN && ui.page == 4u &&
          !strcmp(dev_p(0, DEV_GRAIN)[pdesc_at(dev_p(0, DEV_GRAIN), 16u)].label, "BODY"));
    press(B_EDIT);
    check("..again: GRAIN 6 (GRNS)", ui.dev == DEV_GRAIN && ui.page == 5u && !strcmp(dev_p(0, DEV_GRAIN)[pdesc_at(dev_p(0, DEV_GRAIN), 20u)].label, "GRNS"));
    press(B_EDIT);
    check("EDIT again, past GRAIN's pages 2 to 6: RIPPLE", ui.dev == DEV_RESO);
    press(B_EDIT);
    press(B_EDIT);
    press(B_EDIT);
    press(B_EDIT);
    check("EDIT again, past RIPPLE's pages 2 to 4: GRAIN", ui.dev == DEV_GRAIN && ui.page == 0u);
    press(B_FX);
    check("FX: COLOR", ui.dev == DEV_COLOR);
    press(B_FX);
    press(B_FX);
    press(B_FX);
    check("FX again, past COLOR's pages 2 and 3: SPACE", ui.dev == DEV_SPACE && ui.page == 0u);
    press(B_ENV);
    check("ENV: the source's envelope (TAPE's page 2: ATK DEC SUS REL), ENV lit as its pad",
          ui.kind == FOCUS_DEV && ui.dev == DEV_SRC && ui.page == 1u && focus_btn() == B_ENV);
    press(B_FX);
    press(B_HOME);
    check("HOME: the source again", ui.kind == FOCUS_DEV && ui.dev == DEV_SRC && ui.page == 0u);

    host_enc[panel.enc[EN_K2]] = 3;                      /* KNOB 2 on TAPE: LENGTH 100 -> clamps at 100 */
    host_enc[panel.enc[EN_K3]] = -10;                    /* KNOB 3: SPEED 100 % -> 100 - 10 * 4 = 60 % */
    ui_input();
    check("KNOB 2 clamps at the range's top (LENGTH 100)", tp[0].dev[DEV_SRC][1] == 10000);
    check("KNOB 3 steps a wide range by ~1 % per detent (SPEED 100 -> 60)", tp[0].dev[DEV_SRC][2] == 60);
    check("..and lights that dial (hot)", ui.hot == 2u && ui.hot_t > 0u);

    check("a turned knob becomes the page's last (the visualization names it)", ui.last == 2u);
    press(B_EDIT);
    check("..and a new page starts without one", ui.last == 0xFFu);
    press(B_HOME);
    press(B_HOME);
    check("HOME again: TAPE's page 2", ui.kind == FOCUS_DEV && ui.dev == DEV_SRC && ui.page == 1u);
    press(B_EDIT);
    check("EDIT from elsewhere: GRAIN, page 1", ui.dev == DEV_GRAIN && ui.page == 0u);
    press(B_EDIT);
    check("..again: GRAIN's page 2", ui.dev == DEV_GRAIN && ui.page == 1u);
    host_enc[panel.enc[EN_K1]] = 30;
    ui_input();
    check("..whose KNOB 1 is PTCH (0 -> +24), not SIZE", tp[0].dev[DEV_GRAIN][GP_PTCH] == 24 &&
          tp[0].dev[DEV_GRAIN][0] == 80);
    tp[0].dev[DEV_GRAIN][GP_PTCH] = 0;
    press(B_EDIT);
    check("..again: GRAIN's page 3 (REV PATN SCAL PRND)", ui.dev == DEV_GRAIN && ui.page == 2u &&
          !strcmp(dev_p(0, DEV_GRAIN)[pdesc_at(dev_p(0, DEV_GRAIN), 8u)].label, "REV") && !strcmp(dev_p(0, DEV_GRAIN)[pdesc_at(dev_p(0, DEV_GRAIN), 11u)].label, "PRND"));
    press(B_EDIT);
    check("..again: GRAIN's page 4 (SCAN WARP OFST FDBK: what grains read, set up left to right)", ui.dev == DEV_GRAIN &&
          ui.page == 3u && !strcmp(dev_p(0, DEV_GRAIN)[pdesc_at(dev_p(0, DEV_GRAIN), 12u)].label, "SCAN") && !strcmp(dev_p(0, DEV_GRAIN)[pdesc_at(dev_p(0, DEV_GRAIN), 13u)].label, "WARP"));
    press(B_EDIT);
    check("..again: GRAIN's page 5 (BODY: a floor under the grains)", ui.dev == DEV_GRAIN && ui.page == 4u);
    press(B_EDIT);
    check("..again: GRAIN's page 6 (GRNS)", ui.dev == DEV_GRAIN && ui.page == 5u);
    press(B_EDIT);
    check("..again: RIPPLE, opening with ROUT WET", ui.dev == DEV_RESO && ui.page == 0u &&
          pdesc_at(DEV_P[DEV_RESO], 2u) == RP_ROUT && pdesc_at(DEV_P[DEV_RESO], 3u) == RP_WET);
    press(B_EDIT);
    check("..again: RIPPLE's page 2, what rings (SCAL TONE DRV RIPL)", ui.dev == DEV_RESO && ui.page == 1u &&
          pdesc_at(DEV_P[DEV_RESO], 4u) == RP_SCAL && pdesc_at(DEV_P[DEV_RESO], 5u) == RP_TONE &&
          pdesc_at(DEV_P[DEV_RESO], 6u) == RP_DRV && pdesc_at(DEV_P[DEV_RESO], 7u) == RP_RIPL);
    press(B_EDIT);
    check("..again: RIPPLE's page 3, the filter in front (CUT RES TYPE)", ui.dev == DEV_RESO && ui.page == 2u &&
          pdesc_at(DEV_P[DEV_RESO], 8u) == RP_CUT && pdesc_empty(&DEV_P[DEV_RESO][pdesc_at(DEV_P[DEV_RESO], 11u)]) &&
          pdesc_pages(DEV_P[DEV_RESO]) == 4u);
    press(B_EDIT);
    check("..again: RIPPLE's page 4, the keys (KEYS PICK STRM VARY); MODE, its form, on no page (EDIT + SELECT)",
          ui.dev == DEV_RESO && ui.page == 3u && pdesc_at(DEV_P[DEV_RESO], 12u) == RP_KEYS &&
          pdesc_at(DEV_P[DEV_RESO], 13u) == RP_PICK && pdesc_at(DEV_P[DEV_RESO], 15u) == RP_VARY &&
          !strcmp(DEV_P[DEV_RESO][RP_KEYS].label, "KEYS") && DEV_P[DEV_RESO][RP_MODE].max == 3);
    press(B_EDIT);
    check("..again: back to GRAIN, page 1", ui.dev == DEV_GRAIN && ui.page == 0u);
    press(B_FX);
    press(B_FX);
    press(B_FX);
    press(B_FX);
    press(B_FX);
    check("FX: COLOR, COLOR 2, COLOR 3, SPACE, SPACE 2", ui.dev == DEV_SPACE && ui.page == 1u);
    press(B_LFO);
    press(B_LFO);
    check("a slot pad again: the slot's page 2", ui.kind == FOCUS_SLOT && ui.slot == 0u && ui.page == 1u);
    press(B_ARP);
    check("ARP: the arpeggiator's page (not a modulator)", ui.kind == FOCUS_ARP && ui.page == 0u && focus_btn() == B_ARP);
    press(B_ARP);
    check("..again: its page 2", ui.kind == FOCUS_ARP && ui.page == 1u);
    press(B_ARP);
    check("..again: page 3, RIPL ARP (RIPPLE's arpeggio: its MODE is RIPPLE's knob 12)", ui.kind == FOCUS_ARP &&
          ui.page == 2u && ({ int16_t *vp; const pdesc_t *d = ui_page(0, &vp); vp == &tp[0].dev[DEV_RESO][RP_AMODE] &&
          !strcmp(d->label, "MODE"); }));
    press(B_ARP);
    check("..and round to the first", ui.kind == FOCUS_ARP && ui.page == 0u);
    hold(B_LFO);                                         /* LFO held + OP3: the LFO pad shows M3 */
    key_edge(black_note(BK_OP3));
    let_go(B_LFO);
    check("LFO held + OP3: the LFO pad is M3 (slot 3, the random LFO), its page as the pad is let go, no mute",
          ui.lfo_slot == 3u && ui.kind == FOCUS_SLOT && ui.slot == 3u && !track[2].mute && tp[0].engine[3] == ME_WAVE &&
          tp[0].mod[3][1] == LFO_RND);
    press(B_LFO);
    press(B_LFO);
    press(B_LFO);
    press(B_LFO);
    check("..M3 has four pages, coming round to the first", ui.slot == 3u && ui.page == 0u);
    {
        uint32_t bpm = sys.bpm;
        hold(B_LFO);                                     /* the LFO pad (M3) held + SELECT: its engine */
        host_enc[panel.enc[EN_SELECT]] = 2;
        ui_input();
        let_go(B_LFO);
        check("a slot's pad held + SELECT: LFO -> SEQ (LFO ADSR SEQ FOLLOW), from its defaults, the tempo untouched",
              tp[0].engine[3] == ME_SEQ && tp[0].mod[3][2] == ME_P[ME_SEQ][2].def && sys.bpm == bpm && ui.page == 0u);
        hold(B_LFO);
        host_enc[panel.enc[EN_SELECT]] = -2;
        ui_input();
        let_go(B_LFO);
        check("..and back (round the four)", tp[0].engine[3] == ME_WAVE);
        tp[0].mod[3][1] = LFO_RND;                       /* (as it was: the random LFO) */
        hold(B_SEQ);                                     /* the SEQ pad + SELECT: it stays the sequencer */
        host_enc[panel.enc[EN_SELECT]] = 1;
        ui_input();
        let_go(B_SEQ);
        check("SEQ held + SELECT: M4 stays the sequencer, and says so", tp[0].engine[SLOT_SEQ] == ME_SEQ &&
              !strcmp(ui.msg, "M4 IS THE SEQUENCER"));
        hold(B_LFO);
        key_edge(black_note(BK_OP1));
        let_go(B_LFO);
        check("LFO held + OP1: back to M1", ui.lfo_slot == 0u && ui.slot == 0u);
    }
    press(B_HOME);

    hold(B_GLO);                                         /* GLO held + white key 3: TRACK 3 */
    check("GLO held: the mixer is up, the keys stop playing", ui.view == VIEW_MIXER && sys.keys_live == 0u);
    key_edge(2u + 2u);                                   /* note 4 = A3 = white key 3 */
    check("GLO + white key 3: TRACK 3", sys.sel == 2u);
    let_go(B_GLO);
    check("GLO let go after picking: back to the page, the keys play again", ui.view == VIEW_PAGE && sys.keys_live == 1u);
    check("each track keeps its own values (TRACK 3's SPEED at the default)", tp[2].dev[DEV_SRC][2] == 100);
    press(B_SCL);
    check("SCL does nothing now (the PRD's SEL moved under GLO)", sys.sel == 2u && ui.view == VIEW_PAGE);

    key_edge(black_note(BK_OP2));
    check("black OP2: TRACK 2 muted", track[1].mute == 1u);
    key_edge(black_note(BK_OP2));
    check("..again: unmuted", track[1].mute == 0u);

    key_edge(black_note(BK_OP5));
    check("black OP5: the focused track's tape reversed (TAPE 2's REV)", tp[2].dev[DEV_SRC][TK_REV] == 1);
    key_edge(black_note(BK_OP6));
    check("black OP6: half speed (TAPE 2's HALF)", tp[2].dev[DEV_SRC][TK_HALF] == 1);
    tp[2].dev[DEV_SRC][TK_REV] = tp[2].dev[DEV_SRC][TK_HALF] = 0;
    {   /* POLY held 0.5 s clears the focused tape; let go sooner does nothing; SAVE held undoes */
        uint32_t pn = black_note(BK_POLY);
        fm1_ms = 1000;
        fm1_in.notes |= 1u << pn;
        key_edge(pn);
        fm1_ms = 1300;
        fm1_in.notes &= ~(1u << pn);
        ui_input();
        check("the POLY key let go before 0.5 s: nothing is cleared", tape_src(2) == 3u);
        fm1_ms = 2000;
        fm1_in.notes |= 1u << pn;
        key_edge(pn);
        fm1_ms = 2510;
        ui_input();
        fm1_in.notes &= ~(1u << pn);
        check("..held 0.5 s: TRACK 3's tape cleared (its reel let go)", tape_src(2) == 0u && tape_ctl[2].empty);
        fm1_ms = 3000;
        hold(B_SAVE);
        fm1_ms = 3000 + HOLD_MS[settings_hold % 4u];
        ui_input();
        let_go(B_SAVE);
        check("SAVE held: the clear undone (the reel back)", tape_src(2) == 3u);
        fm1_ms = 4000;
        tap(B_SAVE);
        check("SAVE tapped: no undo; it saves the project (P1)", tape_src(2) == 3u && pj_sum[0].used &&
              !strncmp(ui.msg, "P1 SAVED", 8));
        pj_delete(0);
        ui.msg_t = 0;
    }

    tap(B_GLO);
    check("GLO tapped: the mixer stays up", ui.view == VIEW_MIXER && ui.glo_latched);
    host_enc[panel.enc[EN_K4]] = -20;
    ui_input();
    check("..KNOB 4 sets TRACK 4's level (100 -> 80)", track[3].level == 80u);
    tap(B_GLO);
    check("GLO tapped again: back to the page", ui.view == VIEW_PAGE);
    hold(B_GLO);
    host_enc[panel.enc[EN_K1]] = 3;
    ui_input();
    let_go(B_GLO);
    check("GLO held + KNOB 1: TRACK 1's level, and letting go goes back", track[0].level == 103u && ui.view == VIEW_PAGE);
    tap(B_GLO);
    hold(B_EDIT);
    check("EDIT held on the mixer: the SCENES page (the first), the mixer stays", ui.chan == CHAN_SCENES &&
          ui.view == VIEW_MIXER);
    host_enc[panel.enc[EN_K3]] = 2;
    ui_input();
    let_go(B_EDIT);
    check("EDIT let go after turning a knob: the levels again, the mixer still up", !ui.chan && ui.view == VIEW_MIXER &&
          track[2].level == 100u);
    tap(B_EDIT);
    check("EDIT tapped on the mixer: SCENES stays (latched, as a tapped GLO; the right hand free for the knobs)",
          ui.chan == CHAN_SCENES && ui.chan_latched && ui.view == VIEW_MIXER);
    tap(B_EDIT);
    check("..tapped again: the channel page (latched)", ui.chan == CHAN_STRIP && ui.chan_latched);
    host_enc[panel.enc[EN_K3]] = -30;
    ui_input();
    check("..KNOB 3 sets the selected track's FILT (TRACK 3: 0 -> -30), not a level", tp[2].ch[CH_FILT] == -30 &&
          track[2].level == 100u);
    tp[2].ch[CH_FILT] = 0;
    tap(B_EDIT);
    check("..tapped again: MASTER, the compressor's page (latched)", ui.chan == CHAN_MASTER && ui.chan_latched &&
          ui.view == VIEW_MIXER);
    host_enc[panel.enc[EN_K1]] = 20;
    ui_input();
    check("..KNOB 1 sets the master compressor's AMT (0 -> 20), one for all tracks", mst[MS_AMT] == 20);
    mst[MS_AMT] = 0;
    tap(B_EDIT);
    check("..tapped again: KEY, the project's key and scale (latched)", ui.chan == CHAN_KEY && ui.chan_latched &&
          ui.view == VIEW_MIXER);
    host_enc[panel.enc[EN_K1 + 1u]] = 1;
    ui_input();
    check("..KNOB 2 sets SCAL (MIN -> DOR), one for the project", keyp[KY_SCAL] == 3);
    keyp[KY_SCAL] = 2;
    tap(B_EDIT);
    check("..tapped again: SETUP, the panel's preferences (latched)", ui.chan == CHAN_SETUP && ui.chan_latched &&
          ui.view == VIEW_MIXER);
    tap(B_EDIT);
    check("..tapped again: the levels", !ui.chan && !ui.chan_latched && ui.view == VIEW_MIXER);
    press(B_FX);
    check("a page pad closes the mixer", ui.view == VIEW_PAGE && ui.dev == DEV_COLOR && !ui.glo_latched);
    press(B_HOME);

    host_enc[panel.enc[EN_SELECT]] = 5;
    ui_input();
    check("SELECT: the tempo (120 -> 125)", sys.bpm == 125u);
    press(B_PLAY);
    check("PLAY: playing", sys.playing == 1u);
    press(B_REC);
    check("REC: TRACK 3 armed", sys.rec == 1u << 2);
    press(B_OCTUP);
    check("OCT+: TRACK 3's keys an octave up", track[2].octave == 4u);
}

/* ------------------------------------------------------------- settings --- */
/* a record Felucca 1.0.3 saved (PER4): Bryo keeps its calibration, palette and USB prefs, and saves Felucca's own
 * fields back untouched */
static void test_settings(void)
{
    persist_t p, q;
    uint32_t i;
    int ok = 1;
    memset(&p, 0, sizeof p);
    p.magic = PERSIST_MAGIC;
    p.palette = palette_to_stored(3);
    p.lowcut = 2;
    p.panel = PANEL_DEFAULT;
    p.panel.btn[B_PLAY] = PANEL_DEFAULT.btn[B_REC];      /* a learned table: PLAY and REC swapped */
    p.panel.btn[B_REC] = PANEL_DEFAULT.btn[B_PLAY];
    for (i = 0; i < sizeof p.felucca; i++)
        p.felucca[i] = (uint8_t)(i * 7u + 1u);           /* Felucca's favorites and MENU flags: any bytes */
    p.felucca[BPREFS_AT] = 0;                            /* (but Bryo's own byte, an engine Felucca never had: 0, as
                                                          * every real record has it) */
    p.felucca[PREFS_AT] = PREF_SERIAL_OFF | PREF_USB_FIXED;
    panel = PANEL_DEFAULT;
    check("a Felucca PER4 record is read as current", settings_import(&p, (int)sizeof p) == 1);
    check("..its learned panel table is Bryo's", panel.btn[B_PLAY] == PANEL_DEFAULT.btn[B_REC]);
    check("..USB SERIAL OFF and USB LEVEL FIXED hold", (ui_prefs & PREF_SERIAL_OFF) && fx_usb_fixed == 1u);
    check("..the palette and SPEAKER EQ hold", settings.palette == 3u && settings.lowcut == 2u);
    q = p;
    settings_export(&q);
    for (i = 0; i < sizeof q.felucca; i++)
        ok &= q.felucca[i] == p.felucca[i];
    check("a Bryo save writes Felucca's fields back byte for byte", ok && !memcmp(&q.panel, &p.panel, sizeof q.panel));
    check("a record of the wrong size is refused (no partial read)", settings_import(&p, (int)sizeof p - 1) == 0);
    panel = PANEL_DEFAULT;
    ui_prefs = 0;
    fx_usb_fixed = 0;
}

/* -------------------------------------------------------------- screens --- */
static const char *out_dir;
static void shot_screen(const char *pal, const char *name);
static void chain_poll(void);
static void shot(const char *pal, const char *name)
{
    chain_poll();                                        /* (the main loop's bookkeeping, as every frame) */
    ui.force = 1;
    ui_draw();
    shot_screen(pal, name);
}
static void shot_screen(const char *pal, const char *name)   /* the screen as it is (no redraw) */
{
    char path[512];
    FILE *f;
    uint32_t i;
    snprintf(path, sizeof path, "%s/ppm/%s_%s.ppm", out_dir, pal, name);
    f = fopen(path, "wb");
    if (!f) {
        check(path, 0);
        return;
    }
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = host_screen[i];
        c = (uint16_t)((c >> 8) | (c << 8));             /* (the blit holds the LCD's byte order) */
        fputc(((c >> 11) & 31) * 255 / 31, f);
        fputc(((c >> 5) & 63) * 255 / 63, f);
        fputc((c & 31) * 255 / 31, f);
    }
    fclose(f);
}

static void turn(uint32_t k, int32_t d) { host_enc[panel.enc[EN_K1 + k]] = d; ui_input(); }

static void ur_feed(uint32_t frames, double hz, double a);   /* (test_usbrec's: the computer playing) */
static void screens_in(const char *pal)
{
    uint32_t s;
    power_on();
    lcd_fill(0, 0, 240, 240, T_BG);
    shot(pal, "tape");
    turn(2, -60);                                        /* SPEED to -140 %: reversed, the chevrons in the accent */
    shot(pal, "tape_speed");
    turn(0, 20);                                         /* START 20 %, LENGTH 60 %, DUB 80 % */
    turn(1, -40);
    turn(3, 30);
    shot(pal, "tape_loop");
    press(B_HOME);                                       /* TAPE 2: the slice envelope (as it is) */
    shot(pal, "tape_env");
    press(B_HOME);                                       /* TAPE 3: FADE 60 ms, REV on, GAIN +6 dB */
    turn(0, 50);
    turn(1, 1);
    turn(3, 6);
    shot(pal, "tape2");
    press(B_HOME);                                       /* TAPE 3: REEL KEYS, ROTA 25 % (playing starts there), LVL -6 */
    turn(0, 1);
    turn(1, 25);
    turn(3, -6);
    shot(pal, "tape3");
    turn(3, 6);
    press(B_HOME);
    sys.playing = 1;                                     /* the head running, then REC armed */
    render(600, 0);
    shot(pal, "tape_playing");
    press(B_REC);
    render(4, 0);
    shot(pal, "tape_rec");
    press(B_REC);
    sys.playing = 0;
    render(400, 0);
    hold(B_HOME);                                        /* HOME held + SELECT: SYNTH, its four pages */
    host_enc[panel.enc[EN_SELECT]] = 1;
    ui_input();
    let_go(B_HOME);
    shot(pal, "synth");
    turn(1, 30);                                         /* DTUN 38 ct, MIX 70 %, NOIS 20 % */
    turn(2, 20);
    turn(3, 20);
    shot(pal, "synth_osc");
    press(B_HOME);                                       /* AMP (POLY's order: the envelope, then the filter) */
    turn(0, 30);
    shot(pal, "synth_amp");
    press(B_HOME);                                       /* FILTER: CUT down, RES up */
    turn(0, -20);
    turn(1, 50);
    shot(pal, "synth_filter");
    press(B_HOME);                                       /* VOICE: GLID, DRV, TUNE */
    turn(1, 70);
    turn(2, 60);
    turn(3, -5);
    shot(pal, "synth_voice");
    press(B_HOME);                                       /* LEVEL: -9 dB (KNOB 4, as every source's) */
    turn(3, -9);
    shot(pal, "synth_level");
    hold(B_HOME);                                        /* POLY: its four pages */
    host_enc[panel.enc[EN_SELECT]] = 1;
    ui_input();
    let_go(B_HOME);
    shot(pal, "poly");
    turn(1, 30);                                         /* STRT 30 %, TUNE -5, VOIC 3 */
    turn(2, -5);
    turn(3, -1);
    shot(pal, "poly_start");
    press(B_HOME);                                       /* ENV: a slower attack, SUS 70 */
    turn(0, 40);
    turn(2, -30);
    shot(pal, "poly_env");
    press(B_HOME);                                       /* FILTER: HP, CUT 60, RES 50, ENV +40 */
    turn(0, -67);
    turn(1, 50);
    turn(2, 2);
    turn(3, 40);
    shot(pal, "poly_filter");
    press(B_HOME);                                       /* LEVEL: +4 dB */
    turn(3, 4);
    shot(pal, "poly_level");
    hold(B_HOME);                                        /* DRUM: its four pages */
    host_enc[panel.enc[EN_SELECT]] = 1;
    ui_input();
    let_go(B_HOME);
    shot(pal, "drum");                                   /* PATTERN: ROCK's two bars, the first shown */
    sys.playing = 1;                                     /* playing: the playhead, three sixteenths in */
    render(3u * 172u + 20u, 0);
    shot(pal, "drum_playing");
    sys.playing = 0;
    render(4, 0);
    press(B_HOME);                                       /* VARY: SEED 7 at VARY 80: what this version changes */
    turn(0, 7);
    turn(1, 40);
    shot(pal, "drum_vary");
    press(B_HOME);                                       /* KIT: the instrument STEP picks (BD), DEC +30 */
    turn(1, 30);
    shot(pal, "drum_kit");
    fm1_in.notes = note_bit_of_white(4);                 /* the low conga held: its own knobs, TUNE -3 */
    turn(0, -3);
    shot(pal, "drum_inst");
    fm1_in.notes = 0;
    ui_input();
    press(B_HOME);                                       /* STEP: the snare's row of bar 1 (INST, KNOB 1) */
    turn(0, 1);
    shot(pal, "drum_step");
    turn(2, 2);                                          /* MODE LIVE: the grid, named */
    shot(pal, "drum_live");
    turn(2, -2);
    hold(B_HOME);                                        /* back to TAPE (round past DRUM) */
    host_enc[panel.enc[EN_SELECT]] = 1;
    ui_input();
    let_go(B_HOME);
    press(B_EDIT);
    shot(pal, "grain");
    turn(1, 40);                                         /* GRAIN 1: RATE 80 %, WET 80 (its mix: ROUT WET) */
    turn(3, 80);
    shot(pal, "grain_busy");
    press(B_EDIT);                                       /* GRAIN 2: PTCH +7, SPRD 90, SPRY 60, CONT 100 */
    turn(0, 7);
    turn(1, 60);
    turn(2, 40);
    turn(3, 50);
    shot(pal, "grain2");
    press(B_EDIT);                                       /* GRAIN 3: REV 40, PATN SWNG, SCAL MAJ, PRND 5 */
    turn(0, 40);
    turn(1, -2);
    turn(2, 2);
    turn(3, 5);
    shot(pal, "grain3");
    press(B_EDIT);
    press(B_EDIT);                                       /* GRAIN 5: BODY 40, MODE TRI, ROOT A2, TONE 400 Hz */
    turn(0, 40);
    turn(1, 1);
    turn(3, 14);
    shot(pal, "grain_body");
    tp[0].dev[DEV_GRAIN][GP_BODY] = 0;                   /* (off again: the shots after as they were) */
    tp[0].dev[DEV_GRAIN][GP_BMOD] = BODY_SRC;
    tp[0].dev[DEV_GRAIN][GP_BTON] = DEV_P[DEV_GRAIN][GP_BTON].def;
    focus_dev(DEV_GRAIN, 0);                             /* (playing, on GRAIN 1: SIZE RATE ROUT WET) */
    {   /* playing a bar into the buffer, then the 0 black key held: the buffer frozen, the grains looping it */
        uint32_t u, z = 0;
        for (u = 0; u < 27u; u++)
            if (KEY_BLACK[u] == BK_ZERO)
                z = 1u << u;
        sys.playing = 1;
        for (u = 0; u < 1378u * 3u; u++) {
            render(1, 0);
            chain_poll();
        }
        shot(pal, "grain_playing");
        fm1_in.notes = z;
        render(40, 0);
        shot(pal, "grain_frozen");
        fm1_in.notes = 0;
        focus_dev(DEV_GRAIN, 5);                         /* GRAIN 6: GRNS 12, held whatever else */
        turn(0, 12);
        render(100, 0);
        shot(pal, "grain6");
        turn(0, -12);
        focus_dev(DEV_GRAIN, 3);                         /* GRAIN 4: SCAN DLY, WARP 52 %, OFST 50, FDBK 30 */
        turn(0, 2);
        turn(1, -12);
        turn(2, 25);
        turn(3, 30);
        render(400, 0);
        shot(pal, "grain4");
        turn(0, -3);                                     /* SCAN TAPE: grains of the tape, its field zoomed */
        render(400, 0);
        shot(pal, "grain_tape");
        turn(0, 1);
        turn(1, 12);
        turn(2, -25);
        turn(3, -30);
        sys.playing = 0;
        render(400, 0);
    }
    focus_dev(DEV_RESO, 0);
    shot(pal, "resonator");
    turn(1, 35);                                         /* DEC 95: sharp peaks */
    turn(3, 80);                                         /* WET 80 (KNOB 3-4: ROUT WET, its mix) */
    tp[0].dev[DEV_RESO][RP_TONE] = 90;                   /* TONE 90 (page 2): the upper partials kept */
    shot(pal, "resonator_wet");
    press(B_EDIT);                                       /* RIPPLE 2, what rings: the major chord, DRV 60, RIPL 40 */
    turn(0, 1);
    turn(2, 35);                                         /* (from DRV's default, 25) */
    turn(3, 40);
    render(2, 0);
    shot(pal, "ripple2");
    check("RIPPLE 2 shot: SCAL MAJ, DRV 60, RIPL 40", tp[0].dev[DEV_RESO][RP_SCAL] == 1 &&
          tp[0].dev[DEV_RESO][RP_DRV] == 60 && tp[0].dev[DEV_RESO][RP_RIPL] == 40 && ui.page == 1u);
    fm1_in.notes = note_bit_of_white(7);                 /* a key on the RIPPLE page: the root to G3, plucked */
    render(2, 0);
    fm1_in.notes = 0;
    shot(pal, "resonator_key");
    {   /* the forms: EDIT held + SELECT, the page kept, each its name, labels and picture */
        static const char *const SHOT[4] = {0, "ripple_comb", "ripple_reso", "ripple_bank"};
        uint32_t m, ok = 1;
        focus_dev(DEV_RESO, 1);
        sys.playing = 1;
        for (m = 1; m < 4u; m++) {
            hold(B_EDIT);
            host_enc[panel.enc[EN_SELECT]] = 1;
            ui_input();
            let_go(B_EDIT);
            render(200, 0);
            shot(pal, SHOT[m]);
            ok &= tp[0].dev[DEV_RESO][RP_MODE] == (int16_t)m && ui.dev == DEV_RESO && ui.page == 1u &&
                  !strcmp(dev_name(0, DEV_RESO), RS_FORM_NAME[m]) && pdesc_pages(dev_p(0, DEV_RESO)) == 4u;
        }
        ok &= !strcmp(dev_p(0, DEV_RESO)[RP_TONE].label, "TILT") && !strcmp(dev_p(0, DEV_RESO)[RP_DEC].label, "RING");
        hold(B_EDIT);
        host_enc[panel.enc[EN_SELECT]] = 1;
        ui_input();
        let_go(B_EDIT);
        sys.playing = 0;
        check("RIPPLE's form, EDIT held + SELECT: COMB, RESO, 12-BAND, round to RIPPLE, its page kept (the tempo "
              "left alone)", ok && tp[0].dev[DEV_RESO][RP_MODE] == 0 && ui.page == 1u && ui.dev == DEV_RESO &&
              !strcmp(dev_name(0, DEV_RESO), "RIPPLE") && sys.bpm == 120);
    }
    press(B_EDIT);                                       /* RIPPLE 3, the filter in front: CUT down, RES 60, BP */
    turn(0, -40);
    turn(1, 60);
    turn(2, 1);
    shot(pal, "resonator2");
    tp[0].dev[DEV_RESO][RP_MODE] = 0;                    /* (SCAL stays MAJ, the filter as set: the screens after) */
    tp[0].dev[DEV_RESO][RP_DRV] = 25;
    tp[0].dev[DEV_RESO][RP_RIPL] = 0;
    focus_dev(DEV_RESO, 1);
    press(B_FX);
    shot(pal, "color");
    turn(0, 60);                                         /* DRIVE 60: the transfer curve */
    shot(pal, "color_drive");
    turn(1, 70);                                         /* CRUSH 70: steps */
    shot(pal, "color_crush");
    turn(3, -30);                                        /* WET 70 (KNOB 3-4: ROUT WET, its mix) */
    press(B_FX);                                         /* COLOR 2: NOIS TILT NDEC NTON */
    turn(0, 50);                                         /* NOISE 50 */
    shot(pal, "color_noise");
    turn(1, -30);                                        /* TILT -30: darker */
    shot(pal, "color_tone");
    turn(2, 30);                                         /* NDEC longer, NTON -30 */
    turn(3, -30);
    shot(pal, "color2");
    press(B_FX);                                         /* COLOR 3: CMOD BOTH, LVL -6 dB */
    turn(0, 2);
    turn(3, -6);
    shot(pal, "color3");
    press(B_FX);
    shot(pal, "space");
    turn(1, 40);                                         /* FDBK 70; its sends, DLY 50 and VERB 30 (KNOB 3-4) */
    turn(2, 50);
    turn(3, 30);
    shot(pal, "space_long");
    tp[1].dev[DEV_SPACE][SP_VERB] = 40;                  /* (T2 sends to room B too) */
    tp[1].dev[DEV_SPACE][SP_ROOM] = 1;
    press(B_FX);                                         /* SPACE 2: ROOM B, its SIZE 80, DEC 70 (T1 and T2 share it) */
    turn(0, 1);
    turn(1, 30);
    turn(2, 30);
    chain_poll();
    shot(pal, "space2");
    press(B_FX);                                         /* SPACE 3: the echoes' TONE -40, the room's QUAL LO */
    turn(0, -40);
    turn(3, 1);
    shot(pal, "space3");
    for (s = 0; s < NROOM * NRM; s++)
        rmp[s / NRM][s % NRM] = RM_P[s % NRM].def;
    tp[1].dev[DEV_SPACE][SP_VERB] = 0;
    for (s = 0; s < NSLOT; s++) {                        /* (by slot: mod3 is the SEQ, M4; mod4 slot 3, M3) */
        static const uint8_t B[NSLOT] = {B_LFO, B_LFO, B_SEQ, B_LFO};
        char nm[8] = {'m', 'o', 'd', (char)('1' + s), 0};
        if (s == 1u || s == 3u) {                        /* M2, M3: LFO held + OP2 / OP3, let go: its page (ENV is
                                                          * the source's envelope) */
            hold(B_LFO);
            key_edge(black_note(s == 1u ? BK_OP2 : BK_OP3));
            let_go(B_LFO);
            ui.msg_t = 0;
        } else {
            press(B[s]);
        }
        shot(pal, nm);
    }
    hold(B_LFO);                                         /* the LFO pad back to M1 (its page, as M3's was up) */
    key_edge(black_note(BK_OP1));
    let_go(B_LFO);
    ui.msg_t = 0;                                        /* (the pictures, not the message) */
    turn(1, 1);                                          /* LFO: TRIANGLE, SKEW +40, FOLD 50 */
    turn(2, 40);
    turn(3, 50);
    shot(pal, "mod1_tri_fold");
    press(B_LFO);                                        /* LFO 2: CURV -60, SMTH 30, VAR 40, LEN 4 */
    turn(0, -60);
    turn(1, 30);
    turn(2, 40);
    turn(3, -4);
    shot(pal, "mod1_p2");
    press(B_LFO);                                        /* LFO 3: TRIG KEY, FADE 40 (SYNC stays BPM) */
    turn(1, 1);
    turn(2, 40);
    shot(pal, "mod1_p3");
    press(B_LFO);                                        /* LFO 4: PHAS 90, SPRD 50, AMT 70, OFS +20 */
    turn(0, 90);
    turn(1, 50);
    turn(2, -30);
    turn(3, 20);
    shot(pal, "mod1_p4");
    hold(B_LFO);                                         /* ADSR 2 (M2, the LFO pad on OP2): ACRV -60, DCRV +60, */
    key_edge(black_note(BK_OP2));                        /* RCRV +40, SPRD 40 */
    let_go(B_LFO);
    tap(B_LFO);
    ui.msg_t = 0;
    turn(0, -60);
    turn(1, 60);
    turn(2, 40);
    turn(3, 40);
    shot(pal, "mod2_p2");
    tap(B_LFO);                                          /* ADSR 3: VEL 40, LOOP on */
    turn(0, 40);
    turn(1, 1);
    shot(pal, "mod2_p3");
    hold(B_LFO);                                         /* (the LFO pad back to M1) */
    key_edge(black_note(BK_OP1));
    let_go(B_LFO);
    ui.msg_t = 0;
    press(B_SEQ);
    turn(0, -4);                                         /* SEQ: LEN 12, SLEW 60, SWING 50 */
    turn(2, 60);
    turn(3, 50);
    shot(pal, "mod3_seq");
    press(B_SEQ);                                        /* SEQ 2: PING, PROB 70, STRT 5 */
    turn(0, 2);
    turn(2, -30);
    turn(3, 4);
    shot(pal, "mod3_p2");
    hold(B_LFO);                                         /* M3 (LFO held + OP3; let go: its page) */
    key_edge(black_note(BK_OP3));
    let_go(B_LFO);
    ui.msg_t = 0;                                        /* (the pictures, not the message) */
    shot(pal, "mod4_rnd");                               /* slot 3, M3: the LFO on RND, VAR 20 */
    press(B_LFO);                                        /* LFO 2 on RND: SMTH 40, VAR 60, LEN 12 */
    turn(1, 40);
    turn(2, 40);
    turn(3, 4);
    shot(pal, "mod4_smooth");
    press(B_LFO);
    press(B_LFO);                                        /* LFO 4: PHAS 90, SPRD 40, AMT 80, OFS +20 */
    turn(0, 90);
    turn(1, 40);
    turn(2, -20);
    turn(3, 20);
    shot(pal, "mod4_p4");
    hold(B_LFO);                                         /* M3 runs FOLLOW: RISE 4, FALL 30, GAIN +6 */
    host_enc[panel.enc[EN_SELECT]] = -1;
    ui_input();
    let_go(B_LFO);
    turn(1, 6);
    turn(2, -6);
    turn(3, -20);
    shot(pal, "mod4_follow");
    press(B_LFO);                                        /* FOLLOW 2: HOLD 1/16 */
    turn(0, 2);
    shot(pal, "mod4_follow2");
    hold(B_LFO);                                         /* (the LFO pad back to M1) */
    key_edge(black_note(BK_OP1));
    let_go(B_LFO);
    ui.msg_t = 0;                                        /* (the pictures, not the message) */
    press(B_SEQ);                                        /* SEQ: white keys 2 and 3 held, KNOB 1 raises those steps */
    fm1_in.notes |= note_bit_of_white(1) | note_bit_of_white(2);
    ui_input();
    turn(0, 30);
    shot(pal, "mod3_steps");
    fm1_in.notes &= ~(note_bit_of_white(1) | note_bit_of_white(2));
    ui_input();
    press(B_FX);                                         /* COLOR, LFO's pad held: DRV +40 %, CRSH -25 % */
    hold(B_LFO);
    turn(0, 40);
    turn(1, -25);
    shot(pal, "mod_depth");
    let_go(B_LFO);
    shot(pal, "mod_marks");                              /* let go: the modulated knobs marked */
    mod_clear(0);                                        /* (the screens below as they were) */
    mod_undo_buf.valid = 0;
    press(B_HOME);
    tap(B_GLO);
    track[1].mute = 1;                                   /* track 2 muted, track 1 sounding */
    track_rt[0].peak = 9000;
    track_rt[2].peak = 1200;
    tape_reserve(0, 22, 1);                              /* what the tracks hold: tapes of 4.1, 2.0 and 6.1 s */
    tape_ctl[0].nblk = 22 * MEM_CB;
    tape_reserve(1, 11, 1);
    tape_ctl[1].nblk = 11 * MEM_CB;
    tape_reserve(3, 33, 1);
    tape_ctl[3].nblk = 33 * MEM_CB;
    sys.cpu_q8 = 107;                                    /* (the load as the audio ISR measures it: 41 %) */
    shot(pal, "mixer");
    hold(B_GLO);                                         /* GLO held + SELECT: TRACKS 3 (track 4 parked) */
    host_enc[panel.enc[EN_SELECT]] = -1;
    ui_input();
    shot(pal, "mixer_tracks3");
    host_enc[panel.enc[EN_SELECT]] = 1;
    ui_input();
    let_go(B_GLO);
    tap(B_GLO);                                          /* (the mixer up again, latched, for the shots below) */
    tap(B_EDIT);                                         /* EDIT twice (SCENES, then the channel): LOW +6, HIGH -4, a
                                                          * low-pass, PAN right */
    tap(B_EDIT);
    turn(0, 6);
    turn(1, -4);
    turn(2, -55);
    turn(3, 30);
    shot(pal, "mixer_channel");
    tap(B_EDIT);                                         /* EDIT again: MASTER, the compressor */
    turn(0, 60);                                         /* AMT 60, MIX 70, and what it takes off now */
    turn(3, -30);
    comp.gr_q8 = 220;
    shot(pal, "mixer_master");
    comp.gr_q8 = 0;
    for (s = 0; s < NMS; s++)
        mst[s] = MS_P[s].def;
    tap(B_EDIT);                                         /* (KEY, SETUP, then the levels: the channel's marks) */
    tap(B_EDIT);
    tap(B_EDIT);
    shot(pal, "mixer_eq");
    tap(B_GLO);
    hold(B_GLO);
    shot(pal, "mixer_held");
    let_go(B_GLO);
    host_enc[panel.enc[EN_ALGO]] = 1;                    /* the routing view: a bounce chain T1 -> T2 -> T3, T4 on itself */
    ui_input();
    shot(pal, "route");
    turn(1, 2);                                          /* T2 records T1 */
    turn(2, 3);                                          /* T3 records T2 */
    turn(3, 5);                                          /* T4 records itself */
    sys.rec = 2u;                                        /* (T2 armed) */
    tp[0].src = SRC_SYNTH;
    shot(pal, "route_chain");
    sys.rec = 0;
    tp[0].src = SRC_TAPE;
    press(B_HOME);
    press(B_PLAY);
    press(B_REC);
    shot(pal, "playing");
    ui_message("SAVE: PROJECTS ARRIVE IN PHASE 8");
    shot(pal, "message");
    ui.msg_t = 0;
    {
    uint8_t was_rec = sys.rec;                           /* (put back after: the screens below are as they were) */
    sys.playing = 0;                                     /* the USB record mode: stopped, REC held a second */
    for (s = 0; s < NTRK; s++)
        tape_unprepare(s);
    sys.rec = 0;
    hold(B_REC);
    fm1_ms += 1100;
    ui_input();
    let_go(B_REC);
    ui.msg_t = 0;
    shot(pal, "usbrec");
    ur_feed(400, 441, 20000);                            /* the computer playing: the level */
    shot(pal, "usbrec_live");
    press(B_REC);
    for (s = 0; s < 28u; s++) {                          /* a take of 2.5 s, a phrase that swells and falls */
        ur_feed(3938, 441, 2000.0 + 12000.0 * sin(M_PI * s / 27.0) * (s % 7u < 5u ? 1.0 : 0.3));
        chain_poll();
    }
    ur_feed(40, 441, 16000);
    ui.msg_t = 0;
    shot(pal, "usbrec_rec");
    press(B_REC);
    ui.msg_t = 0;
    shot(pal, "usbrec_choose");
    key_edge((uint32_t)__builtin_ctz(note_bit_of_white(1)));
    shot(pal, "usbrec_pick");
    for (s = 0; s < 5u; s++)                             /* trimmed: the kept part in ink, the rest dim */
        usbrec_knob(UK_STRT, 2);
    for (s = 0; s < 8u; s++)
        usbrec_knob(UK_LEN, -2);
    usbrec_knob(UK_GAIN, 40);
    usbrec_knob(UK_FADE, 6);
    ui.last = UK_LEN;
    shot(pal, "usbrec_trim");
    press(B_HOME);
    press(B_HOME);
    ui.msg_t = 0;
    sys.playing = 1;
    sys.rec = was_rec;
    ui.force = 1;                                        /* (the page drawn again, as it was before the mode) */
    ui_draw();
    }
    ui.uboot = 3;
    lcd_fill(0, 0, 240, 240, 0);
    draw_head();
    shot(pal, "uboot");
    ui.uboot = 0;
    ui_setup_title();                                    /* HARDWARE CALIBRATION, on the dot grid */
    ui_setup_show("PRESS", "PLAY");
    shot_screen(pal, "calibration");
    ui_setup_show("TURN RIGHT", "SELECT");
    shot_screen(pal, "calibration_turn");
    lcd_fill(0, 0, 240, 240, T_BG);                      /* the one-shots, with main.c's and ota_hw.c's own calls */
    draw_text_box(0, 94, 240, &AF_L, "BRYO", T_THEME, 1);
    draw_text_box(0, 134, 240, &AF_S, "4-TRACK SOUND SCULPTING", T_MID, 1);
    shot_screen(pal, "boot");
    lcd_fill(0, 0, 240, 240, T_BG);
    draw_text_box(0, 92, 240, &AF_M, "UPDATE", T_TEXT, 1);
    draw_text_box(0, 124, 240, &AF_S, "CHECK HEAD", T_THEME, 1);
    shot_screen(pal, "update");
    lcd_fill(0, 0, 240, 240, UI_CRASH_BG);
    draw_text_line(0, 8, 240, &AF_M, "BRYO CRASH", UI_CRASH_INK, UI_CRASH_BG, 1);
    draw_text_line(10, 40, 220, &AF_M, "0000000C", UI_CRASH_INK, UI_CRASH_BG, 0);
    draw_text_line(10, 60, 220, &AF_M, "0201A3F4", UI_CRASH_INK, UI_CRASH_BG, 0);
    shot_screen(pal, "crash");
    lcd_fill(0, 0, 240, 240, 0);
}

/* ---------------------------------------------------------- user reels --- */
static void test_uslots(void)
{
    tape_view_t v;
    power_on();
    check("blank flash: every user slot empty, named U1..U6", uslot_free() == 0 && !uslot_valid(5) &&
          !strcmp(uslot_name[0], "U1") && !strcmp(uslot_name[5], "U6"));
    check("a save: slot 1 holds BEAT under the name 'kick drum.wav' -> KICK",
          uslot_save(0, REELS[0].data, REELS[0].pred, REELS[0].idx, REELS[0].peak, REELS[0].nblk, "kick drum.wav") == 0 &&
          uslot_valid(0) && !strcmp(uslot_name[0], "KICK") && uslot_free() == 1);
    tp[0].dev[DEV_SRC][TK_REEL] = (int16_t)(NREEL + 1u);
    tape_view(0, &v);
    check("..and a track plays it straight from flash (REEL KICK)", v.len == REELS[0].nblk * TAPE_BLK && !v.ram &&
          !memcmp(v.data, REELS[0].data, 64) && !strcmp(tape_name(0), "KICK"));
    check("..REC copies it onto the tape like a factory reel", tape_prepare(0) && tape_src(0) == 0u &&
          tape_ctl[0].nblk == REELS[0].nblk);
    tape_unprepare(0);
    host_prog_limit = 20000;                             /* a power cut halfway through the data */
    check("a save torn by a power cut leaves the slot empty, not half old and half new",
          uslot_save(0, REELS[1].data, REELS[1].pred, REELS[1].idx, REELS[1].peak, REELS[1].nblk, "PIANO") != 0 &&
          !uslot_valid(0) && !strcmp(uslot_name[0], "U1"));
    host_prog_limit = 0xFFFFFFFFu;
    memset(host_nor, 0xFF, sizeof host_nor);
    uslot_names();
}

/* ---------------------------------------------------------------- drive --- */
/* the computer's side: a FAT12 reader and writer that knows only what the disk says */
static uint8_t hd_boot[512], hd_fat[12 * 512], hd_root[64 * 32];
static uint32_t hd_spc, hd_fat0, hd_nfat, hd_fats, hd_root0, hd_data, hd_nclus;
static void hd_mount(void)
{
    uint32_t i;
    vdisk_read(0, hd_boot);
    hd_spc = hd_boot[13];
    hd_fat0 = rd16(hd_boot + 14);
    hd_nfat = hd_boot[16];
    hd_fats = rd16(hd_boot + 22);
    hd_root0 = hd_fat0 + hd_nfat * hd_fats;
    hd_data = hd_root0 + rd16(hd_boot + 17) * 32u / 512u;
    hd_nclus = ((rd16(hd_boot + 19) ? rd16(hd_boot + 19) : rd32(hd_boot + 32)) - hd_data) / hd_spc;
    for (i = 0; i < hd_fats; i++)
        vdisk_read(hd_fat0 + i, hd_fat + 512 * i);
    for (i = 0; i < 4; i++)
        vdisk_read(hd_root0 + i, hd_root + 512 * i);
}
static uint32_t hd_fat_get(uint32_t c) { uint32_t v = rd16(hd_fat + c + c / 2); return c & 1 ? v >> 4 : v & 0xFFF; }
static void hd_fat_set(uint32_t c, uint32_t v)
{
    uint32_t o = c + c / 2, w = rd16(hd_fat + o);
    w = c & 1 ? (w & 0xF) | (v << 4) : (w & 0xF000) | (v & 0xFFF);
    wr16(hd_fat + o, w);
}
static uint8_t *hd_find(const char *n83)
{
    uint32_t i;
    for (i = 0; i < 64; i++)
        if (!memcmp(hd_root + 32 * i, n83, 11))
            return hd_root + 32 * i;
    return 0;
}
/* the file at dir entry e into buf (max n bytes); returns its size */
static uint32_t hd_read_file(const uint8_t *e, uint8_t *buf, uint32_t n)
{
    uint32_t c = rd16(e + 26), size = rd32(e + 28), got = 0, s;
    uint8_t sec[512];
    while (c >= 2 && c < 0xFF8 && got < size) {
        for (s = 0; s < hd_spc && got < size; s++) {
            uint32_t k = size - got < 512 ? size - got : 512;
            vdisk_read(hd_data + (c - 2) * hd_spc + s, sec);
            if (got + k <= n)
                memcpy(buf + got, sec, k);
            got += k;
        }
        c = hd_fat_get(c);
    }
    return size;
}
/* write a file of n bytes: into the clusters of an existing entry e (in place), or new clusters and a new entry
 * named short83 (+ a long name); the data first, then the FAT, then the directory, as file managers tend to */
static void hd_write_file(uint8_t *e, const char *short83, const char *lfn, const uint8_t *data, uint32_t n,
                          int name_it)
{
    uint32_t need = (n + hd_spc * 512 - 1) / (hd_spc * 512), cl[512], k = 0, c, i, w = 0;
    uint8_t sec[512];
    if (e) {
        for (c = rd16(e + 26); c >= 2 && c < 0xFF8 && k < need; c = hd_fat_get(c))
            cl[k++] = c;
    }
    for (c = k ? cl[k - 1] + 1 : 2; k < need && c < hd_nclus + 2; c++)   /* next fit: on from the file's end */
        if (!hd_fat_get(c))
            cl[k++] = c;
    for (i = 0; i < need; i++) {
        uint32_t s;
        for (s = 0; s < hd_spc && w < n; s++, w += 512) {   /* (the file's sectors, not the cluster's slack) */
            memset(sec, 0, 512);
            if (w < n)
                memcpy(sec, data + w, n - w < 512 ? n - w : 512);
            vdisk_write(hd_data + (cl[i] - 2) * hd_spc + s, sec);
            vd_spare_fill();                             /* (the main loop runs between USB packets) */
        }
        hd_fat_set(cl[i], i + 1 < need ? cl[i + 1] : 0xFFF);
    }
    for (i = 0; i < hd_fats; i++) {
        vdisk_write(hd_fat0 + i, hd_fat + 512 * i);
        vdisk_write(hd_fat0 + hd_fats + i, hd_fat + 512 * i);
    }
    if (!name_it)
        return;
    if (!e) {
        uint32_t slot;
        for (slot = 0; slot < 63 && hd_root[32 * slot] && hd_root[32 * slot] != 0xE5; slot++)
            ;
        if (lfn) {                                       /* one long-name entry (up to 13 characters) */
            static const uint8_t AT[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
            uint8_t *l = hd_root + 32 * slot;
            memset(l, 0xFF, 32);
            l[0] = 0x41;
            l[11] = 0x0F;
            l[12] = 0;
            wr16(l + 26, 0);
            for (i = 0; i < 13; i++)
                wr16(l + AT[i], i < strlen(lfn) ? (uint8_t)lfn[i] : i == strlen(lfn) ? 0 : 0xFFFF);
            slot++;
        }
        e = hd_root + 32 * slot;
        memset(e, 0, 32);
        memcpy(e, short83, 11);
    }
    wr16(e + 26, cl[0]);
    wr32(e + 28, n);
    for (i = 0; i < 4; i++)
        vdisk_write(hd_root0 + i, hd_root + 512 * i);
}
/* a WAV: rate, channels, bits, frames of a sine at hz (amplitude 0.5) */
static uint32_t make_wav(uint8_t *o, uint32_t rate, uint32_t ch, uint32_t bits, uint32_t tag, uint32_t frames, double hz)
{
    uint32_t bpf = ch * bits / 8, n = frames * bpf, i, c;
    memcpy(o, "RIFF", 4);
    wr32(o + 4, 36 + 8 + 26 + n);
    memcpy(o + 8, "WAVELIST", 8);                        /* a chunk Bryo skips */
    wr32(o + 16, 18);
    memcpy(o + 20, "INFOISFT\x06\0\0\0bryo\0\0", 18);
    memcpy(o + 38, "fmt ", 4);
    wr32(o + 42, 16);
    wr16(o + 46, tag);
    wr16(o + 48, ch);
    wr32(o + 50, rate);
    wr32(o + 54, rate * bpf);
    wr16(o + 58, bpf);
    wr16(o + 60, bits);
    memcpy(o + 62, "data", 4);
    wr32(o + 66, n);
    for (i = 0; i < frames; i++)
        for (c = 0; c < ch; c++) {
            double v = 0.5 * sin(2 * M_PI * hz * i / rate);
            uint8_t *p = o + 70 + i * bpf + c * bits / 8;
            if (tag == 3) {
                float f = (float)v;
                memcpy(p, &f, 4);
            } else if (bits == 16) {
                wr16(p, (uint16_t)(int16_t)(v * 32767));
            } else if (bits == 24) {
                int32_t x = (int32_t)(v * 8388607);
                p[0] = (uint8_t)x; p[1] = (uint8_t)(x >> 8); p[2] = (uint8_t)(x >> 16);
            }
        }
    return 70 + n;
}
/* the tape or reel's sound: zero crossings per second (its pitch, for a sine) and its peak */
static double view_hz(const tape_view_t *v, int32_t *peak)
{
    tape_rd_t r = {0};
    uint32_t i, cross = 0, n = v->len - 512 > 22050 ? 22050 : v->len - 512;   /* (not the last block's padding) */
    int32_t low = 0, pk = 0;
    for (i = 2000; i < n; i++) {                         /* (with hysteresis: ADPCM noise near 0 is no crossing) */
        int32_t x = tape_at(v, &r, (int32_t)i);
        if (x < -1000)
            low = 1;
        else if (x > 1000 && low)
            cross++, low = 0;
        pk = abs(x) > pk ? abs(x) : pk;
    }
    *peak = pk;
    return cross * 22050.0 / (n - 2000);
}

static void test_drive(void)
{
    static uint8_t buf[400000], wav[1200000];
    uint8_t *e;
    tape_view_t v;
    uint32_t n, i, ok;
    int32_t pk;
    double hz;
    char b[120];
    power_on();
    hd_mount();
    check("the drive: a FAT12 volume BRYO (512 B sectors, 63.5 MiB, 16 KiB clusters, under 4085 clusters)",
          rd16(hd_boot + 11) == 512 && hd_boot[510] == 0x55 && hd_boot[511] == 0xAA && !memcmp(hd_boot + 54, "FAT12", 5) &&
          hd_nclus < 4085 && hd_nclus > 4000 && !memcmp(hd_root, "BRYO       ", 11) && hd_root[11] == 0x08);
    check("..with README.TXT, TAPE1-4.WAV and REEL1-4.WAV (factory reels read-only), no USER files on a blank flash",
          hd_find("README  TXT") && hd_find("TAPE1   WAV") && hd_find("TAPE4   WAV") && hd_find("REEL1   WAV") &&
          (hd_find("REEL4   WAV")[11] & 1) && !hd_find("USER1   WAV"));
    n = hd_read_file(hd_find("README  TXT"), buf, sizeof buf);
    check("README.TXT reads as text", n > 200 && !memcmp(buf, "BRYO", 4));
    n = hd_read_file(hd_find("TAPE1   WAV"), buf, sizeof buf);
    tape_view(0, &v);
    ok = n == 44 + v.len * 2 && !memcmp(buf, "RIFF", 4) && rd32(buf + 24) == 22050 && rd16(buf + 22) == 1 && rd16(buf + 34) == 16;
    {
        tape_rd_t r = {0};
        for (i = 0; i < v.len && ok; i += 97)
            ok = (int16_t)rd16(buf + 44 + 2 * i) == tape_at(&v, &r, (int32_t)i);
    }
    check("TAPE1.WAV is track 1's sound: a 22,050 Hz 16-bit mono WAV, sample for sample", ok);

    n = make_wav(wav, 44100, 2, 16, 1, 44100, 441.0);   /* 1 s of 441 Hz, stereo 16-bit 44.1 kHz, a LIST chunk */
    hd_write_file(0, "KICKDR~1WAV", "kick drum.wav", wav, n, 1);
    vdisk_poll();
    ok = uslot_valid(0) && !strcmp(uslot_name[0], "KICK");
    uslot_view(0, &v);
    hz = view_hz(&v, &pk);
    snprintf(b, sizeof b, "a WAV copied on as 'kick drum.wav' (44.1 kHz stereo) lands in user reel 1 as KICK: %.0f Hz, peak %d", hz, (int)pk);
    check(b, ok && fabs(hz - 441) < 8 && pk > 12000 && pk < 20000 && v.len == 22050u / 256u * 256u + 256u);

    e = hd_find("TAPE2   WAV");                          /* overwrite TAPE2.WAV in place: 24-bit 48 kHz mono */
    n = make_wav(wav, 48000, 1, 24, 1, 48000 * 4, 220.0);
    hd_write_file(e, 0, 0, wav, n, 1);
    vdisk_poll();
    tape_view(1, &v);
    hz = view_hz(&v, &pk);
    snprintf(b, sizeof b, "TAPE2.WAV overwritten (24-bit 48 kHz, 4 s) replaces track 2's tape, all 4 s of it: %.0f Hz", hz);
    check(b, tape_src(1) == 0 && tape_ctl[1].nblk >= 88200u / 256u && tape_ctl[1].nblk <= 88200u / 256u + 1u &&
          fabs(hz - 220) < 6 && pk > 12000);

    n = make_wav(wav, 22050, 1, 32, 3, 11025, 1000.0);   /* float32, into a folder: never named in the root */
    hd_write_file(0, 0, 0, wav, n, 0);
    vdisk_poll();
    check("..a WAV with no name in the root waits", !uslot_valid(1));
    fm1_ms += 3500;
    vdisk_poll();
    uslot_view(1, &v);
    hz = view_hz(&v, &pk);
    snprintf(b, sizeof b, "..then goes to the next free user reel as U2 (float32 read): %.0f Hz", hz);
    check(b, uslot_valid(1) && !strcmp(uslot_name[1], "U2") && fabs(hz - 1000) < 20);

    n = make_wav(wav, 22050, 1, 16, 2, 2000, 100.0);     /* format 2 (MS ADPCM): not readable */
    hd_write_file(0, "BAD     WAV", 0, wav, n, 1);
    ui.msg_t = 0;
    vdisk_poll();
    check("a WAV in a format Bryo can't read is refused, with a message, nothing saved", !uslot_valid(2) && ui.msg_t &&
          !strcmp(ui.msg, "THAT WAV COULDN'T BE READ"));
    {   /* scattered: two runs of clusters with a used cluster between them, the FAT written before the data */
        uint32_t cl[64] = {0}, k = 0, c, w = 0, j, need;
        uint8_t sec[512];
        n = make_wav(wav, 22050, 1, 16, 1, 22050, 300.0);
        need = (n + hd_spc * 512 - 1) / (hd_spc * 512);
        for (c = 2; k < need; c++)
            if (!hd_fat_get(c) && (k != 1 || c > cl[0] + 1))   /* a gap after the first cluster */
                cl[k++] = c;
        hd_fat_set(cl[0] + 1, 0xFFF);                      /* (something else's cluster in the gap) */
        for (j = 0; j < need; j++)
            hd_fat_set(cl[j], j + 1 < need ? cl[j + 1] : 0xFFF);
        for (j = 0; j < hd_fats; j++) {
            vdisk_write(hd_fat0 + j, hd_fat + 512 * j);
            vdisk_write(hd_fat0 + hd_fats + j, hd_fat + 512 * j);
        }
        for (j = 0; j < need; j++)
            for (c = 0; c < hd_spc; c++, w += 512) {
                memset(sec, 0, 512);
                if (w < n)
                    memcpy(sec, wav + w, n - w < 512 ? n - w : 512);
                vdisk_write(hd_data + (cl[j] - 2) * hd_spc + c, sec);
                vd_spare_fill();
            }
        for (j = 0; j < 64 && hd_root[32 * j]; j++)
            ;
        memset(hd_root + 32 * j, 0, 32);
        memcpy(hd_root + 32 * j, "SCATTER WAV", 11);
        wr16(hd_root + 32 * j + 26, cl[0]);
        wr32(hd_root + 32 * j + 28, n);
        for (j = 0; j < 4; j++)
            vdisk_write(hd_root0 + j, hd_root + 512 * j);
        vdisk_poll();
        uslot_view(2, &v);
        hz = view_hz(&v, &pk);
        snprintf(b, sizeof b, "a WAV in two scattered runs (the FAT written first) arrives whole: SCAT, %.0f Hz", hz);
        check(b, uslot_valid(2) && !strcmp(uslot_name[2], "SCAT") && fabs(hz - 300) < 6 && v.len >= 22050u);
    }
    hd_mount();
    check("the computer reads back the FAT and directory as it wrote them", hd_find("KICKDR~1WAV") && hd_find("BAD     WAV"));
    vdisk_mount();
    hd_mount();
    check("plugged in again: USER1.WAV and USER2.WAV are files now", hd_find("USER1   WAV") && hd_find("USER2   WAV") &&
          rd32(hd_find("USER1   WAV") + 28) == 44 + REELS[0].nblk * 0 + 2 * (22050u / 256u * 256u + 256u));
    memset(host_nor, 0xFF, sizeof host_nor);
    uslot_names();
}

/* a WAV with the odd parts: 8 or 16 bits, a chunk of odd length (padded), a fmt chunk of fmtlen bytes (40:
 * WAVE_FORMAT_EXTENSIBLE, subformat sub) */
static uint32_t make_wav_x(uint8_t *o, uint32_t rate, uint32_t bits, uint32_t fmtlen, uint32_t sub, uint32_t frames,
                           double hz)
{
    uint32_t bpf = bits / 8, n = frames * bpf, p = 12, i;
    uint8_t *f;
    memcpy(o, "RIFF\0\0\0\0WAVE", 12);
    memcpy(o + p, "junk", 4);                            /* 7 bytes and a pad byte */
    wr32(o + p + 4, 7);
    memset(o + p + 8, 0x55, 8);
    p += 16;
    memcpy(o + p, "fmt ", 4);
    wr32(o + p + 4, fmtlen);
    f = o + p + 8;
    memset(f, 0, fmtlen);
    wr16(f, fmtlen >= 40 ? 0xFFFE : 1);
    wr16(f + 2, 1);
    wr32(f + 4, rate);
    wr32(f + 8, rate * bpf);
    wr16(f + 12, bpf);
    wr16(f + 14, bits);
    if (fmtlen >= 40) {
        wr16(f + 16, 22);
        wr16(f + 18, bits);
        wr32(f + 20, 4);
        wr16(f + 24, sub);
    }
    p += 8 + fmtlen;
    memcpy(o + p, "data", 4);
    wr32(o + p + 4, n);
    p += 8;
    for (i = 0; i < frames; i++) {
        double v = 0.5 * sin(2 * M_PI * hz * i / rate);
        if (bits == 8)
            o[p + i] = (uint8_t)(128 + (int)(v * 127));
        else
            wr16(o + p + 2 * i, (uint16_t)(int16_t)(v * 32767));
    }
    wr32(o + 4, p + n - 8);
    return p + n;
}

/* ---------------------------------------------------------------- SYNTH --- */
/* a buffer's pitch: cycles per second, counted with hysteresis (from sample `from` on) */
static double buf_hz(const int32_t *x, uint32_t from, uint32_t n)
{
    uint32_t i, cross = 0, first = 0, last = 0;
    int low = 0;
    for (i = from; i < n; i++)
        if (x[i] < -800)
            low = 1;
        else if (x[i] > 800 && low) {
            if (!cross)
                first = i;
            last = i;
            cross++;
            low = 0;
        }
    return cross > 1 ? (cross - 1) * 44100.0 / (last - first) : 0;
}
/* how bright: the mean bend between samples (the second difference: a saw's edge is all bend, a sine has
 * almost none) against the mean level */
static double brightness(const int32_t *x, uint32_t from, uint32_t n)
{
    double d = 0, a = 0;
    uint32_t i;
    for (i = from + 2; i < n; i++) {
        d += abs(x[i] - 2 * x[i - 1] + x[i - 2]);
        a += abs(x[i]);
    }
    return a > 0 ? d / a : 0;
}
static uint32_t syn_sounding(uint32_t t)
{
    uint32_t j, n = 0;
    for (j = 0; j < SYN_NV; j++)
        n += syn[t].v[j].stage != 0;
    return n;
}

static void test_synth(void)
{
    enum { NB = 690 };                                   /* 0.5 s */
    static int32_t s[2 * NB * CTL];                      /* (some checks render two of them) */
    int16_t *p;
    char b[120];
    double hz, br_open, br_shut;
    uint32_t i;
    power_on();
    hold(B_HOME);                                        /* HOME held + SELECT: the source */
    host_enc[panel.enc[EN_SELECT]] = 1;
    ui_input();
    let_go(B_HOME);
    p = tp[0].syn;
    check("HOME held + SELECT: track 1's source is SYNTH, its pages and name follow", tp[0].src == SRC_SYNTH &&
          !strcmp(dev_name(0, DEV_SRC), "SYNTH") && pdesc_pages(dev_p(0, DEV_SRC)) == 6u && ui.page == 0 &&
          dev_v(0, DEV_SRC) == p && !strcmp(dev_p(0, DEV_SRC)[0].label, "WAVE"));
    {
        int32_t pk = 0;
        for (i = 0; i < NB; i++) {                       /* (the track's own output: the master may hold a DC tail) */
            render(1, 0);
            pk = peak_of(track_rt[0].last, CTL) > pk ? peak_of(track_rt[0].last, CTL) : pk;
        }
        check("SYNTH, no key down: silence", pk == 0);
    }

    p[SY_WAVE] = 0;                                      /* a plain sine, the filter open */
    p[SY_DTUN] = 0;
    p[SY_MIX] = 0;
    p[SY_CUT] = 127;
    p[SY_ENV] = 0;
    p[SY_RES] = 0;
    fm1_in.notes = note_bit_of_white(9);                 /* white key 10: A3 at OCT 3 */
    render(NB, s);
    hz = buf_hz(s, NB * CTL / 4, NB * CTL);
    snprintf(b, sizeof b, "white key 10 at OCT 3 plays A3: %.1f Hz (220), peak %d", hz, (int)peak_of(s, NB * CTL));
    check(b, fabs(hz - 220.0) < 1.5 && peak_of(s, NB * CTL) > 4000 && syn_sounding(0) == 1);
    fm1_in.notes = 0;
    render(NB * 2, s);
    check("..let go: it releases to silence and the voice is free", syn_sounding(0) == 0 &&
          peak_of(s + NB * CTL, NB * CTL) < 40);

    track[0].octave = 4;                                 /* OCT+: an octave up */
    fm1_in.notes = note_bit_of_white(9);
    render(NB, s);
    hz = buf_hz(s, NB * CTL / 4, NB * CTL);
    snprintf(b, sizeof b, "OCT 4: the same key an octave up: %.1f Hz (440)", hz);
    check(b, fabs(hz - 440.0) < 3.0);
    fm1_in.notes = 0;
    render(NB * 2, 0);
    track[0].octave = 3;

    fm1_in.notes = note_bit_of_white(0) | note_bit_of_white(4) | note_bit_of_white(7);   /* a C major chord */
    render(8, 0);
    check("three keys: three voices", syn_sounding(0) == 3);
    fm1_in.notes |= note_bit_of_white(11);               /* a fourth: the oldest voice is taken */
    render(8, 0);
    {
        uint32_t j, has11 = 0;
        for (j = 0; j < SYN_NV; j++)
            has11 |= syn[0].v[j].key == 11 && syn[0].v[j].stage && syn[0].v[j].stage != 3;
        check("..a fourth key takes a voice (no more than three sound)", syn_sounding(0) == 3 && has11);
    }
    fm1_in.notes = 0;
    render(NB * 2, 0);

    p[SY_VOIC] = 1;                                      /* one voice: legato, gliding */
    p[SY_GLID] = 60;
    fm1_in.notes = note_bit_of_white(0);
    render(20, 0);
    fm1_in.notes |= note_bit_of_white(12);
    render(4, 0);
    {
        int32_t mid = syn[0].v[0].p16;
        render(NB, 0);
        check("VOIC 1: a second key glides the one voice up an octave, no new attack",
              syn_sounding(0) == 1 && mid > 48 * 16 && mid < 60 * 16 && syn[0].v[0].p16 == 60 * 16 &&
              syn[0].v[0].stage == 2);
    }
    fm1_in.notes = note_bit_of_white(0);                 /* the top key up: back down to the one still held */
    render(NB, 0);
    check("..the top key let go: back to the key still held", syn[0].v[0].p16 == 48 * 16 && syn_sounding(0) == 1);
    fm1_in.notes = 0;
    render(NB * 2, 0);
    p[SY_VOIC] = SYN_NV;
    p[SY_GLID] = 0;

    {   /* every wave at the key's pitch; noise and drive stay under full scale */
        uint32_t w, ok = 1;
        char m[64] = "";
        for (w = 0; w < 5u; w++) {
            p[SY_WAVE] = (int16_t)w;
            fm1_in.notes = note_bit_of_white(9);
            render(NB, s);
            hz = buf_hz(s, NB * CTL / 4, NB * CTL);
            ok &= fabs(hz - 220.0) < 1.5;
            snprintf(m + strlen(m), sizeof m - strlen(m), " %s %.0f", N_OSC[w], hz);
            fm1_in.notes = 0;
            render(NB * 2, 0);
        }
        snprintf(b, sizeof b, "every wave plays A3:%s", m);
        check(b, ok);
        p[SY_WAVE] = 3;
        p[SY_NOIS] = 100;
        p[SY_DRV] = 100;
        p[SY_MIX] = 50;
        p[SY_DTUN] = 30;
        fm1_in.notes = note_bit_of_white(0) | note_bit_of_white(4) | note_bit_of_white(7);
        render(NB, s);
        check("a full-noise, full-drive, detuned chord sounds, and the track stays under full scale",
              peak_of(s, NB * CTL) > 8000 && peak_of(s, NB * CTL) <= 32767);
        p[SY_VOIC] = 1;                                  /* VOIC turned down under three held keys */
        render(4, 0);
        check("..VOIC turned down to 1 while three sound: the voices above it release", syn[0].v[1].stage == 3 &&
              syn[0].v[2].stage == 3 && syn[0].v[0].stage == 2);
        p[SY_VOIC] = SYN_NV;
        fm1_in.notes = 0;
        render(2, 0);
        fm1_in.notes = note_bit_of_white(12);            /* all three releasing: the quietest is taken */
        render(1, 0);
        {
            uint32_t j, n12 = 0, rel = 0;
            for (j = 0; j < SYN_NV; j++) {
                n12 += syn[0].v[j].key == 12 && syn[0].v[j].stage == 2;
                rel += syn[0].v[j].stage == 3;
            }
            check("..a key while all three release: it takes one of them, the other two go on releasing",
                  n12 == 1 && rel == 2);
        }
        fm1_in.notes = 0;
        render(NB * 2, 0);
        p[SY_NOIS] = 0;
        p[SY_DRV] = 0;
        p[SY_MIX] = 0;
        p[SY_DTUN] = 0;
    }
    p[SY_VOIC] = 1;                                      /* legato without glide: the pitch jumps, no attack */
    fm1_in.notes = note_bit_of_white(0);
    render(20, 0);
    fm1_in.notes |= note_bit_of_white(5);
    render(1, 0);
    check("VOIC 1, GLID 0: a second key jumps the pitch at once, the envelope goes on",
          syn[0].v[0].p16 == 53 * 16 && syn[0].v[0].stage == 2);
    fm1_in.notes = note_bit_of_white(5);
    render(1, 0);
    check("..the lower key let go: the voice stays on the one still held", syn[0].v[0].p16 == 53 * 16 &&
          syn[0].v[0].key == 5);
    fm1_in.notes = 0;
    render(NB * 2, 0);
    p[SY_VOIC] = SYN_NV;

    p[SY_WAVE] = 3;                                      /* a saw through the filter, open then nearly shut */
    fm1_in.notes = note_bit_of_white(0);
    render(NB, s);
    br_open = brightness(s, NB * CTL / 2, NB * CTL);
    fm1_in.notes = 0;
    render(NB * 2, 0);
    p[SY_CUT] = 40;
    fm1_in.notes = note_bit_of_white(0);
    render(NB, s);
    br_shut = brightness(s, NB * CTL / 2, NB * CTL);
    snprintf(b, sizeof b, "the filter: a saw is darker with CUT down (brightness %.3f -> %.3f)", br_open, br_shut);
    check(b, br_shut < br_open * 0.5 && peak_of(s, NB * CTL) > 1000);
    fm1_in.notes = 0;
    render(NB * 2, 0);
    p[SY_CUT] = SYN_P[SY_CUT].def;
    p[SY_WAVE] = 0;
    p[SY_CUT] = 127;

    {   /* REC on a SYNTH track prints the synth onto its tape: then TAPE plays it back */
        tape_view_t v;
        int32_t pk;
        tp[0].dev[DEV_SRC][TK_DUB] = -100;                   /* (DUB -100: the input replaces the loop) */
        press(B_REC);
        check("REC on a SYNTH track arms its tape", (sys.rec & 1u) && tape_ctl[0].rec_ok);
        sys.playing = 1;
        fm1_in.notes = note_bit_of_white(9);
        render(NB * 6, 0);                               /* 3 s: past the 2 s loop */
        press(B_REC);
        render(2, 0);
        fm1_in.notes = 0;
        sys.playing = 0;
        render(NB, 0);
        tape_view(0, &v);
        hz = view_hz(&v, &pk);
        snprintf(b, sizeof b, "..the take is the note: %.1f Hz on the tape (220), peak %d", hz, (int)pk);
        check(b, fabs(hz - 220.0) < 1 && pk > 3000);
    }
    {   /* back to TAPE while both sound: a 2 ms crossfade, no step */
        int32_t step = 0, before = 0;
        sys.playing = 1;
        fm1_in.notes = note_bit_of_white(9);
        render(NB, s);
        for (i = NB * CTL / 2; i < NB * CTL; i++)
            before = abs(s[i] - s[i - 1]) > before ? abs(s[i] - s[i - 1]) : before;
        tp[0].src = SRC_TAPE;
        render(8, s);
        for (i = 1; i < 8 * CTL; i++)
            step = abs(s[i] - s[i - 1]) > step ? abs(s[i] - s[i - 1]) : step;
        snprintf(b, sizeof b, "SYNTH -> TAPE while both sound: a crossfade (largest step %d, the note's own %d)",
                 (int)step, (int)before);
        check(b, track_rt[0].src_g[SRC_SYNTH] == 0 && track_rt[0].src_g[SRC_TAPE] == 32767 && step < before * 3);
        fm1_in.notes = 0;
        render(4, 0);
        tp[0].src = SRC_SYNTH;
        render(NB * 2, 0);
        check("..switched back, the note held across the switch is seen up and released", syn_sounding(0) == 0);
        sys.playing = 0;
        tp[0].src = SRC_TAPE;
        render(NB / 2, 0);
    }
}

/* ----------------------------------------------------------------- POLY --- */
/* user reel `slot`: a sine of hz, nblk blocks, encoded as a recording is (tape.c tape_commit) */
static void make_sine_reel(uint32_t slot, double hz, uint32_t nblk)
{
    static uint8_t data[TAPE_LEN / 2];
    static int16_t pred[TAPE_NBLK];
    static uint8_t idx[TAPE_NBLK], peak[TAPE_NBLK];
    int16_t blk[TAPE_BLK];
    uint32_t b, i;
    for (b = 0; b < nblk; b++) {
        int32_t d, id = 0;
        for (i = 0; i < TAPE_BLK; i++)
            blk[i] = (int16_t)(16000.0 * sin(2 * M_PI * hz * (b * TAPE_BLK + i) / TAPE_SR));
        d = abs(blk[1] - blk[0]);
        while (id < 88 && IMA_STEP[id] < d)
            id++;
        pred[b] = blk[0];
        idx[b] = (uint8_t)id;
        peak[b] = 16000 >> 7;
        ima_enc(blk, pred[b], idx[b], data + b * (TAPE_BLK / 2), TAPE_BLK);
    }
    uslot_save(slot, data, pred, idx, peak, nblk, "SINE");
}
static uint32_t pol_sounding(uint32_t t)
{
    uint32_t j, n = 0;
    for (j = 0; j < POL_NV; j++)
        n += pol[t].v[j].stage != 0;
    return n;
}
/* white key k held for NB blocks into s, then let go and silent again; returns the pitch */
static double pol_note(uint32_t k, int32_t *s, uint32_t nb)
{
    double hz;
    fm1_in.notes = note_bit_of_white(k);
    render(nb, s);
    hz = buf_hz(s, nb * CTL / 4, nb * CTL);
    fm1_in.notes = 0;
    render(nb * 2, 0);
    return hz;
}

static void test_poly(void)
{
    enum { NB = 690 };                                   /* 0.5 s */
    static int32_t s[2 * NB * CTL];                      /* (some checks render two of them) */
    int16_t *p;
    char b[120];
    double h0, h7, h12;
    int32_t pk_open, pk;
    uint32_t i;
    power_on();
    make_sine_reel(0, 440.0, 200);                       /* user reel 1: 2.3 s of A4 */
    hold(B_HOME);                                        /* HOME held + SELECT, twice round: POLY */
    host_enc[panel.enc[EN_SELECT]] = 2;
    ui_input();
    let_go(B_HOME);
    p = tp[0].pol;
    check("HOME held + SELECT: POLY, six pages (SAMPLE LOOP ENV FILTER TWIN LEVEL), its own knobs", tp[0].src == SRC_POLY &&
          !strcmp(dev_name(0, DEV_SRC), "POLY") && pdesc_pages(dev_p(0, DEV_SRC)) == 6u && dev_v(0, DEV_SRC) == p &&
          p[PL_REEL] == 1);
    {
        int32_t q = 0;
        for (i = 0; i < NB; i++) {
            render(1, 0);
            q = peak_of(track_rt[0].last, CTL) > q ? peak_of(track_rt[0].last, CTL) : q;
        }
        check("POLY, no key down: silence", q == 0);
    }
    p[PL_REEL] = (int16_t)(NREEL + 1u);                  /* the sine */
    p[PL_ROOT] = 48;                                     /* (white key 1 plays it as recorded; every key as it is) */
    keyp[KY_SCAL] = 0;
    h0 = pol_note(0, s, NB);
    pk_open = peak_of(s, NB * CTL);
    h7 = pol_note(7, s, NB);
    h12 = pol_note(12, s, NB);
    snprintf(b, sizeof b, "white key 1 plays the sound as recorded (%.1f Hz), key 8 a fifth up (%.1f), key 13 "
             "an octave (%.1f)", h0, h7, h12);
    check(b, fabs(h0 - 440.0) < 2 && fabs(h7 - 659.3) < 3 && fabs(h12 - 880.0) < 4 && pk_open > 3000);
    check("..each note let go releases and frees its voice", pol_sounding(0) == 0);

    fm1_in.notes = note_bit_of_white(0) | note_bit_of_white(4) | note_bit_of_white(7) | note_bit_of_white(11);
    render(8, 0);
    check("four keys: four voices", pol_sounding(0) == 4);
    fm1_in.notes |= note_bit_of_white(14);
    render(8, 0);
    {
        uint32_t j, has = 0;
        for (j = 0; j < POL_NV; j++)
            has |= pol[0].v[j].key == 14 && pol[0].v[j].stage && pol[0].v[j].stage != 3;
        check("..a fifth takes the oldest (no more than four sound)", pol_sounding(0) == 4 && has);
    }
    p[PL_VOIC] = 2;
    render(2, 0);
    check("..VOIC turned down to 2: the voices above it let go", pol[0].v[2].stage == 3 && pol[0].v[3].stage == 3);
    fm1_in.notes = 0;
    render(NB * 2, 0);
    p[PL_VOIC] = POL_NV;

    p[PL_STRT] = 5000;                                     /* STRT 50: every note from half way */
    fm1_in.notes = note_bit_of_white(0);
    render(1, 0);
    check("STRT 50: a note starts half way through the sound",
          abs((pol[0].v[0].pos >> 12) - 200 * (int32_t)TAPE_BLK / 2) < 32);
    {
        uint32_t ran = 0;
        while (pol_sounding(0) && ran < NB * 4u) {
            render(1, 0);
            ran++;
        }
        snprintf(b, sizeof b, "..held, it plays to the sound's end, fades and frees the voice (%u blocks, key still "
                 "down)", ran);
        check(b, !pol_sounding(0) && abs((int32_t)ran - 200 * 256 / 2 * 2 / CTL) < 8 && fm1_in.notes);   /* (half the reel, 2 outputs a sample) */
    }
    fm1_in.notes = 0;
    render(NB, 0);
    p[PL_STRT] = 0;

    p[PL_CUT] = 30;                                      /* the filter: LP low, HP high, BP on the note */
    fm1_in.notes = note_bit_of_white(0);
    render(NB, s);
    pk = peak_of(s + NB * CTL / 2, NB * CTL / 2);
    fm1_in.notes = 0;
    render(NB * 2, 0);
    snprintf(b, sizeof b, "TYPE LP, CUT 30: the sine falls (peak %d, open %d)", (int)pk, (int)pk_open);
    check(b, pk < pk_open / 4);
    p[PL_TYPE] = 2;
    p[PL_CUT] = 110;
    fm1_in.notes = note_bit_of_white(0);
    render(NB, s);
    pk = peak_of(s + NB * CTL / 2, NB * CTL / 2);
    fm1_in.notes = 0;
    render(NB * 2, 0);
    snprintf(b, sizeof b, "TYPE HP, CUT 110: the sine falls (peak %d)", (int)pk);
    check(b, pk < pk_open / 4);
    p[PL_TYPE] = 0;
    p[PL_CUT] = 127;

    p[PL_REEL] = 0;                                      /* the track's own tape: empty, then a reel copied in */
    fm1_in.notes = note_bit_of_white(0);
    render(NB / 2, s);
    pk = peak_of(s, NB / 2 * CTL);
    fm1_in.notes = 0;
    render(NB, 0);
    tape_prepare(0);
    tape_unprepare(0);
    fm1_in.notes = note_bit_of_white(0);
    render(NB / 2, s);
    fm1_in.notes = 0;
    render(NB, 0);
    check("REEL TAPE: an empty tape plays nothing; once it holds a sound, the keys play it",
          pk == 0 && peak_of(s, NB / 2 * CTL) > 2000);
    tp[0].src = SRC_TAPE;
    render(NB / 2, 0);
    memset(host_nor, 0xFF, sizeof host_nor);
    uslot_names();
}

/* ---------------------------------------------------------------- GRAIN --- */
static uint32_t zero_key_bit(void)
{
    uint32_t n;
    for (n = 0; n < 27u; n++)
        if (KEY_BLACK[n] == BK_ZERO)
            return 1u << n;
    return 0;
}

static void test_grain(void)
{
    enum { NB = 690 };                                   /* 0.5 s */
    static int32_t s[2 * NB * CTL];                      /* (some checks render two of them) */
    int16_t *p;
    char b[120];
    uint32_t i, t, ok, maxc = 0;
    int32_t c0;
    power_on();
    make_sine_reel(0, 440.0, 200);                       /* user reel 1: 2.3 s of A4 (440 Hz) */
    tp[0].dev[DEV_SRC][TK_REEL] = (int16_t)(NREEL + 1u);
    p = tp[0].dev[DEV_GRAIN];
    p[GP_SCAN] = SCAN_TAPE;                              /* (these read the tape: the buffer is test_grain_buffer's) */
    {   /* a grain's own decoder against the tape's block reader, both ways */
        tape_view_t v;
        tape_rd_t rd = {0};
        grain_t g;
        int32_t k, pos;
        tape_view(0, &v);
        memset(&g, 0, sizeof g);
        g.wlo = -1;
        g.inc = 4096;
        for (k = 0, ok = 1, pos = 300; k < 3000; k++, pos += 3)  /* forward, over block boundaries */
            ok &= gr_read(&g, &v, pos << 12) == tape_at(&v, &rd, pos);
        g.wlo = -1;
        g.inc = -4096;
        for (k = 0, pos = 20000; k < 3000; k++, pos -= 5)        /* backwards */
            ok &= gr_read(&g, &v, pos << 12) == tape_at(&v, &rd, pos);
        check("a grain decodes the tape itself, forwards and backwards, exactly as the tape's reader does", ok);
    }
    for (t = 1; t < NTRK; t++)                           /* (only track 1 sounds) */
        track[t].mute = 1;
    sys.playing = 1;
    render(NB, s);
    check("WET 0 (the default): no grains, the track as it was", grain_count(0) == 0 && peak_of(s, NB * CTL) > 2000);
    p[4] = 100;                                          /* WET 100: grains only */
    sys.playing = 0;
    render(NB, 0);
    check("stopped, no key: no grains start", grain_count(0) == 0);
    {   /* one grain's output, sample for sample: the tape at its read position, through its window and its pan */
        static int32_t dry[CTL], gl[CTL], gr[CTL];
        tape_view_t v;
        tape_rd_t rd = {0};
        uint32_t blk, bad = 0, n = 0;
        p[1] = 0;                                        /* (one grain at a time, 500 ms, from the cursor) */
        p[0] = 500;
        p[5] = 0;
        grain_kill(0);
        grain[0].wait = 0;
        grain[0].cur = 0;
        sys.playing = 1;
        gr_plan();                                       /* (its share, now it plays) */
        tape_view(0, &v);
        grain_block(0, dry, 0, 0, gl, gr, CTL);
        {
            int32_t xl[3 + CTL / 2] = {0}, xr[3 + CTL / 2] = {0};   /* the model: half-rate samples after the last
                                                                     * block's three, then the cubic's 2x up */
            for (blk = 0; blk < 600u; blk++) {
                grain_t g = gslot[0];
                int on = gr_on(0) != 0u;
                int32_t m = on ? (int32_t)(g.left < CTL ? g.left : CTL) : 0, h = (m + 1) / 2, e0, e1, j;
                grain_block(0, dry, 0, 0, gl, gr, CTL);
                for (j = 0; j < 3; j++) {
                    xl[j] = xl[CTL / 2 + j];
                    xr[j] = xr[CTL / 2 + j];
                }
                for (j = 0; j < (int32_t)CTL / 2; j++)
                    xl[3 + j] = xr[3 + j] = 0;
                e0 = on ? gr_env(&g, g.len - g.left) : 0;
                e1 = on ? gr_env(&g, g.len - g.left + (uint32_t)m) : 0;
                for (j = 0; j < h; j++, n += 2) {        /* (the window: its block ends, ramped across the block) */
                    int32_t k = g.pos >> 12, f = g.pos & 4095, a = tape_at(&v, &rd, k), bb = tape_at(&v, &rd, k + 1), x;
                    int32_t e = ((e0 << 5) + j * (((e1 - e0) * 32) >> (CTL_LOG2 - 1))) >> 5;
                    x = (a + (((bb - a) * f) >> 12)) * e >> 15;
                    xl[3 + j] = (x * (g.gl * 100 / 100)) >> 15;
                    xr[3 + j] = (x * (g.gr * 100 / 100)) >> 15;
                    g.pos += 2 * g.inc;
                }
                for (j = 0; j < (int32_t)CTL / 2; j++) {
                    bad += gl[2 * j] != xl[j + 1] || gr[2 * j] != xr[j + 1];
                    bad += gl[2 * j + 1] != (9 * (xl[j + 1] + xl[j + 2]) - xl[j] - xl[j + 3]) >> 4;
                    bad += gr[2 * j + 1] != (9 * (xr[j + 1] + xr[j + 2]) - xr[j] - xr[j + 3]) >> 4;
                }
            }
        }
        check("one grain, sample for sample: the tape where it reads (at half rate), through its window and pan, then 2x up", bad == 0 && n > 15000);
        {                                               /* the half-rate mix's tail goes out after the last grain ends */
            int32_t sum = 0;
            grain_kill(0);
            grain[0].hl[0] = grain[0].hl[1] = grain[0].hl[2] = 4000;
            grain[0].hr[0] = grain[0].hr[1] = grain[0].hr[2] = 4000;
            grain[0].hq = 0;
            grain[0].wait = 1 << 30;
            grain_block(0, dry, 0, 0, gl, gr, CTL);
            for (i = 0; i < 6u; i++)
                sum += gl[i];
            check("..no grains left: the half-rate mix's last samples still go out (then nothing to carry)",
                  sum == 14000 && gl[0] == 4000 && gl[1] == 4250 && gl[2] == 4000 && gl[3] == 2000 && gl[4] == 0 &&
                  gl[5] == -250 &&                      /* (the cubic's small undershoot past a step) */
                  grain[0].hq == 1u);
            grain[0].wait = 0;
        }
    }
    sys.playing = 0;                                     /* pitch: one grain at a time, measured inside it */
    render(NB * 2, 0);
    {
        static const int16_t ST[3] = {0, 12, 0};
        double got[3];
        uint32_t k;
        p[3] = 0;
        for (k = 0; k < 3; k++) {
            p[2] = ST[k];
            p[7] = k == 2 ? 100 : 0;                     /* (the third backwards) */
            grain[0].wait = 0;
            grain[0].cur = 0;
            sys.playing = 1;
            render(600, s);                              /* 0.44 s of the first 0.5 s grain */
            got[k] = buf_hz(s, 40 * CTL, 600 * CTL);
            sys.playing = 0;
            render(NB * 2, 0);
        }
        snprintf(b, sizeof b, "a grain's pitch: PTCH 0 %.1f Hz, PTCH +12 %.1f, backwards %.1f (440, 880, 440)", got[0],
                 got[1], got[2]);
        check(b, fabs(got[0] - 440) < 2 && fabs(got[1] - 880) < 3 && fabs(got[2] - 440) < 2);
        p[2] = 0;
        p[7] = 0;
        p[3] = 30;
        p[5] = 20;
    }
    p[1] = 40;
    p[0] = 80;
    sys.playing = 1;
    render(NB, s);
    check("WET 100, playing, the defaults: grains sound", grain_count(0) > 0 && peak_of(s, NB * CTL) > 1000);

    {   /* SPRD, at GRAIN's own output (the master's DC blocker remembers each side for seconds) */
        static int32_t dry[CTL], gl[CTL], gr[CTL];
        uint32_t diff0 = 0, diff1 = 0, j;
        p[3] = 0;
        render(NB, 0);                                   /* (the grains placed before it finish) */
        for (i = 0; i < NB; i++) {
            grain_block(0, dry, 0, 0, gl, gr, CTL);
            for (j = 0; j < CTL; j++)
                diff0 += gl[j] != gr[j];
        }
        p[3] = 100;
        for (i = 0; i < NB; i++) {
            grain_block(0, dry, 0, 0, gl, gr, CTL);
            for (j = 0; j < CTL; j++)
                diff1 += gl[j] != gr[j];
        }
        check("SPRD 0: every grain in the centre (left = right); SPRD 100: placed across the stereo field",
              diff0 == 0 && diff1 > NB * CTL / 2);
        p[3] = 30;
    }

    p[1] = 100;                                          /* RATE 100, SIZE 500: as many as the cap allows */
    p[0] = 500;
    for (i = 0; i < NB; i++) {
        render(1, 0);
        maxc = grain_count(0) > maxc ? grain_count(0) : maxc;
    }
    render(NB, s);
    check("RATE 100, SIZE 500, GRAIN on this track alone: up to the whole pool of 40, never more, under full scale",
          maxc == 40u && peak_of(s, NB * CTL) <= 32767);
    grain_shed();
    for (i = 0, maxc = 0; i < 1300u; i++) {
        render(1, 0);
        maxc = grain_count(0) > maxc ? grain_count(0) : maxc;
    }
    check("shedding shrinks the pool a quarter (40 -> 30): the grains follow it down", grain_cap == 6u &&
          gr_allow(0) == 30u && grain_count(0) <= 30u);
    sys.load_pk = 80;                                    /* (the heaviest half of the last second: 80 %) */
    render(1400, 0);
    check("..a second later with no room (the heaviest half 80 %): nothing back yet, or it would go over again",
          grain_cap == 6u && gr_allow(0) == 30u);
    sys.load_pk = 50;
    render(1400, 0);
    check("..and a second with room (50 %) gives an eighth back (35)", grain_cap == 7u && gr_allow(0) == 35u);
    sys.load_pk = 0;
    grain_cap = GR_CAP;
    p[1] = 40;
    p[0] = 80;

    c0 = grain[0].cur;                                   /* WARP 100: the cursor runs at the tape's pace */
    render(100, 0);
    check("WARP 100: the cursor moves through the loop at the tape's pace",
          abs(((grain[0].cur - c0) >> 12) - 100 * CTL / 2) < 4);
    fm1_in.notes = zero_key_bit();                       /* the 0 key held: frozen */
    c0 = grain[0].cur;
    render(100, 0);
    check("the 0 black key held: every cursor frozen", grain[0].cur == c0 && grain_count(0) > 0);
    fm1_in.notes = 0;
    p[GP_WARP] = -100;
    c0 = grain[0].cur;
    render(10, 0);
    check("WARP -100: the cursor runs backwards", ((c0 - grain[0].cur) >> 12) == 10 * CTL / 2);
    p[GP_WARP] = 0;
    c0 = grain[0].cur;
    render(10, 0);
    check("WARP 0: still", grain[0].cur == c0);
    p[GP_WARP] = 100;

    p[GP_SCAL] = 2;                                      /* SCAL MAJ, PRND 12: every pitch on the major scale */
    p[GP_PRND] = 12;
    for (i = 0, ok = 1; i < NB; i++) {
        uint32_t j;
        render(1, 0);
        for (j = 0; j < GR_SLOTS; j++)
            if (gr_on(j) && gslot[j].trk == 0) {
                int32_t st = gslot[j].st16;
                ok &= st % 16 == 0 && ((0xAB5u >> (uint32_t)(((st / 16) % 12 + 12) % 12)) & 1u) && abs(st) <= 12 * 16;
            }
    }
    check("SCAL MAJ, PRND 12: every grain's pitch a major-scale semitone within an octave", ok);
    p[GP_SCAL] = 1;                                      /* SCAL KEY: the project's scale (A MIN at power-on) */
    keyp[KY_SCAL] = 13;                                  /* .. set to WHOL: even semitones only */
    for (i = 0, ok = 1; i < NB; i++) {
        uint32_t j;
        render(1, 0);
        for (j = 0; j < GR_SLOTS; j++)
            if (gr_on(j) && gslot[j].trk == 0)
                ok &= gslot[j].st16 % 32 == 0;
    }
    check("SCAL KEY, the project's SCAL WHOL: every grain's shift an even number of semitones", ok);
    keyp[KY_SCAL] = 0;                                   /* KEY's SCAL OFF: KEY keeps every semitone */
    for (i = 0, ok = 1, maxc = 0; i < NB; i++) {
        uint32_t j;
        render(1, 0);
        for (j = 0; j < GR_SLOTS; j++)
            if (gr_on(j) && gslot[j].trk == 0) {
                ok &= gslot[j].st16 % 16 == 0;
                maxc += gslot[j].st16 % 32 != 0;
            }
    }
    check("..with KEY's SCAL OFF: whole semitones, odd ones too", ok && maxc > 0);
    maxc = 0;
    keyp[KY_SCAL] = 2;
    check("..GRAIN's list: OFF KEY then the 15 scales, MIN still 3", DEV_P[DEV_GRAIN][GP_SCAL].max == 1 + NSCALE &&
          !strcmp(N_GSCAL[3], "MIN") && !strcmp(N_GSCAL[16], "CHR") && gr_mask(16) == 0xFFFu && gr_mask(0) == 0u);
    p[GP_SCAL] = 0;
    p[GP_PRND] = 0;

    sys.playing = 0;                                     /* the GRAIN page, stopped: a key plays its slice */
    render(NB, 0);
    press(B_EDIT);
    ui_input();
    check("the GRAIN page up: the keys go to GRAIN", sys.keys_grain == 1);
    fm1_in.notes = note_bit_of_white(4);
    render(4, 0);
    {
        tape_view_t v;
        int32_t ls, ll;
        tape_view(0, &v);
        tape_window(0, v.len, &ls, &ll);
        check("..white key 5: the cursor to slice 5, grains while it's held, the tape's head left alone",
              abs((grain[0].cur >> 12) - (ls + ll * 4 / 16)) <= 4 * CTL / 2 && grain_count(0) > 0 &&
              !tape_rt[0].running);   /* (WARP moves it on from there) */
    }
    fm1_in.notes = 0;
    render(NB, 0);
    check("..let go: the grains finish, no new ones", grain_count(0) == 0);
    press(B_HOME);
    ui_input();
    check("another page: the keys go back to the source", sys.keys_grain == 0);

    tp[0].dev[DEV_SRC][TK_REEL] = (int16_t)(NREEL + 2u); /* an empty user reel: nothing to granulate */
    sys.playing = 1;
    render(NB, 0);
    check("nothing on the tape: no grains", grain_count(0) == 0);
    sys.playing = 0;
    for (t = 0; t < NTRK; t++)
        track[t].mute = 0;
    render(NB, 0);
    memset(host_nor, 0xFF, sizeof host_nor);
    uslot_names();
}

/* blocks of audio with the main loop's bookkeeping between them (as main.c runs it) */
static void render_poll(uint32_t blocks)
{
    uint32_t b;
    for (b = 0; b < blocks; b++) {
        render(1, 0);
        chain_poll();
    }
}

/* track t's tape holds n chunks of tone (a take: its length n chunks) */
static void fill_tape(uint32_t t, uint32_t n)
{
    uint32_t b;
    tape_free(t);
    tape_reserve(t, n, 1);
    for (b = 0; b < (uint32_t)tape_ctl[t].nch * MEM_CB; b++)
        tape_chunk(t, b)->peak[b % MEM_CB] = 100;
    tape_ctl[t].nblk = (uint16_t)(tape_ctl[t].nch * MEM_CB);
    tape_ctl[t].empty = 0;
    tp[t].dev[DEV_SRC][TK_REEL] = 0;
}

static uint32_t led_lit;                                 /* (test_controls_more's LED reader, below) */
static void led_on(const uint8_t *a, uint32_t id);

/* GRAIN's live buffer: its bars, what it records, freeze, SCAN */
static uint32_t buf_sum(uint32_t t)                      /* a fingerprint of a buffer's data */
{
    uint32_t c, b, h = 0;
    for (c = 0; c < gbuf[t].nch; c++)
        for (b = 0; b < MEM_CB; b++)
            h = h * 31u + mem_at(gbuf[t].map[c])->data[b][7] + mem_at(gbuf[t].map[c])->peak[b];
    return h;
}
static uint32_t buf_loud(uint32_t t, uint32_t from, uint32_t to)   /* blocks of buffer t in [from, to) above a whisper */
{
    tape_view_t v;
    uint32_t b, n = 0;
    gr_buf_view(t, &v);
    for (b = from; b < to && b < v.len / TAPE_BLK; b++)
        n += tv_peak(&v, b) > 20u;
    return n;
}

/* no clicks in the grains: a 201.3 Hz sine (it doesn't fit the bar, so the live buffer has a real seam at its write
 * head, as music does) through GRAIN, the output's second difference never above 600 (a corner: a click; a sine this
 * loud bends at most about 90 a sample). The cases that clicked: a grain off the buffer's end, one crossing the write
 * head or reading the block being written, WET to 0 cutting the grains */
static int32_t clk_p1, clk_p2;
static double clk_ph;
static uint32_t clk_run(uint32_t blocks, int count)
{
    static int32_t dry[CTL], l[CTL], r[CTL];
    uint32_t b, i, nk = 0;
    for (b = 0; b < blocks; b++) {
        for (i = 0; i < CTL; i++, clk_ph += 2 * 3.14159265358979 * 201.3 / 44100)
            dry[i] = (int32_t)(8000 * sin(clk_ph));
        gr_plan();
        grain_block(0, dry, 0, 0, l, r, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t d2 = l[i] - 2 * clk_p1 + clk_p2;
            nk += count && (d2 > 600 || d2 < -600);
            clk_p2 = clk_p1;
            clk_p1 = l[i];
        }
    }
    return nk;
}
static void test_grain_clicks(void)
{
    static const struct { const char *what; int16_t k[3], v[3], k2, v2; } C[] = {
        {"CONT 0 (2 ms ends)", {GP_CONT, -1, -1}, {0}, -1, 0},
        {"PTCH +12 (reading past the write head)", {GP_PTCH, GP_CONT, -1}, {12, 50}, -1, 0},
        {"REV 100", {GP_REV, GP_CONT, -1}, {100, 50}, -1, 0},
        {"SCAN DLY, OFST 1 (just behind the head)", {GP_SCAN, GP_OFST, GP_CONT}, {SCAN_DLY, 1, 50}, -1, 0},
        {"SCAN POS at 98 %, PTCH +24 (off the buffer's end; the head sweeping past)", {GP_SCAN, GP_OFST, GP_PTCH},
         {SCAN_POS, 98, 24}, -1, 0},
        {"SIZE 10 ms, RATE 100", {GP_SIZE, GP_RATE, -1}, {10, 100}, -1, 0},
        {"PTCH -12 (the write head overtakes them)", {GP_PTCH, GP_CONT, -1}, {-12, 50}, -1, 0},
        {"REV 100, PTCH -12, SIZE 500", {GP_REV, GP_PTCH, GP_SIZE}, {100, -12, 500}, -1, 0},
        {"WET 100 -> 0 while they sound", {GP_CONT, -1, -1}, {50}, GP_WET, 0},
        {"WET 100 -> 30 at once", {GP_CONT, -1, -1}, {50}, GP_WET, 30},
    };
    uint32_t c, j, t, worst = 0;
    char b[160];
    for (c = 0; c < sizeof C / sizeof C[0]; c++) {
        int16_t *p = tp[0].dev[DEV_GRAIN];
        uint32_t nk;
        power_on();
        tp[0].src = SRC_SYNTH;                           /* (live: the buffer always records) */
        for (t = 1; t < NTRK; t++)
            track[t].mute = 1;
        p[GP_WET] = 100;
        p[GP_RATE] = 60;
        p[GP_SIZE] = 120;
        p[GP_CONT] = 0;
        p[GP_SCAN] = SCAN_STR;
        for (j = 0; j < 3u; j++)
            if (C[c].k[j] >= 0)
                p[C[c].k[j]] = C[c].v[j];
        sys.playing = 1;
        chain_poll();
        clk_run(1378 * 3, 0);                            /* (the buffer full, more than once round) */
        nk = clk_run(1378, 1);
        if (C[c].k2 >= 0) {
            p[C[c].k2] = C[c].v2;
            nk += clk_run(400, 1);
        }
        worst = nk > worst ? nk : worst;
        if (nk) {
            snprintf(b, sizeof b, "grains click: %s (%u corners)", C[c].what, nk);
            check(b, 0);
        }
    }
    check("grains of a sine with a seam in its buffer: no click anywhere (CONT 0, PTCH +12 and -12, REV, DLY by the "
          "head, POS off the end, 10 ms grains, WET to 0 and stepped)", worst == 0u);
    power_on();
    render_poll(1378);
}

/* GRAIN's BODY: a floor under the grains. Each case is run twice from power-on, BODY on and off, the rest the same
 * (all deterministic), so the difference is BODY alone */
#define BODY_N 1378u                                     /* blocks: a second */
static int32_t body_on[BODY_N * CTL], body_off[BODY_N * CTL];
static int32_t body_hf = 6000;                           /* the source's 3 kHz (0: only its 100 Hz) */
static int16_t body_ptch, body_prnd, body_scal;          /* the grains' PTCH PRND SCAL (0: the defaults) */
static int16_t body_rptch = 45, body_rwet;               /* RIPPLE's PTCH and WET (ROOT RES follows it) */
static int16_t body_seen[BODY_N], body_to[BODY_N];       /* the wave's pitch over ROOT each block, 1/16 semitone, and
                                                          * the grain's note it glides to */
static void body_run(int32_t *o, int body, int mode, int note, int tone, int wet, int sw_at, int sw_mode)
{
    static int32_t dry[CTL], l[CTL], r[CTL];
    int16_t *p = tp[0].dev[DEV_GRAIN];
    uint32_t b, i, t;
    double ph1 = 0, ph2 = 0;
    power_on();
    tp[0].src = SRC_SYNTH;                               /* (live: the buffer records what comes in) */
    for (t = 1; t < NTRK; t++)
        track[t].mute = 1;
    p[GP_WET] = (int16_t)wet;
    p[GP_BODY] = (int16_t)body;
    p[GP_BMOD] = (int16_t)mode;
    p[GP_ROOT] = (int16_t)note;
    p[GP_BTON] = (int16_t)tone;
    p[GP_PTCH] = body_ptch;
    p[GP_PRND] = body_prnd;
    p[GP_SCAL] = body_scal;
    tp[0].dev[DEV_RESO][RP_PTCH] = body_rptch;
    tp[0].dev[DEV_RESO][RP_WET] = body_rwet;
    sys.playing = 1;
    chain_poll();
    for (b = 0; b < BODY_N; b++) {
        if (sw_at >= 0 && b == (uint32_t)sw_at)
            p[sw_mode >= 0 ? GP_BMOD : GP_BODY] = (int16_t)(sw_mode >= 0 ? sw_mode : 100);
        for (i = 0; i < CTL; i++, ph1 += 2 * 3.14159265358979 * 100 / 44100, ph2 += 2 * 3.14159265358979 * 3000 / 44100)
            dry[i] = (int32_t)(6000 * sin(ph1) + body_hf * sin(ph2));   /* 100 Hz and 3 kHz */
        gr_plan();
        grain_block(0, dry, 0, 0, l, r, CTL);
        for (i = 0; i < CTL; i++)
            o[b * CTL + i] = l[i];
        body_seen[b] = grain[0].bst;
        body_to[b] = grain[0].bto;
    }
}
static double body_amp(double hz, uint32_t from)          /* the difference's amplitude at hz (Goertzel), from block */
{
    double w = 2 * 3.14159265358979 * hz / 44100, c = 2 * cos(w), s0, s1 = 0, s2 = 0;
    uint32_t i, n = 0;
    for (i = from * CTL; i < BODY_N * CTL; i++, n++) {
        s0 = (body_on[i] - body_off[i]) + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return 2 * sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / n;
}
static uint32_t body_xings(uint32_t from)                 /* the difference's upward zero crossings, from block */
{
    uint32_t i, x = 0;
    for (i = from * CTL + 1u; i < BODY_N * CTL; i++)
        x += body_on[i - 1u] - body_off[i - 1u] < 0 && body_on[i] - body_off[i] >= 0;
    return x;
}
static int32_t body_jump(void)                            /* the difference's largest sample-to-sample move */
{
    uint32_t i;
    int32_t m = 0;
    for (i = 1; i < BODY_N * CTL; i++) {
        int32_t d = (body_on[i] - body_off[i]) - (body_on[i - 1u] - body_off[i - 1u]);
        d = d < 0 ? -d : d;
        m = d > m ? d : m;
    }
    return m;
}
/* nothing left running once the sound stops: every device's integer state settles to 0, so COLOR's NOIS (which
 * follows its input's level) closes, and BODY's wave stops with the grains. From playing it (2026-10-11): a steady
 * hiss with the transport stopped, a RIPPLE at DEC 100 holding an offset of about 740 for good */
static int32_t settle_env(void (*setup)(void), int32_t *lp_max, int32_t *ic_max)
{
    uint32_t b, k;
    int32_t mx = 0;
    power_on();
    tp[0].dev[DEV_COLOR][CP_NOIS] = 60;
    setup();
    chain_poll();
    sys.playing = 1;
    if (tp[0].src == SRC_SYNTH)
        fm1_in.notes = note_bit_of_white(0) | note_bit_of_white(4);
    render(1378 * 3, 0);
    fm1_in.notes = 0;
    sys.playing = 0;
    render(1378 * 20, 0);
    for (b = 0; b < 1378u; b++) {
        render(1, 0);
        mx = color[0].env > mx ? color[0].env : mx;
    }
    *lp_max = *ic_max = 0;
    for (k = 0; k < RS_N; k++)
        *lp_max = abs(reso[0].lp[k]) > *lp_max ? abs(reso[0].lp[k]) : *lp_max;
    *ic_max = abs(reso[0].ic1) > abs(reso[0].ic2) ? abs(reso[0].ic1) : abs(reso[0].ic2);
    return mx >> 12;
}
static void st_reso(void) { tp[0].dev[DEV_RESO][RP_WET] = 60; tp[0].dev[DEV_RESO][RP_DEC] = 100; }
static void st_reso_res(void) { st_reso(); tp[0].dev[DEV_RESO][RP_CUT] = 40; tp[0].dev[DEV_RESO][RP_RES] = 90; }
static void st_body_src(void) { tp[0].src = SRC_SYNTH; st_reso(); tp[0].dev[DEV_GRAIN][GP_WET] = 80; tp[0].dev[DEV_GRAIN][GP_BODY] = 100; }
static void st_body_tri(void) { tp[0].dev[DEV_GRAIN][GP_WET] = 80; tp[0].dev[DEV_GRAIN][GP_BODY] = 100; tp[0].dev[DEV_GRAIN][GP_BMOD] = BODY_TRI; }
static void pj_wipe(void);
/* blocks of audio with the main loop's polls between (a growing tape takes its chunks there) */
static void play_polled(uint32_t blocks)
{
    uint32_t b;
    for (b = 0; b < blocks; b++) {
        chain_block(out, CTL);
        if (b % 8u == 0u) {
            tape_poll();
            chain_poll();
        }
    }
    tape_poll();
}

/* a take of set bars (REC held + SELECT): from the downbeat, exactly that long, then REC lets go by itself */
static void test_bar_record(void)
{
    tape_view_t v;
    char b[160];
    int32_t len;
    power_on();
    sys.sel = 1;                                         /* T2 on its own blank tape, recording the others (AUTO) */
    tp[1].dev[DEV_SRC][TK_REEL] = 0;
    hold(B_REC);
    host_enc[panel.enc[EN_SELECT]] = 1;
    ui_input();
    host_enc[panel.enc[EN_SELECT]] = 1;
    ui_input();
    fm1_ms += 1500;                                      /* (held past the USB record mode's second) */
    ui_input();
    let_go(B_REC);
    check("REC held + SELECT twice: 2 BARS; nothing armed, no USB record mode", sys.rec_bars == 2u && !sys.rec &&
          ui.view != VIEW_USBREC && !strncmp(ui.msg, "REC: 2 BARS", 11));
    tap(B_REC);
    check("..REC tapped (stopped): armed, waiting for PLAY's downbeat", (sys.rec >> 1) & 1u);
    ui.msg_t = 0;
    focus_dev(DEV_SRC, 0);
    shot("BRYO", "rec_bars");                            /* (the header: REC's dot, the 2 bars before it) */
    sys.playing = 1;
    play_polled(1378u * 2u);
    check("..PLAY at 120: 2 s in it's still recording (the take is 4 s)", tape_rt[1].rbar == TB_REC &&
          ((sys.rec >> 1) & 1u) && tape_ctl[1].grow);
    play_polled(1378u * 2u + 100u);
    ui_input();
    tape_view(1, &v);
    snprintf(b, sizeof b, "..then it stops by itself: the take is exactly 2 bars (%u tape samples, 88,200 at 120 BPM), REC "
             "let go, and it says so", v.len);
    check(b, v.len == 88200u && !((sys.rec >> 1) & 1u) && !tape_ctl[1].grow && !strncmp(ui.msg, "T2: THE TAKE IS IN", 18));
    check("..and its SYNC is set to its 2 bars (it keeps to them when the tempo moves)", tp[1].dev[DEV_SRC][TK_SYNC] == 2);
    {
        int32_t p0, k;
        uint32_t wraps = 0;
        int32_t last = tape_rt[1].pos >> 12;
        for (k = 0; k < 1378 * 4; k++) {                 /* it loops at its length: once every 4 s */
            render(1, 0);
            p0 = tape_rt[1].pos >> 12;
            wraps += p0 < last;
            last = p0;
        }
        check("..and loops at that length (once in 4 s of play), with sound on it",
              wraps == 1u && tape_peak_at(1, 500) > 0);
    }
    pj_wipe();
    pj_save(0);
    tape_view(1, &v);
    check("..saved with a project into a reel: still exactly 88,200 samples (the reel keeps the exact end)",
          tp[1].dev[DEV_SRC][TK_REEL] > (int16_t)NREEL && v.len == 88200u);
    sys.playing = 0;
    render(20, 0);

    power_on();                                          /* 130 BPM, 1 bar, armed while playing: the next downbeat */
    pj_wipe();
    sys.sel = 2;
    tp[2].dev[DEV_SRC][TK_REEL] = 0;
    sys.rec_bars = 1;
    sys.bpm = 130;
    sys.playing = 1;
    play_polled(1378u * 1u);                             /* 1 s in: the first bar ends at 1.846 s */
    tap(B_REC);
    play_polled(500u);
    check("..armed while playing (1.4 s in): it waits for the next downbeat, nothing recorded yet",
          tape_rt[2].rbar == TB_WAIT && tape_ctl[2].nblk == 0u);
    play_polled(1378u * 3u);
    tape_view(2, &v);
    len = (int32_t)(10584000u / 130u) / 2;
    snprintf(b, sizeof b, "..from the downbeat, one bar at 130 BPM: %u tape samples (%d)", v.len, len);
    check(b, abs((int32_t)v.len - len) <= 1 && !((sys.rec >> 2) & 1u));
    sys.playing = 0;
    render(20, 0);
    sys.rec_bars = 0;
    sys.bpm = 120;
    pj_wipe();
    power_on();
}

/* the zoom: HOME held + a loop's edge moves it a hundredth of a percent, and the picture zooms to it; POLY's END */
static void test_zoom(void)
{
    tape_view_t v;
    int32_t ls, ll, lsb, llb;
    uint32_t i;
    power_on();
    focus_dev(DEV_SRC, 0);                               /* T1's TAPE, page 1 */
    tape_view(0, &v);
    tape_window(0, v.len, &lsb, &llb);
    hold(B_HOME);
    turn(0, 7);
    tape_window(0, v.len, &ls, &ll);
    check("HOME held + STRT 7 detents: 0.07 % (STRT 7 hundredths), the loop starts there to the sample",
          tp[0].dev[DEV_SRC][TK_STRT] == 7 && ls == (int32_t)v.len * 7 / 10000 && ui.zoom == 1u);
    ui.msg_t = 0;
    ui_draw();
    shot_screen("BRYO", "tape_zoom");
    turn(1, -3);
    tape_window(0, v.len, &ls, &ll);
    check("..LEN 3 detents back from 100: 99.97 % (from 0.07 % that's past the end, so the loop runs to it)",
          tp[0].dev[DEV_SRC][TK_LEN] == 9997 && ls + ll == (int32_t)v.len);
    turn(0, -7);
    tape_window(0, v.len, &ls, &ll);
    check("..STRT back to 0: the loop is 99.97 % long, to the sample", ls == 0 &&
          ll == (int32_t)v.len * 99 / 100 + (int32_t)v.len * 97 / 10000);
    turn(0, 7);
    let_go(B_HOME);
    check("..HOME let go: the zoom ends, and the page stays (it was held for the zoom)", ui.zoom == 0u &&
          ui.view == VIEW_PAGE && ui.page == 0u && ui.dev == DEV_SRC);
    turn(0, 2);
    check("..STRT turned alone: whole percents, its hundredths kept (2.07 %)", tp[0].dev[DEV_SRC][TK_STRT] == 207);
    hold(B_HOME);
    turn(0, -300);
    let_go(B_HOME);
    check("..zoomed back past 0: held at 0.00", tp[0].dev[DEV_SRC][TK_STRT] == 0);
    (void)lsb;
    (void)llb;

    power_on();                                          /* POLY's END: where every note stops */
    make_sine_reel(0, 440.0, 200);
    tp[0].src = SRC_POLY;
    tp[0].pol[PL_REEL] = (int16_t)(NREEL + 1u);
    tp[0].pol[PL_ATK] = 0;
    tp[0].pol[PL_SUS] = 100;
    tp[0].pol[PL_END] = 2500;                              /* a quarter of 2.3 s: 0.58 s at the sound's pitch */
    tp[0].pol[PL_ROOT] = 48;                               /* (white key 1: the sound's pitch) */
    focus_dev(DEV_SRC, 0);
    check("POLY's first page: REEL STRT END TUNE", !strcmp(POL_P[pdesc_at(POL_P, 2u)].label, "END") &&
          !strcmp(POL_P[pdesc_at(POL_P, 3u)].label, "TUNE") && !strcmp(POL_P[pdesc_at(POL_P, 20u)].label, "VOIC") &&
          !strcmp(POL_P[pdesc_at(POL_P, 16u)].label, "TWIN") && !strcmp(POL_P[pdesc_at(POL_P, 4u)].label, "LOOP"));
    fm1_in.notes = note_bit_of_white(0);
    render(1, 0);
    for (i = 1; i < 1378u && pol_sounding(0); i++)
        render(1, 0);
    fm1_in.notes = 0;
    render(4, 0);
    {
        char b[120];
        double t = i * CTL / 44100.0 - POL_ENDF * CTL / 44100.0;   /* (its fade after END) */
        snprintf(b, sizeof b, "END 25: a held note stops by itself at a quarter of the sound (%.3f s, 0.580)", t);
        check(b, fabs(t - 0.580) < 0.01);
    }
    hold(B_HOME);
    turn(2, -10);                                        /* (page 1 KNOB 3: END) */
    let_go(B_HOME);
    check("..HOME held + END: hundredths (24.90 %)", tp[0].pol[PL_END] == 2490);
    hold(B_HOME);
    turn(1, 5);
    ui.msg_t = 0;
    ui_draw();
    shot_screen("BRYO", "poly_zoom");
    let_go(B_HOME);
    check("..and STRT: 0.05 %", tp[0].pol[PL_STRT] == 5);
    power_on();
}

/* resting: a device whose input is digital silence and whose tail has gone does none of its work, and wakes on the
 * next sound as it was. From playing it (2026-10-11): half the CPU with nothing playing, and muting tracks saved
 * nothing. Also, in integers SPACE's room and the channel's FILT cycled at -66 and -55 dB for good: that's gone */
static void test_resting(void)
{
    uint32_t b;
    int32_t pk;
    static int32_t buf[200 * CTL];
    power_on();
    tp[0].dev[DEV_RESO][RP_WET] = 50;                    /* T1: strings, colour, a room and a delay, the channel FILT */
    tp[0].dev[DEV_COLOR][CP_TILT] = -30;
    tp[0].dev[DEV_SPACE][SP_VERB] = 50;
    tp[0].dev[DEV_SPACE][SP_DLY] = 40;
    tp[0].ch[CH_FILT] = 20;
    tp[0].ch[CH_LOW] = 3;
    chain_poll();
    sys.playing = 1;
    render(1378 * 2, 0);
    check("playing: nothing rests (sound in everywhere)", !space[0].quiet && !reso[0].quiet);
    sys.playing = 0;
    render(1378 * 8, 0);
    render(200, buf);
    pk = peak_of(buf, 200 * CTL);
    check("stopped 8 s: SPACE, the strings, COLOR and the channel all rest, and the track's out is exactly 0 (the "
          "room's and FILT's integer residue gone with them)",
          space[0].quiet > sp_len(&space[0].dly) && room[0].quiet > (uint32_t)rmp[0][RM_PRE] * 2205u / 100u + 2037u &&
          reso[0].quiet > RS_LEN + CTL &&
          !chan[0].f1[0] && !chan[0].f2[1] && !chan[0].lo[0] && !color[0].tlo[0] && pk == 0 && !track_rt[0].last[7]);
    {
        uint32_t dw = space[0].dw, pw = room[0].pw, ci = room[0].ci[0], rw = reso[0].w;
        render(20, 0);
        check("..resting is none of the work: SPACE's lines, its room's and the strings' stand still (their write "
              "points as they were)", space[0].dw == dw && room[0].pw == pw && room[0].ci[0] == ci && reso[0].w == rw);
    }
    sys.playing = 1;
    for (b = 0, pk = 0; b < 40u; b++) {
        render(1, buf);
        pk = peak_of(buf, CTL) > pk ? peak_of(buf, CTL) : pk;
    }
    check("..PLAY again: they wake on the sound at once (no block of silence first)", pk > 1000 && !space[0].quiet &&
          !reso[0].quiet);
    render(1378, 0);
    focus_dev(DEV_SPACE, 0);                             /* the pictures while it sounds: orange where it's alive */
    shot("BRYO", "space_live");
    focus_dev(DEV_RESO, 0);
    shot("BRYO", "reso_live");
    check("..the screen knows it: SPACE sounding, a string ringing (orange in their pictures)",
          space_sounding(0) && (reso[0].lvl[0] > RV_RING || reso[0].lvl[1] > RV_RING || reso[0].lvl[2] > RV_RING));
    track[0].mute = 1;                                   /* muted: its devices hear silence and rest too */
    render(1378 * 6, 0);
    check("muted (the transport playing): RIPPLE, SPACE and the channel ring down and rest (they cost nothing)",
          reso[0].quiet > RS_LEN + CTL && space[0].quiet > sp_len(&space[0].dly) && !chan[0].f1[0]);
    track[0].mute = 0;
    render(4, 0);
    check("..unmuted: awake again on the track's sound", !space[0].quiet && !reso[0].quiet);
    track[0].mute = 1;
    render(1378 * 6, 0);
    check("..resting, the screen knows that too: SPACE not sounding, no string ringing",
          !space_sounding(0) && !reso[0].lvl[0] && !reso[0].lvl[1] && !reso[0].lvl[2] && !reso[0].lvl[3]);
    track[0].mute = 0;
    power_on();
}

static void test_settle(void)
{
    int32_t e, lp, ic;
    char m[200];
    e = settle_env(st_reso, &lp, &ic);
    snprintf(m, sizeof m, "RIPPLE DEC 100, 20 s after the sound stopped: every string at 0 (largest %d), so COLOR's "
             "NOIS is closed (its envelope %d, was 733: a steady hiss)", lp, e);
    check(m, lp == 0 && e == 0);
    e = settle_env(st_reso_res, &lp, &ic);
    snprintf(m, sizeof m, "..its filter in front at RES 90: let go to 0 on silence (%d), the strings don't ring a residue "
             "up (NOIS %d, was 4931)", ic, e);
    check(m, ic == 0 && lp == 0 && e == 0);
    e = settle_env(st_body_src, &lp, &ic);
    snprintf(m, sizeof m, "GRAIN BODY SRC into it on a SYNTH track: TONE's low-pass settles to 0 too (%d, its residue "
             "rang up to 935)", grain[0].blp);
    check(m, grain[0].blp == 0 && lp == 0 && e == 0);
    e = settle_env(st_body_tri, &lp, &ic);
    snprintf(m, sizeof m, "BODY TRI, the transport stopped: the wave stops with the grains (its gate %d, level %d; it "
             "droned on its own)", grain[0].benv, grain[0].bg);
    check(m, grain[0].benv == 0 && grain[0].bg == 0 && e == 0);
    power_on();
}

static void test_grain_body(void)
{
    double lo, hi, hi2;
    char m[200];
    body_run(body_off, 0, BODY_SRC, 45, 38, 100, -1, -1);
    body_run(body_on, 100, BODY_SRC, 45, 38, 100, -1, -1);
    lo = body_amp(100, 200);
    hi = body_amp(3000, 200);
    snprintf(m, sizeof m, "BODY SRC, TONE 200 Hz, WET 100: the source's low end under the grains (100 Hz at %.0f, 3 kHz "
             "at %.0f)", lo, hi);
    check(m, lo > 3000 && hi < lo / 10);
    body_run(body_on, 100, BODY_SRC, 45, 90, 100, -1, -1);
    hi2 = body_amp(3000, 200);
    snprintf(m, sizeof m, "..TONE up (%u Hz): the 3 kHz comes through (%.0f)", CUTOFF_HZ[90], hi2);
    check(m, hi2 > 5 * hi);
    body_run(body_on, 100, BODY_TRI, 45, 60, 100, -1, -1);
    snprintf(m, sizeof m, "BODY TRI at ROOT A2: 110 Hz (%u cycles in the last 0.85 s), whatever the source", body_xings(200));
    check(m, body_xings(200) >= 92u && body_xings(200) <= 95u);
    body_ptch = 7;                                       /* the grains a fifth up: the wave too, over ROOT */
    body_run(body_off, 0, BODY_TRI, 45, 60, 100, -1, -1);
    body_run(body_on, 100, BODY_TRI, 45, 60, 100, -1, -1);
    snprintf(m, sizeof m, "..PTCH +7: it plays the grains' note, ROOT's fifth (E3, 165 Hz: %u cycles)", body_xings(200));
    check(m, body_xings(200) >= 138u && body_xings(200) <= 142u);
    body_ptch = -19;                                     /* (an octave and a fifth down: a fourth, over ROOT) */
    body_run(body_off, 0, BODY_TRI, 45, 60, 100, -1, -1);
    body_run(body_on, 100, BODY_TRI, 45, 60, 100, -1, -1);
    snprintf(m, sizeof m, "..PTCH -19: the note folded into the octave over ROOT, a floor (ROOT's fourth, D3, 147 Hz: %u cycles)",
             body_xings(200));
    check(m, body_xings(200) >= 123u && body_xings(200) <= 127u);
    body_ptch = 0;
    body_prnd = 12;                                      /* PRND 12, SCAL PEN: the grains a pentatonic melody */
    body_scal = 11;
    body_run(body_off, 0, BODY_TRI, 45, 60, 100, -1, -1);
    body_run(body_on, 100, BODY_TRI, 45, 60, 100, -1, -1);
    {
        uint32_t b, notes = 0, odd = 0, there = 0, n = 0, k;
        for (b = 200; b < BODY_N; b++) {
            k = (uint32_t)body_to[b];
            odd += k % 16u != 0u || k >= 192u || !((0x295u >> (k / 16u)) & 1u);
            if (k < 192u && !((notes >> (k / 16u)) & 1u))
                notes |= 1u << (k / 16u), n++;
            there += body_seen[b] == body_to[b];
        }
        snprintf(m, sizeof m, "..PRND 12, SCAL PEN: the wave plays the grains' pentatonic notes (%u of its 5, %u blocks off "
                 "the scale), arriving at them (%u of %u blocks there), gliding, no steps (largest move %d)", n, odd,
                 there, BODY_N - 200u, body_jump());
        check(m, n >= 4u && odd == 0u && there > (BODY_N - 200u) / 3u && there < BODY_N - 200u && body_jump() < 400);
    }
    body_prnd = 0;                                       /* (PTCH +7, BODY turned up: at the grains' note at once, not
                                                          * gliding up from ROOT) */
    body_scal = 0;
    body_ptch = 7;
    body_run(body_on, 0, BODY_TRI, 45, 60, 100, 600, -1);
    check("..BODY turned up from 0: the wave starts on the grains' note (no glide up to it)",
          body_to[600] == 112 && body_seen[600] == 112 && body_seen[599] != 112);
    body_ptch = 0;
    body_prnd = 0;
    body_scal = 0;
    body_rptch = 57;                                     /* ROOT RES: RIPPLE's root, PTCH A3 (its WET 0: the wave
                                                          * alone measured) */
    body_run(body_off, 0, BODY_TRI, GR_ROOT_RES, 60, 100, -1, -1);
    body_run(body_on, 100, BODY_TRI, GR_ROOT_RES, 60, 100, -1, -1);
    {
        char v[8];
        const char *u;
        param_format(&DEV_P[DEV_GRAIN][GP_ROOT], GR_ROOT_RES, v, &u);
        snprintf(m, sizeof m, "BODY ROOT RES (it reads \"%s\"): the wave on RIPPLE's root, PTCH A3, 220 Hz (%u cycles)",
                 v, body_xings(200));
        check(m, !strcmp(v, "RES") && body_xings(200) >= 186u && body_xings(200) <= 189u);
    }
    body_rptch = 48;                                     /* the strings on C3, RIPPLE's WET 100 (only them heard) */
    body_rwet = 100;
    {
        double a2, res;
        body_run(body_off, 0, BODY_SIN, 45, 60, 100, -1, -1);
        body_run(body_on, 100, BODY_SIN, 45, 60, 100, -1, -1);
        a2 = body_amp(130.81, 200);
        body_run(body_on, 100, BODY_SIN, GR_ROOT_RES, 60, 100, -1, -1);
        res = body_amp(130.81, 200);
        snprintf(m, sizeof m, "..into the strings on C3 (their note, 131 Hz): BODY SIN on ROOT A2, a third away, rings it "
                 "at %.0f, on RES at %.0f", a2, res);
        check(m, res > 8 * a2 && res > 300);
    }
    body_rptch = 45;
    body_rwet = 0;
    body_run(body_off, 0, BODY_TRI, 45, 60, 100, -1, -1);
    body_run(body_on, 100, BODY_SIN, 57, 60, 100, -1, -1);
    snprintf(m, sizeof m, "BODY SIN at ROOT A3: 220 Hz (%u cycles)", body_xings(200));
    check(m, body_xings(200) >= 186u && body_xings(200) <= 189u);
    body_run(body_off, 0, BODY_SRC, 45, 38, 0, -1, -1);
    body_run(body_on, 100, BODY_SRC, 45, 38, 0, -1, -1);
    check("..WET 0: no BODY either (it's on the wet side)", body_amp(100, 200) < 1 && body_jump() == 0);
    body_run(body_off, 0, BODY_TRI, 45, 60, 100, -1, -1);
    body_run(body_on, 0, BODY_TRI, 45, 60, 100, 600, -1);          /* BODY 0 -> 100 in a block */
    snprintf(m, sizeof m, "BODY turned 0 -> 100 at once: it ramps in (largest move %d)", body_jump());
    check(m, body_jump() < 400);
    body_hf = 0;                                         /* (the source only its 100 Hz: its own moves small) */
    body_run(body_off, 0, BODY_SRC, 45, 100, 100, -1, -1);
    body_run(body_on, 100, BODY_SRC, 45, 100, 100, 600, BODY_TRI);   /* MODE SRC -> TRI while it sounds, TONE open */
    body_hf = 6000;
    {                                                    /* (the move at the switch against its own playing's) */
        uint32_t i;
        int32_t at = 0, before = 0;
        for (i = 300u * CTL; i < 604u * CTL; i++) {
            int32_t d = (body_on[i] - body_off[i]) - (body_on[i - 1u] - body_off[i - 1u]);
            d = d < 0 ? -d : d;
            if (i < 590u * CTL)
                before = d > before ? d : before;
            else
                at = d > at ? d : at;
        }
        snprintf(m, sizeof m, "..MODE changed while it sounds (SRC to TRI, TONE open): down and back up, no step "
                 "(largest move at the switch %d, while it played %d)", at, before);
        check(m, at <= 4 * before + 200 && body_xings(700) >= 50u && body_xings(700) <= 56u);   /* (0.49 s of 110
                                                                                                * Hz) */
    }
    power_on();
}

/* silent grains cost nothing: a grain whose whole read is silence (every block's stored peak 0) isn't started, and a
 * grain's blocks of silence are skipped (no decoding, no mixing) while it keeps its place */
static void test_grain_silence(void)
{
    tape_view_t v;
    uint32_t b, blk, nb;
    int32_t pos0, inc, heard = 0, inside = 0, i;
    static int32_t dry[CTL], gl[CTL], gr[CTL];
    power_on();
    fill_tape(0, 4);                                     /* track 1: 4 chunks (64 blocks), factory reel 1's sound */
    {
        tape_view_t rv;
        tape_view_of(0, 1, &rv);
        for (blk = 0; blk < 64u; blk++) {
            mem_chunk_t *c = tape_chunk(0, blk);
            memcpy(c->data[blk % MEM_CB], tv_data(&rv, blk), 128);
            c->pred[blk % MEM_CB] = (int16_t)tv_pred(&rv, blk);
            c->idx[blk % MEM_CB] = (uint8_t)tv_idx(&rv, blk);
        }
    }
    tape_view(0, &v);
    nb = v.len / TAPE_BLK;
    for (blk = 16; blk < 32u; blk++)                     /* blocks 16-31 marked silent (their sound left in: a grain
                                                          * that's quiet there was skipped) */
        tape_chunk(0, blk)->peak[blk % MEM_CB] = 0;
    check("gr_quiet: a stretch inside the silent blocks is silent; one touching sound isn't",
          gr_quiet(&v, 16 * 256, 32 * 256 - 1, (int32_t)v.len, -1) && !gr_quiet(&v, 15 * 256, 20 * 256, (int32_t)v.len, -1) &&
          !gr_quiet(&v, 30 * 256, 33 * 256, (int32_t)v.len, -1));
    check("..the block being written counts as sound (its peak is the old one's)",
          !gr_quiet(&v, 16 * 256, 20 * 256, (int32_t)v.len, 18));
    for (blk = 0; blk < 4u; blk++)                       /* round the loop: the end's last blocks and the start's */
        tape_chunk(0, blk)->peak[blk % MEM_CB] = 0, tape_chunk(0, nb - 1u - blk)->peak[(nb - 1u - blk) % MEM_CB] = 0;
    check("..past the end, round the loop: the start's blocks are the ones looked at",
          gr_quiet(&v, (int32_t)(nb - 2u) * 256, (int32_t)(nb + 2u) * 256, (int32_t)v.len, -1) &&
          !gr_quiet(&v, (int32_t)(nb - 2u) * 256, (int32_t)(nb + 6u) * 256, (int32_t)v.len, -1));
    for (blk = 0; blk < 4u; blk++)
        tape_chunk(0, blk)->peak[blk % MEM_CB] = 100, tape_chunk(0, nb - 1u - blk)->peak[(nb - 1u - blk) % MEM_CB] = 100;
    {                                                    /* a 50 ms grain inside the silence: not started */
        int16_t *p = tp[0].dev[DEV_GRAIN];
        p[GP_WET] = 100;
        p[GP_SCAN] = SCAN_TAPE;
        p[GP_SIZE] = 50;
        p[GP_SPRY] = 0;
        sys.playing = 1;
        grain_kill(0);
        grain[0].cur = (18 * 256) << 12;
        grain[0].wait = 0;
        gr_plan();
        grain_block(0, dry, 0, 0, gl, gr, CTL);
        check("a grain whose whole read is silence isn't started (no work, its slot free)", grain_count(0) == 0u);
        grain[0].cur = (8 * 256) << 12;                  /* (in sound: it is) */
        grain[0].wait = 0;
        grain_block(0, dry, 0, 0, gl, gr, CTL);
        check("..one reading sound is", grain_count(0) == 1u);
    }
    {                                                    /* a grain from block 12 across the silence: its place kept */
        int16_t *p = tp[0].dev[DEV_GRAIN];
        p[GP_WET] = 100;
        p[GP_SCAN] = SCAN_TAPE;
        p[GP_SIZE] = 500;
        p[GP_RATE] = 0;
        p[GP_SPRY] = 0;
        sys.playing = 1;
        grain_kill(0);
        grain[0].cur = (12 * 256) << 12;
        grain[0].wait = 0;
        gr_plan();
        grain_block(0, dry, 0, 0, gl, gr, CTL);          /* (the grain starts in sound: block 12) */
        pos0 = gslot[0].pos;
        inc = gslot[0].inc;
        for (b = 0; b < 600u && gr_on(0); b++) {
            grain[0].wait = 1 << 30;                     /* (no other grain) */
            gr_plan();
            grain_block(0, dry, 0, 0, gl, gr, CTL);
            if ((gslot[0].pos >> 12) >= 33 * 256 && (gslot[0].pos >> 12) < 40 * 256)
                for (i = 0; i < (int32_t)CTL; i++)
                    heard |= gl[i] != 0;
            if ((gslot[0].pos >> 12) >= 17 * 256 && (gslot[0].pos >> 12) < 31 * 256)
                for (i = 0; i < (int32_t)CTL; i++)
                    inside |= gl[i] != 0;
            if (b == 400u) {
                char m[160];
                snprintf(m, sizeof m, "..a grain across 16 silent blocks: nothing worked out inside them, its place kept (%d, "
                         "expected %d), sound again past them", gslot[0].pos >> 12, (pos0 + 2 * inc * 16 * 401) >> 12);
                check(m, gslot[0].pos == pos0 + 2 * inc * 16 * 401 && heard && !inside);
            }
        }
    }
    sys.playing = 0;
    power_on();
}

/* the pool goes to the tracks whose grains would be heard: GRAIN on but nothing in its buffer (a POLY track nobody
 * has played), or the track muted, takes no share; the moment there's sound to granulate, it takes its share */
static void test_grain_shares(void)
{
    char m[160];
    uint32_t t, b, mx = 0;
    power_on();
    sys.bpm = 120;
    sys.sel = 0;
    tp[0].dev[DEV_GRAIN][GP_WET] = 100;                  /* track 1: its tape, dense grains */
    tp[0].dev[DEV_GRAIN][GP_RATE] = 100;
    tp[0].dev[DEV_GRAIN][GP_SIZE] = 500;
    for (t = 1; t < NTRK; t++) {                         /* tracks 2-4: POLY with GRAIN on, nobody playing them */
        tp[t].src = SRC_POLY;
        tp[t].dev[DEV_GRAIN][GP_WET] = 40;
    }
    sys.playing = 1;
    render_poll(1378 * 2);
    for (b = 0; b < 1378u; b++) {
        render_poll(1);
        mx = grain_count(0) > mx ? grain_count(0) : mx;
    }
    check("GRAIN on four tracks, three of them POLY with nothing played: track 1 has the whole pool (40 sounding)",
          gr_allow(0) == 40u && mx == 40u && gr_allow(1) == 0u && gr_allow(2) == 0u && gr_allow(3) == 0u &&
          grain_count(1) == 0u && gr_why(1) && !strcmp(gr_why(1), "SILENT: NO GRAINS") && !gr_why(0));
    sys.sel = 1;                                         /* a chord on track 2: there's something to granulate */
    fm1_in.notes = note_bit_of_white(0) | note_bit_of_white(4);
    render_poll(200);
    fm1_in.notes = 0;
    render_poll(64);
    check("..a chord played on track 2: it takes its share of a pool of 30, track 1 the rest",
          gr_allow(1) > 0u && gr_allow(0) + gr_allow(1) == 30u && gr_allow(2) == 0u);
    tp[0].dev[DEV_GRAIN][GP_RATE] = 50;                  /* track 1 asking 8 (21 a second x 200 ms, with room), track
                                                          * 2 its 3: what's over is theirs too, evenly */
    tp[0].dev[DEV_GRAIN][GP_SIZE] = 200;
    render_poll(2);
    snprintf(m, sizeof m, "..both asking less than the pool: each has what it asks and half of what's over, the odd one "
             "to track 1 (8 + 10, 3 + 9: %u, %u)", gr_allow(0), gr_allow(1));
    check(m, gr_need(tp[0].dev[DEV_GRAIN]) == 8u && gr_allow(0) == 18u && gr_allow(1) == 12u);
    tp[0].dev[DEV_GRAIN][GP_GRNS] = 12;                  /* GRNS: track 1 holds 12, track 2 (AUTO) the rest */
    render_poll(2);
    snprintf(m, sizeof m, "GRNS 12 on track 1: it holds 12, track 2 (AUTO) has the rest of 30 (%u, %u)", gr_allow(0),
             gr_allow(1));
    check(m, gr_allow(0) == 12u && gr_allow(1) == 18u && !mod_tdesc(MOD_TG(DEV_GRAIN, GP_GRNS)));
    tp[1].dev[DEV_GRAIN][GP_GRNS] = 20;
    render_poll(2);
    snprintf(m, sizeof m, "..track 2 holding 20 too (32 held, a pool of 30): the one asking less keeps its 12, the "
             "other the 18 left (%u, %u)", gr_allow(0), gr_allow(1));
    check(m, gr_allow(0) == 12u && gr_allow(1) == 18u);
    tp[0].dev[DEV_GRAIN][GP_GRNS] = 40;
    render_poll(2);
    snprintf(m, sizeof m, "..both holding more than half: an even split (%u, %u)", gr_allow(0), gr_allow(1));
    check(m, gr_allow(0) == 15u && gr_allow(1) == 15u);
    tp[0].dev[DEV_GRAIN][GP_GRNS] = 30;
    tp[1].dev[DEV_GRAIN][GP_GRNS] = 0;
    render_poll(2);
    check("..track 1 holding all 30: track 2 (AUTO) has none, and its page says why",
          gr_allow(0) == 30u && gr_allow(1) == 0u && !gr_why(1));
    sys.sel = 1;
    focus_dev(DEV_GRAIN, 0);
    shot("BRYO", "grain_held");
    sys.sel = 0;
    focus_dev(DEV_GRAIN, 0);
    track[1].mute = 1;
    tp[0].dev[DEV_GRAIN][GP_GRNS] = 12;
    render_poll(2);
    check("..track 2 muted, track 1 alone holding 12: 12, not the 40 (what it holds is what it keeps)",
          gr_allow(0) == 12u);
    tp[0].dev[DEV_GRAIN][GP_GRNS] = 0;
    render_poll(2);
    check("..track 2 muted: its grains wouldn't be heard, the whole pool is track 1's again, asking 8 or not (no cap "
          "at what RATE and SIZE ask)", gr_allow(1) == 0u && gr_allow(0) == 40u && gr_why(1) && !strcmp(gr_why(1), "MUTED: NO GRAINS"));
    track[1].mute = 0;
    sys.playing = 0;
    tp[0].dev[DEV_GRAIN][GP_SCAN] = SCAN_TAPE;
    render_poll(64);
    check("..SCAN TAPE, transport stopped: no grains can start on track 1, it takes no share (track 2 the pool's)",
          gr_allow(0) == 0u && gr_allow(1) > 0u && gr_why(0) && !strcmp(gr_why(0), "STOPPED: NO GRAINS"));
    power_on();                                          /* (all quiet again for the next test: the master's filters
                                                          * settle on silence) */
    render_poll(1378);
}

static void test_grain_buffer(void)
{
    char b[120];
    int16_t *p;
    uint32_t t, i, nb;
    int32_t c0;
    power_on();
    p = tp[0].dev[DEV_GRAIN];
    for (t = 1; t < NTRK; t++)
        track[t].mute = 1;
    chain_poll();
    check("GRAIN at WET 0: no buffer, no memory taken", !gbuf[0].nch && !mem_count(MEM_GRAIN));
    p[GP_WET] = 100;
    chain_poll();
    check("WET up (SCAN STR, the default): a buffer of 1 bar at 120 BPM with 4 tracks (2 s, 11 chunks)",
          p[GP_SCAN] == SCAN_STR && gbuf[0].len == 44100u && gbuf[0].nch == 11 && gbuf[0].bars == 1 &&
          mem_count(MEM_GRAIN) == 11);
    chain_tracks(2);
    chain_poll();
    check("TRACKS 2: 2 bars (4 s)", gbuf[0].len == 88200u && gbuf[0].bars == 2);
    chain_tracks(1);
    chain_poll();
    check("TRACKS 1: 4 bars (8 s)", gbuf[0].len == 176400u && gbuf[0].bars == 4 && gbuf[0].nch == 44);
    sys.bpm = 40;
    chain_poll();
    snprintf(b, sizeof b, "..at 40 BPM 4 bars would pass 12 s: 2 bars (%u samples, %u chunks)", (unsigned)gbuf[0].len,
             (unsigned)gbuf[0].nch);
    check(b, gbuf[0].bars == 2 && gbuf[0].len == 264600u && gbuf[0].nch == 65);
    sys.bpm = 120;
    chain_tracks(4);
    chain_poll();
    check("..back to TRACKS 4, 120 BPM: 1 bar, the rest given back", gbuf[0].nch == 11 && mem_count(MEM_GRAIN) == 11);
    p[GP_SCAN] = SCAN_TAPE;
    chain_poll();
    check("SCAN TAPE: no buffer (grains read the tape)", !gbuf[0].nch && !mem_count(MEM_GRAIN));
    p[GP_SCAN] = SCAN_STR;
    chain_poll();

    sys.playing = 1;                                     /* track 1 plays the beat into its buffer */
    render_poll(1);
    check("PLAY: the write head starts at the bar line", gbuf[0].w <= CTL / 2);
    render_poll(1377);
    snprintf(b, sizeof b, "..1 s on: the write head half a bar on (%d of 44,100), the buffer holds the beat",
             (int)gbuf[0].w);
    check(b, abs(gbuf[0].w - 22050) < CTL && buf_loud(0, 0, 86) > 40u);
    render_poll(1378 * 3);
    {                                                    /* (sparse at the defaults: over a quarter second) */
        uint32_t q, mx = 0;
        for (q = 0; q < 345u; q++) {
            render_poll(1);
            mx = grain_count(0) > mx ? grain_count(0) : mx;
        }
        check("..WET 100, SCAN STR: grains sound", mx > 0u);
    }

    tp[0].src = SRC_SYNTH;                               /* the carry-over: TAPE -> SYNTH (silent), FDBK 0 */
    render_poll(10);
    nb = buf_loud(0, (uint32_t)gbuf[0].w / TAPE_BLK + 4u, 172u);
    snprintf(b, sizeof b, "switched to SYNTH: the beat is still in the buffer ahead of the write head (%u blocks)", nb);
    check(b, nb > 10u);
    render_poll(1378 * 2 + 50);
    check("..a bar later (FDBK 0): all of it the new (silent) source", buf_loud(0, 0, 172) == 0);

    tp[0].src = SRC_TAPE;                                /* FDBK 50: half the old stays each pass */
    render_poll(1378 * 2 + 50);
    {
        uint32_t full = buf_loud(0, 0, 172), half;
        tape_view_t v;
        uint32_t s0 = 0, s1 = 0, k;
        gr_buf_view(0, &v);
        for (k = 0; k < 172; k++)
            s0 += tv_peak(&v, k);
        p[GP_FDBK] = 50;
        tp[0].src = SRC_SYNTH;
        render_poll(1378 * 2 + 50);
        for (k = 0; k < 172; k++)
            s1 += tv_peak(&v, k);
        half = s1 * 100u / (s0 ? s0 : 1u);
        snprintf(b, sizeof b, "FDBK 50: a bar of silence over the beat leaves it at half (%u%%)", half);
        check(b, full > 40u && half >= 40u && half <= 60u);
        p[GP_FDBK] = 0;
    }

    tp[0].src = SRC_TAPE;
    render_poll(1378 * 2 + 50);
    {   /* freeze: the buffer holds, the grains loop it */
        uint32_t h0, w0;
        fm1_in.notes = zero_key_bit();
        render_poll(2);
        h0 = buf_sum(0);
        w0 = (uint32_t)gbuf[0].w;
        c0 = grain[0].cur;
        render_poll(1378);
        check("the 0 key held: the buffer stops recording (its sound unchanged, the write head still)",
              buf_sum(0) == h0 && (uint32_t)gbuf[0].w == w0);
        {                                                /* (sparse grains, RATE 40: one sounds within a few blocks) */
            uint32_t q, any = grain_count(0) > 0;
            for (q = 0; q < 8u && !any; q++) {
                render_poll(1);
                any = grain_count(0) > 0;
            }
            check("..and the grains play on through it (the cursor moves, grains sound)", grain[0].cur != c0 && any);
        }
        sys.bpm = 60;                                    /* half the tempo while frozen: the loop's pace halves */
        chain_poll();
        c0 = grain[0].cur;
        render(100, 0);
        {
            int32_t d = (grain[0].cur - c0) >> 12;
            d = d < 0 ? d + (int32_t)gbuf[0].len : d;
            snprintf(b, sizeof b, "..the tempo halved while frozen: the buffer keeps its size, the loop half the pace (%d)", (int)d);
            check(b, gbuf[0].len == 44100u && abs(d - 100 * CTL / 4) < 4);
        }
        sys.bpm = 120;
        fm1_in.notes = 0;
        render_poll(20);
        check("..let go: it records again", (uint32_t)gbuf[0].w != w0);
    }

    {   /* the 0 key tapped: the freeze latches (as GLO's mixer: held momentary, tapped latched) */
        uint32_t h0, n0 = 0;
        for (n0 = 0; n0 < 27u && KEY_BLACK[n0] != BK_ZERO; n0++)
            ;
        fm1_in.notes = 1u << n0;
        key_edge(n0);
        fm1_in.notes = 0;
        ui_input();
        render_poll(4);
        h0 = buf_sum(0);
        render_poll(1378);
        check("the 0 key tapped: the freeze latches (the buffer holds after the key is up), and its LED lights",
              sys.freeze && buf_sum(0) == h0 && grain_frozen);
        ui_leds();
        led_on(fm1_led, 14u + n0);
        check("..its LED lit", led_lit);
        fm1_in.notes = 1u << n0;
        key_edge(n0);
        fm1_in.notes = 0;
        ui_input();
        render_poll(20);
        check("..tapped again: let go, it records again", !sys.freeze && buf_sum(0) != h0);
    }

    p[GP_SCAN] = SCAN_POS;                               /* POS: OFST 50 holds the cursor halfway */
    p[GP_OFST] = 50;
    render_poll(20);
    check("SCAN POS, OFST 50: the cursor halfway through the buffer, still", (grain[0].cur >> 12) == 22050);
    press(B_EDIT);
    fm1_in.notes = note_bit_of_white(4);
    render_poll(2);
    check("..on the GRAIN page, white key 5: the cursor to the buffer's fifth sixteenth", (grain[0].cur >> 12) == 44100 * 4 / 16);
    fm1_in.notes = 0;
    p[GP_OFST] = 60;
    render_poll(2);
    check("..OFST turned again takes over", (grain[0].cur >> 12) == 44100 * 60 / 100);
    press(B_HOME);
    p[GP_SCAN] = SCAN_DLY;                               /* DLY: OFST 25 behind the write head */
    p[GP_OFST] = 25;
    render_poll(20);
    {
        int32_t lag = gbuf[0].w - (grain[0].cur >> 12);
        lag = lag < 0 ? lag + (int32_t)gbuf[0].len : lag;
        snprintf(b, sizeof b, "SCAN DLY, OFST 25: the cursor a quarter of the buffer behind the write head (%d)", (int)lag);
        check(b, abs(lag - 44100 / 4) < CTL);
    }
    p[GP_SCAN] = SCAN_STR;

    {   /* SYNTH played with the transport stopped: it goes into the buffer and the grains play it */
        static int32_t o[400 * CTL];
        sys.playing = 0;
        render_poll(400);
        tp[0].src = SRC_SYNTH;
        p[GP_WET] = 100;
        fm1_in.notes = note_bit_of_white(9);
        render_poll(1378);
        fm1_in.notes = 0;
        check("SYNTH, stopped: what you play goes into the buffer", buf_loud(0, 0, 172) > 60u);
        render(400, o);
        check("..and grains of it sound after the note ends (no transport needed)", grain_count(0) > 0 &&
              peak_of(o + 200 * CTL, 200 * CTL) > 500);
        tp[0].src = SRC_TAPE;
        sys.playing = 1;
        render_poll(1378 * 2);
    }

    {   /* the pool follows how many tracks have GRAIN on (40, 30, 18), shared by what each asks (the other tracks
         * heard here: a muted track's grains take no share) */
        uint32_t mx = 0, t2;
        int16_t *q = tp[1].dev[DEV_GRAIN];
        for (t2 = 1; t2 < NTRK; t2++)
            track[t2].mute = 0;
        p[GP_RATE] = 100;
        p[GP_SIZE] = 500;
        render_poll(1378);
        for (i = 0; i < 1378; i++) {
            render_poll(1);
            mx = grain_count(0) > mx ? grain_count(0) : mx;
        }
        snprintf(b, sizeof b, "GRAIN on one track, dense (RATE 100, SIZE 500): the whole pool, %u at once (40)", mx);
        check(b, mx == 40u && gr_allow(0) == 40u);
        q[GP_WET] = 100;                                 /* track 2 on, sparse (the defaults: about 1 at once) */
        render_poll(64);                                 /* (its buffer has the tape's sound in it) */
        snprintf(b, sizeof b, "..a second track on, sparse: a pool of 30, it gets the %u it asks, the dense one the other %u",
                 gr_allow(1), gr_allow(0));
        check(b, gr_allow(1) == 3u && gr_allow(0) == 27u);
        for (i = 0, mx = 0; i < 1378; i++) {
            render_poll(1);
            mx = grain_count(0) > mx ? grain_count(0) : mx;
        }
        snprintf(b, sizeof b, "..the dense one thins to its share (%u at most), no grain cut: those sounding finished", mx);
        check(b, mx <= 40u && grain_count(0) <= 27u);
        q[GP_RATE] = 100;                                /* both dense: an even split */
        q[GP_SIZE] = 500;
        render_poll(1);
        check("..both dense: 15 and 15", gr_allow(0) == 15u && gr_allow(1) == 15u);
        tp[2].dev[DEV_GRAIN][GP_WET] = 100;              /* four on: a pool of 18; track 4 sparse, the rest dense */
        tp[2].dev[DEV_GRAIN][GP_RATE] = 100;
        tp[2].dev[DEV_GRAIN][GP_SIZE] = 500;
        tp[3].dev[DEV_GRAIN][GP_WET] = 100;
        render_poll(64);
        for (t2 = 0, mx = 0; t2 < NTRK; t2++)
            mx += gr_allow(t2);
        snprintf(b, sizeof b, "..four on, one sparse: a pool of 18: %u %u %u, and the sparse one its %u", gr_allow(0),
                 gr_allow(1), gr_allow(2), gr_allow(3));
        check(b, mx == 18u && gr_allow(3) == 3u && gr_allow(0) == 5u && gr_allow(1) == 5u && gr_allow(2) == 5u);
        chain_tracks(2);
        render_poll(1);
        check("TRACKS 2 (tracks 3 and 4 parked): two on, 30 between them, 15 and 15", gr_allow(0) == 15u &&
              gr_allow(1) == 15u && gr_allow(2) == 0u && gr_allow(3) == 0u);
        chain_tracks(4);
        for (t2 = 1; t2 < NTRK; t2++) {
            tp[t2].dev[DEV_GRAIN][GP_WET] = 0;
            tp[t2].dev[DEV_GRAIN][GP_RATE] = 40;
            tp[t2].dev[DEV_GRAIN][GP_SIZE] = 80;
        }
        p[GP_RATE] = 40;
        p[GP_SIZE] = 80;
        for (t2 = 1; t2 < NTRK; t2++)
            track[t2].mute = 1;
        render_poll(1378);
    }

    {   /* nothing free: a buffer takes from the end of the longest tape */
        sys.playing = 0;
        render_poll(400);
        p[GP_WET] = 0;
        chain_poll();
        fill_tape(2, MEM_NC * 2u / 3u);                  /* (two thirds and the rest: 100 and 50 of 150) */
        fill_tape(3, MEM_NC - MEM_NC * 2u / 3u);
        while (mem_count(MEM_FREE))
            mem_alloc(MEM_IMPORT);
        p[GP_WET] = 100;
        chain_poll();
        check("no chunk free: GRAIN's buffer takes from the end of the longest tape (track 3's)",
              gbuf[0].nch == 11 && tape_ctl[2].nch == MEM_NC * 2u / 3u - 11u &&
              tape_ctl[3].nch == MEM_NC - MEM_NC * 2u / 3u);
    }
    sys.playing = 0;
    for (t = 0; t < NTRK; t++)
        track[t].mute = 0;
    render_poll(400);
}

/* a buffer's pitch by autocorrelation (partials and noise don't fool it as they do zero crossings), Hz */
static double buf_pitch(const int32_t *x, uint32_t from, uint32_t n, double fmin, double fmax)
{
    uint32_t lag, lo = (uint32_t)(44100.0 / fmax), hi = (uint32_t)(44100.0 / fmin), best = 0, i;
    double bc = -1e300, c[2048];
    for (lag = lo; lag <= hi && lag < 2048u; lag++) {
        double a = 0, e0 = 0, e1 = 0;
        for (i = from; i + lag < n; i++) {
            a += (double)x[i] * x[i + lag];
            e0 += (double)x[i] * x[i];
            e1 += (double)x[i + lag] * x[i + lag];
        }
        c[lag] = a / (sqrt(e0 * e1) + 1);
        if (c[lag] > bc) {
            bc = c[lag];
            best = lag;
        }
    }
    for (lag = lo; lag < best; lag++)                    /* (the shortest lag nearly as good: not an octave down) */
        if (c[lag] >= 0.9 * bc && (lag == lo || c[lag] >= c[lag - 1]) && c[lag] >= c[lag + 1]) {
            best = lag;
            break;
        }
    if (best > lo && best < hi) {                        /* (between samples: a parabola through the peak) */
        double y0 = c[best - 1], y1 = c[best], y2 = c[best + 1], d = y0 - 2 * y1 + y2;
        return 44100.0 / (best + (d ? 0.5 * (y0 - y2) / d : 0));
    }
    return best ? 44100.0 / best : 0;
}
static double buf_rms(const int32_t *x, uint32_t from, uint32_t n)
{
    double a = 0;
    uint32_t i;
    for (i = from; i < n; i++)
        a += (double)x[i] * x[i];
    return sqrt(a / (n - from + 1));
}

/* RIPPLE 3's pluck shape: a fresh track 1 with the strings up, PICK STRM VARY set, and one key on its page (A3) */
static void rpk_setup(int pick, int strm, int vary)
{
    int16_t *p;
    uint32_t t;
    power_on();
    p = tp[0].dev[DEV_RESO];
    for (t = 1; t < NTRK; t++)
        track[t].mute = 1;
    focus_dev(DEV_RESO, 0);
    ui_input();
    p[RP_WET] = 100;
    p[RP_PICK] = (int16_t)pick;
    p[RP_STRM] = (int16_t)strm;
    p[RP_VARY] = (int16_t)vary;
    chain_poll();
    render(4, 0);
}
static void rpk_key(void)
{
    fm1_in.notes = note_bit_of_white(9);
    render(1, 0);
    fm1_in.notes = 0;
}
static double rpk_bright(uint32_t k)                     /* string k's line: its sample-to-sample movement per level */
{
    const int16_t *d = rs_line(&reso[0], k);
    double mv = 0, lv = 0;
    uint32_t i;
    for (i = 1; i < RS_LEN; i++) {
        mv += fabs((double)d[i] - d[i - 1]);
        lv += fabs((double)d[i]);
    }
    return lv > 0 ? mv / lv : 0;
}
static double rpk_energy(uint32_t k)
{
    const int16_t *d = rs_line(&reso[0], k);
    double e = 0;
    uint32_t i;
    for (i = 0; i < RS_LEN; i++)
        e += (double)d[i] * d[i];
    return e;
}
/* a muted track sleeps (chain.c): its source keeps its time and makes no sound. The factory scene twice from power-on,
 * once with tracks 1, 3 and 4 muted for 3 s: the heads and the pattern end where they'd have been, nothing of theirs
 * was rendered, and unmuted they sound again */
static void test_sleep(void)
{
    char b[200];
    int32_t pos_awake, pos_asleep, xpos_awake, xpos_asleep;
    uint32_t now_awake, now_asleep, w_awake, w_asleep, k, voices = 0, drums = 0;
    power_on();
    pj_wipe();
    pj_boot();
    sys.playing = 1;
    render_poll(1378 * 3);
    pos_awake = tape_rt[2].pos;
    xpos_awake = tape_rt[2].xpos;
    now_awake = drm_now[0];
    w_awake = (uint32_t)gbuf[2].w;
    power_on();
    pj_wipe();
    pj_boot();
    sys.playing = 1;
    track[0].mute = track[2].mute = track[3].mute = 1;
    render_poll(1378 * 3);
    pos_asleep = tape_rt[2].pos;
    xpos_asleep = tape_rt[2].xpos;
    now_asleep = drm_now[0];
    w_asleep = (uint32_t)gbuf[2].w;
    for (k = 0; k < POL_NV; k++)
        voices += pol[3].v[k].stage != 0u;
    for (k = 0; k < DRM_NV; k++)
        drums += drm_rt[0].v[k].live;
    snprintf(b, sizeof b, "muted, asleep 3 s: T3's tape head where it would be (%d, %d), T1's pattern on its step (%u, "
             "%u), T3's buffer head in its place (%u, %u)", pos_asleep >> 12, pos_awake >> 12, now_asleep, now_awake,
             w_asleep, w_awake);
    check(b, pos_asleep == pos_awake && xpos_asleep == xpos_awake && now_asleep == now_awake && w_asleep == w_awake);
    check("..and nothing of theirs sounding: no POLY voice on T4, no drum voice on T1, no grains on T3",
          voices == 0 && drums == 0 && grain_count(2) == 0 && grain_count(0) == 0);
    track[0].mute = track[2].mute = track[3].mute = 0;
    k = grain[2].n;
    render_poll(1378);
    check("..unmuted: they sound again (T1's drums, T3's tape and the grains starting on it, T4's voices)",
          peak_of(track_rt[0].last, CTL) > 0 && peak_of(track_rt[2].last, CTL) > 0 && peak_of(track_rt[3].last, CTL) > 0
          && grain[2].n > k);
    power_on();
}

static void test_reso_pluck(void)
{
    char b[200];
    double soft, hard, e3a, e3b;
    int32_t l1[RS_N], l2[RS_N];
    uint32_t k, same;
    rpk_setup(-60, 0, 0);
    rpk_key();
    soft = rpk_bright(0);
    rpk_setup(60, 0, 0);
    rpk_key();
    hard = rpk_bright(0);
    snprintf(b, sizeof b, "RIPPLE PICK: -60 plucks soft (a string's burst moves %.2f a sample per level), +60 hard "
             "(%.2f)", soft, hard);
    check(b, hard > 1.5 * soft && soft > 0);
    rpk_setup(0, 50, 0);
    rpk_key();
    e3a = rpk_energy(3);
    snprintf(b, sizeof b, "..STRM 50: the root string at once, the next 50 ms on, the top one 150 (%u samples to go)",
             reso[0].pend[3]);
    check(b, rpk_energy(0) > 0 && e3a == 0 && reso[0].pend[1] > 2100u && reso[0].pend[1] <= 2205u &&
          reso[0].pend[3] > 6500u && reso[0].pend[3] <= 6615u);
    render(190, 0);                                      /* (138 ms) */
    e3b = rpk_energy(3);
    render(30, 0);                                       /* (160 ms in all) */
    check("..each string as its time comes: the top one still waiting at 138 ms, plucked by 160",
          e3b == 0 && rpk_energy(3) > 0 && !reso[0].pend[3]);
    rpk_key();
    render(30, 0);
    rpk_key();
    check("..a new pluck drops a strum still under way, and starts its own", reso[0].pend[3] > 6400u);
    rpk_setup(0, 0, 0);
    rpk_key();
    for (k = 0, same = 1; k < RS_N; k++)
        same &= reso[0].plv[k] == 32767;
    check("..VARY 0: every pluck and string at full level (the pluck as it was)", same);
    rpk_setup(0, 0, 100);
    rpk_key();
    for (k = 0; k < RS_N; k++)
        l1[k] = reso[0].plv[k];
    render(10, 0);
    rpk_key();
    for (k = 0, same = 0; k < RS_N; k++) {
        l2[k] = reso[0].plv[k];
        same += l1[k] == l2[k];
    }
    snprintf(b, sizeof b, "..VARY 100: each pluck its own levels (%d %d %d %d, then %d %d %d %d), none under a quarter",
             l1[0], l1[1], l1[2], l1[3], l2[0], l2[1], l2[2], l2[3]);
    check(b, same == 0 && l1[0] >= 8192 && l2[3] >= 8192 && l1[0] != l1[1]);
    power_on();
}

static double goertzel(const int32_t *x, uint32_t from, uint32_t to, double hz);   /* (below, with SPACE's tests) */
/* SYNTH's FM (2026-10-13): WAVE FM, a modulator sine bending a carrier's phase. A3 held (220 Hz), the filter open:
 * INDX 0 a sine; INDX up, the harmonics; RATI 2, the odd ones only (the sidebands at 220 +- 2 x 220 k); IENV, bright
 * at the strike and mellower after; FDBK more again */
static double fm_h(const int32_t *x, uint32_t a, uint32_t b, uint32_t h) { return sqrt(goertzel(x, a, b, 220.0 * h)); }
static double fm_rich(const int32_t *x)                  /* harmonics 2-8 over the 1st, A3 over 300 blocks */
{
    double r = 0;
    uint32_t h;
    for (h = 2; h <= 8u; h++)
        r += fm_h(x, 100 * CTL, 400 * CTL, h);
    return r / (fm_h(x, 100 * CTL, 400 * CTL, 1) + 1);
}
static void test_fm(void)
{
    enum { NB = 400 };
    static int32_t s[NB * CTL];
    int16_t *p;
    char b[200];
    double h1, h2, h3, e1, e2, f1;
    power_on();
    tp[0].src = SRC_SYNTH;
    p = tp[0].syn;
    p[SY_WAVE] = SY_FM;
    p[SY_CUT] = 127;
    p[SY_ENV] = 0;
    p[SY_RES] = 0;
    p[SY_SUS] = 100;
    p[SY_DTUN] = 8;                                      /* RATI 1 */
    p[SY_MIX] = 0;                                       /* INDX 0, IENV 0: the carrier alone */
    p[SY_IENV] = 0;
    p[SY_NOIS] = 0;
    focus_dev(DEV_SRC, 0);
    check("WAVE FM: the header says FM SYNTH, page 1 is WAVE RATI INDX IENV, page 5 FDBK KTRK DRV, RATI 1 at 8",
          !strcmp(dev_name(0, DEV_SRC), "FM SYNTH") && dev_p(0, DEV_SRC) == SYN_FM_P &&
          pdesc_at(SYN_FM_P, 1) == SY_DTUN && pdesc_at(SYN_FM_P, 3) == SY_IENV && pdesc_at(SYN_FM_P, 16) == SY_NOIS &&
          N_RATIO[syn_ratio(8)][0] == '1' && N_RATIO[syn_ratio(8)][1] == 0 && N_RATIO[syn_ratio(100)][1] == '4');
    fm1_in.notes = note_bit_of_white(9);
    render(NB, s);
    h1 = fm_h(s, 100 * CTL, NB * CTL, 1);
    h2 = fm_h(s, 100 * CTL, NB * CTL, 2);
    h3 = fm_h(s, 100 * CTL, NB * CTL, 3);
    snprintf(b, sizeof b, "FM, INDX 0: a sine at A3 (2nd %.4f, 3rd %.4f of the 1st)", h2 / h1, h3 / h1);
    check(b, h1 > 0 && h2 < h1 / 100 && h3 < h1 / 100);
    p[SY_MIX] = 60;                                      /* INDX 60, RATI 1: every harmonic */
    render(NB, s);
    h1 = fm_h(s, 100 * CTL, NB * CTL, 1);
    h2 = fm_h(s, 100 * CTL, NB * CTL, 2);
    h3 = fm_h(s, 100 * CTL, NB * CTL, 3);
    snprintf(b, sizeof b, "..INDX 60, RATI 1: the harmonics (2nd %.2f, 3rd %.2f of the 1st)", h2 / h1, h3 / h1);
    check(b, h2 > h1 / 10 && h3 > h1 / 20);
    p[SY_DTUN] = 28;                                     /* RATI 2: the odd harmonics alone */
    render(NB, s);
    h1 = fm_h(s, 100 * CTL, NB * CTL, 1);
    h2 = fm_h(s, 100 * CTL, NB * CTL, 2);
    h3 = fm_h(s, 100 * CTL, NB * CTL, 3);
    snprintf(b, sizeof b, "..RATI %s: the odd harmonics only (2nd %.4f, 3rd %.2f of the 1st)", N_RATIO[syn_ratio(28)],
             h2 / h1, h3 / h1);
    check(b, !strcmp(N_RATIO[syn_ratio(28)], "2") && h3 > h1 / 10 && h2 < h3 / 30);
    p[SY_DTUN] = 8;
    p[SY_MIX] = 0;                                       /* IENV 100, DEC short, SUS 0: the pluck's brightness */
    p[SY_IENV] = 100;
    p[SY_SUS] = 0;
    p[SY_DEC] = 70;
    fm1_in.notes = 0;
    render(NB, 0);
    fm1_in.notes = note_bit_of_white(9);
    render(NB, s);
    e1 = fm_h(s, 4 * CTL, 40 * CTL, 3) / (fm_h(s, 4 * CTL, 40 * CTL, 1) + 1);
    e2 = fm_h(s, 200 * CTL, 236 * CTL, 3) / (fm_h(s, 200 * CTL, 236 * CTL, 1) + 1);
    snprintf(b, sizeof b, "..IENV 100: bright at the strike, mellower as the envelope falls (3rd over 1st %.3f, then %.3f)",
             e1, e2);
    check(b, e1 > 3 * e2);
    p[SY_IENV] = 0;
    p[SY_SUS] = 100;
    p[SY_MIX] = 30;
    render(NB, s);
    f1 = fm_rich(s);
    p[SY_NOIS] = 80;                                     /* FDBK 80 */
    render(NB, s);
    snprintf(b, sizeof b, "..FDBK 80: another timbre (harmonics 2-8 over the 1st %.3f, %.3f without; the modulator "
             "saw-ish: it moves the spectrum, not only up), bounded", fm_rich(s), f1);
    check(b, (fm_rich(s) > 1.25 * f1 || fm_rich(s) < f1 / 1.25) && peak_of(s, NB * CTL) < 32767);
    p[SY_NOIS] = 0;
    p[SY_MIX] = 50;
    p[SY_IENV] = 60;
    ui.msg_t = 0;
    shot("BRYO", "synth_fm");
    fm1_in.notes = 0;
    render(NB, 0);
}

/* SYNTH's PD (2026-10-13): WAVE PD, the CZ's phase distortion. A3 held, the filter open: AMT 0 a sine; SAW at 100 a
 * saw's harmonics (the nth about 1/n), SQR the odd ones; AENV the sweep; a resonant shape's AMT moves its peak up */
static double pd_band(const int32_t *x, uint32_t a, uint32_t b, uint32_t lo, uint32_t hi)
{
    double e = 0;
    uint32_t h;
    for (h = lo; h <= hi; h++)
        e += fm_h(x, a, b, h);
    return e;
}
static void test_pd(void)
{
    enum { NB = 400 };
    static int32_t s[NB * CTL];
    int16_t *p;
    char b[200];
    double h1, h2, h3, lo, hi, lo2, hi2;
    power_on();
    tp[0].src = SRC_SYNTH;
    p = tp[0].syn;
    p[SY_WAVE] = SY_PD;
    p[SY_CUT] = 127;
    p[SY_ENV] = 0;
    p[SY_RES] = 0;
    p[SY_SUS] = 100;
    p[SY_DTUN] = 0;                                      /* SHAP SAW */
    p[SY_MIX] = 0;                                       /* AMT 0, AENV 0 */
    p[SY_IENV] = 0;
    p[SY_NOIS] = 0;
    focus_dev(DEV_SRC, 0);
    check("WAVE PD: the header says PD SYNTH, page 1 WAVE SHAP AMT AENV, SHAP's eight shapes",
          !strcmp(dev_name(0, DEV_SRC), "PD SYNTH") && dev_p(0, DEV_SRC) == SYN_PD_P && pdesc_at(SYN_PD_P, 3) == SY_IENV &&
          !strcmp(N_PDSH[syn_pdsh(0)], "SAW") && !strcmp(N_PDSH[syn_pdsh(100)], "RTRP") && !strcmp(N_PDSH[syn_pdsh(8)], "SAW"));
    fm1_in.notes = note_bit_of_white(9);
    render(NB, s);
    h1 = fm_h(s, 100 * CTL, NB * CTL, 1);
    h2 = fm_h(s, 100 * CTL, NB * CTL, 2);
    snprintf(b, sizeof b, "PD, AMT 0: a sine (2nd %.4f of the 1st)", h2 / h1);
    check(b, h1 > 0 && h2 < h1 / 100);
    p[SY_MIX] = 100;                                     /* SAW, AMT 100 */
    render(NB, s);
    h1 = fm_h(s, 100 * CTL, NB * CTL, 1);
    h2 = fm_h(s, 100 * CTL, NB * CTL, 2);
    h3 = fm_h(s, 100 * CTL, NB * CTL, 3);
    snprintf(b, sizeof b, "..SAW, AMT 100: a saw's harmonics (2nd %.2f, 3rd %.2f of the 1st; a saw's 0.50 0.33)", h2 / h1,
             h3 / h1);
    check(b, h2 / h1 > 0.3 && h2 / h1 < 0.7 && h3 / h1 > 0.18 && h3 / h1 < 0.5);
    p[SY_DTUN] = 13;                                     /* SQR */
    render(NB, s);
    h1 = fm_h(s, 100 * CTL, NB * CTL, 1);
    h2 = fm_h(s, 100 * CTL, NB * CTL, 2);
    h3 = fm_h(s, 100 * CTL, NB * CTL, 3);
    snprintf(b, sizeof b, "..%s, AMT 100: the odd harmonics (2nd %.4f, 3rd %.2f of the 1st)", N_PDSH[syn_pdsh(13)], h2 / h1,
             h3 / h1);
    check(b, !strcmp(N_PDSH[syn_pdsh(13)], "SQR") && h3 > h1 / 10 && h2 < h3 / 20);
    p[SY_DTUN] = 0;                                      /* AENV 100, AMT 0, a short DEC to SUS 0: the sweep */
    p[SY_MIX] = 0;
    p[SY_IENV] = 100;
    p[SY_SUS] = 0;
    p[SY_DEC] = 70;
    fm1_in.notes = 0;
    render(NB, 0);
    fm1_in.notes = note_bit_of_white(9);
    render(NB, s);
    lo = fm_h(s, 4 * CTL, 40 * CTL, 3) / (fm_h(s, 4 * CTL, 40 * CTL, 1) + 1);
    hi = fm_h(s, 200 * CTL, 236 * CTL, 3) / (fm_h(s, 200 * CTL, 236 * CTL, 1) + 1);
    snprintf(b, sizeof b, "..AENV 100: bent at the strike, a sine again as the envelope falls (3rd over 1st %.3f, then %.3f)",
             lo, hi);
    check(b, lo > 0.15 && hi < lo / 3);
    p[SY_IENV] = 0;
    p[SY_SUS] = 100;
    p[SY_DTUN] = 70;                                     /* RSAW: AMT 10, then 90: its peak moves up */
    p[SY_MIX] = 10;
    render(NB, s);
    lo = pd_band(s, 100 * CTL, NB * CTL, 1, 4);
    hi = pd_band(s, 100 * CTL, NB * CTL, 8, 16);
    p[SY_MIX] = 90;
    render(NB, s);
    lo2 = pd_band(s, 100 * CTL, NB * CTL, 1, 4);
    hi2 = pd_band(s, 100 * CTL, NB * CTL, 8, 16);
    snprintf(b, sizeof b, "..%s: AMT 10 -> 90 moves its resonance up (8th-16th over 1st-4th %.2f -> %.2f), bounded",
             N_PDSH[syn_pdsh(70)], hi / (lo + 1), hi2 / (lo2 + 1));
    check(b, !strcmp(N_PDSH[syn_pdsh(70)], "RSAW") && hi2 / (lo2 + 1) > 4 * hi / (lo + 1) && peak_of(s, NB * CTL) < 32767);
    p[SY_DTUN] = 0;
    p[SY_MIX] = 60;
    p[SY_IENV] = 30;
    ui.msg_t = 0;
    shot("BRYO", "synth_pd");
    fm1_in.notes = 0;
    render(NB, 0);
}

/* SYNTH's CHRD (2026-10-13): a key plays a chord on the three voices; the lowest key held is its root */
static void test_chord(void)
{
    enum { NB = 300 };
    static int32_t s[NB * CTL];
    int16_t *p;
    char b[200];
    double r1, r3, r5, off;
    power_on();
    tp[0].src = SRC_SYNTH;
    p = tp[0].syn;
    p[SY_WAVE] = 0;
    p[SY_DTUN] = 0;
    p[SY_MIX] = 0;
    p[SY_CUT] = 127;
    p[SY_ENV] = 0;
    p[SY_SUS] = 100;
    p[SY_CHRD] = 1;                                      /* MAJ */
    fm1_in.notes = note_bit_of_white(9);                 /* A3 */
    render(NB, s);
    r1 = sqrt(goertzel(s, 100 * CTL, NB * CTL, 220.0));
    r3 = sqrt(goertzel(s, 100 * CTL, NB * CTL, 277.18));
    r5 = sqrt(goertzel(s, 100 * CTL, NB * CTL, 329.63));
    off = sqrt(goertzel(s, 100 * CTL, NB * CTL, 246.94));
    snprintf(b, sizeof b, "CHRD MAJ, A3: A C# E (%.0f %.0f %.0f), B none (%.0f), three voices", r1, r3, r5, off);
    check(b, r3 > r1 / 2 && r5 > r1 / 2 && off < r1 / 8 && syn_sounding(0) == 3);   /* (B: the neighbours' leakage, unwindowed) */
    fm1_in.notes = 0;
    render(NB, 0);
    check("..let go: every voice released", syn_sounding(0) == 0);
    p[SY_CHRD] = 5;                                      /* DOM7: root, third, seventh */
    fm1_in.notes = note_bit_of_white(9);
    render(NB, s);
    r5 = sqrt(goertzel(s, 100 * CTL, NB * CTL, 329.63));
    off = sqrt(goertzel(s, 100 * CTL, NB * CTL, 392.0));
    snprintf(b, sizeof b, "..DOM7: the seventh G (%.0f), the fifth E left out (%.0f)", off, r5);
    check(b, off > r1 / 2 && r5 < off / 20);
    fm1_in.notes |= note_bit_of_white(7);                /* G3 down too: the root moves there, legato */
    render(2, 0);
    {
        uint32_t j, legato = 1;
        for (j = 0; j < SYN_NV; j++)
            legato &= syn[0].v[j].stage == 2u && syn[0].v[j].to16 == (55 + SYN_CHORD[5][j]) * 16;
        check("..G3 held as well: the chord moves to G3 (its lowest key), legato (no new attack)", legato);
    }
    fm1_in.notes = 0;
    p[SY_CHRD] = 0;
    render(NB, 0);
}

/* SYNTH's VOX (2026-10-13): a saw through three formants. A2 held (110 Hz: harmonics every 110), the filter open: the
 * energy where each vowel's formants are */
static double vx_band(const int32_t *x, uint32_t a, uint32_t b, double lo, double hi)
{
    double e = 0, hz;
    for (hz = 110; hz <= hi; hz += 110)
        if (hz >= lo)
            e += sqrt(goertzel(x, a, b, hz));
    return e;
}
static void test_vox(void)
{
    enum { NB = 300 };
    static int32_t s[NB * CTL];
    int16_t *p;
    char b[220];
    double a1, a2, i1, i2, u1, u2;
    power_on();
    tp[0].src = SRC_SYNTH;
    p = tp[0].syn;
    p[SY_WAVE] = SY_VOX;
    p[SY_CUT] = 127;
    p[SY_ENV] = 0;
    p[SY_RES] = 0;
    p[SY_SUS] = 100;
    p[SY_DTUN] = 0;                                      /* VOWL A */
    p[SY_MIX] = 50;                                      /* SHFT 0 */
    p[SY_IENV] = 0;
    track[0].octave = 2;
    focus_dev(DEV_SRC, 0);
    {
        char v1[8], v2[8], v3[8];
        const char *u;
        param_format(&SYN_VX_P[SY_DTUN], 0, v1, &u);
        param_format(&SYN_VX_P[SY_DTUN], 50, v2, &u);
        param_format(&SYN_VX_P[SY_DTUN], 37, v3, &u);
        check("WAVE VOX: the header says VOX SYNTH, page 1 WAVE VOWL SHFT VENV, VOWL reads A, I, and E-I between",
              !strcmp(dev_name(0, DEV_SRC), "VOX SYNTH") && dev_p(0, DEV_SRC) == SYN_VX_P && !strcmp(v1, "A") &&
              !strcmp(v2, "I") && !strcmp(v3, "E-I"));
    }
    fm1_in.notes = note_bit_of_white(9);                 /* A2 */
    render(NB, s);
    a1 = vx_band(s, 100 * CTL, NB * CTL, 600, 900);      /* (A's first formant, 730) */
    a2 = vx_band(s, 100 * CTL, NB * CTL, 2000, 2600);    /* (I's second, 2290) */
    p[SY_DTUN] = 50;                                     /* VOWL I */
    render(NB, s);
    i1 = vx_band(s, 100 * CTL, NB * CTL, 600, 900);
    i2 = vx_band(s, 100 * CTL, NB * CTL, 2000, 2600);
    snprintf(b, sizeof b, "VOX: A is strong at 600-900 Hz against 2-2.6 kHz (%.1f), I the other way (%.1f)", a1 / (a2 + 1),
             i1 / (i2 + 1));
    check(b, a1 / (a2 + 1) > 2 * i1 / (i2 + 1) && i2 > a2);
    p[SY_DTUN] = 0;
    p[SY_MIX] = 100;                                     /* SHFT +100: A's formants a ratio of 1.5 up */
    render(NB, s);
    u1 = vx_band(s, 100 * CTL, NB * CTL, 1000, 1250);    /* (730 x 1.5: 1,095) */
    u2 = vx_band(s, 100 * CTL, NB * CTL, 600, 800);
    {
        double up = u1 / (u2 + 1);
        p[SY_MIX] = 50;
        render(NB, s);
        u1 = vx_band(s, 100 * CTL, NB * CTL, 1000, 1250);
        u2 = vx_band(s, 100 * CTL, NB * CTL, 600, 800);
        snprintf(b, sizeof b, "..SHFT +100: A's first formant moves up (1-1.25 kHz over 600-800 Hz: %.2f, %.2f at 0)", up,
                 u1 / (u2 + 1));
        check(b, up > 2 * u1 / (u2 + 1));
    }
    p[SY_IENV] = 50;                                     /* VENV +50: the vowel moves from A toward I at the strike */
    p[SY_SUS] = 0;
    p[SY_DEC] = 70;
    fm1_in.notes = 0;
    render(NB, 0);
    fm1_in.notes = note_bit_of_white(9);
    render(NB, s);
    snprintf(b, sizeof b, "..VENV +50: at the strike higher up (2-2.6 kHz over 600-900 Hz %.2f, then %.2f)",
             vx_band(s, 4 * CTL, 40 * CTL, 2000, 2600) / (vx_band(s, 4 * CTL, 40 * CTL, 600, 900) + 1),
             vx_band(s, 200 * CTL, 236 * CTL, 2000, 2600) / (vx_band(s, 200 * CTL, 236 * CTL, 600, 900) + 1));
    check(b, vx_band(s, 4 * CTL, 40 * CTL, 2000, 2600) / (vx_band(s, 4 * CTL, 40 * CTL, 600, 900) + 1) >
             2 * vx_band(s, 200 * CTL, 236 * CTL, 2000, 2600) / (vx_band(s, 200 * CTL, 236 * CTL, 600, 900) + 1) &&
             peak_of(s, NB * CTL) < 32767);
    p[SY_IENV] = 30;
    p[SY_SUS] = 100;
    p[SY_DTUN] = 20;
    ui.msg_t = 0;
    shot("BRYO", "synth_vox");
    fm1_in.notes = 0;
    render(NB, 0);
    track[0].octave = 3;
}

/* SYNTH's WT (2026-10-13): a wavetable of eight frames in two chunks of the shared memory, taken while WAVE is WT */
static void test_wt(void)
{
    enum { NB = 300 };
    static int32_t s[NB * CTL];
    int16_t *p;
    char b[220];
    double h1, h2, h3, off;
    power_on();
    tp[0].src = SRC_SYNTH;
    p = tp[0].syn;
    p[SY_WAVE] = SY_WT;
    p[SY_TABL] = 0;                                      /* BASC */
    p[SY_DTUN] = 0;                                      /* POS 0: its first frame, a sine */
    p[SY_IENV] = 0;
    p[SY_CUT] = 127;
    p[SY_ENV] = 0;
    p[SY_SUS] = 100;
    chain_poll();
    focus_dev(DEV_SRC, 0);
    check("WAVE WT: two chunks of the shared memory, the table built, the header WT SYNTH, page 1 WAVE TABL POS PENV",
          mem_count(MEM_WT) == 2u && wt[0].ready && wt[0].built == 0 && !strcmp(dev_name(0, DEV_SRC), "WT SYNTH") &&
          pdesc_at(SYN_WT_P, 1) == SY_TABL && pdesc_at(SYN_WT_P, 2) == SY_DTUN && pdesc_at(SYN_WT_P, 3) == SY_IENV);
    fm1_in.notes = note_bit_of_white(9);
    render(NB, s);
    h1 = fm_h(s, 100 * CTL, NB * CTL, 1);
    h2 = fm_h(s, 100 * CTL, NB * CTL, 2);
    snprintf(b, sizeof b, "WT BASC, POS 0: a sine at A3 (2nd %.4f of the 1st)", h2 / h1);
    check(b, h1 > 0 && h2 < h1 / 50);
    p[SY_DTUN] = 58;                                     /* POS 58: frame 4, the saw */
    render(NB, s);
    h1 = fm_h(s, 100 * CTL, NB * CTL, 1);
    h2 = fm_h(s, 100 * CTL, NB * CTL, 2);
    h3 = fm_h(s, 100 * CTL, NB * CTL, 3);
    snprintf(b, sizeof b, "..POS 58: the saw's frame (2nd %.2f, 3rd %.2f of the 1st)", h2 / h1, h3 / h1);
    check(b, h2 / h1 > 0.35 && h2 / h1 < 0.65 && h3 / h1 > 0.2);
    p[SY_DTUN] = 0;                                      /* PENV +100, SUS 0: the strike far into the table */
    p[SY_IENV] = 100;
    p[SY_SUS] = 0;
    p[SY_DEC] = 70;
    fm1_in.notes = 0;
    render(NB, 0);
    fm1_in.notes = note_bit_of_white(9);
    render(NB, s);
    h1 = fm_h(s, 4 * CTL, 40 * CTL, 2) / (fm_h(s, 4 * CTL, 40 * CTL, 1) + 1);
    h2 = fm_h(s, 200 * CTL, 236 * CTL, 2) / (fm_h(s, 200 * CTL, 236 * CTL, 1) + 1);
    snprintf(b, sizeof b, "..PENV +100: the strike far into the table, back toward the sine (2nd over 1st %.3f, then %.3f)",
             h1, h2);
    check(b, h1 > 3 * h2);
    p[SY_IENV] = 0;
    p[SY_SUS] = 100;
    p[SY_TABL] = (int16_t)(WT_NBUILT + 2u);              /* BELL: a reel cut into frames */
    chain_poll();
    render(NB, s);
    h1 = fm_h(s, 100 * CTL, NB * CTL, 1) + fm_h(s, 100 * CTL, NB * CTL, 2) + fm_h(s, 100 * CTL, NB * CTL, 3);
    off = sqrt(goertzel(s, 100 * CTL, NB * CTL, 330.0)) + sqrt(goertzel(s, 100 * CTL, NB * CTL, 550.0)) +
          sqrt(goertzel(s, 100 * CTL, NB * CTL, 770.0));
    snprintf(b, sizeof b, "..TABL %s: a reel's frames, played as cycles at A3 (220 440 660 over 330 550 770: %.1f)",
             N_WTAB[WT_NBUILT + 2u], h1 / (off + 1));
    check(b, !strcmp(N_WTAB[WT_NBUILT + 2u], "BELL") && wt[0].built == (int16_t)(WT_NBUILT + 2u) && h1 > 4 * off);
    p[SY_TABL] = 0;
    p[SY_DTUN] = 40;
    p[SY_IENV] = 40;
    chain_poll();
    ui.msg_t = 0;
    shot("BRYO", "synth_wt");
    fm1_in.notes = 0;
    render(NB, 0);
    p[SY_WAVE] = 3;
    chain_poll();
    check("..WAVE back to SAW: the table's chunks given back", mem_count(MEM_WT) == 0u && !wt[0].ready);
}

/* POLY's TWIN (2026-10-13): reel B beside A. Rendered on track 1's own output (track_rt last), POLY alone */
static double tw_render(int32_t *o, uint32_t nb, uint32_t keys)
{
    uint32_t b;
    double e = 0;
    fm1_in.notes = keys;
    for (b = 0; b < nb; b++) {
        uint32_t i;
        render(1, 0);
        for (i = 0; i < CTL; i++) {
            o[b * CTL + i] = track_rt[0].last[i];
            e += (double)o[b * CTL + i] * o[b * CTL + i];
        }
    }
    return sqrt(e / (nb * CTL));
}
static double tw_corr(const int32_t *a, const int32_t *b, uint32_t n)
{
    double ab = 0, aa = 0, bb = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        ab += (double)a[i] * b[i];
        aa += (double)a[i] * a[i];
        bb += (double)b[i] * b[i];
    }
    return aa > 0 && bb > 0 ? ab / sqrt(aa * bb) : 0;
}
static void test_twin(void)
{
    enum { NB = 200 };
    static int32_t a[NB * CTL], bb[NB * CTL], m[NB * CTL];
    int16_t *p;
    char b[220];
    uint32_t j, n;
    double ea, eb;
    power_on();
    for (j = 1; j < NTRK; j++)
        track[j].mute = 1;
    tp[0].src = SRC_POLY;
    p = tp[0].pol;
    p[PL_REEL] = 2;                                      /* A: BELL */
    p[PL_RLB] = 3;                                       /* B: DRON */
    p[PL_SUS] = 100;
    focus_dev(DEV_SRC, 4);
    check("POLY page 5 is TWIN RLB TUNB BLND (OFF to start; LVL still the last page's KNOB 4)",
          pdesc_at(POL_P, 16) == PL_TWIN && pdesc_at(POL_P, 19) == PL_BLND && pdesc_at(POL_P, 23) == PL_LVL &&
          p[PL_TWIN] == TW_OFF);
    ea = tw_render(a, NB, note_bit_of_white(9));
    tw_render(m, 20, 0);
    p[PL_REEL] = 3;                                      /* (B alone: DRON as A) */
    eb = tw_render(bb, NB, note_bit_of_white(9));
    tw_render(m, 20, 0);
    p[PL_REEL] = 2;
    p[PL_TWIN] = TW_MIX;
    p[PL_BLND] = 0;
    tw_render(m, NB, note_bit_of_white(9));
    snprintf(b, sizeof b, "TWIN MIX, BLND 0: reel A (correlation %.4f); ", tw_corr(m, a, NB * CTL));
    {
        double c0 = tw_corr(m, a, NB * CTL), c1;
        tw_render(m, 20, 0);
        p[PL_BLND] = 100;
        tw_render(m, NB, note_bit_of_white(9));
        c1 = tw_corr(m, bb, NB * CTL);
        snprintf(b + str_len(b), sizeof b - str_len(b), "BLND 100: reel B (%.4f)", c1);
        check(b, c0 > 0.99 && c1 > 0.99 && ea > 100 && eb > 100);
    }
    tw_render(m, 20, 0);
    p[PL_BLND] = 50;
    tw_render(m, NB, note_bit_of_white(9));
    {
        double r = 0, e = 0;
        uint32_t i;
        for (i = 0; i < NB * CTL; i++) {                 /* (the mix against half of each, alone) */
            double d = m[i] - 0.5 * a[i] - 0.5 * bb[i];
            r += d * d;
            e += (double)m[i] * m[i];
        }
        snprintf(b, sizeof b, "..BLND 50: half of each (what's left over: %.4f of it)", sqrt(r / (e + 1)));
        check(b, sqrt(r / (e + 1)) < 0.05);
    }
    tw_render(m, 4, note_bit_of_white(1) | note_bit_of_white(3) | note_bit_of_white(5));
    for (j = 0, n = 0; j < POL_NV; j++)
        n += pol[0].v[j].stage == 1u || pol[0].v[j].stage == 2u;
    check("..MIX: two voices at most (two reels each), three keys held", n == 2u);
    tw_render(m, 30, 0);
    p[PL_TUNB] = 12;
    tw_render(m, 2, note_bit_of_white(9));
    check("..TUNB +12: B's head runs at twice A's step", pol[0].v[0].inc2 > 2 * pol[0].v[0].inc - 4 &&
          pol[0].v[0].inc2 < 2 * pol[0].v[0].inc + 4);
    tw_render(m, 30, 0);
    p[PL_TUNB] = 0;
    p[PL_TWIN] = TW_ALT;                                 /* ALT: A, B, A, B */
    {
        uint32_t seq = 0, k;
        for (k = 0; k < 4u; k++) {
            uint32_t newest = 0;
            tw_render(m, 3, note_bit_of_white(9));
            for (j = 1; j < POL_NV; j++)
                if (pol[0].v[j].age - pol[0].v[newest].age < 0x80000000u)
                    newest = j;
            seq = seq << 1 | pol[0].v[newest].b;
            tw_render(m, 3, 0);
        }
        snprintf(b, sizeof b, "..ALT: four notes alternate reels (%u%u%u%u: B A B A from the first toggle)", (seq >> 3) & 1u,
                 (seq >> 2) & 1u, (seq >> 1) & 1u, seq & 1u);
        check(b, seq == 0xAu || seq == 0x5u);
    }
    p[PL_TWIN] = TW_RND;                                 /* RND, BLND 100: every note B; 0: every note A */
    p[PL_BLND] = 100;
    {
        uint32_t k, allb = 1, alla = 1;
        for (k = 0; k < 6u; k++) {
            tw_render(m, 2, note_bit_of_white(9));
            for (j = 0; j < POL_NV; j++)
                if (pol[0].v[j].stage == 1u || pol[0].v[j].stage == 2u)
                    allb &= pol[0].v[j].b;
            tw_render(m, 30, 0);
        }
        p[PL_BLND] = 0;
        for (k = 0; k < 6u; k++) {
            tw_render(m, 2, note_bit_of_white(9));
            for (j = 0; j < POL_NV; j++)
                if (pol[0].v[j].stage == 1u || pol[0].v[j].stage == 2u)
                    alla &= !pol[0].v[j].b;
            tw_render(m, 30, 0);
        }
        check("..RND: BLND 100 every note B, BLND 0 every note A", allb && alla);
    }
    p[PL_TWIN] = TW_XFD;                                 /* XFD, B the track's own empty tape (silent): a low note
                                                          * mostly A, a high one mostly B (quieter) */
    p[PL_RLB] = 0;
    p[PL_BLND] = 50;
    track[0].octave = 2;
    ea = tw_render(m, NB, note_bit_of_white(0));
    tw_render(m, 30, 0);
    track[0].octave = 5;
    eb = tw_render(m, NB, note_bit_of_white(15));
    tw_render(m, 30, 0);
    snprintf(b, sizeof b, "..XFD: the low note A's (level %.0f), the high one B's, here silent (%.0f)", ea, eb);
    check(b, ea > 4 * eb);
    track[0].octave = 3;
    p[PL_RLB] = 3;
    p[PL_TWIN] = TW_MIX;
    p[PL_BLND] = 40;
    focus_dev(DEV_SRC, 4);
    ui.msg_t = 0;
    shot("BRYO", "poly_twin");
    p[PL_TWIN] = TW_OFF;
    for (j = 1; j < NTRK; j++)
        track[j].mute = 0;
}

/* ARP's GRND (2026-10-13): each step's gate at random, GATE +- GRND points */
static void test_arp_grnd(void)
{
    char b[160];
    uint32_t k, i, n0 = 0, n1 = 0, lo = 100, hi = 0;
    int32_t g0[64], g1[64];
    for (k = 0; k < 2u; k++) {
        int32_t last = -1, *g = k ? g1 : g0;
        uint32_t *nn = k ? &n1 : &n0;
        power_on();
        tp[0].src = SRC_SYNTH;
        tp[0].arp[AR_MODE] = ARM_KEY;
        tp[0].arp[AR_RATE] = 3;                          /* 1/16 */
        tp[0].arp[AR_GATE] = 50;
        tp[0].arp[AR_GRND] = k ? 40 : 0;
        sys.playing = 1;
        fm1_in.notes = note_bit_of_white(0) | note_bit_of_white(4);
        for (i = 0; i < 4000u && *nn < 64u; i++) {
            render(1, 0);
            if (arp_rt[0].len && arp_rt[0].pos != (uint32_t)last) {
                last = (int32_t)arp_rt[0].pos;
                g[(*nn)++] = arp_rt[0].gate * 100 / arp_rt[0].len;
            }
        }
        fm1_in.notes = 0;
        sys.playing = 0;
    }
    for (i = 1; i < n1; i++) {
        lo = (uint32_t)g1[i] < lo ? (uint32_t)g1[i] : lo;
        hi = (uint32_t)g1[i] > hi ? (uint32_t)g1[i] : hi;
    }
    {
        uint32_t same = 1;
        for (i = 2; i < n0; i++)
            same &= g0[i] == g0[1];
        snprintf(b, sizeof b, "ARP GRND 40, GATE 50: each step's gate at random (%u%% .. %u%% over %u steps); GRND 0: all %d%%",
                 lo, hi, n1, g0[1]);
        check(b, same && n0 > 20u && n1 > 20u && lo >= 10u && lo < 30u && hi > 70u && hi <= 90u && g0[1] >= 48 &&
              g0[1] <= 50);
    }
}

/* TAPE's PTCH (2026-10-13): the speed in semitones on top of SPD */
static void test_tape_ptch(void)
{
    char b[200];
    int32_t i0, i12, i7, im12, ih;
    power_on();
    tp[0].dev[DEV_SRC][TK_SPD] = 100;
    i0 = tape_inc(0);
    tp[0].dev[DEV_SRC][TK_PTCH] = 12;
    i12 = tape_inc(0);
    tp[0].dev[DEV_SRC][TK_PTCH] = 7;
    i7 = tape_inc(0);
    tp[0].dev[DEV_SRC][TK_PTCH] = -12;
    im12 = tape_inc(0);
    tp[0].dev[DEV_SRC][TK_PTCH] = 0;
    tp[0].dev[DEV_SRC][TK_HALF] = 1;
    ih = tape_inc(0);
    tp[0].dev[DEV_SRC][TK_HALF] = 0;
    snprintf(b, sizeof b, "TAPE PTCH: +12 twice the speed (%d of %d), +7 a fifth (%.4f), -12 HALF's (%d, %d); page 1 STRT LEN "
             "SPD PTCH", i12, i0, (double)i7 / i0, im12, ih);
    check(b, i12 == 2 * i0 && fabs((double)i7 / i0 - 1.4983) < 0.001 && im12 == ih &&
          pdesc_at(DEV_P[DEV_SRC], 3) == TK_PTCH);
}

/* POLY's LOOP (2026-10-13): a held note loops LSTR..END */
static void test_poly_loop(void)
{
    enum { NB = 1400 };                                  /* ~1 s */
    static int32_t o[NB * CTL];
    char b[200];
    uint32_t j, k;
    int32_t jump[3];
    double late[3];
    for (k = 0; k < 3u; k++) {                           /* OFF, FWD (LXF 0, then 20), PING */
        int16_t *p;
        power_on();
        for (j = 1; j < NTRK; j++)
            track[j].mute = 1;
        tp[0].src = SRC_POLY;
        p = tp[0].pol;
        p[PL_REEL] = 3;                                  /* DRON */
        p[PL_SUS] = 100;
        p[PL_END] = 1500;                                /* (15 %: a short sound, done well inside a second) */
        p[PL_LSTR] = 500;
        p[PL_LOOP] = (int16_t)k;
        p[PL_LXF] = 20;
        tw_render(o, NB, note_bit_of_white(9));
        {
            double e = 0;
            int32_t m = 0;
            for (j = (NB - 200u) * CTL; j < NB * CTL; j++)
                e += (double)o[j] * o[j];
            for (j = 2; j < NB * CTL; j++) {
                int32_t d = abs(o[j] - 2 * o[j - 1] + o[j - 2]);
                m = d > m ? d : m;
            }
            late[k] = sqrt(e / (200 * CTL));
            jump[k] = m;
        }
        fm1_in.notes = 0;
    }
    snprintf(b, sizeof b, "POLY LOOP: OFF the note ends at END (level %.0f a second on); FWD and PING hold it (%.0f, %.0f)",
             late[0], late[1], late[2]);
    check(b, late[0] < 5 && late[1] > 300 && late[2] > 300);
    snprintf(b, sizeof b, "..the loop's seams: FWD with LXF 20 ms and PING no bigger a bend than the sound's own (%d, %d)",
             jump[1], jump[2]);
    check(b, jump[1] < 4000 && jump[2] < 4000);
    {
        int16_t *p = tp[0].pol;
        p[PL_LOOP] = LP_FWD;
        p[PL_LSTR] = 3000;
        p[PL_END] = 7000;
        p[PL_STRT] = 1000;
        focus_dev(DEV_SRC, 1);
        ui.msg_t = 0;
        shot("BRYO", "poly_loop");
        check("..POLY page 2 is LOOP LSTR LXF; ENV on POLY opens page 3", pdesc_at(POL_P, 4) == PL_LOOP &&
              pdesc_at(POL_P, 6) == PL_LXF && env_page() == 2u);
    }
    for (j = 1; j < NTRK; j++)
        track[j].mute = 0;
}

/* SYNTH's INTV, PW and TYPE (2026-10-13) */
static void test_synth_more(void)
{
    enum { NB = 300 };
    static int32_t o[NB * CTL];
    int16_t *p;
    char b[220];
    double f1, f5, h1, h2, h2b, lp1, hp1;
    power_on();
    tp[0].src = SRC_SYNTH;
    p = tp[0].syn;
    p[SY_WAVE] = 0;                                      /* SIN, MIX 50, osc 2 a fifth up (INTV 7, DTUN 0) */
    p[SY_DTUN] = 0;
    p[SY_MIX] = 50;
    p[SY_INTV] = 7;
    p[SY_CUT] = 127;
    p[SY_ENV] = 0;
    p[SY_SUS] = 100;
    fm1_in.notes = note_bit_of_white(9);
    render(NB, o);
    f1 = sqrt(goertzel(o, 100 * CTL, NB * CTL, 220.0));
    f5 = sqrt(goertzel(o, 100 * CTL, NB * CTL, 329.63));
    snprintf(b, sizeof b, "SYNTH INTV +7: the second oscillator a fifth up (E4 %.0f beside A3 %.0f)", f5, f1);
    check(b, f5 > f1 / 2 && f1 > 100);
    p[SY_INTV] = 0;
    p[SY_MIX] = 0;
    p[SY_WAVE] = 2;                                      /* SQR: PW 50 the square (no even harmonics), PW 20 a pulse */
    render(NB, o);
    h1 = sqrt(goertzel(o, 100 * CTL, NB * CTL, 220.0));
    h2 = sqrt(goertzel(o, 100 * CTL, NB * CTL, 440.0));
    p[SY_PW] = 20;
    render(NB, o);
    h2b = sqrt(goertzel(o, 100 * CTL, NB * CTL, 440.0)) / (sqrt(goertzel(o, 100 * CTL, NB * CTL, 220.0)) + 1);
    snprintf(b, sizeof b, "..PW: 50 a square (2nd %.3f of the 1st), 20 a pulse (2nd %.2f)", h2 / (h1 + 1), h2b);
    check(b, h2 / (h1 + 1) < 0.05 && h2b > 0.3);
    p[SY_PW] = 50;
    p[SY_WAVE] = 3;                                      /* SAW through TYPE HP at 1.5 kHz: the fundamental down */
    p[SY_CUT] = 80;
    render(NB, o);
    lp1 = sqrt(goertzel(o, 100 * CTL, NB * CTL, 220.0));
    p[SY_FTYP] = 2;
    render(NB, o);
    hp1 = sqrt(goertzel(o, 100 * CTL, NB * CTL, 220.0));
    snprintf(b, sizeof b, "..TYPE HP: the fundamental under the cutoff falls away (%.0f, LP %.0f); the page CUT RES ENV TYPE",
             hp1, lp1);
    check(b, hp1 < lp1 / 4 && pdesc_at(SYN_CL_P, 11) == SY_FTYP && pdesc_at(SYN_CL_P, 1) == SY_INTV &&
          pdesc_at(SYN_CL_P, 16) == SY_PW);
    p[SY_FTYP] = 0;
    fm1_in.notes = 0;
    render(NB, 0);
}

/* scenes (scene.c, 2026-10-13): the mixer's SCENES page (GLO, then EDIT): white keys 1-4 recall (held + SAVE:
 * store), KNOB 1-4 morph. A track whose source differs fades out, switches, fades in; choices switch at the
 * midpoint; the playing isn't theirs; saved in the project as their differences */
static void scn_key_down(uint32_t n)
{
    fm1_in.notes |= note_bit_of_white(n - 1u);
    host_note_edges |= note_bit_of_white(n - 1u);
    ui_input();
}
static void scn_key_up(uint32_t n) { fm1_in.notes &= ~note_bit_of_white(n - 1u); ui_input(); }
static void scn_open(void)                               /* GLO tapped, EDIT tapped: the SCENES page, latched */
{
    hold(B_GLO);
    let_go(B_GLO);
    press(B_EDIT);
}
static void test_scenes(void)
{
    uint32_t i;
    int16_t *gr;
    char b[160];
    power_on();
    pj_wipe();
    pj_new(0);
    gr = tp[0].dev[DEV_GRAIN];
    gr[GP_WET] = 0;
    tp[0].dev[DEV_RESO][RP_MODE] = RM_STRG;
    track[0].level = 100;
    sys.bpm = 120;
    fm1_ms = 9000;
    hold(B_GLO);
    let_go(B_GLO);
    check("the mixer (GLO tapped) is the levels only: its white keys still play", ui.view == VIEW_MIXER &&
          ui.chan == CHAN_LEVELS && sys.keys_live);
    press(B_EDIT);
    check("..EDIT: the SCENES page first, latched; its white keys are the scenes (they don't play)",
          ui.chan == CHAN_SCENES && ui.chan_latched && !sys.keys_live);
    scn_key_down(1);                                     /* white 1 held + SAVE: scene 1 */
    press(B_SAVE);
    scn_key_up(1);
    check("white 1 held + SAVE: scene 1 stored (no project save, no recall)", scn[0].used && scn_cur == 1u &&
          !ui.save_held && !scn[1].used && ui.chan == CHAN_SCENES);
    gr[GP_WET] = 80;                                     /* scene 2: GRAIN WET 80, RIPPLE COMB, the level 80, SYNTH on
                                                          * track 2 */
    tp[0].dev[DEV_RESO][RP_MODE] = RM_COMB;
    track[0].level = 80;
    tp[1].src = SRC_SYNTH;
    scn_key_down(2);
    press(B_SAVE);
    scn_key_up(2);
    tp[0].dev[DEV_SRC][TK_REEL] = 3;                     /* (the material isn't a scene's: it stays) */
    sys.bpm = 97;
    track[2].mute = 1;
    fm1_ms = 10000;
    scn_key_down(1);                                     /* tapped: scene 1 recalled, a glide */
    scn_key_up(1);
    fm1_ms = 10000 + SCN_GLIDE_MS / 4u;
    ui_input();
    snprintf(b, sizeof b, "white 1 tapped: scene 1 glides back (a quarter in: WET %d, the level %u; track 2's SYNTH fading "
             "out at %u)", gr[GP_WET], track[0].level, track[1].level);
    check(b, gr[GP_WET] == 60 && track[0].level == 85u && tp[0].dev[DEV_RESO][RP_MODE] == RM_COMB &&
          tp[1].src == SRC_SYNTH && track[1].level == 50u);
    fm1_ms = 10000 + SCN_GLIDE_MS * 3u / 4u;
    ui_input();
    check("..three quarters: the choice switched at the midpoint (RIPPLE STRG), track 2 back on TAPE fading in",
          gr[GP_WET] == 20 && tp[0].dev[DEV_RESO][RP_MODE] == RM_STRG && tp[1].src == SRC_TAPE && track[1].level == 50u);
    fm1_ms = 10000 + SCN_GLIDE_MS;
    ui_input();
    check("..the end: scene 1 as stored; the reel, the tempo and the mute left alone", gr[GP_WET] == 0 &&
          track[0].level == 100u && track[1].level == 100u && tp[0].dev[DEV_SRC][TK_REEL] == 3 && sys.bpm == 97 &&
          track[2].mute == 1u && scm.on == 0u);
    turn(1, 10);                                         /* KNOB 2: the morph to scene 2, 5 % a detent, nothing held */
    snprintf(b, sizeof b, "KNOB 2 turned 10: the morph halfway to scene 2 (WET %d, the level %u), no key held, the tempo "
             "left alone", gr[GP_WET], track[0].level);
    check(b, gr[GP_WET] == 40 && track[0].level == 90u && sys.bpm == 97 && scm.on == 2u && scm.to == 1u);
    ui.msg_t = 0;
    shot("BRYO", "mixer_morph");
    turn(1, -4);
    check("..back 4: 30 %, the choice back too (below the midpoint)", gr[GP_WET] == 24 &&
          tp[0].dev[DEV_RESO][RP_MODE] == RM_STRG);
    turn(0, 10);
    check("..KNOB 1 then: from there halfway back to scene 1 (WET 12)", gr[GP_WET] == 12 && scm.to == 0u);
    turn(1, 20);                                         /* (to scene 2 all the way: 2 the current one) */
    check("..KNOB 2 all the way: scene 2 as stored, the current one", gr[GP_WET] == 80 && scn_cur == 2u);
    ui.msg_t = 0;
    shot("BRYO", "mixer_scenes");
    scn_key_down(3);
    scn_key_up(3);
    check("an empty scene tapped: said, nothing moves", gr[GP_WET] == 80 && strstr(ui.msg, "EMPTY") != 0);
    turn(3, 2);
    check("..its knob: said, nothing moves", gr[GP_WET] == 80 && strstr(ui.msg, "EMPTY") != 0);
    hold(B_GLO);                                         /* (GLO: the mixer closes) */
    let_go(B_GLO);
    check("GLO again: the mixer closed, the keys play", ui.view == VIEW_PAGE && sys.keys_live);
    for (i = 0; i < 40u; i++)                            /* (the glide doesn't run off the end) */
        scn_poll();
    {   /* saved and loaded: the scenes come back, as their differences */
        scene_t s1 = scn[0], s2 = scn[1];
        uint32_t len0;
        check("a project with two scenes saves (both fit)", pj_save(0) == 0 && scn_lost == 0u);
        {
            pj_hdr_t h;
            len0 = pj_current(0, &h) >= 0 ? h.len : 0u;
        }
        scn_clear();
        gr[GP_WET] = 5;
        check("..loaded: both scenes back as stored, the others empty",
              pj_load(0) == 0 && !memcmp(&scn[0], &s1, sizeof s1) && !memcmp(&scn[1], &s2, sizeof s2) &&
              !scn[2].used && !scn[3].used && gr[GP_WET] == 80);
        snprintf(b, sizeof b, "..the project with them: %u bytes packed", len0);
        check(b, len0 > 0u && len0 < 2500u);
        check("..the PROJECTS view knows: P1 holds scenes 1 and 2 (read from the slot, not the RAM)",
              pj_sum[0].scn == 3u && pj_sum[0].used && !pj_sum[1].scn);
        host_enc[panel.enc[EN_PRESET]] = 1;              /* (the project view on P1: the strip says 2 SCN) */
        ui_input();
        while (ui.pj_sel != 0u) {
            host_enc[panel.enc[EN_PRESET]] = -1;
            ui_input();
        }
        ui.msg_t = 0;
        shot("BRYO", "projects_scenes");
        press(B_OCTDN);
    }
    {   /* four scenes each differing in every knob from the project: what doesn't fit is saved empty, said, and the
         * save itself goes through */
        uint32_t n, t, g, ok;
        int32_t rng = 99;
        for (n = 0; n < NSCN; n++) {
            scn_take(&scn[n]);
            scn[n].used = 1;
            for (t = 0; t < NTRK; t++)
                for (g = 0; g < MOD_NTGT; g++)
                    if (scn_kind(g) == 1u) {
                        const pdesc_t *d = scn_desc(g);
                        scn[n].v[t][g] = (int16_t)(d->min + (int32_t)(noise32(&rng) % (uint32_t)(d->max - d->min + 1)));
                    }
        }
        ok = pj_save(0) == 0;
        snprintf(b, sizeof b, "four scenes as far from the project as can be: the save goes through, scenes %s%s%s%s "
                 "saved empty and said", scn_lost & 1u ? "1" : "", scn_lost & 2u ? "2" : "", scn_lost & 4u ? "3" : "",
                 scn_lost & 8u ? "4" : "");
        check(b, ok && scn_lost != 0u && pj_load(0) == 0 && (uint32_t)!scn[3].used == ((scn_lost >> 3) & 1u));
    }
    {   /* a track's level moved between blocks: ramped across the next (chain.c rt->gv), not a step */
        static int32_t a1[40 * CTL], a2[40 * CTL];
        uint32_t k, j;
        int32_t r0, r1;
        for (k = 0; k < 2u; k++) {
            int32_t *o = k ? a2 : a1;
            power_on();
            sys.playing = 1;
            render(200, 0);
            for (j = 0; j < 40u; j++) {
                if (k && j == 20u)
                    track[0].level = 60;                 /* (about -20 dB) */
                render(1, 0);
                memcpy(o + j * CTL, track_rt[0].last, sizeof track_rt[0].last);
            }
        }
        r0 = a1[20 * CTL + 1] ? a2[20 * CTL + 1] * 100 / a1[20 * CTL + 1] : 100;
        r1 = a1[20 * CTL + CTL - 1] ? a2[20 * CTL + CTL - 1] * 100 / a1[20 * CTL + CTL - 1] : 0;
        snprintf(b, sizeof b, "a level moved: ramped across the block (%d %% of the sound at its start, %d %% at its end; "
                 "the same level: to the bit)", r0, r1);
        check(b, r0 > 90 && r1 < 20 && !memcmp(a1, a2, 20 * CTL * sizeof a1[0]));
        sys.playing = 0;
    }
}

/* RIPPLE's forms and KEYS (2026-10-13): TUNE retunes what the track's sound rings without a pluck; 12-BAND sorts
 * what comes in into its twelve bands; its bands three a string's chunk, shed three at a time */
static void test_ripple_forms(void)
{
    enum { NB = 690 };
    static int32_t s[NB * CTL], l[CTL], r[CTL];
    int16_t *p;
    char b[160];
    uint32_t i, k;
    int32_t rng = 77;
    double on, off, pk;
#define RF_RUN(keys0, in, buf) do {                                                                       \
        for (k = 0; k < NB; k++) {                                                                    \
            for (i = 0; i < CTL; i++)                                                                 \
                l[i] = r[i] = (in) ? ((int32_t)(noise32(&rng) >> 17) - 16384) / 3 : 0;                \
            reso_block(0, l, r, k == 2u ? (keys0) : 0u, 1, -1, CTL);                                   \
            for (i = 0; i < CTL; i++)                                                                 \
                (buf)[k * CTL + i] = l[i];                                                             \
        }                                                                                             \
    } while (0)
    power_on();
    p = tp[0].dev[DEV_RESO];
    p[RP_WET] = 100;
    p[RP_KEYS] = RK_TUNE;
    chain_poll();
    RF_RUN(1u << 9, 0, s);                               /* a key, A3, nothing coming in */
    check("KEYS TUNE: a key sets the root (A3) and plucks nothing (silence in, silence out)",
          reso[0].root16 == 57 * 16 && peak_of(s, NB * CTL) == 0);
    RF_RUN(0u, 1, s);                                    /* noise in: it rings at the key's tune */
    on = goertzel(s, 100 * CTL, NB * CTL, 220);
    off = goertzel(s, 100 * CTL, NB * CTL, 246.9);
    snprintf(b, sizeof b, "..what comes in rings at the key's root: A3 %.1f x B3", on / (off + 1));
    check(b, on > 8 * off);
    p[RP_KEYS] = RK_PLCK;
    RF_RUN(0u, 0, s);
    RF_RUN(1u << 9, 0, s);
    check("KEYS PLCK: a key plucks (a sound from nothing)", peak_of(s, NB * CTL) > 2000);
    p[RP_MODE] = RM_BANK;                                /* 12-BAND, on the major chord over A3 */
    p[RP_SCAL] = 1;
    p[RP_DEC] = 80;
    p[RP_TONE] = 100;
    RF_RUN(0u, 0, s);
    RF_RUN(0u, 1, s);
    {
        static const double BAND[4] = {220, 277.2, 329.6, 880}, GAP[4] = {246.9, 311.1, 370, 783.99};
        uint32_t ok = 1;
        for (k = 0; k < 4u; k++)
            ok &= goertzel(s, 100 * CTL, NB * CTL, BAND[k]) > 4 * goertzel(s, 100 * CTL, NB * CTL, GAP[k]);
        pk = 0;
        for (k = 0; k < RS_NB; k++)
            pk += reso[0].lvl[k] > 0;
        snprintf(b, sizeof b, "12-BAND: noise in sorted into the chord (A C# E, A5: the 9th band), %.0f of 12 bands lit", pk);
        check(b, ok && pk == 12);
    }
    reso_cap = 2u;                                       /* shed to two chunks: six bands */
    RF_RUN(0u, 1, s);
    for (k = 6, i = 0; k < RS_NB; k++)
        i |= reso[0].lvl[k];
    check("..shed to two strings' worth: six bands, the top six quiet", i == 0 && reso[0].lvl[5] > 0);
    reso_cap = RS_N;
    p[RP_SCAL] = 12;                                     /* OCT over A3: the bands climb past C9, and stop there */
    RF_RUN(0u, 1, s);
    for (k = 0, i = 0; k < RS_NB; k++)
        i += reso[0].lvl[k] > 0;
    check("..OCT over A3: six bands to A8, none past C9 (half rate's ceiling)", i == 6u);
    p[RP_MODE] = RM_STRG;
    p[RP_SCAL] = 0;
#undef RF_RUN
}

static void test_reso(void)
{
    enum { NB = 690 };                                   /* 0.5 s */
    static int32_t s[4 * NB * CTL];
    int16_t *p;
    char b[120];
    double hz, r1, r2;
    uint32_t t, k;
    power_on();
    p = tp[0].dev[DEV_RESO];
    for (t = 1; t < NTRK; t++)
        track[t].mute = 1;
    focus_dev(DEV_RESO, 0);
    ui_input();
    check("the RIPPLE page up: the keys go to it", sys.keys_reso == 1 && sys.keys_grain == 0);
    chain_poll();
    check("RIPPLE at WET 0 (the default): no strings, no memory taken", !reso[0].nch && !mem_count(MEM_RESO));
    fm1_in.notes = note_bit_of_white(9);
    render(NB, s);
    fm1_in.notes = 0;
    check("..a key with WET 0 (stopped): silence", peak_of(s, NB * CTL) == 0);
    p[RP_WET] = 100;
    chain_poll();
    check("WET up: four strings, four chunks of the shared memory", reso[0].nch == RS_N && mem_count(MEM_RESO) == RS_N);
    render(4, 0);
    fm1_in.notes = note_bit_of_white(9);                 /* white key 10 at OCT 3: A3 */
    render(2, 0);
    fm1_in.notes = 0;
    render(NB, s);
    hz = buf_pitch(s, 200 * CTL, NB * CTL, 100, 600);
    snprintf(b, sizeof b, "a key on a TAPE track plucks the strings at its note: A3, %.1f Hz (220), the root A3", hz);
    check(b, fabs(hz - 220) < 2.2 && reso[0].root16 == 57 * 16 && peak_of(s, NB * CTL) > 2000);
    r1 = buf_rms(s, 0, NB * CTL / 4);
    render(NB, s);
    r2 = buf_rms(s, 0, NB * CTL);
    p[RP_DEC] = 90;
    fm1_in.notes = note_bit_of_white(9);
    render(2, 0);
    fm1_in.notes = 0;
    render(NB, s);
    {
        double q1 = buf_rms(s, 0, NB * CTL / 4), q2;
        render(NB, s);
        q2 = buf_rms(s, 0, NB * CTL);
        snprintf(b, sizeof b, "DEC: rings longer as it rises (after 0.5 s: DEC 60 at %.0f%% of its start, DEC 90 at %.0f%%)",
                 100 * r2 / (r1 + 1), 100 * q2 / (q1 + 1));
        check(b, q2 / (q1 + 1) > 2 * r2 / (r1 + 1) && q2 > 0);
    }
    p[RP_DEC] = 60;
    p[RP_SCAL] = 1;                                      /* MAJ: root, third, fifth, octave */
    {
        int32_t r0 = 45 * 16;
        double q = (double)rs_period_q8(r0 + RS_PART16[1][1]) / rs_period_q8(r0), f = (double)rs_period_q8(r0 + RS_PART16[1][2]) / rs_period_q8(r0);
        snprintf(b, sizeof b, "SCAL MAJ: strings on the root, its third (%.4f of its period) and fifth (%.4f)", q, f);
        check(b, fabs(q - 0.7937) < 0.002 && fabs(f - 0.6674) < 0.002);
    }
    {
        uint32_t sc, k, ok = 1, rows = 1;
        static const int16_t FOUR[13][4] = {{0, 192, 304, 384}, {0, 64, 112, 192}, {0, 48, 112, 192}, {0, 32, 64, 112},
                                            {0, 32, 112, 192}, {0, 80, 112, 192}, {0, 64, 112, 160}, {0, 64, 112, 176},
                                            {0, 48, 112, 160}, {0, 48, 96, 144}, {0, 64, 128, 192}, {0, 112, 192, 304},
                                            {0, 192, 384, 576}};
        for (sc = 0; sc <= (uint32_t)RS_SCALES; sc++) {
            ok &= RS_PART_N[sc] >= RS_N && RS_PART_N[sc] <= RS_PARTS;
            for (k = 0; k < RS_N; k++)
                ok &= RS_PART16[sc][k] == FOUR[sc][k];
            for (k = 1; k < RS_PART_N[sc]; k++)         /* each row climbs */
                rows &= RS_PART16[sc][k] > RS_PART16[sc][k - 1];
        }
        check("SCAL: 13 chord sets (HARM MAJ MIN PEN SUS2 SUS4 DOM7 MAJ7 MIN7 DIM AUG 5TH OCT), the strings on each "
              "set's first four notes, the picture's partials climbing", ok && rows && RS_SCALES == 12);
        p[RP_SCAL] = 12;                                 /* OCT: four octaves of the root */
        ok = 1;
        for (k = 1; k < RS_N; k++) {
            double r = (double)rs_period_q8(45 * 16 + RS_PART16[12][k - 1]) / rs_period_q8(45 * 16 + RS_PART16[12][k]);
            ok &= r > 1.99 && r < 2.01;
        }
        check("..OCT: each string half the period of the one below", ok);
    }
    p[RP_SCAL] = 0;
    /* RIPPLE 4 (2026-10-13): MODE (STRG COMB BANK), DRV, RIPL */
    {
        double e1, e2, e3, c1, c2, c3, b1, b2, early, late, early2, late2, pk0, pk1;
        uint32_t i;
        power_on();
        p = tp[0].dev[DEV_RESO];
        for (t = 1; t < NTRK; t++)
            track[t].mute = 1;
        focus_dev(DEV_RESO, 0);
        ui_input();
        p[RP_WET] = 100;
        p[RP_DEC] = 60;
        p[RP_TONE] = 60;
        p[RP_DRV] = 0;
        chain_poll();
        render(4, 0);
#define RS_PLUCK(buf) do { fm1_in.notes = note_bit_of_white(9); render(1, 0); fm1_in.notes = 0; render(NB - 1, buf); } while (0)
        RS_PLUCK(s);                                     /* STRG: A3 and its harmonics, even ones too */
        e1 = goertzel(s, 100 * CTL, NB * CTL, 220);
        e2 = goertzel(s, 100 * CTL, NB * CTL, 440);
        e3 = goertzel(s, 100 * CTL, NB * CTL, 660);
        pk0 = peak_of(s, NB * CTL);
        p[RP_MODE] = 1;
        render(2, 0);
        check("MODE COMB: the switch clears the lines (what rang as strings isn't read as combs)",
              rs_line(&reso[0], 0)[0] == 0 && rs_line(&reso[0], 0)[RS_LEN / 2u] == 0 && reso[0].lp[0] == 0);
        RS_PLUCK(s);
        c1 = goertzel(s, 100 * CTL, NB * CTL, 220);
        c2 = goertzel(s, 100 * CTL, NB * CTL, 440);
        c3 = goertzel(s, 100 * CTL, NB * CTL, 660);
        snprintf(b, sizeof b, "COMB: the pitch stays A3 (half the line, fed back inverted), the even harmonics gone: 2nd/1st "
                 "%.4f (STRG %.4f), 3rd above 2nd", c2 / (c1 + 1), e2 / (e1 + 1));
        check(b, c1 > 0 && c1 > c2 * 10 && c2 / (c1 + 1) < e2 / (e1 + 1) / 10 && c3 > c2 && e2 > e1 / 1000);
        p[RP_MODE] = 2;                                  /* BANK: a band a partial, DEC its ring */
        p[RP_DEC] = 30;
        render(2, 0);
        RS_PLUCK(s);
        b1 = goertzel(s, 20 * CTL, NB * CTL, 220);
        b2 = goertzel(s, 20 * CTL, NB * CTL, 440);
        early = buf_rms(s, 20 * CTL, 100 * CTL);
        late = buf_rms(s, (NB - 100) * CTL, (NB - 1) * CTL);
        p[RP_DEC] = 95;
        render(2, 0);
        RS_PLUCK(s);
        early2 = buf_rms(s, 20 * CTL, 100 * CTL);
        late2 = buf_rms(s, (NB - 100) * CTL, (NB - 1) * CTL);
        snprintf(b, sizeof b, "BANK: a pluck rings the band at A3 (220 over 440: %.1f x) and dies away (DEC 30: %.0f%% left after "
                 "0.4 s; DEC 95: %.0f%%)", b1 / (b2 + 1), 100 * late / (early + 1), 100 * late2 / (early2 + 1));
        check(b, b1 > b2 * 4 && early > 50 && late < early / 4 && late2 / (early2 + 1) > 4 * late / (early + 1));
        p[RP_MODE] = 0;
        p[RP_DEC] = 60;
        render(2, 0);
        {                                                /* DRV's shaper: a sine gains its odd harmonics, nothing even */
            static int32_t w[4096];
            int32_t gq = 256 + ((100 * 1311) >> 8), ig = (int32_t)((1u << 22) / (uint32_t)gq);
            double h1, h2, h3;
            for (i = 0; i < 4096u; i++)                  /* (41 cycles in the window: no leak between the bins) */
                w[i] = rs_shape((int32_t)(20000 * sin(2 * M_PI * 41 * i / 4096.0)), gq, ig);
            h1 = goertzel(w, 0, 4096, 41 * 44100.0 / 4096);
            h2 = goertzel(w, 0, 4096, 82 * 44100.0 / 4096);
            h3 = goertzel(w, 0, 4096, 123 * 44100.0 / 4096);
            snprintf(b, sizeof b, "DRV 100's shaper on a sine: the third harmonic at %.1f%% of the first, the second none, small signals as "
                     "they are", 100 * sqrt(h3 / h1));
            check(b, h3 > h1 / 1000 && h2 < h3 / 20 && rs_shape(100, gq, ig) >= 97 && rs_shape(100, gq, ig) <= 100 &&
                  abs(rs_shape(-100, gq, ig) + rs_shape(100, gq, ig)) <= 2);
        }
        p[RP_DRV] = 100;
        RS_PLUCK(s);
        pk1 = peak_of(s, NB * CTL);
        early = buf_rms(s, (NB - 300) * CTL, (NB - 200) * CTL);
        late = buf_rms(s, (NB - 100) * CTL, (NB - 1) * CTL);
        p[RP_DRV] = 0;
        RS_PLUCK(s);
        early2 = buf_rms(s, (NB - 300) * CTL, (NB - 200) * CTL);
        late2 = buf_rms(s, (NB - 100) * CTL, (NB - 1) * CTL);
        snprintf(b, sizeof b, "DRV 100 on the strings: the pluck no louder (peak %.0f against %.0f), the quiet tail dying at the "
                 "same rate (%.0f%% a 0.15 s against %.0f%%: a small signal passes as it is)", pk1, pk0,
                 100 * late / (early + 1), 100 * late2 / (early2 + 1));
        check(b, pk1 <= pk0 * 1.1 && late > 0 && late / (early + 1) > 0.8 * late2 / (early2 + 1) &&
              late / (early + 1) < 1.25 * late2 / (early2 + 1));
        p[RP_RIPL] = 0;
        render(8, 0);
        {
            uint32_t d0 = reso[0].dq[0], d1;
            render(8, 0);
            check("RIPL 0: a string's delay stands still", reso[0].dq[0] == d0 && reso[0].rc[0] == 0);
            p[RP_RIPL] = 100;
            render(8, 0);
            d0 = reso[0].dq[0];
            render(8, 0);
            d1 = reso[0].dq[0];
            check("RIPL 100: it drifts, block by block, each string a quarter turn behind the last",
                  d1 != d0 && reso[0].rc[0] != reso[0].rc[1] && reso[0].rc[1] != reso[0].rc[2]);
            p[RP_RIPL] = 0;
        }
#undef RS_PLUCK
        power_on();
        p = tp[0].dev[DEV_RESO];
        for (t = 1; t < NTRK; t++)
            track[t].mute = 1;
        focus_dev(DEV_RESO, 0);
        ui_input();
        p[RP_WET] = 100;                                 /* (as the flow above left it) */
        chain_poll();
        render(2, 0);
        fm1_in.notes = note_bit_of_white(9);
        render(2, 0);
        fm1_in.notes = 0;
        p[RP_DEC] = 60;
    }
    p[RP_PTCH] = 50;
    render(2, 0);
    check("PTCH turned after a key: it's the root again", reso[0].root16 == 0 && reso_root16(0) == 50 * 16);
    p[RP_PTCH] = 45;
    render(NB * 2, 0);

    sys.playing = 1;                                     /* the track's sound through the strings */
    p[RP_DEC] = 100;
    p[RP_TONE] = 100;
    for (k = 0; k < 4u; k++) {
        render(NB, s);
        if (peak_of(s, NB * CTL) > 32767)
            break;
    }
    check("the reel ringing the strings at DEC 100, TONE 100 for 2 s: loud, and it never runs away (under full scale)",
          k == 4u && peak_of(s, NB * CTL) > 2500 && peak_of(s, NB * CTL) <= 32767);   /* (2500: DRV 25, the default
                                                                                       * since 2026-10-13, rounds the
                                                                                       * loud ring off by a sixth) */
    p[RP_DEC] = 60;
    p[RP_TONE] = 60;
    p[RP_CUT] = 40;                                      /* the filter in front: dark */
    p[RP_SLOP] = 0;
    render(NB, 0);
    check("the filter in front (CUT 40) changes what rings, the strings still sound", peak_of(track_rt[0].last, CTL) > 0 ||
          reso[0].nch == RS_N);
    p[RP_CUT] = 127;
    sys.playing = 0;
    render(NB, 0);

    tp[0].src = SRC_SYNTH;                               /* a SYNTH track: the keys play it, the strings follow */
    fm1_in.notes = note_bit_of_white(4);
    render(20, 0);
    check("on a SYNTH track the keys still play the synth, and the strings take its note as their root",
          syn_sounding(0) == 1 && reso[0].root16 == (12 * 4 + 4) * 16);
    fm1_in.notes = 0;
    render(NB, 0);
    tp[0].src = SRC_TAPE;

    {
        uint32_t c0 = reso_cap;
        grain_cap = 4;                                   /* (the grains already down to half) */
        chain_shed();
        check("shedding, the grains at half already: a string goes (4 -> 3), and a second later it's back",
              reso_cap == c0 - 1u);
        render(1400, 0);
        check("..back", reso_cap == RS_N);
        grain_cap = GR_CAP;
    }
    chain_tracks(1);
    p = tp[1].dev[DEV_RESO];
    p[RP_WET] = 100;                                     /* (track 2's RIPPLE on, track 2 parked) */
    chain_poll();
    check("a parked track's RIPPLE holds no strings", !reso[1].nch && mem_count(MEM_RESO + 1) == 0);
    chain_tracks(4);
    tp[0].dev[DEV_RESO][RP_WET] = 0;
    chain_poll();
    check("WET 0 again: the strings' chunks back to the pool", !reso[0].nch && !mem_count(MEM_RESO));
    for (t = 0; t < NTRK; t++)
        track[t].mute = 0;
    press(B_HOME);
}

/* a device's block function over a whole buffer, CTL at a time (track 0) */
static void dev_run(void (*f)(uint32_t, int32_t *, int32_t *, uint32_t), int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t b;
    for (b = 0; b + CTL <= n; b += CTL)
        f(0, l + b, r + b, CTL);
}

/* 3 s of silence through tracks 1 and 2's SPACE at levels g0 and g1 (the rooms' tails out, the sends settled at
 * those levels), the buffers left at 0 */
static void sp_quiet2(int32_t *l, int32_t *r, int32_t *l2, int32_t *r2, int32_t g0, int32_t g1)
{
    uint32_t i, n = 3u * 44100u / CTL * CTL;
    memset(l, 0, n / 3u * sizeof l[0]);
    memset(r, 0, n / 3u * sizeof r[0]);
    memset(l2, 0, n / 3u * sizeof l2[0]);
    memset(r2, 0, n / 3u * sizeof r2[0]);
    for (i = 0; i < n; i += CTL) {
        int32_t z[CTL] = {0}, zr[CTL] = {0}, z2[CTL] = {0}, zr2[CTL] = {0};
        space_block(0, z, zr, CTL, g0);
        space_block(1, z2, zr2, CTL, g1);
        space_rooms(z, zr, CTL);
    }
}

/* the power of x[from..to) at hz (Goertzel), per sample */
static double goertzel(const int32_t *x, uint32_t from, uint32_t to, double hz)
{
    double c = 2 * cos(2 * M_PI * hz / 44100.0), s0, s1 = 0, s2 = 0;
    uint32_t i;
    for (i = from; i < to; i++) {
        s0 = x[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / ((double)(to - from) * (to - from));
}

/* track 1's SPACE at full level, and the rooms after it (as chain_block runs them: the room's tail added in) */
static void sp_run(uint32_t t, int32_t *l, int32_t *r, uint32_t n)
{
    space_block(t, l, r, n, 4096);
    space_rooms(l, r, n);
}

static void sine_fill(int32_t *l, int32_t *r, uint32_t n, double hz, double amp)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        l[i] = r[i] = (int32_t)lrint(amp * sin(2 * M_PI * hz * i / 44100.0));
}

static uint32_t peak_at(const int32_t *x, uint32_t from, uint32_t to)
{
    uint32_t i, at = from;
    for (i = from; i < to; i++)
        if (abs(x[i]) > abs(x[at]))
            at = i;
    return at;
}

static double rms_of(const int32_t *x, uint32_t from, uint32_t to)
{
    double a = 0;
    uint32_t i;
    for (i = from; i < to; i++)
        a += (double)x[i] * x[i];
    return to > from ? sqrt(a / (to - from)) : 0;
}

/* COLOR (color.c): each knob does what its picture says, and the defaults leave the sound alone */
static void test_color(void)
{
    enum { N = 8192 };
    static int32_t l[N], r[N], l0[N];
    int16_t *p;
    uint32_t i, ok, runs;
    double a, b2, c;
    char b[140];
    power_on();
    p = tp[0].dev[DEV_COLOR];
    sine_fill(l, r, N, 441, 20000);
    memcpy(l0, l, sizeof l0);
    dev_run(color_block, l, r, N);
    check("COLOR at its defaults: the sound untouched, sample for sample", !memcmp(l, l0, sizeof l0));
    p[CP_LVL] = -6;
    color[0].lvl = 0;
    sine_fill(l, r, N, 441, 20000);
    dev_run(color_block, l, r, N);
    for (i = CTL, ok = 1; i < N; i++)
        ok &= abs(l[i] - l0[i] / 2) <= 1;
    check("LVL -6 dB alone: exactly half", ok);
    p[CP_LVL] = 0;
    p[CP_DRIV] = 100;
    sine_fill(l, r, N, 441, 16000);
    dev_run(color_block, l, r, N);
    a = peak_of(l + CTL, N - CTL) / rms_of(l, CTL, N);
    snprintf(b, sizeof b, "DRV 100: the sine clipped near square (crest %.2f, a sine's 1.41), under full scale", a);
    check(b, a < 1.15 && peak_of(l, N) <= 32767);
    p[CP_DRIV] = 0;
    p[CP_CRSH] = 100;                                    /* CMOD BIT: 2 bits */
    sine_fill(l, r, N, 441, 30000);
    dev_run(color_block, l, r, N);
    for (i = 0, ok = 1; i < N; i++)
        ok &= l[i] % 16384 == 0;
    check("CRSH 100, CMOD BIT: 2 bits left (every sample a multiple of 16384)", ok && peak_of(l, N) == 16384);
    p[CP_CMOD] = CMOD_RATE;
    sine_fill(l, r, N, 441, 20000);
    dev_run(color_block, l, r, N);
    for (i = 1, runs = 0, ok = 0; i < N; i++) {
        runs += l[i] != l[i - 1];
        ok |= l[i] % 4 != 0;
    }
    snprintf(b, sizeof b, "CRSH 100, CMOD RATE: each value held 12 samples (%u changes in %u samples), bits kept", runs, N);
    check(b, runs <= N / 12 + 1 && runs > N / 16 && ok);
    p[CP_CMOD] = CMOD_BOTH;
    sine_fill(l, r, N, 441, 30000);
    dev_run(color_block, l, r, N);
    for (i = CTL, runs = 0, ok = 1; i < N; i++) {
        runs += l[i] != l[i - 1];
        ok &= l[i] % 16384 == 0;
    }
    check("CMOD BOTH: held and 2 bits", ok && runs <= N / 12 + 1);
    p[CP_CRSH] = 0;
    p[CP_CMOD] = CMOD_BIT;

    p[CP_NOIS] = 100;                                    /* NDEC 40: the table's */
    memset(l, 0, sizeof l);
    memset(r, 0, sizeof r);
    dev_run(color_block, l, r, N);
    check("NOIS 100 on silence: silence (the noise rides the sound)", peak_of(l, N) == 0);
    {
        uint32_t ndec = TIME_MS_X10[40] * 441u / 100u;   /* NDEC 40, samples */
        static int32_t L[44100], R[44100];
        memset(L, 0, sizeof L);
        memset(R, 0, sizeof R);
        for (i = 0; i < 441; i++)                        /* a 10 ms hit */
            L[i] = R[i] = 20000;
        dev_run(color_block, L, R, 44100);
        a = rms_of(L, 441, 441 + ndec / 8);
        b2 = rms_of(L, 441 + ndec * 2, 441 + ndec * 3);
        snprintf(b, sizeof b, "NOIS after a hit: noise right after it (rms %.0f), gone by 2 x NDEC (%.1f), %u ms",
                 a, b2, ndec * 10 / 441);
        check(b, a > 1000 && b2 < a / 300);
        p[CP_NTON] = -100;
        memset(L, 0, sizeof L);
        memset(R, 0, sizeof R);
        for (i = 0; i < 44100; i++)
            L[i] = R[i] = 20000;
        color[0].nlp = 0;
        dev_run(color_block, L, R, 44100);
        for (i = 1, a = c = 0; i < 44100; i++) {
            a += (double)(L[i] - 20000) * (L[i] - 20000);
            c += (double)(L[i] - L[i - 1]) * (L[i] - L[i - 1]);
        }
        a = sqrt(c / a);
        p[CP_NTON] = 100;
        for (i = 0; i < 44100; i++)
            L[i] = R[i] = 20000;
        dev_run(color_block, L, R, 44100);
        for (i = 1, b2 = c = 0; i < 44100; i++) {
            b2 += (double)(L[i] - 20000) * (L[i] - 20000);
            c += (double)(L[i] - L[i - 1]) * (L[i] - L[i - 1]);
        }
        b2 = sqrt(c / b2);
        snprintf(b, sizeof b, "NTON: -100 dark, +100 bright (the noise's steps against its level: %.2f, %.2f)", a, b2);
        check(b, a * 3 < b2);
    }
    p[CP_NOIS] = 0;
    p[CP_NTON] = 0;

    p[CP_TILT] = 100;
    sine_fill(l, r, N, 100, 10000);
    dev_run(color_block, l, r, N);
    a = rms_of(l, N / 2, N) / (10000 / M_SQRT2);
    sine_fill(l, r, N, 8000, 10000);
    dev_run(color_block, l, r, N);
    b2 = rms_of(l, N / 2, N) / (10000 / M_SQRT2);
    snprintf(b, sizeof b, "TILT +100: 100 Hz down (x%.2f), 8 kHz up (x%.2f)", a, b2);
    check(b, a < 0.6 && b2 > 1.7);
    p[CP_TILT] = -100;
    sine_fill(l, r, N, 100, 10000);
    dev_run(color_block, l, r, N);
    a = rms_of(l, N / 2, N) / (10000 / M_SQRT2);
    sine_fill(l, r, N, 8000, 10000);
    dev_run(color_block, l, r, N);
    b2 = rms_of(l, N / 2, N) / (10000 / M_SQRT2);
    snprintf(b, sizeof b, "TILT -100: the other way (x%.2f, x%.2f)", a, b2);
    check(b, a > 1.7 && b2 < 0.6);
    p[CP_TILT] = 0;

    p[CP_DRIV] = 100;
    p[CP_WET] = 0;
    dev_run(color_block, l, r, N);                       /* (WET ramps over a block) */
    sine_fill(l, r, N, 441, 20000);
    dev_run(color_block, l, r, N);
    check("WET 0: the dry sound, however much DRV", !memcmp(l, l0, sizeof l0));
    p[CP_WET] = 100;

    for (i = 0; i < 9; i++)                              /* every knob at its corner, a sound past full scale */
        p[i] = DEV_P[DEV_COLOR][i].max;
    for (i = 0; i < N; i++)
        l[i] = r[i] = (i / 50) & 1 ? 131071 : -131071;
    dev_run(color_block, l, r, N);
    check("every knob at its top, a square past full scale: bounded", peak_of(l, N) <= 2 * 131071);
    for (i = 0; i < 9; i++)
        p[i] = DEV_P[DEV_COLOR][i].min;
    dev_run(color_block, l, r, N);
    check("..and every knob at its bottom", peak_of(l, N) <= 131071);
    for (i = 0; i < 9; i++)
        p[i] = DEV_P[DEV_COLOR][i].def;
}

/* SPACE (space.c): the echoes where TIME says and falling by FDBK, the room's tail as long as DEC, the memory
 * taken only while it's used, 8-bit when it's short */
/* CHRONOS's character (2026-10-13): AGE, DIFF, DUCK on the delay, TYPE and MOD on the rooms. Each run from power-on
 * with only the knob under test changed; 0 is covered by the audio fingerprints (bit for bit as before) */
enum { CHN = 44100, CH_AT = 1000 };
static int32_t chl[CHN], chr_[CHN];
static void ch_delay(int age, int dif, int duck, int input)   /* track 1's delay, TIME 100 FDBK 70; input 0 a hit, 1 a
                                                               * 1 kHz tone for 0.5 s then silence */
{
    int16_t *p;
    uint32_t i;
    power_on();
    p = tp[0].dev[DEV_SPACE];
    p[SP_DLY] = 100;
    p[SP_FDBK] = 70;
    p[SP_TIME] = 100;
    p[SP_SPRD] = 0;
    p[SP_AGE] = (int16_t)age;
    p[SP_DIFF] = (int16_t)dif;
    p[SP_DUCK] = (int16_t)duck;
    chain_poll();
    memset(chl, 0, sizeof chl);
    if (input)
        for (i = 0; i < CHN / 2; i++)
            chl[i] = (int32_t)lrint(12000 * sin(2 * M_PI * 1000 * i / 44100.0));
    else
        chl[CH_AT] = chl[CH_AT + 1] = 20000;            /* (a few blocks in: the first block ramps its gains from 0) */
    memcpy(chr_, chl, sizeof chl);
    dev_run(sp_run, chl, chr_, CHN);
}
static double ch_bright(uint32_t a, uint32_t b)             /* sample-to-sample movement per level over [a, b) */
{
    double mv = 0, lv = 0;
    uint32_t i;
    for (i = a + 1u; i < b; i++) {
        mv += fabs((double)chl[i] - chl[i - 1]);
        lv += fabs((double)chl[i]);
    }
    return lv > 0 ? mv / lv : 0;
}
static double ch_energy(uint32_t a, uint32_t b)
{
    double e = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        e += (double)chl[i] * chl[i];
    return e;
}
static uint32_t ch_spread(uint32_t a, uint32_t b)           /* samples over a tenth of the window's peak */
{
    int32_t pk = 0;
    uint32_t i, n = 0;
    for (i = a; i < b; i++)
        pk = abs(chl[i]) > pk ? abs(chl[i]) : pk;
    for (i = a; i < b; i++)
        n += abs(chl[i]) * 10 > pk;
    return n;
}
static int32_t ch_jump(uint32_t a, uint32_t b)              /* the largest second difference over [a, b) */
{
    int32_t m = 0;
    uint32_t i;
    for (i = a + 2u; i < b; i++) {
        int32_t d = chl[i] - 2 * chl[i - 1] + chl[i - 2];
        m = abs(d) > m ? abs(d) : m;
    }
    return m;
}
static void ch_room(int type, int mod)                       /* track 1 into room A (VERB 100, DLY 0): a hit */
{
    power_on();
    tp[0].dev[DEV_SPACE][SP_VERB] = 100;
    rmp[0][RM_TYPE] = (int16_t)type;
    rmp[0][RM_MOD] = (int16_t)mod;
    rmp[0][RM_DEC] = 60;
    chain_poll();
    memset(chl, 0, sizeof chl);
    chl[CH_AT] = chl[CH_AT + 1] = 20000;
    memcpy(chr_, chl, sizeof chl);
    dev_run(sp_run, chl, chr_, CHN);
}
/* a knob turned at once while CHRONOS sounds (a 300 Hz tone into the delay and room A for 0.6 s): the largest bend in
 * the effects alone (the tone taken out) in the 100 ms after the turn, against the same 100 ms unturned. which: 0 AGE,
 * 1 DIFF, 2 MOD, 3 TYPE, 4 SIZE, 5 QUAL, 6 DLY; from, to. (2026-10-13, from playing it: "a little noise glitch" when
 * turning them; before the fix SIZE bent 2,555, TYPE 1,946, QUAL 1,783, DLY 282, AGE 95, MOD 108, the tone's own
 * effects 40-70) */
static int32_t ch_turn(int which, int from, int to, int turn)
{
    int16_t *p;
    uint32_t i, at = (CHN / 2) & ~(CTL - 1u);            /* (on a block's start, where the loop turns it) */
    power_on();
    p = tp[0].dev[DEV_SPACE];
    p[SP_DLY] = 80;
    p[SP_FDBK] = 60;
    p[SP_TIME] = 120;
    p[SP_VERB] = 80;
    rmp[0][RM_DEC] = 70;
    if (which == 0) p[SP_AGE] = (int16_t)from;
    if (which == 1) p[SP_DIFF] = (int16_t)from;
    if (which == 2) rmp[0][RM_MOD] = (int16_t)from;
    if (which == 3) rmp[0][RM_TYPE] = (int16_t)from;
    if (which == 4) rmp[0][RM_SIZE] = (int16_t)from;
    if (which == 5) rmp[0][RM_QUAL] = (int16_t)from;
    if (which == 6) p[SP_DLY] = (int16_t)from;
    chain_poll();
    for (i = 0; i < CHN; i++)
        chl[i] = i < (uint32_t)(CHN * 6 / 10) ? (int32_t)lrint(8000 * sin(2 * M_PI * 300 * i / 44100.0)) : 0;
    memcpy(chr_, chl, sizeof chl);
    for (i = 0; i + CTL <= CHN; i += CTL) {
        if (i == at && turn) {
            if (which == 0) p[SP_AGE] = (int16_t)to;
            if (which == 1) p[SP_DIFF] = (int16_t)to;
            if (which == 2) rmp[0][RM_MOD] = (int16_t)to;
            if (which == 3) rmp[0][RM_TYPE] = (int16_t)to;
            if (which == 4) rmp[0][RM_SIZE] = (int16_t)to;
            if (which == 5) rmp[0][RM_QUAL] = (int16_t)to;
            if (which == 6) p[SP_DLY] = (int16_t)to;
        }
        sp_run(0, chl + i, chr_ + i, CTL);
    }
    for (i = 0; i < (uint32_t)(CHN * 6 / 10); i++)       /* (the effects alone: the dry tone taken out) */
        chl[i] -= (int32_t)lrint(8000 * sin(2 * M_PI * 300 * i / 44100.0));
    return ch_jump(at, at + 4410);
}
static void test_chronos_turns(void)
{
    static const struct { const char *what; int which, from, to; } T[] = {
        {"AGE 40 -> 100", 0, 40, 100}, {"AGE 100 -> 40", 0, 100, 40}, {"DIFF 0 -> 100", 1, 0, 100},
        {"DIFF 100 -> 0", 1, 100, 0}, {"MOD 0 -> 100", 2, 0, 100}, {"TYPE ROOM -> HALL", 3, 0, 2},
        {"TYPE PLAT -> SPRG", 3, 1, 3}, {"room SIZE 30 -> 90", 4, 30, 90}, {"room QUAL HI -> LO", 5, 0, 1},
        {"DLY 80 -> 10", 6, 80, 10},
    };
    char b[200];
    uint32_t k;
    for (k = 0; k < sizeof T / sizeof T[0]; k++) {
        int32_t still = ch_turn(T[k].which, T[k].from, T[k].to, 0), turned;
        int32_t still2 = ch_turn(T[k].which, T[k].to, T[k].to, 0);
        turned = ch_turn(T[k].which, T[k].from, T[k].to, 1);
        snprintf(b, sizeof b, "CHRONOS %s turned at once while it sounds: no glitch (largest bend %d after, %d and %d "
                 "left alone at either end)", T[k].what, turned, still, still2);
        check(b, turned <= 2 * (still > still2 ? still : still2) + 64);
    }
    power_on();
}

static void test_chronos_character(void)
{
    char b[220];
    double b0, b1, e0, e1, f0, f1;
    uint32_t s0, s1, k;
    int32_t j0, j1, wmax = 0;
    ch_delay(0, 0, 0, 0);
    b0 = ch_bright(CH_AT + 3 * 4410 - 500, CHN);   /* (every repeat from the third: AGE's wow moves them) */      /* (the fourth repeat, 400 ms on) */
    ch_delay(100, 0, 0, 0);
    b1 = ch_bright(CH_AT + 3 * 4410 - 500, CHN);
    snprintf(b, sizeof b, "CHRONOS AGE 100: the repeats from the third on darker (%.3f a sample per level, %.3f at AGE 0)", b1, b0);
    check(b, b1 < 0.8 * b0 && b1 > 0);
    for (k = 0; k < 4200u; k++) {                        /* (the read point's wavering: 3 s of a silent delay,
                                                         * the wow and its drift through their cycles; a tone
                                                         * in, so it doesn't rest) */
        int32_t z[CTL], z2[CTL], i;
        for (i = 0; i < (int32_t)CTL; i++)
            z[i] = z2[i] = (int32_t)lrint(4000 * sin(2 * M_PI * 1000 * (k * CTL + (uint32_t)i) / 44100.0));
        sp_run(0, z, z2, CTL);
        wmax = abs(space[0].wob) > wmax ? abs(space[0].wob) : wmax;
    }
    snprintf(b, sizeof b, "..and its time wavers (wow, drift and flutter: the read point off by up to %.1f samples, "
             "125 at most; 47 until 2026-10-13)", wmax / 256.0);
    check(b, wmax > 80 * 256 && wmax <= 125 * 256);
    ch_delay(0, 0, 0, 0);
    s0 = ch_spread(CH_AT + 2 * 4410 - 300, CH_AT + 2 * 4410 + 1500);
    ch_delay(0, 100, 0, 0);
    s1 = ch_spread(CH_AT + 2 * 4410 - 300, CH_AT + 2 * 4410 + 1500);
    snprintf(b, sizeof b, "DIFF 100: the second repeat smeared (%u samples over a tenth of its peak, %u at DIFF 0)", s1, s0);
    check(b, s1 > 4u * s0);
    ch_delay(0, 0, 0, 1);
    for (k = 0; k < CHN; k++)                            /* (the echoes alone: the tone taken out) */
        chl[k] -= k < CHN / 2 ? (int32_t)lrint(12000 * sin(2 * M_PI * 1000 * k / 44100.0)) : 0;
    e0 = ch_energy(CHN / 4, CHN / 2);
    f0 = ch_energy(CHN / 2 + 2 * 4410, CHN);   /* (0.7-1 s: DUCK's ~190 ms let go) */
    ch_delay(0, 0, 100, 1);
    for (k = 0; k < CHN; k++)
        chl[k] -= k < CHN / 2 ? (int32_t)lrint(12000 * sin(2 * M_PI * 1000 * k / 44100.0)) : 0;
    e1 = ch_energy(CHN / 4, CHN / 2);
    f1 = ch_energy(CHN / 2 + 2 * 4410, CHN);
    snprintf(b, sizeof b, "DUCK 100: the echoes down while the track plays (%.0f dB under DUCK 0), back after it stops "
             "(%.0f dB)", 10 * log10(e0 / (e1 + 1)), 10 * log10(f0 / (f1 + 1)));
    check(b, e1 < e0 / 10.0 && f1 > f0 / 4.0);
    {
        static const char *const T[4] = {"ROOM", "PLAT", "HALL", "SPRG"};
        double late[4];
        int32_t pk[4];
        for (k = 0; k < 4u; k++) {
            uint32_t i;
            ch_room((int)k, 0);
            late[k] = ch_energy(CHN / 2, CHN);
            pk[k] = 0;
            for (i = 0; i < CHN; i++)
                pk[k] = abs(chl[i]) > pk[k] ? abs(chl[i]) : pk[k];
        }
        snprintf(b, sizeof b, "TYPE: each its own tail (the late half's energy %s %.2g, %s %.2g, %s %.2g, %s %.2g), "
                 "HALL the longest, none running away", T[0], late[0], T[1], late[1], T[2], late[2], T[3], late[3]);
        check(b, late[2] > late[0] && late[1] != late[0] && late[3] != late[0] && pk[0] < 32767 && pk[1] < 32767 &&
              pk[2] < 32767 && pk[3] < 32767);
    }
    ch_room(0, 0);
    e0 = ch_energy(CHN / 4, CHN);
    j0 = ch_jump(CHN / 4, CHN);
    memcpy(chr_, chl, sizeof chl);
    ch_room(0, 100);
    e1 = ch_energy(CHN / 4, CHN);
    j1 = ch_jump(CHN / 4, CHN);
    snprintf(b, sizeof b, "MOD 100: the tail moves (differs from MOD 0), no steps (largest bend %d, %d unmodulated), "
             "the same length (energy %.2f of it)", j1, j0, e1 / e0);
    check(b, memcmp(chl + CHN / 4, chr_ + CHN / 4, sizeof chl / 2) != 0 && j1 < 2 * j0 + 50 && e1 > 0.5 * e0 &&
          e1 < 2 * e0);
    power_on();
}

static void test_space(void)
{
    enum { N = 44100 };                                  /* 1 s */
    static int32_t l[N], r[N];
    int16_t *p;
    uint32_t i, a1, a2, t, tot0, tot1;
    double e1, e2, x, y;
    char b[160];
    power_on();
    p = tp[0].dev[DEV_SPACE];
    chain_poll();
    check("SPACE at DLY 0, VERB 0 (the defaults): no memory taken", !mem_count(MEM_SPACE) && !space[0].dly.nch);
    sine_fill(l, r, N, 441, 20000);
    memcpy(r, l, sizeof r);
    dev_run(sp_run, l, r, N);
    check("..and the sound untouched", !memcmp(l, r, sizeof r) && l[100] == (int32_t)lrint(20000 * sin(2 * M_PI * 441 * 100 / 44100.0)));

    p[SP_DLY] = 100;
    p[SP_FDBK] = 0;
    p[SP_TIME] = 100;
    p[SP_SPRD] = 0;
    chain_poll();
    check("DLY up: the delay's line, 8 chunks at 16 bits", space[0].dly.ready && !space[0].dly.bits8 &&
          mem_count(MEM_SPACE) == SP_DLY16);
    memset(l, 0, sizeof l);
    memset(r, 0, sizeof r);
    l[0] = r[0] = l[1] = r[1] = 20000;
    dev_run(sp_run, l, r, N / 2);
    a1 = peak_at(l, 100, N / 2);
    snprintf(b, sizeof b, "TIME 100, FDBK 0: one echo at %.1f ms (100) of %d (0.8 of the hit), none after", a1 / 44.1,
             l[a1]);
    check(b, fabs(a1 / 44.1 - 100) < 0.5 && l[a1] > 14000 && l[a1] < 17000 && peak_of(l + 5000, N / 2 - 5000) < 50 &&
          r[a1] == l[a1]);
    p[SP_FDBK] = 50;
    memset(l, 0, sizeof l);
    memset(r, 0, sizeof r);
    l[0] = r[0] = l[1] = r[1] = 20000;
    dev_run(sp_run, l, r, N / 2);
    a1 = peak_at(l, 100, 6000);
    a2 = peak_at(l, 6000, 11000);
    e1 = l[a2] / (double)l[a1];
    snprintf(b, sizeof b, "FDBK 50: a second echo at %.1f ms, %.2f of the first", a2 / 44.1, e1);
    check(b, fabs(a2 / 44.1 - 200) < 0.5 && e1 > 0.4 && e1 < 0.55);
    p[SP_TONE] = -100;
    memset(l, 0, sizeof l);
    memset(r, 0, sizeof r);
    l[0] = r[0] = l[1] = r[1] = 20000;
    dev_run(sp_run, l, r, N / 2);
    e2 = l[peak_at(l, 6000, 11000)] / (double)l[peak_at(l, 100, 6000)];
    p[SP_TONE] = 100;
    memset(l, 0, sizeof l);
    memset(r, 0, sizeof r);
    l[0] = r[0] = l[1] = r[1] = 20000;
    dev_run(sp_run, l, r, N / 2);
    x = abs(l[peak_at(l, 6000, 11000)]) / (double)l[peak_at(l, 100, 6000)];
    snprintf(b, sizeof b, "TONE -100 and +100: each pass loses more (the second echo %.2f, %.2f of the first; 0: %.2f)",
             e2, x, e1);
    check(b, e2 < e1 * 0.6 && x < e1 * 0.9);
    p[SP_TONE] = 0;
    p[SP_FDBK] = 0;
    p[SP_SPRD] = 100;
    memset(l, 0, sizeof l);
    memset(r, 0, sizeof r);
    l[0] = r[0] = l[1] = r[1] = 20000;
    dev_run(sp_run, l, r, N / 2);
    a1 = peak_at(l, 100, N / 2);
    a2 = peak_at(r, 100, N / 2);
    snprintf(b, sizeof b, "SPRD 100: the right echo %.1f ms ahead of the left (10)", (a1 - (double)a2) / 44.1);
    check(b, fabs((a1 - (double)a2) / 44.1 - 10) < 0.3);
    p[SP_SPRD] = 0;

    p[SP_TIME] = 100;                                    /* TIME turned while it sounds: a glide, no jump */
    sine_fill(l, r, N, 200, 10000);
    for (i = 0; i < N / 2; i += CTL)
        sp_run(0, l + i, r + i, CTL);
    p[SP_TIME] = 300;
    for (; i + CTL <= N; i += CTL)
        sp_run(0, l + i, r + i, CTL);
    for (i = 5000, a1 = 0; i < N - N % CTL; i++)
        a1 = abs(l[i] - l[i - 1]) > (int32_t)a1 ? (uint32_t)abs(l[i] - l[i - 1]) : a1;
    snprintf(b, sizeof b, "TIME turned 100 -> 300 ms while echoing: it glides (largest step %u; a 200 Hz sine's: 285 "
             "plus the echo's)", a1);
    check(b, a1 < 800);

    p[SP_DLY] = 0;
    chain_poll();
    check("DLY 0: the line back to the pool", !space[0].dly.nch && !mem_count(MEM_SPACE));
    fill_tape(1, (MEM_NC - 30u) / 2u);                   /* memory short: 20 chunks left free (two tapes take all but */
    fill_tape(2, MEM_NC - 30u - (MEM_NC - 30u) / 2u);    /* .. 30, imports the rest down to 20; 60 and 60 of 150) */
    while (mem_count(MEM_FREE) > 20u)
        mem_alloc(MEM_IMPORT);
    p[SP_DLY] = 100;
    p[SP_TIME] = 100;
    chain_poll();
    check("memory short (20 chunks free): the delay starts 8-bit, 4 chunks", space[0].dly.ready &&
          space[0].dly.bits8 && mem_count(MEM_SPACE) == SP_DLY8 && mem_count(MEM_FREE) == 16u);
    memset(l, 0, sizeof l);
    memset(r, 0, sizeof r);
    l[0] = r[0] = l[1] = r[1] = 20000;
    dev_run(sp_run, l, r, N / 2);
    a1 = peak_at(l, 100, N / 2);
    snprintf(b, sizeof b, "..its echo where 16 bits put it: %.1f ms, %d", a1 / 44.1, l[a1]);
    check(b, fabs(a1 / 44.1 - 100) < 0.5 && l[a1] > 13500 && l[a1] < 17000);
    {
        static int32_t s16[N], s8[N];
        sine_fill(l, r, N / 4, 300, 3000);
        memset(l + N / 4, 0, (N - N / 4) * sizeof l[0]);
        memset(r + N / 4, 0, (N - N / 4) * sizeof r[0]);
        dev_run(sp_run, l, r, N / 2);
        memcpy(s8, l, sizeof s8);
        p[SP_DLY] = 0;
        chain_poll();
        for (i = 0; i < MEM_NC; i++)
            if (mem_owner[i] == MEM_IMPORT)
                mem_free(i);
        p[SP_DLY] = 100;
        chain_poll();
        sine_fill(l, r, N / 4, 300, 3000);
        memset(l + N / 4, 0, (N - N / 4) * sizeof l[0]);
        memset(r + N / 4, 0, (N - N / 4) * sizeof r[0]);
        dev_run(sp_run, l, r, N / 2);
        memcpy(s16, l, sizeof s16);
        for (i = 6000, x = y = 0; i < N / 4; i++) {
            x += (double)(s8[i] - s16[i]) * (s8[i] - s16[i]);
            y += (double)s16[i] * s16[i];
        }
        snprintf(b, sizeof b, "..8-bit against 16: the echo of a quiet sine %.0f dB above its own noise",
                 10 * log10(y / x));
        check(b, 10 * log10(y / x) > 30 && space[0].dly.bits8 == 0);
    }
    p[SP_DLY] = 0;
    chain_poll();
    while (mem_count(MEM_FREE))                          /* nothing free: it takes from the longest tape */
        mem_alloc(MEM_IMPORT);
    tot0 = tape_ctl[1].nch + tape_ctl[2].nch;
    p[SP_DLY] = 100;
    chain_poll();
    tot1 = tape_ctl[1].nch + tape_ctl[2].nch;
    check("nothing free: 8-bit, its 4 chunks from the end of the longest tape", space[0].dly.ready &&
          space[0].dly.bits8 && tot0 - tot1 == SP_DLY8 && mem_count(MEM_SPACE) == SP_DLY8);
    p[SP_DLY] = 0;
    chain_poll();
    for (i = 0; i < MEM_NC; i++)
        if (mem_owner[i] == MEM_IMPORT)
            mem_free(i);
    for (t = 1; t < NTRK; t++)
        tape_free(t);
    tape_poll();

    p[SP_VERB] = 100;                                    /* the room: track 1 sends to A */
    rmp[0][RM_PRE] = 0;
    rmp[0][RM_DEC] = 100;
    chain_poll();
    check("VERB up: room A takes its lines (5 chunks, the pre-delay 5 at 16 bits), B and the track nothing",
          room[0].rev.ready && room[0].pre.ready && !room[0].pre.bits8 && mem_count(MEM_ROOM) == SP_REV + SP_PRE16 &&
          !mem_count(MEM_ROOM + 1) && !mem_count(MEM_SPACE) && room[0].users == 1u && !room[1].users);
    memset(l, 0, sizeof l);
    memset(r, 0, sizeof r);
    for (i = 0; i < 64; i++)
        l[i] = r[i] = 20000;
    dev_run(sp_run, l, r, N);
    e1 = rms_of(l, N / 2, N);
    x = rms_of(l, 2000, 6000);
    rmp[0][RM_DEC] = 0;
    memset(l, 0, sizeof l);
    memset(r, 0, sizeof r);
    dev_run(sp_run, l, r, N);                            /* (the long tail out) */
    for (i = 0; i < 64; i++)
        l[i] = r[i] = 20000;
    dev_run(sp_run, l, r, N);
    e2 = rms_of(l, N / 2, N);
    snprintf(b, sizeof b, "DEC 100 against 0: the tail at 0.5-1 s %.0f against %.1f (the early tail %.0f)", e1, e2, x);
    check(b, e1 > 30 && e2 < e1 / 30 && x > 100);
    rmp[0][RM_DEC] = 40;
    memset(l, 0, sizeof l);
    memset(r, 0, sizeof r);
    dev_run(sp_run, l, r, N / 4);
    for (i = 0; i < 64; i++)
        l[i] = r[i] = 20000;
    dev_run(sp_run, l, r, N / 2);
    for (i = 3000, x = y = e1 = 0; i < N / 2; i++) {
        x += (double)l[i] * r[i];
        y += (double)l[i] * l[i];
        e1 += (double)r[i] * r[i];
    }
    snprintf(b, sizeof b, "a room is stereo: its sides differ (correlation %.2f)", x / sqrt(y * e1));
    check(b, x / sqrt(y * e1) < 0.5);
    rmp[0][RM_PRE] = 100;
    for (i = 0; i < 3; i++) {                            /* (the last tail out) */
        memset(l, 0, sizeof l);
        memset(r, 0, sizeof r);
        dev_run(sp_run, l, r, N);
    }
    memset(l, 0, sizeof l);
    memset(r, 0, sizeof r);
    for (i = 0; i < 64; i++)
        l[i] = r[i] = 20000;
    dev_run(sp_run, l, r, N / 2);
    snprintf(b, sizeof b, "PRE 100 ms: nothing from the room before it (peak %d), the room after (%.0f)",
             peak_of(l + 100, 4300), rms_of(l, 4500, 9000));
    check(b, peak_of(l + 100, 4300) < 20 && rms_of(l, 4500, 9000) > 100);
    rmp[0][RM_PRE] = 20;

    {                                                    /* QUAL LO: half the rate, the same room */
        static int32_t hi[N];
        double ehi, elo;
        for (t = 0; t < 2; t++) {
            rmp[0][RM_QUAL] = (int16_t)t;
            for (i = 0; i < 3; i++) {
                memset(l, 0, sizeof l);
                memset(r, 0, sizeof r);
                dev_run(sp_run, l, r, N);
            }
            memset(l, 0, sizeof l);
            memset(r, 0, sizeof r);
            for (i = 0; i < 64; i++)
                l[i] = r[i] = 20000;
            dev_run(sp_run, l, r, N);
            if (!t)
                memcpy(hi, l, sizeof hi);
        }
        ehi = rms_of(hi, N / 4, N / 2);
        elo = rms_of(l, N / 4, N / 2);
        for (i = 100, a1 = 0; i < N; i++)
            a1 = abs(l[i] - l[i - 1]) > (int32_t)a1 ? (uint32_t)abs(l[i] - l[i - 1]) : a1;
        snprintf(b, sizeof b, "QUAL LO (11.025 kHz): the tail as long (0.25-0.5 s: %.0f against HI's %.0f), stepped "
                 "back up smoothly (largest step %u), darker", elo, ehi, a1);
        check(b, elo > ehi / 2 && elo < ehi * 2 && a1 < 2000 && room[0].qual == 1u);
        {                                                /* what goes in at LO: averaged in pairs first, so a high */
            double e10, e1k;                             /* .. sound doesn't fold down into the room (aliasing) */
            for (t = 0; t < 2; t++) {
                for (i = 0; i < 3; i++) {
                    memset(l, 0, sizeof l);
                    memset(r, 0, sizeof r);
                    dev_run(sp_run, l, r, N);
                }
                sine_fill(l, r, N / 4, t ? 1000 : 10000, 10000);
                memset(l + N / 4, 0, (N - N / 4) * sizeof l[0]);
                memset(r + N / 4, 0, (N - N / 4) * sizeof r[0]);
                dev_run(sp_run, l, r, N / 2);
                if (!t)
                    e10 = rms_of(l, N / 4 + 2000, N / 2);
                else
                    e1k = rms_of(l, N / 4 + 2000, N / 2);
            }
            snprintf(b, sizeof b, "..a 10 kHz sound sent at LO: its tail %.0f against a 1 kHz one's %.0f (averaged in "
                     "pairs: not folded down to 1 kHz)", e10, e1k);
            check(b, e10 < e1k * 0.3);
        }
        rmp[0][RM_QUAL] = RQ_HI;
        {                                                /* HI: back up to 44.1 kHz halfway, then the sample: the */
            double hi18, lo4;                            /* .. images above 16 kHz held down */
            for (i = 0; i < 3; i++) {
                memset(l, 0, sizeof l);
                memset(r, 0, sizeof r);
                dev_run(sp_run, l, r, N);
            }
            rmp[0][RM_TONE] = 100;                       /* (bright: plenty at 4 kHz to image) */
            memset(l, 0, sizeof l);
            memset(r, 0, sizeof r);
            for (i = 0; i < 2048; i++)
                l[i] = r[i] = (int32_t)(rand() % 20001) - 10000;
            dev_run(sp_run, l, r, N / 2);
            rmp[0][RM_TONE] = 0;
            hi18 = goertzel(l, 4000, 12000, 18000);
            lo4 = goertzel(l, 4000, 12000, 22050 - 18000);
            snprintf(b, sizeof b, "..HI's tail stepped back up to 44.1 kHz: its image at 18 kHz %.0f dB under what it "
                     "mirrors at 4.05 kHz", 10 * log10(lo4 / hi18));
            check(b, 10 * log10(lo4 / hi18) > 16);
        }
    }

    {                                                    /* two tracks in one room, and the send at their level */
        static int32_t one[N], l2[N], r2[N];
        uint32_t j;
        tp[1].dev[DEV_SPACE][SP_VERB] = 100;
        tp[1].dev[DEV_SPACE][SP_ROOM] = 0;
        chain_poll();
        check("track 2 sends to room A too: still one room's memory (A's lines shared, B none)",
              room[0].users == 3u && mem_count(MEM_ROOM) == SP_REV + SP_PRE16 && !mem_count(MEM_ROOM + 1));
        for (j = 0; j < 3; j++) {
            int32_t g0 = j == 1 ? 0 : 4096, g1 = j == 0 ? 0 : 4096;
            sp_quiet2(l, r, l2, r2, g0, g1);
            for (i = 0; i < 64; i++)                     /* (8,000: the two together inside the room's 16 bits) */
                l[i] = r[i] = l2[i] = r2[i] = 8000;
            for (i = 0; i + CTL <= N / 4; i += CTL) {
                space_block(0, l + i, r + i, CTL, g0);
                space_block(1, l2 + i, r2 + i, CTL, g1);
                space_rooms(l + i, r + i, CTL);
            }
            if (j == 0)
                memcpy(one, l, sizeof one);
            else if (j == 1) {
                x = 0;
                for (i = 2000; i < N / 4; i++)
                    x += fabs((double)l[i] - one[i]);
                check("..track 2 alone at the same level: the same tail as track 1's", x / (N / 4 - 2000) < 2);
            }
        }
        x = rms_of(l, 2000, N / 4) / rms_of(one, 2000, N / 4);
        snprintf(b, sizeof b, "..both at once: one tail, the sum of theirs (%.2f x one)", x);
        check(b, x > 1.9 && x < 2.1);
        for (j = 0; j < 2; j++) {
            sp_quiet2(l, r, l2, r2, j ? 2048 : 0, 0);
            for (i = 0; i < 64; i++)
                l[i] = r[i] = 8000;
            for (i = 0; i + CTL <= N / 4; i += CTL) {
                space_block(0, l + i, r + i, CTL, j ? 2048 : 0);
                space_rooms(l + i, r + i, CTL);
            }
            if (!j)
                check("..the send follows the track's level: at 0 (or muted: chain_block hands it 0) nothing reaches "
                      "the room", peak_of(l + 100, N / 4 - 100) == 0);
        }
        x = rms_of(l, 2000, N / 4) / rms_of(one, 2000, N / 4);
        snprintf(b, sizeof b, "..at half its level, half the tail (%.2f)", x);
        check(b, x > 0.45 && x < 0.55);
        tp[1].dev[DEV_SPACE][SP_ROOM] = 1;
        chain_poll();
        check("track 2 to room B: B takes its own lines (A keeps track 1's)", room[1].rev.ready && room[1].pre.ready &&
              room[0].users == 1u && room[1].users == 2u && mem_count(MEM_ROOM + 1) == SP_REV + SP_PRE16);
        tp[1].dev[DEV_SPACE][SP_VERB] = 0;
        chain_poll();
        check("..its VERB 0: B gives its lines back", !room[1].users && !mem_count(MEM_ROOM + 1) && !room[1].rev.nch);
    }

    for (i = 0; i < NPK; i++)                            /* the corners: everything up, noise in, then quiet */
        if (!pdesc_empty(&DEV_P[DEV_SPACE][i]))
            p[i] = DEV_P[DEV_SPACE][i].max;
    for (i = 0; i < NRM; i++)
        rmp[1][i] = RM_P[i].max;
    for (t = 0; t < 4; t++) {
        p[SP_TONE] = (int16_t)(t == 0 ? -100 : t == 1 ? 100 : 0);
        rmp[1][RM_TONE] = p[SP_TONE];
        rmp[1][RM_SIZE] = (int16_t)(t * 50 > 100 ? 100 : t * 50);
        rmp[1][RM_QUAL] = (int16_t)(t == 3);
        chain_poll();
        for (i = 0; i < N; i++)
            l[i] = r[i] = (int32_t)(rand() % 65535) - 32767;
        dev_run(sp_run, l, r, N);
        e1 = peak_of(l, N);
        memset(l, 0, sizeof l);
        memset(r, 0, sizeof r);
        dev_run(sp_run, l, r, N);
        snprintf(b, sizeof b, "the corners (room B, TONE %d, SIZE %d, QUAL %s, all else up): bounded (peak %.0f), "
                 "%.0f after 1 s of quiet", p[SP_TONE], rmp[1][RM_SIZE], t == 3 ? "LO" : "HI", e1,
                 (double)peak_of(l + N / 2, N / 2));
        check(b, e1 < 4 * 32767 && peak_of(l, N) < 3 * 32767);
    }
    for (i = 0; i < NRM; i++)
        rmp[1][i] = RM_P[i].def;
    for (i = 0; i < NPK; i++)
        p[i] = pdesc_empty(&DEV_P[DEV_SPACE][i]) ? 0 : DEV_P[DEV_SPACE][i].def;
    p[SP_DLY] = p[SP_VERB] = 100;
    chain_poll();
    chain_tracks(1);
    tp[1].dev[DEV_SPACE][SP_DLY] = 100;                 /* (track 2's SPACE on, track 2 parked) */
    chain_poll();
    check("a parked track's SPACE holds nothing", !space[1].dly.nch && !mem_count(MEM_SPACE + 1));
    chain_tracks(4);
    chain_poll();
    check("..TRACKS back up: it takes its line", space[1].dly.ready);
    tp[1].dev[DEV_SPACE][SP_DLY] = 0;
    p[SP_DLY] = p[SP_VERB] = 0;
    chain_poll();
    check("DLY and VERB 0: every chunk back to the pool", !mem_count(MEM_SPACE) && !mem_count(MEM_SPACE + 1) &&
          !mem_count(MEM_ROOM) && !mem_count(MEM_ROOM + 1) && !room[0].rev.nch && !room[0].pre.nch);

    power_on();                                          /* the room's knobs on SPACE's pages: the room's, shared */
    sys.sel = 0;
    focus_dev(DEV_SPACE, 1);
    turn(1, 6);
    check("SPACE 2 on T1 (room A): KNOB 2 turns room A's SIZE (50 -> 56), not a knob of the track's",
          rmp[0][RM_SIZE] == 56 && rmp[1][RM_SIZE] == 50 && tp[0].dev[DEV_SPACE][3] == 0);
    sys.sel = 2;
    focus_dev(DEV_SPACE, 1);
    check("..T3's SPACE 2 shows room B's (50)", ui.view == VIEW_PAGE && rmp[1][RM_SIZE] == 50);
    turn(2, 10);
    turn(0, -1);                                         /* T3 to room A: its page now shows and turns A's */
    turn(1, 4);
    check("..its DEC turns B's; ROOM A then puts T3 on A, and SIZE turns A's (56 -> 60)",
          rmp[1][RM_DEC] == 50 && tp[2].dev[DEV_SPACE][SP_ROOM] == 0 && rmp[0][RM_SIZE] == 60);
    focus_dev(DEV_SPACE, 2);
    turn(3, 1);
    check("..SPACE 3's QUAL is the room's too (A to LO)", rmp[0][RM_QUAL] == RQ_LO);
    focus_dev(DEV_SPACE, 1);
    hold(B_LFO);
    turn(1, 20);
    let_go(B_LFO);
    check("..LFO held + the room's SIZE: no depth (the room is shared), and it says so",
          mod_count(2) == 0 && rmp[0][RM_SIZE] == 60 && !strncmp(ui.msg, "THE ROOM IS SHARED", 18));
    power_on();
}

/* SYNC: TAPE's loop, GRAIN's grains and SPACE's delay to the tempo */
static int32_t sync_err(uint32_t t)                      /* the head against where the beat says it is, samples */
{
    tape_view_t v;
    int32_t ls, ll, o, d;
    uint32_t per = tape_sync_bars(t) * 4u, f;
    tape_view(t, &v);
    tape_window(t, v.len, &ls, &ll);
    f = tape_sync_frac(t, per, 0);
    o = (int32_t)(((uint32_t)ll >> 16) * f + ((((uint32_t)ll & 0xFFFFu) * f) >> 16));
    d = ls + o - (tape_rt[t].pos >> 12);
    if (d > ll / 2)
        d -= ll;
    else if (d < -ll / 2)
        d += ll;
    return d;
}
static void test_sync(void)
{
    char b[200];
    int32_t e, worst = 0;
    uint32_t k;
    static int32_t l[44100], r[44100];
    power_on();
    tp[0].dev[DEV_SRC][TK_SYNC] = 1;                     /* T1's 2 s reel as 1 bar */
    sys.bpm = 100;                                       /* (1 bar: 2.4 s) */
    sys.playing = 1;
    render(1378u, 0);
    for (k = 0; k < 1378u * 30u; k++) {
        render(1, 0);
        e = abs(sync_err(0));
        worst = e > worst ? e : worst;
    }
    snprintf(b, sizeof b, "TAPE SYNC 1BR at 100 BPM: the 2 s loop stretched to the bar's 2.4 s, on the beat for 30 s "
             "(the head at most %d samples, %.1f ms, off where it belongs)", worst, worst / 22.05);
    check(b, worst < 64);
    sys.bpm = 140;                                       /* a new tempo while it plays */
    render(1378u * 2u, 0);
    for (k = 0, worst = 0; k < 1378u * 10u; k++) {
        render(1, 0);
        e = abs(sync_err(0));
        worst = e > worst ? e : worst;
    }
    snprintf(b, sizeof b, "..140 BPM while playing: locked again within 2 s, then at most %d samples off", worst);
    check(b, worst < 64);
    fm1_in.notes = note_bit_of_white(4);                 /* a slice while synced: the loop goes on from it */
    render(1, 0);
    fm1_in.notes = 0;
    render(1378u * 3u, 0);
    snprintf(b, sizeof b, "..white key 5: the loop jumps to slice 5 and keeps to the beat from there (%d samples off, "
             "the jump kept: %u)", sync_err(0), tape_rt[0].soff);
    check(b, abs(sync_err(0)) < 64 && tape_rt[0].soff != 0u);
    sys.playing = 0;
    render(400, 0);

    power_on();                                          /* GRAIN SYNC: a grain on every beat */
    tp[0].dev[DEV_GRAIN][GP_WET] = 100;
    tp[0].dev[DEV_GRAIN][GP_SYNC] = 1;
    tp[0].dev[DEV_GRAIN][GP_RATE] = 25;                  /* (1/4) */
    tp[0].dev[DEV_GRAIN][GP_PATN] = 0;                   /* EVEN */
    tp[0].dev[DEV_GRAIN][GP_SIZE] = 50;
    chain_poll();
    sys.playing = 1;
    {
        uint32_t n0 = grain[0].n, n, off = 0;
        for (k = 0; k < 1378u * 4u; k++) {
            n = grain[0].n;
            render(1, 0);
            if (grain[0].n != n && ((0x10000u - (mclk.beat & 0xFFFFu)) & 0xFFFFu) > mclk.inc + 1u)
                off++;                                   /* (a grain whose block didn't hold a beat) */
            if (k % 64u == 0u)
                chain_poll();
        }
        snprintf(b, sizeof b, "GRAIN SYNC 1/4 at 120: %u grains in 4 s (8 beats), each in the block its beat falls in "
                 "(%u not)", grain[0].n - n0, off);
        check(b, grain[0].n - n0 >= 7u && grain[0].n - n0 <= 9u && off == 0u);
    }
    sys.playing = 0;
    render(400, 0);

    power_on();                                          /* SPACE SYNC: TIME as a division */
    tp[0].dev[DEV_SPACE][SP_DLY] = 100;
    tp[0].dev[DEV_SPACE][SP_FDBK] = 0;
    tp[0].dev[DEV_SPACE][SP_SPRD] = 0;
    tp[0].dev[DEV_SPACE][SP_SYNC] = 1;
    tp[0].dev[DEV_SPACE][SP_TIME] = 200;                 /* 1/8 */
    chain_poll();
    {
        uint32_t a, j;
        int32_t T[3] = {200, 200, 300};                  /* 1/8 at 120 and 100, 1/4 (folded) at 120 */
        uint16_t B[3] = {120, 100, 120};
        double want[3] = {250.0, 300.0, 250.0};
        for (j = 0; j < 3u; j++) {
            tp[0].dev[DEV_SPACE][SP_TIME] = (int16_t)T[j];
            sys.bpm = B[j];
            memset(l, 0, sizeof l);
            memset(r, 0, sizeof r);
            dev_run(sp_run, l, r, 44100);                /* (the last echo out; TIME's glide there) */
            memset(l, 0, sizeof l);
            memset(r, 0, sizeof r);
            l[0] = r[0] = l[1] = r[1] = 20000;
            dev_run(sp_run, l, r, 44100 / 2);
            a = peak_at(l, 100, 44100 / 2);
            snprintf(b, sizeof b, "SPACE SYNC: %s at %u BPM, the echo at %.1f ms (%.0f)", j == 2u ? "1/4 (too long "
                     "for the line: halved)" : "1/8", B[j], a / 44.1, want[j]);
            check(b, fabs(a / 44.1 - want[j]) < 1.0);
        }
    }
    sys.bpm = 120;
    power_on();
}

/* the computer playing into the FM-1 (what usb.c's uaco_service hands usbrec.c from each 1 ms packet): frames of a
 * sine, l at amplitude a, r at half, phase kept across calls */
static double ur_ph;
static void ur_feed(uint32_t frames, double hz, double a)
{
    uint8_t p[46 * 4];
    while (frames) {
        uint32_t n = frames > 45u ? 45u : frames, i;
        for (i = 0; i < n; i++) {
            int16_t l = (int16_t)lrint(a * sin(ur_ph)), r = (int16_t)lrint(a * 0.5 * sin(ur_ph));
            ur_ph += 2 * M_PI * hz / 44100.0;
            p[4 * i] = (uint8_t)l;
            p[4 * i + 1] = (uint8_t)((uint16_t)l >> 8);
            p[4 * i + 2] = (uint8_t)r;
            p[4 * i + 3] = (uint8_t)((uint16_t)r >> 8);
        }
        uaco_frames(p, n);
        frames -= n;
    }
}

/* blocks rendered as the device would: the computer's frames at 44,100 a second against the DAC's 44,117.6, the
 * main loop between blocks; the left output into buf (or 0) */
static double ur_due;
static void ur_play(uint32_t blocks, double hz, double a, int32_t *buf)
{
    uint32_t b;
    for (b = 0; b < blocks; b++) {
        uint32_t n;
        ur_due += CTL * 44100.0 / 44117.647;
        n = (uint32_t)ur_due;
        ur_due -= n;
        if (a > 0)
            ur_feed(n, hz, a);
        render(1, buf ? buf + b * CTL : 0);
        chain_poll();
        fm1_ms += (b & 1u) ? 1u : 0u;
    }
}

/* the USB record mode (usbrec.c): REC held a second opens it, the computer is heard, a take records exactly what
 * arrived, goes onto the track picked, and the mode closes */
static void test_usbrec(void)
{
    enum { NB = 2756 };                                  /* 2 s */
    static int32_t s[NB * CTL];
    char b[160];
    uint32_t i, k, lo = 0xFFFFFFFFu, hi = 0, nb, frames;
    int32_t jump;
    double hz;
    power_on();
    memset(&ur, 0, sizeof ur);
    ur_mw = ur_mr = ur_rw = ur_rr = 0;
    tp[0].dev[DEV_GRAIN][GP_WET] = 100;                  /* (memory the tracks hold: it goes back in the mode) */
    tp[1].dev[DEV_SPACE][SP_DLY] = 100;
    chain_poll();
    check("before: GRAIN's buffer and SPACE's line hold memory", mem_count(MEM_GRAIN) > 0 && mem_count(MEM_SPACE + 1) > 0);

    sys.playing = 1;                                     /* playing: REC arms as it goes down; held, it says why not */
    hold(B_REC);
    check("playing: REC arms the moment it goes down (a punch-in where it's pressed)", (sys.rec & 1u) == 1u);
    fm1_ms += 1100;
    ui_input();
    check("..held a second while playing: no record mode, a hint to stop first",
          ui.view != VIEW_USBREC && !sys.usbrec && !strcmp(ui.msg, "STOP, THEN HOLD REC: USB RECORD"));
    let_go(B_REC);
    press(B_REC);
    check("..REC again lets go of it", sys.rec == 0);
    sys.playing = 0;

    hold(B_REC);                                         /* stopped: the arm waits for the let-go */
    check("stopped: REC down arms nothing yet", sys.rec == 0);
    fm1_ms += 400;
    ui_input();
    check("..held 0.4 s: \"KEEP HOLDING REC: USB RECORD\"", !strcmp(ui.msg, "KEEP HOLDING REC: USB RECORD") && !sys.usbrec);
    fm1_ms += 700;
    ui_input();
    let_go(B_REC);
    check("..held a second: the USB record mode (READY), and REC's let-go arms nothing",
          ui.view == VIEW_USBREC && sys.usbrec && ur.state == UR_READY && sys.rec == 0 && !sys.playing);
    chain_poll();
    check("the tracks' devices give their memory back (GRAIN's buffer, SPACE's line)",
          !mem_count(MEM_GRAIN) && !mem_count(MEM_SPACE + 1));
    press(B_PLAY);
    check("PLAY does nothing in the mode", !sys.playing);
    render(1, 0);
    check("one block fades the tracks out, then the ISR plays the computer", ur.isr_in == 1);

    ur_play(NB, 441, 12000, s);                          /* the monitor: 2 s of a sine from the computer */
    hz = buf_pitch(s, NB * CTL / 2, NB * CTL, 200, 1000);
    for (i = NB * CTL / 4, jump = 0; i < NB * CTL; i++)
        jump = abs(s[i] - s[i - 1]) > jump ? abs(s[i] - s[i - 1]) : jump;
    for (k = 0; k < 2000; k++) {                         /* the ring's fill, the computer's clock against the DAC's */
        uint32_t f;
        ur_play(1, 441, 12000, 0);
        f = ur_mw - ur_mr;
        lo = f < lo ? f : lo;
        hi = f > hi ? f : hi;
    }
    snprintf(b, sizeof b, "the computer heard: %.1f Hz (441), no step over %d, the ring held at %u..%u frames (aim %u)",
             hz, jump, lo, hi, UR_MON_AIM);
    check(b, fabs(hz - 441) < 2 && jump < 1400 && lo + 48u >= UR_MON_AIM && hi <= UR_MON_AIM + 48u);
    check("..the computer playing: the screen knows (live)", usbrec_live());
    fm1_ms += 300;
    usbrec_poll();
    check("..300 ms with nothing: not live", !usbrec_live());

    press(B_REC);                                        /* a take */
    check("REC: recording", ur.state == UR_RECORDING && ur.feed);
    frames = 0;
    for (k = 0; k < 3u * 1378u; k++) {                   /* 3 s of 300 Hz, in packets of 44 and 45 frames */
        uint32_t n = (k % 10u) == 9u ? 45u : 44u;
        ur_feed(n / 2u, 300, 10000);
        ur_feed(n - n / 2u, 300, 10000);
        frames += n;
        render(1, 0);
        if (k % 4u == 0u)
            chain_poll();
    }
    chain_poll();
    press(B_REC);
    nb = (frames / 2u + TAPE_BLK - 1u) / TAPE_BLK;
    snprintf(b, sizeof b, "REC again: CHOOSE, the take every frame that arrived (%u frames: %u blocks, %u in it), none lost",
             frames, nb, ur.nblk);
    check(b, ur.state == UR_CHOOSE && ur.nblk == nb && ur.lost == 0 && ur.dest < 0);
    press(B_REC);
    check("..REC before a track is picked: it asks for one", ur.state == UR_CHOOSE &&
          !strcmp(ui.msg, "PICK A TRACK: WHITE KEYS 1-4"));
    key_edge(note_bit_of_white(2) ? (uint32_t)__builtin_ctz(note_bit_of_white(2)) : 0u);
    check("..white key 3: track 3 picked", ur.dest == 2);
    check("CHOOSE starts with the whole take: START 0, LENGTH all of it, GAIN 0 dB, FADE 10 ms",
          ur.kv[UK_STRT] == 0 && ur.kv[UK_LEN] == (int16_t)nb && ur.kv[UK_GAIN] == 0 &&
          ur.kv[UK_FADE] * UR_FADE_MS == 10);
    render(NB / 2, s);                                   /* the preview: the take, looping */
    hz = buf_pitch(s, 0, NB / 2 * CTL, 100, 1000);
    snprintf(b, sizeof b, "..you hear the take looping (%.1f Hz, 300), not the computer", hz);
    check(b, fabs(hz - 300) < 1.5 && peak_of(s, NB / 2 * CTL) > 2000);
    {
        uint32_t keep0 = ur_free_after(), k2;
        for (k2 = 0; k2 < 4u; k2++)
            usbrec_knob(UK_STRT, 4);                     /* turned fast: 16 blocks a detent; 4 x 4 x 16 = 256 */
        for (k2 = 0; k2 < 213u; k2++)
            usbrec_knob(UK_STRT, -1);                    /* slowly, a block a detent: back to 43 */
        for (k2 = 0; k2 < 30u; k2++)
            usbrec_knob(UK_LEN, -4);                     /* fast to the least (1 block), then slowly to 86 (1.0 s) */
        for (k2 = 0; k2 < 85u; k2++)
            usbrec_knob(UK_LEN, 1);
        snprintf(b, sizeof b, "KNOB 1-2: START 43 blocks (0.50 s), LENGTH 86 (1.00 s): %d, %d; %u chunks kept, more left "
                 "for the other tracks", ur.kv[UK_STRT], ur.kv[UK_LEN], ur_keep_chunks());
        check(b, ur.kv[UK_STRT] == 43 && ur.kv[UK_LEN] == 86 && ur_keep_chunks() == 6u && ur_free_after() > keep0 &&
              ur.pv_s == 43u * TAPE_BLK && ur.pv_e == 129u * TAPE_BLK);
        usbrec_knob(UK_GAIN, 40);
        snprintf(b, sizeof b, "KNOB 3 past +24 dB: NORM, the loudest point to full scale (x%.2f; the mono take peaks "
                 "at 7,500)", ur.pv_g / 4096.0);
        check(b, ur.kv[UK_GAIN] == UR_NORM && fabs(ur.pv_g / 4096.0 - 32767.0 / 7500) < 0.1);
        usbrec_knob(UK_GAIN, -31);
        usbrec_knob(UK_FADE, 18);
        check("..GAIN -6 dB (x0.5), FADE 100 ms", ur.kv[UK_GAIN] == -6 && ur.pv_g == 2048 &&
              ur.pv_f == 100u * (TAPE_SR / 1000u));
    }
    press(B_REC);
    snprintf(b, sizeof b, "..REC: track 3's tape is the trimmed take (%u blocks), the mode closes, track 3 focused, the "
             "rest of the take back in the pool", tape_ctl[2].nblk);
    check(b, tape_ctl[2].nblk == 86u && tape_ctl[2].nch == 6u && !tape_ctl[2].empty && ur.state == UR_OFF &&
          !sys.usbrec && ui.view == VIEW_PAGE && sys.sel == 2 && !mem_count(MEM_IMPORT));
    {
        tape_view_t v;
        tape_rd_t rd;
        int32_t first = 0, mid = 0, last = 0;
        memset(&rd, 0, sizeof rd);
        tape_view(2, &v);
        for (i = 0; i < 220u; i++)                       /* the first 10 ms of the fade in */
            first = abs(tape_at(&v, &rd, (int32_t)i)) > first ? abs(tape_at(&v, &rd, (int32_t)i)) : first;
        for (i = 6000u; i < 12000u; i++)
            mid = abs(tape_at(&v, &rd, (int32_t)i)) > mid ? abs(tape_at(&v, &rd, (int32_t)i)) : mid;
        for (i = 86u * TAPE_BLK - 220u; i < 86u * TAPE_BLK; i++)
            last = abs(tape_at(&v, &rd, (int32_t)i)) > last ? abs(tape_at(&v, &rd, (int32_t)i)) : last;
        snprintf(b, sizeof b, "..kept as shaped: faded in (%d in the first 10 ms) and out (%d), at half (%d; was 7,500)",
                 first, last, mid);
        check(b, first < 1000 && last < 1000 && mid > 3400 && mid < 4100);
    }
    render(2, 0);
    check("..the monitor fades out and the tracks render again", ur.isr_in == 0);
    for (k = 0; k < NTRK; k++)
        track[k].mute = k != 2u;
    sys.playing = 1;
    render(NB / 4, 0);
    render(NB / 2, s);
    sys.playing = 0;
    hz = buf_pitch(s, 0, NB / 2 * CTL, 100, 1000);
    snprintf(b, sizeof b, "..and it plays: %.1f Hz (300)", hz);
    check(b, fabs(hz - 300) < 1.5 && peak_of(s, NB / 2 * CTL) > 500);
    for (k = 0; k < NTRK; k++)
        track[k].mute = 0;

    hold(B_REC);                                         /* memory full: the take stops where it runs out */
    fm1_ms += 1100;
    ui_input();
    let_go(B_REC);
    chain_poll();
    while (mem_count(MEM_FREE) > 3u)
        mem_alloc(MEM_SPARE);
    press(B_REC);
    for (k = 0; k < 1378u && ur.state == UR_RECORDING; k++) {
        ur_feed(44, 300, 10000);
        render(1, 0);
        chain_poll();
    }
    snprintf(b, sizeof b, "3 chunks free: the take stops at them (%u blocks), CHOOSE, \"MEMORY FULL\"", ur.nblk);
    check(b, ur.state == UR_CHOOSE && ur.nblk == 3u * MEM_CB && ur.nch == 3u &&
          !strcmp(ui.msg, "THE TAKE STOPPED: MEMORY FULL"));
    press(B_HOME);
    check("HOME: the take thrown away, its chunks back (READY)", ur.state == UR_READY && !ur.nch &&
          mem_count(MEM_FREE) == 3u);
    press(B_HOME);
    check("HOME again: the mode closes", ur.state == UR_OFF && !sys.usbrec && ui.view == VIEW_PAGE);
    for (i = 0; i < MEM_NC; i++)
        if (mem_owner[i] == MEM_SPARE)
            mem_free(i);
}

/* a sine through track 0's channel strip (and pan): its level after, against before, in dB (the second half) */
static double ch_db(double hz, double amp)
{
    enum { N = 8192 };
    static int32_t l[N], r[N];
    uint32_t i;
    sine_fill(l, r, N, hz, amp);
    memset(&chan[0], 0, sizeof chan[0]);
    for (i = 0; i + CTL <= N; i += CTL)
        mix_channel(0, l + i, r + i, CTL);
    return 20 * log10(rms_of(l, N / 2, N) / (amp / M_SQRT2));
}

/* the mixer's sound (mixer.c): each channel strip does what its picture draws, the pan, the master compressor */
/* a channel strip's shelf gain changing between blocks (as a modulator moves LOW or HIGH a block at a time) glides
 * across the block, no jump at its edge; a shelf passing through 0 dB keeps its state */
static void test_chan_glide(void)
{
    static int32_t l[CTL], r[CTL];
    int32_t last = 0, jump = 0, b, i, k;
    power_on();
    for (k = 0; k < 2; k++) {                            /* LOW, then HIGH (a sine at 100 Hz, then at 2 kHz) */
        int16_t *g = &tp[0].ch[k ? CH_HIGH : CH_LOW];
        uint32_t ph = 0, inc = (k ? 2000u : 100u) * (0xFFFFFFFFu / 44100u);
        jump = 0;
        for (b = 0; b < 400; b++) {
            *g = (int16_t)(b < 200 ? (b & 1 ? 6 : 0) : (b & 1 ? 12 : -12));   /* (the worst: a whole range a block) */
            for (i = 0; i < (int32_t)CTL; i++, ph += inc)
                l[i] = r[i] = sine_i(ph) >> 3;
            mix_channel(0, l, r, CTL);
            for (i = 0; i < (int32_t)CTL; i++) {
                int32_t d = l[i] - last;
                d = d < 0 ? -d : d;
                jump = b > 4 && d > jump ? d : jump;
                last = l[i];
            }
        }
        *g = 0;
        mix_channel(0, l, r, CTL);
        /* (measured: stepping at the block edges, LOW's largest sample-to-sample move was 14,589 and HIGH's 12,605;
         * gliding, 656 and 3,464: the gain's own ramp and the louder sine's slope) */
        check(k ? "HIGH swung 0/+6 and -12/+12 dB a block: it glides across each block, no jump at the edges"
                : "LOW swung 0/+6 and -12/+12 dB a block: it glides across each block, no jump at the edges",
              jump < (k ? 5000 : 1500));
    }
}

/* a shelf that stopped (its gain at 0, the strip off) and the signal moved on meanwhile: back on, it starts from the
 * signal as it is, not from what it held when it stopped */
static void test_chan_reseed(void)
{
    static int32_t l[CTL], r[CTL];
    int32_t b, i, worst = -65536;
    power_on();
    for (b = 0; b < 100; b++) {                          /* LOW +12 on +8000: its low-pass settles at +8000 */
        tp[0].ch[CH_LOW] = 12;
        for (i = 0; i < (int32_t)CTL; i++)
            l[i] = r[i] = 8000;
        mix_channel(0, l, r, CTL);
    }
    tp[0].ch[CH_LOW] = 0;
    for (b = 0; b < 10; b++) {                           /* off (after its ramp down, still on +8000), then the
                                                         * signal -8000 while it's stopped */
        for (i = 0; i < (int32_t)CTL; i++)
            l[i] = r[i] = b ? -8000 : 8000;
        mix_channel(0, l, r, CTL);
    }
    tp[0].ch[CH_LOW] = 12;
    for (b = 0; b < 3; b++) {                            /* back on: -8000 only gets lower (x up to 4), never above */
        for (i = 0; i < (int32_t)CTL; i++)
            l[i] = r[i] = -8000;
        mix_channel(0, l, r, CTL);
        for (i = 0; i < (int32_t)CTL; i++)
            worst = l[i] > worst ? l[i] : worst;
    }
    check("a low shelf back on after the signal moved: it starts from the signal now (no swing toward the old one)",
          worst <= -8000);
    tp[0].ch[CH_LOW] = 0;
}

static void test_mixer(void)
{
    enum { N = 8192 };
    static int32_t l[N], r[N], l0[N];
    int16_t *ch;
    char b[160];
    double a, c, d;
    uint32_t i, k;
    int32_t k2_dummy;
    power_on();
    ch = tp[0].ch;
    sine_fill(l, r, N, 441, 20000);
    memcpy(l0, l, sizeof l0);
    for (i = 0; i + CTL <= N; i += CTL) {
        mix_channel(0, l + i, r + i, CTL);
        mix_pan(0, l + i, r + i, 4096, CTL);
        mix_comp(l + i, r + i, CTL);
    }
    check("the mixer at its defaults (channel flat, pan centred, AMT 0): the sound untouched, sample for sample",
          !memcmp(l, l0, sizeof l0) && !memcmp(r, l0, sizeof l0));
    ch[CH_LOW] = 12;
    a = ch_db(60, 8000);
    c = ch_db(8000, 8000);
    snprintf(b, sizeof b, "LOW +12: 60 Hz up %.1f dB, 8 kHz %.1f dB", a, c);
    check(b, a > 10 && a < 12.5 && fabs(c) < 1);
    ch[CH_LOW] = 0;
    ch[CH_HIGH] = -12;
    a = ch_db(60, 8000);
    c = ch_db(8000, 8000);
    snprintf(b, sizeof b, "HIGH -12: 8 kHz down %.1f dB, 60 Hz %.1f dB", c, a);
    check(b, c < -10 && c > -12.5 && fabs(a) < 1);
    ch[CH_HIGH] = 0;
    ch[CH_FILT] = -100;
    a = ch_db(50, 8000);
    c = ch_db(1000, 8000);
    snprintf(b, sizeof b, "FILT -100 (low-pass at 92 Hz): 50 Hz %.1f dB, 1 kHz %.1f dB", a, c);
    check(b, fabs(a) < 2 && c < -25);
    ch[CH_FILT] = 100;
    a = ch_db(1000, 8000);
    c = ch_db(10000, 8000);
    snprintf(b, sizeof b, "FILT +100 (high-pass at 4.5 kHz): 1 kHz %.1f dB, 10 kHz %.1f dB", a, c);
    check(b, a < -20 && fabs(c) < 2);
    ch[CH_FILT] = -50;
    a = ch_db(200, 8000);
    c = ch_db(4000, 8000);
    d = ch_db(970, 8000);
    snprintf(b, sizeof b, "FILT -50 (corner ~970 Hz): 200 Hz %.1f, at the corner %.1f, 4 kHz %.1f dB (12 dB an octave)",
             a, d, c);
    check(b, fabs(a) < 1 && d < -1.5 && d > -4.5 && c < -20 && c > -30);
    ch[CH_FILT] = 0;

    for (i = 0; i < 3u; i++) {                           /* the pan: -100, 0, +100 */
        int32_t pan = (int32_t)i * 100 - 100;
        ch[CH_PAN] = (int16_t)pan;
        sine_fill(l, r, N, 441, 10000);
        memset(&chan[0], 0, sizeof chan[0]);
        mix_pan(0, l, r, 4096, CTL);                           /* (the first block ramps from nothing to the pan) */
        for (k = CTL; k + CTL <= N; k += CTL)
            mix_pan(0, l + k, r + k, 4096, CTL);
        a = rms_of(l, N / 2, N) / (10000 / M_SQRT2);
        c = rms_of(r, N / 2, N) / (10000 / M_SQRT2);
        snprintf(b, sizeof b, "PAN %+d: left x%.2f, right x%.2f", pan, a, c);
        check(b, pan < 0 ? a > 0.99 && c < 0.01 : pan > 0 ? c > 0.99 && a < 0.01 : a > 0.99 && c > 0.99);
    }
    ch[CH_PAN] = -50;
    sine_fill(l, r, N, 441, 10000);
    memset(&chan[0], 0, sizeof chan[0]);
    for (k = 0; k + CTL <= N; k += CTL)
        mix_pan(0, l + k, r + k, 4096, CTL);
    a = rms_of(l, N / 2, N) / (10000 / M_SQRT2);
    c = rms_of(r, N / 2, N) / (10000 / M_SQRT2);
    snprintf(b, sizeof b, "PAN -50: the left at full, the right at x%.2f (sqrt 2 sin 22.5 degrees: 0.54)", c);
    check(b, a > 0.99 && fabs(c - 0.541) < 0.02);
    ch[CH_PAN] = 0;

    {   /* the compressor: a quiet and a loud sine, 20 dB apart, before and after */
        double q, lo;
        mst[MS_AMT] = 100;                               /* -30 dB, 4:1 */
        for (i = 0; i < 2u; i++) {
            double amp = i ? 30000 : 3000;
            sine_fill(l, r, N, 441, amp);
            comp.env = 0;
            comp.g = 4096;
            for (k = 0; k + CTL <= N; k += CTL)
                mix_comp(l + k, r + k, CTL);
            if (i)
                lo = 20 * log10(rms_of(l, N / 2, N) / (amp / M_SQRT2));
            else
                q = 20 * log10(rms_of(l, N / 2, N) / (amp / M_SQRT2));
        }
        snprintf(b, sizeof b, "AMT 100: 20 dB apart in, %.1f dB apart out (4:1 above -30 dB); the quiet one lifted "
                 "%.1f dB", 20 + lo - q, q);
        check(b, 20 + lo - q < 9 && 20 + lo - q > 4 && q > 4);
        check("..the meter shows what it takes off", comp.gr_q8 > 256);
        mst[MS_MIX] = 0;
        sine_fill(l, r, N, 441, 30000);
        memcpy(l0, l, sizeof l0);
        comp.env = 0;
        comp.g = 4096;
        for (k = 0; k + CTL <= N; k += CTL)
            mix_comp(l + k, r + k, CTL);
        a = 20 * log10(rms_of(l, N / 2, N) / rms_of(l0, N / 2, N));
        snprintf(b, sizeof b, "MIX 0: the dry sound back (%.2f dB)", a);
        check(b, fabs(a) < 0.1);
        mst[MS_MIX] = 100;
        for (i = 0; i < 2u; i++) {                       /* ATK: a hit, and how soon it's held down */
            uint32_t at = 0;
            mst[MS_ATK] = (int16_t)(i ? 100 : 1);
            memset(l, 0, sizeof l);
            memset(r, 0, sizeof r);
            sine_fill(l + 1024, r + 1024, N - 1024, 441, 30000);
            comp.env = 0;
            comp.g = mx_curve(0, &k2_dummy);
            for (k = 0; k + CTL <= N; k += CTL) {
                mix_comp(l + k, r + k, CTL);
                if (!at && k > 1024 && comp.gr_q8 > 400)
                    at = k - 1024;
            }
            if (i)
                d = at / 44.1;
            else
                c = at / 44.1;
        }
        snprintf(b, sizeof b, "ATK 1 ms against 100 ms: held down 9 dB after %.1f ms against %.1f ms", c, d);
        check(b, c < 3 && d > 10 && d > 8 * c);
        mst[MS_ATK] = 10;
        mst[MS_AMT] = 0;
        sine_fill(l, r, N, 441, 30000);
        memcpy(l0, l, sizeof l0);
        for (k = 0; k + CTL <= N; k += CTL)
            mix_comp(l + k, r + k, CTL);
        check("AMT back to 0: the gain glides home, then the sound is untouched again",
              comp.g == 4096 && !memcmp(l + N / 2, l0 + N / 2, N / 2 * sizeof l[0]));
    }
    for (i = 0; i < NCH; i++)                            /* every corner, a square past full scale: bounded */
        ch[i] = CH_P[i].max;
    for (k = 0; k < N; k++)
        l[k] = r[k] = (k / 50) & 1 ? 131071 : -131071;
    memset(&chan[0], 0, sizeof chan[0]);
    for (k = 0; k + CTL <= N; k += CTL) {
        mix_channel(0, l + k, r + k, CTL);
        mix_pan(0, l + k, r + k, 4096, CTL);
    }
    check("every channel knob at its top, a square past full scale: bounded", peak_of(l, N) < 1000000 &&
          peak_of(r, N) < 1000000);
    for (i = 0; i < NCH; i++)
        ch[i] = CH_P[i].def;
}

/* the tape's edges: an empty user reel, a loop shorter than a block, the seam running backwards */
static void test_tape_edges(void)
{
    tape_view_t v;
    tape_rd_t r = {0};
    int32_t ls, ll, lo, hi, inside = 1;
    uint32_t i, len, t;
    char b[120];
    power_on();
    check("GAIN in dB: +6 doubles, -12 quarters, 0 is unity", db_q10(6) == 2048 && db_q10(-12) == 256 &&
          db_q10(0) == 1024);
    tp[2].dev[DEV_SRC][TK_REEL] = (int16_t)(NREEL + USLOT_N);   /* user reel 6: empty */
    tape_view(2, &v);
    check("a track on an empty user reel plays nothing", v.len == 0 && !v.ram && tape_peak_at(2, 500) == 0);
    tape_ctl[2].nblk = 0;
    check("..REC on it records onto a blank tape: none long yet, two chunks ready, growing", tape_prepare(2) == 1 &&
          tape_src(2) == 0 && tape_ctl[2].nblk == 0 && tape_ctl[2].nch == 2 && tape_ctl[2].grow &&
          !tape_ctl[2].empty && tpk(2, 20) == 0);
    check("..armed twice: still armed, nothing copied again", tape_prepare(2));
    tape_unprepare(2);
    tape_view(0, &v);
    check("a read past the end of the tape is silence", tape_read(&v, &r, (int32_t)(v.len + 5u) << 12) == 0 &&
          tape_at(&v, &r, -1) == 0);
    tape_rt[2].wstaged = 1;                              /* a block staged as the tape is cleared: dropped */
    tape_ctl[2].empty = 1;
    i = tape_ver[2];
    tape_commit(2);
    check("a staged block on a tape cleared meanwhile is dropped", !tape_rt[2].wstaged && tape_ver[2] == i);
    tape_ctl[2].empty = 0;

    len = REELS[0].nblk * TAPE_BLK;
    tp[0].dev[DEV_SRC][TK_STRT] = 10000;                   /* STRT at the end, LEN 1 %: one block, the last */
    tp[0].dev[DEV_SRC][TK_LEN] = 100;
    tape_window(0, 10000, &ls, &ll);                     /* (1 % of 10,000 samples: 100, under a block) */
    check("LEN under a block is a block; STRT at the end keeps the window on the tape",
          ll == (int32_t)TAPE_BLK && ls == 10000 - (int32_t)TAPE_BLK);
    tp[0].dev[DEV_SRC][TK_REV] = 1;                      /* backwards over a short loop: the seam every 12 ms */
    tp[0].dev[DEV_SRC][TK_STRT] = 3000;
    tp[0].dev[DEV_SRC][TK_LEN] = 200;
    tape_window(0, len, &lo, &ll);
    hi = lo + ll;
    sys.playing = 1;
    for (i = 0; i < 400u; i++) {
        render(1, 0);
        inside &= (tape_rt[0].pos >> 12) >= lo && (tape_rt[0].pos >> 12) < hi;
    }
    check("REV over a 2 % loop: the head wraps back to the end, never leaves the window", inside &&
          tape_rt[0].running);
    sys.playing = 0;
    render(400, 0);
    tp[0].dev[DEV_SRC][TK_REV] = 0;
    tp[0].dev[DEV_SRC][TK_STRT] = 0;
    tp[0].dev[DEV_SRC][TK_LEN] = 10000;
    tp[2].dev[DEV_SRC][TK_REEL] = 3;

    tp[0].dev[DEV_SRC][TK_ROTA] = 50;                    /* ROTATE: playing starts half way round the loop */
    sys.playing = 1;
    render(1, 0);
    check("ROTA 50: PLAY starts the head half way through the loop", abs((tape_rt[0].pos >> 12) - (int32_t)len / 2) < 64);
    sys.playing = 0;
    render(400, 0);
    tp[0].dev[DEV_SRC][TK_ROTA] = 97;                    /* slice 1 from 97 %: it runs over the seam */
    fm1_in.notes = note_bit_of_white(0);
    render(1, 0);
    fm1_in.notes = 0;
    {
        int32_t ran = 0, s0 = (tape_rt[0].pos >> 12);
        for (i = 0; i < 2000u && tape_rt[0].running; i++, ran++)
            render(1, 0);
        check("ROTA 97, stopped: slice 1 starts at 97 %, runs over the seam and stops after its length",
              abs(s0 - (int32_t)len * 97 / 100) < 64 && abs(ran - (int32_t)len / 16 * 2 / CTL) <= 2);
    }
    tp[0].dev[DEV_SRC][TK_ROTA] = 0;

    tp[0].dev[DEV_SRC][TK_STRT] = 4000;                    /* STRT cuts the start away, LEN 100: the rest of the tape */
    tape_window(0, len, &ls, &ll);
    check("STRT 40 at LEN 100: the loop is 40 % to the end (it used to slide back to 0)",
          ls == (int32_t)len * 40 / 100 && ls + ll == (int32_t)len);
    tp[0].dev[DEV_SRC][TK_LEN] = 3000;
    tape_window(0, len, &ls, &ll);
    check("..LEN 30 from there: 40 % to 70 %", ls == (int32_t)len * 40 / 100 && ll == (int32_t)len * 30 / 100);
    tp[0].dev[DEV_SRC][TK_STRT] = 9000;
    tape_window(0, len, &ls, &ll);
    check("..STRT 90, LEN 30: trimmed at the end (90 % to the end), not slid back", ls == (int32_t)len * 90 / 100 &&
          ls + ll == (int32_t)len);
    tp[0].dev[DEV_SRC][TK_STRT] = 0;
    tp[0].dev[DEV_SRC][TK_LEN] = 10000;
    sys.playing = 1;                                     /* ROTATE turned while it plays: the loop turns under it */
    render(200, 0);
    {
        int32_t p0 = tape_rt[0].pos >> 12, p1, want;
        tp[0].dev[DEV_SRC][TK_ROTA] = 25;
        render(1, 0);
        p1 = tape_rt[0].pos >> 12;
        want = (p0 + (int32_t)len / 4 + CTL / 2) % (int32_t)len;
        check("ROTA turned 0 -> 25 while playing: the head jumps a quarter of the loop on (crossfaded), so you hear it",
              abs(p1 - want) < 32 && tape_rt[0].xf > 0);
        render(10, 0);
        p0 = tape_rt[0].pos >> 12;
        render(1, 0);
        check("..and then plays on from there", abs((tape_rt[0].pos >> 12) - p0 - CTL / 2) <= 1);
        render(30, 0);                                   /* (ROTA's crossfade done) */
        p0 = tape_rt[0].pos >> 12;                       /* STRT moved just past the head (as an LFO would): it plays */
        tp[0].dev[DEV_SRC][TK_STRT] = (int16_t)((p0 + 200) * 10000 / (int32_t)len);   /* .. on into the loop */
        render(1, 0);
        check("..STRT moved a little past the head: no jump, it plays on into the loop (modulated STRT doesn't step)",
              tape_rt[0].xf == 0 && abs((tape_rt[0].pos >> 12) - p0 - CTL / 2) <= 1);
        tp[0].dev[DEV_SRC][TK_STRT] = (int16_t)((p0 + (int32_t)len / 8) * 10000 / (int32_t)len);
        render(1, 0);
        check("..moved far past it: the head jumps into the loop (crossfaded)", tape_rt[0].xf > 0 &&
              (tape_rt[0].pos >> 12) >= p0 + (int32_t)len / 8 - 2);
        tp[0].dev[DEV_SRC][TK_STRT] = 0;
    }
    sys.playing = 0;
    render(400, 0);
    tp[0].dev[DEV_SRC][TK_ROTA] = 0;
    render(1, 0);

    {   /* DUB's balance: +100 keeps the loop untouched, -50 halves it each pass under silence */
        uint32_t k, same = 1;
        int32_t before = 0, after = 0;
        for (t = 0; t < NTRK; t++)
            track[t].mute = t != 0;                       /* (only track 1 sounds into track 2) */
        check("REC armed on track 2 (its reel copied in)", tape_prepare(1) && tape_ctl[1].rec_ok);
        tp[1].dev[DEV_SRC][TK_DUB] = 100;
        sys.rec = 1u << 1;
        sys.playing = 1;
        render(2800, 0);                                 /* a whole pass and more */
        sys.rec = 0;
        render(2, 0);
        for (k = 0; k < REELS[1].nblk; k++)
            same &= tpk(1, k) == REELS[1].peak[k] && !memcmp(tdata(1, k), REELS[1].data + k * (TAPE_BLK / 2u), TAPE_BLK / 2u);
        check("DUB +100: REC changes nothing (the loop untouched, bit for bit)", same);
        track[0].mute = 1;                               /* silence in */
        tp[1].dev[DEV_SRC][TK_DUB] = -50;
        for (k = 0; k < REELS[1].nblk; k++)
            before += REELS[1].peak[k];
        sys.rec = 1u << 1;
        render(1378 * 2 + 200, 0);                       /* one pass of the 2 s loop (and a little) */
        sys.rec = 0;
        render(2, 0);
        for (k = 0; k < REELS[1].nblk; k++)
            after += tpk(1, k);
        snprintf(b, sizeof b, "DUB -50 under silence: the loop at half its level after a pass (%d%%)",
                 (int)(after * 100 / (before ? before : 1)));
        check(b, after * 100 / before >= 40 && after * 100 / before <= 60);
        sys.playing = 0;
        tape_unprepare(1);
        for (t = 0; t < NTRK; t++)
            track[t].mute = 0;
        tp[1].dev[DEV_SRC][TK_DUB] = 0;
        render(400, 0);
    }
}

/* the LEDs, OCT-, the messages, panic */
static uint32_t led_lit;
static void led_on(const uint8_t *a, uint32_t id)
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] == (int8_t)id)
                return (void)(led_lit = (a[p] >> r) & 1u);
    led_lit = 0;
}
static void test_controls_more(void)
{
    uint32_t oct;
    power_on();
    sys.sel = 0;
    track[1].mute = 1;
    ui_leds();
    led_on(fm1_led, panel.btn[B_HOME]);
    {
        uint32_t home = led_lit, mute2, mute1, glow;
        led_on(fm1_led, 14u + black_note(BK_OP1 + 1));
        mute2 = led_lit;
        led_on(fm1_led, 14u + black_note(BK_OP1));
        mute1 = led_lit;
        led_on(fm1_led_dim, panel.btn[B_FX]);
        glow = led_lit;
        check("LEDs: the focus (HOME on TAPE) lit, track 2's mute key lit, track 1's not, the rest glow (DIM)",
              home && mute2 && !mute1 && glow);
    }
    settings_leds = LEDS_INV;
    sys.playing = 1;
    ui_leds();
    led_on(fm1_led, panel.btn[B_HOME]);
    {
        uint32_t home = led_lit, fx, play;
        led_on(fm1_led, panel.btn[B_FX]);
        fx = led_lit;
        led_on(fm1_led, panel.btn[B_PLAY]);
        play = led_lit;
        check("..INV: the focus dark, the rest lit, no glow; playing: PLAY's own LED dark (its green on)",
              !home && fx && !play && !fm1_led_dim[0] &&
              ((fm1_led[LED_PLAY_GREEN >> 3] >> (LED_PLAY_GREEN & 7u)) & 1u));
    }
    settings_leds = LEDS_DIM;
    sys.playing = 0;
    track[1].mute = 0;
    press(B_LFO);
    ui_leds();
    led_on(fm1_led, panel.btn[B_LFO]);
    check("..a modulator slot focused: its pad lit", led_lit);
    press(B_HOME);

    oct = track[0].octave;
    press(B_OCTDN);
    check("OCT-: one octave down", track[0].octave == oct - 1u);
    track[0].octave = 1;
    press(B_OCTDN);
    check("..not below 1", track[0].octave == 1);
    track[0].octave = (uint8_t)oct;

    tape_ctl[0].nblk = 10;                               /* a take on track 1's tape, a reel chosen over it */
    tape_ctl[0].empty = 0;
    tp[0].dev[DEV_SRC][TK_REEL] = 2;
    ui.msg_t = 0;
    press(B_REC);
    check("REC on a reel over a take: refused, and the message says how", !(sys.rec & 1u) && ui.msg_t &&
          !strcmp(ui.msg, "TAPE HAS A TAKE: CLEAR IT (HOLD POLY)"));
    tp[0].dev[DEV_SRC][TK_REEL] = 1;
    tape_ctl[0].nblk = 0;

    host_enc[panel.enc[EN_PRESET]] = 1;
    ui_input();
    check("PRESETS turned: the project view, pointing at the project loaded last (P1)", ui.view == VIEW_PROJECT &&
          ui.pj_sel == pj_cur);
    host_enc[panel.enc[EN_ALGO]] = 1;
    ui_input();
    check("ALGORITHM turned: the routing view (REC IN), nothing changed yet", ui.view == VIEW_ROUTE &&
          tp[sys.sel].recin == RIN_AUTO);
    host_enc[panel.enc[EN_ALGO]] = 2;
    ui_input();
    check("..turned again: the focused track's REC IN (AUTO -> OTHR -> its own: SELF)", tp[sys.sel].recin == 2 &&
          !strcmp(N_RIN[sys.sel][tp[sys.sel].recin], "SELF"));
    turn(2, 3);
    check("..KNOB 3: track 3's REC IN (T2)", tp[2].recin == 3 && !strcmp(N_RIN[2][3], "T2"));
    turn(2, 40);
    check("..to T4 at most", tp[2].recin == NRIN - 1);
    press(B_HOME);
    check("..a page pad closes it", ui.view == VIEW_PAGE);
    tp[sys.sel].recin = tp[2].recin = RIN_AUTO;

    hold(B_GLO);                                         /* GLOBAL held, EDIT: the channel page (GLOBAL used) */
    hold(B_EDIT);
    check("on the mixer, GLOBAL held + EDIT: its first page (SCENES), GLOBAL counted as used", ui.view == VIEW_MIXER &&
          ui.chan && ui.glo_used);
    let_go(B_EDIT);
    let_go(B_GLO);
    check("..both let go: back to the page", ui.view == VIEW_PAGE && !ui.chan);

    track[0].mute = 1;
    ui.force = 1;
    ui_draw();
    check("the footer says MUTE for a muted track", ui.sig_foot != 0);
    track[0].mute = 0;
    ui.force = 0;
    ui_redraw();
    check("ui_redraw asks for a whole redraw", ui.force);
    sys.playing = 1;
    sys.keys_live = 3;
    chain_panic();
    check("panic: the transport stops, the keys let go", !sys.playing && !sys.keys_live);
}

/* the drive's less travelled paths */
static void test_drive_more(void)
{
    static uint8_t buf[400000], wav[400000];
    tape_view_t v;
    uint8_t sec[512], *e;
    uint32_t n, i, ok, s;
    int32_t pk;
    double hz;
    char b[120];
    power_on();
    vdisk_mount();
    hd_mount();
    n = hd_read_file(hd_find("REEL2   WAV"), buf, sizeof buf);
    {
        const reel_t *r = &REELS[1];
        tape_view_t rv = {r->data, r->pred, r->idx, r->peak, r->nblk * TAPE_BLK, 0};
        tape_rd_t rd = {0};
        ok = n == 44 + rv.len * 2;
        for (i = 0; i < rv.len && ok; i += 89)
            ok = (int16_t)rd16(buf + 44 + 2 * i) == tape_at(&rv, &rd, (int32_t)i);
    }
    check("REEL2.WAV is factory reel 2, sample for sample", ok);

    memset(sec, 'n', sizeof sec);                        /* a file that isn't a WAV: kept in the write cache */
    hd_write_file(0, "NOTES   TXT", 0, sec, 512, 1);
    e = hd_find("NOTES   TXT");
    memset(buf, 0, 512);
    hd_read_file(e, buf, 512);
    vdisk_read(VD_TOTAL - 1, sec);
    for (i = 0, ok = 1; i < 512; i++)
        ok &= sec[i] == 0;
    check("a sector the computer wrote reads back (the write cache); one nobody wrote reads as zeros",
          !memcmp(buf, "nnnn", 4) && buf[511] == 'n' && ok);

    n = make_wav_x(wav, 11025, 8, 16, 0, 11025, 500.0);  /* 8-bit, 11,025 Hz, after a chunk of odd length */
    hd_write_file(0, "LOFI    WAV", 0, wav, n, 1);
    vdisk_poll();
    uslot_view(0, &v);
    hz = view_hz(&v, &pk);
    snprintf(b, sizeof b, "an 8-bit 11,025 Hz WAV with an odd-length chunk: up to 22,050 Hz, LOFI, %.0f Hz", hz);
    check(b, uslot_valid(0) && !strcmp(uslot_name[0], "LOFI") && fabs(hz - 500) < 10 && pk > 12000 &&
          v.len >= 22050u);

    e = hd_find("NOTES   TXT");                          /* the computer deletes a file: its entry is skipped */
    e[0] = 0xE5;
    n = make_wav_x(wav, 22050, 16, 40, 1, 11025, 700.0); /* WAVE_FORMAT_EXTENSIBLE, PCM, named USER5 */
    hd_write_file(0, "USER5   WAV", 0, wav, n, 1);
    vdisk_poll();
    uslot_view(4, &v);
    hz = view_hz(&v, &pk);
    snprintf(b, sizeof b, "an EXTENSIBLE WAV named USER5.WAV goes to user reel 5 (U5), past a deleted entry: %.0f Hz", hz);
    check(b, uslot_valid(4) && !strcmp(uslot_name[4], "U5") && !uslot_valid(1) && fabs(hz - 700) < 14);

    tape_undo.valid = 1;                                 /* a cleared take on track 3 waiting for undo */
    tape_undo.trk = 2;
    e = hd_find("TAPE3   WAV");
    n = make_wav_x(wav, 22050, 16, 16, 0, 5000, 400.0);
    hd_write_file(e, 0, 0, wav, n, 1);
    vdisk_poll();
    check("a WAV over TAPE3.WAV: track 3's tape, its undo dropped, the reel set to the tape",
          !tape_undo.valid && tape_src(2) == 0 && tape_ctl[2].nblk == (5000 + 255) / 256 &&
          !strcmp(ui.msg, "TRACK 3'S TAPE REPLACED"));

    for (s = 0; s < USLOT_N; s++)                        /* every user reel taken */
        if (!uslot_valid(s))
            uslot_save(s, REELS[0].data, REELS[0].pred, REELS[0].idx, REELS[0].peak, REELS[0].nblk, "FULL");
    n = make_wav_x(wav, 22050, 16, 16, 0, 3000, 400.0);
    hd_write_file(0, "MORE    WAV", 0, wav, n, 1);
    vdisk_poll();
    check("every user reel full: refused, and the message says how to replace one",
          !strcmp(ui.msg, "NO FREE REEL: NAME IT USER1-6.WAV") && !strcmp(uslot_name[1], "FULL"));
    host_prog_limit = 0;
    hd_write_file(0, "USER2   WAV", 0, wav, n, 1);
    vdisk_poll();
    host_prog_limit = 0xFFFFFFFFu;
    check("..named USER2.WAV but the flash refuses: the message says so, the slot reads empty",
          !strcmp(ui.msg, "THE FLASH REFUSED THE SAVE") && !uslot_valid(1) && !strcmp(uslot_name[1], "U2"));

    check("msc.c's view of the disk: 130,048 sectors, ready", msc_blocks() == VD_TOTAL && msc_ready());
    msc_attached();                                      /* a bus reset: plugged in again */
    check("..a bus reset: not ready until the main loop has made the volume afresh", !msc_ready() && vd.remount);
    vdisk_poll();
    msc_read(0, sec);
    hd_mount();
    check("..then ready, with the user reels as files (USER5.WAV), the boot sector through msc_read",
          msc_ready() && !vd.remount && sec[510] == 0x55 && hd_find("USER5   WAV") && !hd_find("LOFI    WAV"));
    memset(sec, 7, sizeof sec);
    msc_write(VD_DATA + 5000, sec);
    memset(sec, 0, sizeof sec);
    msc_read(VD_DATA + 5000, sec);
    msc_eject();
    check("..msc_write then msc_read of a sector: the same bytes", sec[0] == 7 && sec[511] == 7);
    memset(host_nor, 0xFF, sizeof host_nor);
    uslot_names();
}

/* the dot canvas: what's drawn off it is dropped, a fourth colour gets the spare code, and a strip reaches the LCD
 * as 2 x 2 pixel dots */
static void test_canvas(void)
{
    uint32_t x, y, ok = 1;
    px_colors();
    px_begin(8);                                         /* 4 dot rows */
    px_box(-5, -5, 7, 7, px_ink);                        /* corner: (0..1, 0..1) */
    px_box(118, 2, 9, 9, px_dim);                        /* right edge: (118..119, 2..3) */
    px_dot(0, 4, px_ink);                                /* below the strip: dropped */
    px_dot(60, 1, 0x07E0u);                              /* a fourth colour */
    check("the canvas clips at its edges and keeps 4 bits a dot", pxc_h == 4 && px_code(px_ink) == 2u &&
          pxc[1][0] == 0x22u && pxc[3][59] == 0x11u && (pxc[1][30] & 15u) == 3u && pxc_pal[3] == 0x07E0u);
    {
        uint32_t k, ok16 = 1;
        for (k = 0; k < 12u; k++)                        /* 12 more: 16 in all, each its own */
            px_dot((int32_t)k, 3, (uint16_t)(0x1000u + k));
        for (k = 0; k < 12u; k++)
            ok16 &= px_code((uint16_t)(0x1000u + k)) == 4u + k;
        check("..16 colours a strip, each its own code; a 17th falls back to ink",
              ok16 && pxc_n == 16u && px_code(0xBEEFu) == 2u && px_code(0x07E0u) == 3u);
        px_begin(8);
        px_box(-5, -5, 7, 7, px_ink);
        px_box(118, 2, 9, 9, px_dim);
        px_dot(60, 1, 0x07E0u);
    }
    lcd_fill(0, 0, 240, 240, 0);
    px_blit(100);
    for (y = 0; y < 8u; y++)
        for (x = 0; x < 240u; x++) {
            uint16_t c = host_screen[(100u + y) * 240u + x];
            c = (uint16_t)((c >> 8) | (c << 8));
            if (x < 4u && y < 4u)
                ok &= c == px_ink;
            else if (x == 120u && (y == 2u || y == 3u))
                ok &= c == 0x07E0u;
            else if (x >= 236u && y >= 4u)
                ok &= c == px_dim;
        }
    check("..and reaches the screen as 2 x 2 pixel dots, in the right colours, at the strip's row", ok &&
          host_screen[108u * 240u] == 0);
}

/* ------------------------------------------------------------- memory --- */
static void test_memory(void)
{
    char b[120];
    uint32_t t, n0, len;
    power_on();
    check("power-on: every chunk free, no tape holds any", mem_count(MEM_FREE) == MEM_NC && !tape_ctl[0].nch &&
          !tape_ctl[3].nch);

    {   /* a blank tape grows while REC records it, and REC let go sets its length */
        for (t = 0; t < NTRK; t++)
            track[t].mute = t != 0;                      /* (track 1's reel into track 2) */
        tp[1].dev[DEV_SRC][TK_REEL] = 0;
        check("REC on track 2's blank tape: ready, growing", tape_prepare(1) == 1 && tape_ctl[1].grow);
        sys.rec = 1u << 1;
        sys.playing = 1;
        render_poll(2756);                               /* 2 s */
        len = tape_ctl[1].nblk;
        snprintf(b, sizeof b, "..2 s of recording: the tape is 2 s long (%u blocks), two chunks ready past its end",
                 (unsigned)len);
        check(b, len >= 170u && len <= 174u && tape_ctl[1].nch * MEM_CB >= len + 2u * MEM_CB &&
              tape_ctl[1].nch * MEM_CB < len + 3u * MEM_CB);
        sys.rec = 0;
        tape_unprepare(1);
        render_poll(4);
        check("..REC let go: it stops growing, its length stays, the chunks past its end go back",
              !tape_ctl[1].grow && tape_ctl[1].nblk == len && tape_ctl[1].nch == (len + MEM_CB - 1u) / MEM_CB &&
              mem_count(MEM_FREE) == MEM_NC - tape_ctl[1].nch);
        {
            int32_t lo = 1 << 30, hi = -1, k;
            for (k = 0; k < 2756; k++) {
                render(1, 0);
                lo = (tape_rt[1].pos >> 12) < lo ? (tape_rt[1].pos >> 12) : lo;
                hi = (tape_rt[1].pos >> 12) > hi ? (tape_rt[1].pos >> 12) : hi;
            }
            check("..and it loops what it recorded (the head stays inside its length)", lo >= 0 &&
                  hi < (int32_t)(len * TAPE_BLK) && hi > (int32_t)(len * TAPE_BLK) * 9 / 10);
        }
        {
            uint32_t k, sound = 0;
            for (k = 0; k < len; k++)
                sound += tpk(1, k) > 20u;
            snprintf(b, sizeof b, "..what it recorded is track 1's beat (%u of %u blocks loud)", sound, (unsigned)len);
            check(b, sound > len / 3u);
        }
        sys.playing = 0;
        render_poll(400);
        for (t = 0; t < NTRK; t++)
            track[t].mute = 0;
    }

    {   /* nothing free: chunks come from a cleared tape first, then a parked track, then the longest tape */
        power_on();
        fill_tape(0, 40);
        fill_tape(2, 20);
        fill_tape(3, 14);
        fill_tape(1, MEM_NC - 76u);                      /* all but 2 chunks (32 with 108; track 1's 40 the longest) */
        tape_clear(2);                                   /* track 3's take cleared (its undo waiting) */
        n0 = tape_ctl[0].nch;
        check("tape_steal takes a cleared tape's chunk first (not the longest), and its undo goes",
              tape_steal(1, 1) && tape_ctl[2].nch == 19 && tape_ctl[0].nch == n0 && !tape_undo.valid);
        while (tape_ctl[2].nch)
            tape_steal(1, 1);
        sys.ntrk = 3;                                    /* TRACKS 3: track 4 parked */
        check("..then a parked track's (TRACKS 3: track 4)", tape_steal(1, 1) && tape_ctl[3].nch == 13 &&
              tape_ctl[0].nch == n0);
        while (tape_ctl[3].nch)
            tape_steal(1, 1);
        sys.ntrk = NTRK;
        check("..then the end of the longest tape, never the one asking", tape_steal(1, 1) &&
              tape_ctl[0].nch == n0 - 1u && tape_ctl[1].nch == MEM_NC - 76u && tape_ctl[0].nblk == (n0 - 1u) * MEM_CB);
        check("..an import (active 0) never takes from a tape in use", !tape_steal(NTRK, 0));
        tape_free(1);
        while (mem_count(MEM_FREE))
            mem_alloc(MEM_IMPORT);                       /* (nothing free at all; track 2's chunks to someone else) */
        {   /* track 2 records a blank tape with nothing free: it takes from track 1, the longest */
            uint32_t a0 = tape_ctl[0].nch;
            for (t = 0; t < NTRK; t++)
                track[t].mute = t != 0;
            tape_ctl[1].empty = 1;
            check("REC on a blank tape with no chunk free: it takes from the longest tape and grows",
                  tape_prepare(1) == 1 && tape_ctl[1].nch == 2 && tape_ctl[0].nch < a0);
            sys.rec = 1u << 1;
            sys.playing = 1;
            render_poll(1378 * 3);
            snprintf(b, sizeof b, "..3 s on: %u blocks, track 1 down from %u chunks to %u", (unsigned)tape_ctl[1].nblk,
                     (unsigned)a0, (unsigned)tape_ctl[0].nch);
            check(b, tape_ctl[1].nblk > 250u && tape_ctl[0].nch < a0 - 10u && tape_ctl[0].nblk <= tape_ctl[0].nch * MEM_CB);
            sys.rec = 0;
            tape_unprepare(1);
            sys.playing = 0;
            render_poll(400);
            for (t = 0; t < NTRK; t++)
                track[t].mute = 0;
        }
    }

    power_on();
    check("one tape holds 127 chunks at most (23.6 s: its head's Q12 position fits 32 bits), or all there are",
          tape_reserve(0, MEM_NC, 1) == (MEM_NC < TAPE_MAXCH ? MEM_NC : TAPE_MAXCH) &&
          (int64_t)TAPE_MAXCH * TAPE_CHS * 4096 + 8192 < 0x7FFFFFFFLL);

    {   /* a tape that can't grow any more stops at its end and loops */
        power_on();
        for (t = 0; t < MEM_NC - 3u; t++)
            mem_alloc(MEM_IMPORT);                       /* (3 chunks left, nothing to take) */
        tp[1].dev[DEV_SRC][TK_REEL] = 0;
        tape_prepare(1);
        sys.rec = 1u << 1;
        sys.playing = 1;
        render_poll(1378 * 2);
        check("nothing left to take: the tape stops at 3 chunks and loops them, REC still on",
              !tape_ctl[1].grow && tape_ctl[1].nblk == 3u * MEM_CB && tape_ctl[1].rec_ok &&
              (tape_rt[1].pos >> 12) < (int32_t)(3u * TAPE_CHS));
        sys.rec = 0;
        tape_unprepare(1);
        sys.playing = 0;
        render_poll(400);
        tape_free(1);
        while (mem_count(MEM_FREE))
            mem_alloc(MEM_IMPORT);
        tp[2].dev[DEV_SRC][TK_REEL] = 0;
        check("..and REC on a blank tape with no chunk free and no tape to take from: refused (-1)",
              tape_prepare(2) == -1 && !tape_ctl[2].rec_ok);
    }

    {   /* TRACKS: the mixer's SELECT parks the tracks above it */
        int32_t p3;
        power_on();
        sys.playing = 1;
        render(10, 0);
        sys.sel = 3;
        sys.rec = 1u << 3;
        tape_prepare(3);
        tap(B_GLO);                                      /* the mixer, latched: SELECT is still the tempo */
        host_enc[panel.enc[EN_SELECT]] = -1;
        ui_input();
        check("on the latched mixer, SELECT is the tempo as everywhere (TRACKS untouched)", sys.ntrk == 4 &&
              sys.bpm == 119);
        sys.bpm = 120;
        hold(B_GLO);
        host_enc[panel.enc[EN_SELECT]] = -1;
        ui_input();
        check("GLO held + SELECT -1: TRACKS 3; the focus moves off track 4, its REC is let go",
              sys.ntrk == 3 && sys.sel == 2 && !(sys.rec & 8u) && !tape_ctl[3].rec_ok && sys.bpm == 120);
        host_enc[panel.enc[EN_SELECT]] = -5;
        ui_input();
        check("..down to 1 at least", sys.ntrk == 1 && sys.sel == 0);
        host_enc[panel.enc[EN_SELECT]] = 2;
        ui_input();
        check("..and back up: TRACKS 3", sys.ntrk == 3);
        let_go(B_GLO);
        render(4, 0);
        p3 = tape_rt[3].pos;
        render(100, 0);
        check("a parked track goes silent and isn't rendered (its head waits where it was)",
              peak_of(track_rt[3].last, CTL) == 0 && tape_rt[3].pos == p3 && track_rt[3].act == 0);
        hold(B_GLO);
        key_edge(note_bit_of_white(3) ? (uint32_t)__builtin_ctz(note_bit_of_white(3)) : 0u);
        check("GLO + white key 4 doesn't pick a parked track", sys.sel != 3);
        host_enc[panel.enc[EN_SELECT]] = 1;              /* (GLO held: the mixer is up) */
        ui_input();
        let_go(B_GLO);
        render(1, 0);
        check("TRACKS 4 again: track 4 fades back in (2 ms) from where its head was",
              track_rt[3].act > 0 && track_rt[3].act < 32767 && sys.ntrk == 4);
        render(10, 0);
        check("..and plays on", track_rt[3].act == 32767 && tape_rt[3].pos != p3);
        sys.playing = 0;
        render(400, 0);
    }

    {   /* a parked track's grains give their slots up (it isn't rendered, so they'd never end) */
        uint32_t k, t2;
        power_on();
        sys.playing = 1;
        for (t2 = 0; t2 < NTRK; t2++) {
            tp[t2].dev[DEV_GRAIN][GP_WET] = 100;
            tp[t2].dev[DEV_GRAIN][GP_RATE] = 100;
            tp[t2].dev[DEV_GRAIN][GP_SIZE] = 500;
        }
        render_poll(1378 * 2);
        chain_tracks(1);
        render_poll(200);
        for (k = 0, t2 = 0; k < 1378u; k++) {
            render_poll(1);
            t2 = grain_count(0) > t2 ? grain_count(0) : t2;
        }
        check("TRACKS 1 with every track's grains sounding: the parked ones' slots free, track 1 sounds the whole 40",
              grain_count(1) == 0 && grain_count(2) == 0 && grain_count(3) == 0 && t2 == 40u);
        {                                                /* (40 slots: two words of the mask, each slot its own bit) */
            uint32_t w0 = gr_used[0], w1 = gr_used[1], t0 = gr_trk[0][0], t1 = gr_trk[0][1];
            gr_used[0] = 0;
            gr_used[1] = 1u << 7;
            gr_trk[0][0] = 0;                            /* (its track's mask says the same: slot 39 is track 1's) */
            gr_trk[0][1] = 1u << 7;
            check("..the slots past 32 are their own (slot 39 is the second word's bit 7, not slot 7)",
                  gr_on(39) && !gr_on(7) && grain_count(0) + grain_count(1) + grain_count(2) + grain_count(3) == 1u);
            gr_used[0] = w0;
            gr_used[1] = w1;
            gr_trk[0][0] = t0;
            gr_trk[0][1] = t1;
        }
        sys.playing = 0;
        chain_tracks(4);
        render_poll(400);
    }
    {   /* a block staged on a tape whose list has been let go is dropped, not written into the next one */
        uint32_t v0;
        power_on();
        tape_reserve(2, 2, 1);
        tape_ctl[2].nblk = 20;
        tape_rt[2].wblk = 3;
        tape_rt[2].wstaged = 1;
        tape_rt[2].wgen = tape_ctl[2].gen;
        tape_free(2);                                    /* (a WAV over TAPE3.WAV swaps the list in) */
        tape_reserve(2, 2, 1);
        tape_ctl[2].nblk = 20;
        v0 = tape_ver[2];
        tape_commit(2);
        check("a block staged before the tape's list was replaced is dropped", !tape_rt[2].wstaged && tape_ver[2] == v0);
    }

    {   /* REC IN: track 3 records track 1 alone, though track 2 plays too */
        uint32_t k, loud = 0;
        power_on();
        tp[1].dev[DEV_SRC][TK_REEL] = 3;                 /* (track 2 plays reel 3 under it, track 4 is silent) */
        track[3].mute = 1;
        tp[2].dev[DEV_SRC][TK_REEL] = 0;
        tp[2].recin = RIN_T1;
        tape_prepare(2);
        sys.rec = 1u << 2;
        sys.playing = 1;
        render_poll(1378);
        sys.rec = 0;
        tape_unprepare(2);
        render_poll(4);
        for (k = 0; k < tape_ctl[2].nblk && k < REELS[0].nblk; k++)
            loud += abs((int)tpk(2, k) - (int)REELS[0].peak[k] * 100 / 127) < 24;
        snprintf(b, sizeof b, "REC IN T1: track 3's take follows track 1's beat alone (%u of %u blocks)", loud,
                 (unsigned)tape_ctl[2].nblk);
        check(b, tape_ctl[2].nblk > 80u && loud > tape_ctl[2].nblk * 3u / 4u);
        sys.playing = 0;
        render_poll(400);
    }
    {   /* a SYNTH track with REC IN OTHR records the others, not its synth */
        int32_t pk;
        power_on();
        tp[0].src = SRC_SYNTH;
        tp[0].dev[DEV_SRC][TK_REEL] = 0;
        tp[0].recin = RIN_OTHR;
        track[1].mute = track[2].mute = track[3].mute = 1;   /* (the others silent) */
        tape_prepare(0);
        sys.rec = 1u;
        sys.playing = 1;
        fm1_in.notes = note_bit_of_white(0);             /* the synth plays */
        render_poll(400);
        fm1_in.notes = 0;
        sys.rec = 0;
        tape_unprepare(0);
        render_poll(4);
        {
            uint32_t k;
            pk = 0;
            for (k = 0; k < tape_ctl[0].nblk; k++)
                pk = (int32_t)tpk(0, k) > pk ? (int32_t)tpk(0, k) : pk;
        }
        check("a SYNTH track on REC IN OTHR records the others (silent here), not its own synth", tape_ctl[0].nblk > 0 &&
              pk == 0);
        sys.playing = 0;
        render_poll(400);
    }

    {   /* a long sound across reel slots */
        tape_view_t v;
        int32_t pk;
        double hz;
        power_on();
        tape_reserve(0, 44, 1);                          /* 700 blocks of a 440 Hz sine on track 1's tape */
        {
            int16_t blk[TAPE_BLK];
            uint32_t bb, i2;
            for (bb = 0; bb < 700u; bb++) {
                mem_chunk_t *m = tape_chunk(0, bb);
                for (i2 = 0; i2 < TAPE_BLK; i2++)
                    blk[i2] = (int16_t)(12000.0 * sin(2 * M_PI * 440.0 * (bb * TAPE_BLK + i2) / TAPE_SR));
                ima_fit(blk, &m->pred[bb % MEM_CB], &m->idx[bb % MEM_CB]);
                ima_enc(blk, m->pred[bb % MEM_CB], m->idx[bb % MEM_CB], m->data[bb % MEM_CB], TAPE_BLK);
                m->peak[bb % MEM_CB] = ima_peak(blk, TAPE_BLK);
            }
            tape_ctl[0].nblk = 700;
            tp[0].dev[DEV_SRC][TK_REEL] = 0;
        }
        tape_view(0, &v);
        check("a 700-block (8.1 s) sound saved to user reel 1 spans three slots",
              uslot_save_view(0, &v, 700, "LONG") == 700 && uslot_valid(0) && uslot_hdr(0)->span == 3 &&
              uslot_covered(1) && uslot_covered(2) && !uslot_covered(3) && uslot_free() == 3 &&
              !strcmp(uslot_name[0], "LONG"));
        tp[1].dev[DEV_SRC][TK_REEL] = (int16_t)(NREEL + 1u);
        tape_view(1, &v);
        hz = view_hz(&v, &pk);
        snprintf(b, sizeof b, "..and plays from flash whole: %u samples, %.1f Hz", (unsigned)v.len, hz);
        check(b, v.len == 700u * TAPE_BLK && fabs(hz - 440) < 3 && pk > 10000);
        check("..REC copies it onto a tape in one piece (44 chunks)", tape_prepare(1) == 1 && tape_ctl[1].nblk == 700 &&
              tape_ctl[1].nch == 44);
        tape_unprepare(1);
        check("a one-slot reel keeps the layout reels always had (data at 4 KiB, span 1)",
              uslot_doff(TAPE_NBLK) == 4096u && uslot_span(TAPE_NBLK) == 1u && uslot_span(TAPE_NBLK + 1u) == 2u);
        check("..six slots hold 21.5 s at most", uslot_fit(USLOT_N) * TAPE_BLK / 22050u == 21u);
        tape_view(0, &v);
        check("a save into a slot a long reel covers breaks the long one (it reads empty, never garbage)",
              uslot_save(1, REELS[0].data, REELS[0].pred, REELS[0].idx, REELS[0].peak, REELS[0].nblk, "KICK") == 0 &&
              !uslot_valid(0) && uslot_valid(1));
        check("..a sound longer than the slots left from where it goes is cut to fit",
              uslot_save_view(4, &v, 700, "CUT") == (int32_t)uslot_fit(2) && uslot_valid(4) && uslot_hdr(4)->span == 2);
        memset(host_nor, 0xFF, sizeof host_nor);
        uslot_names();
    }

    {   /* the capture only takes chunks the main loop set aside (it runs in usb_poll, TIMER5, which can cut into the
         * main loop while that's allocating): with the main loop held up, a WAV is cut where the spares run out */
        static uint8_t wav[5 * 22050 * 2 + 4096];
        uint32_t n, k;
        power_on();
        vdisk_mount();
        hd_mount();
        vd_spare_fill();
        check("the drive up: two chunks set aside for a WAV", vd_sp_w - vd_sp_r == 2u && mem_count(MEM_SPARE) == 2u);
        n = make_wav(wav, 22050, 1, 16, 1, 22050 * 5, 330.0);
        for (k = 0; k < (n + 511u) / 512u; k++) {        /* (the sectors straight in, no main loop between them) */
            uint8_t sec[512];
            memset(sec, 0, sizeof sec);
            memcpy(sec, wav + k * 512u, n - k * 512u < 512u ? n - k * 512u : 512u);
            vdisk_write(VD_DATA + 40000u + k, sec);
        }
        check("..a 5 s WAV with no main loop between its sectors: cut at the two spares (32 blocks), nothing allocated "
              "from the interrupt", cap.cut && cap.nblk == 32u && capm.nch == 2u && mem_count(MEM_IMPORT) == 2u &&
              mem_count(MEM_FREE) == MEM_NC - 2u);
    }

    {   /* a long WAV over USB: a tape as long as memory allows, a reel across slots */
        static uint8_t wav[10 * 44100 * 2 + 64];
        uint32_t n;
        tape_view_t v;
        int32_t pk;
        double hz;
        power_on();
        vdisk_mount();
        hd_mount();
        n = make_wav(wav, 22050, 1, 16, 1, 22050 * 10, 330.0);   /* 10 s */
        hd_write_file(hd_find("TAPE4   WAV"), 0, 0, wav, n, 1);
        vdisk_poll();
        tape_view(3, &v);
        hz = view_hz(&v, &pk);
        snprintf(b, sizeof b, "a 10 s WAV over TAPE4.WAV: all of it on track 4's tape (%u blocks), %.0f Hz", (unsigned)tape_ctl[3].nblk, hz);
        check(b, tape_ctl[3].nblk >= 861u && tape_ctl[3].nblk <= 862u && fabs(hz - 330) < 4 &&
              mem_count(MEM_TAPE + 3) == tape_ctl[3].nch && !mem_count(MEM_IMPORT));
        n = make_wav(wav, 22050, 1, 16, 1, 22050 * 6, 550.0);    /* 6 s, any name: a reel across slots */
        hd_write_file(0, "SIX     WAV", 0, wav, n, 1);
        vdisk_poll();
        uslot_view(0, &v);
        hz = view_hz(&v, &pk);
        snprintf(b, sizeof b, "a 6 s WAV with any name: user reel SIX across two slots, %.0f Hz, its chunks back", hz);
        check(b, uslot_valid(0) && uslot_hdr(0)->span == 2 && !strcmp(uslot_name[0], "SIX") && fabs(hz - 550) < 5 &&
              !mem_count(MEM_IMPORT) && uslot_free() == 2);
        while (mem_count(MEM_FREE))
            mem_alloc(MEM_TAPE + 2);                     /* (every chunk held by a tape in use, */
        vd_sp_r = vd_sp_w;                               /* and none set aside) */
        n = make_wav(wav, 22050, 1, 16, 1, 22050, 550.0);
        hd_write_file(0, "MORE    WAV", 0, wav, n, 1);
        vdisk_poll();
        check("..no memory free (every chunk in a tape in use): refused, and the message says so",
              !strcmp(ui.msg, "NO MEMORY FREE FOR THAT WAV") && !mem_count(MEM_IMPORT));
        memset(host_nor, 0xFF, sizeof host_nor);
        uslot_names();
    }
}

/* ---------------------------------------------------------- modulation --- */
/* track t's modulated PAN (the ISR's copy) over n blocks: its lowest and highest */
static void pan_range(uint32_t t, uint32_t n, int32_t *lo, int32_t *hi)
{
    uint32_t b;
    *lo = 32767;
    *hi = -32768;
    for (b = 0; b < n; b++) {
        chain_block(out, CTL);
        *lo = TPD(t, MA_CH)[CH_PAN] < *lo ? TPD(t, MA_CH)[CH_PAN] : *lo;
        *hi = TPD(t, MA_CH)[CH_PAN] > *hi ? TPD(t, MA_CH)[CH_PAN] : *hi;
    }
}

static void test_lfo_third(void)
{
    /* SYNC BPM at 1/3 beat (RATE 104..111: 21845, not a multiple of 256): the phase is the exact fraction of the
     * division every block. The old (rem << 8) / (div >> 8) reached 65789 there, and the phase wrapped 0.4 % early
     * every cycle (the beat read after the block, or a block earlier: the clock steps around the tick) */
    power_on();
    sys.playing = 1;
    sys.bpm = 120;
    tp[0].mod[0][0] = 104;
    tp[0].mod[0][12] = 1;
    mod_nudge(0, 0, MOD_TG(DEV_COLOR, 1), 30);
    render(2, 0);
    {
        uint32_t b0, b1, binc, ok = 1, wraps = 0, last = 0, k;
        render(1, 0);
        b0 = mclk.beat;
        render(1, 0);
        b1 = mclk.beat;
        binc = b1 - b0;
        for (k = 0; k < 700u; k++) {                     /* 3 cycles at 120 BPM (230 blocks each) */
            uint32_t ph, rel, e0, e1;
            render(1, 0);
            ph = mrt[0][0].ph >> 16;
            rel = mclk.beat - mrt[0][0].t0;
            e0 = (rel % 21845u) * 65536u / 21845u;
            e1 = ((rel - binc) % 21845u) * 65536u / 21845u;
            ok &= ph == e0 || ph == e1;
            wraps += ph < last;
            last = ph;
        }
        check("mod: LFO SYNC at 1/3 beat: the phase is the division's exact fraction every block (3 cycles, 3 wraps)",
              ok && wraps == 3u);
    }
    mod_clear(0);
    power_on();
}

static void test_mod(void)
{
    const uint32_t PAN = MOD_TCH + CH_PAN;
    int32_t lo, hi;
    power_on();
    render(4, 0);
    check("mod: no depths, every track reads its own knobs (nothing copied)", TPD(0, DEV_GRAIN) == tp[0].dev[DEV_GRAIN] && TPD(3, MA_CH) == tp[3].ch);
    check("mod: the reel choices can't be targets", mod_nudge(0, 0, MOD_TSRC + TK_REEL, 50) == -128 &&
                                                    mod_nudge(0, 0, MOD_TSRC + SRC_POLY * NPK + PL_REEL, 50) == -128);
    check("..nor an empty knob", mod_nudge(0, 0, MOD_TG(DEV_SPACE, 18), 50) == -128);   /* (18: empty; 13-15 are AGE DIFF DUCK since 2026-10-13) */

    /* LFO slot 1 (SIN, FREE): depth 100 swings PAN across its whole range; 50: half of it */
    tp[0].mod[0][0] = 127;                               /* RATE: ~20 Hz, many cycles in a few blocks */
    tp[0].mod[0][12] = 0;                                /* SYNC FREE */
    mod_nudge(0, 0, PAN, 100);
    pan_range(0, 300, &lo, &hi);
    check("mod: LFO at depth 100 sweeps PAN end to end", TPD(0, MA_CH) == mda[0][MA_CH] && lo <= -95 && hi >= 95);
    check("..and leaves the knob itself alone", tp[0].ch[CH_PAN] == 0);
    mod_nudge(0, 0, PAN, -75);                           /* (depth x the range each way: 25 % of 200 is 50) */
    pan_range(0, 300, &lo, &hi);
    check("mod: depth 25: a quarter of the range each way, round the knob", lo >= -51 && lo <= -45 && hi >= 45 && hi <= 51);
    tp[0].ch[CH_PAN] = 80;
    pan_range(0, 300, &lo, &hi);
    check("..clamped at the range's ends", hi == 100 && lo >= 25 && lo <= 32);
    tp[0].ch[CH_PAN] = 0;
    tp[0].mod[0][8] = 0;                                 /* AMT 0: the slot moves nothing */
    pan_range(0, 50, &lo, &hi);
    check("mod: AMT 0 silences the slot", lo == 0 && hi == 0);
    tp[0].mod[0][8] = 100;
    tp[0].mod[0][11] = 100;                              /* SPRD 100: the right channel half a cycle on */
    pan_range(0, 40, &lo, &hi);
    {
        int32_t d = TPD(0, MA_CH)[CH_PAN] + mod_pan_r[0];
        check("mod: SPRD on PAN: the right channel's pan mirrors the left's", hi > 40 && d >= -6 && d <= 6);
    }
    tp[0].mod[0][11] = 0;

    /* SYNC BPM: RATE 80..87 is one beat; at 120 BPM a cycle is 0.5 s (689 blocks): the upward zero crossings */
    {
        uint32_t b, n = 0, first = 0, last = 0;
        int32_t prev = 0;
        tp[0].mod[0][0] = 80;
        tp[0].mod[0][12] = 1;
        sys.bpm = 120;
        for (b = 0; b < 2400u; b++) {
            int32_t v;
            chain_block(out, CTL);
            v = TPD(0, MA_CH)[CH_PAN];
            if (prev < 0 && v >= 0) {
                if (!n)
                    first = b;
                last = b;
                n++;
            }
            prev = v;
        }
        check("mod: LFO SYNC BPM: one cycle a beat at 120 BPM", n >= 3u && (last - first) / (n - 1u) >= 685u &&
                                                               (last - first) / (n - 1u) <= 692u);
    }

    /* ADSR in slot 2 on GRAIN WET (base 0): a key held on the track opens it to SUS, let go it closes */
    power_on();
    {
        uint32_t g = MOD_TG(DEV_GRAIN, GP_WET), kb = note_bit_of_white(0);
        int32_t top = 0;
        uint32_t b;
        tp[0].mod[1][0] = 0;                             /* ATK 1 ms, DEC short, SUS 50, REL short */
        tp[0].mod[1][1] = 10;
        tp[0].mod[1][2] = 50;
        tp[0].mod[1][3] = 10;
        mod_nudge(0, 1, g, 100);
        render(4, 0);
        check("mod: ADSR at rest: WET stays at its knob", TPD(0, DEV_GRAIN)[GP_WET] == 0);
        fm1_in.notes |= kb;
        for (b = 0; b < 3u; b++) {
            chain_block(out, CTL);
            top = TPD(0, DEV_GRAIN)[GP_WET] > top ? TPD(0, DEV_GRAIN)[GP_WET] : top;
        }
        check("mod: a key down: the attack reaches the top", top >= 98);
        render(200, 0);
        check("..held: the sustain level", TPD(0, DEV_GRAIN)[GP_WET] >= 48 && TPD(0, DEV_GRAIN)[GP_WET] <= 51);
        fm1_in.notes &= ~kb;
        render(200, 0);
        check("..let go: released to the knob", TPD(0, DEV_GRAIN)[GP_WET] == 0);
        check("mod: a modulated WET keeps GRAIN's memory in use (the poll asks mod_peak)", mod_peak(0, g) == 100);
        check("..mod_peak_dev (the polls' cached form) says the same, before and after the depths change",
              mod_peak_dev(0, DEV_GRAIN, GP_WET) == mod_peak(0, g) && mod_peak_dev(0, DEV_GRAIN, GP_WET) == 100);
        {
            int32_t w = tp[0].dev[DEV_GRAIN][GP_WET];
            tp[0].dev[DEV_GRAIN][GP_WET] = 0;            /* the knob moves: the knob's part is read live */
            check("..the knob's part live: WET 0 with the depth still there", mod_peak_dev(0, DEV_GRAIN, GP_WET) ==
                  mod_peak(0, g) && mod_peak_dev(0, DEV_GRAIN, GP_WET) > 0);
            tp[0].dev[DEV_GRAIN][GP_WET] = (int16_t)w;
            mod_clear(0);                                /* the depths go: the next ask sees no swing */
            check("..the depths cleared: the knob alone", mod_peak_dev(0, DEV_GRAIN, GP_WET) == w &&
                  mod_peak(0, g) == w);
            mod_nudge(0, 1, g, 100);                     /* (the ADSR's depth back: the loop's start below needs it) */
        }
        sys.playing = 1;                                 /* PLAY: the loop starts, a one-shot */
        top = 0;
        for (b = 0; b < 3u; b++) {
            chain_block(out, CTL);
            top = TPD(0, DEV_GRAIN)[GP_WET] > top ? TPD(0, DEV_GRAIN)[GP_WET] : top;
        }
        render(200, 0);
        check("mod: the loop's start: a one-shot (up, then released without a key)", top >= 98 &&
                                                                                 TPD(0, DEV_GRAIN)[GP_WET] == 0);
        sys.playing = 0;
    }

    /* SEQ in slot 3 on COLOR DRV: LEN 4 at 1/16 (a quarter beat: 172 blocks at 120 BPM), steps 0 100 0 100 */
    power_on();
    {
        uint32_t g = MOD_TG(DEV_COLOR, 0), b, ups = 0;
        int32_t prev = -1;
        tp[0].mod[2][0] = 4;
        tp[0].mod[2][1] = 4;
        tp[0].steps[2][0] = 0;
        tp[0].steps[2][1] = 100;
        tp[0].steps[2][2] = 0;
        tp[0].steps[2][3] = 100;
        mod_nudge(0, 2, g, 100);
        sys.playing = 1;
        for (b = 0; b < 172u * 8u; b++) {
            int32_t v;
            chain_block(out, CTL);
            v = TPD(0, DEV_COLOR)[0];
            if (b == 10u)
                check("mod: SEQ step 1 (0): DRV at its knob", v == 0);
            if (b == 172u + 10u)
                check("..step 2 (100): at the top", v == 100);
            if (prev >= 0 && v > prev)
                ups++;
            prev = v;
        }
        check("..stepping every 1/16: 4 rises in 8 steps", ups == 4u);
        {                                                /* AMT and OFS place it, as every engine's output */
            uint32_t hi = 0, lo = 100;
            tp[0].mod[2][9] = 50;                        /* AMT 50 */
            tp[0].mod[2][10] = 20;                       /* OFS +20 */
            for (b = 0; b < 172u * 4u; b++) {
                chain_block(out, CTL);
                if ((uint32_t)TPD(0, DEV_COLOR)[0] > hi)
                    hi = (uint32_t)TPD(0, DEV_COLOR)[0];
                if ((uint32_t)TPD(0, DEV_COLOR)[0] < lo)
                    lo = (uint32_t)TPD(0, DEV_COLOR)[0];
            }
            check("..AMT 50, OFS +20: the steps 0 and 100 land at 20 and 70", lo == 20u && hi == 70u);
            tp[0].mod[2][9] = 100;
            tp[0].mod[2][10] = 0;
        }
        tp[0].mod[2][6] = 0;                             /* PROB 0: no step plays after the first, it holds */
        sys.playing = 0;
        render(1, 0);
        sys.playing = 1;
        prev = -1;
        ups = 0;
        for (b = 0; b < 172u * 8u; b++) {
            chain_block(out, CTL);
            ups += prev >= 0 && TPD(0, DEV_COLOR)[0] != prev;
            prev = TPD(0, DEV_COLOR)[0];
        }
        check("mod: SEQ PROB 0: the first step holds", ups == 0u);
        sys.playing = 0;
    }

    /* FOLLOW in slot 4 on SPACE TONE: the envelope of track 2's last block, RISE and FALL */
    power_on();
    {
        static int32_t loud[CTL], quiet[CTL];
        const int32_t *last[NTRK] = {quiet, loud, quiet, quiet};
        uint32_t g = MOD_TG(DEV_SPACE, SP_TONE), b, i;
        for (i = 0; i < CTL; i++)
            loud[i] = (i & 1u) ? 30000 : -30000;
        param_engine(0, 3, ME_FOLLOW);
        tp[0].mod[3][0] = 2;                             /* SRC T2 */
        tp[0].mod[3][2] = 0;                             /* RISE fast */
        tp[0].mod[3][3] = 0;                             /* FALL fast */
        mod_nudge(0, 3, g, 100);
        for (b = 0; b < 40u; b++)
            mod_tick(0, 0, last);
        check("mod: FOLLOW: a loud source pushes TONE up", TPD(0, DEV_SPACE)[SP_TONE] >= 75);
        last[1] = quiet;
        for (b = 0; b < 40u; b++)
            mod_tick(0, 0, last);
        check("..silent: back to the knob", TPD(0, DEV_SPACE)[SP_TONE] == tp[0].dev[DEV_SPACE][SP_TONE]);
    }

    /* the gestures: a slot's pad held + a knob sets the depth on the page shown; a tap opens the slot */
    power_on();
    press(B_FX);                                         /* COLOR */
    hold(B_LFO);
    turn(1, 5);                                          /* CRSH */
    check("slot pad held + KNOB 2 on COLOR: slot 1's depth to CRSH", mod_dep(0, 0, MOD_TG(DEV_COLOR, 1)) == 5);
    check("..the strip shows the depth while it's held", ui.slot_held == 0u);
    let_go(B_LFO);
    check("..let go after a turn: still COLOR (no page change)", ui.kind == FOCUS_DEV && ui.dev == DEV_COLOR &&
                                                                 ui.slot_held == SLOT_NONE);
    check("..the knob itself is untouched", tp[0].dev[DEV_COLOR][1] == DEV_P[DEV_COLOR][1].def);
    tap(B_LFO);
    check("slot pad tapped: its page", ui.kind == FOCUS_SLOT && ui.slot == 0u);
    tap(B_LFO);
    check("..again: its next page", ui.kind == FOCUS_SLOT && ui.page == 1u);
    press(B_HOME);
    hold(B_LFO);
    key_edge(black_note(BK_OP2));                        /* (the LFO pad on M2: ENV is the envelope's page now) */
    {
        int32_t d = host_enc[panel.enc[EN_SELECT]] = 1;
        (void)d;
        ui_input();
    }
    check("slot pad held + SELECT: that slot's engine (M2: ADSR -> SEQ, any of four), the page stays",
          tp[0].engine[1] == ME_SEQ && ui.kind == FOCUS_DEV && ui.dev == DEV_SRC);
    turn(2, -10);                                        /* TAPE SPD */
    let_go(B_LFO);
    hold(B_LFO);
    key_edge(black_note(BK_OP1));                        /* (back to M1) */
    let_go(B_LFO);
    press(B_HOME);
    check("..and KNOB 3 on TAPE: slot 2's depth to SPD", mod_dep(0, 1, MOD_TSRC + TK_SPD) == -10);
    check("..depths stay when the engine changes", mod_dep(0, 0, MOD_TG(DEV_COLOR, 1)) == 5);
    hold(B_LFO);                                         /* M1 (one depth, CRSH) to ADSR from another page */
    host_enc[panel.enc[EN_SELECT]] = 1;
    ui_input();
    let_go(B_LFO);
    check("..and the message says so (another page up): M1 IS ADSR, 1 DEPTH KEPT", tp[0].engine[0] == ME_ADSR &&
          !strcmp(ui.msg, "M1 IS ADSR, 1 DEPTH KEPT") && mod_dep(0, 0, MOD_TG(DEV_COLOR, 1)) == 5);
    hold(B_LFO);
    host_enc[panel.enc[EN_SELECT]] = -1;
    ui_input();
    let_go(B_LFO);
    check("..back to the LFO, the depth still there", tp[0].engine[0] == ME_WAVE &&
          !strcmp(ui.msg, "M1 IS LFO, 1 DEPTH KEPT"));
    {                                                    /* M1's dozen: the 13th on a page is refused with a message */
        uint32_t g, n = 1;
        for (g = 0; g < MOD_NTGT && n < MOD_SLOT_MAX; g++)
            if (mod_tdesc(g) && g != MOD_TG(DEV_COLOR, 1) && g != MOD_TSRC + TK_SPD && mod_nudge(0, 0, g, 10) > 0)
                n++;
        focus_dev(DEV_SPACE, 0);                         /* SPACE 1: TIME FDBK (ROUT) DLY VERB */
        hold(B_LFO);
        turn(0, 5);                                      /* TIME (not a depth of M1's) */
        let_go(B_LFO);
        check("slot pad held + a knob with the slot's 12 depths used: refused, 12 DEPTHS ON M1: PAD+MONO CLEARS",
              mod_slot_count(0, 0) == MOD_SLOT_MAX && mod_dep(0, 0, MOD_TG(DEV_SPACE, SP_TIME)) == 0 &&
              !strcmp(ui.msg, "12 DEPTHS ON M1: PAD+MONO CLEARS"));
        hold(B_LFO);                                     /* M2 (the LFO pad on M2): its own dozen, room there */
        key_edge(black_note(BK_OP2));
        turn(0, 5);
        let_go(B_LFO);
        check("..another slot still has room: M2's depth to TIME", mod_dep(0, 1, MOD_TG(DEV_SPACE, SP_TIME)) == 5);
        hold(B_LFO);
        key_edge(black_note(BK_OP1));
        let_go(B_LFO);
        mod_clear(0);
        mod_nudge(0, 0, MOD_TG(DEV_COLOR, 1), 5);
        mod_nudge(0, 1, MOD_TSRC + TK_SPD, -10);         /* (as they were for what follows) */
        press(B_HOME);
    }
    hold(B_LFO);
    turn(0, 3);                                          /* TAPE's REEL is on page 3: page 1 knob 1 is STRT */
    let_go(B_LFO);
    check("hold + turn on the source: the depth for that source's knob", mod_dep(0, 0, MOD_TSRC + TK_STRT) == 3);
    hold(B_GLO);
    press(B_EDIT);                                       /* the channel strip */
    let_go(B_GLO);
    if (ui.view == VIEW_MIXER && ui.chan == CHAN_STRIP) {
        hold(B_SEQ);
        turn(3, 20);
        let_go(B_SEQ);
        check("hold + turn on the channel strip: the depth to PAN", mod_dep(0, 2, PAN) == 20);
    }
    press(B_HOME);

    /* MONO held 0.5 s clears the track's depths; SAVE held brings them back */
    {
        uint32_t mn = black_note(BK_MONO), n0 = mod_count(0);
        fm1_ms = 10000;
        fm1_in.notes |= 1u << mn;
        key_edge(mn);
        fm1_ms = 10200;
        fm1_in.notes &= ~(1u << mn);
        ui_input();
        check("MONO let go before 0.5 s: nothing cleared", mod_count(0) == n0 && n0 >= 3u);
        fm1_ms = 11000;
        fm1_in.notes |= 1u << mn;
        key_edge(mn);
        fm1_ms = 11510;
        ui_input();
        fm1_in.notes &= ~(1u << mn);
        ui_input();
        render(1, 0);
        check("..held 0.5 s: the track's depths are gone, its knobs read straight", mod_count(0) == 0u && TPD(0, MA_CH) == tp[0].ch);
        fm1_ms = 12000;
        hold(B_SAVE);
        fm1_ms = 12000 + HOLD_MS[settings_hold % 4u];
        ui_input();
        let_go(B_SAVE);
        check("..SAVE held: undone", mod_count(0) == n0);
        check("..once", mod_undo() == -1);
    }

    /* a SEQ page: the white keys pick steps (they don't play), KNOB 1 sets the held ones */
    press(B_SEQ);
    if (tp[0].engine[2] == ME_SEQ) {
        uint32_t k3 = note_bit_of_white(2), k5 = note_bit_of_white(4);
        int8_t s3 = tp[0].steps[2][2], s5 = tp[0].steps[2][4], s1 = tp[0].steps[2][0], len = (int8_t)tp[0].mod[2][0];
        fm1_in.notes |= k3 | k5;
        ui_input();
        check("SEQ page: the keys don't play the track", !sys.keys_live && ui.steps_held == ((1u << 2) | (1u << 4)));
        turn(0, -7);
        check("..a white key held + KNOB 1: those steps' values", tp[0].steps[2][2] == s3 - 7 && tp[0].steps[2][4] == s5 - 7 &&
                                                                  tp[0].steps[2][0] == s1 && tp[0].mod[2][0] == len);
        fm1_in.notes &= ~(k3 | k5);
        ui_input();
        turn(0, -1);
        check("..no key: KNOB 1 is LEN again", tp[0].mod[2][0] == len - 1);
    }

    /* an ADSR's loop restart follows the track's own loop: on DRUM the pattern's (3 bars here), not SYNTH's 4 bars */
    power_on();
    {
        uint32_t b, n = 0, at[4] = {0}, was = ENV_IDLE;
        tp[0].src = SRC_DRUM;
        tp[0].drm[DM_LEN] = 3;
        memset(tp[0].dpat, 0, sizeof tp[0].dpat);       /* (no hits: they'd trigger it too, below) */
        sys.bpm = 120;
        mod_nudge(0, 1, MOD_TSRC + SRC_DRUM * NPK + DM_TONE, 50);   /* slot 2 (ADSR) on DRUM's TONE */
        sys.playing = 1;
        for (b = 0; b < 12000u; b++) {                   /* (a bar at 120 BPM: 2756.25 blocks) */
            render(1, 0);
            if (mrt[0][1].stage == ENV_ATK && was != ENV_ATK && n < 4u)
                at[n++] = b;
            was = mrt[0][1].stage;
        }
        sys.playing = 0;
        check("DRUM: an ADSR restarts as the pattern does (LEN 3: every 3 bars, 8,269 blocks), not every 4 bars",
              n == 2u && at[0] <= 2u && at[1] >= 8268u && at[1] <= 8272u);
        render(1, 0);                                    /* (the clock sees the stop) */
        tp[0].src = SRC_SYNTH;                           /* SYNTH: every four bars, as before */
        mod_rebuild(0);
        n = 0;
        was = ENV_IDLE;
        sys.playing = 1;
        for (b = 0; b < 12000u; b++) {
            render(1, 0);
            if (mrt[0][1].stage == ENV_ATK && was != ENV_ATK && n < 4u)
                at[n++] = b;
            was = mrt[0][1].stage;
        }
        sys.playing = 0;
        check("..SYNTH: every four bars (11,025 blocks)", n == 2u && at[0] <= 2u && at[1] >= 11024u && at[1] <= 11027u);
        render(1, 0);
    }

    /* a DRUM pattern's hits are its notes: each step that strikes fires an ADSR (one-shot) and restarts a TRIG KEY LFO */
    power_on();
    {
        uint32_t b, st, want = 0, n = 0, was = ENV_IDLE, lfo0 = 0, lfo_ok = 1;
        tp[0].src = SRC_DRUM;
        drm_load(0, 0);                                  /* ROCK */
        tp[0].drm[DM_FILL] = 0;
        for (st = 0; st < 16u; st++)
            want += tp[0].dpat[0][st] != 0;
        sys.bpm = 120;
        mod_nudge(0, 1, MOD_TSRC + SRC_DRUM * NPK + DM_TONE, 50);   /* slot 2: ADSR */
        tp[0].mod[1][0] = 0;                             /* (ATK and DEC short: each hit a fresh attack) */
        tp[0].mod[1][1] = 0;
        tp[0].mod[1][2] = 0;
        mod_nudge(0, 0, MOD_TSRC + SRC_DRUM * NPK + DM_DRV, 50);    /* slot 1: LFO, TRIG KEY, slow */
        tp[0].mod[0][13] = 1;
        tp[0].mod[0][12] = 0;
        tp[0].mod[0][0] = 0;
        sys.playing = 1;
        for (b = 0; b < 2756u; b++) {                    /* one bar */
            render(1, 0);
            if (mrt[0][1].stage == ENV_ATK && was != ENV_ATK)
                n++;
            was = mrt[0][1].stage;
            if (b == 300u)
                lfo0 = mrt[0][0].ph;
            if (b > 300u && b < 400u && mrt[0][0].ph < lfo0)   /* (step 3's hat at block 344) */
                lfo_ok = 0;
        }
        sys.playing = 0;
        render(1, 0);
        {
            char m[96];
            snprintf(m, sizeof m, "DRUM: an ADSR fires on each step that strikes (ROCK's bar 1: %u of %u)", n, want);
            check(m, n == want && want >= 8u);
        }
        check("..and the LFO (TRIG KEY) restarts with them: its phase falls back at the hat on step 3",
              !lfo_ok);
    }

    /* OP5 and OP6 flip the tape's REV and HALF; on another source they say so and leave the tape alone */
    power_on();
    {
        uint32_t o5 = black_note(BK_OP5);
        tp[0].src = SRC_DRUM;
        key_edge(o5);
        check("OP5 on a DRUM track: the tape's REV untouched, a message says why",
              tp[0].dev[DEV_SRC][TK_REV] == 0 && !strcmp(ui.msg, "REVERSE: TAPE TRACKS ONLY"));
        tp[0].src = SRC_TAPE;
        key_edge(o5);
        check("..on a TAPE track: REV on", tp[0].dev[DEV_SRC][TK_REV] == 1);
    }

    /* the pages' layout: every knob once; a source's LVL on KNOB 4 of its last page, ROUT left of it (or KNOB 4);
     * every modulator's AMT and OFS on KNOB 3 and 4 of its last page */
    {
        static const struct { const pdesc_t *p; const char *a, *b; uint32_t away; const char *f3, *f4; } R[] = {
            /* (a b: KNOB 3-4 of the last page; f3 f4: of the first; away: knobs another pad's page shows) */
            {DEV_P[DEV_SRC], 0, "LVL", 1u << TK_HALF},   /* (HALF: OP6's, past the pages) */ {SYN_CL_P, 0, "LVL"}, {SYN_FM_P, 0, "LVL"}, {SYN_PD_P, 0, "LVL"},
            {SYN_VX_P, 0, "LVL"}, {SYN_WT_P, 0, "LVL"},
            {POL_P, 0, "LVL"},
            {DRM_P, 0, "LVL"},
            {DEV_P[DEV_COLOR], 0, "LVL", 0, "ROUT", "WET"}, {DEV_P[DEV_GRAIN], 0, 0, 0, "ROUT", "WET"},
            {DEV_P[DEV_RESO], 0, 0, 0x1F000u, "ROUT", "WET"},   /* (12-15: RIPL ARP, the ARP pad's page 3; 16:
                                                                 * the form, EDIT held + SELECT) */
            {RS_FORM_P[0], 0, 0, 0x1F000u, "ROUT", "WET"}, {RS_FORM_P[1], 0, 0, 0x1F000u, "ROUT", "WET"},
            {RS_FORM_P[2], 0, 0, 0x1F000u, "ROUT", "WET"},
            {ME_P[ME_WAVE], "AMT", "OFS"}, {ME_P[ME_ADSR], "AMT", "OFS"}, {ME_P[ME_SEQ], "AMT", "OFS"},
            {ME_P[ME_FOLLOW], "AMT", "OFS"}, {DEV_P[DEV_SPACE], 0, 0, 0, "DLY", "VERB"}};
        uint32_t i, k, ok = 1, rule = 1;
        for (i = 0; i < sizeof R / sizeof R[0]; i++) {
            uint32_t n = 4u * pdesc_pages(R[i].p), seen = 0, labelled = 0;
            for (k = 0; k < n; k++) {
                uint32_t at = pdesc_at(R[i].p, k);
                if (at >= NPK || ((seen >> at) & 1u))
                    ok = 0;
                seen |= 1u << at;
            }
            for (k = 0; k < NPK; k++)
                if (!pdesc_empty(&R[i].p[k]))
                    labelled |= 1u << k;
            ok &= (labelled & ~seen & ~R[i].away) == 0u;   /* (every knob that has a label is on a page) */
            ok &= (seen & R[i].away) == 0u;              /* (..and only on one) */
            if (R[i].b)
                rule &= !strcmp(R[i].p[pdesc_at(R[i].p, n - 1u)].label, R[i].b);
            if (R[i].a)
                rule &= !strcmp(R[i].p[pdesc_at(R[i].p, n - 2u)].label, R[i].a);
            if (R[i].f3)
                rule &= !strcmp(R[i].p[pdesc_at(R[i].p, 2u)].label, R[i].f3) &&
                        !strcmp(R[i].p[pdesc_at(R[i].p, 3u)].label, R[i].f4);
        }
        check("layout: every page position is one stored knob, every knob is on a page", ok);
        check("..LVL is KNOB 4 of a source's (and COLOR's) last page; an effect opens with ROUT WET at KNOB 3-4 (SPACE: its "
              "sends, DLY VERB); AMT OFS are KNOB 3-4 of a modulator's last", rule);
    }
    power_on();
    press(B_HOME);
    press(B_HOME);
    press(B_HOME);                                       /* TAPE page 4 (the source shows from power-on) */
    turn(3, -2);
    check("TAPE page 4 (its last), KNOB 4: LVL", ui.page == 3u && tp[0].dev[DEV_SRC][TK_LVL] == -2);
    press(B_FX);                                         /* COLOR page 1 */
    turn(2, 1);
    turn(3, 30);
    check("COLOR page 1: KNOB 3 ROUT, KNOB 4 WET (an effect opens with its mix)",
          tp[0].dev[DEV_COLOR][CP_ROUT] == 1 && tp[0].dev[DEV_COLOR][CP_WET] > 0);
    press(B_FX);
    press(B_FX);                                         /* COLOR page 3 */
    turn(3, -3);
    check("COLOR page 3: KNOB 4 LVL", tp[0].dev[DEV_COLOR][CP_LVL] == -3);
    tp[0].src = SRC_SYNTH;
    press(B_HOME);
    press(B_HOME);                                       /* SYNTH page 2: the envelope (POLY's order) */
    turn(0, 3);
    check("SYNTH page 2: the envelope (ATK first), as POLY's", tp[0].syn[SY_ATK] == SYN_P[SY_ATK].def + 3);
    press(B_HOME);
    turn(0, -2);
    check("..page 3: the filter (CUT)", tp[0].syn[SY_CUT] == SYN_P[SY_CUT].def - 2);

    /* HOME turns the page as it's let go, from any page; held + SELECT picks the source and shows it */
    power_on();
    press(B_FX);                                         /* COLOR */
    hold(B_HOME);
    check("HOME down: nothing yet (the page turns as it's let go)", ui.dev == DEV_COLOR);
    let_go(B_HOME);
    check("..let go: the source", ui.dev == DEV_SRC && ui.page == 0u);
    hold(B_HOME);
    let_go(B_HOME);
    check("..again: its next page", ui.dev == DEV_SRC && ui.page == 1u);
    press(B_FX);
    hold(B_HOME);
    host_enc[panel.enc[EN_SELECT]] = 1;
    ui_input();
    check("HOME held + SELECT on COLOR: the next source, shown on its first page", tp[0].src == SRC_SYNTH &&
                                                                                 ui.dev == DEV_SRC && ui.page == 0u);
    let_go(B_HOME);
    check("..let go after SELECT: the page stays", ui.dev == DEV_SRC && ui.page == 0u && sys.bpm == 120);

    /* OCT on a DRUM track: the bar on DRUM's own pages, the octave elsewhere (RIPPLE's root) */
    power_on();
    tp[0].src = SRC_DRUM;
    press(B_HOME);
    press(B_OCTUP);
    check("OCT+ on a DRUM page: the bar STEP shows", tp[0].drm[DM_BAR] == 2 && track[0].octave == 3u);
    press(B_EDIT);
    press(B_EDIT);
    press(B_EDIT);
    press(B_EDIT);
    press(B_EDIT);
    press(B_EDIT);
    press(B_EDIT);                                       /* RIPPLE (past GRAIN's six) */
    press(B_OCTUP);
    check("..on the track's RIPPLE page: the octave", ui.dev == DEV_RESO && track[0].octave == 4u &&
                                                         tp[0].drm[DM_BAR] == 2);

    /* the catch at 0: a depth turned back through 0 stops there; a turn that keeps going stays until the knob rests */
    power_on();
    press(B_FX);                                         /* COLOR */
    fm1_ms = 20000;
    hold(B_LFO);
    {
        uint32_t g = MOD_TG(DEV_COLOR, 1);
        turn(1, 5);
        turn(1, -8);
        check("catch: a depth of 5 turned down 8 stops at 0 (gone)", mod_dep(0, 0, g) == 0 && mod_count(0) == 0u);
        turn(1, -3);
        fm1_ms += CATCH_MS - 10u;
        turn(1, -2);
        check("..a turn that keeps going stays at 0 (each detent holds the catch)", mod_dep(0, 0, g) == 0);
        fm1_ms += CATCH_MS;
        turn(1, -4);
        check("..the knob rested: the next turn goes past, to -4", mod_dep(0, 0, g) == -4);
        turn(1, 6);
        check("..from below too: -4 up 6 stops at 0", mod_dep(0, 0, g) == 0);
        turn(1, -2);
        check("..turning back the way it came isn't held", mod_dep(0, 0, g) == -2);
        turn(1, 2);
        turn(0, 3);                                      /* another knob: no catch of its own */
        check("..the catch is that knob's alone", mod_dep(0, 0, g) == 0 && mod_dep(0, 0, MOD_TG(DEV_COLOR, 0)) == 3);
        check("mod_nudge: from 0 a turn either way just sets it", mod_nudge(0, 1, g, -7) == -7 && mod_nudge(0, 1, g, 7) == 0
                                                                && mod_nudge(0, 1, g, 7) == 7);
    }
    let_go(B_LFO);
    check("..the pad let go after turns: no page change", ui.kind == FOCUS_DEV && ui.dev == DEV_COLOR);

    /* a slot's pad + MONO held 0.5 s: that slot's depths on the page shown go; other slots' and other pages' stay */
    power_on();
    {
        uint32_t mn = black_note(BK_MONO), cr = MOD_TG(DEV_COLOR, 1), dr = MOD_TG(DEV_COLOR, CP_WET), n0;
        mod_nudge(0, 0, cr, 30);
        mod_nudge(0, 0, dr, -20);
        mod_nudge(0, 1, cr, 40);                         /* slot 2, same page: stays */
        mod_nudge(0, 0, MOD_TG(DEV_COLOR, 4), 25);       /* slot 1, COLOR page 2: stays */
        mod_nudge(0, 0, MOD_TSRC + TK_SPD, 10);          /* slot 1, TAPE: stays */
        mod_nudge(0, 2, PAN, 15);                        /* slot 3, the channel: stays */
        n0 = mod_count(0);
        press(B_FX);                                     /* COLOR page 1 */
        fm1_ms = 30000;
        hold(B_LFO);
        fm1_in.notes |= 1u << mn;
        key_edge(mn);
        fm1_ms = 30200;
        let_go(B_LFO);                                   /* the pad first: called off */
        fm1_ms = 30600;
        ui_input();
        fm1_in.notes &= ~(1u << mn);
        ui_input();
        check("slot pad + MONO: the pad let go first calls it off (no whole-track clear)", mod_count(0) == n0);
        check("..and the pad's let-go didn't open its page", ui.kind == FOCUS_DEV && ui.dev == DEV_COLOR);
        fm1_ms = 31000;
        hold(B_LFO);
        fm1_in.notes |= 1u << mn;
        key_edge(mn);
        fm1_ms = 31510;
        ui_input();
        fm1_in.notes &= ~(1u << mn);
        ui_input();
        let_go(B_LFO);
        check("slot pad + MONO held 0.5 s: slot 1's depths on COLOR page 1 gone", !mod_dep(0, 0, cr) && !mod_dep(0, 0, dr)
                                                                                    && mod_count(0) == n0 - 2u);
        check("..slot 2's on the same knob stay", mod_dep(0, 1, cr) == 40);
        check("..slot 1's on another page, the source and the channel stay", mod_dep(0, 0, MOD_TG(DEV_COLOR, 4)) == 25 &&
                                                                              mod_dep(0, 0, MOD_TSRC + TK_SPD) == 10 &&
                                                                              mod_dep(0, 2, PAN) == 15);
        check("..the pad's let-go stays on COLOR", ui.kind == FOCUS_DEV && ui.dev == DEV_COLOR);
        fm1_ms = 32000;
        hold(B_SAVE);
        fm1_ms = 32000 + HOLD_MS[settings_hold % 4u];
        ui_input();
        let_go(B_SAVE);
        check("..SAVE held: undone, every depth back", mod_count(0) == n0 && mod_dep(0, 0, cr) == 30 && mod_dep(0, 0, dr) == -20);
        {
            uint32_t g1[1] = {cr};
            mod_clear(1);                                /* (track 2 has none: an earlier undo isn't touched) */
            check("mod_clear_slot: nothing to clear leaves the undo alone", mod_clear_slot(0, 3, g1, 1) == 0 &&
                                                                            mod_undo() == -1);
        }
    }

    /* the limits: 12 depths a slot, 32 a track */
    power_on();
    {
        uint32_t g, n = 0, last = 0, rc = 0;
        for (g = 0; g < MOD_NTGT && n < 40u; g++)
            if (mod_tdesc(g)) {
                int32_t v = mod_nudge(0, 0, g, 10);
                if (v == -127)
                    rc++;
                else if (v != -128) {
                    n++;
                    last = g;
                }
            }
        check("mod: at most 12 depths from one slot (the 13th is refused with its own code, the track not full)",
              n == MOD_SLOT_MAX && rc > 0 && mod_slot_count(0, 0) == MOD_SLOT_MAX && mod_count(0) == MOD_SLOT_MAX);
        check("..a depth the slot has can still be turned, and to 0", mod_nudge(0, 0, last, 10) == 20 &&
              mod_nudge(0, 0, last, -30) == 0 && mod_slot_count(0, 0) == MOD_SLOT_MAX - 1u);
        check("..mod_dep_set says -2 for a slot's 13th", mod_dep_set(0, 0, last, 10) == 0 &&
              mod_dep_set(0, 0, last + 1u, 10) == -2 && mod_slot_count(0, 0) == MOD_SLOT_MAX);
        for (g = 0, n = MOD_SLOT_MAX; g < MOD_NTGT && n < 40u; g++)   /* the other slots fill the track */
            if (mod_tdesc(g) && mod_nudge(0, 1u + g % 3u, g, 10) > 0)
                n++;
        check("..at most 32 depths a track (the 33rd is refused)", n == MOD_MAX && mod_count(0) == MOD_MAX &&
              mod_slot_count(0, 1) + mod_slot_count(0, 2) + mod_slot_count(0, 3) == MOD_MAX - MOD_SLOT_MAX);
        render(2, 0);
        check("..and they all run", TPD(0, DEV_GRAIN) == mda[0][DEV_GRAIN] && TPD(0, DEV_RESO) == mda[0][DEV_RESO]);
    }
    power_on();
    {                                                    /* the depths are a short sorted list a track */
        static mod_deps_t a;
        power_on();
        mod_nudge(0, 1, MOD_TG(DEV_COLOR, CP_DRIV), 20);
        mod_nudge(0, 0, MOD_TCH + CH_PAN, -30);
        mod_nudge(0, 3, MOD_TG(DEV_GRAIN, GP_WET), 10);
        a = mdl[0];
        power_on();
        mod_nudge(0, 3, MOD_TG(DEV_GRAIN, GP_WET), 10);
        mod_nudge(0, 0, MOD_TCH + CH_PAN, -30);
        mod_nudge(0, 1, MOD_TG(DEV_COLOR, CP_DRIV), 20);
        check("mod: the depths set in another order are the same list (sorted: projects fingerprint the same)",
              !memcmp(&a, &mdl[0], sizeof a) && mod_count(0) == 3u && mdl[0].e[0].g == MOD_TG(DEV_GRAIN, GP_WET));
        mod_nudge(0, 1, MOD_TG(DEV_COLOR, CP_DRIV), -20);
        check("..a depth turned back to 0 leaves the list", mod_count(0) == 2u && !mod_dep(0, 1, MOD_TG(DEV_COLOR, CP_DRIV)));
        power_on();
    }
}

/* each source's LVL (its last page): -12 dB is a quarter of the sound, on TAPE, SYNTH and POLY alike; a target */
/* the DRUM kit's voices, struck alone: each rings out to silence, under full scale, without DC, and the same every
 * hit; an accent is louder; a closed hat chokes the open one */
static uint32_t drum_hit(uint32_t i, uint32_t acc, uint32_t choke_at, int32_t *out, uint32_t max, int32_t *peak)
{
    dk_knobs_t kn;
    dk_coef_t c;
    dk_voice_t v;
    uint32_t n = 0, k;
    drm_knobs_default(&kn);
    kn.accent = (uint8_t)acc;
    dk_setup(&c, &DRM_KIT[i], &kn);
    memset(&v, 0, sizeof v);
    dk_trigger(&v);
    *peak = 0;
    while (n + CTL <= max) {
        if (n == choke_at)
            dk_choke(&v);
        for (k = 0; k < CTL; k++)                        /* (dk_run adds into its block: the voice alone here) */
            out[n + k] = 0;
        dk_run(&c, &v, out + n, CTL);
        for (k = 0; k < CTL; k++)
            *peak = abs(out[n + k]) > *peak ? abs(out[n + k]) : *peak;
        n += CTL;
        if (!v.live)
            break;
    }
    return v.live ? 0u : n;
}

static void test_drum_voices(void)
{
    static int32_t a[44100 * 12], b[44100 * 12];
    char m[160];
    uint32_t i, rings = 1, quiet = 1, nodc = 1, same = 1, louder = 1, k, n, nb, choked;
    int32_t pk, pkb;
    for (i = 0; i < DRM_NINST; i++) {
        int64_t sum = 0;
        n = drum_hit(i, 0, ~0u, a, 44100u * 12u, &pk);
        nb = drum_hit(i, 0, ~0u, b, 44100u * 12u, &pkb);
        rings &= n != 0;
        quiet &= pk > 4000 && pk < 30000;
        for (k = 0; k < n; k++)
            sum += a[k];
        nodc &= n && llabs(sum / (int64_t)n) < 300;
        same &= n == nb && !memcmp(a, b, n * sizeof *a);
        drum_hit(i, 127, ~0u, b, 44100u * 12u, &pkb);
        louder &= pkb > pk;
        if (!n || !(pk > 4000 && pk < 30000) || !(pkb > pk))
            printf("bryo:   (%s: rang out after %u samples, peak %d, accented %d)\n", DRM_KIT[i].code, n, pk, pkb);
    }
    check("DRUM: each of the 16 instruments rings out to silence within 4 s", rings);
    check("..peaks between -18 and -0.8 dBFS, without DC, the same every hit", quiet && nodc && same);
    check("..and an accent makes every one louder", louder);
    {   /* a hit on a voice still ringing: the old tail fades over one block, the new hit starts the next; it
         * used to restart from rest at the block's edge, the tail jumping to 0 (a click on every repeated kick) */
        dk_knobs_t kn;
        dk_coef_t c;
        dk_voice_t v;
        int32_t step = 0, last = 0, y[CTL];
        uint32_t b, j;
        drm_knobs_default(&kn);
        dk_setup(&c, &DRM_KIT[0], &kn);
        memset(&v, 0, sizeof v);
        dk_trigger(&v);
        for (b = 0; b < 140u; b++) {                     /* 100 ms of the first kick, then the second */
            if (b == 138u)
                dk_trigger(&v);
            for (j = 0; j < CTL; j++)
                y[j] = 0;
            dk_run(&c, &v, y, CTL);
            for (j = 0; j < CTL; j++) {
                if (b >= 137u && abs(y[j] - last) > step)
                    step = abs(y[j] - last);
                last = y[j];
            }
            if (b == 138u)
                check("..struck again while it rings: the block fades the old tail to exactly 0, the voice not yet restarted",
                      y[CTL - 1] == 0 && !v.live && v.trig);
        }
        snprintf(m, sizeof m, "..and the biggest sample-to-sample step round the second strike is %d (under 2500: no click)", step);
        check(m, step < 2500 && v.live);
    }
    n = drum_hit(DI_OH, 0, ~0u, a, 44100u * 12u, &pk);
    choked = drum_hit(DI_OH, 0, CTL * 20u, a, 44100u * 12u, &pk);
    check("the open hat rings past half a second; choked (HH, MB) it stops within 25 ms",
          n > 22050u && choked && choked < CTL * 20u + 1103u &&
          (DRM_KIT[DI_HH].chokes & DRM_KIT[DI_MB].chokes) == 1u << DI_OH);
}

/* DRUM on track 1, the others muted: the instruments it strikes over `blocks` blocks (a count each), and the block
 * of each of its first 64 strikes */
static uint32_t drum_hits[16], drum_at[64], drum_nat;
static void drum_run(uint32_t blocks)
{
    uint32_t b, j;
    for (b = 0; b < blocks; b++) {
        uint32_t n0 = drm_rt[0].n;
        uint8_t before[DRM_NV];
        uint32_t age[DRM_NV];
        for (j = 0; j < DRM_NV; j++) {
            before[j] = drm_rt[0].inst[j];
            age[j] = drm_rt[0].age[j];
        }
        chain_block(out, CTL);
        if (drm_rt[0].n != n0)
            for (j = 0; j < DRM_NV; j++)
                if (drm_rt[0].age[j] != age[j] || drm_rt[0].inst[j] != before[j]) {
                    drum_hits[drm_rt[0].inst[j] & 15u]++;
                    if (drum_nat < 64u)
                        drum_at[drum_nat++] = b;
                }
    }
}
static void drum_reset(void)
{
    memset(drum_hits, 0, sizeof drum_hits);
    drum_nat = 0;
}
/* the hits of a pattern as written, per instrument */
static void drum_count(uint32_t t, uint32_t *n)
{
    uint32_t b, s, i;
    for (i = 0; i < 16u; i++)
        n[i] = 0;
    for (b = 0; b < (uint32_t)tp[t].drm[DM_LEN]; b++)
        for (s = 0; s < 16u; s++)
            for (i = 0; i < 16u; i++)
                n[i] += (tp[t].dpat[b][s] >> i) & 1u;
}

/* hardware notes, 2026-10-11: DRUM's STEP keeps the bar OCT picked while playing; a DRUM track's RIPPLE keys pluck
 * the strings alone */
static void test_drum_play_fixes(void)
{
    uint32_t b, k5 = note_bit_of_white(4);
    int32_t pk;
    power_on();
    tp[0].src = SRC_DRUM;
    track[1].mute = track[2].mute = track[3].mute = 1;
    focus_dev(DEV_SRC, 3);                               /* STEP */
    press(B_OCTUP);
    check("STEP, OCT+: bar 2", tp[0].drm[DM_BAR] == 2);
    sys.bpm = 120;
    sys.playing = 1;
    render(200, 0);                                      /* (bar 1 playing) */
    check("..playing in bar 1, STEP still shows bar 2 (the one the keys write)", vz_drm_bar(tp[0].drm) == 1u &&
                                                                                (drm_now[0] >> 4) == 0u);
    {
        uint16_t was = tp[0].dpat[1][4], was1 = tp[0].dpat[0][4];
        fm1_in.notes = k5;
        key_edge(note_of_white(4));
        fm1_in.notes = 0;
        ui_input();
        check("..a key while playing writes step 5 of bar 2 (what's shown), not bar 1",
              tp[0].dpat[1][4] != was && tp[0].dpat[0][4] == was1);
    }
    focus_dev(DEV_SRC, 0);
    check("..PATTERN follows the playhead (bar 1)", vz_drm_bar(tp[0].drm) == 0u);
    sys.playing = 0;
    render(4, 0);

    /* the RIPPLE page: the keys pluck the strings, the drums stay quiet (WET 0: what comes out is the dry) */
    power_on();
    tp[0].src = SRC_DRUM;
    track[1].mute = track[2].mute = track[3].mute = 1;
    memset(tp[0].dpat, 0, sizeof tp[0].dpat);
    tp[0].dev[DEV_RESO][RP_WET] = 0;
    focus_dev(DEV_RESO, 0);
    ui_input();
    fm1_in.notes = k5;
    key_edge(note_of_white(4));
    for (b = 0, pk = 0; b < 200u; b++) {
        render(1, 0);
        pk = peak_of(track_rt[0].last, CTL) > pk ? peak_of(track_rt[0].last, CTL) : pk;
    }
    fm1_in.notes = 0;
    ui_input();
    render(400, 0);
    check("a DRUM track's RIPPLE page: a key doesn't strike the kit (WET 0: silence)", pk == 0);
    focus_dev(DEV_SRC, 0);
    ui_input();
    fm1_in.notes = k5;
    key_edge(note_of_white(4));
    for (b = 0, pk = 0; b < 200u; b++) {
        render(1, 0);
        pk = peak_of(track_rt[0].last, CTL) > pk ? peak_of(track_rt[0].last, CTL) : pk;
    }
    fm1_in.notes = 0;
    ui_input();
    check("..on its own page the same key plays the drum", pk > 1000);
}

static void test_drum(void)
{
    uint32_t want[16], i, b, s, v, ok, same, kept, foreign, differs;
    int16_t *p;
    char m[96];
    power_on();
    tp[0].src = SRC_DRUM;
    track[1].mute = track[2].mute = track[3].mute = 1;
    p = tp[0].drm;
    check("DRUM: a track starts on ROCK, two bars; PATN, LEN, INST, BAR, MODE aren't modulation targets; SEED is",
          tp[0].drm[DM_PATN] == 0 && tp[0].drm[DM_LEN] == 2 && tp[0].dpat[0][0] == (1u << DI_BD | 1u << DI_HH) &&
          !mod_tdesc(MOD_TSRC + SRC_DRUM * NPK + DM_PATN) && !mod_tdesc(MOD_TSRC + SRC_DRUM * NPK + DM_LEN) &&
          !mod_tdesc(MOD_TSRC + SRC_DRUM * NPK + DM_INST) && !mod_tdesc(MOD_TSRC + SRC_DRUM * NPK + DM_MODE) &&
          mod_tdesc(MOD_TSRC + SRC_DRUM * NPK + DM_SEED) && mod_tdesc(MOD_TSRC + SRC_DRUM * NPK + DM_SWNG));

    /* two bars at 120 BPM: 4 s, 5512.5 blocks; a sixteenth is 172.3 blocks */
    sys.bpm = 120;
    sys.playing = 1;
    drum_reset();
    drum_run(5512);
    drum_count(0, want);
    for (ok = 1, i = 0; i < 16u; i++)
        ok &= drum_hits[i] == want[i];
    snprintf(m, sizeof m, "two bars of ROCK as written: BD %u SD %u HH %u OH %u hits (written %u %u %u %u)",
             drum_hits[DI_BD], drum_hits[DI_SD], drum_hits[DI_HH], drum_hits[DI_OH], want[DI_BD], want[DI_SD],
             want[DI_HH], want[DI_OH]);
    check(m, ok);
    check("..on the grid: the third strike (the hat on step 3) 2 sixteenths after the first, to the block",
          drum_nat > 3u && drum_at[0] == 0u && (drum_at[2] == 344u || drum_at[2] == 345u));
    sys.playing = 0;
    render(2, 0);
    p[DM_SWNG] = 100;                                    /* SWNG 100: the off-sixteenths a third of a step late */
    tp[0].dpat[0][1] = 1u << DI_CL;
    sys.playing = 1;
    drum_reset();
    drum_run(400);
    check("SWNG 100: step 2 (an off-sixteenth) sounds a third of a step late: 229 blocks, not 172",
          drum_nat >= 3u && drum_at[2] >= 228u && drum_at[2] <= 230u);
    sys.playing = 0;
    render(2, 0);
    drm_load(0, 0);
    p[DM_SWNG] = 0;

    /* SEED: versions */
    p[DM_VARY] = 100;
    p[DM_FILL] = 100;
    for (same = 1, kept = 1, foreign = 0, differs = 0, v = 1; v <= 200u; v++)
        for (b = 0; b < 2u; b++)
            for (s = 0; s < 16u; s++) {
                drm_hits_t h, h2, h0;
                uint32_t w = tp[0].dpat[b][s];
                drm_step(0, p, v, b, s, &h);
                drm_step(0, p, v, b, s, &h2);
                drm_step(0, p, 0, b, s, &h0);
                same &= h.hit == h2.hit && h.soft == h2.soft && h.acc == h2.acc && h0.hit == w;
                differs += h.hit != w;
                if (!(b == 1u && s >= 8u))                /* (outside a fill's reach) */
                    kept &= (h.hit & w & (1u << DI_BD | 1u << DI_SD)) == (w & (1u << DI_BD | 1u << DI_SD));
                foreign |= h.hit & ~(1u << DI_BD | 1u << DI_SD | 1u << DI_HH | 1u << DI_OH | 1u << DI_CY);
            }
    check("SEED: a version plays the same every time; SEED 0 is the pattern as written", same);
    snprintf(m, sizeof m, "..200 versions at VARY 100: %u steps differ, the backbone (BD SD) never dropped", differs);
    check(m, differs > 400u && kept);
    check("..and nothing the pattern doesn't use comes in (ROCK: no cowbell, no conga)", !foreign);
    drm_load(0, 4);                                      /* BOSA: no snare, no cymbal */
    for (foreign = 0, v = 1; v <= 200u; v++)
        for (b = 0; b < 2u; b++)
            for (s = 0; s < 16u; s++) {
                drm_hits_t h;
                drm_step(0, p, v, b, s, &h);
                foreign |= h.hit & (1u << DI_SD | 1u << DI_CY | 1u << DI_CP);
            }
    check("..BOSA's fills roll on the rim: no snare, no clap, no crash in 200 versions", !foreign);
    drm_load(0, 5);                                      /* RMBA: low conga and high bongo, no low bongo */
    for (foreign = 0, differs = 0, v = 1; v <= 200u; v++)
        for (b = 0; b < 2u; b++)
            for (s = 0; s < 16u; s++) {
                drm_hits_t h;
                drm_step(0, p, v, b, s, &h);
                foreign |= h.hit & ~(1u << DI_BD | 1u << DI_LC | 1u << DI_HB | 1u << DI_CL | 1u << DI_MA);
                differs += ((h.hit ^ tp[0].dpat[b][s]) & DRM_CONGAS) != 0;
            }
    snprintf(m, sizeof m, "..RMBA: the congas vary (%u steps in 200 versions), runs and moves stay on its two drums",
             differs);
    check(m, !foreign && differs > 200u);
    check("DEC: x2 every 50 % (-100: a quarter, +25: 1.5 times, +100: four times)", drm_scale(-100) == 64u &&
          drm_scale(25) == 384u && drm_scale(100) == 1024u && drm_scale(0) == 256u);
    p[DM_SEED] = 5;
    p[DM_EVOL] = 1;
    check("EVOL 1: a new version every loop (SEED 5: 5, 6, 7); OFF: always 5; SEED 0: as written",
          drm_version(p, 0) == 5u && drm_version(p, 2) == 7u && (p[DM_EVOL] = 0, drm_version(p, 9) == 5u) &&
          (p[DM_SEED] = 0, drm_version(p, 9) == 0u));
    p[DM_VARY] = 40;
    p[DM_FILL] = 25;

    drm_load(0, 0);                                      /* seven instruments on one step: five voices, the oldest */
    for (b = 0; b < 2u; b++)                             /* taken (the last five struck ring) */
        for (s = 0; s < 16u; s++)
            tp[0].dpat[b][s] = 0;
    tp[0].dpat[0][0] = 0x7Fu;
    memset(drm_rt, 0, sizeof drm_rt);
    sys.playing = 1;
    render(1, 0);
    for (ok = 1, i = 0; i < DRM_NV; i++)
        ok &= drm_rt[0].inst[i] >= 2u && drm_rt[0].inst[i] <= 6u && (drm_rt[0].v[i].live || drm_rt[0].v[i].trig);
    check("seven instruments on one step: the five voices hold the last five struck", ok);
    sys.playing = 0;
    render(2, 0);

    /* the page: PATN, POLY's clear, SAVE's undo, STEP's keys, OCT, HOME + key */
    power_on();
    tp[0].src = SRC_DRUM;
    p = tp[0].drm;
    turn(0, 1);
    check("PATN turned: DISC written over the pattern (four on the floor)", tp[0].dpat[0][4] & (1u << DI_BD) &&
          tp[0].dpat[0][2] & (1u << DI_OH) && !strcmp(ui.msg, "DISC WRITTEN"));
    press(B_HOME);
    press(B_HOME);
    press(B_HOME);
    check("HOME tapped: VARY, KIT, STEP (the page turns as HOME is let go)", ui.page == 3u);
    key_edge(note_of_white(1));
    check("STEP (HITS): white key 2 toggles step 2 of bar 1 for INST (BD)", tp[0].dpat[0][1] == (1u << DI_BD));
    hold(B_HOME);
    key_edge(note_of_white(1));
    let_go(B_HOME);
    check("HOME held + white key 2: page 2 (VARY), the page keys' way; the let-go doesn't turn it",
          ui.page == 1u && p[DM_INST] == DI_BD);
    hold(B_HOME);
    key_edge(note_of_white(3));
    let_go(B_HOME);
    turn(0, 1);
    check("..HOME held + key 4: STEP; KNOB 1 (INST) to SD", ui.page == 3u && p[DM_INST] == DI_SD);
    press(B_OCTUP);
    key_edge(note_of_white(2));
    check("OCT+: bar 2; key 3 writes the snare there; OCT+ again stays at LEN", p[DM_BAR] == 2 &&
          (tp[0].dpat[1][2] & (1u << DI_SD)) && (press(B_OCTUP), p[DM_BAR] == 2));
    turn(2, 1);
    key_edge(note_of_white(5));
    check("MODE ACC: key 6 accents step 6 of bar 2", (tp[0].dacc[1] >> 5) & 1u);
    turn(2, -1);
    key_edge(black_note(BK_POLY));
    fm1_in.notes = 1u << black_note(BK_POLY);
    fm1_ms += 600;
    ui_input();
    fm1_in.notes = 0;
    check("POLY held: the pattern cleared", !tp[0].dpat[0][0] && !tp[0].dpat[1][2] && ui.undo == UNDO_DRUM);
    hold(B_SAVE);
    fm1_ms += 1000;
    ui_input();
    let_go(B_SAVE);
    check("SAVE held: it comes back, edits and all", tp[0].dpat[0][1] == (1u << DI_BD) && (tp[0].dpat[1][2] & (1u << DI_SD)));
    press(B_HOME);                                       /* PATTERN: PATN over the edited pattern, SAVE undoes it */
    turn(0, 1);
    check("PATN over an edited pattern: FUNK, and the edit kept for SAVE", tp[0].drm[DM_PATN] == 2 &&
          drm_undo_buf.valid && !strcmp(ui.msg, "FUNK WRITTEN. HOLD SAVE: UNDO"));
    hold(B_SAVE);
    fm1_ms += 1000;
    ui_input();
    let_go(B_SAVE);
    check("..SAVE held: the edited one back", tp[0].dpat[0][1] == (1u << DI_BD));
    tp[0].drm[DM_LEN] = 4;                               /* PATN while STEP shows bar 4: back inside its two bars */
    tp[0].drm[DM_BAR] = 4;
    drm_load(0, 0);
    check("PATN while bar 4 is shown: BAR follows LEN down to 2", tp[0].drm[DM_BAR] == 2);
    for (b = 0; b < DRM_NBAR; b++)
        for (s = 0; s < 16u; s++)
            tp[0].dpat[b][s] = 0;
    fm1_in.notes = 1u << black_note(BK_POLY);
    key_edge(black_note(BK_POLY));
    fm1_ms += 600;
    ui_input();
    fm1_in.notes = 0;
    check("POLY held on an empty pattern: says so; SAVE then has nothing to undo",
          !strcmp(ui.msg, "THE PATTERN IS EMPTY") && drm_undo() == -1);
    fm1_in.notes = note_bit_of_white(4);                 /* an instrument held: its own knobs */
    ui_input();
    turn(2, -24);
    check("PATTERN, the low conga held: KNOB 3 is its own LVL (-24 dB), a modulation target too; the box names it",
          ui.drm_inst == DI_LC + 1u && tp[0].dins[DI_LC][DIN_LVL] == -24 &&
          page_target(2) == MOD_TDIN + DI_LC * NDIN + DIN_LVL);
    fm1_in.notes = 0;
    ui_input();
    check("..let go: the page's own knobs again", !ui.drm_inst && ui_page(2, (int16_t **)&p) == &DRM_P[DM_SWNG]);

    /* LIVE and ERAS, playing */
    power_on();
    tp[0].src = SRC_DRUM;
    track[1].mute = track[2].mute = track[3].mute = 1;
    p = tp[0].drm;
    press(B_HOME);
    press(B_HOME);
    press(B_HOME);
    turn(2, 2);
    sys.bpm = 120;
    sys.playing = 1;
    drum_reset();
    drum_run(172u * 4u + 150u);                          /* just before step 5 (late in step 4) */
    fm1_in.notes = note_bit_of_white(7);                 /* the claves, pressed late: written on step 5 */
    ui_input();
    drum_run(2);
    fm1_in.notes = 0;
    ui_input();
    drm_poll();
    drum_run(100);                                       /* step 5 passes: not struck twice */
    check("LIVE: a key late in step 4 sounds now and is written on step 5, not struck again there",
          (tp[0].dpat[0][5] & (1u << DI_CL)) && drum_hits[DI_CL] == 1u && drm_dirty[0]);
    turn(2, 1);
    fm1_in.notes = note_bit_of_white(12);                /* ERAS: the hat held for a bar */
    ui_input();
    drum_run(172u * 16u);
    fm1_in.notes = 0;
    ui_input();
    drm_poll();
    for (ok = 1, s = 0; s < 16u; s++)
        ok &= !(tp[0].dpat[0][s] & (1u << DI_HH)) || !(tp[0].dpat[1][s] & (1u << DI_HH));
    check("ERAS: the hi-hat held through a bar: wiped where the playhead passed, and silent", ok &&
          (tp[0].dpat[0][0] & (1u << DI_BD)));
    sys.playing = 0;
    render(2, 0);
    power_on();
}

/* ROUT: the three devices with a WET as sends. A run of track 1 (a reel playing, the device set up by `dev`, WET as
 * given, ROUT as given) into o: what the track outputs (track_rt.last), from power-on, `blocks` blocks */
static void send_run(uint32_t dev, int32_t wet, int32_t rout, int32_t *o, uint32_t blocks)
{
    uint32_t b;
    int16_t *v = tp[0].dev[dev];
    power_on();
    track[1].mute = track[2].mute = track[3].mute = 1;
    sys.playing = 1;
    if (dev == DEV_GRAIN) {
        v[GP_WET] = (int16_t)wet;
        v[GP_ROUT] = (int16_t)rout;
    } else if (dev == DEV_RESO) {
        v[RP_WET] = (int16_t)wet;
        v[RP_ROUT] = (int16_t)rout;
    } else {
        v[CP_DRIV] = 50;
        v[CP_WET] = (int16_t)wet;
        v[CP_ROUT] = (int16_t)rout;
    }
    for (b = 0; b < blocks; b++) {
        chain_block(out, CTL);
        chain_poll();
        memcpy(o + b * CTL, track_rt[0].last, CTL * sizeof *o);
    }
}

static void test_sends(void)
{
    static int32_t ins[CTL * 600], snd[CTL * 600], dry[CTL * 600];
    static const uint32_t DEV[3] = {DEV_GRAIN, DEV_RESO, DEV_COLOR};
    static const char *const NAME[3] = {"GRAIN", "RIPPLE", "COLOR"};
    uint32_t k, i, n = CTL * 600u, from = CTL * 200u;
    char m[128];
    check("ROUT (INS, SEND) on GRAIN, RIPPLE and COLOR; not a modulation target; SPACE has none",
          !strcmp(DEV_P[DEV_GRAIN][GP_ROUT].label, "ROUT") && !strcmp(DEV_P[DEV_RESO][RP_ROUT].label, "ROUT") &&
          !strcmp(DEV_P[DEV_COLOR][CP_ROUT].label, "ROUT") && !mod_tdesc(MOD_TG(DEV_GRAIN, GP_ROUT)) &&
          !mod_tdesc(MOD_TG(DEV_RESO, RP_ROUT)) && !mod_tdesc(MOD_TG(DEV_COLOR, CP_ROUT)));
    for (k = 0; k < 3u; k++) {                           /* SEND - INS = the dry, at WET 100 (from 0.15 s on) */
        int32_t err = 0, pd = 0, pe = 0;
        send_run(DEV[k], 100, 0, ins, 600);
        send_run(DEV[k], 100, 1, snd, 600);
        send_run(DEV[k], 0, 0, dry, 600);
        for (i = from; i < n; i++) {
            int32_t e = snd[i] - ins[i] - dry[i];
            err = (e < 0 ? -e : e) > err ? (e < 0 ? -e : e) : err;
            pd = abs(dry[i]) > pd ? abs(dry[i]) : pd;
            pe = abs(ins[i]) > pe ? abs(ins[i]) : pe;
        }
        snprintf(m, sizeof m, "%s at WET 100: SEND is the dry plus what INS plays (off by %d of a dry peak of %d)",
                 NAME[k], err, pd);
        check(m, pd > 1000 && pe > 300 && err <= 8);
    }
    {                                                    /* ROUT flipped under a held note: the dry ramps, no click */
        int32_t jump = 0, base = 0;
        power_on();
        track[1].mute = track[2].mute = track[3].mute = 1;
        tp[0].src = SRC_SYNTH;
        tp[0].syn[SY_WAVE] = 0;
        tp[0].dev[DEV_COLOR][CP_WET] = 100;              /* (COLOR at its defaults: a send doubles the dry) */
        sys.keys_live = 1;
        fm1_in.notes = note_bit_of_white(0);
        render(300, 0);
        for (i = 0; i < 20u; i++) {
            chain_block(out, CTL);
            for (k = 1; k < CTL; k++)
                base = abs(track_rt[0].last[k] - track_rt[0].last[k - 1]) > base ? abs(track_rt[0].last[k] - track_rt[0].last[k - 1]) : base;
        }
        tp[0].dev[DEV_COLOR][CP_ROUT] = 1;
        for (i = 0; i < 4u; i++) {
            int32_t last = track_rt[0].last[CTL - 1];
            chain_block(out, CTL);
            jump = abs(track_rt[0].last[0] - last) > jump ? abs(track_rt[0].last[0] - last) : jump;
            for (k = 1; k < CTL; k++)
                jump = abs(track_rt[0].last[k] - track_rt[0].last[k - 1]) > jump ? abs(track_rt[0].last[k] - track_rt[0].last[k - 1]) : jump;
        }
        fm1_in.notes = 0;
        snprintf(m, sizeof m, "COLOR's ROUT flipped under a held sine: no step (the largest move %d, steady %d)", jump, base);
        check(m, base > 20 && jump < base * 5 / 2);
    }
    power_on();
}

/* projects (project.c) in the host's NOR model: save, power-cycle, everything back; tapes into reels; a power cut
 * mid-save; a Felucca record; the project view's controls */
static void pj_wipe(void)                                /* the project sectors and the reel slots erased */
{
    uint32_t s, k;
    for (s = 0; s < PJ_N; s++) {
        rf_erase(pj_sector(s, 0));
        rf_erase(pj_sector(s, 1));
    }
    for (k = 0; k < USLOT_N * USLOT_SIZE; k += 4096u)
        rf_erase(USLOT_BASE + k);
    uslot_names();
}

static void pj_scene(void)                               /* a session far from the defaults */
{
    uint32_t t;
    sys.bpm = 97;
    chain_tracks(3);
    mst[MS_AMT] = 45;
    for (t = 0; t < NTRK; t++) {
        tp[t].dev[DEV_GRAIN][GP_WET] = (int16_t)(10 + t * 20);
        tp[t].dev[DEV_COLOR][CP_ROUT] = 1;
        tp[t].ch[CH_PAN] = (int16_t)(t * 30 - 40);
        track[t].level = (uint8_t)(70 + t);
        track[t].octave = (uint8_t)(2 + t);
    }
    track[2].mute = 1;
    tp[0].src = SRC_SYNTH;
    tp[0].syn[SY_CUT] = 33;
    tp[1].src = SRC_DRUM;
    drm_load(1, 2);
    tp[1].drm[DM_SEED] = 17;
    tp[1].dpat[1][7] |= 1u << DI_CB;
    tp[1].dacc[0] = 0x0101u;
    tp[1].dins[DI_SD][DIN_TUNE] = 3;
    tp[2].src = SRC_POLY;
    tp[2].pol[PL_TUNE] = -7;
    param_engine(3, 2, ME_SEQ);
    tp[3].steps[2][5] = 77;
    tp[3].recin = RIN_OTHR;
    mod_nudge(1, 0, MOD_TSRC + SRC_DRUM * NPK + DM_SEED, 30);
    mod_nudge(3, 2, MOD_TG(DEV_SPACE, SP_DLY), -25);
    mod_nudge(0, 1, MOD_TCH + CH_FILT, 60);
}

/* saving while playing: the copy a save writes is erased ahead, so the save only programs (no gap in the sound);
 * the next one's erased at once when the save stopped the sound anyway (a take into a reel), else when it's quiet;
 * and the output fades out and in around flash work instead of being cut */
/* the biggest project there can be: every knob of every array on every track random in its range (none at its
 * default, so little packs away), every pattern step and accent, every SEQ step, 32 depths a track. It has to fit
 * its sector and come back as it was (NPK went from 20 to 24 knobs an array for GRAIN's GRNS: 384 bytes more of it
 * unpacked) */
static uint32_t wr_rnd = 7;
static int16_t wr_val(const pdesc_t *d)
{
    wr_rnd = wr_rnd * 1103515245u + 12345u;
    return pdesc_empty(d) ? 0 : (int16_t)(d->min + (int32_t)((wr_rnd >> 8) % (uint32_t)(d->max - d->min + 1)));
}
static void test_project_worst(void)
{
    static track_params_t want[NTRK];
    static mod_deps_t wdep[NTRK];
    static int16_t wrm[NROOM][NRM];
    uint32_t t, s, k, g, n;
    pj_hdr_t h;
    int rc;
    char m[160];
    pj_wipe();
    power_on();
    for (t = 0; t < NTRK; t++) {
        track_params_t *P = &tp[t];
        for (s = 0; s < NDEV; s++)
            for (k = 0; k < NPK; k++)
                P->dev[s][k] = wr_val(&DEV_P[s][k]);
        P->dev[DEV_SRC][TK_REEL] = 1;                    /* (a factory reel: the main loop's choice, kept simple) */
        P->pol[0] = 0;
        for (k = 0; k < NPK; k++) {
            P->syn[k] = wr_val(&SYN_P[k]);
            P->pol[k] = wr_val(&POL_P[k]);
            P->drm[k] = wr_val(&DRM_P[k]);
        }
        P->pol[PL_REEL] = 1;
        for (s = 0; s < NSLOT; s++) {
            param_engine(t, s, s);
            for (k = 0; k < NPK; k++)
                P->mod[s][k] = wr_val(&ME_P[P->engine[s]][k]);
            for (k = 0; k < 16u; k++)
                P->steps[s][k] = (int8_t)(((wr_rnd = wr_rnd * 1103515245u + 12345u) >> 16) % 101u);
        }
        for (k = 0; k < NCH; k++)
            P->ch[k] = wr_val(&CH_P[k]);
        for (s = 0; s < DRM_NBAR; s++) {
            for (k = 0; k < 16u; k++)
                P->dpat[s][k] = (uint16_t)((wr_rnd = wr_rnd * 1103515245u + 12345u) >> 12);
            P->dacc[s] = (uint16_t)((wr_rnd = wr_rnd * 1103515245u + 12345u) >> 12);
        }
        for (s = 0; s < DRM_NINST; s++)
            for (k = 0; k < NDIN; k++)
                P->dins[s][k] = wr_val(&DRI_P[k]);
        for (k = 0; k < NARP; k++)
            P->arp[k] = wr_val(&ARP_P[k]);
        P->src = (uint8_t)(t % NSRC);
        for (k = 0; k < NRM && t < NROOM; k++)           /* (the rooms) */
            rmp[t][k] = wr_val(&RM_P[k]);
        for (g = 0, n = 0; g < MOD_NTGT && n < MOD_MAX; g += 5u)
            if (mod_tdesc(g) && mod_nudge(t, g % NSLOT, g, (int32_t)((wr_rnd = wr_rnd * 1103515245u + 12345u) >> 16) % 199 - 99) != -128)
                n++;
    }
    for (t = 0; t < NTRK; t++)                           /* (SPACE's places for its room's knobs: stored as 0) */
        for (k = 0; k < NPK; k++)
            if (SP_RMK[k])
                tp[t].dev[DEV_SPACE][k] = 0;
    memcpy(want, tp, sizeof tp);
    memcpy(wdep, mdl, sizeof mdl);
    memcpy(wrm, rmp, sizeof rmp);
    rc = pj_save(2);
    memset(&h, 0, sizeof h);
    pj_current(2, &h);
    power_on();
    rc |= pj_load(2);
    snprintf(m, sizeof m, "the biggest project there can be (every knob random): it fits (%u bytes "
             "packed, %u unpacked, a sector holds %u) and comes back as it was", h.len, h.raw, PJ_PAYLOAD_MAX);
    check(m, rc == 0 && mod_count(0) == MOD_MAX && !memcmp(tp, want, sizeof tp) && !memcmp(mdl, wdep, sizeof mdl) &&
          !memcmp(rmp, wrm, sizeof rmp));
    pj_wipe();
    power_on();
}

static int syn_plays(uint32_t t, int32_t note16);

/* KEY: the project's key and scale (param.c key_snap): a note outside it plays the nearest one in it, on SYNTH and
 * POLY, and it's saved with the project. POLY's ROOT (the key that plays the sound as it is) and FINE, SYNTH's FINE */
static void test_key_tuning(void)
{
    enum { NB = 1378 };                                  /* 1 s */
    static int32_t s[NB * CTL];
    int32_t n, k, sc, far = 0, out = 0, i0;
    uint32_t c;
    char m[200];
    double hz0, hz1;
    power_on();
    check("KEY: a new project starts on A MIN, the factory reels' key", keyp[KY_KEY] == 9 && keyp[KY_SCAL] == 2);
    keyp[KY_SCAL] = 0;
    for (n = 0; n < 128; n++)
        out |= key_snap(n) != n;
    check("..SCAL OFF leaves every note as it is", !out);
    keyp[KY_KEY] = 0;
    keyp[KY_SCAL] = 1;                                   /* C MAJ */
    check("..C MAJ: C#3 plays C3, F#3 F3 (between two: the lower), B3 B3, C4 C4", key_snap(49) == 48 &&
          key_snap(54) == 53 && key_snap(59) == 59 && key_snap(60) == 60);
    keyp[KY_KEY] = 9;
    keyp[KY_SCAL] = 10;                                  /* A PEN: A B C# E F# */
    check("..A PEN: C plays B, D C#, G F# (the nearest)", key_snap(60) == 59 && key_snap(62) == 61 &&
          key_snap(67) == 66);
    for (k = 0; k < 12; k++)                             /* every key, every scale, every note */
        for (sc = 1; sc <= NSCALE; sc++) {
            keyp[KY_KEY] = (int16_t)k;
            keyp[KY_SCAL] = (int16_t)sc;
            for (n = 12; n < 116; n++) {
                int32_t q = key_snap(n), d = q - n;
                out |= !((key_mask(sc) >> ((q + 12 - k) % 12)) & 1u);
                far |= d < -1 || d > 1;
            }
        }
    check("..every key and scale: a note snaps into the scale, never more than a semitone away", !out && !far);
    {
        static const uint8_t NOTES[NSCALE] = {7, 7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 6, 6, 8, 12};
        uint32_t ok = 1, c;
        for (sc = 1; sc <= NSCALE; sc++) {
            for (n = 0, c = 0; n < 12; n++)
                c += (key_mask(sc) >> n) & 1u;
            ok &= c == NOTES[sc - 1] && (key_mask(sc) & 1u) && key_mask(sc) < 0x1000u;
        }
        check("..15 scales (7 modes, HMIN MMIN, PEN MPEN, BLUE, WHOL DIM CHR), each with its root and its count of "
              "notes; MAJ 1 and MIN 2 where they were", ok && !strcmp(N_KSCAL[1], "MAJ") && !strcmp(N_KSCAL[2], "MIN") &&
              !strcmp(N_KSCAL[NSCALE], "CHR") && key_mask(0) == 0xFFFu && key_mask(16) == 0xFFFu);
    }

    power_on();                                          /* a new POLY track: BELL, ROOT A3, so a key sounds its note */
    tp[0].src = SRC_POLY;
    fm1_in.notes = note_bit_of_white(0);                 /* C3: BELL (A3) read 9 semitones down */
    render(4, 0);
    i0 = pol[0].v[0].inc;
    fm1_in.notes = 0;
    render(80, 0);
    fm1_in.notes = note_bit_of_white(9);                 /* A3: BELL as it is */
    render(4, 0);
    snprintf(m, sizeof m, "a new POLY track plays BELL with ROOT A3: key C3 reads it at %d (1218: 2048 x 2^(-9/12)), "
             "key A3 at %d (2048, as recorded)", i0, pol[0].v[0].inc);
    check(m, tp[0].pol[PL_REEL] == 1 && tp[0].pol[PL_ROOT] == 57 && i0 >= 1216 && i0 <= 1220 &&
          pol[0].v[0].inc >= 2046 && pol[0].v[0].inc <= 2050);
    fm1_in.notes = 0;
    render(80, 0);
    pj_wipe();                                           /* the factory scene: A MIN */
    keyp[KY_KEY] = 0;
    keyp[KY_SCAL] = 0;
    pj_boot();
    check("..the factory scene is in A MIN (KEY A, SCAL MIN)", keyp[KY_KEY] == 9 && keyp[KY_SCAL] == 2);

    power_on();                                          /* SYNTH: the keys through the scale */
    tp[0].src = SRC_SYNTH;
    keyp[KY_KEY] = 0;
    keyp[KY_SCAL] = 1;                                   /* C MAJ */
    fm1_in.notes = note_bit_of_white(1);                 /* (shortcut keys: white key 2 is C#3) */
    render(4, 0);
    check("SYNTH, C MAJ: white key 2 (C#3) sounds C3", syn_plays(0, 48 * 16) && !syn_plays(0, 49 * 16));
    fm1_in.notes = 0;
    render(80, 0);
    tp[0].src = SRC_POLY;                                /* POLY: the same */
    fm1_in.notes = note_bit_of_white(1);
    render(4, 0);
    check("POLY, C MAJ: white key 2 (C#3) sounds C3", pol[0].v[0].stage && pol[0].v[0].note == 48);
    fm1_in.notes = 0;
    render(80, 0);
    keyp[KY_SCAL] = 0;

    tp[0].pol[PL_ROOT] = 48;                             /* ROOT: C3 plays the sound as it is (0.5 a sample) */
    fm1_in.notes = note_bit_of_white(0);
    render(4, 0);
    i0 = pol[0].v[0].inc;
    tp[0].pol[PL_ROOT] = 60;                             /* the sound is a C4: C3 plays it an octave down */
    render(1, 0);
    c = (uint32_t)pol[0].v[0].inc;
    tp[0].pol[PL_ROOT] = 48;
    tp[0].pol[PL_FINE] = 50;                             /* +50 cents: 2^(50/1200) = 1.0293 */
    render(1, 0);
    snprintf(m, sizeof m, "POLY: ROOT C3 reads the sound at %d (2048, 0.5), ROOT C4 at %u (half), FINE +50 at %d "
             "(x1.0293)", i0, c, pol[0].v[0].inc);
    check(m, i0 >= 2046 && i0 <= 2050 && c * 2u >= (uint32_t)i0 - 2u && c * 2u <= (uint32_t)i0 + 2u &&
          pol[0].v[0].inc * 10000 / i0 >= 10283 && pol[0].v[0].inc * 10000 / i0 <= 10303);
    fm1_in.notes = 0;
    render(80, 0);

    tp[0].src = SRC_SYNTH;                               /* SYNTH's FINE: a sine, A3, then +50 cents */
    tp[0].syn[SY_WAVE] = 0;
    tp[0].syn[SY_DTUN] = 0;
    tp[0].syn[SY_MIX] = 0;
    tp[0].syn[SY_CUT] = 127;
    tp[0].syn[SY_ENV] = 0;
    fm1_in.notes = note_bit_of_white(9);
    render(NB, s);
    hz0 = buf_hz(s, NB * CTL / 4, NB * CTL);
    tp[0].syn[SY_FINE] = 50;
    render(NB, s);
    hz1 = buf_hz(s, NB * CTL / 4, NB * CTL);
    fm1_in.notes = 0;
    snprintf(m, sizeof m, "SYNTH: FINE +50 on A3: %.2f Hz to %.2f Hz (226.45: 50 cents)", hz0, hz1);
    check(m, fabs(hz0 - 220.0) < 1.5 && fabs(hz1 - 226.45) < 1.0);

    power_on();                                          /* the project keeps KEY and SCAL */
    keyp[KY_KEY] = 7;
    keyp[KY_SCAL] = 3;
    n = pj_save(3);
    power_on();
    n |= pj_load(3);
    check("..a project keeps KEY and SCAL (G DOR saved, G DOR back)", n == 0 && keyp[KY_KEY] == 7 &&
          keyp[KY_SCAL] == 3);
    pj_wipe();

    power_on();                                          /* the pages */
    keyp[KY_KEY] = 9;
    keyp[KY_SCAL] = 2;                                   /* A MIN */
    tap(B_GLO);
    tap(B_EDIT);
    tap(B_EDIT);
    tap(B_EDIT);
    tap(B_EDIT);
    ui.msg_t = 0;
    check("the KEY page: GLO, EDIT x4 (SCENES CHANNEL MASTER KEY)", ui.view == VIEW_MIXER && ui.chan == CHAN_KEY);
    shot("BRYO", "mixer_key");
    tap(B_GLO);
    tp[0].src = SRC_POLY;
    tp[0].pol[PL_ROOT] = 48;
    focus_dev(DEV_SRC, 5);                               /* POLY 6: VOIC ROOT FINE LVL */
    turn(1, 9);
    turn(2, 12);
    ui.msg_t = 0;
    snprintf(m, sizeof m, "POLY's page 6: KNOB 2 turns ROOT (C3 -> %d), KNOB 3 FINE (0 -> %d ct)", tp[0].pol[PL_ROOT],
             tp[0].pol[PL_FINE]);
    check(m, tp[0].pol[PL_ROOT] == 57 && tp[0].pol[PL_FINE] > 0);
    shot("BRYO", "poly_tuning");
    tp[0].src = SRC_SYNTH;
    focus_dev(DEV_SRC, 3);                               /* SYNTH 4: VOIC GLID TUNE FINE */
    turn(3, -7);
    ui.msg_t = 0;
    check("SYNTH's page 4: KNOB 4 turns FINE", tp[0].syn[SY_FINE] < 0);
    shot("BRYO", "synth_tuning");
    power_on();
}

static void test_save_seamless(void)
{
    uint32_t n0, b, i, t_;
    int32_t worst = 0, prev = 0, mx = 0;
    power_on();
    pj_wipe();
    pj_boot();
    check("boot: P1's next copy is erased ahead (the audio isn't running yet)", pj_blank(pj_sector(0, 0)) &&
          pj_blank(pj_sector(0, 1)) && pj_pre_off == 0xFFFFFFFFu);
    sys.playing = 1;
    render_poll(200);
    n0 = host_nerase;
    check("a save while playing, nothing new recorded: written, nothing erased (the sound plays on)",
          pj_save(0) == 0 && host_nerase == n0);
    check("..the next save's copy (the other) is still blank from boot: nothing to erase ahead",
          pj_blank(pj_sector(0, 1)) && pj_pre_off == 0xFFFFFFFFu);
    tp[0].dev[DEV_COLOR][0] = 20;                        /* (a change, saved again: into the other copy) */
    render_poll(10);
    n0 = host_nerase;
    check("..saved again while playing: the other copy was blank, still nothing erased", pj_save(0) == 0 &&
          host_nerase == n0);
    for (t_ = 0; t_ < NTRK; t_++)                        /* (playing, everything muted: silent, but a sound could
                                                          * start any step) */
        track[t_].mute = 1;
    for (b = 0; b < 1000u; b++) {                        /* playing: the erase ahead waits */
        render_poll(1);
        fm1_ms += 10;
        pj_poll();
    }
    for (t_ = 0; t_ < NTRK; t_++)
        track[t_].mute = 0;
    check("..now the older copy is the next one: marked to erase ahead, and while it plays, that waits",
          host_nerase == n0 && pj_pre_off == pj_sector(0, 0) && !pj_blank(pj_sector(0, 0)));
    sys.playing = 0;
    for (t_ = 0; t_ < NTRK; t_++)                        /* (stopped and silent) */
        track[t_].mute = 1;
    for (b = 0; b < 1000u; b++) {                       /* (the meters fall to silence, then a second of it) */
        render_poll(1);
        fm1_ms += 5;
        pj_poll();
    }
    check("..stopped and silent a second: the next copy erased then", host_nerase == n0 + 1u &&
          pj_pre_off == 0xFFFFFFFFu);
    for (t_ = 0; t_ < NTRK; t_++)
        track[t_].mute = 0;
    sys.playing = 1;
    fill_tape(1, 4);                                     /* a take: its reel is written (the flash stops the sound) */
    tape_ctl[1].empty = 0;
    n0 = host_nerase;
    pj_save(0);
    check("a save with a new take: the reel erased (the sound stops for it), and the next copy too, in the same gap",
          host_nerase > n0 + 1u && pj_pre_off == 0xFFFFFFFFu);
    tp[0].dev[DEV_COLOR][0] = 30;                        /* (both copies written, then a power cycle) */
    pj_save(0);
    check("..(both copies hold a save now)", !pj_blank(pj_sector(0, 0)) && !pj_blank(pj_sector(0, 1)));
    power_on();
    n0 = host_nerase;
    pj_boot();
    check("boot after saves: the older copy erased then, before the audio runs, the newer loaded",
          host_nerase == n0 + 1u && (pj_blank(pj_sector(0, 0)) != pj_blank(pj_sector(0, 1))) &&
          tp[0].dev[DEV_COLOR][0] == 30);

    power_on();                                          /* the output's fade */
    for (i = 1; i < NTRK; i++)
        track[i].mute = 1;
    sys.playing = 1;
    render_poll(1378);
    out_mute_req = 1;
    {
        uint32_t heard = 0;                              /* (output samples still sounding after the request) */
        for (b = 0; b < 40u; b++) {
            render(1, 0);
            for (i = 0; i < CTL; i++) {
                int32_t d = out[2u * i] - prev;
                d = d < 0 ? -d : d;
                worst = b && d > worst ? d : worst;
                heard += out[2u * i] != 0;
                prev = out[2u * i];
            }
        }
        check("flash work: the output fades to silence over 6 ms (no cut) and says so (out_quiet)", out_g == 0 &&
              out_quiet >= OUT_QUIET_BLOCKS && out[0] == 0 && worst < 2000 && heard > 200u);
    }
    out_mute_req = 0;
    for (b = 0, worst = 0; b < 40u; b++) {
        render(1, 0);
        for (i = 0; i < CTL; i++) {
            int32_t d = out[2u * i] - prev, a = out[2u * i] < 0 ? -out[2u * i] : out[2u * i];
            d = d < 0 ? -d : d;
            worst = d > worst ? d : worst;
            mx = a > mx ? a : mx;
            prev = out[2u * i];
        }
    }
    check("..let go: it fades back in, to the full level", out_g == 32767 && out_quiet == 0 && mx > 1000 &&
          worst < 2000);
    pj_wipe();
    power_on();
}

static void test_projects(void)
{
    static track_params_t want[NTRK];
    static mod_deps_t wdep[NTRK];
    track_ctl_t wtr[NTRK];
    int16_t wmst[NMS];
    pj_hdr_t h;
    uint32_t s, n0, k;
    int rc, same;
    char m[128];
    power_on();
    pj_wipe();
    pj_boot();
    for (same = 1, s = 0; s < PJ_N; s++)
        same &= !pj_sum[s].used;
    check("projects: none saved yet: every slot empty, the factory scene (T1 DRUM, depths on every track), P1 the "
          "current one", same && pj_cur == 0 && tp[0].src == SRC_DRUM && mod_count(0) == 2u && mod_count(1) == 2u &&
          mod_count(2) == 2u && mod_count(3) == 3u && !pj_changed());
    param_defaults();                                    /* (the scene below on the plain defaults) */
    for (k = 0; k < NTRK; k++) {
        mdl[k].n = 0;
        mod_rebuild(k);
    }
    pj_scene();
    check("..a change makes the project changed", pj_changed());
    fm1_ms += 300;
    ui_input();
    draw_head();
    check("..the header's asterisk shows it (within a quarter second)", ui.unsaved == 1u);
    shot("BRYO", "unsaved");
    rc = pj_save(1);
    fm1_ms += 300;
    ui_input();
    check("..saved: the asterisk goes", ui.unsaved == 0u);
    fm1_ms += 300;
    ui_input();
    memcpy(want, tp, sizeof tp);
    memcpy(wdep, mdl, sizeof mdl);
    memcpy(wtr, track, sizeof track);
    memcpy(wmst, mst, sizeof mst);
    pj_current(1, &h);
    snprintf(m, sizeof m, "saved into P2: %u bytes packed (%u unpacked, a sector holds %u)", h.len, h.raw, PJ_PAYLOAD_MAX);
    check(m, rc == 0 && pj_cur == 1 && !pj_changed() && pj_sum[1].used && h.len < h.raw && h.len < 2500u);
    power_on();                                          /* a power cycle */
    check("..power-on alone: the defaults (nothing loaded yet)", tp[1].src == SRC_TAPE && sys.bpm != 97);
    pj_boot();
    same = !memcmp(tp, want, sizeof tp) && !memcmp(mdl, wdep, sizeof mdl) && !memcmp(mst, wmst, sizeof mst);
    for (k = 0; k < NTRK; k++)
        same &= track[k].level == wtr[k].level && track[k].mute == wtr[k].mute && track[k].octave == wtr[k].octave;
    check("..booted: P2 back as saved: every knob, source, pattern, step, depth, level, mute, octave, the tempo",
          same && pj_cur == 1 && sys.bpm == 97 && sys.ntrk == 3 && mod_count(1) == 1u && mod_count(3) == 1u &&
          !pj_changed());
    {                                                    /* the depths reach the ISR (the lists rebuilt) */
        const int32_t *last[NTRK] = {track_rt[0].last, track_rt[1].last, track_rt[2].last, track_rt[3].last};
        mod_tick(0, 0, last);
        check("..and the loaded depths are live (track 2's list holds SEED's)", (mod_on >> 1) & 1u);
    }

    /* a track's own tape: kept in a user reel named after the project and the track */
    tp[0].src = SRC_TAPE;
    tp[0].dev[DEV_SRC][TK_REEL] = 1;
    tape_prepare(0);                                     /* (factory reel 1 copied onto the tape: a take) */
    tape_unprepare(0);
    n0 = tape_ctl[0].nblk;
    rc = pj_save(1);
    k = (uint32_t)tp[0].dev[DEV_SRC][TK_REEL];
    check("..T1's own tape saved: into user reel P2T1, the track now plays that reel (the same length)",
          rc == 0 && k > NREEL && uslot_valid(k - NREEL - 1u) && !strcmp(uslot_hdr(k - NREEL - 1u)->name, "P2T1") &&
          uslot_hdr(k - NREEL - 1u)->nblk == n0);
    for (n0 = 0, s = 0; s < USLOT_N; s++)
        n0 += uslot_valid(s);
    rc = pj_save(1);
    for (k = 0, s = 0; s < USLOT_N; s++)
        k += uslot_valid(s);
    check("..saved again unchanged: no new reel", rc == 0 && k == n0 && n0 == 1u);
    power_on();
    pj_boot();
    {
        tape_view_t v;
        tape_view(0, &v);
        check("..power-cycled: T1 plays P2T1, as long as the take", v.len == uslot_hdr(0)->nblk * TAPE_BLK && v.len);
    }
    for (s = 1; s < USLOT_N; s++)                        /* no reel free: the project still saves */
        uslot_save(s, REELS[0].data, REELS[0].pred, REELS[0].idx, REELS[0].peak, 4, "FULL");
    tp[2].dev[DEV_SRC][TK_REEL] = 1;
    tp[2].pol[PL_REEL] = 1;
    tape_prepare(2);
    tape_unprepare(2);
    rc = pj_save(1);
    check("..no reel free for T3's take: saved without it, and SAVE knows which", rc == 1 << 2 && pj_sum[1].used);

    /* a power cut in the middle of a save: the copy before stays in charge */
    sys.bpm = 150;
    host_prog_limit = 600;
    rc = pj_save(1);
    host_prog_limit = 0xFFFFFFFFu;
    power_on();
    pj_boot();
    check("a power cut halfway through a save: the copy before is loaded (the tempo it had, not 150)",
          rc < 0 && pj_sum[1].used && sys.bpm == 97);

    /* a Felucca record in a slot's sector reads as empty; an empty slot doesn't load */
    rf_erase(pj_sector(0, 0));
    {
        static const uint8_t FELU[32] = {'F', 'E', 'L', 'U', 1, 0, 0, 0, 1};
        rf_prog(pj_sector(0, 0), FELU, sizeof FELU);
    }
    pj_scan(0);
    check("a Felucca project in P1's sector: P1 reads as empty, and doesn't load", !pj_sum[0].used && pj_load(0) == -1);

    /* the project view */
    power_on();
    pj_boot();
    host_enc[panel.enc[EN_PRESET]] = 1;
    ui_input();
    check("PRESETS: the project view on P2 (the one loaded); turned again, P3", ui.view == VIEW_PROJECT && ui.pj_sel == 1u &&
          (host_enc[panel.enc[EN_PRESET]] = 1, ui_input(), ui.pj_sel == 2u));
    shot("BRYO", "projects");
    press(B_OCTUP);
    {
        uint32_t t, blank = 1, b;
        for (t = 0; t < NTRK; t++) {
            blank &= tp[t].dev[DEV_SRC][TK_REEL] == 0 && tp[t].pol[PL_REEL] == 0 && mdl[t].n == 0 && tape_ctl[t].empty &&
                     track[t].level == 100u && tp[t].arp[AR_MODE] == ARM_OFF;
            for (b = 0; b < DRM_NBAR; b++)
                blank &= tp[t].dpat[b][0] == 0 && tp[t].dacc[b] == 0;
        }
        check("..OCT+ on an empty slot: a new project there, blank (empty tapes, no reels, no pattern, no depths, 120 BPM)",
              blank && sys.bpm == 120 && pj_cur == 2 && !pj_sum[2].used && !pj_changed() &&
              !strncmp(ui.msg, "P3: NEW AND BLANK", 17));
        shot("BRYO", "project_new");
    }
    pj_load(1);                                          /* (back to P2's scene for the steps below) */
    tap(B_SAVE);
    check("..SAVE: P3 saved (an empty slot: no question), now the current one", pj_sum[2].used && pj_cur == 2 &&
          !strncmp(ui.msg, "P3 SAVED", 8));
    host_enc[panel.enc[EN_PRESET]] = -1;
    ui_input();
    tap(B_SAVE);
    check("..SAVE on P2 (another project): asked first, not saved", ui.pj_ask == PJ_ASK_SAVE && pj_cur == 2);
    tap(B_SAVE);
    check("..SAVE again within 3 s: saved over it", pj_cur == 1 && !strncmp(ui.msg, "P2 SAVED", 8));
    sys.bpm = 133;
    host_enc[panel.enc[EN_PRESET]] = 1;
    ui_input();
    press(B_OCTUP);
    check("..OCT+ on P3 with a change not saved: asked first", ui.pj_ask == PJ_ASK_LOAD && sys.bpm == 133);
    press(B_OCTUP);
    check("..OCT+ again: P3 loaded", pj_cur == 2 && sys.bpm == 97 && !strcmp(ui.msg, "P3 LOADED"));
    fm1_in.notes = 1u << black_note(BK_POLY);
    key_edge(black_note(BK_POLY));
    fm1_ms += 600;
    ui_input();
    fm1_in.notes = 0;
    check("..POLY held: P3 deleted (its reels stay)", !pj_sum[2].used && !strncmp(ui.msg, "P3 DELETED", 10) &&
          uslot_valid(0));
    turn(0, 5);
    press(B_OCTDN);
    check("..the knobs do nothing there; OCT-: back to the page", ui.view == VIEW_PAGE && tp[0].dev[DEV_SRC][0] == 0);
    sys.bpm = 101;
    tap(B_SAVE);
    check("SAVE tapped on a page: into the current project (P3)", pj_cur == 2 && pj_sum[2].used && pj_sum[2].bpm == 101);

    {   /* the rooms and each track's ROOM are saved with the project; a track preset keeps the track's ROOM */
        pj_wipe();
        power_on();
        rmp[0][RM_SIZE] = 70;
        rmp[0][RM_PRE] = 150;
        rmp[1][RM_QUAL] = RQ_LO;
        tp[0].dev[DEV_SPACE][SP_ROOM] = 1;
        tp[1].dev[DEV_SPACE][SP_VERB] = 40;
        rc = pj_save(0);
        power_on();
        rc |= pj_load(0);
        check("the rooms and each track's ROOM come back as saved (A: SIZE 70, PRE 150; B: QUAL LO; T1 on B)",
              rc == 0 && rmp[0][RM_SIZE] == 70 && rmp[0][RM_PRE] == 150 && rmp[1][RM_QUAL] == RQ_LO &&
              tp[0].dev[DEV_SPACE][SP_ROOM] == 1 && tp[1].dev[DEV_SPACE][SP_ROOM] == 0);
        tp[1].dev[DEV_SPACE][SP_ROOM] = 1;               /* a track preset: the room it sends to stays the track's */
        rc = tpr_save(0, 1);
        tp[1].dev[DEV_SPACE][SP_ROOM] = 0;
        rc |= tpr_load(0, 1);
        check("..a track preset loaded keeps the track's ROOM (the rooms are the mix's)", rc == 0 &&
              tp[1].dev[DEV_SPACE][SP_ROOM] == 0 && tp[1].dev[DEV_SPACE][SP_VERB] == 40);
    }
    pj_wipe();
    power_on();
}

/* each drum's own knobs (TUNE DEC LVL TONE) are modulation targets: its key held, then a modulator's pad, a knob */
static int32_t din_peak(void)
{
    uint32_t b;
    int32_t pk = 0;
    sys.playing = 1;
    for (b = 0; b < 300u; b++) {
        render(1, 0);
        pk = peak_of(track_rt[0].last, CTL) > pk ? peak_of(track_rt[0].last, CTL) : pk;
    }
    sys.playing = 0;
    render(2000, 0);
    return pk;
}
static void test_drum_mod(void)
{
    uint32_t g = MOD_TDIN + DI_SD * NDIN + DIN_LVL;
    int32_t plain, low;
    power_on();
    tp[0].src = SRC_DRUM;
    track[1].mute = track[2].mute = track[3].mute = 1;
    focus_dev(DEV_SRC, 2);                               /* KIT */
    fm1_in.notes = note_bit_of_white(1);                 /* SD's key held */
    key_edge(note_of_white(1));
    check("KIT, SD's key held: KNOB 1-4 are SD's own, and they're targets", ui.drm_inst == DI_SD + 1u &&
          page_target(2) == g && mod_tdesc(g) == &DRI_P[DIN_LVL]);
    hold(B_LFO);                                         /* then the modulator's pad (no page jump: the key was first) */
    turn(2, 50);
    let_go(B_LFO);
    fm1_in.notes = 0;
    ui_input();
    check("..then LFO held + KNOB 3: M1's depth to SD's LVL, 50 (the page stays KIT)", mod_dep(0, 0, g) == 50 &&
          ui.page == 2u && tp[0].dins[DI_SD][DIN_LVL] == 0);
    mod_dep_set(0, 0, g, 0);
    mod_rebuild(0);

    /* the sound: SD alone on step 1; M1 an LFO held at -1 (AMT 0, OFS -100) and a depth of 100 on SD's LVL */
    memset(tp[0].dpat, 0, sizeof tp[0].dpat);
    tp[0].dpat[0][0] = 1u << DI_SD;
    plain = din_peak();
    tp[0].mod[0][8] = 0;                                 /* M1 (LFO) AMT 0 */
    tp[0].mod[0][9] = -100;                              /* OFS -100: its output pinned to the bottom */
    mod_nudge(0, 0, g, 100);
    low = din_peak();
    check("an LFO on SD's LVL reaches the hit: pinned low, the snare much quieter (-24 dB at the bottom)",
          plain > 1000 && low < plain / 8);
    check("..SD's own LVL knob isn't touched (the modulated copy is what the hit reads)", tp[0].dins[DI_SD][DIN_LVL] == 0);
    /* a project keeps it */
    pj_wipe();
    pj_save(0);
    mod_dep_set(0, 0, g, 0);
    mod_rebuild(0);
    pj_load(0);
    check("..a project keeps a depth on an instrument's knob", mod_dep(0, 0, g) == 100);
    pj_wipe();
}

/* the white keys as steps: DRUM's STEP page lights the bar's set steps, and the playhead flips the one it's on; a
 * SEQ page glows its LEN from STRT and lights the step playing and the keys held */
static void white_lights(uint32_t *lit, uint32_t *glow)
{
    uint32_t k;
    *lit = *glow = 0;
    ui_input();
    ui_leds();
    for (k = 0; k < 27u; k++) {
        uint32_t w = KEY_WHITE[k];
        if (w == KEY_NONE || w >= 16u)
            continue;
        led_on(fm1_led, 14u + k);
        *lit |= led_lit << w;
        led_on(fm1_led_dim, 14u + k);
        *glow |= led_lit << w;
    }
}
static void test_step_leds(void)
{
    uint32_t lit, glow, b, now = 0xFFu, ok = 1, seen = 0;
    char m[160];
    power_on();
    tp[0].src = SRC_DRUM;
    memset(tp[0].dpat, 0, sizeof tp[0].dpat);
    memset(tp[0].dacc, 0, sizeof tp[0].dacc);
    tp[0].drm[DM_MODE] = DMODE_HITS;
    tp[0].drm[DM_BAR] = 1;
    tp[0].drm[DM_INST] = 0;
    focus_dev(DEV_SRC, 3);                               /* STEP */
    key_edge(note_of_white(0));
    key_edge(note_of_white(4));
    key_edge(note_of_white(10));
    white_lights(&lit, &glow);
    snprintf(m, sizeof m, "DRUM STEP, three steps added (1, 5, 11): their white keys lit, all 16 glowing (lit %04x)", lit);
    check(m, tp[0].dpat[0][4] == 1u && lit == 0x0411u && glow == 0xFFFFu);
    sys.playing = 1;
    for (b = 0; b < 1378u; b++) {                        /* a second playing: wherever the playhead is in bar 1, its
                                                          * key flipped (a set step dim, an empty one lit) */
        render(1, 0);
        now = drm_now[0];
        white_lights(&lit, &glow);
        if (now != 0xFFu && now < 16u) {
            ok &= lit == (0x0411u ^ (1u << now));
            seen |= 1u << now;
        } else {
            ok &= lit == 0x0411u;
        }
    }
    snprintf(m, sizeof m, "..playing: the step playing flips its key as the playhead passes (%u of bar 1's steps seen)",
             (uint32_t)__builtin_popcount(seen));
    check(m, ok && (seen & 0x0411u) && (seen & ~0x0411u & 0xFFFFu));
    sys.playing = 0;
    tp[0].drm[DM_MODE] = DMODE_ACC;
    tp[0].dacc[0] = 0x8001u;
    white_lights(&lit, &glow);
    check("..ACC: the accents' keys lit instead", lit == 0x8001u);
    tp[0].drm[DM_MODE] = DMODE_LIVE;
    white_lights(&lit, &glow);
    check("..LIVE: the keys play the kit, no steps shown", lit == 0u);
    tp[0].drm[DM_MODE] = DMODE_HITS;
    hold(B_GLO);
    white_lights(&lit, &glow);
    let_go(B_GLO);
    check("..GLO held: not the steps (the white keys pick tracks then)", lit != 0x0411u);

    power_on();                                          /* a SEQ: LEN 8 from STRT 3 */
    focus_slot(SLOT_SEQ);
    tp[0].mod[SLOT_SEQ][0] = 8;
    tp[0].mod[SLOT_SEQ][7] = 3;
    tp[0].mod[SLOT_SEQ][1] = 4;                          /* (RATE 1/16: LEN round twice in the two seconds below) */
    white_lights(&lit, &glow);
    check("SEQ page, LEN 8 from STRT 3: steps 3-10 glow, none lit while stopped", glow == 0x03FCu && lit == 0u);
    sys.playing = 1;
    for (b = 0, ok = 1, seen = 0; b < 1378u * 2u; b++) {
        int32_t n;
        render(1, 0);
        n = mod_seq_now(0, SLOT_SEQ);
        white_lights(&lit, &glow);
        if (n >= 0) {
            ok &= lit == 1u << (uint32_t)n;
            seen |= 1u << (uint32_t)n;
        }
    }
    snprintf(m, sizeof m, "..playing: the step playing lit, one at a time, only within LEN (%04x seen)", seen);
    check(m, ok && seen == 0x03FCu);
    sys.playing = 0;
    fm1_in.notes = note_bit_of_white(12);
    white_lights(&lit, &glow);
    fm1_in.notes = 0;
    check("..a white key held (KNOB 1 sets its step): lit", (lit >> 12) & 1u);
    power_on();
}

/* page keys: a page's pad held, the white keys show its pages (dim: there, lit: shown) and pick one */
static void test_page_keys(void)
{
    uint32_t k, dim_ok = 1, lit_ok = 1, dark_ok = 1;
    power_on();
    hold(B_HOME);                                        /* TAPE, page 1: four pages */
    ui_leds();
    for (k = 0; k < 27u; k++) {
        uint32_t w = KEY_WHITE[k];
        led_on(fm1_led_dim, 14u + k);
        if (w != KEY_NONE && w < 4u)
            dim_ok &= led_lit;
        else
            dark_ok &= !led_lit;
        led_on(fm1_led, 14u + k);
        if (w != KEY_NONE && w < 4u)
            lit_ok &= led_lit == (w == 0u);
        else
            dark_ok &= !led_lit;
    }
    check("HOME held on TAPE: white keys 1-4 glow (its four pages), key 1 lit (the one shown), every other key dark",
          dim_ok && lit_ok && dark_ok && !sys.keys_live);
    key_edge(note_of_white(2));
    let_go(B_HOME);
    check("..+ key 3: TAPE page 3, and the let-go doesn't turn it on", ui.dev == DEV_SRC && ui.page == 2u);
    hold(B_HOME);
    key_edge(note_of_white(6));
    let_go(B_HOME);
    check("..+ key 7 (past its four): says so, the page stays", ui.page == 2u && !strcmp(ui.msg, "NO PAGE THERE"));
    hold(B_EDIT);
    key_edge(note_of_white(7));
    let_go(B_EDIT);
    check("EDIT held + key 8: RIPPLE page 2 (GRAIN's six, then RIPPLE's)", ui.dev == DEV_RESO && ui.page == 1u);
    press(B_EDIT);
    check("..EDIT tapped still turns pages (on the let-go): RIPPLE page 3", ui.dev == DEV_RESO && ui.page == 2u);
    hold(B_FX);
    key_edge(note_of_white(4));
    let_go(B_FX);
    check("FX held + key 5: SPACE page 2 (COLOR's three, then SPACE's)", ui.dev == DEV_SPACE && ui.page == 1u);
    hold(B_LFO);
    key_edge(note_of_white(2));
    let_go(B_LFO);
    check("LFO held + key 3: M1's page 3", ui.kind == FOCUS_SLOT && ui.slot == 0u && ui.page == 2u);
    hold(B_LFO);
    ui_leds();
    led_on(fm1_led, 14u + black_note(BK_OP1));
    k = led_lit;
    led_on(fm1_led_dim, 14u + black_note(BK_OP2));
    check("..LFO held: OP1 lit (it's on M1), OP2 glowing (M2 to pick)", k == 1u && led_lit == 1u);
    let_go(B_LFO);
    hold(B_SEQ);
    key_edge(note_of_white(1));
    let_go(B_SEQ);
    check("SEQ held + key 2: M4's page 2", ui.kind == FOCUS_SLOT && ui.slot == SLOT_SEQ && ui.page == 1u);
    hold(B_ARP);
    key_edge(note_of_white(1));
    let_go(B_ARP);
    check("ARP held + key 2: the arpeggiator's page 2", ui.kind == FOCUS_ARP && ui.page == 1u);
    press(B_ARP);
    check("..ARP tapped: its next page (on the let-go)", ui.kind == FOCUS_ARP && ui.page == 2u);
    hold(B_ENV);
    ui_leds();
    led_on(fm1_led, 14u + note_of_white(0));
    let_go(B_ENV);
    check("ENV held: key 1 lit (its one page, shown)", led_lit == 1u && ui.dev == DEV_SRC && ui.page == 1u);
}

/* ENV: the source's envelope page on every source; TAPE's slice envelope, stopped and playing */
static int32_t env_peak(uint32_t blocks)
{
    uint32_t b;
    int32_t pk = 0;
    for (b = 0; b < blocks; b++) {
        render(1, 0);
        pk = peak_of(track_rt[0].last, CTL) > pk ? peak_of(track_rt[0].last, CTL) : pk;
    }
    return pk;
}
static void test_env_pad(void)
{
    static const uint8_t SRCS[4] = {SRC_TAPE, SRC_SYNTH, SRC_POLY, SRC_DRUM};
    static const char *const FIRST[4] = {"ATK", "ATK", "ATK", "TUNE"};
    uint32_t i, k1 = note_of_white(0);
    int32_t held, after, tail;
    for (i = 0; i < 4u; i++) {
        int16_t *vp;
        power_on();
        tp[0].src = SRCS[i];
        press(B_ENV);
        {
            char m[96];
            const pdesc_t *d = ui_page(0, &vp);
            snprintf(m, sizeof m, "ENV on %s: its envelope's page (KNOB 1 %s), ENV lit", SRC_NAME[SRCS[i]], d->label);
            check(m, ui.view == VIEW_PAGE && ui.dev == DEV_SRC && !strcmp(d->label, FIRST[i]) && focus_btn() == B_ENV);
        }
    }

    /* TAPE stopped: a held key's slice through ATK DEC SUS, REL as it's let go */
    power_on();
    track[1].mute = track[2].mute = track[3].mute = 1;
    tp[0].dev[DEV_SRC][TK_REEL] = 2;
    render(50, 0);
    fm1_in.notes = 1u << k1;
    held = env_peak(60);
    fm1_in.notes = 0;
    after = env_peak(3000);
    check("TAPE, the defaults: a tapped slice plays as before (to its end, then silence)", held > 1000 &&
                                                                                        peak_of(track_rt[0].last, CTL) == 0);
    tp[0].dev[DEV_SRC][TK_DEC] = 30;                     /* DEC short, SUS 0: a held slice dies away */
    tp[0].dev[DEV_SRC][TK_SUS] = 0;
    fm1_in.notes = 1u << k1;
    held = env_peak(20);
    tail = env_peak(200);
    check("..SUS 0, DEC short: a held slice sounds, then dies away while the key's still held", held > 1000 && tail < held / 50);
    fm1_in.notes = 0;
    render(50, 0);
    tp[0].dev[DEV_SRC][TK_SUS] = 100;                    /* SUS 100, REL short: let go, it stops */
    tp[0].dev[DEV_SRC][TK_REL] = 20;
    fm1_in.notes = 1u << k1;
    held = env_peak(40);
    fm1_in.notes = 0;
    render(20, 0);
    after = env_peak(20);
    check("..REL short: let go, it's silent within a few ms", held > 1000 && after == 0);
    tp[0].dev[DEV_SRC][TK_ATK] = 90;                     /* ATK long (~0.7 s): it rises */
    fm1_in.notes = 1u << k1;
    held = env_peak(3);
    tail = env_peak(400);
    fm1_in.notes = 0;
    render(100, 0);
    check("..ATK long: the slice rises (quiet at first)", held < tail / 8 && tail > 1000);

    /* playing: the loop as it is; a key's jump fades in over ATK */
    tp[0].dev[DEV_SRC][TK_ATK] = 0;
    sys.playing = 1;
    held = env_peak(200);
    check("TAPE playing, no key: the loop as it is (SUS and REL don't gate it)", held > 1000);
    tp[0].dev[DEV_SRC][TK_ATK] = 90;
    fm1_in.notes = 1u << k1;
    render(1, 0);
    after = env_peak(2);
    tail = env_peak(400);
    fm1_in.notes = 0;
    held = env_peak(400);
    check("..a key's jump fades in over ATK, and let go the loop stays", after < tail / 4 && held > 1000);
    tp[0].dev[DEV_SRC][TK_ATK] = 0;                      /* SUS 0 and a key held long: the loop doesn't die away */
    tp[0].dev[DEV_SRC][TK_DEC] = 20;
    tp[0].dev[DEV_SRC][TK_SUS] = 0;
    fm1_in.notes = 1u << k1;
    render(300, 0);
    tail = env_peak(300);
    fm1_in.notes = 0;
    check("..playing, SUS 0 and a key held: the loop still sounds (DEC SUS REL are the stopped slice's)", tail > 1000);
    sys.playing = 0;
    render(4, 0);
}

/* track presets (tpreset.c): one track's sound saved, loaded into another, what stays with the track, the undo, a
 * power cycle, a delete, a save cut by a power cut, a full bank; and SCL's reset to the defaults */
static void tpr_wipe(void)
{
    rf_erase(pj_sector(TPR_BANK, 0));
    rf_erase(pj_sector(TPR_BANK, 1));
    tpr_scan();
}
static void test_tpresets(void)
{
    uint32_t i, k, g, ok;
    int rc;
    power_on();
    tpr_wipe();
    for (ok = 1, i = 0; i < TPR_N; i++)
        ok &= !tpr_sum[i].used;
    check("track presets: an empty bank, eight empty presets (A..H)", ok && TPR_N == 8u);

    /* track 1: SYNTH, some knobs, three depths, a SEQ's steps */
    tp[0].src = SRC_SYNTH;
    tp[0].syn[SY_CUT] = 40;
    tp[0].syn[SY_RES] = 77;
    tp[0].dev[DEV_SPACE][4] = 33;
    tp[0].ch[CH_PAN] = -20;
    tp[0].steps[2][3] = 9;
    tp[0].mod[0][0] = 99;
    mod_nudge(0, 0, MOD_TSRC + SRC_SYNTH * NPK + SY_CUT, 30);
    mod_nudge(0, 1, MOD_TG(DEV_COLOR, 0), -15);
    mod_nudge(0, 2, MOD_TCH + CH_PAN, 40);
    rc = tpr_save(0, 0);
    check("track 1 saved as A: the list shows SYNTH, its engines, 3 depths",
          rc == 0 && tpr_sum[0].used && tpr_sum[0].src == SRC_SYNTH && tpr_sum[0].ndep == 3u &&
          tpr_sum[0].eng[1] == ME_ADSR && !tpr_sum[1].used);

    /* track 2: a TAPE track on factory reel 3, a pattern, its own level and octave, POLY knobs of its own */
    tp[1].dev[DEV_SRC][TK_REEL] = 3;
    tp[1].pol[PL_CUT] = 50;
    tp[1].dpat[0][5] = 0x11;
    tp[1].drm[DM_PATN] = 4;
    track[1].level = 60;
    track[1].octave = 5;
    tp[1].recin = RIN_OTHR;
    mod_nudge(1, 3, MOD_TG(DEV_GRAIN, 0), 50);
    pj_hash_at = pj_hash();
    rc = tpr_load(0, 1);
    check("A into track 2: the project counts as changed", pj_changed());
    for (ok = 1, k = 0; k < NPK; k++)
        ok &= tp[1].syn[k] == tp[0].syn[k] && tp[1].mod[0][k] == tp[0].mod[0][k] &&
              tp[1].dev[DEV_SPACE][k] == tp[0].dev[DEV_SPACE][k];
    check("A into track 2: SYNTH with track 1's knobs, devices, slots and channel strip",
          rc == 0 && ok && tp[1].src == SRC_SYNTH && tp[1].ch[CH_PAN] == -20 && tp[1].steps[2][3] == 9);
    check("..its depths, and only those (track 2's own one gone)",
          mod_count(1) == 3u && mod_dep(1, 0, MOD_TSRC + SRC_SYNTH * NPK + SY_CUT) == 30 &&
          mod_dep(1, 2, MOD_TCH + CH_PAN) == 40 && !mod_dep(1, 3, MOD_TG(DEV_GRAIN, 0)));
    check("..what isn't the sound stays: the pattern, level, octave, REC IN, POLY's knobs (a source it isn't on)",
          tp[1].dpat[0][5] == 0x11 && tp[1].drm[DM_PATN] == 4 && track[1].level == 60 && track[1].octave == 5u &&
          tp[1].recin == RIN_OTHR && tp[1].pol[PL_CUT] == 50);
    check("..a preset from a track on its own tape names no reel: track 2 keeps reel 3",
          tp[1].dev[DEV_SRC][TK_REEL] == 3);
    rc = trk_restore();
    check("the snapshot back (SAVE held's): track 2 as it was, its own depth too",
          rc == 0 && tp[1].src == SRC_TAPE && tp[1].syn[SY_CUT] == SYN_P[SY_CUT].def && mod_count(1) == 1u &&
          mod_dep(1, 3, MOD_TG(DEV_GRAIN, 0)) == 50 && trk_restore() == -1);

    /* a TAPE track on its own tape saved as E: no reel named, so the track it goes to keeps its own */
    tp[3].src = SRC_TAPE;
    tp[3].dev[DEV_SRC][TK_REEL] = 0;
    tp[3].dev[DEV_SRC][TK_SPD] = 150;
    tpr_save(4, 3);
    tp[1].src = SRC_TAPE;
    tp[1].dev[DEV_SRC][TK_REEL] = 3;
    tpr_load(4, 1);
    check("E (a TAPE track on its own tape) into a track on reel 3: SPD comes, reel 3 stays",
          tp[1].dev[DEV_SRC][TK_SPD] == 150 && tp[1].dev[DEV_SRC][TK_REEL] == 3);
    tpr_delete(4);

    /* a TAPE track on factory reel 2 saved as C: the reel is named */
    tp[2].dev[DEV_SRC][TK_REEL] = 2;
    tp[2].dev[DEV_SRC][TK_SPD] = -50;
    tpr_save(2, 2);
    tpr_load(2, 1);
    check("C (a TAPE track on reel 2) into track 2: the reel comes with it", tp[1].src == SRC_TAPE &&
          tp[1].dev[DEV_SRC][TK_REEL] == 2 && tp[1].dev[DEV_SRC][TK_SPD] == -50);

    /* DRUM: the kit's sound comes, the pattern's own knobs stay */
    tp[3].src = SRC_DRUM;
    tp[3].drm[DM_TONE] = 40;
    tp[3].drm[DM_LEN] = 4;
    tp[3].dins[DI_SD][DIN_TUNE] = 5;
    tpr_save(3, 3);
    tp[0].src = SRC_DRUM;
    tp[0].drm[DM_LEN] = 2;
    tp[0].drm[DM_PATN] = 7;
    tpr_load(3, 0);
    check("D (DRUM) into track 1: TONE and the snare's tune come, LEN and PATN (the pattern's) stay",
          tp[0].drm[DM_TONE] == 40 && tp[0].dins[DI_SD][DIN_TUNE] == 5 && tp[0].drm[DM_LEN] == 2 &&
          tp[0].drm[DM_PATN] == 7);

    /* a power cycle: the bank's still there */
    power_on();
    tpr_boot();
    check("after a power cycle: A, C and D listed, B empty", tpr_sum[0].used && !tpr_sum[1].used && tpr_sum[2].used &&
                                                           tpr_sum[3].used && tpr_sum[3].src == SRC_DRUM);
    rc = tpr_load(0, 0);
    check("..and A loads", rc == 0 && tp[0].src == SRC_SYNTH && tp[0].syn[SY_RES] == 77);
    check("an empty preset doesn't load (nothing changes)", tpr_load(1, 0) == -1 && tp[0].src == SRC_SYNTH);

    /* a delete; a save cut short by a power cut leaves the bank as it was */
    rc = tpr_delete(2);
    check("C deleted: the others stay", rc == 0 && !tpr_sum[2].used && tpr_sum[0].used && tpr_sum[3].used);
    host_prog_limit = 300;
    rc = tpr_save(5, 1);
    host_prog_limit = 0xFFFFFFFFu;
    tpr_scan();
    check("a save cut by a power cut: refused, the bank as it was (A and D, no F)",
          rc != 0 && tpr_sum[0].used && tpr_sum[3].used && !tpr_sum[5].used);

    /* a full bank: the densest tracks there can be, until one doesn't fit */
    {
        uint32_t r = 99, s, n = 0;
        int last = 0;
        for (s = 0; s < NDEV; s++)
            for (k = 0; k < NPK; k++)
                if (!pdesc_empty(&DEV_P[s][k]))
                    tp[2].dev[s][k] = (int16_t)(DEV_P[s][k].min + (r = r * 1103515245u + 12345u) % (uint32_t)(DEV_P[s][k].max - DEV_P[s][k].min + 1));
        for (s = 0; s < NSLOT; s++) {
            param_engine(2, s, ME_SEQ);
            for (k = 0; k < 16u; k++)
                tp[2].steps[s][k] = (int8_t)((r = r * 1103515245u + 12345u) >> 16) % 100;
        }
        for (s = 0; s < NSLOT; s++)                      /* (every slot knob and DRUM's kit: random too) */
            for (k = 0; k < 12u; k++)
                tp[2].mod[s][k] = (int16_t)(ME_P[ME_SEQ][k].min + (r = r * 1103515245u + 12345u) % (uint32_t)(ME_P[ME_SEQ][k].max - ME_P[ME_SEQ][k].min + 1));
        for (s = 0; s < DRM_NINST; s++)
            for (k = 0; k < NDIN; k++)
                tp[2].dins[s][k] = (int16_t)(DRI_P[k].min + (r = r * 1103515245u + 12345u) % (uint32_t)(DRI_P[k].max - DRI_P[k].min + 1));
        for (k = 0; k < NPK; k++)
            if (!pdesc_empty(&DRM_P[k]))
                tp[2].drm[k] = (int16_t)(DRM_P[k].min + (r = r * 1103515245u + 12345u) % (uint32_t)(DRM_P[k].max - DRM_P[k].min + 1));
        tp[2].src = SRC_DRUM;
        for (g = 0, n = 0; g < MOD_NTGT && n < MOD_MAX; g++)
            if (mod_tdesc(g) && mod_nudge(2, g % 4u, g, (int32_t)((r = r * 1103515245u + 12345u) >> 16) % 200 - 100) != -128)
                n++;
        for (i = 0; i < TPR_N && (last = tpr_save(i, 2)) == 0; i++)
            ;
        {
            char m[120];
            snprintf(m, sizeof m, "a bank of the densest tracks there can be: %u fit, then it says it's full (-2)", i);
            check(m, i >= 5u && (i == TPR_N || last == -2));
        }
        tpr_scan();
        check("..and what it held before the refusal is still there", tpr_sum[0].used && tpr_sum[4].used);
    }

    /* the view: PRESETS turned, EDIT to the presets' page, SAVE, OCT+, POLY held; SAVE held undoes the load */
    power_on();
    tpr_wipe();
    tp[0].src = SRC_POLY;
    tp[0].pol[PL_CUT] = 30;
    host_enc[panel.enc[EN_PRESET]] = 1;
    ui_input();
    press(B_EDIT);
    check("PRESETS, then EDIT: the track presets' page (A pointed at)", ui.view == VIEW_PROJECT && ui.pj_page == 1u &&
                                                                       ui.tpr_sel == 0u);
    host_enc[panel.enc[EN_PRESET]] = 1;
    ui_input();
    tap(B_SAVE);
    check("..PRESETS to B, SAVE tapped: track 1 saved as B", ui.tpr_sel == 1u && tpr_sum[1].used &&
                                                             tpr_sum[1].src == SRC_POLY &&
                                                             !strcmp(ui.msg, "PRESET B SAVED FROM T1"));
    ui.msg_t = 0;                                        /* (the message off: the page itself) */
    shot("BRYO", "presets");
    tap(B_SAVE);
    check("..again over it: asked first", ui.pj_ask == PJ_ASK_SAVE);
    tap(B_SAVE);
    check("..SAVE again: saved over", ui.pj_ask == PJ_ASK_NONE && !strcmp(ui.msg, "PRESET B SAVED FROM T1"));
    sys.sel = 2;
    press(B_OCTUP);
    check("OCT+: B into the focused track (T3), no question, HOLD SAVE offered",
          tp[2].src == SRC_POLY && tp[2].pol[PL_CUT] == 30 && ui.undo == UNDO_TRACK &&
          !strcmp(ui.msg, "PRESET B INTO T3. HOLD SAVE: UNDO"));
    fm1_ms = 50000;
    hold(B_SAVE);
    fm1_ms = 50000 + HOLD_MS[settings_hold % 4u];
    ui_input();
    let_go(B_SAVE);
    check("..SAVE held: track 3 back as it was", tp[2].src == SRC_TAPE && tp[2].pol[PL_CUT] == POL_P[PL_CUT].def &&
                                                 !strcmp(ui.msg, "THE TRACK IS BACK"));
    {
        uint32_t pn = black_note(BK_POLY);
        fm1_ms = 60000;
        fm1_in.notes |= 1u << pn;
        key_edge(pn);
        fm1_ms = 60510;
        ui_input();
        fm1_in.notes &= ~(1u << pn);
        ui_input();
        check("POLY held on the presets' page: B deleted", !tpr_sum[1].used && !strcmp(ui.msg, "PRESET B DELETED"));
    }
    {   /* REELS, the third page: your reels; POLY held deletes the one pointed at */
        uint32_t pn = black_note(BK_POLY), r, lg = 0;
        tape_view_t v;
        for (r = 1; r < NREEL; r++)                      /* (the longest factory reel: it spans slots) */
            if (REELS[r].nblk > REELS[lg].nblk)
                lg = r;
        uslot_save(0, REELS[0].data, REELS[0].pred, REELS[0].idx, REELS[0].peak, REELS[0].nblk, "KICK");
        {                                                /* LONG: 1.3 slots (a reel repeated), so it spans two */
            static uint8_t d[(TAPE_NBLK + 100u) * (TAPE_BLK / 2u)], ix[TAPE_NBLK + 100u], pk[TAPE_NBLK + 100u];
            static int16_t pr[TAPE_NBLK + 100u];
            uint32_t b, nb = TAPE_NBLK + 100u;
            for (b = 0; b < nb; b++) {
                uint32_t q = b % REELS[lg].nblk;
                memcpy(d + b * (TAPE_BLK / 2u), REELS[lg].data + q * (TAPE_BLK / 2u), TAPE_BLK / 2u);
                pr[b] = REELS[lg].pred[q];
                ix[b] = REELS[lg].idx[q];
                pk[b] = REELS[lg].peak[q];
            }
            uslot_save(2, d, pr, ix, pk, nb, "LONG");
        }
        tp[0].dev[DEV_SRC][TK_REEL] = (int16_t)(NREEL + 1u);   /* T1's TAPE plays U1, T2's POLY U3 */
        tp[0].src = SRC_TAPE;
        tp[1].src = SRC_POLY;
        tp[1].pol[PL_REEL] = (int16_t)(NREEL + 3u);
        press(B_EDIT);
        check("EDIT again: REELS, your reels' page (U1 pointed at)", ui.view == VIEW_PROJECT && ui.pj_page == 2u &&
              ui.reel_sel == 0u);
        host_enc[panel.enc[EN_PRESET]] = 2;
        ui_input();
        ui.msg_t = 0;
        shot("BRYO", "reels");
        check("..PRESETS turns through U1..U6 (to U3); a long reel's slots read as one (U3 starts it)",
              ui.reel_sel == 2u && uslot_owner(2) == 2 && uslot_hdr(2)->span == 2u && uslot_owner(3) == 2);
        host_enc[panel.enc[EN_PRESET]] = 1;              /* (U4: a slot it runs into; deleting there deletes it) */
        ui_input();
        fm1_ms = 61000;
        fm1_in.notes |= 1u << pn;
        key_edge(pn);
        fm1_ms = 61510;
        ui_input();
        fm1_in.notes &= ~(1u << pn);
        ui_input();
        tape_view_of(1, NREEL + 3u, &v);
        check("..POLY held: LONG deleted (from any slot it covers), its slots free, POLY on T2 now plays nothing there",
              !uslot_valid(2) && uslot_owner(3) < 0 && !strcmp(ui.msg, "LONG DELETED") && v.len == 0u &&
              uslot_valid(0) && uslot_free_run(2) >= 0);
        host_enc[panel.enc[EN_PRESET]] = -6;
        ui_input();
        sys.playing = 1;
        render(10, 0);
        check("..U1 still plays on T1 (another reel untouched)", ui.reel_sel == 0u && peak_of(track_rt[0].last, CTL) > 0);
        fm1_ms = 62000;
        fm1_in.notes |= 1u << pn;
        key_edge(pn);
        fm1_ms = 62510;
        ui_input();
        fm1_in.notes &= ~(1u << pn);
        ui_input();
        render(10, 0);
        check("..deleted while T1 plays it: T1 goes silent at once, its REEL still names the slot (now U1, empty)",
              !uslot_valid(0) && peak_of(track_rt[0].last, CTL) == 0 && tp[0].dev[DEV_SRC][TK_REEL] == (int16_t)(NREEL + 1u) &&
              !strcmp(uslot_name[0], "U1"));
        sys.playing = 0;
        render(10, 0);
        fm1_ms = 63000;
        fm1_in.notes |= 1u << pn;
        key_edge(pn);
        fm1_ms = 63510;
        ui_input();
        fm1_in.notes &= ~(1u << pn);
        ui_input();
        check("..POLY held on an empty slot: it says so", !strcmp(ui.msg, "U1 IS EMPTY"));
        tp[1].src = SRC_TAPE;
    }
    press(B_EDIT);
    check("EDIT again: back to PROJECTS", ui.view == VIEW_PROJECT && ui.pj_page == 0u);
    press(B_OCTDN);
    sys.sel = 0;

    /* SCL held + a knob: that knob's default; + the page's pad: the page's; SAVE held puts them back */
    power_on();
    press(B_FX);                                         /* COLOR */
    tp[0].dev[DEV_COLOR][CP_DRIV] = 70;
    tp[0].dev[DEV_COLOR][CP_CRSH] = 50;
    tp[0].dev[DEV_COLOR][CP_TILT] = -40;
    hold(B_SCL);
    shot("BRYO", "scl_held");
    turn(0, 3);
    check("SCL held + KNOB 1 on COLOR: DRV to its default (0), the others as they were",
          tp[0].dev[DEV_COLOR][CP_DRIV] == 0 && tp[0].dev[DEV_COLOR][CP_CRSH] == 50 &&
          !strcmp(ui.msg, "DRV TO DEFAULT. HOLD SAVE: UNDO"));
    press(B_FX);
    check("..+ FX (the page's pad): the whole page to its defaults (TILT, on page 2, as it was), and the page stays",
          tp[0].dev[DEV_COLOR][CP_CRSH] == 0 && tp[0].dev[DEV_COLOR][CP_TILT] == -40 && ui.dev == DEV_COLOR &&
          ui.page == 0u);
    check("..the footer says what SCL does", ui.scl_held == 1u);
    let_go(B_SCL);
    turn(1, 4);
    check("SCL let go: a knob turns as usual", tp[0].dev[DEV_COLOR][CP_CRSH] == 4);
    fm1_ms = 70000;
    hold(B_SAVE);
    fm1_ms = 70000 + HOLD_MS[settings_hold % 4u];
    ui_input();
    let_go(B_SAVE);
    check("SAVE held: the knobs as they were before SCL (one undo for the whole hold)",
          tp[0].dev[DEV_COLOR][CP_DRIV] == 70 && tp[0].dev[DEV_COLOR][CP_CRSH] == 50 &&
          tp[0].dev[DEV_COLOR][CP_TILT] == -40);
    tp[0].src = SRC_DRUM;
    focus_dev(DEV_SRC, 0);
    tp[0].drm[DM_PATN] = 3;
    hold(B_SCL);
    turn(0, 1);
    check("SCL + PATN: not reset (it writes a pattern as it moves): said so", tp[0].drm[DM_PATN] == 3 &&
                                                                             !strcmp(ui.msg, "PATN WRITES A PATTERN: TURN IT"));
    press(B_HOME);
    check("..+ HOME on PATTERN: the rest of the page (SWNG, ACNT, LEN) to defaults, PATN kept, no page turn",
          tp[0].drm[DM_PATN] == 3 && tp[0].drm[DM_ACNT] == DRM_P[DM_ACNT].def && ui.page == 0u);
    let_go(B_SCL);
    hold(B_GLO);
    track[2].level = 30;
    hold(B_SCL);
    turn(2, -1);
    check("GLO's mixer + SCL + KNOB 3: track 3's level to its default (100)", track[2].level == 100);
    let_go(B_SCL);
    let_go(B_GLO);
    tpr_wipe();
}

/* the factory track presets (tpfactory.c): each loads, holds to its knobs' ranges, sounds (not silent, nothing past
 * full scale) and sounds unlike its source's defaults; what stays with the track stays; the view and its refusals */
static int32_t tf_a[44100 * 2], tf_b[44100 * 2];
static uint32_t tf_render(int32_t *buf, uint32_t f, int factory, int32_t *pk)   /* (2 s of track 1, the master out) */
{
    uint32_t b, i, n = 2756u, clip = 0;
    power_on();
    track[1].mute = track[2].mute = track[3].mute = 1;
    tp[0].dev[DEV_SRC][TK_REEL] = 2;                     /* (a factory reel for TAPE, POLY's default for POLY) */
    sys.bpm = 120;
    if (factory)
        tpf_load(f, 0);
    else
        tp[0].src = TPF[f].src;
    if (tp[0].src == SRC_SYNTH || tp[0].src == SRC_POLY)
        fm1_in.notes = note_bit_of_white(4);             /* (a key held the whole time) */
    sys.playing = 1;
    *pk = 0;
    for (b = 0; b < n; b++) {
        chain_block(out, CTL);
        chain_poll();                                    /* (the main loop: the devices' memory as it's wanted) */
        for (i = 0; i < CTL; i++) {
            int32_t v = out[2u * i], a = v < 0 ? -v : v;
            buf[(b * CTL + i) % (44100u * 2u)] = v;
            if (a > *pk)
                *pk = a;
            clip += a >= 32767;
        }
    }
    fm1_in.notes = 0;
    sys.playing = 0;
    render(2, 0);
    return clip;
}
static void test_tpfactory(void)   /* (and each within 3 dB of its defaults' peak: no jump in level) */
{
    uint32_t f, k;
    char m[160];
    for (f = 0; f < TPF_N; f++) {
        int32_t pa, pb;
        uint32_t clip, ok = 1;
        double diff = 0, e = 0;
        clip = tf_render(tf_a, f, 1, &pa);
        for (k = 0; k < TPF[f].nset; k++) {                 /* every knob it sets is in range and set */
            const pdesc_t *ds = 0;
            const int16_t *v = tpf_arr(0, TPF[f].set[k].a, &ds);
            ok &= v && ds && TPF[f].set[k].v >= ds[TPF[f].set[k].k].min && TPF[f].set[k].v <= ds[TPF[f].set[k].k].max;
        }
        ok &= mod_count(0) == TPF[f].ndep && tp[0].src == TPF[f].src;
        tf_render(tf_b, f, 0, &pb);
        for (k = 0; k < 44100u * 2u; k++) {
            double d = (double)tf_a[k] - tf_b[k];
            diff += d * d;
            e += (double)tf_b[k] * tf_b[k];
        }
        snprintf(m, sizeof m, "factory %u %s (%s): its knobs in range, %u depths; peak %.1f dBFS, %u samples at full "
                 "scale (the defaults: %.1f); %.1f dB from the defaults' sound", f + 1u, TPF[f].name, SRC_NAME[TPF[f].src],
                 TPF[f].ndep, 20.0 * log10((pa > 0 ? pa : 1) / 32767.0), clip, 20.0 * log10((pb > 0 ? pb : 1) / 32767.0),
                 10.0 * log10((diff + 1.0) / (e + 1.0)));
        check(m, ok && pa > 1000 && clip == 0u && diff > e * 0.01 && fabs(20.0 * log10((double)pa / pb)) <= 3.0);
    }

    /* what stays with the track */
    power_on();
    tp[0].src = SRC_DRUM;
    tp[0].drm[DM_PATN] = 5;
    tp[0].drm[DM_LEN] = 3;
    tp[0].dpat[1][2] = 0x21;
    tp[0].dev[DEV_SRC][TK_REEL] = 4;
    track[0].level = 70;
    tpf_load(7, 0);                                      /* DUB */
    check("DUB loaded: the pattern, PATN, LEN, the reel and the level stay; SEED and VARY are the preset's",
          tp[0].drm[DM_PATN] == 5 && tp[0].drm[DM_LEN] == 3 && tp[0].dpat[1][2] == 0x21 &&
          tp[0].dev[DEV_SRC][TK_REEL] == 4 && track[0].level == 70 && tp[0].drm[DM_SEED] == 7 &&
          tp[0].drm[DM_VARY] == 50);
    check("..and SAVE held's snapshot puts the track back", trk_restore() == 0 && tp[0].drm[DM_SEED] == 0 &&
                                                            tp[0].drm[DM_DECY] == 0);

    /* the view: past H, the factory ones */
    power_on();
    tpr_wipe();
    host_enc[panel.enc[EN_PRESET]] = 1;
    ui_input();
    press(B_EDIT);
    host_enc[panel.enc[EN_PRESET]] = (int32_t)TPR_N + 2;
    ui_input();
    check("PRESETS turned past H: factory 3 (BASS)", ui.tpr_sel == TPR_N + 2u);
    shot("BRYO", "presets_factory");
    press(B_OCTUP);
    check("..OCT+: BASS into T1", tp[0].src == SRC_SYNTH && tp[0].syn[SY_VOIC] == 1 && tp[0].syn[SY_TUNE] == -12 &&
                                  !strcmp(ui.msg, "BASS INTO T1. HOLD SAVE: UNDO"));
    tap(B_SAVE);
    check("..SAVE on a factory one: refused, says where to save", !strcmp(ui.msg, "FACTORY: SAVE INTO A..H"));
    {
        uint32_t pn = black_note(BK_POLY);
        fm1_ms = 80000;
        fm1_in.notes |= 1u << pn;
        key_edge(pn);
        fm1_ms = 80510;
        ui_input();
        fm1_in.notes &= ~(1u << pn);
        ui_input();
        check("..POLY held on it: it stays", !strcmp(ui.msg, "FACTORY PRESETS STAY"));
    }
    host_enc[panel.enc[EN_PRESET]] = 20;
    ui_input();
    check("..the last is factory 8", ui.tpr_sel == TPR_N + TPF_N - 1u);
    press(B_OCTDN);
}

/* Modulation reaches the sound: for a knob of each kind of array (a device, each source, DRUM, the channel) and each
 * engine, ~1.1 s of track 1 with a slot's depth on it against the same without. The knob values are checked
 * elsewhere (test_mod, test_mod_engines); this checks the audio itself moves, and that a depth of 0 is exactly the
 * sound without one. (tests/mod_audit.sh does every target, slowly; this is the quick guard in the suite) */
static int32_t mz_a[CTL * 1500], mz_b[CTL * 1500];
static void mz_setup(uint32_t src)
{
    power_on();
    track[1].mute = track[2].mute = track[3].mute = 1;
    tp[0].src = (uint8_t)src;
    sys.bpm = 120;
    sys.playing = 1;
    sys.keys_live = 1;
    fm1_in.notes = src == SRC_SYNTH || src == SRC_POLY ? note_bit_of_white(0) : 0u;   /* (a held note) */
}
/* track 1's own output (before the output stage, whose DC blocker, EQ and limiter carry their state from whatever
 * played before: they're not what modulation moves) */
static void mz_render(int32_t *o)
{
    uint32_t b;
    for (b = 0; b < 1500u; b++) {
        chain_block(out, CTL);
        memcpy(o + b * CTL, track_rt[0].last, CTL * sizeof *o);
    }
}
/* the difference modulation makes, in dB against the sound without it (-999: none at all) */
static double mz_diff(uint32_t src, uint32_t e, uint32_t g, int32_t dep)
{
    double sa = 0, sd = 0;
    uint32_t i;
    mz_setup(src);
    mz_render(mz_a);
    mz_setup(src);
    param_engine(0, 0, e);
    if (e == ME_WAVE) {                                  /* the LFO free and quick: several cycles in the window */
        tp[0].mod[0][0] = 100;
        tp[0].mod[0][12] = 0;
    }
    mod_nudge(0, 0, g, dep);
    mz_render(mz_b);
    fm1_in.notes = 0;
    for (i = 0; i < CTL * 1500u; i++) {
        double d = (double)mz_b[i] - mz_a[i];
        sa += (double)mz_a[i] * mz_a[i];
        sd += d * d;
    }
    return sd == 0 ? -999.0 : 10 * log10(sd / (sa + 1));
}

static void test_mod_audio(void)
{
    static const struct {
        const char *what;
        uint32_t src, e, g;
        int32_t dep;
    } C[] = {
        {"an LFO on TAPE's SPD (a source knob)", SRC_TAPE, ME_WAVE, MOD_TSRC + SRC_TAPE * NPK + TK_SPD, 40},
        {"an LFO on COLOR's DRV (a device)", SRC_TAPE, ME_WAVE, MOD_TG(DEV_COLOR, CP_DRIV), 80},
        {"an ADSR on SYNTH's CUT, a note held", SRC_SYNTH, ME_ADSR, MOD_TSRC + SRC_SYNTH * NPK + SY_CUT, 60},
        {"an LFO on POLY's TUNE, a note held", SRC_POLY, ME_WAVE, MOD_TSRC + SRC_POLY * NPK + PL_TUNE, 30},
        {"a SEQ on DRUM's TUNE, the pattern playing", SRC_DRUM, ME_SEQ, MOD_TSRC + SRC_DRUM * NPK + DM_TUNE, 60},
        {"a FOLLOW (its own track) on the channel's FILT", SRC_TAPE, ME_FOLLOW, MOD_TCH + CH_FILT, -80},
    };
    uint32_t k;
    char m[128];
    for (k = 0; k < NELEM(C); k++) {
        double d = mz_diff(C[k].src, C[k].e, C[k].g, C[k].dep);
        snprintf(m, sizeof m, "mod reaches the sound: %s (the difference %.2f dB of it)", C[k].what, d);
        check(m, d > -30.0);
    }
    snprintf(m, sizeof m, "..and a depth of 0 is exactly the sound without one (%s)",
             mz_diff(SRC_TAPE, ME_WAVE, MOD_TG(DEV_COLOR, CP_DRIV), 0) == -999.0 ? "no sample differs" : "SOME DIFFER");
    check(m, strstr(m, "no sample differs") != 0);
    power_on();
}

/* phase 9's rails: under a modulated knob, the reach and a block at the value the sound gets, moving as the LFO
 * does; its own band redraws, not the strip */
static void test_rails(void)
{
    uint32_t f, moves = 0, strip_redraws = 0, seen = 0, sig0, sstrip;
    int32_t set, lo, hi, now, last = -1, nmin = 99, nmax = -1;
    power_on();
    sys.playing = 1;
    press(B_FX);                                         /* COLOR: DRV 50, an LFO a beat on it at depth 40 */
    tp[0].dev[DEV_COLOR][CP_DRIV] = 50;
    tp[0].mod[0][0] = 100;
    tp[0].mod[0][12] = 0;
    mod_nudge(0, 0, MOD_TG(DEV_COLOR, CP_DRIV), 40);
    ui_draw();
    check("rails: none under a knob nothing modulates (CRSH)", !rail_of(1, &set, &lo, &hi, &now));
    check("..DRV's: set at 50 %, its reach both ways for an LFO (10 % .. 90 %)",
          rail_of(0, &set, &lo, &hi, &now) && set == 11 && lo == 2 && hi == 20);
    sstrip = ui.sig_strip;
    sig0 = ui.sig_rail;
    for (f = 0; f < 120u; f++) {                         /* 120 frames (~1.8 s), 20 blocks a frame */
        render(20, 0);
        ui_draw();
        rail_of(0, &set, &lo, &hi, &now);
        if (now != last)
            moves++;
        last = now;
        nmin = now < nmin ? now : nmin;
        nmax = now > nmax ? now : nmax;
        strip_redraws += ui.sig_strip != sstrip;
        sstrip = ui.sig_strip;
        seen |= ui.sig_rail != sig0;
    }
    {
        char m[128];
        snprintf(m, sizeof m, "..the block follows the LFO: %u moves over the reach (%d .. %d), the rails redrawn, the "
                 "strip %u times", moves, nmin, nmax, strip_redraws);
        check(m, moves > 20u && nmin <= 4 && nmax >= 18 && seen && strip_redraws == 0u);
    }
    sys.playing = 0;
    power_on();
}

static void test_source_level(void)
{
    static int32_t a[CTL * 300], b[CTL * 300];
    static const char *const NAME[NSRC] = {"TAPE LVL -12 dB: a quarter of the track's sound",
                                           "SYNTH LVL -12 dB: a quarter", "POLY LVL -12 dB: a quarter",
                                           "DRUM LVL -12 dB: a quarter (its pattern playing)"};
    uint32_t src;
    for (src = 0; src < NSRC; src++) {
        int16_t *lv = src == SRC_TAPE ? &tp[0].dev[DEV_SRC][TK_LVL] : src == SRC_SYNTH ? &tp[0].syn[SY_LVL]
                    : src == SRC_POLY ? &tp[0].pol[PL_LVL] : &tp[0].drm[DM_LVL];
        int32_t pa, pb;
        power_on();
        tp[0].src = (uint8_t)src;
        track[1].mute = track[2].mute = track[3].mute = 1;
        sys.playing = src == SRC_TAPE || src == SRC_DRUM;
        fm1_in.notes = src == SRC_TAPE || src == SRC_DRUM ? 0u : note_bit_of_white(0);
        render(100, 0);
        render(300, a);
        power_on();                                      /* the same again, from power-on, at -12 dB */
        tp[0].src = (uint8_t)src;
        track[1].mute = track[2].mute = track[3].mute = 1;
        sys.playing = src == SRC_TAPE || src == SRC_DRUM;
        fm1_in.notes = src == SRC_TAPE || src == SRC_DRUM ? 0u : note_bit_of_white(0);
        *lv = -12;
        render(100, 0);
        render(300, b);
        fm1_in.notes = 0;
        pa = peak_of(a, CTL * 300u);
        pb = peak_of(b, CTL * 300u);
        check(NAME[src], pa > 2000 && pb * 100 > pa * 22 && pb * 100 < pa * 28);
    }
    check("..and LVL can be modulated (a target on every source)",
          mod_tdesc(MOD_TSRC + SRC_TAPE * NPK + TK_LVL) && mod_tdesc(MOD_TSRC + SRC_SYNTH * NPK + SY_LVL) &&
          mod_tdesc(MOD_TSRC + SRC_POLY * NPK + PL_LVL) && mod_tdesc(MOD_TSRC + SRC_DRUM * NPK + DM_LVL));
    sys.playing = 0;
    power_on();
}

/* the engines' switches, one block at a time through mod_tick (track 1, GRAIN WET from 0 at depth 100: the value is
 * the slot's output in %) */
static int32_t mod_step(uint32_t keys)
{
    static const int32_t zero[CTL];
    const int32_t *last[NTRK] = {zero, zero, zero, zero};
    mod_tick(keys, 0, last);
    return TPD(0, DEV_GRAIN)[GP_WET];
}

static void test_mod_engines(void)
{
    const uint32_t WET = MOD_TG(DEV_GRAIN, GP_WET);
    uint32_t b, i;
    int32_t v, early, late;
    power_on();
    sys.bpm = 120;
    sys.playing = 1;
    /* SEQ's orders: steps 0 33 66 100 at 1/16 (172 blocks a step); a sample from the middle of each step */
    {
        static const int32_t WANT[4][6] = {{0, 33, 66, 100, 0, 33}, {100, 66, 33, 0, 100, 66}, {0, 33, 66, 100, 66, 33}};
        static const char *const NAME[3] = {"mod: SEQ FWD: 1 2 3 4", "mod: SEQ REV: 4 3 2 1", "mod: SEQ PING: 1 2 3 4 3 2"};
        uint32_t d;
        param_engine(0, 2, ME_SEQ);
        tp[0].mod[2][0] = 4;
        tp[0].mod[2][1] = 4;
        for (i = 0; i < 4u; i++)
            tp[0].steps[2][i] = (int8_t)(i * 100u / 3u);
        mod_nudge(0, 2, WET, 100);
        for (d = 0; d < 4u; d++) {
            int ok = 1, seen[4] = {0};
            tp[0].mod[2][4] = (int16_t)d;
            sys.playing = 0;
            mod_step(0);
            sys.playing = 1;
            for (i = 0; i < 6u; i++) {
                for (b = 0; b < 172u; b++)
                    v = mod_step(0);
                if (d < 3u)
                    ok &= v == WANT[d][i];
                else
                    seen[v == 0 ? 0 : v == 33 ? 1 : v == 66 ? 2 : 3]++;
            }
            if (d < 3u)
                check(NAME[d], ok);
            else
                check("mod: SEQ RND: steps from the four, not FWD's", seen[0] + seen[1] + seen[2] + seen[3] == 6 &&
                                                                     !(seen[0] == 2 && seen[1] == 2 && seen[2] == 1));
        }
        tp[0].mod[2][4] = 0;
        tp[0].mod[2][2] = 50;                            /* SLEW 50: halfway through the glide, halfway there */
        sys.playing = 0;
        mod_step(0);
        sys.playing = 1;
        for (b = 0; b < 172u + 43u; b++)
            v = mod_step(0);
        check("mod: SEQ SLEW: a glide into the next step", v > 5 && v < 30);
        for (b = 0; b < 60u; b++)
            v = mod_step(0);
        check("..arriving at it", v == 33);
        tp[0].mod[2][2] = 0;
        tp[0].mod[2][3] = 100;                           /* SWNG 100: step 2 starts a third of a step late */
        sys.playing = 0;
        mod_step(0);
        sys.playing = 1;
        for (b = 0; b < 172u + 30u; b++)
            v = mod_step(0);
        check("mod: SEQ SWNG: the off-beat starts late", v == 0);
        for (b = 0; b < 40u; b++)
            v = mod_step(0);
        check("..then plays", v == 33);
        mod_clear(0);
    }
    /* ADSR LOOP: a key held, attack and decay come round again (the level climbs after reaching SUS) */
    {
        uint32_t rises = 0, low = 1;
        param_engine(0, 1, ME_ADSR);
        tp[0].mod[1][0] = 20;
        tp[0].mod[1][1] = 20;
        tp[0].mod[1][2] = 20;
        tp[0].mod[1][9] = 1;
        mod_nudge(0, 1, WET, 100);
        for (b = 0; b < 600u; b++) {
            v = mod_step(1u);
            if (low && v > 90) {                         /* up to the top from SUS (20 %): one more time round */
                rises++;
                low = 0;
            } else if (v <= 25) {
                low = 1;
            }
        }
        check("mod: ADSR LOOP: attack and decay again while the key is held", rises >= 3u);
        mod_clear(0);
    }
    /* LFO FADE 100: the first cycle fades in (TRIG KEY restarts it) */
    {
        param_engine(0, 0, ME_WAVE);
        tp[0].mod[0][0] = 60;                            /* ~0.5 Hz: a cycle in ~2700 blocks */
        tp[0].mod[0][12] = 0;
        tp[0].mod[0][13] = 1;
        tp[0].mod[0][14] = 100;
        tp[0].mod[0][1] = 2;                             /* SQR: full swing from the start, but for FADE */
        tp[0].ch[CH_PAN] = 0;
        mod_nudge(0, 0, MOD_TCH + CH_PAN, 50);
        mod_step(0);
        mod_step(1u);
        early = TPD(0, MA_CH)[CH_PAN];
        for (b = 0; b < 3000u; b++)
            mod_step(1u);
        late = TPD(0, MA_CH)[CH_PAN];
        check("mod: LFO FADE: quiet at the restart, full a cycle on", early > -10 && early < 10 && (late > 85 || late < -85));
        mod_clear(0);
    }
    /* LFO VAR on RND: the steps drift each time round; on a shape: each cycle's level */
    {
        int16_t was[16];
        uint32_t moved = 0, k;
        param_engine(0, 0, ME_WAVE);
        tp[0].mod[0][1] = LFO_RND;
        tp[0].mod[0][0] = 127;
        tp[0].mod[0][12] = 0;
        tp[0].mod[0][6] = 80;
        mod_nudge(0, 0, WET, 50);
        mod_step(0);
        for (k = 0; k < 16u; k++)
            was[k] = mrt[0][0].rnd[k];
        for (b = 0; b < 400u; b++)
            mod_step(0);
        for (k = 0; k < 16u; k++)
            moved += mrt[0][0].rnd[k] != was[k];
        check("mod: LFO VAR on RND: the steps drift from time round to time round", moved >= 12u);
        tp[0].mod[0][1] = 0;                             /* SIN, VAR 80, LEN 2: two cycles' levels */
        tp[0].mod[0][7] = 2;
        for (b = 0, v = 0; b < 400u; b++) {
            int32_t x = mod_step(0);
            v = x > v ? x : v;
        }
        check("..on SIN: the cycles' levels vary (never all at full)", v > 10 && v < 50);
        mod_clear(0);
    }
    /* FOLLOW HOLD 1/4: the envelope sampled once a beat (689 blocks at 120 BPM) */
    {
        static int32_t loud[CTL], quiet[CTL];
        const int32_t *last[NTRK] = {quiet, loud, quiet, quiet};
        uint32_t changes = 0;
        int32_t prev = -1;
        for (i = 0; i < CTL; i++)
            loud[i] = 20000;
        param_engine(0, 3, ME_FOLLOW);
        tp[0].mod[3][0] = 2;
        tp[0].mod[3][4] = 4;
        mod_nudge(0, 3, WET, 100);
        for (b = 0; b < 689u * 3u; b++) {
            last[1] = (b / 100u) & 1u ? loud : quiet;    /* (a source pulsing every 100 blocks) */
            mod_tick(0, 0, last);
            v = TPD(0, DEV_GRAIN)[GP_WET];
            changes += prev >= 0 && v != prev;
            prev = v;
        }
        check("mod: FOLLOW HOLD 1/4: the level moves only on the beat", changes >= 1u && changes <= 3u);
    }
    sys.playing = 0;
    power_on();
    /* the pitch knobs modulate finer than a semitone: an LFO on TUNE at a small depth is vibrato, not a trill */
    {
        uint32_t b, fine = 0, f;
        int32_t lo = 1 << 20, hi = -(1 << 20);
        power_on();
        tp[0].src = SRC_SYNTH;
        tp[0].mod[0][0] = 100;                           /* ~6 Hz */
        tp[0].mod[0][12] = 0;
        mod_nudge(0, 0, MOD_TSRC + SRC_SYNTH * NPK + SY_TUNE, 1);   /* 1 % of 48 semitones: about half a semitone */
        for (b = 0; b < 400u; b++) {
            static const int32_t zero[CTL];
            const int32_t *last[NTRK] = {zero, zero, zero, zero};
            mod_tick(0, 0, last);
            f = (uint32_t)mod_pitch16(0, FN_SYN, TPD(0, MA_SYN)[SY_TUNE]);
            fine += (f & 15u) != 0u;
            lo = (int32_t)f < lo ? (int32_t)f : lo;
            hi = (int32_t)f > hi ? (int32_t)f : hi;
        }
        check("mod: TUNE modulates in 1/16 semitones (vibrato: about +-half a semitone, between the semitones)",
              fine > 100u && lo >= -9 && lo <= -7 && hi >= 7 && hi <= 9);
        check("..and reads as the knob x 16 when nothing modulates it", mod_pitch16(1, FN_SYN, 5) == 80);
        mod_clear(0);                                    /* a SEQ on TUNE: its steps stay on semitones */
        param_engine(0, 2, ME_SEQ);
        tp[0].mod[2][0] = 4;
        tp[0].mod[2][1] = 4;
        tp[0].steps[2][0] = 15;                          /* 15 % of 48: 7.2 semitones, a fifth */
        tp[0].steps[2][1] = 21;
        tp[0].steps[2][2] = 6;
        tp[0].steps[2][3] = 25;
        mod_nudge(0, 2, MOD_TSRC + SRC_SYNTH * NPK + SY_TUNE, 100);
        sys.playing = 1;
        for (b = 0, fine = 0; b < 800u; b++) {
            static const int32_t zero[CTL];
            const int32_t *last[NTRK] = {zero, zero, zero, zero};
            mod_tick(0, 0, last);
            f = (uint32_t)mod_pitch16(0, FN_SYN, TPD(0, MA_SYN)[SY_TUNE]);
            fine += (f & 15u) != 0u;
        }
        sys.playing = 0;
        check("..a SEQ on TUNE lands on semitones (a step is a note)", fine == 0u && f % 16u == 0u);
    }
    /* the filters' cutoffs modulate finer than their semitones: a slow sweep moves a little every block, never in
     * whole-step jumps (SYNTH's CUT, POLY's, RIPPLE's, the channel's FILT) */
    {
        static const uint32_t FNS[4] = {FN_SCUT, FN_PCUT, FN_RCUT, FN_FILT};
        static const char *const NAME[4] = {"mod: SYNTH's CUT sweeps in 1/256 steps (no semitone staircase)",
                                            "..POLY's CUT", "..RIPPLE's CUT", "..the channel's FILT (1/256 of its 1 % steps)"};
        uint32_t w, b;
        for (w = 0; w < 4u; w++) {
            uint32_t g = w == 0u ? MOD_TSRC + SRC_SYNTH * NPK + SY_CUT : w == 1u ? MOD_TSRC + SRC_POLY * NPK + PL_CUT
                       : w == 2u ? MOD_TG(DEV_RESO, RP_CUT) : MOD_TCH + CH_FILT;
            uint32_t k, a = mod_tarr(g, &k), frac = 0, big = 0;
            int32_t prev = 0;
            power_on();
            mod_base(0, a)[k] = (int16_t)(w == 3u ? -40 : 60);
            tp[0].mod[0][0] = 20;                        /* a slow triangle */
            tp[0].mod[0][1] = 1;
            tp[0].mod[0][12] = 0;
            mod_nudge(0, 0, g, 20);
            for (b = 0; b < 2000u; b++) {
                static const int32_t zero[CTL];
                const int32_t *last[NTRK] = {zero, zero, zero, zero};
                int32_t v;
                mod_tick(0, 0, last);
                v = mod_fine8(0, FNS[w], TPD(0, a)[k]);
                frac += (v & 255) != 0;
                if (b && (v - prev > 128 || prev - v > 128))
                    big++;
                prev = v;
            }
            check(NAME[w], frac > 1500u && big == 0u && (mod_fmask[0] >> FNS[w]) & 1u);
        }
        check("..and a cutoff reads as the knob x 256 when nothing modulates it", mod_fine8(1, FN_SCUT, 64) == 64 * 256);
        power_on();
    }
    /* RIPPLE's root glides when PTCH moves (half the way a block), so the strings' read point never jumps */
    {
        int32_t r0, r1;
        power_on();
        reso_poll();
        tp[0].dev[DEV_RESO][RP_WET] = 60;
        reso_poll();
        reso_poll();
        reso_poll();
        reso_poll();
        render(20, 0);
        r0 = reso[0].rs16;
        tp[0].dev[DEV_RESO][RP_PTCH] += 12;
        render(1, 0);
        r1 = reso[0].rs16;
        render(30, 0);
        check("RIPPLE: PTCH +12 glides there (half the way in a block, all of it soon after)",
              reso[0].nch > 0u && r1 > r0 && r1 < r0 + 12 * 16 && reso[0].rs16 >= r0 + 12 * 16 - 1);
        power_on();
    }
    /* TUNE moves a held note (SYNTH and POLY add it as the voice plays, not when the key went down) */
    {
        static int32_t buf[CTL * 400];
        uint32_t src, i;
        for (src = SRC_SYNTH; src <= SRC_SYNTH; src++) {
            uint32_t up0 = 0, up1 = 0;
            int16_t *tune = src == SRC_SYNTH ? &tp[0].syn[SY_TUNE] : &tp[0].pol[PL_TUNE];
            power_on();
            tp[0].src = (uint8_t)src;
            track[1].mute = track[2].mute = track[3].mute = 1;
            fm1_in.notes = note_bit_of_white(0);
            render(100, 0);
            render(400, buf);
            for (i = 1; i < CTL * 400u; i++)
                up0 += buf[i - 1] < 0 && buf[i] >= 0;
            *tune = 12;
            render(20, 0);
            render(400, buf);
            for (i = 1; i < CTL * 400u; i++)
                up1 += buf[i - 1] < 0 && buf[i] >= 0;
            fm1_in.notes = 0;
            if (src == SRC_SYNTH)
                check("SYNTH: TUNE +12 on a held note: an octave up (TUNE is live, so it can be modulated)",
                      up0 > 20u && up1 * 10u > up0 * 17u && up1 * 10u < up0 * 23u);
        }
        {                                                /* POLY: the voice's head steps twice as fast */
            int32_t i0;
            power_on();
            tp[0].src = SRC_POLY;
            fm1_in.notes = note_bit_of_white(0);
            render(4, 0);
            i0 = pol[0].v[0].inc;
            tp[0].pol[PL_TUNE] = 12;
            render(1, 0);
            fm1_in.notes = 0;
            check("POLY: TUNE +12 on a held note: an octave up", i0 > 0 && pol[0].v[0].stage &&
                                                                 pol[0].v[0].inc >= 2 * i0 - 2 && pol[0].v[0].inc <= 2 * i0 + 2);
        }
        power_on();
    }
}

/* track t's synth has a voice playing (not releasing) note16 */
static int syn_plays(uint32_t t, int32_t note16)
{
    uint32_t j;
    for (j = 0; j < SYN_NV; j++)
        if ((syn[t].v[j].stage == 1u || syn[t].v[j].stage == 2u) && syn[t].v[j].to16 == note16)
            return 1;
    return 0;
}

/* the notes the arpeggiator plays on track t over blocks: each step's note as it starts, into seq (up to max);
 * how many; and whether every step had a silent block before the next (*gaps) */
static uint32_t arp_steps(uint32_t t, uint32_t blocks, uint8_t *seq, uint32_t max, int *gaps)
{
    uint32_t b, n = 0, was = arp_rt[t].on, quiet = !was;   /* (a step already sounding isn't counted) */
    *gaps = 1;
    for (b = 0; b < blocks; b++) {
        uint32_t on;
        render(1, 0);
        on = arp_rt[t].on;
        if (on && !was) {
            if (n && !quiet)
                *gaps = 0;
            if (n < max)
                seq[n] = arp_rt[t].cur;
            n++;
            quiet = 0;
        }
        if (!on)
            quiet = 1;
        was = on;
    }
    return n;
}

/* the ARP pad's arpeggio on TAPE (its slices) and DRUM (its drums), and RIPPLE's own (its page 3) */
static void test_arp_sources(void)
{
    uint8_t seq[32];
    uint32_t n, b, w1 = note_bit_of_white(0), w3 = note_bit_of_white(2), w5 = note_bit_of_white(4), hit = 1, roots = 0;
    int gaps, quiet = 1;
    uint32_t ran = 0;
    int32_t last = -1;
    power_on();
    sys.bpm = 120;
    tp[0].src = SRC_TAPE;
    tp[0].dev[DEV_SRC][TK_LEN] = 2500;                     /* (slices: the keys as on the TAPE page) */
    tp[0].arp[AR_MODE] = ARM_KEY;
    focus_dev(DEV_SRC, 0);
    fm1_in.notes = w1 | w5;
    n = arp_steps(0, 172 * 4 + 20, seq, 32, &gaps);
    check("TAPE, ARP KEY UP: slices 1 5 1 5, a sixteenth each", n >= 4u && seq[0] == 0u && seq[1] == 4u && seq[2] == 0u &&
          seq[3] == 4u && gaps);
    check("..the tape plays them", tape_rt[0].running);
    tp[0].arp[AR_RNGE] = 3;
    arp_rt[0].pos = 0;
    n = arp_steps(0, 172 * 4, seq, 32, &gaps);
    check("..RNGE 3 spreads nothing (no octaves of a slice): still 1 5 1 5", n >= 4u && seq[0] == 0u && seq[1] == 4u &&
          seq[2] == 0u && seq[3] == 4u);
    fm1_in.notes = 0;
    render(4, 0);
    check("..the keys let go: it stops", arp_rt[0].n == 0u && !arp_rt[0].on);
    fm1_in.notes = w1;
    render(4, 0);
    fm1_in.notes = 0;
    tp[0].arp[AR_MODE] = ARM_OFF;
    render(4, 0);

    power_on();
    sys.bpm = 120;
    tp[0].src = SRC_DRUM;
    tp[0].drm[DM_PATN] = 0;
    tp[0].arp[AR_MODE] = ARM_KEY;
    tp[0].arp[AR_ORDR] = ARO_UPDN;
    focus_dev(DEV_SRC, 0);
    fm1_in.notes = w1 | w3 | w5;
    n = arp_steps(0, 172 * 5 + 20, seq, 32, &gaps);
    check("DRUM, ARP KEY UPDN: drums 1 3 5 3 1, up and down", n >= 5u && seq[0] == 0u && seq[1] == 2u && seq[2] == 4u &&
          seq[3] == 2u && seq[4] == 0u && gaps);
    for (b = 0; b < 172u * 3u; b++) {                    /* each step: the drum's key, and only it, into the kit */
        render(1, 0);
        if (arp_rt[0].on)
            hit &= drm_rt[0].keys == 1u << arp_rt[0].cur;
        else
            hit &= drm_rt[0].keys == 0u;
    }
    check("..each step strikes that drum alone (the kit sees one key a step)", hit);
    press(B_ARP);
    shot("BRYO", "arp_drum");
    fm1_in.notes = 0;
    render(4, 0);
    tp[0].arp[AR_MODE] = ARM_OFF;

    {                                                    /* RIPL ARP's page shows RIPPLE's knobs 12-15 */
        uint32_t k, same = pdesc_pages(ARP_P) == 3u;
        for (k = 0; k < 4u; k++) {
            const pdesc_t *a = &ARP_P[ARP_RES + k], *d = &DEV_P[DEV_RESO][RP_AMODE + k];
            same &= !strcmp(a->label, d->label) && a->min == d->min && a->max == d->max && a->def == d->def &&
                    a->fmt == d->fmt;
        }
        check("RIPL ARP (the ARP pad's page 3) describes RIPPLE's knobs 12-15 as they're stored", same);
    }
    power_on();                                          /* RIPPLE's arpeggio on a TAPE track */
    sys.bpm = 120;
    tp[0].src = SRC_TAPE;
    tp[0].dev[DEV_SRC][TK_LEN] = 2500;                     /* (slices: a key would start one, but not here) */
    tp[0].dev[DEV_RESO][RP_WET] = 100;
    tp[0].dev[DEV_RESO][RP_AMODE] = ARM_KEY;
    hold(B_ARP);                                         /* ARP held + key 3: RIPL ARP */
    key_edge(note_of_white(2));
    let_go(B_ARP);
    check("ARP held + key 3: RIPL ARP, RIPPLE's arpeggio; the keys its", ui.kind == FOCUS_ARP && ui.page == 2u &&
          sys.keys_rarp && !sys.keys_reso);
    fm1_in.notes = w1 | w5;
    for (b = 0; b < 172u * 5u; b++) {
        render(1, 0);
        if (reso_arp_on[0] && quiet && reso[0].root16 != last) {
            roots |= reso[0].root16 == (48 + 0) * 16 ? 1u : reso[0].root16 == (48 + 4) * 16 ? 2u : 4u;
            last = reso[0].root16;
        }
        quiet = !reso_arp_on[0];
        ran |= tape_rt[0].running;
    }
    check("..KEY, two keys held: the strings plucked at C3 and E3 in turn", roots == 3u && reso_arp[0].n == 2u);
    shot("BRYO", "arp_res");
    hold(B_SCL);                                         /* (SCL + the ARP pad: RIPL ARP to its defaults: off) */
    press(B_ARP);
    let_go(B_SCL);
    check("..SCL held + ARP on RIPL ARP: its knobs to their defaults (MODE OFF), the track's ARP left alone",
          tp[0].dev[DEV_RESO][RP_AMODE] == ARM_OFF && tp[0].dev[DEV_RESO][RP_ARAT] == 3 && ui.page == 2u);
    tp[0].dev[DEV_RESO][RP_AMODE] = ARM_KEY;
    check("..the tape stays quiet (the keys are the strings' on this page)", !ran && arp_rt[0].n == 0u);
    fm1_in.notes = 0;
    render(4, 0);
    check("..let go: the arpeggio stops", reso_arp[0].n == 0u && !reso_arp_on[0]);
    tp[0].dev[DEV_RESO][RP_AMODE] = ARM_LTCH;
    tp[0].dev[DEV_RESO][RP_ARNG] = 2;
    fm1_in.notes = w1;
    render(4, 0);
    fm1_in.notes = 0;
    focus_dev(DEV_SRC, 0);
    ui_input();
    roots = 0;
    for (b = 0; b < 172u * 3u; b++) {
        render(1, 0);
        roots |= reso[0].root16 == 48 * 16 ? 1u : reso[0].root16 == 60 * 16 ? 2u : 0u;
    }
    check("..LTCH, RNGE 2, off its page: C3 and C4 go on plucking", roots == 3u && reso_arp[0].n == 1u);
    hold(B_ARP);
    key_edge(note_of_white(2));
    let_go(B_ARP);
    fm1_in.notes = 1u << black_note(BK_POLY);
    key_edge(black_note(BK_POLY));
    fm1_ms += 600;
    ui_input();
    fm1_in.notes = 0;
    render(2, 0);
    check("..POLY held on RIPL ARP: its notes cleared, the track's arpeggio left alone", reso_arp[0].n == 0u &&
          !strcmp(ui.msg, "RIPL ARP NOTES CLEARED"));
    tp[0].dev[DEV_RESO][RP_AMODE] = ARM_OFF;
    render(4, 0);
}

static void test_arp_keys(void)
{
    uint8_t seq[32];
    uint32_t n, i, c, e3 = note_bit_of_white(4), c3 = note_bit_of_white(0);
    int gaps, ok;
    int16_t v;
    power_on();
    tp[0].src = SRC_SYNTH;
    sys.bpm = 120;
    focus_dev(DEV_SRC, 0);
    press(B_ARP);
    turn(0, 1);                                          /* MODE: KEY */
    check("ARP on a SYNTH track: KNOB 1 turns MODE to KEY", tp[0].arp[AR_MODE] == ARM_KEY && ui.kind == FOCUS_ARP);
    fm1_in.notes = c3 | e3;                              /* C3 and E3 held */
    n = arp_steps(0, 900, seq, 32, &gaps);               /* (about 0.65 s: 1/16 at 120 is 172 blocks) */
    check("KEY, UP, 1/16: C3 E3 C3 E3, a sixteenth each, a silent block between steps",
          n >= 4u && n <= 6u && seq[0] == 48u && seq[1] == 52u && seq[2] == 48u && seq[3] == 52u && gaps);
    check("..the synth plays them", syn_sounding(0) > 0);
    shot("BRYO", "arp");
    tp[0].arp[AR_RNGE] = 2;
    tp[0].arp[AR_ORDR] = ARO_UPDN;
    arp_rt[0].pos = 0;
    n = arp_steps(0, 172 * 7, seq, 32, &gaps);
    check("RNGE 2, UPDN: C3 E3 C4 E4 C4 E3, then round", n >= 6u && seq[0] == 48u && seq[1] == 52u && seq[2] == 60u &&
          seq[3] == 64u && seq[4] == 60u && seq[5] == 52u);
    tp[0].arp[AR_RNGE] = 1;
    tp[0].arp[AR_ORDR] = ARO_DOWN;
    arp_rt[0].pos = 0;
    n = arp_steps(0, 172 * 3, seq, 32, &gaps);
    check("DOWN: E3 C3 E3", n >= 3u && seq[0] == 52u && seq[1] == 48u && seq[2] == 52u);
    fm1_in.notes = 0;
    render(4, 0);
    check("KEY: the keys let go, the arpeggio stops", arp_rt[0].n == 0u && !arp_rt[0].on);
    tp[0].arp[AR_MODE] = ARM_LTCH;
    tp[0].arp[AR_ORDR] = ARO_UP;
    fm1_in.notes = c3 | e3;
    render(4, 0);
    fm1_in.notes = 0;
    n = arp_steps(0, 172 * 3, seq, 32, &gaps);
    check("LTCH: the chord let go goes on playing", arp_rt[0].n == 2u && n >= 2u);
    fm1_in.notes = note_bit_of_white(7);                 /* G3: a new chord after letting go */
    render(4, 0);
    fm1_in.notes = 0;
    n = arp_steps(0, 172 * 2, seq, 32, &gaps);
    check("..a key after letting go: a new chord (G3 alone)", arp_rt[0].n == 1u && n >= 1u && seq[0] == 55u);
    tp[0].arp[AR_MODE] = ARM_ADD;
    render(1, 0);                                        /* (LTCH -> ADD keeps the notes) */
    for (i = 0; i < 3u; i++) {                           /* C3, E3, A3, one after the other */
        fm1_in.notes = note_bit_of_white(i == 0 ? 0u : i == 1u ? 4u : 9u);
        render(2, 0);
        fm1_in.notes = 0;
        render(2, 0);
    }
    check("ADD: every key pressed joins, let go or not (G3, then C3 E3 A3)", arp_rt[0].n == 4u);
    shot("BRYO", "arp_add");
    fm1_in.notes = 1u << black_note(BK_POLY);
    key_edge(black_note(BK_POLY));
    fm1_ms += 600;
    ui_input();
    fm1_in.notes = 0;
    render(2, 0);
    check("..POLY held on the ARP page: the notes cleared, the tape left alone", arp_rt[0].n == 0u &&
          !strcmp(ui.msg, "ARP NOTES CLEARED"));
    tp[0].arp[AR_MODE] = ARM_OFF;
    fm1_in.notes = c3;
    render(4, 0);
    check("MODE OFF: the keys straight through", key_base[0] == 48u && syn_sounding(0) > 0);
    fm1_in.notes = 0;
    render(4, 0);
    tp[0].src = SRC_TAPE;
    tp[0].arp[AR_MODE] = ARM_KEY;
    fm1_in.notes = c3;
    render(4, 0);
    check("a TAPE track: the arpeggio holds the slice (white key 1: slice 1, not a note)", arp_rt[0].n == 1u &&
          arp_rt[0].note[0] == 0u);
    fm1_in.notes = 0;
    render(2, 0);
    tp[0].arp[AR_MODE] = ARM_OFF;

    /* the black keys as notes: GLO held + OCT+ */
    tp[0].src = SRC_SYNTH;
    keyp[KY_SCAL] = 0;                                   /* (every note as it is: A MIN would play F for F#) */
    focus_dev(DEV_SRC, 0);
    hold(B_GLO);
    press(B_OCTUP);
    let_go(B_GLO);
    check("GLO held + OCT+: the black keys play notes (kept as a setting), and it says so", sys.keys_notes == 1u &&
          (bryo_prefs & BPREF_NOTES) && !strcmp(ui.msg, "BLACK KEYS PLAY NOTES") && ui.view == VIEW_PAGE);
    fm1_in.notes = 1u << black_note(BK_OP1);
    key_edge(black_note(BK_OP1));
    render(4, 0);
    check("..black OP1 doesn't mute: it plays F#3 (SCAL OFF; the keyboard from F3)", !track[0].mute &&
          key_base[0] == 53u && syn_plays(0, (53 + (int32_t)black_note(BK_OP1)) * 16) && black_note(BK_OP1) == 1u);
    fm1_in.notes = 0;
    render(4, 0);
    fm1_in.notes = c3;                                   /* white key 1, F3 now */
    render(4, 0);
    check("..white key 1 plays F3", syn_plays(0, 53 * 16));
    fm1_in.notes = 0;
    render(4, 0);
    hold(B_GLO);                                         /* GLO held: the black keys are the shortcuts again */
    key_edge(black_note(BK_OP2));
    check("..GLO held + OP2: track 2 muted (the shortcuts come back under GLO)", track[1].mute == 1u);
    ui_leds();
    led_on(fm1_led, 14u + black_note(BK_OP2));
    check("..and its LED shows it while GLO is held", led_lit == 1u && sys.black_short == 1u);
    let_go(B_GLO);
    ui_leds();
    led_on(fm1_led, 14u + black_note(BK_OP2));
    check("..GLO let go: the OP keys are keys again (no mute lit), the 0 key won't freeze", led_lit == 0u &&
          sys.black_short == 0u);
    {
        uint32_t z = black_note(BK_ZERO);
        fm1_in.notes = 1u << z;
        render(2, 0);
        check("..the 0 key held as a note: GRAIN isn't frozen", !grain_frozen);
        fm1_in.notes = 0;
        render(2, 0);
    }
    hold(B_GLO);
    key_edge(black_note(BK_OP2));
    let_go(B_GLO);
    check("..GLO + OP2 again: unmuted", track[1].mute == 0u);
    hold(B_LFO);                                         /* the LFO pad's picks still work */
    key_edge(black_note(BK_OP2));
    let_go(B_LFO);
    check("..LFO held + OP2 still picks M2", ui.lfo_slot == 1u && ui.slot == 1u);
    hold(B_GLO);
    press(B_OCTUP);
    let_go(B_GLO);
    check("GLO held + OCT+ again: the shortcuts back", !sys.keys_notes && !(bryo_prefs & BPREF_NOTES));
    key_edge(black_note(BK_OP1));
    check("..OP1 mutes again", track[0].mute == 1u);
    key_edge(black_note(BK_OP1));

    /* the LFO pad's slot: depths go to the one it shows */
    hold(B_LFO);
    key_edge(black_note(BK_OP2));
    press(B_FX);                                         /* (COLOR, while LFO is held) */
    let_go(B_LFO);
    focus_dev(DEV_COLOR, 0);
    hold(B_LFO);
    turn(1, 5);
    let_go(B_LFO);
    check("LFO pad on M2: a knob turned while it's held sets M2's depth", mod_dep(0, 1, MOD_TG(DEV_COLOR, 1)) == 5 &&
          mod_dep(0, 0, MOD_TG(DEV_COLOR, 1)) == 0);
    mod_clear(0);
    hold(B_LFO);
    key_edge(black_note(BK_OP1));
    let_go(B_LFO);
    focus_dev(DEV_COLOR, 0);

    /* SETUP: the mixer's last page; KNOB's steps */
    tap(B_GLO);
    tap(B_EDIT);
    tap(B_EDIT);
    tap(B_EDIT);
    tap(B_EDIT);
    tap(B_EDIT);
    check("SETUP: the mixer's last page (GLO, EDIT x5: SCENES CHANNEL MASTER KEY SETUP)", ui.view == VIEW_MIXER &&
          ui.chan == CHAN_SETUP);
    turn(1, 2);                                          /* KNOB: 1X -> 4X */
    check("..KNOB 2 sets KNOB: 4X", ((bryo_prefs >> 1) & 3u) == KNOB_4X);
    ui.msg_t = 0;                                        /* (the picture under the message) */
    shot("BRYO", "setup");
    tap(B_GLO);
    focus_dev(DEV_COLOR, 0);
    v = tp[0].dev[DEV_COLOR][0];
    turn(0, 1);
    check("..4X: one detent of DRV (0-100) moves it 4", tp[0].dev[DEV_COLOR][0] == v + 4);
    focus_dev(DEV_COLOR, 2);
    c = (uint32_t)tp[0].dev[DEV_COLOR][6];
    turn(0, 1);
    check("..a choice (CMOD) still moves one a detent", (uint32_t)tp[0].dev[DEV_COLOR][6] == c + 1u);
    bryo_prefs = (uint8_t)((bryo_prefs & ~6u) | KNOB_ACC << 1);
    focus_dev(DEV_COLOR, 0);
    tp[0].dev[DEV_COLOR][0] = 0;
    for (ok = 1, i = 0; i < 4u; i++) {                  /* a fast turn: a detent every 10 ms */
        fm1_ms += 10;
        turn(0, 1);
    }
    check("ACC: a fast turn goes further than a detent a step", tp[0].dev[DEV_COLOR][0] > 4);
    v = tp[0].dev[DEV_COLOR][0];
    fm1_ms += 500;
    turn(0, 1);
    fm1_ms += 500;
    turn(0, 1);
    check("..a slow one, a step a detent", tp[0].dev[DEV_COLOR][0] == v + 2);
    bryo_prefs = 0;
    bprefs_apply();
    (void)ok;

    /* projects keep the arpeggiator (version 3) */
    power_on();
    pj_wipe();
    pj_boot();
    tp[2].src = SRC_POLY;
    tp[2].arp[AR_MODE] = ARM_LTCH;
    tp[2].arp[AR_ORDR] = ARO_RND;
    tp[2].arp[AR_RATE] = 1;
    tp[2].arp[AR_SWNG] = 30;
    check("a project saved", pj_save(0) == 0);
    power_on();
    pj_boot();
    check("..loaded: the arpeggiator as it was", tp[2].arp[AR_MODE] == ARM_LTCH && tp[2].arp[AR_ORDR] == ARO_RND &&
          tp[2].arp[AR_RATE] == 1 && tp[2].arp[AR_SWNG] == 30 && tp[0].arp[AR_MODE] == ARM_OFF);
    pj_wipe();
}

/* the factory scene: what plays with nothing saved, and FACTORY in the project view */
static uint32_t distinct_add(int32_t *seen, uint32_t n, int32_t v)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (seen[i] == v)
            return n;
    if (n < 64u)
        seen[n++] = v;
    return n;
}

static void test_factory_scene(void)
{
    int32_t seed[64], an2[64], an4[64], tu[64], rp[64], pk = 0;
    uint32_t nseed = 0, n2 = 0, n4 = 0, ntu = 0, nrp = 0, b, i, inkey = 1, steps2 = 0, was2 = 0;
    char m[200];
    power_on();
    pj_wipe();
    pj_boot();
    check("nothing saved: the factory scene (T1 DRUM on FUNK, T2 POLY on BELL and T4 on FOLD, both arpeggiated, T3 "
          "TAPE on DRON), nothing changed yet",
          tp[0].src == SRC_DRUM && tp[0].drm[DM_PATN] == 2 && tp[1].src == SRC_POLY && tp[1].pol[PL_REEL] == 2 &&
          tp[3].src == SRC_POLY && tp[3].pol[PL_REEL] == 4 && tp[2].src == SRC_TAPE && tp[2].dev[DEV_SRC][TK_REEL] == 3 &&
          tp[1].arp[AR_MODE] == ARM_LTCH && tp[3].arp[AR_MODE] == ARM_LTCH && !pj_changed() && pj_cur == 0u);
    render(200, 0);
    check("..stopped: the arpeggiators hold their notes and wait for PLAY", arp_rt[1].n == 4u && arp_rt[3].n == 5u &&
          !arp_rt[1].on && !arp_rt[3].on && arp_rt[1].waiting);
    sys.playing = 1;
    for (b = 0; b < 2756u * 8u; b++) {                   /* eight bars at 120 */
        chain_block(out, CTL);
        for (i = 0; i < 2u * CTL; i++)
            pk = out[i] > pk ? out[i] : -out[i] > pk ? -out[i] : pk;
        if (arp_rt[1].on && !was2) {                     /* each of T2's notes as it sounds: in A minor? */
            int32_t tr = TPD(1, MA_POL)[PL_TUNE], pc = ((int32_t)arp_rt[1].cur + tr) % 12;   /* (TUNE: the chords) */
            inkey &= pc == 9 || pc == 11 || pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7;
            n2 = distinct_add(an2, n2, (int32_t)arp_rt[1].cur + tr);
            steps2++;
        }
        was2 = arp_rt[1].on;
        if (b % 64u == 0u) {
            nseed = distinct_add(seed, nseed, TPD(0, MA_DRM)[DM_SEED]);
            ntu = distinct_add(tu, ntu, TPD(1, MA_POL)[PL_TUNE]);
            nrp = distinct_add(rp, nrp, TPD(2, DEV_RESO)[RP_PTCH]);
            n4 = distinct_add(an4, n4, (int32_t)arp_rt[3].cur + TPD(3, MA_POL)[PL_TUNE]);
        }
    }
    snprintf(m, sizeof m, "..eight bars: the drums' SEED takes %u versions, T2's arpeggio %u steps over %u notes and %u "
             "chords, T3's strings %u roots, T4's arpeggio %u notes; peak %d", nseed, steps2, n2, ntu, nrp, n4, pk);
    check(m, nseed >= 3u && steps2 >= 60u && n2 >= 8u && ntu == 3u && nrp >= 4u && n4 >= 6u && pk < 32767);
    check("..every note T2's arpeggio plays is in A minor (Am7 Dm7 Em7)", inkey);
    sys.playing = 0;
    render(4, 0);
    check("..STOP: the arpeggios stop, their notes kept", !arp_rt[1].on && !arp_rt[3].on && arp_rt[1].n == 4u);
    check("a project saved with the scene keeps the arpeggiators' notes", pj_save(1) == 0);
    power_on();
    pj_boot();
    render(2, 0);
    check("..loaded: T2 holds Am7 again, T4 the pentatonic, waiting for PLAY", arp_rt[1].n == 4u && arp_rt[1].note[0] == 57u &&
          arp_rt[1].note[3] == 67u && arp_rt[3].n == 5u && arp_rt[1].waiting && pj_cur == 1u);
    pj_wipe();
    power_on();
    pj_boot();

    /* FACTORY in the project view, over a saved project */
    tp[2].dev[DEV_COLOR][CP_DRIV] = 60;
    check("a project saved into P1 (the scene, changed)", pj_save(0) == 0);
    power_on();
    pj_boot();
    check("..power-on: P1 back (not the scene)", tp[2].dev[DEV_COLOR][CP_DRIV] == 60 && pj_cur == 0u);
    host_enc[panel.enc[EN_PRESET]] = 1;
    ui_input();
    for (i = 0; i < PJ_N; i++) {
        host_enc[panel.enc[EN_PRESET]] = 1;
        ui_input();
    }
    check("PRESETS turned past P6: FACTORY", ui.view == VIEW_PROJECT && ui.pj_sel == PJ_N);
    shot("BRYO", "projects_factory");
    press(B_OCTUP);
    check("..OCT+: the factory scene loaded, P1 left as saved, no slot current", tp[0].src == SRC_DRUM &&
          tp[2].dev[DEV_COLOR][CP_DRIV] == 0 && pj_cur == PJ_N && pj_sum[0].used && !strcmp(ui.msg, "FACTORY SCENE LOADED"));
    tap(B_SAVE);
    check("..SAVE on FACTORY: it says to save into a slot", !strcmp(ui.msg, "FACTORY: SAVE INTO P1..P6") && pj_cur == PJ_N);
    fm1_in.notes = 1u << black_note(BK_POLY);
    key_edge(black_note(BK_POLY));
    fm1_ms += 600;
    ui_input();
    fm1_in.notes = 0;
    check("..POLY held on FACTORY: it stays", !strcmp(ui.msg, "THE FACTORY SCENE STAYS"));
    host_enc[panel.enc[EN_PRESET]] = 1;                 /* TEST, past FACTORY: the scene with every stage at work */
    ui_input();
    host_enc[panel.enc[EN_PRESET]] = 1;
    ui_input();
    check("PRESETS turned past FACTORY: TEST, and no further", ui.view == VIEW_PROJECT && ui.pj_sel == PJ_TEST);
    ui.msg_t = 0;                                        /* (the picture, not FACTORY's message) */
    shot("BRYO", "projects_test");
    press(B_OCTUP);
    check("..OCT+: the TEST scene loaded (FACTORY's music, T1's drums through GRAIN)",
          tp[0].src == SRC_DRUM && tp[0].dev[DEV_GRAIN][GP_WET] == 30 && tp[2].dev[DEV_RESO][RP_WET] > 0 &&
          pj_cur == PJ_TEST && !strcmp(ui.msg, "TEST SCENE LOADED"));
    tap(B_SAVE);
    check("..SAVE on TEST: it says to save into a slot", !strcmp(ui.msg, "TEST: SAVE INTO P1..P6") && pj_cur == PJ_TEST);
    sys.playing = 1;
    render_poll(1378 * 2);
    check("..playing: T1's grain buffer recording the drums, T3's strings ringing",
          gbuf[0].len > 0 && reso[2].nch == RS_N && peak_of(track_rt[0].last, CTL) > 0);
    sys.playing = 0;
    press(B_OCTDN);
    tap(B_SAVE);
    check("..SAVE tapped on a page: where to save it", !strcmp(ui.msg, "TURN PRESETS TO A SLOT, THEN SAVE") &&
          pj_sum[0].used);
    power_on();
    pj_boot();
    check("..and P1 still comes back at power-on", tp[2].dev[DEV_COLOR][CP_DRIV] == 60);
    pj_wipe();
}

/* overload sheds DRUM's voices after the grains and the strings, and gives them back */
static void test_drum_shed(void)
{
    uint32_t b, j, live, i;
    power_on();
    tp[0].src = SRC_DRUM;
    sys.playing = 1;
    for (i = 0; i < 5u; i++)                             /* five instruments struck together */
        drm_strike(0, tp[0].drm, i == 0 ? 0u : i + 11u > 15u ? 15u : i + 11u, 0, 0);
    render(1, 0);
    grain_cap = 4u;                                      /* (the grains and strings already shed) */
    reso_cap = 2u;
    chain_shed();
    chain_shed();
    chain_shed();
    check("overload past the grains and strings: the drum voices come down, 5 -> 2", drm_cap == 2u);
    render(40, 0);                                       /* (30 ms: 1.5 ms decays, past -60 dB in ~10) */
    for (live = 0, j = 0; j < DRM_NV; j++)
        live += drm_rt[0].v[j].live;
    check("..the voices over the cap fade out (a 1.5 ms decay), at most two ring", live <= 2u);
    for (b = 0; b < 1378u * 3u + 10u; b++)
        render(1, 0);
    check("..three calm seconds: three voices back", drm_cap == 5u);
    sys.playing = 0;
    grain_cap = GR_CAP;
    reso_cap = RS_N;
}

int main(int argc, char **argv)
{
    out_dir = argc > 1 ? argv[1] : "build/bryo_ui";
    memset(host_nor, 0xFF, sizeof host_nor);
    test_canvas();
    test_tape();
    test_uslots();
    test_drive();
    test_drive_more();
    test_tape_edges();
    test_memory();
    test_controls_more();
    test_synth();
    test_poly();
    test_grain();
    test_grain_buffer();
    test_grain_shares();
    test_grain_silence();
    test_grain_body();
    test_settle();
    test_resting();
    test_bar_record();
    test_zoom();
    test_sync();
    test_grain_clicks();
    test_reso();
    test_reso_pluck();
    test_ripple_forms();
    test_scenes();
    test_fm();
    test_pd();
    test_chord();
    test_vox();
    test_wt();
    test_twin();
    test_arp_grnd();
    test_tape_ptch();
    test_poly_loop();
    test_synth_more();
    test_sleep();
    test_color();
    test_space();
    test_chronos_character();
    test_chronos_turns();
    test_usbrec();
    test_mixer();
    test_chan_glide();
    test_chan_reseed();
    test_mod();
    test_mod_engines();
    test_mod_audio();
    test_rails();
    test_source_level();
    test_drum_voices();
    test_drum();
    test_drum_play_fixes();
    test_sends();
    test_projects();
    test_project_worst();
    test_save_seamless();
    test_key_tuning();
    test_lfo_third();
    test_env_pad();
    test_page_keys();
    test_step_leds();
    test_drum_mod();
    test_tpresets();
    test_tpfactory();
    test_arp_keys();
    test_arp_sources();
    test_factory_scene();
    test_drum_shed();
    test_input();
    test_settings();
    palette_set(UI_GREY_INDEX);                          /* (the screens are one ink now, whatever the palette) */
    screens_in("BRYO");
    printf(fails ? "bryo: %d FAILED\n" : "bryo: all passed\n", fails);
    return fails != 0;
}
