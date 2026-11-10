import subprocess
from pathlib import Path

root = Path.cwd()
temp = root / "datasets/test"
temp.mkdir(exist_ok=True)
fixtures = {
    "empty.txt": "",
    "empty.csv": "",
    "bad.csv": 'a,b\n"unterminated',
    "unknown.xyz": "x",
    "missing.txt": None,
}
for name, data in fixtures.items():
    if data is not None:
        (temp / name).write_text(data)


def execute(v, paths, cwd=None):
    script = "".join("3\n" + str(p) + "\n" for p in paths) + "4\n"
    return subprocess.run(
        [str(root / "build" / v / "file-analyzer")],
        cwd=cwd or root / "build" / v,
        input=script,
        text=True,
        capture_output=True,
        check=True,
    )


paths = [root.parent / "example" / ("ejemplo." + x) for x in ("txt", "csv", "log")] + [
    temp / x for x in fixtures
]
a, b = (execute(v, paths) for v in ("static", "dynamic"))


def blocks(text):
    return [
        x.split("\n\n=== Analizador de archivos ===", 1)[0]
        for x in text.split("Ruta del archivo: ")[1:]
    ]


assert blocks(a.stdout) == blocks(b.stdout)
assert len(blocks(a.stdout)) == len(paths)
assert "formato CSV inválido" in a.stdout and "archivo CSV está vacío" in a.stdout
assert (
    "No hay un analizador disponible" in a.stdout
    and "El análisis no se completó" in a.stdout
)
missing = temp / "no-plugins"
missing.mkdir(exist_ok=True)
r = execute("dynamic", [root.parent / "example/ejemplo.txt"], missing)
assert (
    "no se pudo abrir el directorio" in r.stderr
    and "No hay un analizador disponible" in r.stdout
)
r = subprocess.run(
    [
        str(root / "build/runner"),
        str(missing),
        "/dev/null",
        "/dev/null",
        str(temp / "err"),
    ],
    capture_output=True,
)
assert r.returncode != 0
subprocess.run(
    ["python3", "tools/benchmark.py", "--trials", "1", "--warmups", "0"], check=True
)
for f in ("raw.csv", "summary.csv"):
    assert (root / "results" / f).read_text().count("\n") > 1
print("Correctness and failure-path tests passed")
