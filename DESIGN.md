# DESIGN DOCUMENT — William Tang Portfolio

_Last updated : 2025-05-11_

---

## 1 • High-level architecture

```
Browser  ⇄  HTML + SCSS compiled to style.css
        ⇄  JS modules (plugins & main.js)
                        ⇄  contact.php (AJAX endpoint) ─▶ mail()
```

*Purely static* except for the small PHP handler that relays form submissions to e-mail.  
Doing a static-first approach keeps hosting cheap and removes operational complexity.

---

## 2 • Front end

| Layer | Details |
|-------|---------|
| Layout | Hand-rolled semantic HTML5 + Bootstrap 3 grid. |
| Theme | Custom `style.css` overrides; colour palette switchable via `/assets/theme-color/*.css`. |
| Animation | Pre-loader spinner defined in CSS, removed after JS `loaded` class. WOW.js + Animate.css for scroll; Typed.js for hero text. |
| Interaction | `main.js` orchestrates sticky-header, smooth-scroll, counters, MixItUp filter and plugin initialisation. |

### Why Bootstrap 3 (not 5)?

* Chosen early in the course; all the third-party plugins I wanted to use had BS3 fragments and I preferred shipping a polished site over a large mid-project migration.  
* Grid & utility classes still met my needs; I poly-filled missing flexbox utilities with custom CSS.

---

## 3 • Back end

**contact.php**

* Validates POST input (name, e-mail, message) supplied by `contact.js`.
* Returns JSON `{ type : "success" | "danger", message : "..."}`
* Uses PHP `mail()`; no database, no PII stored. Hostinger’s spam protection is enabled.

Security considerations:

* Client-side validator prevents empty fields, but _server-side_ regex also sanitises e-mail.  
* `htmlspecialchars()` used on message body before interpolation.  
* Would add CSRF token and rate-limiter if scaling beyond personal use.

---

## 4 • Deployment & DevOps

* **Host** : Hostinger business starter plan (Apache + PHP 8.1)  
* **CI** : GitHub → manual upload (ZIP file on Gradescope) for now  
* **Domain** : williamxtang.com (bought on Hostinger)  
* **E-mail** : domain mailbox `wxt@…`; SMTP relay via `localhost` -- honestly it's more efficient to just open williamxtang.com and try the contact form.

A future improvement is GitHub Actions pushing directly via Hostinger’s API/SFTP.

---

## 5 • Key components & design decisions

| Component | Decision | Rationale / Trade-offs |
|-----------|----------|------------------------|
| **Pre-loader** | CSS keyframes + JS 2.5 s timeout | Quick win for perceived performance; easy to shorten/remove. |
| **Typed.js** | MIT lib vs custom | Tiny footprint and fast to integrate. |
| **MixItUp** | v2 GPL build | Simpler API than Isotope; GPL acceptable for educational project. |
| **Carousel** | Owl vs BS carousel | Owl supports touch-swipe and multiple items per slide. |
| **Blog** | Static HTML for now | Avoids DB/Markdown pipeline until needed. |

---

## 6 • Accessibility & performance

* Semantic tags (`header`, `nav`, `section`, etc.) aid screen-readers.
* `alt` text on all relevant images, ARIA labels on social links.
* Critical CSS inlined in `<head>`; JS deferred (`<script src …>` at bottom).
* Lighthouse mobile score: **92** after image compression.

---

## 7 • External assets & licenses

All third-party libraries are MIT, GPL or SIL OFL. Each retains its original header; full licenses stored in `/assets/` where provided. No license requires monetary payment or “commercial” license for personal use, so I think including them in this project should be compliant.

---

## 8 • Future roadmap

* Switch to Bootstrap 5 + ES modules
* Convert blog to Hugo or Eleventy static-site generator
* Add automated tests (Playwright for visual regression)
* Continuous deploy via GitHub Actions

---
