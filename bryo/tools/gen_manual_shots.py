#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The manual's screenshots (docs/manual/img/), from the host's screen renders.

  tools/gen_manual_shots.py [RENDERS] [OUT]     (default: build/bryo_ui/ppm, docs/manual/img)

Run tests/run_tests.sh first (it renders every screen into build/bryo_ui/ppm). Each shot is a render at twice its
size, pixel for pixel, so the manual shows exactly what tests/bryo_golden.txt fingerprints. screen.png is the
TAPE page with numbered marks, for the manual's "reading the screen". After a deliberate screen change, run this
again and commit the pictures that changed.
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw

SHOTS = [
    "arp", "arp_add", "arp_drum", "arp_res", "boot", "calibration", "color", "color2", "color3", "crash", "drum", "drum_inst", "drum_kit", "drum_step",
    "drum_vary", "grain", "grain2", "grain4", "grain_body", "grain6", "grain_busy", "grain_frozen", "grain_playing", "mixer", "mixer_channel",
    "mixer_held", "mixer_master", "mixer_tracks3", "mod1", "mod1_p4", "mod2", "mod3_steps", "mod4_follow",
    "mod_depth", "mod_marks", "poly", "presets", "presets_factory", "projects", "resonator2", "resonator_wet",
    "project_new", "projects_factory", "route", "route_chain", "scl_held", "setup", "space", "space2", "space_long", "synth", "synth_filter", "tape", "tape2", "tape3", "tape_env",
    "tape_loop", "tape_playing", "tape_rec", "tape_speed", "uboot", "usbrec", "usbrec_rec", "usbrec_trim",
]
SCALE = 2
# the marks on screen.png, in the margin around the shot: (number, side, x, y), x and y the point in the render's
# own pixels (240 x 240) that the mark's line runs to
MARKS = [(1, "top", 10, 8), (2, "top", 45, 8), (3, "top", 175, 8), (4, "left", 10, 75),
         (5, "left", 10, 165), (6, "bottom", 40, 233), (7, "bottom", 205, 233)]
MARGIN = 44


def shot(src):
    im = Image.open(src).convert("RGB")
    return im.resize((im.width * SCALE, im.height * SCALE), Image.NEAREST)


def save(im, path):
    im.quantize(colors=16, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(path, optimize=True)


def main():
    src = Path(sys.argv[1] if len(sys.argv) > 1 else "build/bryo_ui/ppm")
    out = Path(sys.argv[2] if len(sys.argv) > 2 else "docs/manual/img")
    out.mkdir(parents=True, exist_ok=True)
    missing = [n for n in SHOTS if not (src / f"BRYO_{n}.ppm").exists()]
    if missing:
        sys.exit(f"gen_manual_shots: no render for {', '.join(missing)} in {src} (run tests/run_tests.sh)")
    for n in SHOTS:
        save(shot(src / f"BRYO_{n}.ppm"), out / f"{n}.png")
    s = shot(src / "BRYO_tape_playing.ppm")
    im = Image.new("RGB", (s.width + MARGIN, s.height + 2 * MARGIN), (255, 255, 255))
    im.paste(s, (MARGIN, MARGIN))
    dr = ImageDraw.Draw(im)
    r, ink = 13, (255, 176, 0)
    for k, side, x, y in MARKS:
        px, py = MARGIN + x * SCALE, MARGIN + y * SCALE
        cx, cy = {"top": (px, MARGIN // 2), "bottom": (px, im.height - MARGIN // 2), "left": (MARGIN // 2, py)}[side]
        dr.line((cx, cy, px, py), fill=ink, width=3)
        dr.ellipse((px - 4, py - 4, px + 4, py + 4), fill=ink)
        dr.ellipse((cx - r, cy - r, cx + r, cy + r), fill=ink, outline=(0, 0, 0), width=2)
        dr.text((cx, cy), str(k), fill=(0, 0, 0), anchor="mm", font_size=18)
    save(im, out / "screen.png")
    print(f"gen_manual_shots: {len(SHOTS) + 1} pictures -> {out}")


if __name__ == "__main__":
    main()
