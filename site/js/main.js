/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Bryo site behaviour. No framework, no build step.
 *
 * Every feature finds its own elements and returns quietly when they aren't on the page, so one file
 * serves both index.html and docs.html. Without JavaScript the pages still work: the menu links are
 * plain anchors (the footer repeats them), the first tab's panel is visible, and the hero shows the
 * boot screen.
 */
(function () {
  "use strict";

  var reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)");

  function $(sel, root) { return (root || document).querySelector(sel); }
  function $$(sel, root) { return Array.prototype.slice.call((root || document).querySelectorAll(sel)); }

  /* ---------- menu: a <dialog>, so focus is trapped and Escape closes it natively ---------- */
  function initMenu() {
    var menu = $("#menu");
    var openBtn = $("[data-menu-open]");
    if (!menu || !openBtn) return;

    // Very old browsers without <dialog>: send the button to the footer's links instead.
    if (typeof menu.showModal !== "function") {
      openBtn.addEventListener("click", function () {
        var footer = $(".site-footer");
        if (footer) footer.scrollIntoView();
      });
      return;
    }

    openBtn.addEventListener("click", function () {
      menu.showModal();
      openBtn.setAttribute("aria-expanded", "true");
    });

    menu.addEventListener("close", function () {
      openBtn.setAttribute("aria-expanded", "false");
      openBtn.focus();
    });

    $$("[data-menu-close]", menu).forEach(function (btn) {
      btn.addEventListener("click", function () { menu.close(); });
    });

    // A link to a spot on this page has to close the menu, or the dialog stays over it.
    $$("a", menu).forEach(function (a) {
      a.addEventListener("click", function () { menu.close(); });
    });
  }

  /* ---------- hero: cycle through real screens ----------
   * Moves on its own, so WCAG 2.2.2 wants a way to stop it: the Pause button. With reduced motion it
   * starts paused and you step through by pressing Play. */
  function initScreens() {
    var root = $("[data-screens]");
    if (!root) return;
    var imgs = $$(".device__screen img", root);
    var label = $("[data-screen-label]", root);
    var toggle = $("[data-screen-toggle]", root);
    if (imgs.length < 2 || !toggle) return;

    var names = ["Boot", "Home · Tape", "Edit · Grain", "Edit · Resonator", "FX · Space", "Glo · Mixer"];
    var i = 0;
    var timer = null;
    var INTERVAL = 2600;

    function show(n) {
      imgs[i].classList.remove("is-on");
      i = (n + imgs.length) % imgs.length;
      imgs[i].classList.add("is-on");
      if (label) label.textContent = names[i] || "";
    }

    function play() {
      if (timer) return;
      timer = window.setInterval(function () { show(i + 1); }, INTERVAL);
      toggle.textContent = "Pause";
      toggle.setAttribute("aria-pressed", "false");
    }

    function pause() {
      window.clearInterval(timer);
      timer = null;
      toggle.textContent = "Play";
      toggle.setAttribute("aria-pressed", "true");
    }

    toggle.addEventListener("click", function () { timer ? pause() : play(); });

    // Don't spin while the tab is hidden; pick up again only if it was running.
    var wasPlaying = false;
    document.addEventListener("visibilitychange", function () {
      if (document.hidden) { wasPlaying = !!timer; pause(); }
      else if (wasPlaying) play();
    });

    if (reduceMotion.matches) pause(); else play();
  }

  /* ---------- tabs (WAI-ARIA tabs pattern, automatic activation) ---------- */
  function initTabs() {
    $$("[data-tabs]").forEach(function (root) {
      var tabs = $$('[role="tab"]', root);
      if (!tabs.length) return;

      function select(tab, focus) {
        tabs.forEach(function (t) {
          var on = t === tab;
          t.setAttribute("aria-selected", on ? "true" : "false");
          t.tabIndex = on ? 0 : -1;
          var panel = document.getElementById(t.getAttribute("aria-controls"));
          if (panel) panel.hidden = !on;
        });
        if (focus) tab.focus();
      }

      tabs.forEach(function (tab, idx) {
        tab.addEventListener("click", function () { select(tab, false); });
        tab.addEventListener("keydown", function (e) {
          var next = null;
          switch (e.key) {
            case "ArrowRight": next = tabs[(idx + 1) % tabs.length]; break;
            case "ArrowLeft": next = tabs[(idx - 1 + tabs.length) % tabs.length]; break;
            case "Home": next = tabs[0]; break;
            case "End": next = tabs[tabs.length - 1]; break;
            default: return;
          }
          e.preventDefault();
          select(next, true);
        });
      });
    });
  }

  /* ---------- docs: highlight the section you're reading in the contents ---------- */
  function initScrollSpy() {
    var toc = $(".toc");
    if (!toc || !("IntersectionObserver" in window)) return;
    var links = $$('a[href^="#"]', toc);
    var byId = {};
    links.forEach(function (a) { byId[a.getAttribute("href").slice(1)] = a; });
    var sections = Object.keys(byId)
      .map(function (id) { return document.getElementById(id); })
      .filter(Boolean);
    if (!sections.length) return;

    var visible = {};
    function mark() {
      // the first section (in document order) that has any part in the reading band wins
      var current = null;
      for (var k = 0; k < sections.length; k++) {
        if (visible[sections[k].id]) { current = sections[k].id; break; }
      }
      if (!current) return;
      links.forEach(function (a) {
        if (a === byId[current]) a.setAttribute("aria-current", "true");
        else a.removeAttribute("aria-current");
      });
    }

    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (en) { visible[en.target.id] = en.isIntersecting; });
      mark();
    }, { rootMargin: "-30% 0px -60% 0px" });
    sections.forEach(function (s) { io.observe(s); });

    // On phones the contents is a <details>: close it after you pick a section.
    var details = toc.closest("details") || (toc.tagName === "DETAILS" ? toc : null);
    if (details) {
      links.forEach(function (a) {
        a.addEventListener("click", function () {
          if (window.matchMedia("(max-width: 999px)").matches) details.open = false;
        });
      });
      // Desktop shows it as a sticky sidebar, always open; phones start it closed so it doesn't push
      // the page down. (Without JavaScript it stays open, which is the safe default.)
      var wide = window.matchMedia("(min-width: 1000px)");
      var sync = function () { details.open = wide.matches; };
      sync();
      if (wide.addEventListener) wide.addEventListener("change", sync);
    }
  }

  /* ---------- docs: copy buttons on code blocks ---------- */
  function initCopy() {
    var blocks = $$(".code");
    if (!blocks.length) return;

    var status = document.createElement("p");
    status.className = "visually-hidden";
    status.setAttribute("role", "status");
    document.body.appendChild(status);

    function announce(msg) {
      status.textContent = "";
      window.setTimeout(function () { status.textContent = msg; }, 50);
    }

    function fallbackCopy(text) {
      var ta = document.createElement("textarea");
      ta.value = text;
      ta.setAttribute("readonly", "");
      ta.style.position = "fixed";
      ta.style.opacity = "0";
      document.body.appendChild(ta);
      ta.select();
      var ok = false;
      try { ok = document.execCommand("copy"); } catch (e) { ok = false; }
      document.body.removeChild(ta);
      return ok ? Promise.resolve() : Promise.reject(new Error("copy failed"));
    }

    blocks.forEach(function (block) {
      var pre = $("pre", block);
      if (!pre) return;
      var btn = document.createElement("button");
      btn.type = "button";
      btn.className = "btn code__copy";
      btn.textContent = "Copy";
      btn.setAttribute("aria-label", "Copy code");
      block.appendChild(btn);

      btn.addEventListener("click", function () {
        var text = pre.innerText.replace(/\n$/, "");
        var p = navigator.clipboard && window.isSecureContext
          ? navigator.clipboard.writeText(text).catch(function () { return fallbackCopy(text); })
          : fallbackCopy(text);
        p.then(function () {
          btn.textContent = "Copied";
          announce("Copied to the clipboard");
        }, function () {
          btn.textContent = "Select";
          announce("Couldn't copy. Select the text and copy it yourself.");
          var range = document.createRange();
          range.selectNodeContents(pre);
          var sel = window.getSelection();
          sel.removeAllRanges();
          sel.addRange(range);
        }).then(function () {
          window.setTimeout(function () { btn.textContent = "Copy"; }, 1800);
        });
      });
    });
  }

  function init() {
    var steps = [initMenu, initScreens, initTabs, initScrollSpy, initCopy];
    steps.forEach(function (fn) {
      // One feature failing shouldn't take the others down with it.
      try { fn(); } catch (err) { if (window.console) console.error("bryo-site:", fn.name, err); }
    });
  }

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
  else init();
})();
