#!/usr/bin/env python3
"""Generate matching vector SVG and printable Letter-landscape PDF."""

from html import escape
from pathlib import Path

from reportlab.lib.pagesizes import landscape, letter
from reportlab.pdfgen import canvas

ROOT = Path(__file__).resolve().parents[1]
W, H = landscape(letter)
c = canvas.Canvas(str(ROOT / "docs/wiring.pdf"), pagesize=(W, H))
c.setTitle("Panasonic FV-16VEC1S ESP32 active UART interface")
c.setAuthor("rvdbijl / panasonic-erv")
svg = [
    f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}pt" height="{H}pt" viewBox="0 0 {W} {H}">',
    '<rect width="100%" height="100%" fill="white"/>',
]


# Drawing coordinates use a top-left origin shared with SVG. ReportLab uses
# a bottom-left origin, so each primitive flips Y when writing the PDF. Emit
# both formats here so circuit edits cannot silently diverge between drawings.
def text(x, y, s, size=10, bold=False):
    c.setFont("Helvetica-Bold" if bold else "Helvetica", size)
    c.drawString(x, H - y, s)
    svg.append(
        f'<text x="{x}" y="{y}" font-family="Arial, sans-serif" font-size="{size}" font-weight="{700 if bold else 400}">{escape(s)}</text>'
    )


def line(x1, y1, x2, y2):
    c.setLineWidth(1.2)
    c.line(x1, H - y1, x2, H - y2)
    svg.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="black" stroke-width="1.2"/>')


def rect(x, y, w, h):
    c.rect(x, H - y - h, w, h)
    svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="none" stroke="black"/>')


# Dots explicitly identify electrical junctions; line crossings alone do not.
def dot(x, y):
    c.circle(x, H - y, 2.8, fill=1, stroke=0)
    svg.append(f'<circle cx="{x}" cy="{y}" r="2.8" fill="black"/>')


def resistor(x1, y1, x2, y2):
    if y1 == y2:
        mid = (x1 + x2) / 2
        line(x1, y1, mid - 20, y1)
        rect(mid - 20, y1 - 6, 40, 12)
        line(mid + 20, y1, x2, y2)
    else:
        mid = (y1 + y2) / 2
        line(x1, y1, x1, mid - 20)
        rect(x1 - 6, mid - 20, 12, 40)
        line(x1, mid + 20, x2, y2)


text(36, 35, "Panasonic FV-16VEC1S / ESP32", 21, True)
text(
    36,
    57,
    "Active UART wiring - example GPIO16 RX / GPIO17 TX; pins configurable in ESPHome",
    11,
)
text(
    36,
    80,
    "ONLY for the verified 4.16-4.48 V ERV TX line. Not suitable for the ~6.7 V controller TX.",
    10,
    True,
)
rect(36, 115, 120, 328)
rect(610, 115, 146, 328)
text(49, 139, "PANASONIC", 12, True)
text(49, 157, "ERV signals", 11)
text(627, 139, "ESP32-S3", 12, True)
text(627, 157, "GPIO examples", 11)
for y, label in [(200, "TX"), (305, "GND"), (370, "RX"), (420, "12 V")]:
    text(99, y + 4, label, 11, True)
for y, label in [(200, "GPIO16 / RX"), (305, "GND"), (370, "GPIO17 / TX")]:
    text(624, y + 4, label, 11, True)
# Receive path: ERV TX -> R1 -> RX node, with R2 from that node to ground.
# Signal labels indicate device-relative direction, not connector pin numbers.
line(156, 200, 205, 200)
resistor(205, 200, 305, 200)
line(305, 200, 610, 200)
text(232, 179, "R1 2.2k", 11, True)
dot(360, 200)
text(414, 181, "TP1: protected RX node", 10)
resistor(360, 200, 360, 305)
text(379, 251, "R2 4.7k", 11, True)
line(156, 305, 610, 305)
dot(360, 305)
text(433, 290, "COMMON GROUND", 10, True)
# Separate transmit path: ESP TX -> R3 -> optional JP1 -> ERV RX.
# It must never share a driven connection with the OEM controller's TX.
line(156, 370, 217, 370)
rect(217, 361, 34, 18)
text(219, 348, "JP1", 10, True)
line(217, 370, 251, 370)
line(251, 370, 428, 370)
resistor(428, 370, 548, 370)
line(548, 370, 610, 370)
text(455, 348, "R3 1.0k", 11, True)
text(198, 398, "Optional removable TX link", 10)
text(418, 398, "Data direction: ESP to ERV", 10)
line(156, 420, 177, 420)
line(177, 416, 185, 424)
line(177, 424, 185, 416)
text(196, 424, "NO CONNECTION to ESP", 10, True)
line(681, 115, 681, 99)
text(611, 94, "USB power adapter", 10, True)
text(
    36,
    469,
    "OEM controller TX must be disconnected from ERV RX before connecting the ESP transmit path.",
    10,
    True,
)
text(
    36,
    489,
    "R1/R2 divider + nominal 45k internal pull-down: about 2.74-2.95 V at TP1 over the measured range.",
    10,
)
text(
    36,
    507,
    "Use rx_pull_down: true for this circuit. Without internal pull: about 2.83-3.05 V. Measure your unit.",
    10,
)
text(
    36,
    525,
    "R1/R2/R3: 1%, 1/4 W. Dots join wires. TX/RX labels are from the ERV perspective.",
    10,
)
text(
    36,
    543,
    "3.3 V ESP transmit worked on the tested unit. R3 is not a level shifter. Never connect ERV 12 V to ESP.",
    10,
)
text(
    36,
    561,
    "4800 baud / 8 data / EVEN parity / 1 stop / RX and TX inverted. Verify voltage and direction first.",
    10,
)
line(36, 578, 756, 578)
text(
    36,
    594,
    "Rev B - 2026-09-27 | github.com/rvdbijl/panasonic-erv | See docs/WIRING.md for limits and checks.",
    9,
)
c.showPage()
c.save()
svg.append("</svg>")
(ROOT / "docs/wiring.svg").write_text("\n".join(svg) + "\n")
print("Generated docs/wiring.pdf and docs/wiring.svg")
