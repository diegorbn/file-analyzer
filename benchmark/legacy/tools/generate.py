import argparse
import random
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument("--preset", choices=["smoke", "medium", "heavy"], default="smoke")
p.add_argument("--count", type=int, help="files per format")
p.add_argument("--bytes", type=int, help="approximate bytes per file")
p.add_argument("--seed", type=int, default=12345)
a = p.parse_args()
count, size = {"smoke": (3, 4096), "medium": (50, 65536), "heavy": (200, 400000)}[
    a.preset
]
count, size = a.count or count, a.bytes or size
if count < 1 or size < 1:
    p.error("count and bytes must be positive")
r = random.Random(a.seed)
root = Path("datasets")
root.mkdir(exist_ok=True)
for ext in ("txt", "csv"):
    d = root / ext
    d.mkdir(exist_ok=True)
    for old in d.glob("*." + ext):
        old.unlink()
    for i in range(count):
        with (d / f"{i:05}.{ext}").open("w", encoding="utf-8", newline="") as f:
            if ext == "csv":
                f.write("id,value,name,note,empty\n")
            while f.tell() < size:
                n = r.randrange(100000)
                if ext == "txt":
                    f.write(
                        f"Párrafo {i}: datos comunes y palabra{n}.\n\tUna línea corta.\n\n"
                    )
                else:
                    f.write(f'{n},{n / 100:.2f},item{i},"value, quoted",\n')
