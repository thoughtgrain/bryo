<!-- SPDX-License-Identifier: GPL-3.0-only -->
# The Bryo site

This folder is Bryo's website, <https://thoughtgrain.github.io/bryo/>: a landing page and a short guide, in plain
HTML, CSS and JavaScript. No framework, no build step, no `node_modules`. GitHub Pages serves it from the
`gh-pages` branch.

## What's in here

```
site/
├── index.html        the landing page: the instrument, the walkthrough, the chain, sources, modulation,
│                     arpeggiators, scenes, specs, getting it, thanks to Felucca
├── docs.html         the short guide: install, the panel, a first song, the sources and effects, the mixer,
│                     recording, saving, a quick reference, building, credits
├── css/style.css     every style, with the colour and type tokens at the top
├── js/main.js        the menu, the hero's screens, the walkthrough, the chain tabs, the docs' contents and copy buttons
├── fonts/            Bryo PX5, the firmware's screen face, regular and bold (generated)
├── img/
│   ├── wordmark.svg  BRYO in PX5, as the boot screen spells it (generated)
│   ├── favicon.svg   a B in PX5 (generated)
│   ├── panel.svg     the FM-1 panel as Bryo uses it (generated)
│   ├── steps/        the first-song guide's panels, one a step (generated)
│   ├── icons.svg     the menu, close and GitHub icons as <symbol>s
│   └── *.png         real Bryo screens, copied from bryo/docs/manual/img
├── tools/
│   ├── gen_px_webfont.py   the fonts, the wordmark and the favicon, from the firmware's PX5 table
│   ├── gen_panel.py        panel.svg and the step panels, from the same table, the screen's colours and real renders
│   └── steps.json          what each step of the first-song guide lights on the panel, and its screen
└── .nojekyll         tells Pages to serve the files as they are
```

## The look

The cues are from Elektron's product pages: near-black, hairline rules, a sticky strip with the product's name and
one outlined button, big tight headings with a pixel-type `/ 01` above each section. It fits because the FM-1's
screen *is* pixel art, and every graphic here comes from the current firmware:

- **The type is the firmware's.** `tools/gen_px_webfont.py` reads `PX5`, the 5×7 face in
  `bryo/tools/gen_px_font.py` (the table `ui_pxfont.h` is built from), and writes it as a web font, plus a bold cut
  with each dot doubled to the right, like the screens' titles. Lower case folds to capitals as `px_glyph()` does.
  A character PX5 doesn't draw isn't in the font, so pixel text sticks to what it has (no `·`, no `ü`, no `&`).
- **The colours are the firmware's,** value for value from `firmware/src/ui_px.c`: black, white, and Okabe and
  Ito's colour-blind-safe accents. Colour only says where you are or what's alive, as on the screen: sky blue
  `#56B4E9` a page, reddish purple `#CC79A7` modulation, orange `#E69F00` sound, vermillion `#D55E00` recording,
  yellow `#F0E442` something to know.
- **The panel is drawn from the current controls.** `tools/gen_panel.py` letters it in PX5, edges each pad in its
  page's colour and puts a real render in the screen. A control `docs/controls.tsv` marks as planned is drawn dim
  and dashed (none is now).
- **The screens are the real renders** from `tests/bryo_host.c`, on the firmware's 2×2 px dot grid, shown flat at a
  whole multiple of their size with `image-rendering: pixelated`.
- **Wireframe, mono all the way down.** No fills and no glows: buttons, tabs, cards, tables and code are 1 px wires
  on black, and hover and focus change a wire's colour. Guides are dashed, a 48 px drafting grid sits behind the
  hero, and an L of white at each corner marks the things you look at (hidden in forced-colours mode).
- **Two fixed-width faces:** Bryo PX5 for labels, buttons, numbers and nav (short strings only); IBM Plex Mono for
  everything you read, with tight tracking on headings and a 64-character measure for body text.

Below the walkthrough, the homepage runs in bands: each part gets a full-width row with one firmware screen beside
its words, swapping sides from band to band. The screens never grow past 480 px, twice the firmware's 240, so the
dots stay square. On phones every band is screen first, then words.

## When the firmware changes

From the repository root:

```sh
# a manual screen changed (bryo/tools/gen_manual_shots.py): copy every screen the site uses
for f in site/img/*.png; do m=bryo/docs/manual/img/$(basename "$f"); [ -f "$m" ] && cp "$m" "$f"; done
python3 site/tools/gen_panel.py                              # a control's job or a colour changed
python3 site/tools/gen_panel.py --steps site/tools/steps.json   # ...and the first-song guide's panels
python3 site/tools/gen_px_webfont.py                         # PX5 changed: fonts, wordmark, favicon
```

The webfont needs fontTools (`pip3 install fonttools`); the panel needs nothing beyond Python. The screens are
copied rather than linked because Pages only serves this folder. Two screens aren't in the manual
(`drum_playing.png`, `synth_amp.png`): they come from the host renders in `bryo/build/bryo_ui`.

Then read the pages against the manual: the text is written by hand, and it's what falls behind.

## The first-song guide's pictures

Every step of the docs' "Your first song" guide shows the panel with the controls to use lit, and the real screen
that step gives. Each entry in `tools/steps.json` names a screen (a png in `img/`) and the marks: `"press"` (an
orange outline), `"hold"` (an orange fill: keep it down while you do the rest) or `["turn", degrees]` (the knob's
pointer swung that far from 12 o'clock). The generator writes `img/steps/<id>.svg`; `dials` is the panel with all
eight dials lit. Where a step's exact state has no render, the step shows the page it's on and says the value in
words. The step SVGs are about 125 KB each before compression and load lazily.

## The walkthrough ("Watch your hands")

The homepage's section 02 plays the FM-1 for you, so people see the whole idea (hold a pad, turn a knob, no menus)
before reading about it. `initTour` in `js/main.js` fetches `img/panel.svg` and puts it inline; `gen_panel.py`
gives every control a `<g id="ctl-NAME">` with its outline as `rect.hit` and a knob's pointer as `rect.ptr`, so
CSS can light them (the names are listed at the top of the generator). Each routine in `TOUR` is a list of steps
that **press**, **hold** and **turn** controls, then swap the screen. Every screen is a real render, and the steps
are ordered so the picture is always true. If the fetch fails, the plain `<img>` panel stays and the steps still
work.

It plays by itself only while it's on screen and the tab is visible, never with `prefers-reduced-motion`, and stops
once you use Prev, Next or a step (then each step is announced in a polite live region). The panel is
`aria-hidden`; the caption says in words what the hands are doing.

## Accessibility

- A skip link, landmarks, one `h1` per page and headings in order.
- The menu is a native `<dialog>` opened with `showModal()`: focus is trapped, Escape closes it, and focus returns
  to the button that opened it.
- The chain is a WAI-ARIA tabs widget (arrow keys, Home, End; only the selected tab is in the tab order).
- The hero's screens change on their own, so there's a Pause button (WCAG 2.2.2); with `prefers-reduced-motion`
  they start paused.
- Wide tables scroll inside their own focusable region, so nothing scrolls sideways at 320 px.
- Text contrast is at least 6.6:1, and 10.6:1 for body copy; the firmware's dim ink (4.5:1) is only used in the
  panel drawing. Focus rings are a 2 px yellow outline, and colour swatches always sit next to the colour's name.
- Copy buttons announce "Copied" through a `role="status"` region and fall back to selecting the text.

## Working on it locally

Any static server works. From the repository root:

```sh
python3 -m http.server 8000 --directory site
# open http://localhost:8000
```

Use a server, not `file://`: the icons are an external SVG sprite, and browsers block that from `file://`.

## Deploying

`.github/workflows/pages.yml` runs on every push to the default branch that touches `site/` (or by hand from the
Actions tab). It checks that every local `src` and `href` in the pages points at a real file, then pushes `site/`
to the `gh-pages` branch as a single fresh commit, so the branch never carries the source's history.

It needs one setting, once: **Settings → Pages → Build and deployment → Source: Deploy from a branch**, branch
**gh-pages**, folder **/ (root)**. Links back into the repository use `blob/HEAD/...`, which GitHub resolves to the
default branch whatever it's called.

## Credit

Bryo is a fork of [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita
([@kurogedelic](https://github.com/kurogedelic)) / Hügelton Instruments, and the site says so on both pages, in
the footer and in its own "Thank you, Felucca" section. Everything here is GPL-3.0-only, like the rest of Bryo. The
fonts load from Google Fonts under the SIL Open Font Licence; the GitHub mark in `icons.svg` belongs to GitHub.
