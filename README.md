# William Tang — personal website

Live at <https://williamxtang.com/>

## Layout

| Path | What it is |
|---|---|
| `index.html` | The personal site — hero, about, selected work, writing, contact. |
| `style.css` | Styles for the personal site. Accent colour lives in `assets/theme-color/color_1.css`. |
| `assets/` | Images, fonts, and JS for the personal site. |
| `ps70.html` | Index of the digital fabrication archive. |
| `ps70-style.css` | Styles for every fabrication page. |
| `01_intro/` … `13_finalproject/` | One folder per week of Harvard PS70: the write-up plus its source files (`.ino`, `.py`, `.step`, `.dxf`, `.f3d`). |
| `strapdown.js` | Renders the fabrication write-ups. See below. |

## How the fabrication pages work

Each week's `index.html` keeps its content inside an `<xmp>` element, which
[strapdown.js](https://github.com/Naereen/StrapDown.js) renders on load. Markdown
and raw HTML both work inside it, with two consequences worth knowing:

- **Markdown rules still apply.** Underscores inside `<code>` need escaping as
  `&#95;`, and `<` as `&lt;`, or the parser will mangle them.
- **Code blocks are `<pre><code class="lang-cpp">`.** `ps70-style.css` sets
  `white-space: pre` on `pre code` deliberately — Google Code Prettify checks the
  computed value and collapses indentation without it.

`strapdown.js` is vendored and has been patched in two places: its stylesheet and
favicon injection, and its navbar injection. Both pointed at files that do not
exist here. The reasons are commented at each edit site.

## Conventions

- Both halves share Inter, the `#059bff` accent, and the same header treatment.
- Images are sized for display, not straight off the camera: 1400px on the long
  edge for write-ups, 720×446 for index cards under `assets/img/ps70-thumbs/`.
- The site is static. There is no build step — edit and push.
