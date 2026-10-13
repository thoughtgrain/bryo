/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: the visualization panel under the pictograms (y 124..215, dot rows 0..45 of its own canvas). One picture
 * per view, drawn from the parameter values (not from audio), so it is right the moment a knob turns and costs
 * nothing while nothing changes.
 *
 * The look follows the pictograms above it (ui_px.c): one ink on the background, 1-dot strokes, square nodes where
 * a value sits, dotted drop lines and guides, small 3 x 5 labels under the plot, the way a groovebox draws an
 * envelope. The page's last-turned knob gets its label inverted here too, where the picture has one.
 *
 *   SYNTH      per page: the oscillators' cycles, the filter's response, the envelope, the keys and voices
 *   POLY       per page: the sound with STRT and the keys' range, the envelope, the filter with its TYPE
 *   DRUM       PATTERN, VARY: the bar as a grid of what this pass plays (written, left out, added); KIT: the
 *              instrument's sound; STEP: the instrument's row of the bar
 *   TAPE       the whole tape's sound, large, lit inside the loop window (STRT, LEN, bracketed) with its 16 slices
 *              ticked under it; the playhead over it (stopped: where playing starts, moved by ROTA)
 *   GRAIN      the grains as they sound: where each reads (across), its pitch (up), its stretch of the sound
 *              under its window with the playhead running its way; the cursor and SPRY's reach; FROZEN
 *   RIPPLE  the response over 8 octaves: peaks on PTCH's partials (harmonics, or a scale's chord tones; a node on
 *              each, the root's filled and named), as sharp as DEC makes them, kept up top by TONE, shaped by the
 *              filter before them (dotted: CUT RES TYPE), blended with the dotted dry line by WET
 *   COLOR      a sine through the device (dotted: in; solid: out); the inset follows the last-turned knob: the
 *              transfer curve (DRV, CRSH, CMOD), the noise after a hit (NOIS, NDEC), its filter (NTON), TILT
 *   SPACE      the dry hit, the echoes as stems with nodes (TIME apart, falling by FDBK) and the reverb tail
 *   MOD slots  LFO's shape (RND: its loop of steps) and its next time round dotted, ADSR's envelope with its
 *              stages named and bent, SEQ's 16 steps as bars of their values, FOLLOW's envelope over what it listens to
 *   REC IN     (ALGORITHM) who records whom: the tracks as boxes, a line from each one heard into the recorder
 *   MIXER      the levels: what each track holds of the shared memory, TRACKS and the time free; EDIT (the
 *              channel): the selected track's EQ and filter as one response, the pan as two speakers
 *
 * The DSP of each device will use the same mappings as these pictures (the comments name them), so what is drawn
 * is what is heard. Integer only: no float on the device. */

#define VZ_H 92                      /* px */
#define DX0 2                        /* the plot, in dots */
#define DX1 117
#define DW (DX1 - DX0)
#define DY0 1
#define DY1 34                       /* the plot's floor */
#define DH (DY1 - DY0)
#define DMID ((DY0 + DY1) / 2)
#define DLBL 38                      /* the label row (3 x 5) */

static uint32_t vz_seed;
static uint32_t vz_rand(void)        /* deterministic: the same picture for the same values */
{
    vz_seed = vz_seed * 1664525u + 1013904223u;
    return vz_seed >> 8;
}

/* a value's node: a 3 x 3 square, solid or open, centred on (x, y) */
static void vz_node(int32_t x, int32_t y, int solid)
{
    if (solid)
        px_box(x - 1, y - 1, 3, 3, px_ink);
    else
        px_frame(x - 1, y - 1, 3, 3, px_ink, 1);
}

/* a label under the plot centred on x (kept on the panel); inverted when it names the last-turned knob */
static void vz_label(int32_t x, const char *s, int on)
{
    int32_t w = px_text_w(PXF_3, s), x0 = clamp(x - w / 2, 1, 118 - w);
    if (on)
        px_tag(x0 - 1, DLBL - 1, PXF_3, s, px_ink, px_bg);
    else
        px_text(x0, DLBL, PXF_3, s, px_ink);
}

/* the last-turned knob of a page 2 as an inverted tag, "MIX 80%", its right end at xr (in 3 x 5) */
static void vz_ktag(int32_t xr, int32_t y, const pdesc_t *d, int32_t v)
{
    char b[16], val[12];
    const char *u;
    param_format(d, v, val, &u);
    str_cpy(b, d->label, sizeof b);
    str_cpy(b + str_len(b), " ", 2);
    str_cpy(b + str_len(b), val, 8);
    str_cpy(b + str_len(b), u, 4);
    px_tag(xr - px_text_w(PXF_3, b) - 1, y, PXF_3, b, px_ink, px_bg);
}

/* a gain in dB as a factor, x1000 (2x per 6 dB, straight between) */
static int32_t db_x1000(int32_t db)
{
    int32_t f = 1000;
    for (; db >= 6; db -= 6)
        f *= 2;
    for (; db <= -6; db += 6)
        f /= 2;
    return db >= 0 ? f + f * db / 6 : f - f * (-db) / 12;
}

/* a polyline through n points of xs, ys */
static void vz_poly(const int32_t *xs, const int32_t *ys, int32_t n, uint16_t c, int32_t step)
{
    int32_t k;
    for (k = 1; k < n; k++)
        px_line(xs[k - 1], ys[k - 1], xs[k], ys[k], c, step);
}

/* -------------------------------------------------------------- ZOOM --- */
/* The zoom (HOME held + a loop's edge: TAPE STRT LEN, POLY STRT END): the sound itself around the edge at e (tape
 * samples), decoded, each dot column the lowest and highest sample under it; what plays lit (right of a start,
 * left of an end: right), the rest dim; the edge a line in orange. The window is half a percent of the sound either
 * way, at least 23 ms (512 samples) and at most 370 ms, so a detent (a hundredth of a percent) moves the line a
 * dot or so on any length. Under it: the edge's name and time, and the window's half width. */
static void vz_zoom(const tape_view_t *tv, int32_t e, int right, const char *name)
{
    static tape_rd_t rd;
    int32_t w = clamp((int32_t)tv->len / 200, 512, 8192), x, mid = DMID, amp = DH / 2 - 1, cx = DX0 + DW / 2;
    char b[24];
    rd.ok = 0;
    px_line(DX0, mid, DX1, mid, px_dim, 2);
    for (x = DX0; x <= DX1; x++) {
        int32_t s0 = e - w + (x - DX0) * 2 * w / DW, s1 = e - w + (x + 1 - DX0) * 2 * w / DW, s, lo = 0, hi = 0;
        int in = right ? x >= cx : x < cx;
        for (s = s0; s < s1 || s == s0; s++) {
            int32_t y = s >= 0 && (uint32_t)s < tv->len ? tape_at(tv, &rd, s) : 0;
            lo = y < lo ? y : lo;
            hi = y > hi ? y : hi;
        }
        px_box(x, mid - hi * amp / 32768, 1, (hi - lo) * amp / 32768 + 1, in ? px_ink : px_dim);
    }
    px_line(cx, DY0, cx, DY1, px_live, 1);                             /* the edge */
    px_box(cx - 2, DY0, 5, 1, px_live);
    px_box(cx - 1, DY0 + 1, 3, 1, px_live);
    str_cpy(b, name, sizeof b);                                        /* "IN 1.234 S" */
    str_cpy(b + str_len(b), " ", 2);
    fmt_int(b + str_len(b), e / TAPE_SR);
    str_cpy(b + str_len(b), ".", 2);
    {
        int32_t ms = e % TAPE_SR * 1000 / TAPE_SR;
        char d3[4] = {(char)('0' + ms / 100), (char)('0' + ms / 10 % 10), (char)('0' + ms % 10), 0};
        str_cpy(b + str_len(b), d3, 4);
    }
    str_cpy(b + str_len(b), " S", 3);
    px_tag(1, DLBL - 1, PXF_3, b, px_ink, px_bg);
    str_cpy(b, "+-", sizeof b);
    fmt_int(b + 2, w * 1000 / TAPE_SR);
    str_cpy(b + str_len(b), " MS", 4);
    px_text(118 - px_text_w(PXF_3, b), DLBL, PXF_3, b, px_dim);
}

/* -------------------------------------------------------------- TAPE --- */
static void viz_adsr(const int16_t *v, uint32_t f);
static void viz_tape(const int16_t *v, uint32_t f)
{
    if (ui.zoom && (f == TK_STRT || f == TK_LEN)) {   /* HOME held + STRT or LEN: the zoom on that edge */
        tape_view_t tv;
        int32_t ls, ll;
        tape_view(sys.sel, &tv);
        if (tv.len) {
            tape_window_of(sys.sel, tv.len, &ls, &ll);
            vz_zoom(&tv, f == TK_STRT ? ls : ls + ll, f == TK_STRT, f == TK_STRT ? "IN" : "OUT");
            return;
        }
    }
    if (pdesc_at(DEV_P[DEV_SRC], 4u * ui.page) / 4u == 3u) {   /* the envelope's page: the envelope, as SYNTH's */
        int16_t a[NPK] = {0};
        a[0] = v[TK_ATK];
        a[1] = v[TK_DEC];
        a[2] = v[TK_SUS];
        a[3] = v[TK_REL];
        a[10] = 100;
        viz_adsr(a, f >= TK_ATK && f <= TK_REL ? f - TK_ATK : 0xFFu);
        return;
    }
    /* The whole of what the track plays (its tape or a reel, named at the top left), its sound drawn large from the
     * blocks' peaks (each dot column the loudest block under it, so no hit falls between columns): lit inside the
     * loop window (STRT, LEN, bracketed), dim outside, with the 16 slices the white keys play ticked under it. The
     * playhead is where the head is while it runs (a dotted cut through the sound), else where playing starts
     * (ROTA's point; the end when reversed); REC armed tagged at the top right. SPD's direction and speed are the
     * strip's pictogram above, so the sound gets the rows they took here. */
    int32_t r0 = 6, r1 = 114, rw = r1 - r0, top = 10, bot = 35, mid = 23, amp = 12, k, x;
    int32_t x0 = r0 + v[0] * rw / 10000, x1 = x0 + v[1] * rw / 10000;   /* (hundredths of a percent) */
    int32_t sp = v[2] * (v[5] ? -1 : 1) / (v[6] ? 2 : 1), gain = db_x1000(v[7]) * db_x1000(v[TK_LVL]) / 1000;   /* REV,
                                                                                * HALF; GAIN, LVL */
    int32_t fw = 1 + v[4] * 8 / 100;                   /* XFAD (drawn wider than to scale, so it shows) */
    tape_view_t tv;
    uint32_t nb;
    if (x1 > r1)
        x1 = r1;
    tape_view(sys.sel, &tv);
    nb = tv.len / TAPE_BLK;
    px_line(r0, mid, r1, mid, px_dim, 2);                            /* the centre line */
    for (x = r0; x <= r1 && nb; x++) {                                 /* the sample: each column's loudest block */
        uint32_t b0 = (uint32_t)(x - r0) * nb / (uint32_t)(rw + 1), b1 = (uint32_t)(x - r0 + 1) * nb / (uint32_t)(rw + 1), b, pk = 0;
        int32_t a;
        for (b = b0; b <= b1 && b < nb; b++)
            pk = tv_peak(&tv, b) > pk ? tv_peak(&tv, b) : pk;
        a = clamp((int32_t)pk * amp * gain / (255 * 1000), 0, amp);   /* (GAIN: clips at the frame) */
        if (a)
            px_box(x, mid - a, 1, 2 * a + 1, x >= x0 && x <= x1 ? px_ink : px_dim);
    }
    for (k = 1; k < 16; k++)                                           /* the slices, ticked under the window */
        px_dot(x0 + (x1 - x0) * k / 16, bot + 1, px_dim);
    for (k = 0; k < 2; k++) {                                          /* the brackets, 2 dots wide when turned */
        int32_t bx = k ? x1 : x0, s = k ? -1 : 1, w = f == (uint32_t)k ? 2 : 1;
        px_box(k ? bx - w + 1 : bx, top - 2, w, bot - top + 5, px_ink);
        px_line(bx, top - 2, bx + 2 * s, top - 2, px_ink, 1);
        px_line(bx, bot + 2, bx + 2 * s, bot + 2, px_ink, 1);
    }
    if (v[4] && x1 - x0 > 2 * fw + 2) {               /* XFAD: the ramps at the loop's ends, dotted */
        px_line(x0 + 1, bot - 1, x0 + fw, top + 1, px_ink, 2);
        px_line(x1 - fw, top + 1, x1 - 1, bot - 1, px_ink, 2);
    }
    {   /* the playhead: a triangle over the sound, where the head is (or where playing starts) */
        int32_t xr, hd = tape_head(sys.sel), px;
        xr = x0 + (x1 - x0) * v[9] / 100;              /* ROTATE: where playing starts in the window */
        px = hd >= 0 ? r0 + hd * rw / 1000 : sp < 0 ? (v[9] ? xr - 1 : x1 - 3) : (v[9] ? xr : x0 + 3);
        for (k = 0; k < 3; k++)                        /* (running: orange, alive) */
            px_box(px - 2 + k, top - 5 + k, 5 - 2 * k, 1, hd >= 0 ? px_live : px_ink);
        if (hd >= 0) {                                 /* running: a cut through the sound, and its edges */
            px_box(px - 1, top + 1, 3, bot - top - 1, px_bg);
            px_line(px, top + 1, px, bot - 1, px_live, 2);
        }
    }
    px_text(1, 0, PXF_3, tape_name(sys.sel), px_ink);                /* what it plays */
    if ((sys.rec >> sys.sel) & 1u)
        px_tag(105, 0, PXF_3, "REC", px_rec, px_bg);   /* (recording: red, as the header's bar) */
    vz_label(x0, "IN", f == 0u);
    if (x1 - x0 >= 22 || f == 1u)
        vz_label(x1, "OUT", f == 1u);
    if (f == 3u || f == 2u)
        vz_label(60, f == 3u ? "DUB" : "SPD", 1);   /* (SPD's turn still names it: the playhead's side shows REV) */
    if (f >= 4u && f < NPK && !pdesc_empty(&DEV_P[DEV_SRC][f]))
        vz_ktag(78, DLBL - 1, &DEV_P[DEV_SRC][f], v[f]);
}

/* a source's LEVEL page (SYNTH 5, POLY 4): the dB scale from -24 to +6, the level as a bar along it, 0 dB marked */
static void vz_level(int32_t db, const pdesc_t *d, int on)
{
    int32_t x, k, y0 = DMID - 5, y1 = DMID + 5, xl = DX0 + (clamp(db, -24, 6) + 24) * DW / 30, xz = DX0 + 24 * DW / 30;
    px_frame(DX0, y0, DW + 1, y1 - y0 + 1, px_dim, 2);
    px_box(DX0 + 1, y0 + 2, xl - DX0, y1 - y0 - 3, px_ink);
    for (x = xz, k = y0 - 4; k <= y1 + 3; k += 2)                     /* 0 dB: where the source is as it plays */
        px_dot(x, k, px_ink);
    for (k = -24; k <= 6; k += 6) {                                   /* the scale */
        char b[6];
        x = DX0 + (k + 24) * DW / 30;
        px_line(x, y1 + 2, x, y1 + 3, px_dim, 1);
        if (k > 0) {
            b[0] = '+';
            fmt_int(b + 1, k);
        } else {
            fmt_int(b, k);
        }
        px_text(clamp(x - px_text_w(PXF_3, b) / 2, 1, 118 - px_text_w(PXF_3, b)), DLBL, PXF_3, b, px_dim);
    }
    px_text(DX0, DY0, PXF_3, "LEVEL", px_ink);
    if (on)
        vz_ktag(118, DY0 - 1, d, db);
}

/* ------------------------------------------------------------- GRAIN --- */
/* d, a distance along a loop of ll samples, the short way round */
static int32_t gr_wrap(int32_t d, int32_t ll)
{
    while (d > ll / 2)
        d -= ll;
    while (d < -ll / 2)
        d += ll;
    return d;
}

static void viz_grain(const int16_t *v, uint32_t f)
{
    /* A field of the grains as they sound, not the whole sample. Across: where a grain reads; up and down: its pitch
     * (PTCH and its PRND, the sound's own in the middle, +-2 octaves at the edges). A grain is drawn while it sounds:
     * the stretch it reads (wider when it reads faster, narrower slowed down), its height its window (CONT) swelled
     * by the level there, played solid and still to play dim, its playhead an arrow the way it runs (REV: right to
     * left). Nothing sounding (stopped, or WET 0): one grain's outline at the cursor, from the knobs.
     *   SCAN TAPE: the stretch of TAPE's loop window around the cursor (in the middle) that grains can reach, SPRY's
     *              range dotted under it.
     *   the buffer (STR POS DLY): all of it, its bars (the bar lines dotted, the beats ticked), the write head a
     *              solid line while it records (gone while frozen), the cursor the triangle on top, the sound's
     *              level along the bottom. WET 0: the bars it will hold, empty. */
    grain_trk_t *G = &grain[sys.sel];
    gr_buf_t *B = &gbuf[sys.sel];
    tape_view_t tv;
    uint32_t scan = (uint32_t)clamp(v[GP_SCAN], 0, 3), buf = scan != SCAN_TAPE;
    int32_t ls, ll, i, k, x, cx, yc0 = buf ? 16 : 19, n = 0, sp, reach, gspan, fw, cur, held = 0;
    char b[24];
    if (buf) {
        gr_buf_view(sys.sel, &tv);
        ls = 0;
        ll = (int32_t)tv.len;
        if (!ll && v[GP_WET]) {                                        /* on, but nothing to hold it in */
            px_text_c(0, 120, 12, PXF_3, "NO MEMORY FREE", px_dim);
            px_text_c(0, 120, 20, PXF_3, "FOR THE BUFFER", px_dim);
            goto labels;
        }
        if (!ll)                                                       /* WET 0: the bars it will hold, empty */
            ll = (int32_t)(gr_bars() * (5292000u / (sys.bpm ? sys.bpm : 120u)));
    } else {
        tape_view(sys.sel, &tv);
        if (!tv.len) {
            px_text_c(0, 120, 16, PXF_3, "NOTHING ON THE TAPE", px_dim);
            return;
        }
        tape_window_of(sys.sel, tv.len, &ls, &ll);
    }
    cur = G->cur >> 12;
    reach = ll * v[GP_SPRY] / 200;                                     /* SPRY: samples either way */
    gspan = (int32_t)((uint32_t)gr_rate(v[GP_PTCH] * 16) * ((uint32_t)v[GP_SIZE] * 441u / 10u) >> 12);   /* a grain, read */
    if (buf) {                                                         /* the whole buffer, left to right */
        fw = ll;
        cx = DX0 + (int32_t)((uint32_t)(cur - ls) * (uint32_t)DW / (uint32_t)ll);
    } else {                                                           /* the cursor's neighbourhood, centred */
        fw = clamp(2 * reach + 3 * gspan > 4 * gspan ? 2 * reach + 3 * gspan : 4 * gspan, 256, ll);
        cx = 60;
    }
#define GX(s) (buf ? DX0 + (int32_t)((uint32_t)((((s) - ls) % ll + ll) % ll) * (uint32_t)DW / (uint32_t)ll) \
                   : cx + gr_wrap((s) - cur, ll) * DW / fw)            /* a sample's column */
#define GY(st16) (yc0 - (st16) * (buf ? 11 : 14) / (24 * 16))        /* a pitch's row */
    if (buf) {                                                         /* the bars and beats, the write head, the level */
        uint32_t bars = tv.len && B->bars ? B->bars : gr_bars(), q;
        for (q = 0; q <= bars * 4u; q++) {
            int32_t xq = DX0 + (int32_t)(q * (uint32_t)DW / (bars * 4u));
            if (q % 4u == 0u)
                px_line(xq, 4, xq, 28, px_dim, 2);
            else
                px_dot(xq, 4, px_dim);
        }
        for (x = DX0; x <= DX1; x++) {                                 /* the level along the bottom: 3 dots at most */
            uint32_t s0 = (uint32_t)(x - DX0) * (uint32_t)ll / (uint32_t)DW, bb = s0 / TAPE_BLK;
            int32_t a = bb < tv.len / TAPE_BLK ? (int32_t)tv_peak(&tv, bb) * 4 / 256 : 0;
            if (a)
                px_box(x, 33 - a, 1, a, px_dim);
        }
        if (!grain_frozen && tv.len && (sys.playing || tp[sys.sel].src != SRC_TAPE)) {   /* (only while it records) */
            int32_t xw = GX(B->w);
            px_line(xw, 2, xw, 33, px_ink, 1);
        }
    } else {
        sp = reach * DW / fw;                                          /* SPRY's reach, dots */
        px_line(clamp(cx - sp, DX0, DX1), 36, clamp(cx + sp, DX0, DX1), 36, px_dim, 2);   /* where grains may start */
    }
    px_line(DX0, yc0, DX1, yc0, px_dim, 4);                            /* the sound's own pitch */
    for (k = 0; k < (int32_t)GR_SLOTS; k++) {                          /* the grains sounding */
        const grain_t *g = &gslot[k];
        int32_t rate, span, p0, a0, xa, xb, ph, yc, fr;
        if (!gr_on((uint32_t)k) || g->trk != sys.sel || !g->len)
            continue;
        n++;
        rate = g->inc < 0 ? -g->inc : g->inc;
        span = (int32_t)((uint32_t)rate * g->len >> 12);
        p0 = (g->pos >> 12) - ((g->inc * (int32_t)(g->len - g->left)) >> 12);   /* (fits: |inc| < 2^15, len < 2^15) */
        a0 = g->rev ? p0 - span : p0;
        xa = GX(a0);
        xb = xa + span * DW / fw;
        if (xb <= xa)
            xb = xa + 1;
        ph = xa + ((g->pos >> 12) - a0) * DW / fw;
        yc = clamp(GY(g->st16), 5, buf ? 28 : 33);
        fr = (int32_t)(g->fade * 1000u / g->len);                     /* each ramp, of the grain (1/1000) */
        for (x = clamp(xa, DX0, DX1); x <= clamp(xb, DX0, DX1); x++) {
            int32_t at = (x - xa) * 1000 / (xb - xa), e = 32767, s = a0 + (x - xa) * span / (xb - xa), h;
            uint32_t blk = (uint32_t)clamp(s, 0, (int32_t)tv.len - 1) / TAPE_BLK;
            int on = g->rev ? x >= ph : x <= ph;                       /* played */
            if (at < fr)
                e = GR_RAMP[at * 128 / (fr ? fr : 1)];
            else if (1000 - at < fr)
                e = GR_RAMP[(1000 - at) * 128 / (fr ? fr : 1)];
            h = (2 + clamp((int32_t)tv_peak(&tv, blk) * 3, 0, 255) * (buf ? 3 : 5) / 255) * e / 32767;   /* (quiet stretches: still a shape) */
            px_box(x, yc - h, 1, 2 * h + 1, on ? px_live : px_dim);   /* (what it's played: orange, alive) */
        }
        if (ph >= DX0 && ph <= DX1) {                                  /* the playhead, an arrow its way */
            int32_t d = g->rev ? -1 : 1;
            px_box(ph, yc - 3, 1, 7, px_ink);
            px_dot(ph + d, yc - 1, px_ink);
            px_dot(ph + d, yc + 1, px_ink);
            px_dot(ph + 2 * d, yc, px_ink);
        }
    }
    if (!n) {                                                          /* nothing sounding: a grain from the knobs */
        int32_t span = gspan, xa = cx, xb = cx + span * DW / fw, yc = clamp(GY(v[GP_PTCH] * 16), 5, buf ? 28 : 33);
        int32_t fr = v[GP_CONT] * 5, pyu = yc, pyd = yc, hm = buf ? 4 : 6;
        if (xb <= xa + 2)
            xb = xa + 3;
        for (x = xa; x <= xb && x <= DX1; x++) {
            int32_t at = (x - xa) * 1000 / (xb - xa), e = 32767, h;
            if (at < fr)
                e = GR_RAMP[at * 128 / (fr ? fr : 1)];
            else if (1000 - at < fr)
                e = GR_RAMP[(1000 - at) * 128 / (fr ? fr : 1)];
            h = hm * e / 32767;
            if (x > xa) {
                px_line(x - 1, pyu, x, yc - h, px_dim, 1);
                px_line(x - 1, pyd, x, yc + h, px_dim, 1);
            }
            pyu = yc - h;
            pyd = yc + h;
        }
        if (v[GP_REV]) {                                               /* REV: some run backwards */
            px_dot(xa - 1, yc - 1, px_dim);
            px_dot(xa - 1, yc + 1, px_dim);
            px_dot(xa - 2, yc, px_dim);
        }
    }
    for (i = 0; i < 3; i++)                                            /* the cursor, where grains start */
        if (cx >= DX0 && cx <= DX1)
            px_box(cx - 2 + i, i, 5 - 2 * i, 1, px_ink);
    if (grain_frozen)
        px_tag(118 - px_text_w(PXF_3, "FROZEN") - 1, 0, PXF_3, "FROZEN", px_note, px_bg);
#undef GX
#undef GY
labels:
    if (!v[GP_WET])
        str_cpy(b, "WET 0: OFF", sizeof b);
    else if (!n && gr_why(sys.sel))                                    /* no share: why ("STOPPED", "MUTED", "SILENT": nothing
                                                                        * in the buffer yet), not "0 OF 0" */
        str_cpy(b, gr_why(sys.sel), sizeof b);
    else {
        uint8_t sh[NTRK];                                              /* (worked out here as the ISR does: right */
        gr_plan_into(sh);                                              /*  before the first block has run too) */
        if (!sh[sys.sel])                                              /* AUTO, the held tracks have taken it all (its last
                                                                        * grains finishing: not "1 OF 0") */
            str_cpy(b, "TAKEN: NO GRAINS", sizeof b);
        else {
            fmt_int(b, n);                                             /* "5 OF 12 GRAINS": its share now; "5 OF */
            str_cpy(b + str_len(b), " OF ", 5);                        /* 12 HELD": GRNS 12, kept whatever else */
            fmt_int(b + str_len(b), sh[sys.sel]);
            str_cpy(b + str_len(b), v[GP_GRNS] > 0 ? " HELD" : " GRAINS", 8);
            held = v[GP_GRNS] > 0;
        }
    }
    if (f == 1u || f == 4u)
        px_tag(1, DLBL - 1, PXF_3, b, held ? px_note : px_ink, px_bg);
    else
        px_text(2, DLBL, PXF_3, b, held ? px_note : px_ink);   /* (GRNS holding: "HELD", in yellow) */
    if (f >= 5u && f < 16u && !pdesc_empty(&DEV_P[DEV_GRAIN][f]))
        vz_ktag(118, DLBL - 1, &DEV_P[DEV_GRAIN][f], v[f]);
    else if (buf) {                                                    /* the buffer: its bars ("STR 2 BARS") */
        char m[16];
        str_cpy(m, N_SCAN[scan], sizeof m);
        str_cpy(m + str_len(m), " ", 2);
        uint32_t bars = tv.len && B->bars ? B->bars : gr_bars();
        fmt_int(m + str_len(m), (int32_t)bars);
        str_cpy(m + str_len(m), bars > 1u ? " BARS" : " BAR", 6);
        px_text(118 - px_text_w(PXF_3, m), DLBL, PXF_3, m, px_dim);
    } else
        px_text(118 - px_text_w(PXF_3, tape_name(sys.sel)), DLBL, PXF_3, tape_name(sys.sel), px_dim);   /* its source */
}

/* --------------------------------------------------------- RIPPLE --- */
/* the partials: reso.c RS_PART16, the strings' table (the first four of a row sound) */
#define PART_16 RS_PART16
#define PART_N RS_PART_N

/* x as a pitch: MIDI 28 (E1, 41 Hz) .. 124 (8 octaves), in 1/16 semitones */
static int32_t reso_n16(int32_t x) { return 28 * 16 + (x - DX0) * 96 * 16 / DW; }

/* the filter before the strings (CUT, RES, TYPE) at pitch n16, 0..1000 (+ the resonance's peak, up to 2000):
 * CUT's index is about a semitone a step from 30 Hz (MIDI 23) */
static int32_t reso_filter(const int16_t *v, int32_t n16)
{
    int32_t c16 = (23 + v[4]) * 16, d = (n16 - c16) / 16, pk = v[5] * 10 * 64 / (64 + d * d), g;
    switch (v[6]) {
    case 1: g = 1000 - (d < 0 ? -d : d) * 40; break;                   /* BP: 6 dB an octave each side */
    case 2: g = d >= 0 ? 1000 : 1000 + d * 80; break;                  /* HP */
    default: g = d <= 0 ? 1000 : 1000 - d * 80; break;                 /* LP: 12 dB an octave */
    }
    return clamp(g, 0, 1000) + pk;
}

/* peaks at the partials; the peak width narrows as DEC rises; TONE keeps the upper partials (dark at 0); the filter
 * shapes what reaches them; WET blends with the flat dry line (35 %), or (ROUT SEND) adds to it; 0..1000 */
static int32_t reso_at(const int16_t *v, int32_t x)
{
    int32_t n16 = reso_n16(x), width16 = 4 + (100 - v[1]) * 28 / 100, best = 0, k, sc = clamp(v[7], 0, RS_SCALES);
    for (k = 0; k < PART_N[sc]; k++) {
        int32_t d = n16 - (v[0] * 16 + PART_16[sc][k]), amp = 1000 - k * (100 - v[2]) * 9, pk;
        if (amp <= 0)
            break;
        if (d < -width16 * 6 || d > width16 * 6)
            continue;
        d = d < 0 ? -d : d;
        pk = amp * width16 * width16 / (width16 * width16 + d * d);   /* 1 / (1 + (d / w)^2) */
        if (pk > best)
            best = pk;
    }
    best = clamp(best * reso_filter(v, n16) / 1000, 0, 1000);
    if (v[RP_ROUT])
        return clamp(350 + best * v[3] / 100 * 65 / 100, 0, 1000);
    return (best * v[3] + 350 * (100 - v[3])) / 100;
}

#define RV_RING 192                   /* a string ringing, for the picture: its level (reso.c lvl) above about -45 dB */
/* the ripples (2026-10-13): a string ringing makes the curve wave round its node, out to 24 dots each way and dying
 * as it goes, as high as the string is loud (1..5 dots), its wavelength the string's pitch (an octave up: half as
 * long). The wave drifts with RIPL's phase, a quarter turn a string, so the drift runs across the strings as it does
 * in the sound, or slowly with time when RIPL is 0 (a turn in two seconds). Returns the curve's offset at x, and
 * sets *live when a string's ripple reaches x (the curve goes orange there, as the stems do) */
static int32_t reso_ripple(const int16_t *v, int32_t sc, int32_t x, int *live)
{
    const reso_t *R = &reso[sys.sel];
    int32_t k, sum = 0;
    for (k = 0; k < (int32_t)RS_N && k < PART_N[sc]; k++) {
        int32_t hx = DX0 + (v[0] * 16 + PART_16[sc][k] - 28 * 16) * DW / (96 * 16), lv = R->lvl[k], d, amp, lam, ph;
        d = x > hx ? x - hx : hx - x;
        if (lv <= RV_RING || d >= 24)
            continue;
        amp = 1 + clamp(lv, 0, 16384) * 4 / 16384;
        lam = 24 * 192 / (192 + PART_16[sc][k]);
        lam = lam < 6 ? 6 : lam;
        ph = v[RP_RIPL] ? (int32_t)((R->rph + (uint32_t)k * 0x40000000u) >> 26)
                        : (int32_t)(((fm1_ms >> 5) + (uint32_t)k * 16u) & 63u);
        sum += px_sin((d * 64 / lam + ph) & 63) * amp * (24 - d) / 24 / 1000;
        *live = 1;
    }
    return sum;
}

static void viz_reso(const int16_t *v, uint32_t f)
{
    int32_t x, k, py = 0, top = DY0 + 6, base = DY1 + 2, sc = clamp(v[7], 0, RS_SCALES);
    px_line(DX0, base - 350 * (base - top) / 1000, DX1, base - 350 * (base - top) / 1000, px_dim, 2);   /* dry */
    px_line(DX0, base + 1, DX1, base + 1, px_dim, 2);
    if (v[4] < 127 || v[5] || v[6] || f == 4u || f == 5u || f == 6u)   /* the filter: dotted, once it does anything */
        for (x = DX0; x <= DX1; x += 2)
            px_dot(x, base - clamp(reso_filter(v, reso_n16(x)), 0, 1000) * (base - top) / 1000, px_ink);
    for (x = DX0; x <= DX1; x++) {                                     /* the curve, rippling where a string rings */
        int live = 0;
        int32_t y = base - reso_at(v, x) * (base - top) / 1000 - reso_ripple(v, sc, x, &live);
        y = clamp(y, top - 4, base);
        if (x > DX0)
            px_line(x - 1, py, x, y, live ? px_live : px_ink, 1);
        py = y;
    }
    for (k = 0; k < PART_N[sc]; k++) {                                 /* a node on each partial still sounding */
        int32_t hx = DX0 + (v[0] * 16 + PART_16[sc][k] - 28 * 16) * DW / (96 * 16);
        if (hx > DX1 - 1 || 1000 - k * (100 - v[2]) * 9 <= 0 ||
            (k && hx - (DX0 + (v[0] * 16 + PART_16[sc][k - 1] - 28 * 16) * DW / (96 * 16)) < 3))
            break;                                                     /* (closer than 3 dots: the rest are a blur) */
        if (k < (int32_t)RS_N && reso[sys.sel].lvl[k] > RV_RING) {   /* a string ringing now: a stem up to its
                                                                        * node, both orange (the sound, alive) */
            px_line(hx, base, hx, base - reso_at(v, hx) * (base - top) / 1000, px_live, 1);
            px_box(hx - 1, base - reso_at(v, hx) * (base - top) / 1000 - 1, 3, 3, px_live);
        } else
            vz_node(hx, base - reso_at(v, hx) * (base - top) / 1000, k == 0);
    }
    {   /* PTCH: its note over its node */
        int32_t rx = DX0 + (v[0] * 16 - 28 * 16) * DW / (96 * 16);
        char b[8];
        const char *u;
        param_format(&DEV_P[DEV_RESO][0], v[0], b, &u);
        if (f == 0u)
            px_tag(clamp(rx - 3, 1, 110), 0, PXF_3, b, px_ink, px_bg);
        else
            px_text(clamp(rx - 2, 1, 110), 0, PXF_3, b, px_ink);
    }
    vz_label(DX0 + 8, "E1", 0);
    vz_label(DX1 - 8, "E9", 0);
    vz_label((DX0 + DX1) / 2, f == RP_ROUT ? N_ROUT[clamp(v[RP_ROUT], 0, 1)]
             : f == RP_MODE ? N_RMODE[clamp(v[RP_MODE], 0, 2)]
             : f >= 1u && f < NPK && !pdesc_empty(&DEV_P[DEV_RESO][f]) ? DEV_P[DEV_RESO][f].label : N_RSCAL[sc],
             f >= 1u && f != RP_MODE && f < NPK && !pdesc_empty(&DEV_P[DEV_RESO][f]));
    if (f >= 4u && f < 8u)
        vz_ktag(118, DY0 - 1, &DEV_P[DEV_RESO][f], v[f]);
}

/* ------------------------------------------------------------- COLOR --- */
/* the mappings color.c plays (color_shape there: the drive and the bits): DRV 0..100 -> gain 1..16 into the soft
 * clip (0: none); CRSH 0..100 takes, by CMOD, 16..2 bits (BIT), a hold of 1..12 samples (RATE), or both; NOIS:
 * noise riding the signal's envelope, which falls over NDEC after the sound; NTON: a one-pole filter on that noise
 * (- dark .. + bright); TILT: the low end against the high (on a sine: nothing to draw, so the inset shows it); WET:
 * the dry and the coloured signal; LVL: the output in dB */

static int32_t color_out(int32_t x, int32_t shaped, const int16_t *v)   /* WET (INS: a crossfade; SEND: added to the
                                                                           * dry), then LVL */
{
    if (v[CP_ROUT])
        return (x + shaped * v[7] / 100) * db_x1000(v[8]) / 1000;
    return (x + (shaped - x) * v[7] / 100) * db_x1000(v[8]) / 1000;
}

static void viz_color(const int16_t *v, uint32_t f)
{
    int32_t bx = DX0, bw = 32, wx = DX0 + 37, ww = DX1 - wx, i, py = 0, held = 0;
    int32_t hold = (int32_t)col_hold(v);                              /* CMOD RATE or BOTH: hold a value */
    int32_t fall = 1000 - 2000 / (2 + (int32_t)TIME_MS_X10[v[4] & 127] / 40), env = 0;   /* NDEC, per dot x1000 */
    int32_t lp = 0, a = 8 + (v[5] + 100) * 75 / 200, cy = DMID + 1, amp = DH / 2;   /* NTON: the filter's step */
    px_frame(bx, DY0, bw, DH + 2, px_dim, 2);                          /* the inset */
    if (f == 5u) {                                                     /* the noise filter's response */
        int32_t pyc = 0;
        for (i = 0; i < bw - 4; i++) {
            int32_t fr = i * 100 / (bw - 4), gg = 100 * a / (a + fr + 1), y = DY1 - 2 - gg * (DH - 6) / 100;
            if (i)
                px_line(bx + 1 + i, pyc, bx + 2 + i, y, px_ink, 1);
            pyc = y;
        }
    } else if (f == 3u) {                                              /* TILT: one line, leaning */
        px_line(bx + 2, cy, bx + bw - 3, cy, px_dim, 2);
        px_line(bx + 2, cy + v[3] * (amp - 4) / 100, bx + bw - 3, cy - v[3] * (amp - 4) / 100, px_ink, 1);
    } else if (f == 2u || f == 4u) {                                   /* the noise: a hit, and the noise falling */
        vz_seed = 99u;                                                 /* .. after it over NDEC */
        for (i = 2; i < bw - 2; i++) {
            int32_t e = i < 6 ? 1000 : env * fall / 1000, k, n = (e * (4 + v[2] * 2) / 1000 + 9) / 10;
            env = e;
            for (k = 0; k < n; k++)
                px_dot(bx + i, DY1 - 1 - (int32_t)(vz_rand() % (uint32_t)(1 + e * (DH - 3) / 1000)), px_ink);
        }
    } else {                                                           /* the transfer curve */
        int32_t pyc = 0;
        px_line(bx + 2, cy, bx + bw - 3, cy, px_dim, 2);
        for (i = 0; i < bw - 4; i++) {
            int32_t x = (i * 2 - (bw - 5)) * 32767 / (bw - 5);
            int32_t y = cy - clamp(color_out(x, color_shape(x, v), v), -32767, 32767) * (amp - 3) / 32767;
            if (i)
                px_line(bx + 1 + i, pyc, bx + 2 + i, y, px_ink, 1);
            pyc = y;
        }
    }
    px_line(wx, cy, DX1, cy, px_dim, 2);
    vz_seed = 777u;
    env = 0;
    for (i = 0; i <= ww; i++) {                                        /* two cycles of a sine: in dotted, out solid */
        int32_t s = sine_i((uint32_t)i * (0xFFFFFFFFu / (uint32_t)ww) * 2u), y, yi, nz, e;
        if (i % hold == 0)
            held = color_shape(s, v);                                  /* CMOD RATE: hold a value */
        e = s < 0 ? -s : s;
        env = e > env ? e : env * fall / 1000;                         /* NDEC: the noise's envelope */
        nz = ((int32_t)(vz_rand() & 0xFFFF) - 32768) * v[2] / 100;      /* NOISE, through NTON */
        lp += (nz - lp) * a / 100;
        y = color_out(s, held + (lp * env >> 16), v);
        y = cy - clamp(y, -32767, 32767) * (amp - 1) / 32767;
        yi = cy - s * (amp - 1) / 32767;
        if (i) {
            if (i % 2 == 0)
                px_dot(wx + i, yi, px_dim);
            px_line(wx + i - 1, py, wx + i, y, px_ink, 1);
        }
        py = y;
    }
    vz_label(bx + bw / 2, f == CP_ROUT ? N_ROUT[clamp(v[CP_ROUT], 0, 1)] : f < 10u ? DEV_P[DEV_COLOR][f].label : "CURVE", f < 10u);
    vz_label(wx + ww / 2, "IN : OUT", 0);
    if (f >= 4u && f < 9u)
        vz_ktag(118, DY0 - 1, &DEV_P[DEV_COLOR][f], v[f]);
}

/* ------------------------------------------------------------- SPACE --- */
/* the window is 2 s: the dry hit at 0, echoes TIME ms apart falling by FDBK, the tail rising over SIZE (10..90 ms)
 * and falling over DECAY (0.2..4.2 s). DLY or VERB at 0 is off (space.c: no memory taken): its part is drawn dim,
 * as it would be at 100, so TIME, FDBK, SIZE and DEC still show what they set */
/* SPACE on track t is sounding now: its delay, or the room it sends to, running and not resting (space.c: the wet
 * past -66 dB within 0.1 s) */
static int space_sounding(uint32_t t)
{
    const room_t *R = &room[clamp(tp[t].dev[DEV_SPACE][SP_ROOM], 0, 1)];
    return (space[t].dlive && space[t].quiet < 2205u) ||
           ((R->users >> t & 1u) && R->live && R->quiet < (R->qual ? 1103u : 2205u));
}

/* A room's tail over 2 s from column x0 to the plot's right, along rows y1 - h .. y1, as SPACE draws it: held back
 * by PRE, rising over SIZE (10..90 ms), falling over DEC (0.2..4.2 s) */
static void vz_tail(const int16_t *q, int32_t x0, int32_t y1, int32_t h, uint16_t ink)
{
    int32_t x, py = y1, rise = 10 + q[RM_SIZE] * 80 / 100, len = 200 + q[RM_DEC] * 40;
    for (x = x0; x <= DX1; x++) {
        int32_t ms = (x - x0) * 2000 / (DX1 - x0) - q[RM_PRE], e, y;
        if (ms < 0) {
            py = y1;
            continue;
        }
        e = ms < rise ? ms * 1000 / rise : 1000 - (ms - rise) * 1000 / len;
        if (e <= 0 && ms >= rise)                       /* (the end of the tail; at its first instant it is 0 too) */
            break;
        e = e * e / 1000;
        y = y1 - e * h / 1000;
        if (x > x0)
            px_line(x - 1, py, x, y, ink, 1);
        py = y;
    }
}

/* room r is sounding now: a track sends to it, and it's running and not resting (space.c: within 0.1 s) */
static int room_sounding(uint32_t r)
{
    return room[r].users && room[r].live && room[r].quiet < (room[r].qual ? 1103u : 2205u);
}

static void viz_space(const int16_t *v, uint32_t f)
{
    /* DLY scales the echoes, VERB the tail; TONE (- low-pass, + high-pass on the feedback) makes each echo lose a
     * little more. The tail is the room ROOM picks (its SIZE, DEC and PRE, on pages 2-3: shared by every track on
     * it, so its label names them when another track sends there too) */
    const int16_t *q = rmp[clamp(v[2], 0, 1)];
    int32_t t, x, py = DY1, rise = 10 + q[RM_SIZE] * 80 / 100, len = 200 + q[RM_DEC] * 40, g = 32767, first = -1;
    int32_t pre = q[RM_PRE];
    int32_t tl = 100 - (v[6] < 0 ? -v[6] : v[6]) / 5;                  /* TONE: what each pass keeps, % */
    int32_t tms = v[SP_SYNC] ? sp_sync_q8(v[0]) / 5645 : v[0];         /* TIME, ms (SYNC: the division's) */
    int32_t dly = v[4] ? v[4] : 100, verb = v[5] ? v[5] : 100;
    int live = space_sounding(sys.sel);                                /* (sounding now: the echoes and the tail in
                                                                        * orange; resting, white) */
    uint16_t dink = v[4] ? (live ? px_live : px_ink) : px_dim, vink = v[5] ? (live ? px_live : px_ink) : px_dim;
    px_line(DX0, DY1 + 1, DX1, DY1 + 1, px_dim, 2);
    for (x = DX0; x <= DX1; x++) {                                     /* the tail: its edge alone, no fill */
        int32_t ms = (x - DX0) * 2000 / DW - pre, e, y;
        if (ms < 0) {
            py = DY1;
            continue;
        }
        e = ms < rise ? ms * 1000 / rise : 1000 - (ms - rise) * 1000 / len;
        if (e <= 0 && ms >= rise)                       /* (the end of the tail; at its first instant it is 0 too) */
            break;
        e = e * e / 1000 * verb / 100;
        y = DY1 - e * DH / 1000;
        if (x > DX0)
            px_line(x - 1, py, x, y, vink, 1);
        py = y;
    }
    px_line(DX0 + 1, DY1, DX0 + 1, DY0 + 1, px_ink, 1);                /* the dry hit */
    vz_node(DX0 + 1, DY0 + 1, 1);
    for (t = tms; t < 2000 && g > 1500; t += tms) {                    /* the echoes: stems with nodes */
        int32_t h;
        g = (t == tms) ? 26000 : g * v[1] / 100 * tl / 100;
        x = DX0 + t * DW / 2000;
        h = g * DH / 32767 * dly / 100;
        if (first < 0)
            first = x;
        px_line(x, DY1, x, DY1 - h, dink, 1);
        if (v[4])
            vz_node(x, DY1 - h, f <= 1u || f == 4u);
        else
            px_dot(x, DY1 - h, px_dim);
    }
    if (f >= 2u && f < NPK && !pdesc_empty(&DEV_P[DEV_SPACE][f]))
        vz_ktag(118, DY0 - 1, &DEV_P[DEV_SPACE][f], *sp_knob(sys.sel, f));
    px_text(DX0, DLBL, PXF_3, "DRY", px_ink);
    if (first >= DX0 + 20 && first < DX1 - 20)
        vz_label(first + 2, f == 1u ? "FDBK" : "ECHO", f <= 1u || f == 4u);
    {                                                                  /* the room, and who shares it */
        char b[16];
        uint32_t u = room[clamp(v[2], 0, 1)].users, k, n;
        if (u & ~(1u << sys.sel)) {                                    /* "B: T1 T3" */
            b[0] = v[2] ? 'B' : 'A';
            b[1] = ':';
            for (k = 0, n = 2; k < NTRK; k++)
                if (u >> k & 1u) {
                    b[n++] = ' ';
                    b[n++] = 'T';
                    b[n++] = (char)('1' + k);
                }
            b[n] = 0;
        } else {
            str_cpy(b, v[2] ? "ROOM B" : "ROOM A", sizeof b);
            n = 6;
        }
        vz_label(DX1 - (int32_t)n * 2, b, f == 2u || f == 3u || f == 5u || f >= 8u);
    }
}

/* ------------------------------------------------------------- MODS --- */
/* The modulators share the S-4's placement knobs: AMT scales the shape, OFS moves it up or down, PHAS starts it
 * later in its cycle, SPRD is the right channel's phase against the left (drawn as a dotted second trace). A
 * page 2 or 3 knob you turn is tagged with its value at the right of the label row. */
static int32_t vz_place(int32_t y, int32_t amt, int32_t ofs)         /* Q15 -> Q15: AMT, then OFS */
{
    return clamp(y * amt / 100 + ofs * 327, -32767, 32767);
}

/* The LFO's random steps: a loop of LEN values (Q15), and the next time round, drifted by VAR. One table for
 * both RND's steps and, on the other shapes, each cycle's level (VAR's drift repeats every LEN cycles). */
static void lfo_rand(const int16_t *v, int16_t *now, int16_t *next)
{
    int32_t k;
    vz_seed = 4242u;
    for (k = 0; k < 16; k++)
        now[k] = (int16_t)(((int32_t)(vz_rand() % 2001u) - 1000) * 32);
    for (k = 0; k < 16; k++)
        next[k] = (int16_t)clamp(now[k] + ((int32_t)(vz_rand() % 2001u) - 1000) * 32 * v[6] / 100, -32767, 32767);
}

/* (lfo_at and isqrt: mod.c, shared with the sound) */

static void viz_wave(const int16_t *v, uint32_t f)
{
    /* A window of time, from PHAS: 4 seconds free-running (a tick a second), 2 bars on SYNC BPM (a tick a beat), so
     * RATE shows as how many cycles fit (half a cycle to 16: the ends held readable). SMTH slews what's drawn
     * (RND's steps glide); VAR: the next time round, dotted (RND: the loop's steps drifted; the other shapes: each
     * cycle's level drifted, repeating every LEN cycles). FADE fades the first cycle in; TRIG KEY marks the key that
     * restarts it; SPRD's right channel, dim. AMT and OFS place it all. */
    static const char *const NAME[5] = {"SINE", "TRIANGLE", "SQUARE", "SAW", "RANDOM"};
    int32_t i, py = DMID, sh = clamp(v[1], 0, 4), half = DH / 2 - 1, span, fade;
    int16_t now[16], nxt[16];
    int32_t sm = 0, smn = 0, smr = 0, k = 1000 - v[5] * 9, n = clamp(v[7], 1, 16);
    if (v[12])                                                         /* cycles x 1000 across the window */
        span = (int32_t)((8000u << 16) / MOD_DIV[(uint32_t)clamp(v[0], 0, 127) >> 3]);   /* 8 beats over the division */
    else
        span = (int32_t)(((lfo_inc(v[0]) >> 16) * 344531u) >> 12);     /* 4 s x the rate (cycles a block, Q32) */
    span = clamp(span, 500, 16000);
    fade = v[14] * (DW * 1000 / span) / 100;                           /* FADE: of the first cycle, in dots */
    lfo_rand(v, now, nxt);
    px_line(DX0, DMID, DX1, DMID, px_dim, 2);
    for (i = 0; i <= (v[12] ? 8 : 4); i++) {                           /* the ticks: beats, or seconds */
        int32_t x = DX0 + i * DW / (v[12] ? 8 : 4);
        px_line(x, DMID - 1, x, DMID + 1, v[12] ? px_ink : px_dim, 1);
    }
    if (v[13]) {                                                       /* a key going down, at the start */
        px_box(DX0, DY0 - 1, 3, 4, px_ink);
        px_line(DX0 + 1, DY0 + 3, DX0 + 1, DY1, px_ink, 3);
    }
    for (i = 0; i <= DW; i++) {
        int32_t ph = i * span / DW + v[10] * 1000 / 360, p = ph % 1000, cyc = ph / 1000, y, yn, yr;
        if (sh == 4) {
            y = lfo_at(v, p, now);
            yn = lfo_at(v, p, nxt);
        } else {                                                       /* VAR on a shape: each cycle's level */
            int32_t a0 = now[cyc % n] < 0 ? -now[cyc % n] : now[cyc % n], a1 = nxt[cyc % n] < 0 ? -nxt[cyc % n] : nxt[cyc % n];
            int32_t lv = 32767 - a0 * v[6] / 100, ln = 32767 - a1 * v[6] / 100;
            y = lfo_at(v, p, now);
            yn = y * (ln / 64) / 512;
            y = y * (lv / 64) / 512;
        }
        yr = lfo_at(v, (p + v[11] * 5) % 1000, now);
        if (i == 0) {
            sm = y;
            smn = yn;
            smr = yr;
        }
        sm += (y - sm) * k / 1000;                                     /* SMTH: a one-pole slew */
        smn += (yn - smn) * k / 1000;
        smr += (yr - smr) * k / 1000;
        y = sm;
        yn = smn;
        yr = smr;
        if (i < fade) {
            y = y * i / fade;
            yn = yn * i / fade;
            yr = yr * i / fade;
        }
        y = DMID - vz_place(y, v[8], v[9]) * half / 32767;
        if (v[6] && (i % 3) == 0)
            px_dot(DX0 + i, DMID - vz_place(yn, v[8], v[9]) * half / 32767, px_ink);
        if (v[11] && (i & 1) == 0)
            px_dot(DX0 + i, DMID - vz_place(yr, v[8], v[9]) * half / 32767, px_dim);
        if (i)
            px_line(DX0 + i - 1, py, DX0 + i, y, px_ink, 1);
        py = y;
    }
    vz_label(DX0 + 14, NAME[sh], f == 1u);
    if (f == 2u || f == 3u)
        vz_label(DX1 - 10, f == 2u ? "SKEW" : "FOLD", 1);
    else
        vz_label(DX1 - 12, v[12] ? "2 BARS" : "4 SEC", 0);           /* (the window's width) */
    if (f >= 4u && f < NPK)
        vz_ktag(118, DLBL - 1, &ME_P[ME_WAVE][f], v[f]);
}

static void viz_adsr(const int16_t *v, uint32_t f)
{
    /* the times (0..127 on TIME_MS_X10: 1 ms .. 10 s) on a square-root scale so short and long both read; each
     * stage bent by its curve (ACRV DCRV RCRV); VEL: the softest key's envelope, dotted; LOOP: attack and decay
     * coming round again, dotted; SPRD: the right channel, dotted, later; AMT, OFS place it */
    int32_t a = (int32_t)TIME_MS_X10[v[0] & 127], d = (int32_t)TIME_MS_X10[v[1] & 127], r = (int32_t)TIME_MS_X10[v[3] & 127];
    int32_t sa = 1, sd = 1, sr = 1, tot, xs[5], ys[5], k, x, base = DY1, top = DY0 + 2, w = DW - 4, py = 0;
    static const char *const L[4] = {"ATK", "DEC", "SUS", "REL"};
    static const uint8_t CRV[4] = {4, 5, 0xFF, 6};                     /* each stage's curve knob (SUS holds) */
    while (sa * sa < a) sa++;
    while (sd * sd < d) sd++;
    while (sr * sr < r) sr++;
    tot = sa + sd + sr + (sa + sd + sr) / 3;                            /* (a sustain plateau a third as long) */
    xs[0] = DX0 + 2;
    xs[1] = xs[0] + sa * w / tot;
    xs[2] = xs[1] + sd * w / tot;
    xs[3] = xs[2] + (sa + sd + sr) / 3 * w / tot;
    xs[4] = DX1 - 2;
    ys[0] = 0;                                                         /* levels 0..1000, placed below */
    ys[1] = 1000;
    ys[2] = ys[3] = v[2] * 10;
    ys[4] = 0;
    px_line(DX0, base + 1, DX1, base + 1, px_dim, 2);
    for (k = 0; k < 4; k++)                                            /* the envelope, stage by stage */
        for (x = xs[k]; x <= xs[k + 1]; x++) {
            int32_t span = xs[k + 1] - xs[k], t = span ? (x - xs[k]) * 1000 / span : 1000, lv, yy, c;
            c = CRV[k] == 0xFF ? 0 : v[CRV[k]];
            lv = ys[k] + (ys[k + 1] - ys[k]) * env_bend(t, ys[k + 1] > ys[k] ? -c : c) / 1000;
            lv = clamp(lv * v[10] / 100 + v[11] * 10, 0, 1000);
            yy = base - lv * (base - top) / 1000;
            if (v[8] && (x & 1) == 0)                                  /* VEL: the softest key */
                px_dot(x, base - lv * (100 - v[8]) / 100 * (base - top) / 1000, px_ink);
            if (v[7] && (x & 1) == 0 && x + v[7] * 8 / 100 <= DX1)      /* SPRD: the right channel, later */
                px_dot(x + v[7] * 8 / 100, yy, px_dim);
            if (x > xs[0])
                px_line(x - 1, py, x, yy, px_ink, 1);
            py = yy;
        }
    if (v[9]) {                                                        /* LOOP: attack and decay again */
        int32_t sy = base - clamp(ys[2] * v[10] / 100 + v[11] * 10, 0, 1000) * (base - top) / 1000;
        int32_t lx[3] = {xs[2], xs[2] + (xs[1] - xs[0]), xs[2] + (xs[2] - xs[0])}, ly[3] = {sy, top, sy};
        vz_poly(lx, ly, 3, px_ink, 2);
    }
    for (k = 1; k < 4; k++) {                                          /* nodes, and drop lines to the floor */
        int32_t ny = base - clamp(ys[k] * v[10] / 100 + v[11] * 10, 0, 1000) * (base - top) / 1000;
        px_line(xs[k], ny + 2, xs[k], base, px_dim, 2);
        vz_node(xs[k], ny, 0);
    }
    {                                                                  /* the stages, named under their stretch,
                                                                        * kept apart when stages are short */
        int32_t lx[4];
        for (k = 0; k < 4; k++)
            lx[k] = (xs[k] + xs[k + 1]) / 2 + 1;
        for (k = 1; k < 4; k++)
            if (lx[k] < lx[k - 1] + 22)
                lx[k] = lx[k - 1] + 22;
        for (k = 3; k >= 0; k--)
            if (lx[k] > DX1 - 10 - (3 - k) * 22)
                lx[k] = DX1 - 10 - (3 - k) * 22;
        for (k = 0; k < 4; k++)
            vz_label(lx[k], L[k], f == (uint32_t)k || f == (uint32_t)CRV[k]);
    }
    if (f >= 4u && f < NPK)
        vz_ktag(118, DY0 - 1, &ME_P[ME_ADSR][f], v[f]);
}

/* ------------------------------------------------------------- SYNTH --- */
/* The page shown decides the picture (each page is one part of the voice):
 *   OSC     three cycles of the two oscillators: the mix solid, the second one alone dotted, drifting ahead by DTUN
 *           (exaggerated, so a few cents show), NOIS as grit around the line
 *   FILTER  the low-pass's response over the audible range (log, CUTOFF_HZ's), its peak by RES; dotted, where the
 *           envelope at its top takes it (ENV); KTRK's arrow: where it sits an octave up the keys
 *   AMP     the envelope, drawn as the ADSR modulator's
 *   VOICE   the keys' note range (OCT and TUNE), the voices that play at once, the glide between two notes, drive */
static int32_t vz_osc(uint32_t w, int32_t p)          /* SYNTH's wave at phase p (64 a cycle), x1000 */
{
    return w == 4u ? ((p & 63) < 16 ? 1000 : -1000) : px_wave(w, p);
}

/* the low-pass's level in dB x10 at index i of the cutoff scale (about a semitone a step), cutoff at c, RES 0..100 */
static int32_t vz_lp_db10(int32_t i, int32_t c, int32_t res)
{
    int32_t d = i - c, pk = res * 18;                /* the peak: up to 18 dB */
    return pk * 16 / (16 + d * d) - (d > 0 ? d * 10 : 0);   /* 12 dB an octave above it (1 dB a step) */
}

/* the same for a filter of TYPE type: 0 low-pass, 1 band-pass (6 dB an octave each side), 2 high-pass */
static int32_t vz_filt_db10(int32_t i, int32_t c, int32_t res, int32_t type)
{
    int32_t d = i - c;
    if (type == 1)
        return res * 18 * 16 / (16 + d * d) - (d < 0 ? -d : d) * 5;
    if (type == 2)
        return res * 18 * 16 / (16 + d * d) - (d < 0 ? -d * 10 : 0);
    return vz_lp_db10(i, c, res);
}

/* FINE from x0 to x1: a cents meter (-50 .. +50, the node at FINE), its value over it unless in tune (st: TUNE's
 * semitones before it, unless 0x7F) */
static void vz_cents(int32_t x0, int32_t x1, int32_t ct, int32_t st)
{
    int32_t w = x1 - x0, mx = x0 + w / 2, y = 28, nx = mx + clamp(ct, -50, 50) * (w / 2) / 50;
    char b[16];
    px_line(x0, y, x0 + w, y, px_dim, 2);
    px_line(mx, y - 3, mx, y + 3, px_dim, 1);           /* in tune */
    px_line(x0, y - 2, x0, y + 2, px_dim, 1);
    px_line(x0 + w, y - 2, x0 + w, y + 2, px_dim, 1);
    vz_node(nx, y, 1);
    b[0] = 0;
    if (st != 0x7F) {
        b[0] = '+';
        fmt_int(st > 0 ? b + 1 : b, st);
        str_cpy(b + str_len(b), ct ? "st " : "st", 4);
    }
    if (ct) {
        char *e = b + str_len(b);
        e[0] = '+';
        fmt_int(ct > 0 ? e + 1 : e, ct);
        str_cpy(e + str_len(e), "ct", 3);
    }
    px_text_c(x0, x1 - x0, 19, PXF_3, b, px_ink);
}

/* a filter page: the response over the cutoff scale (CUT c, RES res, TYPE type), dotted where the envelope at its
 * top takes it (ENV env, -100..100, 96 steps at full), KTRK's arrow (ktrk %: where it sits an octave up the keys);
 * fi: the page's last-turned knob (0..3, 0xFF none), page: its four knobs (for the tag), knobs CUT RES TYPE|- ENV */
static void vz_filter(int32_t c, int32_t res, int32_t type, int32_t env, int32_t ktrk, uint32_t fi,
                      const pdesc_t *page, const int16_t *pv, uint32_t env_k)
{
    int32_t x, ce = clamp(c + env * 96 / 100, 0, 127), ck = clamp(c + ktrk * 12 / 100, 0, 127);
    int32_t py = 0, z = DY0 + 13;                     /* 0 dB at z; 18 dB of peak above it, 24 below */
    px_line(DX0, z, DX1, z, px_dim, 2);
    for (x = DX0; x <= DX1; x++) {
        int32_t i = (x - DX0) * 127 / DW;
        int32_t yy = clamp(z - vz_filt_db10(i, c, res, type) * 9 / 100, DY0, DY1);
        int32_t ye = clamp(z - vz_filt_db10(i, ce, res, type) * 9 / 100, DY0, DY1);
        if (x > DX0) {
            px_line(x - 1, py, x, yy, px_ink, 1);
            if (env && (x & 1) == 0)
                px_dot(x, ye, px_ink);
        }
        py = yy;
    }
    {
        int32_t cx = DX0 + c * DW / 127, ex = DX0 + ce * DW / 127, kx = DX0 + ck * DW / 127;
        vz_node(cx, clamp(z - vz_filt_db10(c, c, res, type) * 9 / 100, DY0 + 1, DY1 - 1), 1);
        px_line(cx, DY1 - 3, cx, DY1, px_ink, 1);
        if (env && ex != cx) {
            vz_node(ex, clamp(z - vz_filt_db10(ce, ce, res, type) * 9 / 100, DY0 + 1, DY1 - 1), 0);
            px_line(cx, DY1 - 1, ex, DY1 - 1, px_ink, 2);
        }
        if (ktrk && kx > cx + 2)                      /* KTRK: an octave up the keys */
            px_line(cx, DY1 + 1, kx, DY1 + 1, px_dim, 1);
        vz_label(cx, "CUT", fi == 0u);
        if (env && (ex - cx > 14 || cx - ex > 14))
            vz_label(ex, "ENV", fi == env_k);
    }
    if (fi < 4u && fi != 0u)
        vz_ktag(118, DY0 - 1, &page[fi], pv[fi]);
}

static void viz_synth(const int16_t *v, uint32_t f)
{
    uint32_t bp = ui.page >= 4u ? 4u : pdesc_at(SYN_P, 4u * ui.page) / 4u;   /* the stored block the page shows (page
                                                                              * 5, DRV . . LVL: 4) */
    int32_t x, k;
    if (bp == 0u) {
        uint32_t w = (uint32_t)clamp(v[SY_WAVE], 0, 4);
        int32_t py = 0, m2 = v[SY_MIX], dr = v[SY_DTUN];
        vz_seed = 77u;
        px_line(DX0, DMID, DX1, DMID, px_dim, 2);
        for (x = DX0; x <= DX1; x++) {
            int32_t p = (x - DX0) * 192 / DW, p2 = p + (x - DX0) * dr * 24 / (DW * 100);   /* 3 cycles; drift */
            int32_t a = vz_osc(w, p), b2 = vz_osc(w, p2), o = (a * (100 - m2) + b2 * m2) / 100, yy;
            yy = DMID - o * (DH / 2 - 2) / 1000;
            if (x > DX0)
                px_line(x - 1, py, x, yy, px_ink, 1);
            if (m2 && (x & 1) == 0)
                px_dot(x, DMID - b2 * (DH / 2 - 2) / 1000, px_dim);
            if (v[SY_NOIS] && (int32_t)(vz_rand() % 100u) < v[SY_NOIS] / 2)
                px_dot(x, yy + (int32_t)(vz_rand() % 7u) - 3, px_ink);
            py = yy;
        }
        vz_label(14, N_OSC[w], f == 0u);
        vz_label(45, "DTUN", f == 1u);
        vz_label(75, "MIX", f == 2u);
        vz_label(105, "NOIS", f == 3u);
    } else if (bp == 1u) {
        vz_filter(v[SY_CUT], v[SY_RES], 0, v[SY_ENV], v[SY_KTRK], f < NPK ? f % 4u : 0xFFu, SYN_P + 4, v + 4, 2u);
    } else if (bp == 2u) {
        int16_t a[NPK] = {0};
        a[0] = v[SY_ATK];
        a[1] = v[SY_DEC];
        a[2] = v[SY_SUS];
        a[3] = v[SY_REL];
        a[10] = 100;                                  /* (AMT 100, OFS 0: the envelope as it is) */
        viz_adsr(a, f >= SY_ATK && f <= SY_REL ? f - SY_ATK : 0xFFu);
    } else if (bp >= 4u && f == SY_DRV) {             /* DRV: the transfer curve, the plot's height */
        int32_t g = 1000 + v[SY_DRV] * 20, py = 0, w = 40, x0 = 40;
        px_line(x0, DY0, x0 + w, DY1, px_dim, 2);       /* (straight through: no drive) */
        px_frame(x0, DY0, w + 1, DH + 1, px_dim, 2);
        for (x = 0; x <= w; x++) {
            int32_t yy = DMID - px_clip((x - w / 2) * 2000 / w, g) * (DH / 2) / 1000;
            if (x)
                px_line(x0 + x - 1, py, x0 + x, yy, px_ink, 1);
            py = yy;
        }
        vz_label(60, "DRV", 1);
    } else if (bp >= 4u) {
        vz_level(v[SY_LVL], &SYN_P[SY_LVL], f == SY_LVL);
    } else {
        static const char NOTE[12][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        int32_t lo = 12 * ((int32_t)track[sys.sel].octave + 1) + v[SY_TUNE], hi = lo + 15, nv = v[SY_VOIC];
        char b[16];
        lo = clamp(lo, 0, 127);
        hi = clamp(hi, 0, 127);
        str_cpy(b, NOTE[lo % 12], 3);                 /* the white keys' range: "C3-D#4" */
        fmt_int(b + str_len(b), lo / 12 - 1);
        str_cpy(b + str_len(b), "-", 2);
        str_cpy(b + str_len(b), NOTE[hi % 12], 3);
        fmt_int(b + str_len(b), hi / 12 - 1);
        px_text_c(0, 120, DY0 + 1, PXF_5, b, px_ink);
        for (k = 0; k < (int32_t)SYN_NV; k++)          /* VOIC: the voices, as many solid as play */
            if (k < nv)
                px_box(6, 14 + k * 6, 18, 4, px_ink);
            else
                px_frame(6, 14 + k * 6, 18, 4, px_dim, 2);
        {   /* GLID: from one note to the next, sloped by the glide's time (square root, as the envelope's) */
            int32_t t = (int32_t)TIME_MS_X10[v[SY_GLID] & 127], sq = 1, w;
            while (sq * sq < t)
                sq++;
            w = v[SY_GLID] ? clamp(sq * 22 / 317, 1, 22) : 0;
            px_line(34, 31, 40, 31, px_ink, 1);
            px_line(40, 31, 40 + w, 15, px_ink, 1);
            px_line(40 + w, 15, 62, 15, px_ink, 1);
            px_line(34, 15, 62, 15, px_dim, 2);
        }
        vz_cents(66, 116, v[SY_FINE], v[SY_TUNE]);   /* TUNE and FINE */
        vz_label(15, "VOIC", f == SY_VOIC);
        vz_label(48, "GLID", f == SY_GLID);
        vz_label(78, "TUNE", f == SY_TUNE);
        vz_label(105, "FINE", f == SY_FINE);
    }
}

/* -------------------------------------------------------------- POLY --- */
/* SAMPLE: the sound (its blocks' peaks; named), lit from STRT, where every note starts, to END, where it stops; the
 * keys' range under it (OCT, TUNE) and how fast the top and bottom keys read it; the voices as boxes. ENV: the envelope, as SYNTH's.
 * FILTER: the response, its TYPE, dotted where ENV takes it */
static void viz_poly(const int16_t *v, uint32_t f)
{
    uint32_t bp = ui.page == 3u ? 3u : pdesc_at(POL_P, 4u * ui.page) / 4u;   /* the stored block the page shows (page
                                                                            * 4, VOIC ROOT FINE LVL: 3) */
    static const char NOTE[12][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int32_t x, k;
    if (bp == 0u && ui.zoom && (f == PL_STRT || f == PL_END)) {   /* HOME held + STRT or END: the zoom on that edge */
        tape_view_t vw;
        tape_view_of(sys.sel, (uint32_t)clamp(v[PL_REEL], 0, (int32_t)(NREEL + USLOT_N)), &vw);
        if (vw.len) {
            int32_t e = pct100_of(vw.len, f == PL_STRT ? v[PL_STRT] : v[PL_END]);
            vz_zoom(&vw, e, f == PL_STRT, f == PL_STRT ? "STRT" : "END");
            return;
        }
    }
    if (bp == 0u) {
        tape_view_t vw;
        int32_t sx = DX0 + v[PL_STRT] * DW / 10000, mid = 11, lo, hi, nv = v[PL_VOIC];
        int32_t ex = DX0 + v[PL_END] * DW / 10000;
        char b[24];
        tape_view_of(sys.sel, (uint32_t)clamp(v[PL_REEL], 0, (int32_t)(NREEL + USLOT_N)), &vw);
        for (x = DX0; x <= DX1; x++) {                                 /* the sound */
            uint32_t blk = vw.len ? (uint32_t)(x - DX0) * (vw.len / TAPE_BLK) / (uint32_t)(DW + 1) : 0;
            int32_t a = vw.len ? (int32_t)tv_peak(&vw, blk) * 9 / 255 : 0;
            px_box(x, mid - a, 1, 2 * a + 1, x >= sx && x <= ex ? px_ink : px_dim);
        }
        if (ex < DX1) {                                                /* END: where every note stops */
            px_box(ex, DY0, 1, 22, px_ink);
            for (k = 0; k < 3; k++)
                px_box(ex - 2 + k, DY0 + 21 - k, 5 - 2 * k, 1, px_ink);
        }
        if (!vw.len)
            px_text_c(0, 120, mid - 2, PXF_3, "EMPTY", px_ink);
        px_box(sx, DY0, 1, 22, px_ink);                                /* STRT: where every note starts */
        for (k = 0; k < 3; k++)
            px_box(sx - 2 + k, DY0 + k, 5 - 2 * k, 1, px_ink);
        lo = clamp(12 * ((int32_t)track[sys.sel].octave + 1) + v[PL_TUNE], 0, 127);   /* the keys' range */
        hi = clamp(lo + 15, 0, 127);
        str_cpy(b, NOTE[lo % 12], 3);
        fmt_int(b + str_len(b), lo / 12 - 1);
        str_cpy(b + str_len(b), "-", 2);
        str_cpy(b + str_len(b), NOTE[hi % 12], 3);
        fmt_int(b + str_len(b), hi / 12 - 1);
        px_text(DX0, 26, PXF_3, b, f == 2u ? px_ink : px_dim);
        for (k = 0; k < (int32_t)POL_NV; k++)                          /* the voices */
            if (k < nv)
                px_box(DX1 - 30 + k * 8, 26, 6, 5, px_ink);
            else
                px_frame(DX1 - 30 + k * 8, 26, 6, 5, px_dim, 2);
        vz_label(DX0 + 14, tape_name_of(sys.sel, (uint32_t)clamp(v[PL_REEL], 0, (int32_t)(NREEL + USLOT_N))), f == 0u);
        if (sx > DX0 + 34 || f == 1u)                                  /* (clear of the name, unless just turned) */
            vz_label(clamp(sx, DX0 + 14, DX1 - 30), "STRT", f == 1u);
        if (ex < DX1 - 10 || f == PL_END)
            vz_label(clamp(ex, DX0 + 30, DX1 - 8), "END", f == PL_END);
        if (f == 2u)
            vz_ktag(78, DLBL - 1, &POL_P[PL_TUNE], v[PL_TUNE]);
    } else if (bp == 1u) {
        int16_t a[NPK] = {0};
        a[0] = v[PL_ATK];
        a[1] = v[PL_DEC];
        a[2] = v[PL_SUS];
        a[3] = v[PL_REL];
        a[10] = 100;
        viz_adsr(a, f >= 4u && f < 8u ? f - 4u : 0xFFu);
    } else if (bp >= 3u && f == PL_LVL) {
        vz_level(v[PL_LVL], &POL_P[PL_LVL], 1);
    } else if (bp >= 3u) {                            /* VOIC; ROOT, the key that plays the sound as it is; FINE */
        int32_t r = clamp(v[PL_ROOT], 0, 127), nv = v[PL_VOIC];
        const char *u;
        char b[12];
        for (k = 0; k < (int32_t)POL_NV; k++)
            if (k < nv)
                px_box(6, 9 + k * 6, 18, 4, px_ink);
            else
                px_frame(6, 9 + k * 6, 18, 4, px_dim, 2);
        str_cpy(b, NOTE[r % 12], 3);
        fmt_int(b + str_len(b), r / 12 - 1);
        px_text_c(30, 32, 9, PXF_5, b, px_ink);
        px_text_c(30, 32, 19, PXF_3, "AS IS", px_dim);
        vz_cents(64, 88, v[PL_FINE], 0x7F);
        param_format(&POL_P[PL_LVL], v[PL_LVL], b, &u);
        str_cpy(b + str_len(b), u, 3);
        px_text_c(92, 26, 19, PXF_3, b, px_ink);
        vz_label(15, "VOIC", f == PL_VOIC);
        vz_label(46, "ROOT", f == PL_ROOT);
        vz_label(76, "FINE", f == PL_FINE);
        vz_label(105, "LVL", 0);
    } else {
        vz_filter(v[PL_CUT], v[PL_RES], clamp(v[PL_TYPE], 0, 2), v[PL_ENV], 0, f < NPK ? f % 4u : 0xFFu, POL_P + 8,
                  v + 8, 3u);
        px_text(DX0, DY0, PXF_3, N_SLOP[clamp(v[PL_TYPE], 0, 2)], px_ink);
    }
}

/* -------------------------------------------------------------- DRUM --- */
static const char *const DRM_LONG[DRM_NINST] = {"BASS DRUM", "SNARE", "CLAP", "RIM SHOT", "LOW CONGA", "LOW BONGO",
                                                "HIGH BONGO", "CLAVES", "COWBELL", "MARACAS", "TAMBOURINE", "GUIRO",
                                                "HI-HAT", "OPEN HAT", "METAL BEAT", "CYMBAL"};

/* the bar a DRUM page shows: STEP always the one OCT picked (the keys write there, playing or not: the playhead
 * shows only while it's in that bar); the other pages the one playing, else that one */
static uint32_t vz_drm_bar(const int16_t *v)
{
    uint32_t len = (uint32_t)clamp(v[DM_LEN], 2, (int32_t)DRM_NBAR), now = drm_now[sys.sel];
    return sys.playing && now != 0xFFu && ui.page != 3u ? (now >> 4) % len
                                                        : (uint32_t)clamp(v[DM_BAR] - 1, 0, (int32_t)len - 1);
}

/* PATTERN, VARY, STEP in LIVE or ERAS: the bar as a grid, a row per instrument the pattern uses (up to five, left
 * to right on the keys; spaced out when there are fewer), its code at the left, the 16 steps in groups of four. What this pass plays: a hit as written
 * solid; one this version leaves out, a dot; one it adds, hollow (a quiet one: small). The accented steps marked over
 * the grid, the playhead under it, the bars at the right (the shown one filled). */
static uint32_t vz_drm_grid(const int16_t *v, uint32_t bar)
{
    uint32_t len = (uint32_t)clamp(v[DM_LEN], 2, (int32_t)DRM_NBAR), used = 0, now = drm_now[sys.sel], b, s, i, row = 0;
    uint32_t ver = drm_version(v, sys.playing ? drm_pass[sys.sel] : 0u), more = 0, nrow = 0;
    int32_t pitch;
    drm_hits_t h[16];
    for (b = 0; b < len; b++)
        for (s = 0; s < 16u; s++)
            used |= tp[sys.sel].dpat[b][s];
    for (s = 0; s < 16u; s++)
        drm_step(sys.sel, v, ver, bar, s, &h[s]);
    for (s = 0; s < 16u; s++)                                         /* (a version can bring in its crash) */
        used |= h[s].hit;
    for (i = 0; i < DRM_NINST; i++)
        nrow += (used >> i) & 1u;
    pitch = nrow <= 3u ? 9 : nrow == 4u ? 7 : 6;
    for (s = 0; s < 16u; s++) {                                       /* the accents, over the steps */
        int32_t x = 12 + (int32_t)s * 6 + (int32_t)(s / 4u);
        if (h[s].acc)
            px_box(x, DY0, 4, 2, px_ink);
        if (sys.playing && now != 0xFFu && now == bar * 16u + s)      /* the playhead, under them (orange) */
            px_box(x - 1, DY1 + 1, 6, 2, px_live);
    }
    for (i = 0; i < DRM_NINST; i++) {
        int32_t y = DY0 + 4 + (int32_t)row * pitch;
        if (!((used >> i) & 1u))
            continue;
        if (row == 5u) {
            more++;
            continue;
        }
        px_text(DX0, y, PXF_3, DRM_KIT[i].code, px_ink);
        for (s = 0; s < 16u; s++) {
            int32_t x = 12 + (int32_t)s * 6 + (int32_t)(s / 4u);
            uint32_t w = (tp[sys.sel].dpat[bar][s] >> i) & 1u, p = (h[s].hit >> i) & 1u;
            if (p && (h[s].soft >> i) & 1u)
                px_frame(x + 1, y + 1, 2, 2, px_ink, 1);
            else if (p && w)
                px_box(x, y, 4, 4, px_ink);
            else if (p)
                px_frame(x, y, 4, 4, px_ink, 1);
            else if (w)
                px_dot(x + 1, y + 1, px_ink);
            else
                px_dot(x + 1, y + 2, px_dim);
        }
        row++;
    }
    if (!used)
        px_text_c(12, 100, DY0 + 14, PXF_3, "EMPTY: WRITE IT ON STEP", px_dim);
    for (b = 0; b < len; b++)                                         /* the bars */
        if (b == bar)
            px_box(113, DY0 + 4 + (int32_t)b * 8, 4, 6, px_ink);
        else
            px_frame(113, DY0 + 4 + (int32_t)b * 8, 4, 6, px_dim, 1);
    return more;                                                      /* (instruments past five: how many) */
}

/* KIT, or an instrument held: its sound, struck as the pattern would strike it (rendered off line through its
 * voice: what you see is what plays), the peaks over up to 0.74 s, its name */
static void vz_drm_sound(const int16_t *v, uint32_t i)
{
    static dk_coef_t c;
    static dk_voice_t vo;
    uint8_t pk[128];
    const int16_t *in = tp[sys.sel].dins[i];
    dk_knobs_t k;
    int32_t y[CTL], mid = 15, x;
    uint32_t nch = 0, j, m;
    k.tune = (int16_t)(clamp(v[DM_TUNE] + in[DIN_TUNE], -24, 24) * 16);
    k.dscale = (uint16_t)drm_scale(v[DM_DECY] + in[DIN_DECY]);
    k.bright = (int16_t)clamp((v[DM_TONE] + in[DIN_TONE]) * 127 / 100, -127, 127);
    k.level = (uint16_t)(db_q10(clamp(in[DIN_LVL], -24, 6)) * 100 / 1024);
    k.accent = 0;
    k.drive = (uint8_t)(clamp(v[DM_DRV], 0, 100) * 127 / 100);
    dk_setup(&c, &DRM_KIT[i], &k);
    memset(&vo, 0, sizeof vo);
    dk_trigger(&vo);
    while (nch < 128u && (vo.live || vo.trig)) {                      /* 256 samples a chunk */
        int32_t a = 0;
        for (j = 0; j < 8u; j++) {
            for (m = 0; m < CTL; m++)                                 /* (dk_run adds into its block) */
                y[m] = 0;
            dk_run(&c, &vo, y, CTL);
            for (m = 0; m < CTL; m++)
                a = y[m] > a ? y[m] : -y[m] > a ? -y[m] : a;
        }
        pk[nch++] = (uint8_t)clamp(a * 16 / 16384, 0, 16);
    }
    px_line(DX0, mid, DX1, mid, px_dim, 2);
    for (x = DX0; x <= DX1; x++) {
        uint32_t ch = (uint32_t)(x - DX0) * 128u / (uint32_t)(DW + 1);
        int32_t a = ch < nch ? pk[ch] : 0;
        if (a)
            px_box(x, mid - a, 1, 2 * a + 1, px_ink);
    }
    {
        char b[24];
        str_cpy(b, DRM_KIT[i].code, 3);
        str_cpy(b + 2, " ", 2);
        str_cpy(b + 3, DRM_LONG[i], 16);
        px_text(DX0, DLBL, PXF_3, b, px_ink);
        px_text(DX1 - px_text_w(PXF_3, "0.74S") + 1, DY0, PXF_3, "0.74S", px_dim);
    }
}

/* DRUM's pages: PATTERN and VARY the bar's grid, KIT the instrument's sound (STEP's, or the one held), STEP the
 * instrument's row of the bar large (LIVE, ERAS: the grid) */
static void viz_drum(const int16_t *v, uint32_t f)
{
    uint32_t bar = vz_drm_bar(v), i = (uint32_t)clamp(v[DM_INST], 0, 15), s, more = 0;
    char b[24];
    if (ui.drm_inst) {                                                /* an instrument held: its sound, its knobs */
        vz_drm_sound(v, (ui.drm_inst - 1u) & 15u);
        if (ui.last < NDIN)
            vz_ktag(118, DLBL - 1, &DRI_P[ui.last], tp[sys.sel].dins[(ui.drm_inst - 1u) & 15u][ui.last]);
        return;
    }
    if (ui.page == 2u) {
        vz_drm_sound(v, i);
        if (f >= 8u && f < 12u)
            vz_ktag(118, DLBL - 1, &DRM_P[f], v[f]);
        return;
    }
    if (ui.page == 3u && v[DM_MODE] != DMODE_LIVE && v[DM_MODE] != DMODE_ERAS) {   /* STEP: the instrument's row */
        uint32_t row = tp[sys.sel].dpat[bar][0], acc = tp[sys.sel].dacc[bar], now = drm_now[sys.sel];
        for (s = 0; s < 16u; s++) {
            int32_t x = DX0 + 1 + (int32_t)s * 7 + (int32_t)(s / 4u) * 2;
            row = tp[sys.sel].dpat[bar][s];
            if ((acc >> s) & 1u)
                px_box(x, DY0 + 2, 5, 2, v[DM_MODE] == DMODE_ACC ? px_ink : px_dim);
            if ((row >> i) & 1u)
                px_box(x, DY0 + 6, 5, 20, v[DM_MODE] == DMODE_ACC ? px_dim : px_ink);
            else
                px_frame(x, DY0 + 6, 5, 20, px_dim, 2);
            if (sys.playing && now == bar * 16u + s)
                px_box(x - 1, DY0 + 28, 7, 2, px_live);           /* (the step playing: orange) */
            if (!(s & 3u)) {
                char n[4];
                fmt_int(n, (int32_t)s + 1);
                px_text(x, DY1 - 1, PXF_3, n, px_dim);
            }
        }
    } else {
        more = vz_drm_grid(v, bar);
    }
    str_cpy(b, ui.page == 1u ? "SEED " : N_DPATN[clamp(v[DM_PATN], 0, (int32_t)DRM_NPRESET - 1)], 8);
    if (ui.page == 1u) {
        if (v[DM_SEED] > 0)
            fmt_int(b + 5, (int32_t)drm_version(v, sys.playing ? drm_pass[sys.sel] : 0u));
        else
            str_cpy(b + 5, "OFF", 4);
    }
    if (ui.page == 3u)
        str_cpy(b, DRM_KIT[i].code, 3);
    str_cpy(b + str_len(b), "  BAR ", 7);
    fmt_int(b + str_len(b), (int32_t)bar + 1);
    if (more) {                                                       /* (rows the grid had no room for) */
        str_cpy(b + str_len(b), "  +", 4);
        fmt_int(b + str_len(b), (int32_t)more);
    }
    px_text(DX0, DLBL, PXF_3, b, px_ink);
    if (f < NPK && !pdesc_empty(&DRM_P[f]))
        vz_ktag(118, DLBL - 1, &DRM_P[f], v[f]);
    else if (ui.page == 3u)
        px_text(118 - px_text_w(PXF_3, N_DMODE[clamp(v[DM_MODE], 0, 3)]), DLBL, PXF_3, N_DMODE[clamp(v[DM_MODE], 0, 3)],
                px_ink);
}

/* FOLLOW: the envelope of a sound: what the source track plays, from its tape's peaks, dim; GAIN
 * scales what goes in, RISE and FALL are how fast the envelope (solid) climbs and drops, HOLD samples it on the
 * tempo's divisions (steps), AMT and OFS place it, SPRD the right channel dotted */
static void viz_follow(const int16_t *v, uint32_t f)
{
    static const uint8_t HOLD_DOTS[5] = {0, 4, 7, 14, 28};            /* a division's width on this 2-bar panel */
    int32_t x, e = 0, held = 0, py = DY1, kr = clamp(3000 / (3 + v[2]), 15, 1000), kf = clamp(3000 / (3 + v[3]), 15, 1000);
    int32_t gain = db_x1000(v[1]), hd = HOLD_DOTS[clamp(v[4], 0, 4)], src = clamp(v[0], 0, 5);
    uint32_t t = src >= 1 && src <= 4 ? (uint32_t)(src - 1) : sys.sel;
    char b[12];
    px_line(DX0, DY1 + 1, DX1, DY1 + 1, px_dim, 2);
    for (x = DX0; x <= DX1; x++) {
        int32_t in = clamp(tape_peak_at(t, (x - DX0) * 1000 / DW) * gain / 1000, 0, 1000), out, yy;
        if ((x & 1) == 0)
            px_line(x, DY1, x, DY1 - in * (DH - 2) / 1000, px_dim, 1);   /* what it listens to */
        e += (in - e) * (in > e ? kr : kf) / 1000;
        if (!hd || (x - DX0) % hd == 0)
            held = e;
        out = clamp(held * v[5] / 100 + v[6] * 10, 0, 1000);
        yy = DY1 - out * (DH - 2) / 1000;
        if (v[7] && (x & 1) == 0 && x + v[7] * 8 / 100 <= DX1)
            px_dot(x + v[7] * 8 / 100, yy, px_ink);
        if (x > DX0)
            px_line(x - 1, py, x, yy, px_ink, 1);
        py = yy;
    }
    str_cpy(b, "SRC ", sizeof b);
    str_cpy(b + 4, N_SRC[src], 6);
    if (f == 0u)
        px_tag(1, DLBL - 1, PXF_3, b, px_ink, px_bg);
    else
        px_text(2, DLBL, PXF_3, b, px_ink);
    if (f >= 1u && f < NPK)
        vz_ktag(118, DLBL - 1, &ME_P[ME_FOLLOW][f], v[f]);
    else
        px_text(118 - px_text_w(PXF_3, tape_name(t)), DLBL, PXF_3, tape_name(t), px_dim);
}

static void viz_seq(const int16_t *v, uint32_t f)
{
    /* the 16 steps in four groups of four, each a bar as tall as its value (tp[].steps, 0..100) with a node on
     * top; the steps past LEN are their dotted slots only. SWING nudges the off-beats late; SLEW draws the glide
     * from each step's value into the next. The step numbers sit under each group; the last-turned knob is named
     * at the right of that row. The steps whose white keys are held (KNOB 1 sets them) are framed. */
    const int8_t *st = tp[sys.sel].steps[ui.slot];
    int32_t k, bw = 5, gap = 1, gg = 3, top = DY0 + 1, floor = DY1, h = floor - top, sw = v[3] * 2 / 100;
    int32_t px = 0, py = 0;
    px_line(DX0, floor + 1, DX1, floor + 1, px_dim, 2);
    for (k = 0; k < 16; k++) {
        int32_t x = DX0 + 3 + k * (bw + gap) + (k / 4) * gg + ((k & 1) ? sw : 0), on = k < v[0];
        int32_t y = floor - st[k] * h / 100;
        uint16_t c = sys.playing && mod_seq_now(sys.sel, ui.slot) == k ? px_live : px_ink;   /* (the step playing:
                                                                        * orange; its white key lights too) */
        if (on) {
            px_frame(x, top, bw, h + 1, px_dim, 2);                    /* the step's range, dotted */
            if ((k * 37 + 11) % 100 < v[6])                            /* its value (PROB: the steps that won't */
                px_box(x + 1, y, bw - 2, floor - y + 1, c);            /* play this time are hollow) */
            else
                px_frame(x + 1, y, bw - 2, floor - y + 1, c, 1);
            px_box(x, y, bw, 1, c);                                    /* a cap as wide as the step */
            if (k + 1 == v[7]) {                                       /* STRT: a triangle over the first step */
                px_box(x, top - 2, bw, 1, px_ink);
                px_box(x + 1, top - 1, bw - 2, 1, px_ink);
            }
            if ((ui.steps_held >> k) & 1u)                             /* a white key held: KNOB 1 sets this one */
                px_frame(x - 1, top - 1, bw + 2, h + 3, px_ink, 1);
            if (v[2] && k)                                             /* SLEW: the glide from the last value */
                px_line(px, py, x + v[2] * (bw - 1) / 100, y, px_ink, 1);
            px = x + bw - 1;
            py = y;
        } else {
            px_line(x, floor, x + bw - 1, floor, px_dim, 1);
        }
        if ((k & 3) == 0) {
            char n[4];
            fmt_int(n, k + 1);
            px_text(x, DLBL, PXF_3, n, k < v[0] ? px_ink : px_dim);
        }
    }
    if (f < 4u) {
        static const char *const L[4] = {"LEN", "RATE", "SLEW", "SWNG"};
        px_tag(118 - px_text_w(PXF_3, L[f]) - 1, DLBL - 1, PXF_3, L[f], px_ink, px_bg);
    } else if (f < 8u) {
        vz_ktag(118, DLBL - 1, &ME_P[ME_SEQ][f], v[f]);
    } else if (v[4]) {                                                 /* not forward: the order, named */
        px_text(118 - px_text_w(PXF_3, ME_P[ME_SEQ][4].names[v[4]]), DLBL, PXF_3, ME_P[ME_SEQ][4].names[v[4]], px_ink);
    }
}

/* ------------------------------------------------------------- ARP --- */
/* the arpeggiator (arp.c): the notes it walks, one bar per step of the walk, as tall as its pitch among them, the step
 * sounding solid and the rest outlined; up to 32 steps (RND: the notes it picks from, low to high, the one sounding
 * solid). Under it the order and the octaves, or the last-turned knob. No notes: what to do. */
static void viz_arp(const int16_t *v, uint32_t f)
{
    int16_t ra[NARP] = {0};
    int res = ui.page == 2u;                                           /* RIPL ARP: RIPPLE's, its knobs (12-15) as
                                                                        * an AR_* array, as reso_arp_block builds */
    const arp_rt_t *r = res ? &reso_arp[sys.sel] : &arp_rt[sys.sel];
    uint8_t s[ARP_MAXN];
    uint32_t n, ord, oct, len, per, show, i, now;
    int32_t lo = 127, hi = 0, top = DY0 + 1, floor = DY1, h = floor - top;
    char b[24];
    int notes = res || tp[sys.sel].src == SRC_SYNTH || tp[sys.sel].src == SRC_POLY;   /* (TAPE, DRUM: slices, drums) */
    if (res) {
        for (i = 0; i < 4u; i++)
            ra[i] = tp[sys.sel].dev[DEV_RESO][RP_AMODE + i];
        v = ra;
    }
    n = arp_notes(v, r, s);
    ord = (uint32_t)clamp(v[AR_ORDR], 0, 4);
    oct = (uint32_t)clamp(v[AR_RNGE], 1, 3);
    if (!notes)
        oct = 1u;                                                      /* (no octaves to spread them over) */
    len = n * oct;
    per = n ? (ord == ARO_RND ? len : arp_period(ord, len)) : 0u;
    show = per < 32u ? per : 32u;
    now = r->pos ? (r->pos - 1u) % (per ? per : 1u) : 0u;
    if (v[AR_MODE] == ARM_OFF) {
        px_text_c(0, 120, 12, PXF_3, "OFF: TURN MODE TO", px_dim);
        px_text_c(0, 120, 20, PXF_3, "KEY, LTCH OR ADD", px_dim);
    } else if (!n) {
        px_text_c(0, 120, 16, PXF_3, notes ? "HOLD SOME KEYS" : tp[sys.sel].src == SRC_TAPE ? "HOLD SOME SLICES"
                  : "HOLD SOME DRUMS", px_dim);
    } else {
        for (i = 0; i < len; i++) {                                    /* the walk's range */
            int32_t m = (int32_t)arp_at(s, n, i);
            lo = m < lo ? m : lo;
            hi = m > hi ? m : hi;
        }
        px_line(DX0, floor + 1, DX1, floor + 1, px_dim, 2);
        for (i = 0; i < show; i++) {
            uint32_t k = ord == ARO_RND ? i : arp_place(ord, len, i, 0);
            int32_t bw = (int32_t)(DW / show) - 1, x = DX0 + 1 + (int32_t)i * (bw + 1), m = (int32_t)arp_at(s, n, ord == ARO_RND ? k : k);
            int32_t y = floor - (hi > lo ? (m - lo) * (h - 3) / (hi - lo) + 3 : h / 2);
            int playing = ord == ARO_RND ? (uint32_t)m == r->cur && r->on : i == now && r->pos;
            if (bw < 1)
                bw = 1;
            if (playing)                                               /* (the note playing: filled, orange) */
                px_box(x, y, bw, floor - y + 1, px_live);
            else
                px_frame(x, y, bw, floor - y + 1, px_ink, bw > 2 ? 1 : 2);
        }
    }
    if (res && f >= ARP_RES && f < ARP_RES + 4u) {
        vz_ktag(118, DLBL - 1, &ARP_P[f], v[f - ARP_RES]);
    } else if (!res && f < NARP && !pdesc_empty(&ARP_P[f])) {
        vz_ktag(118, DLBL - 1, &ARP_P[f], v[f]);
    } else {
        str_cpy(b, N_ARO[ord], sizeof b);
        str_cpy(b + str_len(b), !notes ? "" : oct > 1u ? (oct > 2u ? " 3 OCT" : " 2 OCT") : " 1 OCT", 8);
        px_text(118 - px_text_w(PXF_3, b), DLBL, PXF_3, b, px_ink);
    }
    if (n) {
        fmt_int(b, (int32_t)n);
        str_cpy(b + str_len(b), !notes ? (tp[sys.sel].src == SRC_TAPE ? (n == 1u ? " SLICE" : " SLICES")
                : (n == 1u ? " DRUM" : " DRUMS")) : n == 1u ? " NOTE" : " NOTES", 8);
        px_text(DX0, DLBL, PXF_3, b, px_ink);
    }
}

/* SETUP (the mixer's last page): what each preference does, in words */
static void viz_setup(void)
{
    static const char *const KN[4] = {"KNOBS: A STEP A DETENT", "KNOBS: 2 STEPS A DETENT", "KNOBS: 4 STEPS A DETENT",
                                      "KNOBS: FAST TURNS GO FURTHER"};
    px_text_c(0, 120, 3, PXF_3, sys.keys_notes ? "BLACK KEYS PLAY NOTES: F TO G" : "BLACK KEYS: THE SHORTCUTS", px_ink);
    px_text_c(0, 120, 11, PXF_3, KN[(bryo_prefs >> 1) & 3u], px_ink);
    {                                                   /* the load by stage (chain.c ld_pm, in %), three to a line
                                                         * (four ran off the edge), and the loop speeds from RAM and
                                                         * flash (main.c ld_probe) */
        static const char *const L[8] = {"SRC ", "  GRN ", "  RES ", "COL ", "  SPC ", "  PRE ", "ROOM ", "  MST "};
        static const uint8_t S[8] = {LD_SRC, LD_GRN, LD_RES, LD_COL, LD_SPC, LD_PRE, LD_ROOM, LD_MST};
        char b[48], v[12];
        uint32_t k;
        for (k = 0; k < 8u; k++) {
            if (k % 3u == 0u)
                b[0] = 0;
            str_cpy(b + str_len(b), L[k], (unsigned)(sizeof b - str_len(b)));
            fmt_int(v, (int32_t)((ld_pm[S[k]] + 5u) / 10u));
            str_cpy(b + str_len(b), v, (unsigned)(sizeof b - str_len(b)));
            if (k == 7u) {
                str_cpy(b + str_len(b), "  R/F ", (unsigned)(sizeof b - str_len(b)));
                fmt_int(v, (int32_t)ld_ram);
                str_cpy(b + str_len(b), v, (unsigned)(sizeof b - str_len(b)));
                str_cpy(b + str_len(b), "/", (unsigned)(sizeof b - str_len(b)));
                fmt_int(v, (int32_t)ld_xip);
                str_cpy(b + str_len(b), v, (unsigned)(sizeof b - str_len(b)));
            }
            if (k % 3u == 2u || k == 7u)
                px_text_c(0, 120, 19 + 8 * (int32_t)(k / 3u), PXF_3, b, px_ink);
        }
    }
}

/* KEY, the mixer's page: an octave, C to B, the scale's notes solid and the rest hollow (a key there plays the
 * nearest solid one: param.c key_snap), the key's note marked under it; beside it, what the scale is called and how
 * many notes it keeps. SCAL OFF: every key solid. */
static void viz_key(void)
{
    static const int8_t WHITE[7] = {0, 2, 4, 5, 7, 9, 11}, BLACK[5] = {1, 3, 6, 8, 10}, BAFTER[5] = {0, 1, 3, 4, 5};
    uint32_t sc = (uint32_t)clamp(keyp[KY_SCAL], 0, NSCALE), key = (uint32_t)clamp(keyp[KY_KEY], 0, 11), n, c;
    uint32_t m = key_mask((int32_t)sc);
    int32_t x0 = DX0 + 2, y0 = DY0 + 1, k, rx = 0;
    char b[16];
    for (k = 0; k < 7; k++) {                           /* the white keys: solid in the scale, dotted out of it */
        int32_t x = x0 + k * 8;
        n = (uint32_t)WHITE[k];
        if ((m >> ((n + 12u - key) % 12u)) & 1u)
            px_box(x, y0, 7, 30, px_ink);
        else
            px_frame(x, y0, 7, 30, px_dim, 2);
        if (n == key)
            rx = x + 3;
    }
    for (k = 0; k < 5; k++) {                           /* the black ones over the gaps, a gap round each */
        int32_t x = x0 + (BAFTER[k] + 1) * 8 - 3;
        n = (uint32_t)BLACK[k];
        px_box(x - 1, y0, 7, 20, px_bg);
        if ((m >> ((n + 12u - key) % 12u)) & 1u)
            px_box(x, y0, 5, 19, px_ink);
        else
            px_frame(x, y0, 5, 19, px_dim, 2);
        if (n == key)
            rx = x + 2;
    }
    px_box(rx - 1, y0 + 31, 3, 2, px_ink);              /* the key's note */
    for (n = 0, c = 0; n < 12u; n++)
        c += (m >> n) & 1u;
    str_cpy(b, N_KEYN[key], 4);
    str_cpy(b + str_len(b), " ", 2);
    str_cpy(b + str_len(b), N_KSCAL[sc], 6);
    px_text_c(64, 54, DY0 + 1, PXF_5, b, px_ink);
    px_text_c(64, 54, 13, PXF_3, sc ? N_SCALE_FULL[sc - 1] : "EVERY NOTE", px_ink);
    if (sc) {
        fmt_int(b, (int32_t)c);
        str_cpy(b + str_len(b), " OF 12 NOTES", 14);
        px_text_c(64, 54, 20, PXF_3, b, px_dim);
        px_text_c(64, 54, 27, PXF_3, "ELSE: NEAREST", px_dim);
    } else {
        px_text_c(64, 54, 20, PXF_3, "SCAL: OFF", px_dim);
    }
    vz_label(x0 + 28, "KEY", ui.last == 0u);
    vz_label(91, "SCAL", ui.last == 1u);
}

/* ------------------------------------------------------------- MIXER --- */
/* the selected track's channel strip (the mixer, both pages: the levels above, this below): its EQ and filter as
 * one response over 8 octaves (ch_resp, ui_px.c: what mixer.c applies), a node on each shelf's corner and
 * on the filter's, and the pan as two speakers whose bars show each side's gain */
static void viz_channel(uint32_t f)
{
    const int16_t *ch = tp[sys.sel].ch;
    int32_t w = 88, x, py = 0, mid = 15, k, pl, pr;
    int on = ui.chan;                                  /* the labels invert only where the knobs set them */
    px_line(DX0, mid, DX0 + w, mid, px_dim, 2);       /* 0 dB */
    px_line(DX0, mid - 10, DX0 + w, mid - 10, px_dim, 4);   /* +12 */
    px_line(DX0, mid + 10, DX0 + w, mid + 10, px_dim, 4);   /* -12 */
    for (x = 0; x <= w; x++) {
        int32_t y = clamp(mid - ch_resp(ch, x, w + 1) * 10 / 120, DY0, DY1);
        if (x)
            px_line(DX0 + x - 1, py, DX0 + x, y, px_ink, 1);
        py = y;
    }
    for (k = 0; k < 2; k++) {                          /* the shelves' corners */
        int32_t cx = DX0 + w * (k ? 70 : 30) / 100;
        vz_node(cx, clamp(mid - ch_resp(ch, cx - DX0, w + 1) * 10 / 120, DY0, DY1), 0);
    }
    if (ch[CH_FILT]) {                                 /* the filter's corner, solid */
        int32_t c = ch[CH_FILT] < 0 ? w - (-ch[CH_FILT]) * w * 85 / 10000 : ch[CH_FILT] * w * 85 / 10000;
        vz_node(DX0 + c, clamp(mid - ch_resp(ch, c, w + 1) * 10 / 120, DY0, DY1), 1);
    }
    /* the pan: L and R, each side's gain as a bar (mixer.c: equal power, sqrt 2 times cos and sin of the place, at
     * most full, so the centre has both at full) */
    pl = clamp(px_cos((ch[CH_PAN] + 100) * 8 / 100) * 22 * 1414 / 1000000, 0, 22);
    pr = clamp(px_sin((ch[CH_PAN] + 100) * 8 / 100) * 22 * 1414 / 1000000, 0, 22);
    px_text(98, DY0, PXF_3, "L", px_ink);
    px_text(113, DY0, PXF_3, "R", px_ink);
    px_box(98, DY1 - pl, 3, pl, px_ink);
    px_box(113, DY1 - pr, 3, pr, px_ink);
    px_line(97, DY1 + 1, 117, DY1 + 1, px_dim, 2);
    px_box(106 + ch[CH_PAN] * 6 / 100, DY1 - 3, 3, 3, px_ink);   /* where it sits */
    vz_label(DX0 + w * 30 / 100 - 4, "LOW", on && f == 0u);
    vz_label(DX0 + w * 70 / 100 + 4, "HIGH", on && f == 1u);
    vz_label(DX0 + w / 2, "FILT", on && f == 2u);
    vz_label(107, "PAN", on && f == 3u);
    {   /* whose channel: the selected track, tagged (the fader with the border above) */
        char n[3] = {'T', (char)('1' + sys.sel), 0};
        px_tag(1, DLBL - 1, PXF_3, n, px_ink, px_bg);
    }
}

/* the master compressor (the mixer's MASTER page): in against out from -36 dB to full scale as one line (what
 * mixer.c mx_curve does to a steady sound; the dim diagonal is unity), a node where the threshold sits, and what
 * it takes off now as a bar falling from the top on the right, with its dB */
static void viz_master(uint32_t f)
{
    int32_t w = 88, x, py = 0, gr = comp.gr_q8, h;
    char b[12];
    px_line(DX0, DY1, DX0 + w, DY1 - w * 36 / 42 * DH / w, px_dim, 2);   /* unity: out = in */
    for (x = 0; x <= w; x++) {
        int32_t lv = 15 * 256 - 1531 + x * 1531 / w, g, d, out, y;
        g = mx_curve(lv, &d);
        out = lv + mx_log2((uint32_t)g) - 12 * 256;    /* (g is Q12: log2 minus 12 octaves) */
        y = clamp(DY1 - (out - (15 * 256 - 1531)) * DH / 1787, DY0, DY1);   /* (42 dB of rows: 1787) */
        if (x)
            px_line(DX0 + x - 1, py, DX0 + x, y, px_ink, 1);
        py = y;
    }
    if (mst[MS_AMT]) {                                  /* the threshold */
        int32_t tx = DX0 + w - mst[MS_AMT] * 1276 / 100 * w / 1531, d;
        int32_t g = mx_curve(15 * 256 - mst[MS_AMT] * 1276 / 100, &d), out;
        out = 15 * 256 - mst[MS_AMT] * 1276 / 100 + mx_log2((uint32_t)g) - 12 * 256;
        vz_node(tx, clamp(DY1 - (out - (15 * 256 - 1531)) * DH / 1787, DY0, DY1), f == 0u);
    }
    h = clamp(gr * DH / 512, 0, DH);                     /* (12 dB, 2 octaves, the whole height) */
    px_frame(103, DY0, 7, DH + 1, px_dim, 2);
    if (h)
        px_box(104, DY0 + 1, 5, h, px_ink);
    str_cpy(b, "-", sizeof b);                          /* "-3.2": dB off now */
    fmt_int(b + 1, gr * 602 / 25600);
    str_cpy(b + str_len(b), ".", 2);
    fmt_int(b + str_len(b), gr * 602 / 2560 % 10);
    px_text_c(97, 20, DLBL, PXF_3, gr ? b : "0", px_ink);
    vz_label(DX0 + w / 2, "IN : OUT", 0);
    if (f < 4u)
        vz_ktag(92, DY0 - 1, &MS_P[f], mst[f]);
}

/* tenths of a second as "4.2" (the unit goes apart: an S after digits reads as a 5 in these faces) */
static void vz_secs(char *b, int32_t tenths)
{
    fmt_int(b, tenths / 10);
    str_cpy(b + str_len(b), ".", 2);
    fmt_int(b + str_len(b), tenths % 10);
}

/* chunks as tenths of a second of tape-format sound */
static int32_t vz_chunk_tenths(uint32_t n) { return (int32_t)(n * TAPE_CHS * 10u / TAPE_SR); }

/* The mixer's levels page: what each track holds of the shared memory, under its fader: its tape's seconds and,
 * when GRAIN is on, its live buffer's (GR); a parked track says OFF, its tape (kept, taken first) dim. Then the
 * memory as one ribbon (each track's tape solid, its GRAIN buffer, strings and delay dim, then the shared rooms dim
 * under an R where it fits, what's free dotted), and under it TRACKS and the time still free. SELECT sets TRACKS
 * here. */
static void viz_memory(void)
{
    char b[20];
    uint32_t t, x0 = DX0, c;
    for (t = 0; t < NTRK; t++) {
        int32_t cx = 30 * (int32_t)t, off = t >= sys.ntrk;
        uint32_t nt = tape_ctl[t].nch, ng = mem_count(MEM_GRAIN + t);
        uint16_t col = off ? px_dim : px_ink;
        if (off)
            px_text_c(cx, 30, 3, PXF_5, "OFF", px_dim);
        if (nt) {
            vz_secs(b, vz_chunk_tenths(nt));
            px_text_c(cx, 30, off ? 13 : 3, off ? PXF_3 : PXF_5, b, col);
            if (!off)
                px_text_c(cx, 30, 12, PXF_3, "SEC", px_dim);
        } else if (!off) {
            px_text_c(cx, 30, 3, PXF_5, "-", px_ink);
        }
        if (ng && !off) {
            str_cpy(b, "GR ", sizeof b);
            vz_secs(b + 3, vz_chunk_tenths(ng));
            px_text_c(cx, 30, 18, PXF_3, b, px_ink);
        }
    }
    px_line(DX0, 27, DX1, 27, px_dim, 2);              /* the ribbon: free memory dotted under the rest */
    for (t = 0; t < NTRK; t++) {
        uint32_t nt = mem_count(MEM_TAPE + t), ng = mem_count(MEM_GRAIN + t) + mem_count(MEM_RESO + t) +
                 mem_count(MEM_SPACE + t), w0 = x0;
        uint32_t wt = nt * (uint32_t)DW / MEM_NC, wg = ng * (uint32_t)DW / MEM_NC;
        if (nt && !wt)
            wt = 1;
        if (ng && !wg)
            wg = 1;
        if (wt)
            px_box((int32_t)x0, 25, (int32_t)wt, 5, t >= sys.ntrk ? px_dim : px_ink);
        x0 += wt;
        if (wg)
            px_box((int32_t)x0, 25, (int32_t)wg, 5, px_dim);
        x0 += wg;
        if (x0 - w0 >= 6u) {                             /* its number under it, where it fits */
            b[0] = (char)('1' + t);
            b[1] = 0;
            px_text_c((int32_t)w0, (int32_t)(x0 - w0), 31, PXF_3, b, px_ink);
        }
        if (x0 > w0)
            x0++;                                       /* (a gap between tracks) */
    }
    c = mem_count(MEM_ROOM) + mem_count(MEM_ROOM + 1u);   /* the rooms A and B (no track's: the mix's) */
    if (c) {
        uint32_t w = c * (uint32_t)DW / MEM_NC + 1u;
        px_box((int32_t)x0, 25, (int32_t)w, 5, px_dim);
        if (w >= 6u)
            px_text_c((int32_t)x0, (int32_t)w, 31, PXF_3, "R", px_ink);
        x0 += w + 1u;
    }
    c = mem_count(MEM_IMPORT);
    if (c)
        px_box((int32_t)x0, 25, (int32_t)(c * (uint32_t)DW / MEM_NC + 1u), 5, px_dim);
    str_cpy(b, "TRACKS ", sizeof b);
    fmt_int(b + 7, sys.ntrk);
    px_tag(1, DLBL - 1, PXF_3, b, px_ink, px_bg);
    str_cpy(b, "FREE ", sizeof b);                     /* the time free, at the ribbon's free end */
    vz_secs(b + 5, vz_chunk_tenths(mem_count(MEM_FREE)));
    str_cpy(b + str_len(b), " SEC", 5);
    px_box(118 - px_text_w(PXF_3, b) - 2, 30, px_text_w(PXF_3, b) + 3, 7, px_bg);
    px_text(118 - px_text_w(PXF_3, b), 31, PXF_3, b, px_ink);
    str_cpy(b, "CPU ", sizeof b);                      /* the audio ISR's load (audio.c), in 5 % steps, lit from 85 %
                                                        * (where shedding starts) */
    fmt_int(b + 4, (int32_t)(sys.cpu_q8 * 100u / 256u / 5u * 5u));
    str_cpy(b + str_len(b), "%", 2);
    px_text(118 - px_text_w(PXF_3, b), DLBL, PXF_3, b, sys.cpu_q8 * 100u / 256u >= 85u ? px_rec : px_dim);   /* (near
                                                                        * its limit: red, as recording's warning) */
}

/* The routing view (ALGORITHM): what each track's REC records. The tracks are boxes under their knobs (filled while
 * REC is armed, dim when parked); a line runs from the track heard down into a lane of the recording track's own and
 * up into it, an arrow at its end; solid while that REC is armed, dotted while it isn't. The others' mix (AUTO on a
 * TAPE track, OTHR) and AUTO on a SYNTH or POLY track (its own source) are named in the lane instead. SELF: a loop
 * under the box. Under it all, the last-turned (or the
 * focused) track's routing in words. */
static void viz_route(uint32_t f)
{
    static const char *const SRC_SHORT[NSRC] = {"TAPE", "SYNTH", "POLY"};
    uint32_t k, j, w = f < 4u ? f : sys.sel;
    char b[28];
    for (k = 0; k < NTRK; k++) {                         /* the tracks */
        int32_t cx = 30 * (int32_t)k + 15, off = k >= sys.ntrk, armed = !off && ((sys.rec >> k) & 1u);
        char n[3] = {'T', (char)('1' + k), 0};
        if (armed) {
            px_box(cx - 8, 1, 17, 9, px_ink);
            px_text_c(cx - 8, 17, 3, PXF_3, n, px_bg);
        } else {
            px_frame(cx - 8, 1, 17, 9, off ? px_dim : px_ink, 1);
            px_text_c(cx - 8, 17, 3, PXF_3, n, off ? px_dim : px_ink);
        }
    }
    for (k = 0; k < NTRK && k < sys.ntrk; k++) {         /* each recording track's lane: what it hears */
        uint32_t from = tp[k].recin, st = (sys.rec >> k) & 1u ? 1u : 2u;
        int32_t y = 19 + 4 * (int32_t)k, ck = 30 * (int32_t)k + 15;   /* (words under the box, lanes below) */
        uint16_t c = st == 1u ? px_ink : px_dim;
        if (from == RIN_AUTO || from == RIN_OTHR)       /* (in words, below) */
            continue;
        if (from == RIN_T1 + k) {                        /* SELF: round under its box and back in */
            px_line(ck - 4, 11, ck - 4, y, c, st);
            px_line(ck - 4, y, ck + 4, y, c, st);
            px_line(ck + 4, y, ck + 4, 11, c, st);
            px_dot(ck + 3, 12, c);
            px_dot(ck + 5, 12, c);
            continue;
        }
        for (j = 0; j < NTRK; j++) {
            int32_t xs, xa;
            if (j == k || j >= sys.ntrk || (from >= RIN_T1 && from - RIN_T1 != j))
                continue;
            xs = 30 * (int32_t)j + 15 + 2 * (int32_t)k - 3;   /* (each lane leaves a box at its own column) */
            xa = ck + 2 * (int32_t)j - 3;
            px_line(xs, 11, xs, y, c, st);
            px_line(xs, y, xa, y, c, st);
            px_line(xa, y, xa, 11, c, st);
            px_dot(xa - 1, 12, c);                       /* the arrow's head, into the box */
            px_dot(xa + 1, 12, c);
        }
    }
    for (k = 0; k < NTRK && k < sys.ntrk; k++) {         /* the others' mix (AUTO on a TAPE track, OTHR) and AUTO's
                                                         * own source in words: lines from every box would crowd out
                                                         * the routes you've set; over any line passing under */
        uint32_t from = tp[k].recin;
        const char *wd = from == RIN_AUTO && tp[k].src != SRC_TAPE ? SRC_SHORT[tp[k].src % NSRC]
                       : from == RIN_AUTO || from == RIN_OTHR ? "OTHERS" : 0;
        int32_t cx = 30 * (int32_t)k;
        if (!wd)
            continue;
        px_box(cx + 15 - px_text_w(PXF_3, wd) / 2 - 1, 11, px_text_w(PXF_3, wd) + 2, 7, px_bg);
        px_text_c(cx, 30, 12, PXF_3, wd, (sys.rec >> k) & 1u ? px_ink : px_dim);
    }
    b[0] = 'T';                                          /* the routing in words */
    b[1] = (char)('1' + w);
    b[2] = 0;
    if (w >= sys.ntrk)
        str_cpy(b + 2, " IS OFF (TRACKS)", sizeof b - 2);
    else if (tp[w].recin == RIN_AUTO && tp[w].src != SRC_TAPE)
        str_cpy(b + 2, tp[w].src == SRC_SYNTH ? " RECORDS ITS SYNTH" : " RECORDS ITS POLY", sizeof b - 2);
    else if (tp[w].recin == RIN_AUTO || tp[w].recin == RIN_OTHR)
        str_cpy(b + 2, " RECORDS THE OTHERS", sizeof b - 2);
    else if (tp[w].recin == RIN_T1 + w)
        str_cpy(b + 2, " RECORDS ITSELF", sizeof b - 2);
    else {
        str_cpy(b + 2, " RECORDS T", sizeof b - 2);
        b[str_len(b) + 1] = 0;
        b[str_len(b)] = (char)('1' + tp[w].recin - RIN_T1);
    }
    if (f < 4u)
        px_tag(1, DLBL - 1, PXF_3, b, px_ink, px_bg);
    else
        px_text(2, DLBL, PXF_3, b, px_ink);
}

/* --------------------------------------------------- USB record mode --- */
/* The strip: the four tracks a take can go to, each its number in a box, and what its tape holds now (seconds, "-"
 * empty, OFF parked). Picking one (CHOOSE) fills its box and says REPLACE under it, where SEC was. */
/* seconds of n blocks, two decimals ("12.34") */
static void ur_secs2(char *b, uint32_t n)
{
    uint32_t cs = n * TAPE_BLK * 100u / TAPE_SR;
    fmt_int(b, (int32_t)(cs / 100u));
    str_cpy(b + str_len(b), ".", 2);
    b[str_len(b) + 1] = 0;
    b[str_len(b)] = (char)('0' + cs / 10u % 10u);
    b[str_len(b) + 1] = 0;
    b[str_len(b)] = (char)('0' + cs % 10u);
}

/* CHOOSE's knobs: START and LENGTH (seconds), GAIN (dB, NORM), FADE (ms), each with its pictogram */
static void viz_usbrec_knobs(void)
{
    static const char *const NAME[4] = {"STRT", "LEN", "GAIN", "FADE"};
    static const uint8_t PK[4] = {PK_START, PK_LENGTH, PK_FADER, PK_FADE};
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        pdesc_t d = {NAME[k], 0, 1, 0, F_NUM, 0};
        int32_t x = 30 * (int32_t)k, v = ur.kv[k];
        char b[12];
        if (k <= UK_LEN) {
            d.max = (int16_t)(ur.nblk ? ur.nblk : 1u);
            ur_secs2(b, (uint32_t)v);
        } else if (k == UK_GAIN) {
            d.min = -24;
            d.max = UR_NORM;
            if (v == UR_NORM) {
                str_cpy(b, "NORM", sizeof b);
            } else {
                str_cpy(b, v > 0 ? "+" : "", sizeof b);
                fmt_int(b + str_len(b), v);
                str_cpy(b + str_len(b), "DB", 3);
            }
        } else {
            d.max = 100;
            fmt_int(b, v * UR_FADE_MS);
            str_cpy(b + str_len(b), "MS", 3);
        }
        px_picto(PK[k], x + 4, 2, &d, v, px_ink);
        if (ui.last == k)
            px_tag(x + (30 - px_text_w(PXF_5, NAME[k])) / 2 - 1, 26, PXF_5, NAME[k], px_ink, px_bg);
        else
            px_text_c(x, 30, 27, PXF_5, NAME[k], px_ink);
        px_text_c(x, 30, 37, PXF_5, b, px_ink);
    }
}

static void viz_usbrec_strip(void)
{
    uint32_t k;
    char b[12];
    if (ur.state == UR_CHOOSE) {
        viz_usbrec_knobs();
        return;
    }
    for (k = 0; k < NTRK; k++) {
        int32_t x = 30 * (int32_t)k, off = k >= sys.ntrk, pick = ur.state == UR_CHOOSE && ur.dest == (int8_t)k;
        char n[2] = {(char)('1' + k), 0};
        uint16_t c = off ? px_dim : px_ink;
        if (pick) {
            px_box(x + 7, 2, 16, 20, px_ink);
            px_text_big(x + 10, 5, 2, n, px_bg);
        } else {
            px_frame(x + 7, 2, 16, 20, c, off ? 2 : 1);
            px_text_big(x + 10, 5, 2, n, c);
        }
        if (off) {
            px_text_c(x, 30, 27, PXF_5, "OFF", px_dim);
            continue;
        }
        if (tape_ctl[k].nblk && !tape_ctl[k].empty) {
            vz_secs(b, (int32_t)(tape_ctl[k].nblk * TAPE_BLK * 10u / TAPE_SR));
            px_text_c(x, 30, 27, PXF_5, b, px_ink);
            if (!pick)
                px_text_c(x, 30, 36, PXF_3, "SEC", px_dim);
        } else {
            px_text_c(x, 30, 27, PXF_5, "-", px_ink);
        }
        if (pick)
            px_text_c(x, 30, 36, PXF_3, "REPLACE", px_ink);
    }
}

/* the memory as one ribbon at row y: the other tracks' tapes dim (not the one the take replaces), the take in ink
 * (what would be kept), what's left a dotted line */
static void ur_ribbon(int32_t y, uint32_t take)
{
    uint32_t t, x = DX0;
    for (t = 0; t < NTRK; t++) {
        uint32_t n = (ur.state == UR_CHOOSE && ur.dest == (int8_t)t) ? 0u : tape_ctl[t].nch, w = n * (uint32_t)DW / MEM_NC;
        if (n && !w)
            w = 1;
        if (w) {
            px_box((int32_t)x, y, (int32_t)w, 3, px_dim);
            x += w + 1u;
        }
    }
    if (take) {
        uint32_t w = take * (uint32_t)DW / MEM_NC + 1u;
        px_box((int32_t)x, y - 1, (int32_t)w, 5, px_ink);
        x += w + 1u;
    }
    if ((int32_t)x < DX1)
        px_line((int32_t)x, y + 1, DX1, y + 1, px_dim, 2);
}

/* CHOOSE: the take's outline (the kept part in ink, the rest dim, the cut points and where the loop plays), the
 * ribbon, what's kept and what's left, and where it goes */
static void viz_usbrec_choose(void)
{
    uint32_t n = ur.nblk ? ur.nblk : 1u, s = (uint32_t)ur.kv[UK_STRT], e = s + (uint32_t)ur.kv[UK_LEN], c, py = 0;
    int32_t x;
    char b[32], t[12];
    for (x = 0; x <= DW; x++) {                                    /* the outline: one line, no fill */
        uint32_t b0 = (uint32_t)x * n / (DW + 1u), b1 = ((uint32_t)x + 1u) * n / (DW + 1u), pk = 0, y;
        if (b1 <= b0)
            b1 = b0 + 1u;
        for (c = b0; c < b1 && c < ur.nblk; c++) {
            uint32_t p = ur_chunk(c)->peak[c % MEM_CB];
            pk = p > pk ? p : pk;
        }
        y = 16u - (pk > 255u ? 255u : pk) * 15u / 255u;
        if (x)
            px_line(DX0 + x - 1, (int32_t)py, DX0 + x, (int32_t)y, b0 >= s && b0 < e ? px_ink : px_dim, b0 >= s && b0 < e ? 1 : 2);
        py = y;
    }
    px_line(DX0 + (int32_t)(s * (DW + 1u) / n), 1, DX0 + (int32_t)(s * (DW + 1u) / n), 17, px_ink, 1);
    px_line(DX0 + (int32_t)(e * (DW + 1u) / n) - 1, 1, DX0 + (int32_t)(e * (DW + 1u) / n) - 1, 17, px_ink, 1);
    x = DX0 + (int32_t)((ur.pp >> 12) / TAPE_BLK * (DW + 1u) / n);   /* where the loop plays */
    px_box(x, 18, 1, 2, px_ink);
    ur_ribbon(23, ur_keep_chunks());
    ur_secs2(t, (uint32_t)ur.kv[UK_LEN]);
    str_cpy(b, "KEEP ", sizeof b);
    str_cpy(b + str_len(b), t, 8);
    px_text(DX0, 28, PXF_5, b, px_ink);
    vz_secs(b, vz_chunk_tenths(ur_free_after()));                 /* what the other tracks would still have */
    str_cpy(b + str_len(b), " SEC LEFT", 10);
    px_text(DX1 - px_text_w(PXF_3, b) + 1, 30, PXF_3, b, px_dim);
    if (ur.dest < 0) {
        str_cpy(b, "WHICH TRACK? WHITE KEYS 1-", sizeof b);
        fmt_int(b + str_len(b), sys.ntrk);
    } else {
        str_cpy(b, "ONTO TRACK ", sizeof b);
        fmt_int(b + str_len(b), ur.dest + 1);
        if (tape_ctl[(uint32_t)ur.dest].nblk && !tape_ctl[(uint32_t)ur.dest].empty) {
            str_cpy(b + str_len(b), ", REPLACING ", 13);
            vz_secs(t, (int32_t)(tape_ctl[(uint32_t)ur.dest].nblk * TAPE_BLK * 10u / TAPE_SR));
            str_cpy(b + str_len(b), t, 8);
        }
    }
    px_text(DX0, DLBL, PXF_3, b, ur.dest < 0 ? px_dim : px_ink);
}

/* The panel: what the step is, the time large, and one level bar (both sides' peak), plain. READY: the room a take
 * has, or a note to play something on the computer; RECORDING: the take's seconds of the room, and the memory as a
 * ribbon (the take growing against the other tracks); CHOOSE: viz_usbrec_choose. */
static void viz_usbrec(void)
{
    char b[28], t[12];
    int32_t pk = ur.peak, w;
    uint32_t room = ur_room() * TAPE_CHS * 10u / TAPE_SR;          /* tenths */
    ur.peak = pk - (pk >> 2);                                      /* (the bar falls between frames) */
    if (ur.state == UR_CHOOSE) {
        viz_usbrec_choose();
        return;
    }
    if (ur.state == UR_READY) {
        px_text(DX0, 2, PXF_5B, "READY", px_ink);
        vz_secs(t, (int32_t)room);
        str_cpy(b, "ROOM FOR ", sizeof b);
        str_cpy(b + str_len(b), t, 8);
        str_cpy(b + str_len(b), " SEC", 5);
        px_text(DX0, 14, PXF_5, b, px_ink);
    } else {                                                       /* RECORDING */
        vz_secs(t, (int32_t)usbrec_tenths());
        px_text_big(DX0, 1, 3, t, px_ink);
        w = DX0 + 18 * (int32_t)str_len(t);
        vz_secs(b + 3, (int32_t)room);
        b[0] = 'O';
        b[1] = 'F';
        b[2] = ' ';
        px_text(w, 9, PXF_3, "SEC", px_ink);
        px_text(w, 16, PXF_3, b, px_dim);
        ur_ribbon(24, ur.nch);
    }
    if (!usbrec_live()) {                                          /* nothing arriving */
        px_text(DX0, 28, PXF_3, "PLAY SOUND ON THE COMPUTER", px_dim);
        px_text(DX0, 35, PXF_3, "WITH BRYO AS ITS OUTPUT", px_dim);
        return;
    }
    w = pk * (DW + 1) / 32767;                                     /* the level: one bar, a tick at full scale */
    px_frame(DX0, 30, DW + 1, 5, px_dim, 2);
    if (w > 0)
        px_box(DX0, 31, w > DW ? DW : w, 3, px_ink);
    px_text(DX0, DLBL, PXF_3, "LEVEL", px_dim);
}

/* a message: an inverted box over the panel, its words wrapped at 18 characters */
static void viz_message(void)
{
    char line[20];
    const char *s = ui.msg;
    int32_t y = 8, n = 0;
    px_box(2, 4, 116, 32, px_ink);
    while (*s && y < 34) {
        const char *e = s, *brk = 0;
        int32_t len;
        while (*e && e - s <= 18) {
            if (*e == ' ')
                brk = e;
            e++;
        }
        len = *e && brk ? (int32_t)(brk - s) : (int32_t)(e - s);
        str_cpy(line, s, (uint32_t)len + 1u);
        px_text_c(2, 116, y, PXF_5, line, px_bg);
        s += len;
        while (*s == ' ')
            s++;
        y += 10;
        n++;
    }
    (void)n;
}

/* the panel: its signature (what it shows), then the picture */
/* ---------------------------------------------------------- PROJECTS --- */
/* the project view's strip: the slot PRESETS points at, large; whether it holds a project, its tempo, each track's
 * source; LOADED when it's the one the instrument holds (CHANGED once something's been changed since); a second
 * press waiting, named */
static void viz_tpr_strip(void);
static void viz_reel_strip(void);
static void viz_project_strip(void)
{
    const pj_sum_t *m = &pj_sum[ui.pj_sel % PJ_N];
    if (ui.pj_page == 2u) {
        viz_reel_strip();
        return;
    }
    if (ui.pj_page) {
        viz_tpr_strip();
        return;
    }
    char n[4] = {'P', (char)('1' + ui.pj_sel % PJ_N), 0, 0}, b[16];
    uint32_t t;
    if (ui.pj_sel >= PJ_N) {                                           /* FACTORY: the scene Bryo starts with; TEST */
        int test = ui.pj_sel == PJ_TEST;
        px_text_big(2, 3, 3, test ? "T" : "F", px_ink);
        px_text(30, 3, PXF_5B, test ? "TEST" : "FACTORY", px_ink);
        px_text(30, 13, PXF_5, test ? "DRUMS + GRAINS" : "EVOLVING SCENE", px_ink);
        px_tag(30, 22, PXF_3, ui.pj_ask == PJ_ASK_LOAD ? "OCT+ AGAIN: LOAD" : pj_cur == ui.pj_sel ? "LOADED" : "OCT+ LOADS IT",
               px_ink, px_bg);
        for (t = 0; t < NTRK; t++) {
            int32_t x = 30 * (int32_t)t;
            char tn[3] = {'T', (char)('1' + t), 0};
            px_line(x + 2, 31, x + 27, 31, px_dim, 2);
            px_text_c(x, 30, 34, PXF_3, tn, px_dim);
            px_text_c(x, 30, 41, PXF_5, SRC_NAME[TPS[t].src % NSRC], px_ink);
        }
        return;
    }
    px_text_big(2, 3, 3, n, px_ink);
    px_text(44, 3, PXF_5B, m->used ? "SAVED" : "EMPTY", m->used ? px_ink : px_dim);
    if (m->used) {
        fmt_int(b, m->bpm);
        str_cpy(b + str_len(b), " BPM", 5);
        px_text(44, 13, PXF_5, b, px_ink);
    }
    if (ui.pj_sel == pj_cur)
        px_tag(44, 22, PXF_3, pj_changed() ? "LOADED, CHANGED" : "LOADED", px_ink, px_bg);
    if (ui.pj_ask)
        px_tag(44, 22, PXF_3, ui.pj_ask == PJ_ASK_LOAD ? "OCT+ AGAIN: LOAD" : "SAVE AGAIN: OVER IT", px_ink, px_bg);
    for (t = 0; t < NTRK; t++) {                                       /* each track's source */
        int32_t x = 30 * (int32_t)t;
        char tn[3] = {'T', (char)('1' + t), 0};
        px_line(x + 2, 31, x + 27, 31, px_dim, 2);
        px_text_c(x, 30, 34, PXF_3, tn, px_dim);
        px_text_c(x, 30, 41, PXF_5, m->used ? SRC_NAME[m->src[t] % NSRC] : "-", m->used ? px_ink : px_dim);
    }
}

/* the project view's picture: the six slots, three by two, each with its number and its tracks' sources (T S P D)
 * or EMPTY; the one PRESETS points at inverted, the loaded one marked with a block in its corner */
static void viz_tpr(void);
static void viz_reels(void);
static void viz_project(void)
{
    uint32_t s, t;
    if (ui.pj_page == 2u) {
        viz_reels();
        return;
    }
    if (ui.pj_page) {
        viz_tpr();
        return;
    }
    if (ui.pj_sel == PJ_TEST) {                                        /* TEST, past FACTORY: what it's for */
        px_text_c(0, 120, 3, PXF_3, "FACTORY, T1'S DRUMS IN GRAIN:", px_ink);
        px_text_c(0, 120, 11, PXF_3, "READ THE LOAD ON SETUP", px_ink);
        px_text_c(0, 120, 19, PXF_3, "(GLO, EDIT TO SETUP)", px_ink);
        px_text_c(0, 120, 31, PXF_3, "YOUR PROJECTS STAY", px_dim);
        return;
    }
    if (ui.pj_sel >= PJ_N) {                                           /* FACTORY, past P6: what it is */
        px_text_c(0, 120, 3, PXF_3, "THE START-UP SCENE:", px_ink);
        px_text_c(0, 120, 11, PXF_3, "DRUMS, GRAINS, STRINGS", px_ink);
        px_text_c(0, 120, 19, PXF_3, "THE MODULATORS CHANGE", px_ink);
        px_text_c(0, 120, 31, PXF_3, "YOUR PROJECTS STAY", px_dim);
        return;
    }
    for (s = 0; s < PJ_N; s++) {
        const pj_sum_t *m = &pj_sum[s];
        int32_t x = 1 + (int32_t)(s % 3u) * 40, y = 1 + (int32_t)(s / 3u) * 21, sel = s == ui.pj_sel;
        uint16_t ink = sel ? px_bg : px_ink;
        char n[3] = {'P', (char)('1' + s), 0}, src[5] = {0};
        if (sel)
            px_box(x, y, 38, 19, px_ink);
        else
            px_frame(x, y, 38, 19, m->used ? px_ink : px_dim, m->used ? 1 : 2);
        px_text(x + 3, y + 3, PXF_5B, n, ink);
        if (s == pj_cur)
            px_box(x + 32, y + 2, 4, 4, ink);
        for (t = 0; t < NTRK; t++)
            src[t] = SRC_NAME[m->src[t] % NSRC][0];
        px_text(x + 3, y + 12, PXF_3, m->used ? src : "EMPTY", sel ? px_bg : m->used ? px_ink : px_dim);
    }
}

/* ------------------------------------------------------------ REELS --- */
/* a reel of yours in seconds (tenths) */
static int32_t vz_reel_tenths(uint32_t r) { return (int32_t)(uslot_hdr(r)->nblk * TAPE_BLK * 10u / TAPE_SR); }

/* the reels' strip (the project view's third page): the slot PRESETS points at, large (U1..U6); the reel there, its
 * length and the slots it spans (a slot a longer reel runs into names that reel); which tracks play it now, by
 * TAPE's REEL or POLY's; what POLY held does */
static void viz_reel_strip(void)
{
    char n[3] = {'U', (char)('1' + ui.reel_sel % USLOT_N), 0}, b[20];
    int32_t r = uslot_owner(ui.reel_sel);
    uint32_t t;
    px_text_big(2, 3, 3, n, px_ink);
    if (r < 0) {
        px_text(44, 3, PXF_5B, "EMPTY", px_dim);
        px_text(44, 13, PXF_3, "SAVE A PROJECT WITH A", px_dim);
        px_text(44, 20, PXF_3, "TAKE, OR COPY A WAV IN", px_dim);
    } else {
        uint32_t span = uslot_hdr((uint32_t)r)->span ? uslot_hdr((uint32_t)r)->span : 1u;
        px_text(44, 3, PXF_5B, uslot_name[r], px_ink);
        vz_secs(b, vz_reel_tenths((uint32_t)r));
        str_cpy(b + str_len(b), " SEC", 5);
        if (span > 1u) {                                               /* "U2-U4" */
            str_cpy(b + str_len(b), " U", 3);
            fmt_int(b + str_len(b), r + 1);
            str_cpy(b + str_len(b), "-U", 3);
            fmt_int(b + str_len(b), r + (int32_t)span);
        }
        px_text(44, 13, PXF_5, b, px_ink);
        px_tag(44, 22, PXF_3, "POLY HELD: DELETE", px_ink, px_bg);
    }
    for (t = 0; t < NTRK; t++) {                                       /* who plays it */
        int32_t x = 30 * (int32_t)t;
        uint32_t u = r < 0 ? 0u : NREEL + 1u + (uint32_t)r;
        char tn[3] = {'T', (char)('1' + t), 0};
        const char *w = u && tp[t].src == SRC_TAPE && tape_src(t) == u ? "TAPE"
                      : u && tp[t].src == SRC_POLY && (uint32_t)tp[t].pol[PL_REEL] == u ? "POLY" : "-";
        px_line(x + 2, 31, x + 27, 31, px_dim, 2);
        px_text_c(x, 30, 34, PXF_3, tn, px_dim);
        px_text_c(x, 30, 41, PXF_5, w, w[0] == '-' ? px_dim : px_ink);
    }
}

/* the reels' picture: the six slots, three by two, each its name and length (or EMPTY; a slot a longer reel runs
 * into: dotted, with that reel's name); the one PRESETS points at inverted */
static void viz_reels(void)
{
    uint32_t s;
    char b[12];
    for (s = 0; s < USLOT_N; s++) {
        int32_t x = 1 + (int32_t)(s % 3u) * 40, y = 1 + (int32_t)(s / 3u) * 21, sel = s == ui.reel_sel;
        int32_t r = uslot_owner(s), head = r == (int32_t)s;
        uint16_t ink = sel ? px_bg : r < 0 ? px_dim : px_ink;
        if (sel)
            px_box(x, y, 38, 19, px_ink);
        else
            px_frame(x, y, 38, 19, r < 0 ? px_dim : px_ink, head ? 1 : 2);
        px_text(x + 3, y + 3, PXF_5B, r < 0 ? "-" : uslot_name[r], ink);
        if (head) {
            vz_secs(b, vz_reel_tenths(s));
            str_cpy(b + str_len(b), " S", 3);
        } else
            str_cpy(b, r < 0 ? "EMPTY" : "(CONT.)", sizeof b);
        px_text(x + 3, y + 12, PXF_3, b, ink);
    }
}

/* the track presets' strip: the one PRESETS points at, large (A..H); whether it holds one, its source, the track it
 * would load into (or be saved from); each slot's engine and the depths (or "-" while it's empty) */
static void viz_tpf_strip(void);
static void viz_tpr_strip(void)
{
    const tpr_sum_t *q = &tpr_sum[ui.tpr_sel % TPR_N];
    char n[2] = {(char)('A' + ui.tpr_sel % TPR_N), 0}, b[20];
    uint32_t s;
    if (ui.tpr_sel >= TPR_N) {
        viz_tpf_strip();
        return;
    }
    px_text_big(2, 3, 3, n, px_ink);
    px_text(30, 3, PXF_5B, q->used ? SRC_NAME[q->src % NSRC] : "EMPTY", q->used ? px_ink : px_dim);
    if (q->used) {
        fmt_int(b, q->ndep);
        str_cpy(b + str_len(b), q->ndep == 1u ? " DEPTH" : " DEPTHS", 8);
        px_text(30, 13, PXF_5, b, px_ink);
    }
    str_cpy(b, "T", sizeof b);                                         /* the track it goes to, or comes from */
    b[1] = (char)('1' + sys.sel);
    b[2] = 0;
    px_tag(30, 22, PXF_3, ui.pj_ask ? (ui.pj_ask == PJ_ASK_LOAD ? "OCT+ AGAIN: LOAD" : "SAVE AGAIN: OVER IT")
                                    : q->used ? "OCT+: INTO THE TRACK" : "SAVE: THE TRACK HERE", px_ink, px_bg);
    px_text(116 - px_text_w(PXF_5B, b), 3, PXF_5B, b, px_ink);
    for (s = 0; s < NSLOT; s++) {                                      /* each slot's engine, M1..M4 */
        int32_t x = 30 * (int32_t)s;
        char sn[3] = {'M', (char)('1' + s), 0};
        px_line(x + 2, 31, x + 27, 31, px_dim, 2);
        px_text_c(x, 30, 34, PXF_3, sn, px_dim);
        px_text_c(x, 30, 41, PXF_5, q->used ? ME_NAME[q->eng[SLOT_OF_NUM[s]] % NME] : "-", q->used ? px_ink : px_dim);
    }
}

/* the track presets' picture: the eight, four by two, each its letter and its source (or EMPTY); the one PRESETS
 * points at inverted */
/* a factory preset on the strip: its number large, its name, its source and depths, each slot's engine */
static void viz_tpf_strip(void)
{
    const tpf_t *F = &TPF[(ui.tpr_sel - TPR_N) % TPF_N];
    char n[2] = {(char)('1' + (ui.tpr_sel - TPR_N) % TPF_N), 0}, b[24];
    uint32_t s;
    px_text_big(2, 3, 3, n, px_ink);
    px_text(30, 3, PXF_5B, F->name, px_ink);
    str_cpy(b, SRC_NAME[F->src % NSRC], sizeof b);
    if (F->ndep) {                                                     /* (none: just the source, it's full width) */
        str_cpy(b + str_len(b), ", ", 3);
        fmt_int(b + str_len(b), F->ndep);
        str_cpy(b + str_len(b), F->ndep == 1u ? " DEPTH" : " DEPTHS", 8);
    }
    px_text(30, 13, PXF_5, b, px_ink);
    px_tag(30, 22, PXF_3, "FACTORY: OCT+ LOADS", px_ink, px_bg);
    b[0] = 'T';
    b[1] = (char)('1' + sys.sel);
    b[2] = 0;
    px_text(116 - px_text_w(PXF_5B, b), 3, PXF_5B, b, px_ink);
    for (s = 0; s < NSLOT; s++) {                                      /* (M1..M4) */
        int32_t x = 30 * (int32_t)s;
        char sn[3] = {'M', (char)('1' + s), 0};
        px_line(x + 2, 31, x + 27, 31, px_dim, 2);
        px_text_c(x, 30, 34, PXF_3, sn, px_dim);
        px_text_c(x, 30, 41, PXF_5, ME_NAME[F->eng[SLOT_OF_NUM[s]] % NME], px_ink);
    }
}

/* the factory presets' picture: the eight, four by two, each its number and name; the one pointed at inverted */
static void viz_tpf(void)
{
    uint32_t f;
    for (f = 0; f < TPF_N; f++) {
        int32_t x = 1 + (int32_t)(f % 4u) * 30, y = 1 + (int32_t)(f / 4u) * 21, sel = f + TPR_N == ui.tpr_sel;
        uint16_t ink = sel ? px_bg : px_ink;
        char n[2] = {(char)('1' + f), 0};
        if (sel)
            px_box(x, y, 28, 19, px_ink);
        else
            px_frame(x, y, 28, 19, px_ink, 1);
        px_text(x + 3, y + 3, PXF_5B, n, ink);
        px_text(x + 3, y + 12, PXF_3, TPF[f].name, ink);
    }
}

static void viz_tpr(void)
{
    uint32_t p;
    if (ui.tpr_sel >= TPR_N) {                                         /* (past H: the factory ones) */
        viz_tpf();
        return;
    }
    for (p = 0; p < TPR_N; p++) {
        const tpr_sum_t *q = &tpr_sum[p];
        int32_t x = 1 + (int32_t)(p % 4u) * 30, y = 1 + (int32_t)(p / 4u) * 21, sel = p == ui.tpr_sel;
        uint16_t ink = sel ? px_bg : px_ink;
        char n[2] = {(char)('A' + p), 0};
        if (sel)
            px_box(x, y, 28, 19, px_ink);
        else
            px_frame(x, y, 28, 19, q->used ? px_ink : px_dim, q->used ? 1 : 2);
        px_text(x + 3, y + 3, PXF_5B, n, ink);
        px_text(x + 3, y + 12, PXF_3, q->used ? SRC_NAME[q->src % NSRC] : "EMPTY", sel ? px_bg : q->used ? px_ink : px_dim);
    }
}

static uint32_t viz_sig(void)
{
    uint32_t h = 2166136261u + ui.view * 7u + ui.kind * 31u + ui.dev * 131u + ui.slot * 1009u + ui.glo_held * 3u +
                 (ui.last < 4u ? ui.last + 1u : 0u) * 7919u + sys.sel * 104729u + ux.theme * 3u, k, t;
    if (ui.msg_t)
        return hash_str(h ^ 0x5A5Au, ui.msg);
    if (ui.view == VIEW_USBREC) {                                      /* the step, the time, the level, the room */
        uint32_t tn = ur.state == UR_RECORDING ? usbrec_tenths() : ur.nblk;
        if (ur.state == UR_CHOOSE) {                                   /* the knobs, the loop's place, the memory */
            for (k = 0; k < 4u; k++)
                h = (h ^ (uint32_t)(ur.kv[k] + 32768)) * 16777619u;
            h = (h ^ ((ur.pp >> 12) / TAPE_BLK * (DW + 1u) / (ur.nblk ? ur.nblk : 1u))) * 16777619u;
            for (t = 0; t < NTRK; t++)
                h = (h ^ tape_ctl[t].nch) * 16777619u;
            h += mem_count(MEM_FREE) * 2654435761u;
        }
        return h + ur.state * 31u + tn * 131u + (uint32_t)(ur.dest + 1) * 7u + (uint32_t)usbrec_live() * 65537u +
               (uint32_t)(ur.peak * 24 / 32767) * 524287u + ur_room() * 8191u + sys.ntrk * 3u;
    }
    if (ui.view == VIEW_MIXER) {
        for (t = 0; t < NCH; t++)
            h = (h ^ (uint32_t)(tp[sys.sel].ch[t] + 32768)) * 16777619u;
        if (!ui.chan)                                                  /* the memory: who holds how much */
            for (k = 0; k < MEM_NC; k++)
                h = (h ^ mem_owner[k]) * 16777619u;
        if (ui.chan == CHAN_MASTER)                                    /* the compressor's knobs and what it takes off */
            for (k = 0; k < NMS; k++)
                h = (h ^ (uint32_t)(mst[k] + 32768)) * 16777619u;
        if (ui.chan == CHAN_KEY)                                       /* the key and scale */
            return h + 0x6E1u + (uint32_t)keyp[KY_KEY] * 31u + (uint32_t)keyp[KY_SCAL] * 977u + ui.last * 7u;
        if (ui.chan == CHAN_SETUP)                                     /* the preferences */
        {
            uint32_t k, lh = 0;
            for (k = 0; k < LD_N; k++)
                lh = (lh ^ (uint32_t)(ld_pm[k] + 5u) / 10u) * 16777619u;
            return h + 0x5E7u + bryo_prefs * 31u + settings_hold * 977u + settings_leds * 7919u + lh;
        }
        return h + ui.chan * 977u + sys.ntrk * 5381u + (ui.chan ? 0u : sys.cpu_q8 * 100u / 256u / 5u * 7919u) +
               (ui.chan == CHAN_MASTER ? (uint32_t)comp.gr_q8 * 602u / 2560u * 104729u : 0u);
    }
    if (ui.view == VIEW_PROJECT && ui.pj_page == 2u) {                 /* your reels, which is pointed at */
        for (t = 0; t < USLOT_N; t++)
            h = (h ^ (uslot_ok[t] | (uint32_t)uslot_name[t][0] << 8 | (uint32_t)uslot_name[t][1] << 16)) * 16777619u;
        return h + ui.reel_sel * 31u + 0xEE1u;
    }
    if (ui.view == VIEW_PROJECT && ui.pj_page) {                       /* the track presets, which is pointed at */
        for (t = 0; t < TPR_N; t++)
            h = (h ^ (tpr_sum[t].used | (uint32_t)tpr_sum[t].src << 2)) * 16777619u;
        return h + ui.tpr_sel * 31u + 0x51u;
    }
    if (ui.view == VIEW_PROJECT) {                                     /* the slots, which is pointed at, loaded */
        for (t = 0; t < PJ_N; t++)
            h = (h ^ (pj_sum[t].used | pj_sum[t].src[0] << 2 | pj_sum[t].src[1] << 4 | pj_sum[t].src[2] << 6 |
                      (uint32_t)pj_sum[t].src[3] << 8)) * 16777619u;
        return h + ui.pj_sel * 31u + pj_cur * 977u;
    }
    if (ui.view == VIEW_ROUTE) {
        for (t = 0; t < NTRK; t++)
            h = (h ^ (uint32_t)(tp[t].recin + tp[t].src * 16u)) * 16777619u;
        return h + sys.rec * 977u + sys.ntrk * 5381u + 0x55u;
    }
    if (ui.kind == FOCUS_ARP) {                                        /* the arpeggiator: its knobs, its notes, the
                                                                        * step sounding (RIPL ARP: RIPPLE's) */
        const arp_rt_t *r = ui.page == 2u ? &reso_arp[sys.sel] : &arp_rt[sys.sel];
        for (k = 0; k < NARP; k++)
            h = (h ^ (uint32_t)(tp[sys.sel].arp[k] + 32768)) * 16777619u;
        for (k = 0; k < 4u; k++)
            h = (h ^ (uint32_t)(tp[sys.sel].dev[DEV_RESO][RP_AMODE + k] + 32768)) * 16777619u;
        for (k = 0; k < r->n; k++)
            h = (h ^ r->note[k]) * 16777619u;
        return h + ui.page * 389u + tp[sys.sel].src * 3571u + r->n * 131u + r->cur * 7u + r->on * 65537u + (r->pos & 63u) * 524287u;
    }
    for (k = 0; k < NPK; k++) {                                        /* both pages: page 2 shows in the picture */
        const int16_t *v = ui.kind == FOCUS_SLOT ? tp[sys.sel].mod[ui.slot] : dev_v(sys.sel, ui.dev);
        h = (h ^ (uint32_t)(v[k] + 32768)) * 16777619u;
    }
    h += ui.page * 389u + ui.zoom * 7349u;
    if (ui.kind == FOCUS_SLOT && tp[sys.sel].engine[ui.slot] == ME_SEQ)   /* SEQ draws its step values (and the one
                                                                         * playing) */
        h = (h ^ (uint32_t)(sys.playing ? mod_seq_now(sys.sel, ui.slot) + 2 : 0)) * 16777619u;
    if (ui.kind == FOCUS_SLOT && tp[sys.sel].engine[ui.slot] == ME_SEQ)
        for (k = 0; k < 16u; k++)
            h = (h ^ (uint32_t)(uint8_t)tp[sys.sel].steps[ui.slot][k]) * 16777619u + ui.steps_held * 7u;
    if (ui.kind == FOCUS_DEV && ui.dev == DEV_SRC)                     /* the source; the keys' range: the octave */
        h = (h ^ (tp[sys.sel].src * 7u + track[sys.sel].octave * 131u)) * 16777619u;
    if (ui.kind == FOCUS_DEV && ui.dev == DEV_SRC && tp[sys.sel].src == SRC_DRUM) {   /* DRUM: the pattern, the
                                                                                     * playhead, an instrument held */
        const track_params_t *P = &tp[sys.sel];
        for (k = 0; k < DRM_NBAR * 16u; k++)
            h = (h ^ P->dpat[k >> 4][k & 15u]) * 16777619u;
        for (k = 0; k < DRM_NBAR; k++)
            h = (h ^ P->dacc[k]) * 16777619u;
        for (k = 0; k < DRM_NINST * NDIN; k++)
            h = (h ^ (uint32_t)(P->dins[k / NDIN][k % NDIN] + 32768)) * 16777619u;
        h = (h ^ (ui.drm_inst + (sys.playing ? drm_now[sys.sel] * 31u + drm_pass[sys.sel] * 7919u : 0u))) * 16777619u;
    }
    if (ui.kind == FOCUS_DEV && ui.dev == DEV_SRC && tp[sys.sel].src == SRC_POLY)   /* POLY on the track's tape */
        h = (h ^ (tape_ver[sys.sel] * 31u)) * 16777619u;
    if (ui.kind == FOCUS_DEV && ((ui.dev == DEV_SRC && tp[sys.sel].src == SRC_TAPE) || ui.dev == DEV_GRAIN))   /* the tape, the head */
        h = (h ^ (tape_ver[sys.sel] * 31u + (uint32_t)(tape_head(sys.sel) * DW / 1000 + 7) + sys.rec * 977u)) * 16777619u;
    if (ui.kind == FOCUS_SLOT && tp[sys.sel].engine[ui.slot] == ME_FOLLOW)
        for (t = 0; t < NTRK; t++)
            h = (h ^ tape_ver[t]) * 16777619u;
    if (ui.kind == FOCUS_DEV && ui.dev == DEV_GRAIN) {                 /* GRAIN: its cursor, FROZEN, the grains */
        const grain_trk_t *G = &grain[sys.sel];
        const gr_buf_t *B = &gbuf[sys.sel];
        uint8_t sh[NTRK];
        gr_plan_into(sh);                                              /* (its share: other tracks move it too) */
        h = (h ^ (uint32_t)((G->cur >> 12) / 256 * 2 + grain_frozen + sh[sys.sel] * 4096u)) * 16777619u;
        h = (h ^ (uint32_t)(B->len + B->bars * 7u + (uint32_t)B->w / 1024u * 131u)) * 16777619u;
        for (k = 0; k < GR_SLOTS; k++)
            if (gr_on((uint32_t)k) && gslot[k].trk == sys.sel)
                h = (h ^ (uint32_t)(gslot[k].pos >> 18) ^ k << 24) * 16777619u;
    }
    if (ui.kind == FOCUS_DEV && ui.dev == DEV_SPACE) {                 /* SPACE: sounding or resting; its room's tail */
        const int16_t *q = rmp[clamp(tp[sys.sel].dev[DEV_SPACE][SP_ROOM], 0, 1)];
        h = (h ^ (uint32_t)space_sounding(sys.sel) * 0x9E37u) * 16777619u;
        for (k = 0; k < (int32_t)NRM; k++)
            h = (h ^ (uint32_t)(q[k] + 32768) << (k & 1u)) * 16777619u;
        h = (h ^ room[clamp(tp[sys.sel].dev[DEV_SPACE][SP_ROOM], 0, 1)].users) * 16777619u;
    }
    if (ui.kind == FOCUS_DEV && ui.dev == DEV_RESO) {                  /* RIPPLE: a key's root, the strings ringing */
        uint32_t ring = 0;
        for (k = 0; k < (int32_t)RS_N; k++)
            ring |= (uint32_t)(reso[sys.sel].lvl[k] > RV_RING) << k;
        h = (h ^ (uint32_t)reso_root16(sys.sel) ^ ring << 24) * 16777619u;
    }
    if (ui.kind == FOCUS_DEV && ui.dev == DEV_GRAIN)                   /* GRAIN draws TAPE's loop window too */
        h = (h ^ (uint32_t)(tp[sys.sel].dev[DEV_SRC][0] << 8 | tp[sys.sel].dev[DEV_SRC][1])) * 16777619u;
    return h + (ui.kind == FOCUS_SLOT ? tp[sys.sel].engine[ui.slot] * 65537u : 0u);
}

static void draw_viz(void)
{
    uint32_t sig = viz_sig(), f = ui.last < 4u ? pdesc_at(ui.kind == FOCUS_SLOT ? ME_P[tp[sys.sel].engine[ui.slot]]
                                                          : ui.kind == FOCUS_ARP ? ARP_P : dev_p(sys.sel, ui.dev),
                                                          4u * ui.page + ui.last) : 0xFFu;
    const int16_t *v = ui.kind == FOCUS_SLOT ? tp[sys.sel].mod[ui.slot] : ui.kind == FOCUS_ARP ? tp[sys.sel].arp
                     : dev_v(sys.sel, ui.dev);   /* both pages */
    if (!ui.force && sig == ui.sig_viz)
        return;
    ui.sig_viz = sig;
    px_colors();
    px_begin(VZ_H);
    if (ui.msg_t) {
        viz_message();
    } else if (ui.view == VIEW_USBREC) {
        viz_usbrec();
    } else if (ui.view == VIEW_MIXER) {
        if (ui.chan == CHAN_SETUP)
            viz_setup();
        else if (ui.chan == CHAN_KEY)
            viz_key();
        else if (ui.chan == CHAN_MASTER)
            viz_master(f);
        else if (ui.chan)
            viz_channel(f);
        else
            viz_memory();
    } else if (ui.view == VIEW_PROJECT) {
        viz_project();
    } else if (ui.view == VIEW_ROUTE) {
        viz_route(ui.last < 4u ? ui.last : 0xFFu);
    } else {
        if (ui.kind == FOCUS_ARP) {
            viz_arp(v, f);
        } else if (ui.kind == FOCUS_SLOT) {
            switch (tp[sys.sel].engine[ui.slot]) {
            case ME_WAVE: viz_wave(v, f); break;
            case ME_ADSR: viz_adsr(v, f); break;
            case ME_FOLLOW: viz_follow(v, f); break;
            default: viz_seq(v, f); break;
            }
        } else {
            switch (ui.dev) {
            case DEV_SRC:
                if (tp[sys.sel].src == SRC_SYNTH)
                    viz_synth(v, f);
                else if (tp[sys.sel].src == SRC_POLY)
                    viz_poly(v, f);
                else if (tp[sys.sel].src == SRC_DRUM)
                    viz_drum(v, f);
                else
                    viz_tape(v, f);
                break;
            case DEV_GRAIN: viz_grain(v, f); break;
            case DEV_RESO: {                                    /* (a key's root shows as PTCH's) */
                int16_t rv[NPK];
                uint32_t k;
                for (k = 0; k < NPK; k++)
                    rv[k] = v[k];
                rv[RP_PTCH] = (int16_t)(reso_root16(sys.sel) / 16);
                viz_reso(rv, f);
                break;
            }
            case DEV_COLOR: viz_color(v, f); break;
            default: viz_space(v, f); break;
            }
        }
    }
    px_blit(UI_VIZ_Y);
}
