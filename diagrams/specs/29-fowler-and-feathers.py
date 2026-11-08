"""Feathers builds the safety net, then Fowler's named refactorings run under it."""

from generate_diagram import emit

L, R, W = 50, 740, 360       # box x for each stage, shared width
ROWS = [75, 145, 215]
H = 55
LAST_Y, LAST_H = 285, 60

emit(
    "29-fowler-and-feathers",
    1150,
    400,
    bands=[
        {"x": 20, "y": 35, "w": 420, "h": 330, "label": "Stage 1, Feathers: build the safety net",
         "fill": "#fafafa"},
        {"x": 710, "y": 35, "w": 420, "h": 330, "label": "Stage 2, Fowler: refactor under test",
         "fill": "#f5f5f5"},
    ],
    nodes=[
        {"x": L, "y": ROWS[0], "w": W, "h": H, "style": "box",
         "lines": ["Inherited ETA estimator", "wall-clock read inline, six-argument call"]},
        {"x": L, "y": ROWS[1], "w": W, "h": H, "style": "box",
         "lines": ["Characterization test", "pins the carrier dispatch and surcharge math"]},
        {"x": L, "y": ROWS[2], "w": W, "h": H, "style": "accent",
         "lines": ["Extract Interface + Parameterize Constructor", "the clock becomes a substitutable seam"]},
        {"x": L, "y": LAST_Y, "w": W, "h": LAST_H, "style": "ink",
         "lines": ["Safety net", "deterministic, fully green suite"]},
        {"x": R, "y": ROWS[0], "w": W, "h": H, "style": "box",
         "lines": ["Extract Function", "holiday rule and surcharge rule get names"]},
        {"x": R, "y": ROWS[1], "w": W, "h": H, "style": "box",
         "lines": ["Replace Conditional with Polymorphism", "CarrierProfile replaces the carrier if/else chain"]},
        {"x": R, "y": ROWS[2], "w": W, "h": H, "style": "box",
         "lines": ["Introduce Parameter Object", "ShipmentProfile replaces the six-argument call"]},
        {"x": R, "y": LAST_Y, "w": W, "h": LAST_H, "style": "ink",
         "lines": ["Modernized design", "identical answers, named refactorings, still green"]},
    ],
    edges=[
        *[{"x1": L + W / 2, "y1": y + H, "x2": L + W / 2, "y2": y2}
          for y, y2 in zip(ROWS, ROWS[1:] + [LAST_Y])],
        *[{"x1": R + W / 2, "y1": y + H, "x2": R + W / 2, "y2": y2}
          for y, y2 in zip(ROWS, ROWS[1:] + [LAST_Y])],
        {"x1": L + W, "y1": 300, "x2": R, "y2": 102, "label": "then Fowler's named refactorings",
         "amber": True, "lx": -26, "ly": -42},
    ],
    notes=[
        {"x": 575, "y": 20, "anchor": "middle", "bold": True,
         "text": "Fowler and Feathers: sequential stages of one job, not competing methods"},
    ],
)
