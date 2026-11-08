"""The clamp that adjustment_for() owns bounds every subtype's discount rate."""

from generate_diagram import emit

ROW1, ROW2, H = 80, 230, 70

emit(
    "03-lsp-clamp",
    1040,
    370,
    bands=[
        {"x": 20, "y": 30, "w": 1000, "h": 295,
         "label": "PercentageDiscountRule::adjustment_for(): final, owns the clamp", "fill": "#fafafa"},
    ],
    nodes=[
        {"x": 45, "y": ROW1, "w": 140, "h": H, "style": "box", "lines": ["subtotal in", "Cents value"]},
        {"x": 350, "y": ROW1, "w": 210, "h": H, "style": "accent",
         "lines": ["discount_rate() hook", "overridable; may return > 1.0"]},
        {"x": 700, "y": ROW1, "w": 280, "h": H, "style": "ink",
         "lines": ["clamp to [0, 1]", "owned by base; every subtype inherits it"]},
        {"x": 740, "y": ROW2, "w": 200, "h": H, "style": "box", "lines": ["apply to subtotal", "negate and round"]},
        {"x": 400, "y": ROW2, "w": 220, "h": H, "style": "ink",
         "lines": ["bounded adjustment out", "never exceeds the subtotal"]},
    ],
    edges=[
        {"x1": 185, "y1": 115, "x2": 350, "y2": 115, "label": "discount_rate(subtotal)", "lx": 0, "ly": -8},
        {"x1": 560, "y1": 115, "x2": 700, "y2": 115, "label": "raw rate, unclamped", "amber": True,
         "lx": 0, "ly": -8},
        {"x1": 840, "y1": ROW1 + H, "x2": 840, "y2": ROW2, "label": "clamped rate", "lx": 44, "ly": 4},
        {"x1": 740, "y1": 265, "x2": 620, "y2": 265, "label": "adjustment", "lx": 0, "ly": -8},
    ],
    notes=[
        {"x": 520, "y": 352, "anchor": "middle", "bold": True, "color": "#888888",
         "text": "LoyaltyDiscountRule overrides only discount_rate(); it never touches the clamp."},
    ],
)
