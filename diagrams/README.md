# Diagrams

Figures for this book are pairs of files in this directory:
`<name>.svg` (rendered, embedded in the site) and `<name>.excalidraw`
(editable source).

## Style

Diagrams are heavy (every chapter gets at least one), trending toward a
**C4-style** standard, and carry **no UML** — no class diagrams, no
arrows-with-stereotypes notation. Use `lgtm-diagram-generator`'s default
amber house theme (this project does not override the palette). Emphasis
or "ink" boxes that call out a decision point are **light fill with dark
text** — never a dark-filled box with light text.

## Naming and embedding

Each figure is named `<NN>-<slug>` (chapter number plus a short slug,
e.g. `08-strategy-forces`) and embedded in a chapter's Markdown body via:

```liquid
{% include excalidraw.html name="NN-slug" caption="Figure N.x: ..." %}
```

The include resolves the SVG at `/diagrams/<name>.svg` and offers the
`.excalidraw` source as a download at `/diagrams/<name>.excalidraw`. If a
figure hasn't been drawn yet, the include shows a placeholder so the page
doesn't break. Add a matching entry to `_data/diagrams.yml` so the
`/diagrams/` gallery picks it up alongside the chapter embed.

## Generating figures

Drive `scripts/generate_diagram.py` from `lgtm-diagram-generator` for
every figure so the whole set stays visually consistent. The first two
figures landed with chapters 1 and 8 (r01): `01-forces-and-decisions`
(patterns as decisions under forces) and `08-strategy-forces` (Strategy
expressed as a choice under forces, not a UML class diagram).
