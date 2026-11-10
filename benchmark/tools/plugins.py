#!/usr/bin/env python3
"""Manage the built and installed plugins for each application."""
import argparse
import os
import shutil
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NAMES = ('txt', 'csv', 'log')
TARGET_NAMES = {'demo': NAMES, 'benchmark': ('txt', 'csv')}


def path(directory, name):
    if name not in NAMES:
        raise ValueError(f'Unknown plugin: {name}')
    return directory / (name + '_plugin.so')


def check_target(target):
    if target.is_symlink() or (target.exists() and not target.is_file()):
        raise ValueError(f'Unsafe plugin path: {target}')


def atomic_copy(source, target):
    """Replace the directory entry, never overwrite a possibly mapped inode."""
    check_target(target)
    fd, temp_name = tempfile.mkstemp(prefix='.plugin-', dir=target.parent)
    os.close(fd)
    try:
        shutil.copy2(source, temp_name)
        os.replace(temp_name, target)
    finally:
        Path(temp_name).unlink(missing_ok=True)


def install(built, active, names):
    if active.is_symlink() or built.is_symlink():
        raise ValueError('Plugin directories must not be symbolic links')
    active.mkdir(parents=True, exist_ok=True)
    for name in names:
        source = path(built, name)
        if not source.is_file():
            raise FileNotFoundError(source)
        check_target(path(active, name))
    for name in names:
        atomic_copy(path(built, name), path(active, name))


def remove(active, names):
    if active.is_symlink():
        raise ValueError('Plugin directory must not be a symbolic link')
    for name in names:
        target = path(active, name)
        check_target(target)
        target.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', choices=TARGET_NAMES, required=True)
    parser.add_argument('action', choices=('list', 'activate', 'deactivate', 'reset'))
    parser.add_argument('names', nargs='*', choices=NAMES)
    args = parser.parse_args()
    allowed = TARGET_NAMES[args.target]
    if any(name not in allowed for name in args.names):
        parser.error(f'{args.target} supports: {", ".join(allowed)}')
    root = ROOT if args.target == 'benchmark' else ROOT / args.target
    built, active = root / 'plugins-built', root / 'plugins-active'
    if args.action == 'list':
        for label, directory in (('built', built), ('active', active)):
            print(label + ': ' + ', '.join(n for n in allowed if path(directory, n).is_file()))
    elif args.action == 'activate':
        install(built, active, args.names or allowed)
    elif args.action == 'reset':
        remove(active, allowed)
    else:
        remove(active, args.names)


if __name__ == '__main__':
    main()
