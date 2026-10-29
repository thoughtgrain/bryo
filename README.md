# Bryo

This repository holds Bryo, my firmware for the M-VAVE FM-1, and its website.

## bryo/

[Bryo](bryo/) turns the FM-1 into a four-track sound-sculpting instrument. It's a fork of
[Felucca](https://github.com/hugelton/Felucca), the GPL-3.0-only replacement firmware by Leo Kuroshita / Hügelton
Instruments, starting from Felucca 1.0.3 (`b22a24b`), imported unmodified so every change after it is easy to see
in the history. (The name: *bryon* is Greek for moss.)

- To play it: [the manual](bryo/docs/manual/README.md).
- What it is and how to build it: [bryo/README.md](bryo/README.md).
- Where it stands, and how to work on it: [bryo/docs/bryo-status.md](bryo/docs/bryo-status.md).
- The thanks, and how I try to be a good downstream: [bryo/ACKNOWLEDGEMENTS.md](bryo/ACKNOWLEDGEMENTS.md). The GPL
  modification notice, including what still says "Felucca" and why: [bryo/NOTICE.md](bryo/NOTICE.md).

Phases 1 to 9 of 10 are built, and it has run on an FM-1 since 2026-10-10; the hardware checkpoint is under way.
Everything inside `bryo/` is GPL-3.0-only, same as upstream.

## site/

[site/](site/) is the website, <https://thoughtgrain.github.io/bryo/>: plain HTML, CSS and JavaScript with no
build step, a landing page and a short guide that links into the manual. `.github/workflows/pages.yml` publishes
it to the `gh-pages` branch on every push to the default branch that touches it. How it's put together and how to
run it locally: [site/README.md](site/README.md).
