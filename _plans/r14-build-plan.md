---
title: "r14 Build Plan"
output: false
---

# r14 — Second diagrams (12 chapters)

From the diagram-coverage audit: 12 of 33 chapters get a SECOND figure (5 HIGH,
7 MED). Each is a paired SVG + `.excalidraw` via lgtm-diagram-generator, C4-style,
no UML, light-fill emphasis boxes, embedded as "Figure N.2" in the relevant
section, and registered in `_data/diagrams.yml` (orchestrator).

| Ch | Diagram name | Subject | Prio |
|----|--------------|---------|------|
| 31 | 31-final-project-sequence | runtime sequence of one shipment across the 5 patterns | HIGH |
| 20 | 20-memento-save-restore | Memento save/restore via an opaque caretaker token | HIGH |
| 15 | 15-bridge-matrix | Bridge two axes: N+M classes vs N×M explosion | HIGH |
| 12 | 12-factory-method-flow | Factory Method: fixed sequence with one varying creation hook | HIGH |
| 3 | 03-lsp-clamp | LSP template-method clamp control flow | HIGH |
| 23 | 23-prototype-registry | self-registering prototype registry across translation units | MED |
| 10 | 10-fsm-representations | one rule set, four FSM representations side by side | MED |
| 24 | 24-threadpool-vs-active-object | thread pool vs active object runtime flow | MED |
| 6 | 06-value-vs-reference | value vs reference semantics side by side | MED |
| 5 | 05-move-ownership-transfer | move transfers ownership between two RAII guards | MED |
| 16 | 16-decorator-ordering | decorator ordering before/after with cache short-circuit | MED |
| 28 | 28-instance-delegator | Introduce Instance Delegator seam over a static API | MED |

## Step board

| # | Step | Owner | Status |
|---|------|-------|--------|
| 0 | Branch release/r14 + plan | main | DONE |
| 1 | Exec A: ch31, ch20, ch15 (generate + embed) | exec | TODO |
| 2 | Exec B: ch12, ch3, ch23 | exec | TODO |
| 3 | Exec C: ch10, ch24, ch6 | exec | TODO |
| 4 | Exec D: ch5, ch16, ch28 | exec | TODO |
| 5 | Register all 12 in `_data/diagrams.yml` | main | TODO |
| 6 | Integration jekyll build; confirm each chapter shows 2 figures | main | TODO |
| 7 | Opus validation: C4/no-UML/light-fill, embeds resolve, captions correct | validate | TODO |
| 8 | PR, squash-merge, tag r14.x, release | main | TODO |

## Acceptance criteria
1. 12 paired `diagrams/<name>.svg` + `.excalidraw`, C4-style, NO UML, light-fill
   emphasis boxes + dark text, visually consistent with existing diagrams.
2. Each embedded in its chapter as "Figure N.2" in the section where its mechanism
   is discussed (not stacked next to Figure N.1 unless that is the right spot).
3. All 12 registered in `_data/diagrams.yml` (gallery page lists them).
4. `bundle exec jekyll build` exits 0; each target chapter renders two figures.
5. No prose changed beyond the one include line (+ an optional lead-in clause); no
   code touched; superbuild unaffected.
