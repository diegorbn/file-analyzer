#!/usr/bin/env python3
"""Create the fixed 10 MiB benchmark dataset, with deterministic exact sizes."""
from pathlib import Path
import argparse
import os

ROOT = Path(__file__).resolve().parents[1] / 'datasets'
SIZE = 524288
parser = argparse.ArgumentParser()
parser.add_argument('--directory', type=Path, default=ROOT)
a = parser.parse_args()
for ext in ('txt', 'csv'):
    folder = a.directory / ext
    folder.mkdir(parents=True, exist_ok=True)
    for i in range(10):
        line = (f'row{i:05d},123.45,example,content\n' if ext == 'csv' else f'File {i:05d}: deterministic sample text and words.\n').encode()
        header = b'id,value,name,note\n' if ext == 'csv' else b''
        content = header + (line * ((SIZE - len(header)) // len(line) + 1))[:SIZE-len(header)]
        path = folder / f'{i:05d}.{ext}'
        if path.exists():
            if path.read_bytes() != content:
                raise SystemExit(f'Refusing to overwrite existing dataset: {path}')
            continue
        temp = path.with_suffix(path.suffix + '.tmp')
        temp.write_bytes(content)
        os.replace(temp, path)
print(f'Generated 10 TXT and 10 CSV files of {SIZE} bytes in {a.directory}')
