import argparse, csv, os, statistics, subprocess, sys
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument("--trials", type=int, default=10)
p.add_argument("--warmups", type=int, default=2)
a = p.parse_args()
if a.trials < 1 or a.warmups < 0:
    p.error("invalid trial count")
root = Path.cwd()
results = root / "results"
results.mkdir(exist_ok=True)
txt = sorted((root / "datasets/txt").glob("*.txt"))
csvfiles = sorted((root / "datasets/csv").glob("*.csv"))
if not txt or not csvfiles:
    sys.exit("Generate both datasets first")
workloads = {
    "txt": txt,
    "csv": csvfiles,
    "mixed": [x for pair in zip(txt, csvfiles) for x in pair]
    + txt[len(csvfiles) :]
    + csvfiles[len(txt) :],
}
runner = root / "build/runner"


def run(variant, files, capture):
    script = results / "input.txt"
    script.write_text("".join("3\n" + str(f) + "\n" for f in files) + "4\n")
    output = results / (variant + ".out")
    err = results / (variant + ".err")
    cmd = [
        str(runner),
        str(root / "build" / variant),
        str(script),
        str(output),
        str(err),
    ]
    process = subprocess.run(cmd, text=True, capture_output=True)
    if process.returncode or err.read_text():
        raise RuntimeError(f"{cmd}: {process.stderr} {err.read_text()}")
    if not capture:
        blocks = reports(output.read_text())
        if len(blocks) != len(files) or any(
            "El análisis no se completó." in x or "Error:" in x for x in blocks
        ):
            raise RuntimeError("Timed run skipped a file or reported an analysis error")
    return tuple(map(float, process.stdout.strip().split(",")))


def reports(text):
    # Menu delimiters are common to both applications; retain complete analysis blocks.
    return [
        x.split("\n\n=== Analizador de archivos ===", 1)[0]
        for x in text.split("Analizando con ")[1:]
    ]


def validate(files):
    for v in ("static", "dynamic"):
        run(v, files, True)
    a1, b = [
        reports((results / (v + ".out")).read_text()) for v in ("static", "dynamic")
    ]
    if (
        len(a1) != len(files)
        or a1 != b
        or any("El análisis no se completó." in x or "Error:" in x for x in a1)
    ):
        raise RuntimeError(
            "Validation failed: reports differ, an analyzer failed, or a file was skipped"
        )


raw = []
summaries = []
for old in (results / "raw.csv", results / "summary.csv"):
    old.unlink(missing_ok=True)
try:
    validate([root.parent / "example/ejemplo.log"] + txt + csvfiles)
    for category, files in workloads.items():
        size = sum(f.stat().st_size for f in files)
        samples = {"static": [], "dynamic": []}
        for i in range(a.warmups + a.trials):
            for v in (("static", "dynamic") if i % 2 == 0 else ("dynamic", "static")):
                ms, rss = run(v, files, False)
                if i >= a.warmups:
                    samples[v].append(ms)
                    exe = (root / "build" / v / "file-analyzer").stat().st_size
                    libs = (
                        0
                        if v == "static"
                        else sum(
                            f.stat().st_size
                            for f in (root / "build/dynamic/plugins").glob("*.so")
                        )
                    )
                    raw.append(
                        [
                            category,
                            v,
                            i - a.warmups + 1,
                            ms,
                            len(files),
                            size,
                            size / 1e6 / (ms / 1000),
                            int(rss),
                            exe,
                            libs,
                        ]
                    )
        for v, values in samples.items():
            summaries.append(
                [
                    category,
                    v,
                    statistics.mean(values),
                    statistics.median(values),
                    min(values),
                    max(values),
                    statistics.stdev(values) if len(values) > 1 else 0,
                    statistics.mean(samples["dynamic"])
                    - statistics.mean(samples["static"]),
                    (
                        statistics.mean(samples["dynamic"])
                        / statistics.mean(samples["static"])
                        - 1
                    )
                    * 100,
                ]
            )
        s = statistics.mean(samples["static"])
        d = statistics.mean(samples["dynamic"])
        print(
            f"{category:8} static {s:10.3f} ms dynamic {d:10.3f} ms difference {d-s:+10.3f} ms {(d-s)/s*100:+8.2f}%"
        )
except (RuntimeError, OSError) as e:
    sys.exit(str(e))
for name, header, rows in [
    (
        "raw.csv",
        [
            "workload",
            "variant",
            "trial",
            "elapsed_ms",
            "files",
            "bytes",
            "throughput_MB_s",
            "peak_rss_kb",
            "executable_bytes",
            "library_bytes",
        ],
        raw,
    ),
    (
        "summary.csv",
        [
            "workload",
            "variant",
            "mean_ms",
            "median_ms",
            "min_ms",
            "max_ms",
            "stddev_ms",
            "dynamic_minus_static_ms",
            "dynamic_vs_static_percent",
        ],
        summaries,
    ),
]:
    with (results / name).open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)
