"""DIP inverts the policy's dependency onto an abstraction; ISP splits the fat interface into roles."""

from generate_diagram import emit

PANEL = "#fafafa"
ROLE_Y, ROLE_H, ROLE_W = 440, 55, 220
GATE_Y, GATE_H = 580, 55

emit(
    "04-solid-dependency-inversion",
    1000,
    670,
    bands=[
        {"x": 20, "y": 20, "w": 960, "h": 230, "label": "Dependency inversion (DIP)", "fill": PANEL},
        {"x": 20, "y": 280, "w": 960, "h": 370,
         "label": "Interface segregation (ISP): one fat interface split into focused roles", "fill": PANEL},
    ],
    nodes=[
        # DIP panel
        {"x": 60, "y": 60, "w": 230, "h": 70, "style": "accent",
         "lines": ["OrderNotificationPolicy", "(high-level policy)"]},
        {"x": 400, "y": 160, "w": 230, "h": 70, "style": "ink", "lines": ["MessageSender", "(abstraction)"]},
        {"x": 740, "y": 70, "w": 210, "h": 60, "style": "box", "lines": ["EmailGateway", "(concrete)"]},
        {"x": 740, "y": 170, "w": 210, "h": 60, "style": "box", "lines": ["SmsGateway", "(concrete)"]},
        # ISP panel: fat interface on top, roles in the middle, implementations below
        {"x": 400, "y": 320, "w": 220, "h": 60, "style": "ghost",
         "lines": ["NotificationGateway", "(fat interface, avoided)"]},
        {"x": 140, "y": ROLE_Y, "w": ROLE_W, "h": ROLE_H, "style": "ink", "lines": ["MessageSender"]},
        {"x": 400, "y": ROLE_Y, "w": ROLE_W, "h": ROLE_H, "style": "ink", "lines": ["TemplateRenderer"]},
        {"x": 660, "y": ROLE_Y, "w": ROLE_W, "h": ROLE_H, "style": "ink", "lines": ["DeliveryStatusProvider"]},
        {"x": 160, "y": GATE_Y, "w": 180, "h": GATE_H, "style": "box",
         "lines": ["SmsGateway", "(implements one)"]},
        {"x": 405, "y": GATE_Y, "w": 210, "h": GATE_H, "style": "box",
         "lines": ["EmailGateway", "(implements all three)"]},
    ],
    edges=[
        {"x1": 290, "y1": 120, "x2": 400, "y2": 170, "label": "depends on (inverted)", "amber": True,
         "lx": 14, "ly": -28},
        {"x1": 290, "y1": 85, "x2": 740, "y2": 100, "label": "traditional direct coupling (avoided)",
         "dashed": True, "lx": 6, "ly": -10},
        {"x1": 740, "y1": 130, "x2": 630, "y2": 175, "label": "implements", "lx": 8, "ly": 24.5},
        {"x1": 740, "y1": 200, "x2": 630, "y2": 205, "label": "implements", "lx": 0, "ly": 19.5},
        {"x1": 470, "y1": 380, "x2": 250, "y2": ROLE_Y, "label": "segregated into", "dashed": True,
         "lx": -12, "ly": -17},
        {"x1": 510, "y1": 380, "x2": 510, "y2": ROLE_Y, "dashed": True},
        {"x1": 550, "y1": 380, "x2": 770, "y2": ROLE_Y, "dashed": True},
        {"x1": 250, "y1": GATE_Y, "x2": 250, "y2": ROLE_Y + ROLE_H, "label": "implements", "lx": 38, "ly": 4},
        {"x1": 550, "y1": GATE_Y, "x2": 770, "y2": ROLE_Y + ROLE_H, "label": "implements", "lx": 8, "ly": 24.5},
        {"x1": 510, "y1": GATE_Y, "x2": 510, "y2": ROLE_Y + ROLE_H, "label": "implements", "lx": 38, "ly": 4},
        {"x1": 470, "y1": GATE_Y, "x2": 250 + 40, "y2": ROLE_Y + ROLE_H, "label": "implements",
         "lx": -11, "ly": 26.5},
    ],
    notes=[
        {"x": 500, "y": 45, "text": "same abstraction, named by both the policy and its implementations",
         "anchor": "middle", "size": 13, "color": "#64748b"},
    ],
)
