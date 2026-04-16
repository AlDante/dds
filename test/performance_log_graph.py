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

LEGACY_WORKLOAD_ROW_RE = re.compile(
    r"^\| `(?P<name>[^`]+)` \| (?P<median>[0-9]+(?:\.[0-9]+)?) \| "
    r"(?P<mean>[0-9]+(?:\.[0-9]+)?) \| (?P<min>[0-9]+(?:\.[0-9]+)?) \| (?P<max>[0-9]+(?:\.[0-9]+)?) \|$"
)
BENCHMARK_ROW_RE = re.compile(
    r"^\| `(?P<name>[^`]+)` \| (?P<boards>\d+) \| (?P<total>[0-9]+(?:\.[0-9]+)?) \| "
    r"(?P<per_board>[0-9]+(?:\.[0-9]+)?) \|$"
)
PER_BOARD_TIMINGS_RE = re.compile(
    r"^- Per-board timings for `(?P<name>[^`]+)` \(s\): `(?P<values>[0-9., ]+)`$"
)
GRAPH_OUTLIERS_RE = re.compile(r"^- Graph outliers: (?P<workloads>.+)$")
BACKTICK_NAME_RE = re.compile(r"`([^`]+)`")


@dataclass(frozen=True)
class PerformanceEntry:
    timestamp: str
    commit: str
    dirty: bool
    total_workloads: dict[str, float]
    per_board_workloads: dict[str, float]
    per_board_samples: dict[str, tuple[float, ...]]
    graph_outliers: frozenset[str]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def parse_performance_log(log_path: Path) -> list[PerformanceEntry]:
    entries: list[PerformanceEntry] = []
    current_timestamp: str | None = None
    current_commit: str | None = None
    current_dirty = False
    current_total_workloads: dict[str, float] = {}
    current_per_board_workloads: dict[str, float] = {}
    current_per_board_samples: dict[str, tuple[float, ...]] = {}
    current_graph_outliers: set[str] = set()

    def flush_current() -> None:
        nonlocal current_timestamp, current_commit, current_dirty
        nonlocal current_total_workloads, current_per_board_workloads, current_per_board_samples, current_graph_outliers
        if current_timestamp is None or current_commit is None:
            return
        if current_total_workloads or current_per_board_workloads or current_per_board_samples:
            entries.append(
                PerformanceEntry(
                    timestamp=current_timestamp,
                    commit=current_commit,
                    dirty=current_dirty,
                    total_workloads=dict(current_total_workloads),
                    per_board_workloads=dict(current_per_board_workloads),
                    per_board_samples=dict(current_per_board_samples),
                    graph_outliers=frozenset(current_graph_outliers),
                )
            )
        current_timestamp = None
        current_commit = None
        current_dirty = False
        current_total_workloads = {}
        current_per_board_workloads = {}
        current_per_board_samples = {}
        current_graph_outliers = set()

    for raw_line in log_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        entry_match = ENTRY_RE.match(line)
        if entry_match:
            flush_current()
            current_timestamp = entry_match.group("timestamp")
            current_commit = entry_match.group("commit")
            current_dirty = entry_match.group("dirty") is not None
            continue

        row_match = LEGACY_WORKLOAD_ROW_RE.match(line)
        if row_match and current_timestamp is not None:
            current_total_workloads[row_match.group("name")] = float(row_match.group("median"))
            continue

        benchmark_match = BENCHMARK_ROW_RE.match(line)
        if benchmark_match and current_timestamp is not None:
            name = benchmark_match.group("name")
            current_total_workloads[name] = float(benchmark_match.group("total"))
            current_per_board_workloads[name] = float(benchmark_match.group("per_board"))
            continue

        per_board_match = PER_BOARD_TIMINGS_RE.match(line)
        if per_board_match and current_timestamp is not None:
            values = tuple(
                float(part.strip())
                for part in per_board_match.group("values").split(",")
                if part.strip()
            )
            current_per_board_samples[per_board_match.group("name")] = values
            continue

        outlier_match = GRAPH_OUTLIERS_RE.match(line)
        if outlier_match and current_timestamp is not None:
            current_graph_outliers.update(BACKTICK_NAME_RE.findall(outlier_match.group("workloads")))

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


def median_value(values: list[float]) -> float:
    ordered = sorted(values)
    count = len(ordered)
    if count == 0:
        raise ValueError("median_value requires at least one value")
    middle = count // 2
    if count % 2 == 1:
        return ordered[middle]
    return (ordered[middle - 1] + ordered[middle]) / 2.0


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

    total_workload_names = sorted({name for entry in entries for name in entry.total_workloads})
    per_board_workload_names = sorted(
        {name for entry in entries for name in entry.per_board_workloads} |
        {name for entry in entries for name in entry.per_board_samples}
    )
    workload_names = sorted(set(total_workload_names) | set(per_board_workload_names))
    if not total_workload_names:
        raise ValueError(f"No workload rows found in {log_path}")

    width = 1320
    height = 1100
    left_margin = 110.0
    right_margin = 280.0
    top_margin = 120.0
    bottom_margin = 190.0
    plot_width = width - left_margin - right_margin
    panel_gap = 110.0
    panel_height = (height - top_margin - bottom_margin - panel_gap) / 2.0

    def x_position(index: int) -> float:
        if len(entries) == 1:
            return left_margin + plot_width / 2.0
        return left_margin + (plot_width * index) / (len(entries) - 1)

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
        "<desc id=\"desc\">Recorded performance entries showing cumulative runtime and per-board runtime across standardized runs and standalone alpha-mu baselines.</desc>"
    )
    svg_parts.append(f'<rect x="0" y="0" width="{width}" height="{height}" fill="#ffffff" />')

    svg_parts.append(svg_text(left_margin, 48, "DDS standardized performance trend", font_size="28", font_weight="700", fill="#111827"))
    svg_parts.append(
        svg_text(
            left_margin,
            78,
            "Top panel: cumulative runtime. Bottom panel: per-board runtime for entries that record board counts.",
            font_size="15",
            fill="#4b5563",
        )
    )
    svg_parts.append(
        svg_text(
            left_margin,
            100,
            "Hollow X markers denote flagged historical outliers/non-comparable measurements and are excluded from the trend line for that workload.",
            font_size="13",
            fill="#6b7280",
        )
    )

    def render_panel(
        title: str,
        series_getter,
        plot_top: float,
        y_axis_label: str,
        empty_message: str,
        show_x_labels: bool,
    ) -> None:
        plot_left = left_margin
        plot_right = left_margin + plot_width
        plot_bottom = plot_top + panel_height
        values = [
            value
            for entry in entries
            for value in series_getter(entry).values()
            if value > 0.0
        ]

        svg_parts.append(
            f'<rect x="{plot_left:.2f}" y="{plot_top:.2f}" width="{plot_width:.2f}" height="{panel_height:.2f}" fill="#fcfcfd" stroke="#d1d5db" stroke-width="1" />'
        )
        svg_parts.append(
            svg_text(plot_left + 12, plot_top + 24, title, font_size="16", font_weight="700", fill="#111827")
        )

        if not values:
            svg_parts.append(
                svg_text(
                    plot_left + plot_width / 2.0,
                    plot_top + panel_height / 2.0,
                    empty_message,
                    text_anchor="middle",
                    font_size="14",
                    fill="#6b7280",
                )
            )
            return

        y_min = 10 ** math.floor(math.log10(min(values) / 1.15))
        y_max = 10 ** math.ceil(math.log10(max(values) * 1.15))
        ticks = generate_tick_values(y_min, y_max)

        def y_position(value: float) -> float:
            ratio = (math.log10(value) - math.log10(y_min)) / (math.log10(y_max) - math.log10(y_min))
            return plot_top + panel_height - ratio * panel_height

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
                plot_top + panel_height / 2.0,
                y_axis_label,
                transform=f"rotate(-90 34 {plot_top + panel_height / 2.0:.2f})",
                font_size="14",
                fill="#374151",
            )
        )

        for index, entry in enumerate(entries):
            x = x_position(index)
            svg_parts.append(svg_line(x, plot_top, x, plot_bottom, stroke="#f3f4f6", stroke_width="1"))
            if show_x_labels:
                label = f"{entry.commit} {entry.timestamp[11:16]}"
                svg_parts.append(
                    f'<text x="{x:.2f}" y="{plot_bottom + 26:.2f}" text-anchor="end" font-size="12" fill="#374151" transform="rotate(-35 {x:.2f} {plot_bottom + 26:.2f})">{html.escape(label)}</text>'
                )

        for workload_name in workload_names:
            points: list[tuple[float, float]] = []
            outlier_points: list[tuple[float, float]] = []
            for index, entry in enumerate(entries):
                value = series_getter(entry).get(workload_name)
                if value is None or value <= 0.0:
                    continue
                point = (x_position(index), y_position(value))
                if workload_name in entry.graph_outliers:
                    outlier_points.append(point)
                else:
                    points.append(point)
            if not points and not outlier_points:
                continue
            color = color_by_workload[workload_name]
            if len(points) >= 2:
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
            for point_x, point_y in outlier_points:
                svg_parts.append(svg_circle(point_x, point_y, 6.0, fill="#ffffff", stroke=color, stroke_width="2.5"))
                svg_parts.append(svg_line(point_x - 4.0, point_y - 4.0, point_x + 4.0, point_y + 4.0, stroke=color, stroke_width="2.0", stroke_linecap="round"))
                svg_parts.append(svg_line(point_x - 4.0, point_y + 4.0, point_x + 4.0, point_y - 4.0, stroke=color, stroke_width="2.0", stroke_linecap="round"))

    def per_board_samples_for_entry(entry: PerformanceEntry) -> dict[str, tuple[float, ...]]:
        samples: dict[str, tuple[float, ...]] = {}
        names = set(entry.per_board_workloads) | set(entry.per_board_samples)
        for name in names:
            if name in entry.per_board_samples and entry.per_board_samples[name]:
                samples[name] = entry.per_board_samples[name]
            else:
                scalar = entry.per_board_workloads.get(name)
                if scalar is not None and scalar > 0.0:
                    samples[name] = (scalar,)
        return samples

    def render_per_board_panel(plot_top: float) -> None:
        plot_left = left_margin
        plot_right = left_margin + plot_width
        plot_bottom = plot_top + panel_height
        all_samples = [
            sample
            for entry in entries
            for values in per_board_samples_for_entry(entry).values()
            for sample in values
            if sample > 0.0
        ]

        svg_parts.append(
            f'<rect x="{plot_left:.2f}" y="{plot_top:.2f}" width="{plot_width:.2f}" height="{panel_height:.2f}" fill="#fcfcfd" stroke="#d1d5db" stroke-width="1" />'
        )
        svg_parts.append(
            svg_text(
                plot_left + 12,
                plot_top + 24,
                "Per-board runtime for board-counted benchmarks",
                font_size="16",
                font_weight="700",
                fill="#111827",
            )
        )

        if not all_samples:
            svg_parts.append(
                svg_text(
                    plot_left + plot_width / 2.0,
                    plot_top + panel_height / 2.0,
                    "No board-counted benchmark entries recorded yet.",
                    text_anchor="middle",
                    font_size="14",
                    fill="#6b7280",
                )
            )
            return

        y_min = 10 ** math.floor(math.log10(min(all_samples) / 1.15))
        y_max = 10 ** math.ceil(math.log10(max(all_samples) * 1.15))
        ticks = generate_tick_values(y_min, y_max)

        def y_position(value: float) -> float:
            ratio = (math.log10(value) - math.log10(y_min)) / (math.log10(y_max) - math.log10(y_min))
            return plot_top + panel_height - ratio * panel_height

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
                plot_top + panel_height / 2.0,
                "Per-board time (seconds, log scale)",
                transform=f"rotate(-90 34 {plot_top + panel_height / 2.0:.2f})",
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

        for workload_name in workload_names:
            color = color_by_workload[workload_name]
            median_points: list[tuple[float, float]] = []
            for index, entry in enumerate(entries):
                samples = per_board_samples_for_entry(entry).get(workload_name)
                if samples is None:
                    continue
                x = x_position(index)
                sample_list = [sample for sample in samples if sample > 0.0]
                if not sample_list:
                    continue

                is_outlier = workload_name in entry.graph_outliers
                min_sample = min(sample_list)
                max_sample = max(sample_list)
                median_sample = median_value(sample_list)
                whisker_y1 = y_position(min_sample)
                whisker_y2 = y_position(max_sample)
                median_y = y_position(median_sample)

                if len(sample_list) > 1:
                    svg_parts.append(
                        svg_line(
                            x,
                            whisker_y1,
                            x,
                            whisker_y2,
                            stroke=color,
                            stroke_width="1.5",
                            stroke_opacity="0.45",
                            stroke_dasharray=("4 3" if is_outlier else ""),
                        )
                    )
                    svg_parts.append(svg_line(x - 5.0, whisker_y1, x + 5.0, whisker_y1, stroke=color, stroke_width="1.5", stroke_opacity="0.45"))
                    svg_parts.append(svg_line(x - 5.0, whisker_y2, x + 5.0, whisker_y2, stroke=color, stroke_width="1.5", stroke_opacity="0.45"))

                count = len(sample_list)
                for sample_index, sample in enumerate(sample_list):
                    offset = 0.0
                    if count > 1:
                        offset = (sample_index - (count - 1) / 2.0) * min(10.0 / max(count - 1, 1), 1.8)
                    svg_parts.append(
                        svg_circle(
                            x + offset,
                            y_position(sample),
                            2.2,
                            fill=color,
                            fill_opacity=("0.28" if not is_outlier else "0.0"),
                            stroke=color,
                            stroke_opacity="0.45",
                            stroke_width="0.8",
                        )
                    )

                if is_outlier:
                    svg_parts.append(svg_circle(x, median_y, 6.0, fill="#ffffff", stroke=color, stroke_width="2.5"))
                    svg_parts.append(svg_line(x - 4.0, median_y - 4.0, x + 4.0, median_y + 4.0, stroke=color, stroke_width="2.0", stroke_linecap="round"))
                    svg_parts.append(svg_line(x - 4.0, median_y + 4.0, x + 4.0, median_y - 4.0, stroke=color, stroke_width="2.0", stroke_linecap="round"))
                else:
                    median_points.append((x, median_y))
                    svg_parts.append(svg_circle(x, median_y, 4.5, fill=color, stroke="#ffffff", stroke_width="1.5"))

            if len(median_points) >= 2:
                svg_parts.append(
                    svg_polyline(
                        median_points,
                        fill="none",
                        stroke=color,
                        stroke_width="3",
                        stroke_linecap="round",
                        stroke_linejoin="round",
                    )
                )

    render_panel(
        "Cumulative runtime by workload",
        lambda entry: entry.total_workloads,
        top_margin,
        "Cumulative time (seconds, log scale)",
        "No cumulative runtime data recorded yet.",
        False,
    )

    render_per_board_panel(top_margin + panel_height + panel_gap)

    plot_bottom = top_margin + 2.0 * panel_height + panel_gap
    plot_right = left_margin + plot_width
    svg_parts.append(
        svg_text(
            left_margin + plot_width / 2.0,
            height - 36,
            "Commit/time of recorded performance entry",
            text_anchor="middle",
            font_size="14",
            fill="#374151",
        )
    )

    legend_x = plot_right + 28.0
    legend_y = top_margin + 20.0
    svg_parts.append(svg_text(legend_x, legend_y, "Workloads", font_size="16", font_weight="700", fill="#111827"))
    for legend_index, workload_name in enumerate(workload_names, start=1):
        row_y = legend_y + 26.0 * legend_index
        color = color_by_workload[workload_name]
        svg_parts.append(svg_line(legend_x, row_y - 5, legend_x + 28, row_y - 5, stroke=color, stroke_width="4", stroke_linecap="round"))
        svg_parts.append(svg_circle(legend_x + 14, row_y - 5, 4.5, fill=color, stroke="#ffffff", stroke_width="1.5"))
        svg_parts.append(svg_text(legend_x + 40, row_y, workload_name, font_size="13", fill="#111827"))

    legend_marker_y = legend_y + 26.0 * (len(workload_names) + 1)
    svg_parts.append(svg_text(legend_x, legend_marker_y, "Graph markers", font_size="16", font_weight="700", fill="#111827"))
    marker_row_y = legend_marker_y + 26.0
    svg_parts.append(svg_circle(legend_x + 14, marker_row_y - 5, 6.0, fill="#ffffff", stroke="#6b7280", stroke_width="2.0"))
    svg_parts.append(svg_line(legend_x + 10, marker_row_y - 9, legend_x + 18, marker_row_y - 1, stroke="#6b7280", stroke_width="1.8", stroke_linecap="round"))
    svg_parts.append(svg_line(legend_x + 10, marker_row_y - 1, legend_x + 18, marker_row_y - 9, stroke="#6b7280", stroke_width="1.8", stroke_linecap="round"))
    svg_parts.append(svg_text(legend_x + 40, marker_row_y, "Flagged outlier / non-comparable measurement", font_size="13", fill="#111827"))
    marker_row_y += 24.0
    svg_parts.append(svg_circle(legend_x + 14, marker_row_y - 5, 4.5, fill="#6b7280", stroke="#ffffff", stroke_width="1.5"))
    svg_parts.append(svg_text(legend_x + 40, marker_row_y, "Median per-board runtime", font_size="13", fill="#111827"))
    marker_row_y += 24.0
    svg_parts.append(svg_line(legend_x + 14, marker_row_y - 13, legend_x + 14, marker_row_y + 3, stroke="#6b7280", stroke_width="1.5", stroke_opacity="0.45"))
    svg_parts.append(svg_line(legend_x + 9, marker_row_y - 13, legend_x + 19, marker_row_y - 13, stroke="#6b7280", stroke_width="1.5", stroke_opacity="0.45"))
    svg_parts.append(svg_line(legend_x + 9, marker_row_y + 3, legend_x + 19, marker_row_y + 3, stroke="#6b7280", stroke_width="1.5", stroke_opacity="0.45"))
    svg_parts.append(svg_text(legend_x + 40, marker_row_y, "Min/max range from recorded board times", font_size="13", fill="#111827"))
    marker_row_y += 24.0
    svg_parts.append(svg_circle(legend_x + 11, marker_row_y - 5, 2.2, fill="#6b7280", fill_opacity="0.28", stroke="#6b7280", stroke_opacity="0.45", stroke_width="0.8"))
    svg_parts.append(svg_circle(legend_x + 17, marker_row_y - 2, 2.2, fill="#6b7280", fill_opacity="0.28", stroke="#6b7280", stroke_opacity="0.45", stroke_width="0.8"))
    svg_parts.append(svg_text(legend_x + 40, marker_row_y, "Individual board timings when recorded", font_size="13", fill="#111827"))

    latest_entry = entries[-1]
    summary_y = legend_y + 26.0 * (len(workload_names) + 4)
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
    latest_total_names = sorted(latest_entry.total_workloads)
    latest_per_board_names = sorted(latest_entry.per_board_workloads)
    if latest_total_names:
        svg_parts.append(
            svg_text(
                legend_x,
                summary_y + 46,
                f"Cumulative series: {', '.join(latest_total_names[:3])}{' ...' if len(latest_total_names) > 3 else ''}",
                font_size="13",
                fill="#374151",
            )
        )
    if latest_per_board_names:
        svg_parts.append(
            svg_text(
                legend_x,
                summary_y + 68,
                f"Per-board series: {', '.join(latest_per_board_names[:3])}{' ...' if len(latest_per_board_names) > 3 else ''}",
                font_size="13",
                fill="#374151",
            )
        )
        latest_samples = sum(len(values) for values in latest_entry.per_board_samples.values())
        if latest_samples > 0:
            svg_parts.append(
                svg_text(
                    legend_x,
                    summary_y + 90,
                    f"Recorded individual board timings: {latest_samples}",
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
