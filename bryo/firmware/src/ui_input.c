/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: buttons, keys and encoders into actions, and the LEDs. Main loop only.
 *
 * Phase 1 mapping (docs/bryo-architecture.md, PRD 2):
 *   HOME           focus the track's source (TAPE); again: its page 2
 *   EDIT           focus GRAIN; again: GRAIN 2, RIPPLE, GRAIN ... (each device's pages, then the next device)
 *   FX             focus COLOR; again: COLOR 2, SPACE, SPACE 2, COLOR ...
 *   LFO ENV SEQ    the modulators (param.c SLOT_*): tapped, a slot's page as the pad is let go; again: its next
 *                  page. Held: KNOB 1..4 set that slot's depth to the knobs on the page shown (a device, the source,
 *                  the channel strip; mod.c); held + SELECT: its engine. The LFO pad holds the three swappable ones,
 *                  M1 M2 M3 (LFO, ADSR or FOLLOW each): held + OP1 OP2 OP3 picks the one it shows. ENV is M2 straight
 *                  away. SEQ is M4, always the step sequencer
 *   ARP            the arpeggiator's page (arp.c); again: its page 2. On it, POLY held clears the notes it holds
 *   SEQ slot page  a white key held + KNOB 1: that step's value (the keys pick steps here, they don't play)
 *   MONO held      half a second: clear the focused track's modulation (SAVE held: undo)
 *   GLO held       the mixer while held: white keys 1..4 pick the track, KNOB 1..4 set the levels, SELECT sets
 *                  TRACKS; let go: back
 *   GLO tapped     the mixer stays up; tap again (or any page pad): back
 *   EDIT on the mixer  the selected track's channel (LOW HIGH FILT PAN) on KNOB 1..4: held, while held; tapped,
 *                  latched, as GLO's mixer and the 0 key's freeze. Tapped again: MASTER (the compressor: AMT ATK
 *                  REL MIX); again: the levels
 *   SCL            held + a knob: that knob to its default; held + the page's own pad: the page's knobs to their
 *                  defaults (PATN excepted: it writes a pattern). SAVE held puts back what the hold reset
 *   PLAY           start / stop
 *   REC            arm the focused track (playing: as it goes down, so a punch-in lands where it's pressed;
 *                  stopped: as it's let go). Held a second while stopped: the USB record mode (usbrec.c): REC
 *                  starts and stops the take, the white keys pick its track, REC again puts it there, HOME goes
 *                  back a step
 *   OCT- / OCT+    the white keys' octave (DRUM's own pages: the bar STEP shows)
 *   PRESETS        the project view (project.c): turned again, the slot it points at. There OCT+ loads it, SAVE
 *                  saves into it, POLY held deletes it, OCT- (or any page pad) goes back. Loading over changes
 *                  not saved, or saving over another project, wants a second press within 3 s. EDIT: its second
 *                  page, the track PRESETS (tpreset.c, A..H): OCT+ loads one into the focused track (SAVE held
 *                  puts the track back), SAVE saves the focused track there, POLY held deletes it
 *   SAVE tapped    save into the project loaded or saved last (P1 to start with); held: undo the last clear (or
 *                  the last preset load or SCL reset)
 *   DRUM           HOME turns its pages as it's let go; held + a white key: STEP's instrument (heard). On PATTERN,
 *                  VARY and KIT the white keys play the kit, and one held makes KNOB 1..4 its own TUNE DEC LVL
 *                  TONE; on STEP they toggle the bar's steps (HITS, ACC), or play and write (LIVE), or wipe (ERAS).
 *                  POLY held clears the pattern (SAVE held: undo)
 *   KNOB 1..4      the four values on screen (SETUP's KNOB: a detent 1, 2 or 4 steps, or more on a fast turn);
 *                  SELECT: the tempo
 *   black OP1..OP4 track mutes (SETUP's KEYS on NOTE: the black keys play, and none of their shortcuts are there)
 *   GLO held + OCT+  KEYS: the black keys as shortcuts or notes (SETUP, the mixer's last page, has it too)
 * What the later phases add (p-locks, the views behind PRESETS, the other black keys) slots into the same
 * functions. */

/* the pad a slot's page belongs to (slot 1, M2, shows on ENV; the LFO pad can show it too) */
static const uint8_t SLOT_BTN[NSLOT] = {B_LFO, B_LFO, B_SEQ, B_LFO};   /* (M1-M3 through the LFO pad; ENV is the
                                                                     * source's envelope page) */

/* ------------------------------------------------------------ LEDs --- */
static uint8_t led_pos[41];          /* button 0..13 / key 14..40 -> (column << 3) | row; 0xFF none */
static void led_pos_init(void)
{
    uint32_t id, p, r;
    for (id = 0; id < 41u; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)((p << 3) | r);
    }
}
static void led_put(uint8_t *nl, uint32_t id, int on)
{
    uint8_t q = led_pos[id];
    if (q != 0xFF && on)
        nl[q >> 3] |= (uint8_t)(1u << (q & 7u));
}
/* PLAY's second, green LED: not in the key matrix, found on the hardware at column 8, row PA9 (bit 1) */
#define LED_PLAY_GREEN ((8u << 3) | 1u)
static void led_clear(uint8_t *nl, uint32_t id)
{
    uint8_t q = led_pos[id];
    if (q != 0xFF)
        nl[q >> 3] &= (uint8_t)~(1u << (q & 7u));
}

/* the page of the focused track's source that ENV opens: SYNTH's AMP, POLY's ENV, TAPE's envelope (each the second
 * page), DRUM's KIT (every instrument's decay, with TUNE TONE DRV) */
static uint32_t env_page(void) { return tp[sys.sel].src == SRC_DRUM ? 2u : 1u; }

/* the pad that focuses what the screen shows */
static uint32_t focus_btn(void)
{
    if (ui.view == VIEW_MIXER)
        return ui.chan ? B_EDIT : B_GLO;
    if (ui.view == VIEW_ROUTE || ui.view == VIEW_USBREC || ui.view == VIEW_PROJECT)   /* (no pad: ALGORITHM and
                                                                                     * PRESETS are knobs; REC held) */
        return NB;
    if (ui.kind == FOCUS_ARP)
        return B_ARP;
    if (ui.kind == FOCUS_SLOT)
        return SLOT_BTN[ui.slot % NSLOT];
    if (ui.dev == DEV_SRC)
        return ui.page == env_page() ? B_ENV : B_HOME;   /* (the envelope's page: ENV is its pad) */
    return ui.dev <= DEV_RESO ? B_EDIT : B_FX;
}

/* Lit: the focus pad, REC armed, the octave keys away from the default, the muted tracks' OP keys. The rest glow
 * dim (the LEDS setting: OFF, DIM LO, DIM HI, INV). Each picture is built off-line and copied one
 * byte per column, the glow first, so an LED going from lit to dim never has a dark frame. */
static int pad_down(void);
static int drm_step_page(void);
static int seq_page(void);
static uint32_t pad_pages(uint32_t b);
static int pad_shown(uint32_t b, uint32_t i);
static void ui_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0};
    uint32_t k, c, mode = settings_leds;
    static uint8_t ready;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    if (focus_btn() < NB)
        led_put(nl, panel.btn[focus_btn()], 1);
    if (ui.view == VIEW_PAGE && ui.kind == FOCUS_SLOT && ui.slot == ui.lfo_slot)   /* (M2 through the LFO pad: both) */
        led_put(nl, panel.btn[B_LFO], 1);
    led_put(nl, panel.btn[B_REC], ui.view == VIEW_USBREC ? ur.state == UR_RECORDING : (sys.rec >> sys.sel) & 1u);
    if (ui.view == VIEW_USBREC && ur.state == UR_CHOOSE)   /* the keys that pick the take's track */
        for (k = 0; k < 27u; k++)
            led_put(nl, 14u + k, KEY_WHITE[k] < sys.ntrk);
    led_put(nl, panel.btn[B_OCTDN], track[sys.sel].octave < 3u);
    led_put(nl, panel.btn[B_OCTUP], track[sys.sel].octave > 3u);
    for (k = 0; k < 27u; k++) {
        uint32_t b = KEY_BLACK[k];
        led_put(nl, 14u + k, (!sys.keys_notes || ui.glo_held) &&   /* (KEYS NOTE: GLO held shows the shortcuts) */
                             ((b <= BK_OP4 && track[b].mute) || (b == BK_ZERO && sys.freeze)));   /* (0 lit: latched) */
        led_put(nd, 14u + k, 1);
    }
    if ((drm_step_page() && sys.drum_mode == DMODE_HITS) || seq_page()) {   /* (GLO held: the mixer, not these) */
        /* the white keys as steps, where they set steps: DRUM's STEP page (HITS, ACC) lights the bar's steps that are
         * set (the instrument's hits, or the accents) and flips the one playing as it passes (a set step goes dim
         * for it, an empty one lit), so the playhead reads over both; a SEQ page glows the steps in LEN (from STRT),
         * lights the one playing and the ones held (KNOB 1 sets them) */
        uint32_t on = 0, glow = 0xFFFFu, s;
        if (seq_page()) {
            const int16_t *v = tp[sys.sel].mod[ui.slot];
            uint32_t len = (uint32_t)clamp(v[0], 1, 16), st = (uint32_t)clamp(v[7], 1, 16) - 1u;
            int32_t now = mod_seq_now(sys.sel, ui.slot);
            glow = 0;
            for (s = 0; s < len; s++)
                glow |= 1u << ((s + st) % 16u);
            on = ui.steps_held;
            if (sys.playing && now >= 0)
                on |= 1u << (uint32_t)now;
        } else {
            const int16_t *p = tp[sys.sel].drm;
            uint32_t bar = (uint32_t)clamp(p[DM_BAR], 1, (int32_t)DRM_NBAR) - 1u, i = (uint32_t)clamp(p[DM_INST], 0, 15);
            uint32_t now = drm_now[sys.sel];
            for (s = 0; s < 16u; s++)
                on |= (p[DM_MODE] == DMODE_ACC ? (tp[sys.sel].dacc[bar] >> s) & 1u
                                               : (tp[sys.sel].dpat[bar][s] >> i) & 1u) << s;
            if (sys.playing && now != 0xFFu && now / 16u == bar)
                on ^= 1u << (now % 16u);
        }
        for (k = 0; k < 27u; k++) {
            uint32_t w = KEY_WHITE[k];
            if (w == KEY_NONE || w >= 16u)
                continue;
            led_clear(nl, 14u + k);
            led_clear(nd, 14u + k);
            led_put(nd, 14u + k, (glow >> w) & 1u);
            led_put(nl, 14u + k, (on >> w) & 1u);
        }
    }
    if (pad_down()) {                                   /* a page's pad held: the keys are its pages (dim: there,
                                                         * lit: the one shown, the rest dark); the LFO pad also shows
                                                         * OP1-OP3, which of M1 M2 M3 it's on */
        static const uint8_t PICK[3] = {0, 1, 3};
        uint32_t n = pad_pages(ui.pg_pad);
        for (k = 0; k < 27u; k++) {
            uint32_t w = KEY_WHITE[k], b = KEY_BLACK[k];
            led_clear(nl, 14u + k);
            led_clear(nd, 14u + k);
            if (w != KEY_NONE && w < n) {
                led_put(nd, 14u + k, 1);
                led_put(nl, 14u + k, pad_shown(ui.pg_pad, w));
            } else if (ui.pg_pad == B_LFO && b <= BK_OP3) {
                led_put(nd, 14u + k, 1);
                led_put(nl, 14u + k, ui.lfo_slot == PICK[b]);
            }
        }
    }
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    if (sys.playing)                                    /* playing: PLAY's green, its own LED dark in every mode */
        led_clear(nd, panel.btn[B_PLAY]);
    for (c = 0; c < FM1_NCOL; c++) {
        if (mode == LEDS_INV)                           /* INV: the active ones dark, the rest lit */
            nl[c] = (uint8_t)(nd[c] & ~nl[c]);
        nd[c] = mode == LEDS_DIM || mode == LEDS_DIM_LO ? nd[c] : 0u;   /* OFF, INV: no glow */
    }
    if (sys.playing)
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    fm1_led_dim_level(mode == LEDS_DIM_LO);
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led_dim[c] = nd[c];
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = nl[c];
}

/* ----------------------------------------------------------- actions --- */
/* a page pad: that page (the mixer closes); the visualization starts without a last-turned knob */
static void focus_dev(uint32_t d, uint32_t page)
{
    ui.view = VIEW_PAGE;
    ui.chan = 0;
    ui.chan_latched = 0;
    ui.glo_latched = 0;
    ui.kind = FOCUS_DEV;
    ui.dev = (uint8_t)d;
    ui.page = (uint8_t)page;
    ui.last = 0xFF;
}

/* a pad that holds devices a and b (b == a: one device): pressed again, the next page, then the next device,
 * round; from elsewhere, a's first page */
static void pad_devices(uint32_t a, uint32_t b)
{
    uint32_t d = ui.dev;
    if (ui.view != VIEW_PAGE || ui.kind != FOCUS_DEV || (d != a && d != b)) {
        focus_dev(a, 0);
        return;
    }
    if (ui.page + 1u < pdesc_pages(dev_p(sys.sel, d)))
        focus_dev(d, ui.page + 1u);
    else
        focus_dev(d == a ? b : a, 0);
}

static void focus_slot(uint32_t s)
{
    uint32_t again = ui.view == VIEW_PAGE && ui.kind == FOCUS_SLOT && ui.slot == s;
    ui.view = VIEW_PAGE;
    ui.chan = 0;
    ui.chan_latched = 0;
    ui.glo_latched = 0;
    ui.kind = FOCUS_SLOT;
    ui.slot = (uint8_t)s;
    ui.page = again ? (uint8_t)((ui.page + 1u) % pdesc_pages(ME_P[tp[sys.sel].engine[s]])) : 0u;
    ui.last = 0xFF;
}

/* the ARP pad: the arpeggiator's page; again, its next page */
static void focus_arp(void)
{
    uint32_t again = ui.view == VIEW_PAGE && ui.kind == FOCUS_ARP;
    ui.view = VIEW_PAGE;
    ui.chan = 0;
    ui.chan_latched = 0;
    ui.glo_latched = 0;
    ui.kind = FOCUS_ARP;
    ui.page = again ? (uint8_t)((ui.page + 1u) % pdesc_pages(ARP_P)) : 0u;
    ui.last = 0xFF;
}

/* ------------------------------------------------------------- page keys: a pad held, the white keys its pages --- */
/* a page's pad is down (recorded as it went down, and still held) */
static int pad_down(void) { return ui.pg_pad < NB && ((fm1_in.buttons >> panel.btn[ui.pg_pad]) & 1u); }

/* What pad b's pages are, in key order: n of them; page i (0..n-1) shown by pad_go. HOME: the source's; ENV: its
 * envelope's; EDIT: GRAIN's then RIPPLE's; FX: COLOR's then SPACE's; LFO: the modulator it's on; SEQ: M4's;
 * ARP: the arpeggiator's */
static uint32_t pad_pages(uint32_t b)
{
    switch (b) {
    case B_HOME: return pdesc_pages(dev_p(sys.sel, DEV_SRC));
    case B_ENV: return 1u;
    case B_EDIT: return pdesc_pages(DEV_P[DEV_GRAIN]) + pdesc_pages(DEV_P[DEV_RESO]);
    case B_FX: return pdesc_pages(DEV_P[DEV_COLOR]) + pdesc_pages(DEV_P[DEV_SPACE]);
    case B_LFO: return pdesc_pages(ME_P[tp[sys.sel].engine[ui.lfo_slot % NSLOT] % NME]);
    case B_SEQ: return pdesc_pages(ME_P[tp[sys.sel].engine[SLOT_SEQ] % NME]);
    case B_ARP: return pdesc_pages(ARP_P);
    default: return 0u;
    }
}
/* (the device, slot or arpeggiator page i of pad b is) */
static void pad_where(uint32_t b, uint32_t i, uint32_t *kind, uint32_t *what, uint32_t *page)
{
    uint32_t g;
    *kind = FOCUS_DEV;
    *what = DEV_SRC;
    *page = i;
    switch (b) {
    case B_ENV: *page = env_page(); break;
    case B_EDIT:
        g = pdesc_pages(DEV_P[DEV_GRAIN]);
        *what = i < g ? DEV_GRAIN : DEV_RESO;
        *page = i < g ? i : i - g;
        break;
    case B_FX:
        g = pdesc_pages(DEV_P[DEV_COLOR]);
        *what = i < g ? DEV_COLOR : DEV_SPACE;
        *page = i < g ? i : i - g;
        break;
    case B_LFO: *kind = FOCUS_SLOT, *what = ui.lfo_slot % NSLOT; break;
    case B_SEQ: *kind = FOCUS_SLOT, *what = SLOT_SEQ; break;
    case B_ARP: *kind = FOCUS_ARP, *what = 0; break;
    default: break;
    }
}
/* the page pad b's key i would show is the one up */
static int pad_shown(uint32_t b, uint32_t i)
{
    uint32_t kind, what, page;
    pad_where(b, i, &kind, &what, &page);
    return ui.view == VIEW_PAGE && ui.kind == kind && ui.page == page &&
           (kind == FOCUS_ARP || (kind == FOCUS_SLOT ? ui.slot == what : ui.dev == what));
}
static void focus_slot(uint32_t s);
static void focus_arp(void);
/* pad b held + white key i: its page i (past its last: said) */
static void pad_go(uint32_t b, uint32_t i)
{
    uint32_t kind, what, page;
    ui.pg_used = 1;
    if (b == B_HOME)
        ui.home_used = 1;
    if (ui.slot_held != SLOT_NONE)
        ui.slot_used = 1;
    if (i >= pad_pages(b)) {
        ui_message("NO PAGE THERE");
        return;
    }
    pad_where(b, i, &kind, &what, &page);
    if (kind == FOCUS_SLOT) {
        focus_slot(what);
    } else if (kind == FOCUS_ARP) {
        focus_arp();
    } else {
        focus_dev(what, 0);
    }
    ui.page = (uint8_t)page;
    ui.last = 0xFF;
}

/* a slot's pad (b) down: nothing yet. Let go without turning anything (slot_up), its page; held, the knobs set its
 * depths (on_knob) and SELECT its engine */
static void slot_down(uint32_t s, uint32_t b)
{
    ui.slot_btn = (uint8_t)b;
    ui.slot_held = (uint8_t)s;
    ui.slot_used = 0;
    ui.catch_dir = 0;
}

static void slot_up(void)
{
    uint32_t s = ui.slot_held;
    ui.slot_held = SLOT_NONE;
    if (!ui.slot_used && s < NSLOT)
        focus_slot(s);
}

/* the focused page is a DRUM track's (its source page) */
static int drm_page(void)
{
    return ui.view == VIEW_PAGE && ui.kind == FOCUS_DEV && ui.dev == DEV_SRC && tp[sys.sel].src == SRC_DRUM;
}
/* .. its STEP page */
static int drm_step_page(void) { return drm_page() && ui.page == 3u; }

#define CATCH_MS 250u                                   /* a depth caught at 0 holds a turn this long */

/* the target (mod.c) knob c of the page shown can be modulated as, or MOD_NTGT: none (a slot's page, the levels,
 * the master, the routing) */
static uint32_t page_target(uint32_t c)
{
    if (ui.view == VIEW_MIXER && ui.chan == CHAN_STRIP)
        return MOD_TCH + c;
    if (ui.view == VIEW_PAGE && ui.kind == FOCUS_DEV && ui.drm_inst)   /* an instrument's own knobs (its key held) */
        return c < NDIN ? MOD_TDIN + ((ui.drm_inst - 1u) & 15u) * NDIN + c : MOD_NTGT;
    if (ui.view != VIEW_PAGE || ui.kind != FOCUS_DEV)
        return MOD_NTGT;
    c = pdesc_at(dev_p(sys.sel, ui.dev), c + 4u * ui.page);   /* (the page's position: the stored knob) */
    return ui.dev == DEV_SRC ? MOD_TSRC + (tp[sys.sel].src % NSRC) * NPK + c : MOD_TG(ui.dev, c);
}

/* the focused page is a SEQ slot's (the white keys pick its steps) */
static int seq_page(void)
{
    return ui.view == VIEW_PAGE && ui.kind == FOCUS_SLOT && tp[sys.sel].engine[ui.slot] == ME_SEQ;
}

/* GLO down: the mixer comes up and stays while GLO is held */
static void glo_down(void)
{
    ui.glo_held = 1;
    ui.glo_used = 0;
    ui.glo_t0 = fm1_ms;
    ui.view = VIEW_MIXER;
    ui.last = 0xFF;
}

/* GLO up: after a hold that did something, or a long hold, back to the page; after a tap, the mixer stays (a tap
 * on a mixer that was already up closes it) */
static void glo_up(void)
{
    int tap = !ui.glo_used && (uint32_t)(fm1_ms - ui.glo_t0) < HOLD_MS[settings_hold % 4u];
    ui.glo_held = 0;
    if (tap && !ui.glo_latched) {
        ui.glo_latched = 1;
        return;
    }
    ui.glo_latched = 0;
    ui.view = VIEW_PAGE;
    ui.chan = 0;
    ui.chan_latched = 0;
    ui.last = 0xFF;
}

/* ------------------------------------------------------------ projects --- */
/* PRESETS turned: the project view opens on the project loaded last; turned again, it points at the next slot */
static void pj_turn(int32_t d)
{
    if (ui.view != VIEW_PROJECT) {
        ui.view = VIEW_PROJECT;
        ui.chan = 0;
        ui.chan_latched = 0;
        ui.glo_latched = 0;
        ui.pj_sel = pj_cur;
    } else if (ui.pj_page == 2u) {                      /* (your reels: U1..U6) */
        ui.reel_sel = (uint8_t)clamp((int32_t)ui.reel_sel + d, 0, (int32_t)USLOT_N - 1);
    } else if (ui.pj_page) {                            /* (the track presets' page: A..H) */
        ui.tpr_sel = (uint8_t)clamp((int32_t)ui.tpr_sel + d, 0, (int32_t)(TPR_N + TPF_N) - 1);   /* A..H, 1..8 */
    } else {
        ui.pj_sel = (uint8_t)clamp((int32_t)ui.pj_sel + d, 0, (int32_t)PJ_TEST);   /* (past P6: FACTORY, TEST) */
    }
    ui.pj_ask = PJ_ASK_NONE;
    ui.last = 0xFF;
}

/* an action that wants a second press: asked now (0), or already asked and pressed again in time (1) */
static int pj_confirmed(uint32_t what, const char *ask)
{
    if (ui.pj_ask == what && (uint32_t)(fm1_ms - ui.pj_ask_t) < 3000u) {
        ui.pj_ask = PJ_ASK_NONE;
        return 1;
    }
    ui.pj_ask = (uint8_t)what;
    ui.pj_ask_t = fm1_ms;
    ui_message(ask);
    return 0;
}

/* the instrument saved into slot s, and what came of it said */
static void pj_do_save(uint32_t s)
{
    char m[40] = "SAVING P";
    int rc;
    uint32_t t;
    m[8] = (char)('1' + s);
    m[9] = 0;
    ui_message(m);
    ui_draw();                                          /* (the flash holds the main loop while it writes) */
    rc = pj_save(s);
    str_cpy(m, "P", sizeof m);
    m[1] = (char)('1' + s);
    m[2] = 0;
    if (rc < 0) {
        str_cpy(m + 2, " NOT SAVED: THE FLASH REFUSED", 32);
    } else if (rc == 0) {
        str_cpy(m + 2, " SAVED", 8);
    } else {                                            /* saved, some tapes without a reel to go in */
        str_cpy(m + 2, " SAVED. NO FREE REEL:", 24);
        for (t = 0; t < NTRK; t++)
            if ((rc >> t) & 1) {
                char tn[4] = {' ', 'T', (char)('1' + t), 0};
                str_cpy(m + str_len(m), tn, 4);
            }
    }
    ui_message(m);
}

/* SAVE tapped: on the project view into the slot pointed at (over another project: asked first), elsewhere into the
 * project loaded or saved last */
static void tpr_save_tap(void);
static void pj_save_tap(void)
{
    uint32_t s = ui.view == VIEW_PROJECT ? ui.pj_sel : pj_cur;
    if (ui.view == VIEW_PROJECT && ui.pj_page == 2u) {
        ui_message("REELS ARE SAVED WITH A PROJECT, OR COPIED IN OVER USB");
        return;
    }
    if (ui.view == VIEW_PROJECT && ui.pj_page) {
        tpr_save_tap();
        return;
    }
    if (s >= PJ_N) {                                    /* FACTORY: it's saved into a slot, not over itself */
        ui_message(ui.view != VIEW_PROJECT ? "TURN PRESETS TO A SLOT, THEN SAVE" : s == PJ_TEST ? "TEST: SAVE INTO P1..P6"
                                                                                       : "FACTORY: SAVE INTO P1..P6");
        return;
    }
    if (ui.view == VIEW_PROJECT && pj_sum[s].used && s != pj_cur) {
        char m[40] = "P";
        m[1] = (char)('1' + s);
        m[2] = 0;
        str_cpy(m + 2, " HOLDS A PROJECT: SAVE AGAIN", 30);
        if (!pj_confirmed(PJ_ASK_SAVE, m))
            return;
    }
    pj_do_save(s);
}

/* OCT+ on the project view: the slot pointed at, loaded (over changes not saved: asked first) */
static void pj_load_press(void)
{
    uint32_t s = ui.pj_sel;
    char m[40] = "P";
    m[1] = (char)('1' + s);
    m[2] = 0;
    if (pj_changed() && !pj_confirmed(PJ_ASK_LOAD, "CHANGES NOT SAVED: OCT+ AGAIN"))
        return;
    if (s == PJ_TEST) {                                 /* TEST: the scene to read the load on */
        trk_undo.valid = 0;
        pj_test();
        ui_message("TEST SCENE LOADED");
        return;
    }
    if (s >= PJ_N) {                                    /* FACTORY: the scene Bryo starts with */
        trk_undo.valid = 0;
        pj_factory();
        ui_message("FACTORY SCENE LOADED");
        return;
    }
    if (!pj_sum[s].used) {                              /* an empty slot: a new project there, blank */
        trk_undo.valid = 0;
        pj_new(s);
        str_cpy(m + 2, ": NEW AND BLANK. SAVE KEEPS IT", 32);
        ui_message(m);
        return;
    }
    trk_undo.valid = 0;                                 /* (the snapshot belonged to the session before) */
    str_cpy(m + 2, pj_load(s) == 0 ? " LOADED" : " CAN'T BE READ", 16);
    ui_message(m);
}

/* ------------------------------------------------------------- track presets (tpreset.c) --- */
/* "PRESET A", into m */
static void tpr_name(char *m, uint32_t p)
{
    str_cpy(m, "PRESET ", 8);
    m[7] = (char)('A' + p % TPR_N);
    m[8] = 0;
}

/* SAVE tapped on the presets' page: the focused track saved as the preset pointed at (over another: asked first) */
static void tpr_save_tap(void)
{
    uint32_t p = ui.tpr_sel;
    char m[40];
    int rc;
    if (p >= TPR_N) {                                   /* (a factory one) */
        ui_message("FACTORY: SAVE INTO A..H");
        return;
    }
    tpr_name(m, p);
    if (tpr_sum[p].used) {
        str_cpy(m + 8, " HOLDS ONE: SAVE AGAIN", 24);
        if (!pj_confirmed(PJ_ASK_SAVE, m))
            return;
        tpr_name(m, p);
    }
    rc = tpr_save(p, sys.sel);
    str_cpy(m + 8, rc == 0 ? " SAVED FROM T" : rc == -2 ? " NOT SAVED: PRESETS FULL" : " NOT SAVED: FLASH", 26);
    if (rc == 0) {
        m[str_len(m) + 1] = 0;
        m[str_len(m)] = (char)('1' + sys.sel);
    }
    ui_message(m);
}

/* OCT+ on the presets' page: the preset pointed at, into the focused track (no question: SAVE held puts the track
 * back, so presets can be tried one after another) */
static void tpr_load_press(void)
{
    uint32_t p = ui.tpr_sel;
    char m[40];
    if (p >= TPR_N) {                                   /* a factory one: its name */
        str_cpy(m, TPF[(p - TPR_N) % TPF_N].name, 8);
        if (tpf_load(p - TPR_N, sys.sel) == 0) {
            str_cpy(m + str_len(m), " INTO T", 8);
            m[str_len(m) + 1] = 0;
            m[str_len(m)] = (char)('1' + sys.sel);
            str_cpy(m + str_len(m), ". HOLD SAVE: UNDO", 18);
            ui.undo = UNDO_TRACK;
        }
        ui_message(m);
        return;
    }
    tpr_name(m, p);
    if (!tpr_sum[p].used) {
        str_cpy(m + 8, " IS EMPTY", 12);
    } else if (tpr_load(p, sys.sel) == 0) {
        str_cpy(m + 8, " INTO T", 8);
        m[15] = (char)('1' + sys.sel);
        m[16] = 0;
        str_cpy(m + 16, ". HOLD SAVE: UNDO", 18);
        ui.undo = UNDO_TRACK;
    } else {
        str_cpy(m + 8, " CAN'T BE READ", 16);
    }
    ui_message(m);
}

/* REC: arm the focused track (the tape made ready: a reel copied in), or let go of it */
static void rec_arm(void)
{
    if ((sys.rec >> sys.sel) & 1u) {
        sys.rec &= (uint8_t)~(1u << sys.sel);
        tape_unprepare(sys.sel);
    } else {
        int r = tape_prepare(sys.sel);
        if (r > 0)
            sys.rec |= (uint8_t)(1u << sys.sel);
        else
            ui_message(r < 0 ? "NO MEMORY FREE TO RECORD" : "TAPE HAS A TAKE: CLEAR IT (HOLD POLY)");
    }
}

/* the USB record mode opens (REC held a second, stopped) */
static void usbrec_open(void)
{
    usbrec_enter();
    ui.view = VIEW_USBREC;
    ui.chan = 0;
    ui.chan_latched = 0;
    ui.glo_latched = 0;
    ui.glo_held = 0;
    ui.last = 0xFF;
}

/* SETUP KEYS: the black keys as notes (1) or the shortcuts (0), kept with the settings */
static void keys_set(int notes)
{
    bryo_prefs = (uint8_t)(notes ? bryo_prefs | BPREF_NOTES : bryo_prefs & ~BPREF_NOTES);
    bprefs_apply();
    settings_save();
    ui_message(notes ? "BLACK KEYS PLAY NOTES" : "BLACK KEYS: SHORTCUTS");
}

/* SETUP's knob c set to v (ui.c ui_page shows them; settings.c keeps them, saved while stopped) */
static void setup_set(uint32_t c, int32_t v)
{
    switch (c) {
    case ST_KEYS:
        if ((v != 0) != (sys.keys_notes != 0))
            keys_set(v != 0);
        return;
    case ST_KNOB:
        bryo_prefs = (uint8_t)((bryo_prefs & ~6u) | ((uint32_t)v & 3u) << 1);
        break;
    case ST_HOLD:
        settings_hold = (uint8_t)(v & 3);
        break;
    default:
        settings_leds = LEDS_MENU[v & 3];
        break;
    }
    settings_save();
}

#define ACC_GAP 40u                                     /* KNOB ACC: detents closer than this (ms) are a fast turn */
#define ACC_RATE 50u
/* d detents of encoder e (KNOB 1-4, 4 SELECT) on a value of range steps: as many steps as SETUP's KNOB makes them.
 * Ranges of 32 steps or fewer (and choices: range 0) always move one a detent. ACC is Felucca's (#52): the time
 * between detents sets a multiplier of 1 to 4, a slow turn stays fine */
static int32_t knob_steps(uint32_t e, int32_t d, int32_t range)
{
    uint32_t mode = (bryo_prefs >> 1) & 3u, now = fm1_ms & 0xFFFFFFu, st, up = d > 0, pi, a, i, m = 1;
    if (!d || range <= 32)
        return d;
    if (mode != KNOB_ACC)
        return d * (mode == KNOB_4X ? 4 : mode == KNOB_2X ? 2 : 1);
    st = ui.enc_t[e % 5u];
    pi = st >> 25;
    a = (uint32_t)(d < 0 ? -d : d);
    i = ((now - st) & 0xFFFFFFu) / a;                   /* ms a detent, this read */
    if (!st || ((st >> 24) & 1u) != up || i >= ACC_GAP) {
        i = 127u;                                       /* a new turn, or the other way */
    } else if (pi < ACC_GAP) {
        m = ACC_RATE / (i > pi ? i : pi ? pi : 1u);
        m = m < 1u ? 1u : m > 4u ? 4u : m;
    }
    ui.enc_t[e % 5u] = now | up << 24 | (i ? i : 1u) << 25;
    return d * (int32_t)m;
}

static void on_button(uint32_t b)
{
    switch (b) {
    case B_SCL:                                         /* held: a knob or the page's pad back to the default(s) */
        ui.scl_held = 1;
        ui.scl_used = 0;
        break;
    case B_HOME:                                        /* the source, its next page: as HOME is let go (the slot
                                                         * pads' way), so it can be held for SELECT or a page key */
        ui.home_held = 1;
        ui.home_used = 0;
        ui.pg_pad = B_HOME;
        ui.pg_used = 0;
        break;
    case B_EDIT:                                        /* GRAIN's pages, RIPPLE's; on the mixer: the channel */
        if (ui.view == VIEW_PROJECT) {                  /* the project view's pages: PROJECTS, PRESETS, REELS (as
                                                         * EDIT steps the mixer's) */
            ui.pj_page = (uint8_t)((ui.pj_page + 1u) % 3u);
            ui.pj_ask = PJ_ASK_NONE;
            break;
        }
        if (ui.view == VIEW_MIXER) {                    /* (held: while held; tapped: latched; tapped again: off) */
            if (ui.chan == CHAN_SETUP && ui.chan_latched) {   /* LEVELS CHANNEL MASTER KEY SETUP, LEVELS */
                ui.chan = CHAN_LEVELS;
                ui.chan_latched = 0;
            } else {
                ui.chan = !ui.chan_latched || ui.chan >= CHAN_SETUP ? CHAN_STRIP : (uint8_t)(ui.chan + 1u);
                ui.chan_latched = 0;
                ui.chan_held = 1;
                ui.chan_used = 0;
                ui.chan_t0 = fm1_ms;
            }
            ui.last = 0xFF;
            if (ui.glo_held)
                ui.glo_used = 1;
            break;
        }
        ui.pg_pad = B_EDIT;                             /* (GRAIN's next page, then RIPPLE's: as it's let go) */
        ui.pg_used = 0;
        break;
    case B_FX:                                          /* COLOR's pages, then SPACE's: as it's let go */
        ui.pg_pad = B_FX;
        ui.pg_used = 0;
        break;
    case B_LFO: slot_down(ui.lfo_slot, B_LFO); ui.pg_pad = B_LFO; ui.pg_used = 0; break;   /* (M1 M2 M3: the one
                                                                                          * it shows) */
    case B_ENV:                                         /* the source's envelope (M2 is on the LFO pad: OP2) */
        focus_dev(DEV_SRC, env_page());
        ui.pg_pad = B_ENV;
        ui.pg_used = 0;
        break;
    case B_SEQ: slot_down(SLOT_SEQ, B_SEQ); ui.pg_pad = B_SEQ; ui.pg_used = 0; break;   /* M4, the sequencer */
    case B_ARP: ui.pg_pad = B_ARP; ui.pg_used = 0; break;   /* (its pages: as it's let go) */
    case B_GLO:
        glo_down();
        break;
    case B_PLAY:
        sys.playing = (uint8_t)!sys.playing;
        break;
    case B_REC:                                         /* arm: playing, now; stopped, at the let-go (held a second
                                                         * instead: the USB record mode) */
        ui.rec_held = 1;
        ui.rec_said = 0;
        ui.rec_turned = 0;
        ui.rec_t0 = fm1_ms;
        if (sys.playing)
            rec_arm();
        break;
    case B_SAVE:                                        /* tap: save the project; held: undo the last clear */
        ui.save_held = 1;
        ui.save_t0 = fm1_ms;
        break;
    case B_OCTDN:
    case B_OCTUP:
        if (ui.glo_held && b == B_OCTUP) {              /* GLO held + OCT+: the black keys, shortcuts or notes */
            keys_set(!sys.keys_notes);
            ui.glo_used = 1;
        } else if (ui.view == VIEW_PROJECT) {                  /* the project view: OCT+ loads, OCT- goes back */
            if (b == B_OCTUP) {
                if (ui.pj_page == 2u)
                    ui_message("A REEL PLAYS FROM TAPE'S REEL KNOB");
                else if (ui.pj_page)
                    tpr_load_press();
                else
                    pj_load_press();
            } else {
                ui.view = VIEW_PAGE;
                ui.pj_ask = PJ_ASK_NONE;
                ui.last = 0xFF;
            }
        } else if (drm_page()) {                       /* DRUM's own pages: the bar STEP shows (elsewhere, as on
                                                         * every track, the octave: RIPPLE's root) */
            int16_t *bp = &tp[sys.sel].drm[DM_BAR];
            *bp = (int16_t)clamp(*bp + (b == B_OCTUP ? 1 : -1), 1, tp[sys.sel].drm[DM_LEN]);
        } else if (b == B_OCTDN && track[sys.sel].octave > 1u) {
            track[sys.sel].octave--;
        } else if (b == B_OCTUP && track[sys.sel].octave < 6u) {
            track[sys.sel].octave++;
        }
        break;
    default:
        break;
    }
}

/* LFO held + OP1, OP2 or OP3: the LFO pad's slot is M1, M2 or M3 (a knob turned now sets that one's depth; let go
 * without one, its page) */
static void lfo_pick(uint32_t k)
{
    static const uint8_t PICK[3] = {0, 1, 3};
    uint32_t s = PICK[k % 3u];
    char m[32] = "LFO PAD: M";
    ui.lfo_slot = (uint8_t)s;
    ui.slot_held = (uint8_t)s;
    ui.catch_dir = 0;
    if (ui.view == VIEW_PAGE && ui.kind == FOCUS_SLOT && ui.slot != SLOT_SEQ && ui.slot != s) {
        focus_slot(s);                                  /* (an LFO-pad page up: the one picked, and the let-go */
        ui.slot_used = 1;                               /*  doesn't turn its page) */
    }
    m[10] = (char)('0' + SLOT_NUM[s]);
    m[11] = ' ';
    m[12] = 0;
    str_cpy(m + 12, ME_NAME[tp[sys.sel].engine[s] % NME], 8);
    ui_message(m);
}

static void on_black(uint32_t k)
{
    int16_t *tk = tp[sys.sel].dev[DEV_SRC];
    if (ui.slot_held != SLOT_NONE && ui.slot_btn == B_LFO && k <= BK_OP3) {
        lfo_pick(k);
        return;
    }
    if (sys.keys_notes && !ui.glo_held)                 /* KEYS NOTE: the black keys play (chain.c); GLO held, they're
                                                         * the shortcuts again (GLO's mixer shows what they toggle) */
        return;
    if (k <= BK_OP4) {
        track[k].mute = (uint8_t)!track[k].mute;
    } else if (k == BK_OP5 || k == BK_OP6) {           /* the focused track's tape: reverse, half speed (another
                                                         * source: said, not flipped out of sight) */
        if (tp[sys.sel].src != SRC_TAPE)
            ui_message(k == BK_OP5 ? "REVERSE: TAPE TRACKS ONLY" : "HALF SPEED: TAPE TRACKS ONLY");
        else
            tk[k == BK_OP5 ? TK_REV : TK_HALF] = (int16_t)!tk[k == BK_OP5 ? TK_REV : TK_HALF];
    } else if (k == BK_ZERO) {                          /* GRAIN's freeze: held, while it's held (the ISR reads
                                                         * the key); tapped, latched until the next tap */
        ui.zero_held = 1;
        ui.zero_t0 = fm1_ms;
    } else if (k == BK_MONO) {                          /* held half a second: clear the track's modulation (a
                                                         * slot's pad held too: that slot's on the page shown) */
        ui.mono_held = 1;
        ui.mono_t0 = fm1_ms;
        ui.mono_slot = ui.slot_held;
        if (ui.slot_held < NSLOT)
            ui.slot_used = 1;                           /* (the pad's let-go doesn't open its page) */
        ui_message(ui.slot_held < NSLOT ? "KEEP HOLDING: CLEAR THIS SLOT'S DEPTHS HERE" : "KEEP HOLDING MONO TO CLEAR MODULATION");
    } else if (k == BK_POLY) {                          /* held HOLD: clear the focused track's tape (DRUM: its
                                                         * pattern) */
        ui.poly_held = 1;
        ui.poly_t0 = fm1_ms;
        ui_message(ui.view == VIEW_PROJECT ? "KEEP HOLDING POLY TO DELETE IT" : ui.view == VIEW_PAGE && ui.kind == FOCUS_ARP
                   ? (ui.page == 2u ? "KEEP HOLDING POLY TO CLEAR RIPL ARP" : "KEEP HOLDING POLY TO CLEAR THE NOTES") : tp[sys.sel].src == SRC_DRUM ?
                   "KEEP HOLDING POLY TO CLEAR THE PATTERN" : "KEEP HOLDING POLY TO CLEAR THE TAPE");
    }
}

/* the POLY key and SAVE held long enough: the clear and its undo; let go sooner: the clear is off, SAVE is a tap */
static void hold_keys(void)
{
    uint32_t hold = HOLD_MS[settings_hold % 4u], k;     /* (POLY: half a second, PRD 2.3) */
    if (ui.poly_held) {
        for (k = 0; k < 27u && KEY_BLACK[k] != BK_POLY; k++)
            ;
        if (!((fm1_in.notes >> k) & 1u)) {
            ui.poly_held = 0;
            ui.msg_t = 1;                               /* (the message ends next frame) */
        } else if ((uint32_t)(fm1_ms - ui.poly_t0) >= 500u && ui.view == VIEW_PROJECT && ui.pj_page == 2u) {
            char m[40];                                 /* a reel of yours, deleted */
            int32_t r = uslot_owner(ui.reel_sel);
            ui.poly_held = 0;
            if (r < 0) {
                str_cpy(m, "U", sizeof m);
                m[1] = (char)('1' + ui.reel_sel);
                m[2] = 0;
                str_cpy(m + 2, " IS EMPTY", 10);
            } else {
                str_cpy(m, uslot_name[r], 8);
                str_cpy(m + str_len(m), uslot_delete((uint32_t)r) == 0 ? " DELETED" : " NOT DELETED: FLASH", 22);
            }
            ui_message(m);
        } else if ((uint32_t)(fm1_ms - ui.poly_t0) >= 500u && ui.view == VIEW_PROJECT && ui.pj_page) {   /* a preset */
            char m[40];
            ui.poly_held = 0;
            tpr_name(m, ui.tpr_sel);
            if (ui.tpr_sel >= TPR_N)                    /* (a factory one) */
                str_cpy(m, "FACTORY PRESETS STAY", sizeof m);
            else
                str_cpy(m + 8, !tpr_sum[ui.tpr_sel].used ? " IS EMPTY" : tpr_delete(ui.tpr_sel) == 0 ? " DELETED"
                                                                                                    : " NOT DELETED: FLASH", 22);
            ui_message(m);
        } else if ((uint32_t)(fm1_ms - ui.poly_t0) >= 500u && ui.view == VIEW_PROJECT && ui.pj_sel >= PJ_N) {
            ui.poly_held = 0;
            ui_message(ui.pj_sel == PJ_TEST ? "THE TEST SCENE STAYS" : "THE FACTORY SCENE STAYS");
        } else if ((uint32_t)(fm1_ms - ui.poly_t0) >= 500u && ui.view == VIEW_PROJECT) {   /* a project, deleted */
            char m[40] = "P";
            ui.poly_held = 0;
            m[1] = (char)('1' + ui.pj_sel);
            m[2] = 0;
            str_cpy(m + 2, !pj_sum[ui.pj_sel].used ? " IS EMPTY" : pj_delete(ui.pj_sel) == 0 ? " DELETED (ITS REELS STAY)"
                                                                                           : " NOT DELETED: FLASH", 28);
            ui_message(m);
        } else if ((uint32_t)(fm1_ms - ui.poly_t0) >= 500u && ui.view == VIEW_PAGE && ui.kind == FOCUS_ARP) {
            ui.poly_held = 0;                           /* the arpeggiator's notes (a latched or piled-up pattern;
                                                         * RIPL ARP's on its page) */
            if (ui.page == 2u) {
                RING_PUBLISH();
                reso_arp_clear_req[sys.sel] = 1;
                ui_message("RIPL ARP NOTES CLEARED");
            } else {
                arp_clear(sys.sel);
                ui_message("ARP NOTES CLEARED");
            }
        } else if ((uint32_t)(fm1_ms - ui.poly_t0) >= 500u && tp[sys.sel].src == SRC_DRUM) {
            ui.poly_held = 0;
            if (drm_clear(sys.sel)) {
                ui_message("PATTERN CLEARED. HOLD SAVE: UNDO");
                ui.undo = UNDO_DRUM;
            } else {
                ui_message("THE PATTERN IS EMPTY");
            }
        } else if ((uint32_t)(fm1_ms - ui.poly_t0) >= 500u) {
            char m[40] = "TRACK ";
            ui.poly_held = 0;
            tape_clear(sys.sel);
            sys.rec &= (uint8_t)~(1u << sys.sel);
            fmt_int(m + 6, (int32_t)sys.sel + 1);
            str_cpy(m + str_len(m), " CLEARED. HOLD SAVE: UNDO", 28);
            ui_message(m);
            ui.undo = UNDO_TAPE;
        }
    }
    if (ui.mono_held) {                                 /* MONO: the same, for the track's modulation (mod.c) */
        for (k = 0; k < 27u && KEY_BLACK[k] != BK_MONO; k++)
            ;
        if (!((fm1_in.notes >> k) & 1u)) {
            ui.mono_held = 0;
            ui.msg_t = 1;
        } else if (ui.mono_slot < NSLOT && ui.slot_held != ui.mono_slot) {   /* the pad let go first: off */
            ui.mono_held = 0;
            ui_message("CLEAR CALLED OFF");
        } else if ((uint32_t)(fm1_ms - ui.mono_t0) >= 500u && ui.mono_slot < NSLOT) {   /* one slot, this page */
            uint32_t g[4], n = 0, c;
            char m[40] = "SLOT ";
            ui.mono_held = 0;
            for (c = 0; c < 4u; c++)
                if (page_target(c) < MOD_NTGT)
                    g[n++] = page_target(c);
            m[5] = (char)('0' + SLOT_NUM[ui.mono_slot % NSLOT]);
            m[6] = 0;
            if (!n) {
                str_cpy(m + 6, ": NO DEPTHS ON THIS PAGE", 26);
            } else if (mod_clear_slot(sys.sel, ui.mono_slot, g, n)) {
                str_cpy(m + 6, " CLEARED HERE. HOLD SAVE: UNDO", 33);
                ui.undo = UNDO_MOD;
            } else {
                str_cpy(m + 6, " HAS NO DEPTHS HERE", 22);
            }
            ui_message(m);
        } else if ((uint32_t)(fm1_ms - ui.mono_t0) >= 500u) {
            char m[40] = "T";
            ui.mono_held = 0;
            fmt_int(m + 1, (int32_t)sys.sel + 1);
            if (mod_clear(sys.sel)) {
                str_cpy(m + str_len(m), " MODULATION CLEARED. HOLD SAVE: UNDO", 37);
                ui.undo = UNDO_MOD;
            } else {
                str_cpy(m + str_len(m), " HAS NO MODULATION", 20);
            }
            ui_message(m);
        }
    }
    if (ui.zero_held) {                                 /* the 0 key let go: a tap latches the freeze or lets it go */
        for (k = 0; k < 27u && KEY_BLACK[k] != BK_ZERO; k++)
            ;
        if (!((fm1_in.notes >> k) & 1u)) {
            ui.zero_held = 0;
            if ((uint32_t)(fm1_ms - ui.zero_t0) < hold) {
                sys.freeze = (uint8_t)!sys.freeze;
                ui_message(sys.freeze ? "FROZEN. TAP 0 TO LET GO" : "FREEZE LET GO");
            }
        }
    }
    if (ui.rec_held) {                                  /* REC: a tap arms (stopped), a second's hold the record mode */
        int held = (int)((fm1_in.buttons >> panel.btn[B_REC]) & 1u);
        uint32_t ms = (uint32_t)(fm1_ms - ui.rec_t0);
        if (!held) {
            ui.rec_held = 0;
            if (!sys.playing && ms < 1000u && !ui.rec_turned)
                rec_arm();
            if (ui.rec_said && !ui.rec_turned)
                ui.msg_t = 1;                           /* (the hint ends next frame) */
        } else if (ui.rec_turned) {                     /* (setting the bars: not the USB record mode) */
        } else if (ms >= 1000u) {
            ui.rec_held = 0;
            if (sys.playing)
                ui_message("STOP, THEN HOLD REC: USB RECORD");
            else
                usbrec_open();
        } else if (ms >= 300u && !ui.rec_said && !sys.playing) {
            ui.rec_said = 1;
            ui_message("KEEP HOLDING REC: USB RECORD");
        }
    }
    if (ui.save_held) {
        int held = (int)((fm1_in.buttons >> panel.btn[B_SAVE]) & 1u);
        if (!held) {
            ui.save_held = 0;
            pj_save_tap();
        } else if ((uint32_t)(fm1_ms - ui.save_t0) >= hold) {
            ui.save_held = 0;
            int r = ui.undo == UNDO_MOD ? mod_undo() : ui.undo == UNDO_DRUM ? drm_undo() : ui.undo == UNDO_TRACK
                    ? trk_restore() : tape_undo_clear();
            ui_message(r < 0 ? "NOTHING TO UNDO" : ui.undo == UNDO_TRACK ? "THE TRACK IS BACK" : "CLEAR UNDONE");
        }
    }
}

/* ------------------------------------------------------------- SCL: back to the defaults --- */
/* SCL physically down (ui.scl_held clears as it's let go) */
static int scl_down(void) { return ui.scl_held && ((fm1_in.buttons >> panel.btn[B_SCL]) & 1u); }

/* knob c of the page shown (descriptor p, value at vp) to its default. 0: nothing there (or PATN, which writes a
 * pattern when it moves: it's turned, not reset). The first reset of an SCL hold snapshots the track (SAVE held) */
static int scl_default(uint32_t c, const pdesc_t *p, int16_t *vp)
{
    int16_t v = p->def;
    if (pdesc_empty(p) || p == &DRM_P[DM_PATN])
        return 0;
    if (!ui.scl_used) {
        trk_snap(sys.sel);
        ui.undo = UNDO_TRACK;
        ui.scl_used = 1;
    }
    if (ui.view == VIEW_MIXER && ui.chan == CHAN_SETUP)
        setup_set(c, v);
    else if (ui.view == VIEW_MIXER && !ui.chan)
        track[c].level = (uint8_t)v;
    else if (ui.view == VIEW_ROUTE)
        tp[c].recin = (uint8_t)v;
    else
        *vp = v;
    if (p == &DRM_P[DM_LEN] && tp[sys.sel].drm[DM_BAR] > v)   /* (the bar shown stays in the pattern) */
        tp[sys.sel].drm[DM_BAR] = v;
    return 1;
}

/* SCL held + KNOB c: that knob to its default */
static void scl_knob(uint32_t c)
{
    int16_t *vp;
    const pdesc_t *p = ui_page(c, &vp);
    char m[40];
    if (p == &DRM_P[DM_PATN]) {
        ui_message("PATN WRITES A PATTERN: TURN IT");
        return;
    }
    if (!scl_default(c, p, vp))
        return;
    str_cpy(m, p->label, 6);
    str_cpy(m + str_len(m), " TO DEFAULT. HOLD SAVE: UNDO", 30);
    ui_message(m);
    ui.hot = (uint8_t)c;
    ui.hot_t = 33;
    ui.last = (uint8_t)c;
}

/* SCL held + the page's own pad: every knob on the page shown to its default */
static void scl_page(void)
{
    uint32_t c, n = 0;
    for (c = 0; c < 4u; c++) {
        int16_t *vp;
        const pdesc_t *p = ui_page(c, &vp);
        n += (uint32_t)scl_default(c, p, vp);
    }
    ui_message(n ? "PAGE TO DEFAULTS. HOLD SAVE: UNDO" : "NOTHING ON THIS PAGE TO RESET");
    ui.last = 0xFF;
}

/* KNOB c turned by d detents: the value on screen; it lights up (hot) for half a second */
static void on_knob(uint32_t c, int32_t d)
{
    int16_t *vp;
    const pdesc_t *p = ui_page(c, &vp);
    int16_t v;
    if (ui.slot_held != SLOT_NONE && page_target(c) < MOD_NTGT) {   /* a slot's pad held: its depth to this knob */
        uint32_t g = page_target(c);
        int32_t o = mod_dep(sys.sel, ui.slot_held, g), v;
        ui.slot_used = 1;
        ui.hot = (uint8_t)c;
        ui.hot_t = 33;
        ui.last = (uint8_t)c;
        if (ui.catch_dir && ui.catch_g == g && ui.catch_s == ui.slot_held && d * ui.catch_dir > 0
            && (uint32_t)(fm1_ms - ui.catch_t) < CATCH_MS) {
            ui.catch_t = fm1_ms;                        /* (caught at 0: a turn that keeps going stays there) */
            return;
        }
        ui.catch_dir = 0;
        v = mod_nudge(sys.sel, ui.slot_held, g, knob_steps(c, d, 200));
        if (o && !v) {                                  /* reached 0 from either side: the catch */
            ui.catch_g = (uint8_t)g;
            ui.catch_s = ui.slot_held;
            ui.catch_dir = (int8_t)(d < 0 ? -1 : 1);
            ui.catch_t = fm1_ms;
        }
        if (v == -127) {                                /* the slot's dozen: its pad + MONO clears this page's */
            char m[40] = "12 DEPTHS ON M";
            m[14] = (char)('0' + SLOT_NUM[ui.slot_held % NSLOT]);
            str_cpy(m + 15, ": PAD+MONO CLEARS", 18);
            ui_message(m);
        } else if (v == -128)
            ui_message(pdesc_empty(p) ? "NO KNOB THERE" : mod_tdesc(page_target(c)) ? "32 DEPTHS ON THIS TRACK: MONO CLEARS"
                       : p->names == N_REEL ? "THE REEL CAN'T BE MODULATED"
                       : ui.dev == DEV_SPACE ? "THE ROOM IS SHARED: NO DEPTHS" : "THIS KNOB CAN'T BE MODULATED");
        return;
    }
    if (ui.slot_held != SLOT_NONE)                      /* (a page without depths: the knob as usual) */
        ui.slot_used = 1;
    if (ui.view == VIEW_PROJECT)                        /* (the project view: the knobs have nothing there) */
        return;
    if (scl_down()) {                                   /* SCL held: the knob back to its default */
        scl_knob(c);
        return;
    }
    if (seq_page() && c == 0u && ui.steps_held) {      /* a SEQ page, white keys held: KNOB 1 sets their steps */
        uint32_t k;
        for (k = 0; k < 16u; k++)
            if ((ui.steps_held >> k) & 1u)
                tp[sys.sel].steps[ui.slot][k] = (int8_t)clamp(tp[sys.sel].steps[ui.slot][k] + knob_steps(c, d, 100), 0, 100);
        return;                                         /* (LEN isn't the knob turned: its tag stays as it was) */
    }
    if (pdesc_empty(p))                                 /* an unused knob on a page */
        return;
    if (zoom_edge(p) && ui.home_held && ((fm1_in.buttons >> panel.btn[B_HOME]) & 1u)) {
        /* HOME held + a loop's edge (TAPE STRT LEN, POLY STRT END): the zoom. A detent moves it a hundredth of a
         * percent instead of a whole one (a fast turn goes further, as KNOB says), the picture zooms to the decoded
         * sound around the edge */
        *vp = (int16_t)clamp(*vp + knob_steps(c, d, 1000), p->min, p->max);
        ui.home_used = 1;
        ui.zoom = (uint8_t)(c + 1u);
        ui.hot = (uint8_t)c;
        ui.hot_t = 33;
        ui.last = (uint8_t)c;
        return;
    }
    v = param_nudge(p, *vp, knob_steps(c, d, p->fmt == F_ENUM ? 0 : (p->max - p->min) / param_step(p)));
    if (ui.view == VIEW_MIXER && ui.chan == CHAN_SETUP)
        setup_set(c, v);
    else if (ui.view == VIEW_MIXER && !ui.chan)
        track[c].level = (uint8_t)v;
    else if (ui.view == VIEW_ROUTE)
        tp[c].recin = (uint8_t)v;
    else
        *vp = v;
    if (p == &DRM_P[DM_PATN]) {                         /* PATN: its rhythm written (an edited pattern kept) */
        char m[40];
        int kept = drm_dirty[sys.sel];
        drm_patn(sys.sel, (uint32_t)v);
        str_cpy(m, N_DPATN[v % (int32_t)DRM_NPRESET], 8);
        str_cpy(m + str_len(m), kept ? " WRITTEN. HOLD SAVE: UNDO" : " WRITTEN", 28);
        if (kept)
            ui.undo = UNDO_DRUM;
        ui_message(m);
    } else if (p == &DRM_P[DM_LEN] && tp[sys.sel].drm[DM_BAR] > v) {   /* (the bar shown stays in the pattern) */
        tp[sys.sel].drm[DM_BAR] = v;
    }
    ui.hot = (uint8_t)c;
    ui.hot_t = 33;
    ui.last = (uint8_t)c;
    if (ui.glo_held)
        ui.glo_used = 1;
    if (ui.chan_held)
        ui.chan_used = 1;
}

/* the USB record mode's controls (usbrec.c): REC, HOME and the white keys; the rest wait until it closes */
static void usbrec_input(uint32_t pressed, uint32_t notes)
{
    uint32_t id, k;
    sys.keys_live = 0;
    sys.keys_grain = sys.keys_reso = 0;
    for (id = 0; id < NB; id++)
        if ((pressed >> id) & 1u) {
            uint32_t b = panel_btn_of(id);
            if (b == B_REC)
                usbrec_rec();
            else if (b == B_HOME)
                usbrec_back();
        }
    for (k = 0; k < 27u; k++)
        if (((notes >> k) & 1u) && KEY_WHITE[k] != KEY_NONE)
            usbrec_pick(KEY_WHITE[k]);
    for (k = 0; k < 4u; k++) {                          /* CHOOSE: the take's START LEN GAIN FADE (otherwise turns
                                                         * are dropped, not saved up) */
        int32_t d = panel_enc(EN_K1 + k);
        if (d && ur.state == UR_CHOOSE) {
            usbrec_knob(k, d);
            ui.last = (uint8_t)k;
        }
    }
    (void)panel_enc(EN_SELECT);
    (void)panel_enc(EN_PRESET);
    (void)panel_enc(EN_ALGO);
    if (ur.state != UR_CHOOSE)
        ui.last = 0xFF;
    if (ur.state == UR_OFF) {                           /* it closed: the page it opened from */
        ui.view = VIEW_PAGE;
        ui.last = 0xFF;
        ui.force = 1;
    }
}

/* a take of set bars just ended (tape_poll): say so */
static void bars_said(void)
{
    uint32_t t;
    char m[40];
    for (t = 0; t < NTRK; t++)
        if ((tape_bars_done >> t) & 1u) {
            tape_bars_done &= (uint8_t)~(1u << t);
            str_cpy(m, "T", sizeof m);
            m[1] = (char)('1' + t);
            m[2] = 0;
            str_cpy(m + 2, ": THE TAKE IS IN, ", 20);
            vz_secs(m + str_len(m), tape_ms(t) / 100);
            str_cpy(m + str_len(m), " SEC", 5);
            ui_message(m);
        }
}

static void ui_input(void)
{
    uint32_t released = 0, pressed = fm1_input_edges(&released), notes = fm1_input_note_edges(), id, k;
    int32_t d;
    if (ui.view == VIEW_USBREC) {
        usbrec_input(pressed, notes);
        return;
    }
    for (id = 0; id < NB; id++)
        if ((pressed >> id) & 1u) {
            uint32_t b = panel_btn_of(id);
            if (b < NB && b != B_SCL && scl_down() && (b == focus_btn() || (b == B_LFO && ui.view == VIEW_PAGE &&
                                                                       ui.kind == FOCUS_SLOT && ui.slot == ui.lfo_slot)))
                scl_page();                             /* SCL held + the page's own pad: its defaults */
            else if (b < NB)
                on_button(b);
        }
    if (ui.chan_held && (((released >> panel.btn[B_EDIT]) & 1u) || !((fm1_in.buttons >> panel.btn[B_EDIT]) & 1u))) {
        ui.chan_held = 0;                               /* EDIT let go: a tap keeps the channel page, a hold that
                                                         * turned something (or a long one) goes back to the levels */
        if (!ui.chan_used && (uint32_t)(fm1_ms - ui.chan_t0) < HOLD_MS[settings_hold % 4u] && ui.view == VIEW_MIXER) {
            ui.chan_latched = 1;
        } else {
            ui.chan = 0;
            ui.chan_latched = 0;
            ui.last = 0xFF;
        }
    }
    hold_keys();
    if (tape_bars_done)
        bars_said();
    if (ui.scl_held && !((fm1_in.buttons >> panel.btn[B_SCL]) & 1u))
        ui.scl_held = 0;                                /* (SCL let go) */
    if (ui.glo_held && (((released >> panel.btn[B_GLO]) & 1u) || !((fm1_in.buttons >> panel.btn[B_GLO]) & 1u)))
        glo_up();
    if (ui.pg_pad < NB && !((fm1_in.buttons >> panel.btn[ui.pg_pad]) & 1u)) {   /* a page's pad let go: EDIT FX and
                                                                               * ARP turn their page now, unless a
                                                                               * page key was used (HOME, the slots:
                                                                               * below, by their own flags) */
        uint32_t b = ui.pg_pad;
        ui.pg_pad = NB;
        if (!ui.pg_used) {
            if (b == B_EDIT)
                pad_devices(DEV_GRAIN, DEV_RESO);
            else if (b == B_FX)
                pad_devices(DEV_COLOR, DEV_SPACE);
            else if (b == B_ARP)
                focus_arp();
        }
    }
    if (ui.home_held && !((fm1_in.buttons >> panel.btn[B_HOME]) & 1u)) {   /* HOME let go: the source's next page,
                                                                           * unless it was held for something */
        ui.home_held = 0;
        ui.zoom = 0;
        if (!ui.home_used)
            pad_devices(DEV_SRC, DEV_SRC);
    }
    ui.steps_held = (uint16_t)(seq_page() ? white_keys(fm1_in.notes) : 0u);
    {                                                   /* DRUM: STEP's mode; an instrument held on the other pages */
        uint32_t w = white_keys(fm1_in.notes), m = drm_step_page() ? (uint32_t)tp[sys.sel].drm[DM_MODE] : DMODE_HITS;
        sys.drum_mode = (uint8_t)(m == DMODE_LIVE || m == DMODE_ERAS ? m : DMODE_HITS);
        if (drm_page() && !drm_step_page() && (!pad_down() || ui.drm_inst) && !ui.glo_held && w) {   /* (a key held
                                                                        * first, then a modulator's pad: its depths) */
            for (k = 0; !((w >> k) & 1u); k++)
                ;
            if (ui.drm_inst != k + 1u)
                ui.last = 0xFF;
            ui.drm_inst = (uint8_t)(k + 1u);
        } else {
            if (ui.drm_inst)
                ui.last = 0xFF;
            ui.drm_inst = 0;
        }
    }
    sys.black_short = (uint8_t)(!sys.keys_notes || ui.glo_held);   /* (the 0 key's freeze: chain.c) */
    sys.keys_live = (uint8_t)(!ui.glo_held && !seq_page() && !pad_down() &&   /* (a page pad held: the keys pick
                                                                            * pages, they don't play) */
                              !(drm_step_page() && sys.drum_mode == DMODE_HITS));   /* under GLO the white keys pick
                                                                * the track, on a SEQ page the steps, on DRUM's STEP
                                                                * (HITS, ACC) the bar's steps: they don't play */
    sys.keys_grain = (uint8_t)(ui.view == VIEW_PAGE && ui.kind == FOCUS_DEV && ui.dev == DEV_GRAIN);
    sys.keys_reso = (uint8_t)(ui.view == VIEW_PAGE && ui.kind == FOCUS_DEV && ui.dev == DEV_RESO);
    sys.keys_rarp = (uint8_t)(ui.view == VIEW_PAGE && ui.kind == FOCUS_ARP && ui.page == 2u);
    mod_show = (uint8_t)(seq_page() ? 1u + sys.sel * NSLOT + ui.slot % NSLOT : 0u);   /* (its keys show its step) */
    if ((uint32_t)(fm1_ms - ui.unsaved_t) >= 250u) {    /* the header's unsaved mark: the state against the last save
                                                        * or load (a hash of 3.3 KB: four times a second, not a frame) */
        ui.unsaved_t = fm1_ms;
        ui.unsaved = (uint8_t)pj_changed();
    }
    for (k = 0; k < 27u; k++)
        if ((notes >> k) & 1u) {
            if (KEY_BLACK[k] != KEY_NONE) {
                on_black(KEY_BLACK[k]);
            } else if (ui.glo_held && KEY_WHITE[k] < sys.ntrk) {
                sys.sel = KEY_WHITE[k];                  /* GLO + white key 1..TRACKS: the track */
                ui.glo_used = 1;
            } else if (pad_down()) {                     /* a page's pad held + white key n: its page n */
                pad_go(ui.pg_pad, KEY_WHITE[k]);
            } else if (drm_step_page() && sys.drum_mode == DMODE_HITS && !ui.glo_held) {
                drm_toggle(sys.sel, KEY_WHITE[k]);      /* STEP (HITS, ACC): the step toggled */
            }
        }
    for (k = 0; k < 4u; k++)
        if ((d = panel_enc(EN_K1 + k)) != 0)
            on_knob(k, d);
    if (ui.slot_held != SLOT_NONE && !((fm1_in.buttons >> panel.btn[ui.slot_btn % NB]) & 1u))
        slot_up();                                      /* (after the knobs: a turn in the same pass counts) */
    if ((d = panel_enc(EN_SELECT)) != 0) {
        if (ui.slot_held != SLOT_NONE) {
            /* a slot's pad held + SELECT: its engine (LFO ADSR SEQ FOLLOW), from its defaults; its depths stay. The
             * SEQ pad's stays the sequencer */
            uint32_t s = ui.slot_held, i, e;
            char m[40] = "M";
            ui.slot_used = 1;
            for (i = 0; i < 4u && SWAP_ENGINE[i] != tp[sys.sel].engine[s]; i++)
                ;
            e = SWAP_ENGINE[(uint32_t)(((int32_t)(i % 4u) + d % 4 + 4) % 4)];
            if (s == SLOT_SEQ) {
                ui_message("M4 IS THE SEQUENCER");
            } else {
                param_engine(sys.sel, s, e);
                if (ui.view == VIEW_PAGE && ui.kind == FOCUS_SLOT && ui.slot == s) {
                    ui.page = 0;
                    ui.last = 0xFF;
                } else {                                /* (another page up: say what it is now, and that its
                                                         * depths are still there: the new engine drives them) */
                    uint32_t nd = mod_slot_count(sys.sel, s);
                    fmt_int(m + 1, SLOT_NUM[s % NSLOT]);
                    str_cpy(m + str_len(m), " IS ", 5);
                    str_cpy(m + str_len(m), ME_NAME[e], 8);
                    if (nd) {
                        str_cpy(m + str_len(m), ", ", 3);
                        fmt_int(m + str_len(m), (int32_t)nd);
                        str_cpy(m + str_len(m), nd == 1u ? " DEPTH KEPT" : " DEPTHS KEPT", 13);
                    }
                    ui_message(m);
                }
            }
        } else if (ui.home_held && ((fm1_in.buttons >> panel.btn[B_HOME]) & 1u)) {
            /* HOME held + SELECT, from any page: the track's source (TAPE, SYNTH, POLY, DRUM), shown on its first
             * page; each keeps its own knobs */
            tp[sys.sel].src = (uint8_t)(((int32_t)tp[sys.sel].src + d % (int32_t)NSRC + (int32_t)NSRC) % (int32_t)NSRC);
            focus_dev(DEV_SRC, 0);
            ui.home_used = 1;
        } else if (ui.rec_held && ((fm1_in.buttons >> panel.btn[B_REC]) & 1u)) {
            /* REC held + SELECT: the bars a take records (FREE, 1, 2, 4, 8): armed, it starts on the downbeat and
             * stops by itself, exactly that long (tape.c). The REC press this turn came with does nothing else (its
             * arm, playing, is taken back) */
            static const uint8_t BARS[5] = {0, 1, 2, 4, 8};
            uint32_t i;
            char m[40];
            for (i = 0; i < 4u && BARS[i] != sys.rec_bars; i++)
                ;
            i = (uint32_t)clamp((int32_t)i + (d > 0 ? 1 : -1), 0, 4);
            sys.rec_bars = BARS[i];
            if (!ui.rec_turned && sys.playing)
                rec_arm();
            ui.rec_turned = 1;
            if (sys.rec_bars) {
                str_cpy(m, "REC: ", sizeof m);
                fmt_int(m + 5, sys.rec_bars);
                str_cpy(m + str_len(m), sys.rec_bars == 1u ? " BAR FROM THE DOWNBEAT" : " BARS FROM THE DOWNBEAT", 26);
            } else {
                str_cpy(m, "REC: FREE (UNTIL REC AGAIN)", sizeof m);
            }
            ui_message(m);
        } else if (ui.glo_held) {                       /* GLO held + SELECT: TRACKS (how many are in use), as a pad
                                                         * held + SELECT picks that pad's thing elsewhere */
            chain_tracks((int32_t)sys.ntrk + d);
            ui.last = 0xFF;
            ui.glo_used = 1;
        } else {                                        /* SELECT: the global tempo, on every view (KNOB ACC: a fast
                                                         * turn goes further; 2X, 4X leave it a BPM a detent) */
            sys.bpm = (uint16_t)clamp((int32_t)sys.bpm + (((bryo_prefs >> 1) & 3u) == KNOB_ACC ? knob_steps(4, d, 200) : d), 40, 240);
        }
    }
    if ((d = panel_enc(EN_PRESET)) != 0)               /* PRESETS: the project view, the slot it points at */
        pj_turn(d);
    if (ui.pj_ask && (uint32_t)(fm1_ms - ui.pj_ask_t) >= 3000u)   /* (a second press that didn't come) */
        ui.pj_ask = PJ_ASK_NONE;
    if ((d = panel_enc(EN_ALGO)) != 0) {               /* ALGORITHM: the routing view (REC IN); turned on it, the
                                                         * focused track's */
        if (ui.view != VIEW_ROUTE) {
            ui.view = VIEW_ROUTE;
            ui.chan = 0;
            ui.chan_latched = 0;
            ui.glo_latched = 0;
            ui.last = 0xFF;
        } else {
            tp[sys.sel].recin = (uint8_t)clamp((int32_t)tp[sys.sel].recin + d, 0, (int32_t)NRIN - 1);
            ui.hot = sys.sel;
            ui.hot_t = 33;
            ui.last = sys.sel;
        }
    }
}
