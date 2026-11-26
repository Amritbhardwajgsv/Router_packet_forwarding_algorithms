#!/usr/bin/env python3
"""
Builds graphs from results.csv (the output of ./timing).

Every PNG has two parts: the graph on top and an explanation panel below it.
The panel says what is plotted, how to read it, and key takeaways that are
computed from the real numbers in the CSV (nothing in the takeaways is hard-coded).

Run:
    python3 plot_results.py                  # reads results.csv, writes PNGs into plots/
    python3 plot_results.py other.csv outdir

Requirement: matplotlib only
    sudo apt install python3-matplotlib
"""
import csv
import os
import sys
import textwrap
from collections import defaultdict

try:
    import matplotlib
    matplotlib.use("Agg")                       # render PNGs without a display (works in WSL)
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle
    from matplotlib.ticker import FuncFormatter
except ImportError:
    sys.exit("matplotlib not found. Install it with:  sudo apt install python3-matplotlib")

CSV_PATH = sys.argv[1] if len(sys.argv) > 1 else "results.csv"
OUT_DIR = sys.argv[2] if len(sys.argv) > 2 else "plots"

# fixed colour and marker per algorithm so every graph looks consistent
STYLE = {
    "linear":    ("#d62728", "o"),
    "trie":      ("#1f77b4", "s"),
    "patricia":  ("#2ca02c", "^"),
    "multibit2": ("#9467bd", "D"),
    "lulea":     ("#ff7f0e", "P"),
}
ORDER = ["linear", "trie", "patricia", "multibit2", "lulea"]
LINE_RATE_NS = 67        # 64-byte packets at 10 Gbps: roughly a 67 ns per-packet budget

NOTE_LOOKUP = "ns/lookup also includes the ipToBits (string conversion) time of every lookup."
NOTE_SPEEDUP = "Linear search is skipped on very large tables, so its line (and the speedups) may end early."
NOTE_MEMORY = ("memory = nodes x sizeof(node). The heap used by patricia's label strings is not counted, "
               "so patricia looks smaller than it really is.")
NOTE_LATENCY = ("p50/p99 are measured per lookup, so the timer's own overhead (~20-50 ns) is included "
                "in every value.")


# ---------------------------------------------------------------- data
def load(path):
    if not os.path.exists(path):
        sys.exit(f"{path} not found. Run ./timing first (from the project root).")
    data = defaultdict(dict)                    # data[algo][routes] = {column: value}
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            algo = row["algo"].strip()
            routes = int(float(row["routes"]))
            vals = {}
            for k, v in row.items():
                if k == "algo" or v is None or v == "":
                    continue
                vals[k] = float(v)
            data[algo][routes] = vals           # if (algo, routes) repeats, keep the last one
    if not data:
        sys.exit(f"{path} is empty.")
    return data


def algos_in(data):
    known = [a for a in ORDER if a in data]
    return known + sorted(a for a in data if a not in ORDER)


def series(data, algo, key):
    """(routes list, values list) for one algo, only where the key exists and is > 0."""
    xs, ys = [], []
    for r in sorted(data[algo]):
        v = data[algo][r].get(key)
        if v is not None and v > 0:
            xs.append(r)
            ys.append(v)
    return xs, ys


def human(n, _pos=None):
    n = float(n)
    if n >= 1e6:
        return f"{n / 1e6:g}M"
    if n >= 1e3:
        return f"{n / 1e3:g}k"
    return f"{n:g}"


def style(algo):
    return STYLE.get(algo, ("#7f7f7f", "x"))


def value(data, algo, routes, key):
    return data.get(algo, {}).get(routes, {}).get(key)


def common_largest(data, algos, key):
    """Largest table size at which ALL the given algos have a positive value for key."""
    sets = [set(series(data, a, key)[0]) for a in algos]
    if not sets:
        return None
    common = set.intersection(*sets)
    return max(common) if common else None


def with_data(data, key):
    """Algos that have at least one positive value for key."""
    return [a for a in algos_in(data) if series(data, a, key)[0]]


def ns_fmt(v):
    return f"{v:,.0f} ns" if v >= 100 else f"{v:,.1f} ns"


# ---------------------------------------------------------------- figure + explanation panel
def build_panel_text(width_in, what, how, takeaways, note=None):
    chars = max(40, int(width_in * 13) - 4)     # rough characters per line at font size 8.5
    parts = [textwrap.fill("WHAT IT SHOWS: " + what, chars, subsequent_indent="    "),
             textwrap.fill("HOW TO READ IT: " + how, chars, subsequent_indent="    "),
             "KEY TAKEAWAYS (computed from the CSV):"]
    if takeaways:
        for t in takeaways:
            parts.append(textwrap.fill("  - " + t, chars, subsequent_indent="      "))
    else:
        parts.append("  - Not enough data in the CSV to compute takeaways.")
    if note:
        parts.append(textwrap.fill("NOTE: " + note, chars, subsequent_indent="    "))
    return "\n".join(parts)


def new_figure(width, plot_h, ncols, what, how, takeaways, note=None):
    """Figure with graph(s) on top and an explanation panel below. Returns (fig, axes)."""
    text = build_panel_text(width, what, how, takeaways, note)
    panel_h = (text.count("\n") + 1) * 0.2 + 0.45
    fig = plt.figure(figsize=(width, plot_h + panel_h), layout="constrained")
    gs = fig.add_gridspec(2, ncols, height_ratios=[plot_h, panel_h])
    axes = [fig.add_subplot(gs[0, i]) for i in range(ncols)]
    tax = fig.add_subplot(gs[1, :])
    tax.set_axis_off()
    tax.add_patch(Rectangle((0, 0), 1, 1, transform=tax.transAxes,
                            facecolor="#f4f4f4", edgecolor="#bbbbbb"))
    tax.text(0.012, 0.96, text, transform=tax.transAxes, va="top", ha="left",
             fontsize=8.5, linespacing=1.35)
    return fig, axes


def setup_routes_axis(ax, data):
    all_routes = sorted({r for a in data for r in data[a]})
    ax.set_xscale("log")
    ax.set_xticks(all_routes)
    ax.xaxis.set_major_formatter(FuncFormatter(human))
    ax.minorticks_off()
    ax.set_xlabel("routes in table")
    ax.grid(True, which="major", alpha=0.3)


def save(fig, name):
    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, name)
    fig.savefig(path, dpi=140)
    plt.close(fig)
    print("saved", path)


# ---------------------------------------------------------------- takeaways (all computed from data)
def takeaways_lookup(data):
    algos = with_data(data, "ns_per_lookup")
    out = []
    r = common_largest(data, algos, "ns_per_lookup")
    if r and len(algos) > 1:
        ns = {a: value(data, a, r, "ns_per_lookup") for a in algos}
        fast = min(ns, key=ns.get)
        slow = max(ns, key=ns.get)
        out.append(f"At {human(r)} routes (the largest size every algorithm was measured at), {fast} is fastest "
                   f"at {ns_fmt(ns[fast])} per lookup and {slow} is slowest at {ns_fmt(ns[slow])} "
                   f"({ns[slow] / ns[fast]:,.0f}x slower).")
        ok = [a for a in algos if ns[a] <= LINE_RATE_NS]
        if ok:
            out.append(f"Within the ~{LINE_RATE_NS} ns 10G line-rate budget at {human(r)} routes: {', '.join(ok)}.")
        else:
            out.append(f"No algorithm fits inside the ~{LINE_RATE_NS} ns 10G line-rate budget at {human(r)} routes.")
        bad = [a for a in algos if ns[a] > LINE_RATE_NS]
        if ok and bad:
            out.append(f"Over budget at that size: {', '.join(bad)}.")
    growth = []
    for a in algos:
        xs, ys = series(data, a, "ns_per_lookup")
        if len(xs) > 1:
            growth.append(f"{a} {ns_fmt(ys[0])} -> {ns_fmt(ys[-1])} ({ys[-1] / ys[0]:.1f}x, "
                          f"{human(xs[0])} -> {human(xs[-1])} routes)")
    if growth:
        out.append("Growth from the smallest to the largest table: " + "; ".join(growth) + ".")
    return out


def takeaways_speedup(data, algos):
    out = []
    best = None
    for a in algos:
        xs, ys = [], []
        for r in sorted(data[a]):
            lin = value(data, "linear", r, "ns_per_lookup")
            cur = value(data, a, r, "ns_per_lookup")
            if lin and cur:
                xs.append(r)
                ys.append(lin / cur)
        if not xs:
            continue
        if best is None or ys[-1] > best[2]:
            best = (a, xs[-1], ys[-1])
        trend = ""
        if len(xs) > 1:
            trend = (f"; it was {ys[0]:,.0f}x at {human(xs[0])} routes, "
                     f"so the gap {'widens' if ys[-1] > ys[0] else 'shrinks'} as the table grows")
        out.append(f"{a} is {ys[-1]:,.0f}x faster than linear at {human(xs[-1])} routes{trend}.")
    if best and len(out) > 1:
        out.append(f"Biggest speedup over linear: {best[0]} ({best[2]:,.0f}x at {human(best[1])} routes).")
    return out


def takeaways_memory(data):
    algos = with_data(data, "mem_mb")
    out = []
    r = common_largest(data, algos, "mem_mb")
    if r and len(algos) > 1:
        mem = {a: value(data, a, r, "mem_mb") for a in algos}
        small = min(mem, key=mem.get)
        big = max(mem, key=mem.get)
        out.append(f"At {human(r)} routes, {small} uses the least memory ({mem[small]:,.1f} MB) and {big} "
                   f"the most ({mem[big]:,.1f} MB), a {mem[big] / mem[small]:.1f}x difference.")
        if "trie" in mem:
            for a in algos:
                if a != "trie":
                    ratio = mem[a] / mem["trie"]
                    word = "less" if ratio < 1 else "more"
                    out.append(f"{a} uses {ratio:.2f}x the memory of trie "
                               f"({abs(1 - ratio) * 100:.0f}% {word}).")
    for a in algos:
        xs, ys = series(data, a, "mem_mb")
        if len(xs) > 1:
            out.append(f"{a} grows from {ys[0]:,.2f} MB to {ys[-1]:,.1f} MB between {human(xs[0])} and "
                       f"{human(xs[-1])} routes.")
    return out


def takeaways_nodes(data):
    algos = with_data(data, "nodes")
    out = []
    r = common_largest(data, algos, "nodes")
    if r and len(algos) > 1:
        npr = {a: value(data, a, r, "nodes") / r for a in algos}
        fewest = min(npr, key=npr.get)
        most = max(npr, key=npr.get)
        out.append(f"At {human(r)} routes: " + ", ".join(f"{a} {npr[a]:.2f} nodes/route" for a in algos) + ".")
        out.append(f"{fewest} needs the fewest nodes per route; {most} needs the most "
                   f"({npr[most] / npr[fewest]:.1f}x more).")
        if "trie" in npr and "patricia" in npr:
            cut = (1 - npr["patricia"] / npr["trie"]) * 100
            out.append(f"Path compression (patricia vs trie) removes {cut:.0f}% of the nodes "
                       f"at {human(r)} routes.")
    return out


def takeaways_latency(data, largest, names):
    out = []
    if not names:
        return out
    p50 = {a: data[a][largest]["p50_ns"] for a in names}
    p99 = {a: data[a][largest]["p99_ns"] for a in names}
    ratio = {a: p99[a] / p50[a] for a in names if p50[a] > 0}
    out.append(f"At {human(largest)} routes: " +
               ", ".join(f"{a} p50 {p50[a]:,.0f} / p99 {p99[a]:,.0f} ns" for a in names) + ".")
    if ratio:
        wide = max(ratio, key=ratio.get)
        tight = min(ratio, key=ratio.get)
        out.append(f"Tail (p99 / p50): {wide} has the widest tail ({ratio[wide]:.1f}x) and {tight} the "
                   f"tightest ({ratio[tight]:.1f}x).")
    best = min(p99, key=p99.get)
    out.append(f"Best worst-case (lowest p99): {best} at {p99[best]:,.0f} ns.")
    over = [a for a in names if p99[a] > LINE_RATE_NS]
    if over:
        out.append(f"p99 is above the ~{LINE_RATE_NS} ns line-rate budget for: {', '.join(over)}.")
    else:
        out.append(f"Every algorithm's p99 stays inside the ~{LINE_RATE_NS} ns line-rate budget.")
    return out


# ---------------------------------------------------------------- plots
def plot_lookup_time(data):
    fig, (ax,) = new_figure(
        8, 5, 1,
        what="Average time of one routing lookup (ns/lookup) for each algorithm, as the number of routes "
             "in the table grows. The dashed grey line is the per-packet time budget for 10 Gbps line rate "
             f"with 64-byte packets (~{LINE_RATE_NS} ns).",
        how="X axis = table size, Y axis = ns per lookup (both log scales). Lower is better. A flat line "
            "means lookup time does not depend on table size; a line rising with the table means lookups "
            "get slower as routes are added. A line below the dashed line can keep up with 10G traffic.",
        takeaways=takeaways_lookup(data), note=NOTE_LOOKUP)
    for a in algos_in(data):
        xs, ys = series(data, a, "ns_per_lookup")
        if xs:
            c, m = style(a)
            ax.plot(xs, ys, marker=m, color=c, label=a, linewidth=2)
    ax.axhline(LINE_RATE_NS, color="gray", linestyle="--", linewidth=1)
    # x in axes fraction, y in data coordinates (safe with a log scale)
    ax.text(0.01, LINE_RATE_NS * 1.08, f"10G line rate ~ {LINE_RATE_NS} ns/packet",
            transform=ax.get_yaxis_transform(), color="gray", fontsize=8, va="bottom")
    ax.set_yscale("log")
    setup_routes_axis(ax, data)
    ax.set_ylabel("ns per lookup (log scale, lower is better)")
    ax.set_title("Lookup time vs table size")
    ax.legend()
    save(fig, "01_lookup_time.png")


def plot_speedup(data):
    if "linear" not in data:
        print("skip speedup plot: no linear results in the CSV")
        return
    algos = [a for a in algos_in(data) if a != "linear"]
    series_list = []
    for a in algos:
        xs, ys = [], []
        for r in sorted(data[a]):
            lin = value(data, "linear", r, "ns_per_lookup")
            cur = value(data, a, r, "ns_per_lookup")
            if lin and cur:
                xs.append(r)
                ys.append(lin / cur)
        if xs:
            series_list.append((a, xs, ys))
    if not series_list:
        return
    fig, (ax,) = new_figure(
        8, 5, 1,
        what="How many times faster each algorithm is than linear search at the same table size "
             "(speedup = linear ns/lookup divided by the algorithm's ns/lookup).",
        how="X axis = table size, Y axis = speedup (log scale). Higher is better. A value of 100 means "
            "100x faster than linear search. A rising line means the advantage over linear grows with "
            "table size.",
        takeaways=takeaways_speedup(data, algos), note=NOTE_SPEEDUP)
    for a, xs, ys in series_list:
        c, m = style(a)
        ax.plot(xs, ys, marker=m, color=c, label=a, linewidth=2)
    ax.set_yscale("log")
    setup_routes_axis(ax, data)
    ax.set_ylabel("speedup over linear (x, log scale)")
    ax.set_title("How much faster than linear search")
    ax.legend()
    save(fig, "02_speedup_vs_linear.png")


def plot_memory(data):
    algos = with_data(data, "mem_mb")
    if not algos:
        print("skip memory plot: the CSV has no mem_mb column (run a newer ./timing)")
        return
    fig, (ax,) = new_figure(
        8, 5, 1,
        what="Estimated memory used by each data structure (MB) as the number of routes grows.",
        how="X axis = table size, Y axis = memory in MB (log scale). Lower is better. Compare line heights "
            "for the cost of each structure; compare slopes to see how fast memory grows with the table.",
        takeaways=takeaways_memory(data), note=NOTE_MEMORY)
    for a in algos:
        xs, ys = series(data, a, "mem_mb")
        c, m = style(a)
        ax.plot(xs, ys, marker=m, color=c, label=a, linewidth=2)
    ax.set_yscale("log")
    setup_routes_axis(ax, data)
    ax.set_ylabel("memory in MB (log scale, lower is better)")
    ax.set_title("Memory vs table size")
    ax.legend()
    save(fig, "03_memory.png")


def plot_nodes(data):
    algos = with_data(data, "nodes")
    if not algos:
        return
    fig, (ax1, ax2) = new_figure(
        12, 5, 2,
        what="Left: total number of nodes each structure allocates. Right: the same number divided by "
             "the number of routes (nodes per route), which shows how efficiently each structure stores a route.",
        how="Left: log-log, lower is better. Right: linear Y axis starting at 0; a lower line means fewer "
            "nodes are needed per route, so the structure is more compact. A line that is flat means node "
            "count grows in proportion to the table.",
        takeaways=takeaways_nodes(data))
    for a in algos:
        xs, ys = series(data, a, "nodes")
        c, m = style(a)
        ax1.plot(xs, ys, marker=m, color=c, label=a, linewidth=2)
        ax2.plot(xs, [y / x for x, y in zip(xs, ys)], marker=m, color=c, label=a, linewidth=2)
    ax1.set_yscale("log")
    setup_routes_axis(ax1, data)
    ax1.set_ylabel("nodes (log scale)")
    ax1.set_title("Total nodes")
    ax1.legend()
    setup_routes_axis(ax2, data)
    ax2.set_ylabel("nodes per route")
    ax2.set_title("Nodes per route (shows the benefit of path compression)")
    ax2.set_ylim(bottom=0)
    ax2.legend()
    save(fig, "04_nodes.png")


def plot_latency(data):
    has = [a for a in algos_in(data) if any("p50_ns" in v and "p99_ns" in v for v in data[a].values())]
    if not has:
        print("skip latency plot: the CSV has no p50_ns / p99_ns columns")
        return
    largest = max(r for a in has for r in data[a] if "p50_ns" in data[a][r] and "p99_ns" in data[a][r])
    names = [a for a in has if largest in data[a] and "p50_ns" in data[a][largest] and "p99_ns" in data[a][largest]]
    fig, (ax1, ax2) = new_figure(
        12, 5, 2,
        what=f"Left: typical (p50) vs worst-1% (p99) lookup latency for each algorithm at the largest "
             f"table size ({human(largest)} routes). Right: how the p99 tail latency changes as the table grows.",
        how="p50 is the median lookup: half of all lookups are faster. p99 means 99% of lookups are faster "
            "than this value, so it captures the slow outliers. Lower is better. A big gap between the "
            "p50 and p99 bars means unpredictable timing. On the right, a rising line means the worst "
            "cases get worse as the table grows.",
        takeaways=takeaways_latency(data, largest, names), note=NOTE_LATENCY)

    w = 0.38
    xs = list(range(len(names)))
    p50 = [data[a][largest]["p50_ns"] for a in names]
    p99 = [data[a][largest]["p99_ns"] for a in names]
    b1 = ax1.bar([x - w / 2 for x in xs], p50, w, label="p50 (typical)", color="#4c72b0")
    b2 = ax1.bar([x + w / 2 for x in xs], p99, w, label="p99 (worst 1%)", color="#dd8452")
    ax1.set_xticks(xs)
    ax1.set_xticklabels(names)
    ax1.set_ylabel("ns per lookup")
    ax1.set_title(f"Latency at {human(largest)} routes")
    ax1.set_ylim(0, max(p99) * 1.15)
    ax1.bar_label(b1, fmt="%.0f", fontsize=7)
    ax1.bar_label(b2, fmt="%.0f", fontsize=7)
    ax1.legend()
    ax1.grid(True, axis="y", alpha=0.3)

    for a in has:
        sx, sy = series(data, a, "p99_ns")
        if sx:
            c, m = style(a)
            ax2.plot(sx, sy, marker=m, color=c, label=a, linewidth=2)
    ax2.set_yscale("log")
    setup_routes_axis(ax2, data)
    ax2.set_ylabel("p99 ns per lookup (log scale)")
    ax2.set_title("Tail latency vs table size")
    ax2.legend()
    save(fig, "05_latency.png")


# ---------------------------------------------------------------- text summary
def print_summary(data):
    print("\nSummary (ns = ns per lookup)")
    print(f"{'routes':>9}  {'algo':<10} {'ns':>10} {'p50':>8} {'p99':>8} {'nodes':>10} {'mem_MB':>8}"
          f" {'vs linear':>10} {'vs trie':>8}")
    all_routes = sorted({r for a in data for r in data[a]})
    for r in all_routes:
        lin = data.get("linear", {}).get(r, {}).get("ns_per_lookup")
        tri = data.get("trie", {}).get(r, {}).get("ns_per_lookup")
        for a in algos_in(data):
            row = data[a].get(r)
            if not row:
                continue
            ns = row.get("ns_per_lookup", 0)
            vs_lin = f"{lin / ns:,.0f}x" if lin and ns and a != "linear" else "-"
            vs_tri = f"{tri / ns:.2f}x" if tri and ns and a != "trie" else "-"
            print(f"{human(r):>9}  {a:<10} {ns:>10,.1f} {row.get('p50_ns', 0):>8,.0f} {row.get('p99_ns', 0):>8,.0f}"
                  f" {row.get('nodes', 0):>10,.0f} {row.get('mem_mb', 0):>8,.1f} {vs_lin:>10} {vs_tri:>8}")
        print()
    print("vs linear / vs trie: above 1.00x means faster than that baseline, below 1.00x means slower.")


def main():
    data = load(CSV_PATH)
    print(f"loaded {CSV_PATH}: algorithms = {', '.join(algos_in(data))}")
    plot_lookup_time(data)
    plot_speedup(data)
    plot_memory(data)
    plot_nodes(data)
    plot_latency(data)
    print_summary(data)


if __name__ == "__main__":
    main()