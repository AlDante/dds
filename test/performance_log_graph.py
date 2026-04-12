#!/usr/bin/env python3

from __future__ import annotations

import argparse
import html
import math
import re
from dataclasses import dataclass
from pathlib import Path


ENTRY_RE = re.compile(
    r"^## (?P<timestamp>\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) — commit `(?P<commit>[0-9a-f]+)`(?P<dirty> \(dirty\))?$"
)

WORKLOAD_ROW_RE = re.compile(
    r"^\| `(?P<name>[^`]+)` \| (?P<median>[0-9]+(?:\.[0-9]+)?) \| "
    r"(?P<mean>[0-9]+(?:\.[0-9]+)?) \| (?P<min>[0-9]+(?:\.[0-9]+)?) \| (?P<max>[0-9]+(?:\.[0-9]+)?) \|$"
)


@dataclass(frozen=True)
class PerformanceEntry:
    timestamp: str
    commit: str
    dirty: bool
    workloads: dict[str, float]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def parse_performance_log(log_path: Path) -> list[PerformanceEntry]:
    entries: list[PerformanceEntry] = []
    current_timestamp: str | None = None
    current_commit: str | None = None
    current_dirty = False
    current_workloads: dict[str, float] = {}

    def flush_current() -> None:
        nonlocal current_timestamp, current_commit, current_dirty, current_workloads
        if current_timestamp is None or current_commit is None:
            return
        if current_workloads:
            entries.append(
                PerformanceEntry(
                    timestamp=current_timestamp,
                    commit=current_commit,
                    dirty=current_dirty,
                    workloads=dict(current_workloads),
                )
            )
        current_timestamp = None
        current_commit = None
        current_dirty = False
        current_workloads = {}

    for raw_line in log_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        entry_match = ENTRY_RE.match(line)
        if entry_match:
            flush_current()
            current_timestamp = entry_match.group("timestamp")
            current_commit = entry_match.group("commit")
            current_dirty = entry_match.group("dirty") is not None
            continue

        row_match = WORKLOAD_ROW_RE.match(line)
        if row_match and current_timestamp is not None:
            current_workloads[row_match.group("name")] = float(row_match.group("median"))

    flush_current()
    return entries


def format_seconds_label(value: float) -> str:
    if value >= 100:
        return f"{value:.0f} s"
    if value >= 10:
        return f"{value:.0f} s"
    if value >= 1:
        return f"{value:.1f} s"
    return f"{value:.2f} s"


def svg_text(x: float, y: float, text: str, **attrs: str | float) -> str:
    pairs = [f'x="{x:.2f}"', f'y="{y:.2f}"']
    for key, value in attrs.items():
        pairs.append(f'{key.replace("_", "-")}="{html.escape(str(value), quote=True)}"')
    return f"<text {' '.join(pairs)}>{html.escape(text)}</text>"


def svg_line(x1: float, y1: float, x2: float, y2: float, **attrs: str | float) -> str:
    pairs = [
        f'x1="{x1:.2f}"',
        f'y1="{y1:.2f}"',
        f'x2="{x2:.2f}"',
        f'y2="{y2:.2f}"',
    ]
    for key, value in attrs.items():
        pairs.append(f'{key.replace("_", "-")}="{html.escape(str(value), quote=True)}"')
    return f"<line {' '.join(pairs)} />"


def svg_circle(cx: float, cy: float, r: float, **attrs: str | float) -> str:
    pairs = [f'cx="{cx:.2f}"', f'cy="{cy:.2f}"', f'r="{r:.2f}"']
    for key, value in attrs.items():
        pairs.append(f'{key.replace("_", "-")}="{html.escape(str(value), quote=True)}"')
    return f"<circle {' '.join(pairs)} />"


def svg_polyline(points: list[tuple[float, float]], **attrs: str | float) -> str:
    points_attr = " ".join(f"{x:.2f},{y:.2f}" for x, y in points)
    pairs = [f'points="{points_attr}"']
    for key, value in attrs.items():
        pairs.append(f'{key.replace("_", "-")}="{html.escape(str(value), quote=True)}"')
    return f"<polyline {' '.join(pairs)} />"


def generate_tick_values(min_value: float, max_value: float) -> list[float]:
    tick_values: list[float] = []
    min_exponent = math.floor(math.log10(min_value))
    max_exponent = math.ceil(math.log10(max_value))
    for exponent in range(min_exponent, max_exponent + 1):
        base = 10 ** exponent
        for factor in (1, 2, 5):
            tick = factor * base
            if min_value <= tick <= max_value:
                tick_values.append(float(tick))
    if not tick_values:
        tick_values = [min_value, max_value]
    if tick_values[0] > min_value:
        tick_values.insert(0, min_value)
    if tick_values[-1] < max_value:
        tick_values.append(max_value)
    return sorted(dict.fromkeys(round(value, 12) for value in tick_values))


def render_performance_log_graph(log_path: Path, output_path: Path) -> dict[str, int]:
    entries = parse_performance_log(log_path)
    if not entries:
        raise ValueError(f"No performance entries found in {log_path}")

    workload_names = sorted({name for entry in entries for name in entry.workloads})
    if not workload_names:
        raise ValueError(f"No workload rows found in {log_path}")

    width = 1320
    height = 860
    left_margin = 110.0
    right_margin = 280.0
    top_margin = 120.0
    bottom_margin = 190.0
    plot_width = width - left_margin - right_margin
    plot_height = height - top_margin - bottom_margin

    all_values = [value for entry in entries for value in entry.workloads.values() if value > 0.0]
    min_value = min(all_values)
    max_value = max(all_values)
    y_min = 10 ** math.floor(math.log10(min_value / 1.15))
    y_max = 10 ** math.ceil(math.log10(max_value * 1.15))
    ticks = generate_tick_values(y_min, y_max)

    def x_position(index: int) -> float:
        if len(entries) == 1:
            return left_margin + plot_width / 2.0
        return left_margin + (plot_width * index) / (len(entries) - 1)

    def y_position(value: float) -> float:
        ratio = (math.log10(value) - math.log10(y_min)) / (math.log10(y_max) - math.log10(y_min))
        return top_margin + plot_height - ratio * plot_height

    colors = [
        "#4C78A8",
        "#F58518",
        "#54A24B",
        "#E45756",
        "#72B7B2",
        "#B279A2",
        "#FF9DA6",
        "#9D755D",
        "#BAB0AC",
    ]
    color_by_workload = {name: colors[index % len(colors)] for index, name in enumerate(workload_names)}

    svg_parts: list[str] = []
    svg_parts.append('<?xml version="1.0" encoding="UTF-8"?>')
    svg_parts.append(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-labelledby="title desc">'
    )
    svg_parts.append("<title id=\"title\">DDS standardized performance trend</title>")
    svg_parts.append(
        "<desc id=\"desc\">Median elapsed seconds per workload across recorded standardized performance runs on a logarithmic y-axis.</desc>"
    )
    svg_parts.append(f'<rect x="0" y="0" width="{width}" height="{height}" fill="#ffffff" />')

    svg_parts.append(svg_text(left_margin, 48, "DDS standardized performance trend", font_size="28", font_weight="700", fill="#111827"))
    svg_parts.append(
        svg_text(
            left_margin,
            78,
            "Median elapsed seconds per workload entry from docs/performance-log.md (logarithmic y-axis).",
            font_size="15",
            fill="#4b5563",
        )
    )

    plot_left = left_margin
    plot_right = left_margin + plot_width
    plot_top = top_margin
    plot_bottom = top_margin + plot_height

    svg_parts.append(
        f'<rect x="{plot_left:.2f}" y="{plot_top:.2f}" width="{plot_width:.2f}" height="{plot_height:.2f}" fill="#fcfcfd" stroke="#d1d5db" stroke-width="1" />'
    )

    for tick in ticks:
        y = y_position(tick)
        svg_parts.append(svg_line(plot_left, y, plot_right, y, stroke="#e5e7eb", stroke_width="1"))
        svg_parts.append(
            svg_text(
                plot_left - 14,
                y + 5,
                format_seconds_label(tick),
                text_anchor="end",
                font_size="13",
                fill="#374151",
            )
        )

    svg_parts.append(svg_line(plot_left, plot_top, plot_left, plot_bottom, stroke="#6b7280", stroke_width="1.5"))
    svg_parts.append(svg_line(plot_left, plot_bottom, plot_right, plot_bottom, stroke="#6b7280", stroke_width="1.5"))
    svg_parts.append(
        svg_text(
            34,
            plot_top + plot_height / 2.0,
            "Median runtime (seconds, log scale)",
            transform=f"rotate(-90 34 {plot_top + plot_height / 2.0:.2f})",
            font_size="14",
            fill="#374151",
        )
    )

    for index, entry in enumerate(entries):
        x = x_position(index)
        svg_parts.append(svg_line(x, plot_top, x, plot_bottom, stroke="#f3f4f6", stroke_width="1"))
        label = f"{entry.commit} {entry.timestamp[11:16]}"
        svg_parts.append(
            f'<text x="{x:.2f}" y="{plot_bottom + 26:.2f}" text-anchor="end" font-size="12" fill="#374151" transform="rotate(-35 {x:.2f} {plot_bottom + 26:.2f})">{html.escape(label)}</text>'
        )
    svg_parts.append(
        svg_text(
            plot_left + plot_width / 2.0,
            height - 36,
            "Commit/time of recorded standardized run",
            text_anchor="middle",
            font_size="14",
            fill="#374151",
        )
    )

    for workload_name in workload_names:
        points: list[tuple[float, float]] = []
        for index, entry in enumerate(entries):
            value = entry.workloads.get(workload_name)
            if value is None or value <= 0.0:
                continue
            points.append((x_position(index), y_position(value)))
        if not points:
            continue
        color = color_by_workload[workload_name]
        svg_parts.append(
            svg_polyline(
                points,
                fill="none",
                stroke=color,
                stroke_width="3",
                stroke_linecap="round",
                stroke_linejoin="round",
            )
        )
        for point_x, point_y in points:
            svg_parts.append(svg_circle(point_x, point_y, 4.5, fill=color, stroke="#ffffff", stroke_width="1.5"))

    legend_x = plot_right + 28.0
    legend_y = plot_top + 20.0
    svg_parts.append(svg_text(legend_x, legend_y, "Workloads", font_size="16", font_weight="700", fill="#111827"))
    for legend_index, workload_name in enumerate(workload_names, start=1):
        row_y = legend_y + 26.0 * legend_index
        color = color_by_workload[workload_name]
        svg_parts.append(svg_line(legend_x, row_y - 5, legend_x + 28, row_y - 5, stroke=color, stroke_width="4", stroke_linecap="round"))
        svg_parts.append(svg_circle(legend_x + 14, row_y - 5, 4.5, fill=color, stroke="#ffffff", stroke_width="1.5"))
        svg_parts.append(svg_text(legend_x + 40, row_y, workload_name, font_size="13", fill="#111827"))

    latest_entry = entries[-1]
    summary_y = legend_y + 26.0 * (len(workload_names) + 2)
    svg_parts.append(svg_text(legend_x, summary_y, "Latest entry", font_size="16", font_weight="700", fill="#111827"))
    svg_parts.append(
        svg_text(
            legend_x,
            summary_y + 24,
            f"{latest_entry.timestamp} / {latest_entry.commit}{' dirty' if latest_entry.dirty else ''}",
            font_size="13",
            fill="#374151",
        )
    )

    svg_parts.append("</svg>")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(svg_parts) + "\n", encoding="utf-8")
    return {"entry_count": len(entries), "workload_count": len(workload_names)}


def main() -> int:
    parser = argparse.ArgumentParser(description="Render an SVG trend graph from docs/performance-log.md.")
    parser.add_argument(
        "--log-path",
        default="",
        help="Optional path to the markdown log. Defaults to docs/performance-log.md under the repository root.",
    )
    parser.add_argument(
        "--output-path",
        default="",
        help="Optional path to the SVG output. Defaults to docs/performance-log.svg under the repository root.",
    )
    args = parser.parse_args()

    root = repo_root()
    log_path = Path(args.log_path).resolve() if args.log_path else root / "docs" / "performance-log.md"
    output_path = Path(args.output_path).resolve() if args.output_path else root / "docs" / "performance-log.svg"
    result = render_performance_log_graph(log_path, output_path)
    print(
        f"Wrote {output_path} from {result['entry_count']} performance entries across {result['workload_count']} workloads."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
