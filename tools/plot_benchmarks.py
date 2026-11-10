#!/usr/bin/env python3
"""Turn Google Benchmark JSON output into the broad-phase chart and table used in the README.

    idem_bench --benchmark_out=docs/benchmarks/results.json --benchmark_out_format=json
    python tools/plot_benchmarks.py docs/benchmarks/results.json
"""

import argparse
import json
import pathlib
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK_MUTED = "#52514e"
GRID = "#e4e3df"

SERIES = {
    "brute": ("Brute force  O(n²)", "#2a78d6", "o"),
    "grid": ("Uniform grid  O(n)", "#eb6834", "s"),
    "bvh": ("LBVH  O(n log n)", "#1baf7a", "^"),
}


def to_ms(entry):
    scale = {"ns": 1e-6, "us": 1e-3, "ms": 1.0, "s": 1e3}[entry["time_unit"]]
    return entry["real_time"] * scale


def load(path):
    data = json.loads(pathlib.Path(path).read_text())
    groups = defaultdict(dict)
    for entry in data["benchmarks"]:
        if entry.get("run_type") != "iteration":
            continue
        family, _, rest = entry["name"].partition("/")
        if not family.startswith("broadphase"):
            continue
        variant, _, count = rest.partition("/")
        groups[(family, variant)][int(count)] = to_ms(entry)
    return data["context"], groups


def style_axes(ax, title):
    ax.set_facecolor(SURFACE)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_title(title, loc="left", color=INK, fontsize=11, fontweight="bold", pad=10)
    ax.set_xlabel("bodies", color=INK_MUTED)
    ax.grid(True, which="major", color=GRID, linewidth=0.8)
    ax.tick_params(colors=INK_MUTED, which="both", labelsize=9)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(GRID)


def plot_family(ax, groups, family, variants):
    for variant in variants:
        points = groups.get((family, variant))
        if not points:
            continue
        label, color, marker = SERIES[variant]
        xs = sorted(points)
        ys = [points[x] for x in xs]
        ax.plot(xs, ys, color=color, linewidth=2, marker=marker, markersize=6,
                markeredgecolor=SURFACE, markeredgewidth=1.5, label=label)
        ax.annotate(f"{ys[-1]:.1f} ms", (xs[-1], ys[-1]), xytext=(6, 0), textcoords="offset points",
                    va="center", fontsize=8.5, color=INK)


def markdown_table(groups, family, variants):
    counts = sorted({n for (f, v), pts in groups.items() if f == family for n in pts})
    header = "| bodies | " + " | ".join(SERIES[v][0].split("  ")[0] for v in variants) + " |"
    lines = [header, "|---:|" + "---:|" * len(variants)]
    for n in counts:
        cells = []
        for v in variants:
            t = groups.get((family, v), {}).get(n)
            cells.append(f"{t:.2f} ms" if t is not None else "—")
        lines.append(f"| {n:,} | " + " | ".join(cells) + " |")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("results", type=pathlib.Path)
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("docs/benchmarks/broadphase.svg"))
    args = parser.parse_args()

    context, groups = load(args.results)

    plt.rcParams["font.family"] = "DejaVu Sans"
    fig, (left, right) = plt.subplots(1, 2, figsize=(11, 4.4), facecolor=SURFACE)
    style_axes(left, "Similar sizes (settled scene)")
    style_axes(right, "1% of bodies 8x larger")
    left.set_ylabel("find_pairs, ms per call", color=INK_MUTED)

    plot_family(left, groups, "broadphase", ["brute", "grid", "bvh"])
    plot_family(right, groups, "broadphase_outliers", ["grid", "bvh"])
    right.set_ylim(left.get_ylim())

    handles, labels = left.get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=3, frameon=False, fontsize=9.5,
               labelcolor=INK, bbox_to_anchor=(0.5, 1.0))
    cpu = f"{context.get('num_cpus', '?')} x {context.get('mhz_per_cpu', '?')} MHz"
    fig.text(0.01, 0.01, f"Google Benchmark, Release build, single thread, {cpu}", fontsize=8, color=INK_MUTED)
    fig.tight_layout(rect=(0, 0.03, 1, 0.92))

    args.out.parent.mkdir(parents=True, exist_ok=True)
    # No timestamp and no trailing whitespace, so regenerating the chart gives a clean diff.
    fig.savefig(args.out, facecolor=SURFACE, metadata={"Date": None})
    if args.out.suffix == ".svg":
        text = args.out.read_text()
        args.out.write_text("\n".join(line.rstrip() for line in text.splitlines()) + "\n")
    print(f"wrote {args.out}\n")
    print(markdown_table(groups, "broadphase", ["brute", "grid", "bvh"]))
    print()
    print(markdown_table(groups, "broadphase_outliers", ["grid", "bvh"]))


if __name__ == "__main__":
    main()
