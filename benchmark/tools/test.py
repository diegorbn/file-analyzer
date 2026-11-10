#!/usr/bin/env python3
"""Compatibility entry point for the shared project test suite."""
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
sys.exit(subprocess.call([sys.executable, str(root / 'tests/test_project.py')], cwd=root))
