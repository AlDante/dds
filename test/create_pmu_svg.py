#!/usr/bin/env python3
"""Generate the PMU ladder SVG chart."""
import os

svg = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 520" font-family="system-ui, sans-serif" font-size="11">
<text x="500" y="20" text-anchor="middle" font-size="14" font-weight="bold">PMU Ladder v2: Cycles % Delta vs Baseline (170e566)</text>
<text x="500" y="36" text-anchor="middle" font-size="10" fill="#666">Single board, serial, list9 board 1, depth 2. Scale: 18px per 1%.</text>
<line x1="80" y1="380" x2="940" y2="380" stroke="#333" stroke-width="1"/>
<text x="75" y="384" text-anchor="end" font-size="9" fill="#555" font-weight="bold">0%</text>
<line x1="80" y1="200" x2="940" y2="200" stroke="#e0e0e0" stroke-width="0.5"/>
<text x="75" y="204" text-anchor="end" font-size="9" fill="#555">+10%</text>
<line x1="80" y1="290" x2="940" y2="290" stroke="#e0e0e0" stroke-width="0.5"/>
<text x="75" y="294" text-anchor="end" font-size="9" fill="#555">+5%</text>
<line x1="80" y1="110" x2="940" y2="110" stroke="#e0e0e0" stroke-width="0.5"/>
<text x="75" y="114" text-anchor="end" font-size="9" fill="#555">+15%</text>
<line x1="350" y1="50" x2="350" y2="450" stroke="#ccc" stroke-width="1" stroke-dasharray="4,4"/>
<text x="210" y="65" text-anchor="middle" font-size="9" fill="#999" font-style="italic">Independent</text>
<text x="640" y="65" text-anchor="middle" font-size="9" fill="#999" font-style="italic">Cumulative commits</text>
"""

# Data: (label, cycles_delta_pct, x_center)
bars = [
    ("8.1\\nHot/cold", 2.2, 130),
    ("8.5\\npos", 0.9, 220),
    ("8.4\\nmoveType", 1.2, 310),
    ("9b7d54a\\nAll+DL", 10.6, 420),
    ("2b4b27c\\nRev DL", 7.9, 510),
    ("96d02a3\\nNEON", 6.9, 600),
    ("4f218e6\\nCLZ", 12.7, 690),
    ("bcdd518\\nModern", 12.7, 780),
    ("fa2280e\\nHEAD", 13.2, 870),
]

for label, pct, cx in bars:
    h = pct * 18  # 18px per percent
    y = 380 - h
    color = "#2196F3" if pct < 5 else "#F44336" if pct > 10 else "#FF9800"
    svg += f'<rect x="{cx-20}" y="{y:.0f}" width="40" height="{h:.0f}" fill="{color}" opacity="0.85"/>\n'
    svg += f'<text x="{cx}" y="{380 + 14}" text-anchor="middle" font-size="8">{label.split(chr(92)+"n")[0]}</text>\n'
    if "\\n" in label:
        svg += f'<text x="{cx}" y="{380 + 23}" text-anchor="middle" font-size="8">{label.split(chr(92)+"n")[1]}</text>\n'
    svg += f'<text x="{cx}" y="{y - 4:.0f}" text-anchor="middle" font-size="8" fill="#333">+{pct}%</text>\n'

# Annotations
svg += '<text x="555" y="95" text-anchor="middle" font-size="9" fill="#d32f2f">DepthLocal + struct interaction</text>\n'
svg += '<text x="780" y="80" text-anchor="middle" font-size="9" fill="#d32f2f">CLZ intrinsic adds +5.8%</text>\n'

# Legend
svg += '<text x="500" y="480" text-anchor="middle" font-size="9" fill="#666">Blue: within noise. Orange: moderate regression. Red: significant regression (&gt;10%).</text>\n'
svg += '<text x="500" y="495" text-anchor="middle" font-size="9" fill="#666">Root cause: increased instruction count, not cache pressure (IPC flat at 3.36-3.43).</text>\n'

svg += "</svg>\n"

path = os.path.join(os.path.dirname(__file__), "..", "docs", "pmu-ladder.svg")
# Remove if exists
if os.path.exists(path):
    os.remove(path)
with open(path, "w") as f:
    f.write(svg)
print(f"Created {path}")

