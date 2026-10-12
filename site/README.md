<!-- SPDX-License-Identifier: GPL-3.0-only -->
# The Bryo site

This folder is Bryo's website: a landing page and a short documentation page, in plain HTML, CSS and JavaScript.
No framework, no build step, no `node_modules`. GitHub Pages serves it from the `gh-pages` branch.

## What's in here

```
site/
├── index.html        the landing page: the instrument, the chain, sources, specs, getting it, thanks to Felucca
├── docs.html         the short guide (install, the panel, recording, saving, reference, building, credits)
├── css/style.css     every style, with the colour and type tokens at the top
├── js/main.js        the menu, the hero's screens, the chain tabs, the docs' contents and copy buttons
├── img/
│   ├── icons.svg     the logo, menu, close and GitHub icons as <symbol>s
│   ├── favicon.svg
│   ├── panel.svg     the labelled FM-1 panel, copied from the manual
│   └── *.png         real Bryo screens, copied from bryo/docs/manual/img
└── .nojekyll         tells Pages to serve the files as they are
```

## The look, and why

I took the cues from Elektron's product pages: near-black, hairline rules, a sticky strip with the product's name
and one outlined button, big tight headings with a pixel-type `/ 01` above each section. It fits because the FM-1's
screen *is* pixel art, and the pictures here are the real thing: rendered by the same C code that runs on the
device (`tests/bryo_host.c`), on its 2×2 px dot grid. `image-rendering: pixelated` keeps them crisp when they scale.

Type is split by job:

- **Silkscreen** (a boxy pixel face) for labels, buttons, numbers and nav. Short strings only.
- **Schibsted Grotesk** for anything you read. Pixel type at paragraph length is hard on the eyes, so it never
  carries body copy.
- **IBM Plex Mono** for code.

Colour isn't decoration. It follows Bryo's own screen rule: white on black, and colour only says where you are or
what's alive. Blue is a page, purple is modulation, orange is sound, red is recording, yellow is something to know.
The tokens live at the top of `css/style.css`.

## Accessibility

Since this is a page about an instrument with no menus, I wanted the site itself to be just as direct to use, with
any input:

- A skip link, landmarks, one `h1` per page and headings in order.
- The menu is a native `<dialog>` opened with `showModal()`, so focus is trapped and Escape closes it without
  extra code. Closing it returns focus to the button that opened it.
- The chain is a WAI-ARIA tabs widget: arrow keys, Home and End move between stages, and only the selected tab is
  in the tab order.
- The hero's screens change on their own, so there's a Pause button (WCAG 2.2.2). With `prefers-reduced-motion`
  they start paused, and the tilt on the device goes away.
- Wide tables scroll inside their own focusable region, never the page, so nothing scrolls sideways at 320 px.
- Text contrast is at least 5.6:1 (the faintest grey) and 8.9:1 for body copy. Focus rings are a 2 px yellow
  outline. Colour swatches in the docs always sit next to the colour's name.
- Copy buttons announce "Copied" through a `role="status"` region, and fall back to selecting the text when the
  clipboard isn't available.

## Working on it locally

Any static server works. From the repository root:

```sh
python3 -m http.server 8000 --directory site
# open http://localhost:8000
```

Open it through a server, not `file://`: the icons are an external SVG sprite (`<use href="img/icons.svg#logo">`),
and browsers block that from `file://`.

If a manual screen changes (`tools/gen_manual_shots.py` regenerates them), copy the new PNG over the one in
`site/img/`. I copy them rather than pointing at `bryo/docs/manual/img/` because Pages only serves this folder.

## Deploying

`.github/workflows/pages.yml` runs on every push to the default branch that touches `site/` (or by hand from the Actions tab).
It first checks that every local `src` and `href` in the pages points at a real file, then pushes the contents of
`site/` to the `gh-pages` branch as a single fresh commit. Rebuilding the branch from scratch each time keeps it
small, since it never carries the source's history.

One setting is needed once, in the repository on GitHub: **Settings → Pages → Build and deployment → Source:
Deploy from a branch**, branch **gh-pages**, folder **/ (root)**. The site then lives at
<https://thoughtgrain.github.io/moss-and-air/>.

All the links back into the repository use `blob/HEAD/...`, which GitHub resolves to the default branch, so they
keep working whatever that branch is called.

## Credit

Bryo is a fork of [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita
([@kurogedelic](https://github.com/kurogedelic)) / Hügelton Instruments, and the site says so on both pages, in the
footer and in its own "Thank you, Felucca" section. The screens and the panel drawing come from Bryo's manual.
Everything here is GPL-3.0-only, like the rest of Bryo. The fonts load from Google Fonts under the SIL Open Font
Licence; the GitHub mark in `icons.svg` belongs to GitHub.
