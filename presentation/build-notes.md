# Companion decks — build notes

Three Red Hat-branded 16:9 decks, built with pptxgenjs (lgtm-presentation house style).

| Deck source | Output |
|-------------|--------|
| `deck-101.js` | `cpp-design-patterns-101-patterns-and-solid.pptx` |
| `deck-201.js` | `cpp-design-patterns-201-idiomatic-cpp.pptx` |
| `deck-testing-legacy-ai.js` | `cpp-design-patterns-testing-legacy-ai.pptx` |

## Rebuild
```sh
cd presentation
npm install pptxgenjs          # first time only (node_modules is gitignored)
# regenerate figure PNGs from the site diagrams:
for f in ../diagrams/*.svg; do
  python3 -c "import cairosvg,sys; cairosvg.svg2png(url='$f', write_to='png/'+__import__('os').path.basename('$f')[:-4]+'.png', output_width=1600)"
done
node deck-101.js && node deck-201.js && node deck-testing-legacy-ai.js
```

Figures in `png/` are rendered from `../diagrams/*.svg`. Speaker notes match slides 1:1.
Conventions: Red Hat fonts, logo on every content slide, shipping-domain examples,
ideas conveyed directly (no chapter references on slides).
