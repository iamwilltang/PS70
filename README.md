# William Tang — Personal-Portfolio Website

Live site 👉 <https://williamxtang.com>  
CS50 Final Project — Harvard College, AY 2025

---

## 1 • What this project is

A single-page (plus blog) personal-portfolio site that showcases my projects, skills and contact information.  
Major goals:

* modern, animated landing page that feels “app-like”  
* maintainable HTML/CSS/JS codebase I can extend after the course  
* lightweight back-end (only for the contact form), deployable on inexpensive shared hosting

---

## 2 • Quick start

| Action | Command / Steps |
|--------|-----------------|
| **Clone** | Download the ZIP file from Gradescope |
| **Serve locally** | `python3 -m http.server 8000` then browse to <http://localhost:8000> |
| **Contact form** | Requires PHP (≥7.4).  If you run a local PHP server:<br>`php -S localhost:8080` |
| **Build & deploy** | Static files can be dropped onto any host. I used Hostinger for hosting; I uploaded the repo contents (public_html/) onto their file-manager. This saved me the trouble of figuring out the hosting vs domain flagging issue with contact.php. |

---

## 3 • File / folder layout (top-level)

```
assets/               → third-party CSS, JS, fonts & images
│  ├─ css/
|  ├─ fonts/
│  ├─ img/
│  ├─ js/
|  └─theme-color/
blog.html, single-blog.html     → my blog
index.html                      → landing page
contact.php                     → AJAX mail handler (no DB)
style.css                       → my custom style layer
README.md, DESIGN.md
```

---

## 4 • Features for users

* **Intro animation** (pre-loader spinner + Typed.js tagline)
* Smooth-scroll navigation & automatic scroll-spy highlight
* Isotope-style project filter (MixItUp)
* Image lightbox (Magnific Popup)
* Animated counters, WOW.js scroll reveals, Owl Carousel testimonials (semi-removed because not really relevant)
* Basic blog engine (static HTML today but Markdown → HTML ready)
* AJAX/validator contact form that e-mails **wxt@williamxtang.com**

---

## 5 • Configuring the contact form

`contact.php` expects the following environment variables (or hard-coded values):

```php
$from      = 'wxt@williamxtang.com';
$subject   = 'New message from contact form';
```

Hostinger lets you create a domain mailbox, so the PHP `mail()` function worked right away for me. If you switch hosts, update this file accordingly.

---

## 6 • Third-party libraries & licences

| Library | Purpose | Licence |
|---------|---------|---------|
| **Bootstrap 3.3.7** | grid & components & Glyphicon Halflings | MIT |
| **jQuery 2.1.4** | DOM & AJAX | MIT |
| **Font Awesome 4.7** | icons | SIL OFL (Fonts) / MIT (CSS) |
| **Typed.js** | typing animation | MIT |
| **WOW.js + Animate.css** | scroll reveal/animations | MIT |
| **MixItUp 2** | “Projects” filtering | Commercial / GPL — using open-source GPL build |
| **Magnific Popup** | lightbox | MIT |
| **Owl Carousel 2** | testimonials slider | MIT | (included but then not used)

All are bundled locally to avoid CDN latency. Each retains its original header comment where the licence is declared—per the licence terms.

---

## 7 • Attribution

*HTML structure was cobbled together + refined with AI help after surveying several open-source portfolio templates.*  
I don't believe any template code is included verbatim, but the overall aesthetic (full-screen hero, hover-overlay portfolio cards, etc.) is inspired by ideas I saw while researching.

---

## 8 • Video demo

<https://www.loom.com/share/673dac729b0747aead02e0c0aeb1f644?sid=60b16640-af50-4317-a82c-4298dca95956> (≤ 3 min walkthrough)

---

## 9 • Known issues / future work

* Upgrade to Bootstrap 5 / ES-modules
* Replace jQuery AJAX with Fetch API
* Add simple CMS or Markdown pipeline for blog posts
* Lighthouse audit: improve a11y color contrast
