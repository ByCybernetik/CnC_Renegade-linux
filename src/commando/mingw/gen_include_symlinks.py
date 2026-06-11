#!/usr/bin/env python3
"""VC6-style #include paths with original casing on case-sensitive hosts."""
from __future__ import annotations

import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
INC_ROOT = Path(__file__).resolve().parent / 'include'

SRC = ROOT / 'src'
CODE = ROOT / 'Code'

PREFIX_DIRS = {
    'WWLib': SRC / 'wwlib',
    'wwlib': SRC / 'wwlib',
    'WWDebug': SRC / 'wwdebug',
    'wwdebug': SRC / 'wwdebug',
    'WOLAPI': CODE / 'wolapi',
    'wolapi': CODE / 'wolapi',
    'WWOnline': CODE / 'WWOnline',
    'wwonline': CODE / 'WWOnline',
    'WWUI': SRC / 'wwui',
    'wwui': SRC / 'wwui',
    'WWMath': SRC / 'wwmath',
    'wwmath': SRC / 'wwmath',
    'WWAudio': SRC / 'wwaudio',
    'wwaudio': SRC / 'wwaudio',
    'Combat': SRC / 'combat',
    'combat': SRC / 'combat',
    'commando': SRC / 'commando',
    'Commando': SRC / 'commando',
    'BinkMovie': SRC / 'binkmovie',
    'binkmovie': SRC / 'binkmovie',
    'WWTranslateDB': SRC / 'wwtranslatedb',
    'wwtranslatedb': SRC / 'wwtranslatedb',
    'WW3D2': SRC / 'ww3d2',
    'ww3d2': SRC / 'ww3d2',
    'wwnet': SRC / 'wwnet',
    'wwbitpack': SRC / 'wwbitpack',
    'wwutil': SRC / 'wwutil',
    'wwsaveload': SRC / 'wwsaveload',
    'wwphys': SRC / 'wwphys',
    'SControl': SRC / 'scontrol',
    'scontrol': SRC / 'scontrol',
    'Scripts': SRC / 'scripts',
    'scripts': SRC / 'scripts',
}

SEARCH_BASES = list({p for p in PREFIX_DIRS.values()}) + [
    SRC / 'ww3d2',
    CODE / 'BandTest',
]

SCAN_DIRS = [
    SRC / 'commando',
    SRC / 'combat',
    SRC / 'wwlib',
    SRC / 'wwaudio',
    SRC / 'wwui',
    SRC / 'scripts',
    SRC / 'wwmath',
    SRC / 'wwnet',
    SRC / 'wwphys',
    SRC / 'ww3d2',
    CODE / 'WWOnline',
    CODE / 'wolapi',
    CODE / 'WOLBrowser',
]

INCLUDE_RE = re.compile(
    r'#include\s*[<"]([^>"]+)[>"]'
)
REL_COMMANDO_RE = re.compile(
    r'#include\s*["\']\.\./[Cc]ommando/([^"\']+)["\']'
)
REL_PARENT_RE = re.compile(
    r'#include\s*["\'](\.\./[^"\']+)["\']'
)


def find_case_insensitive(base: Path, rel: str) -> Path | None:
    cur = base
    for part in rel.replace('\\', '/').split('/'):
        if not part:
            continue
        if not cur.is_dir():
            return None
        nxt = None
        low = part.lower()
        for entry in cur.iterdir():
            if entry.name.lower() == low:
                nxt = entry
                break
        if nxt is None:
            return None
        cur = nxt
    return cur if cur.is_file() else None


def resolve_include(inc: str) -> Path | None:
    inc = inc.replace('\\', '/')
    if inc.startswith('../commando/') or inc.startswith('../Commando/'):
        return find_case_insensitive(SRC / 'commando', inc.split('/', 1)[1])

    if inc.startswith('../'):
        rel = inc[3:].replace('//', '/')
        if rel.lower().startswith('wwonline/'):
            return find_case_insensitive(CODE / 'WWOnline', rel.split('/', 1)[1])
        if rel.lower().startswith('wwlib/'):
            return find_case_insensitive(SRC / 'wwlib', rel.split('/', 1)[1])
        return find_case_insensitive(ROOT, rel)

    if '/' in inc:
        prefix, _, rest = inc.partition('/')
        base = PREFIX_DIRS.get(prefix)
        if base is None:
            return None
        return find_case_insensitive(base, rest)

    if inc.lower() == 'scripts.h':
        hit = find_case_insensitive(SRC / 'combat', 'scripts.h')
        if hit is not None:
            return hit

    for base in SEARCH_BASES:
        hit = find_case_insensitive(base, inc)
        if hit is not None:
            return hit
    return None


def needs_symlink(include_path: str, target: Path) -> bool:
    inc_parts = include_path.replace('\\', '/').split('/')
    try:
        act_parts = target.relative_to(ROOT).as_posix().split('/')
    except ValueError:
        return True
    n = min(len(inc_parts), len(act_parts))
    for i in range(1, n + 1):
        if inc_parts[-i] != act_parts[-i]:
            return True
    return len(inc_parts) != len(act_parts)


def collect_includes() -> set[str]:
    out: set[str] = set()
    for root in SCAN_DIRS:
        if not root.is_dir():
            continue
        for path in root.rglob('*'):
            if path.suffix.lower() not in ('.cpp', '.h', '.c'):
                continue
            if 'mingw/include' in str(path) or 'build-' in path.parts:
                continue
            try:
                text = path.read_text(errors='ignore')
            except OSError:
                continue
            for m in INCLUDE_RE.finditer(text):
                inc = m.group(1).strip()
                if inc.endswith(('.cpp', '.c')):
                    continue
                out.add(inc)
            for m in REL_COMMANDO_RE.finditer(text):
                out.add('commando/' + m.group(1))
            for m in REL_PARENT_RE.finditer(text):
                out.add(m.group(1))
    return out


EXTRA_INCLUDES = (
    'winmain.h',
    'WINMAIN.H',
    'WW3D.H',
    'ww3d.h',
    'TARGA.H',
    'targa.h',
    'scripts.h',  # combat ScriptManager (not src/scripts/scripts.h)
)


def main() -> int:
    requested = collect_includes() | set(EXTRA_INCLUDES)
    INC_ROOT.mkdir(parents=True, exist_ok=True)

    created = 0
    missing: list[str] = []
    for inc in sorted(requested):
        target = resolve_include(inc)
        if target is None:
            missing.append(inc)
            continue
        if not needs_symlink(inc, target):
            continue

        link = INC_ROOT / inc.replace('\\', '/')
        link.parent.mkdir(parents=True, exist_ok=True)
        if link.exists() or link.is_symlink():
            link.unlink()
        link.symlink_to(os.path.relpath(target, link.parent))
        created += 1

    atl = Path(__file__).resolve().parent / 'atlbase.h'
    if atl.is_file():
        atl_link = INC_ROOT / 'atlbase.h'
        if atl_link.exists() or atl_link.is_symlink():
            atl_link.unlink()
        atl_link.symlink_to(os.path.relpath(atl, INC_ROOT))

    print(f'gen_include_symlinks: {created} case aliases under {INC_ROOT}')
    if missing:
        print(f'not resolved ({len(missing)}):')
        for m in missing[:15]:
            print(f'  {m}')
        if len(missing) > 15:
            print(f'  ... and {len(missing) - 15} more')
    return 0


if __name__ == '__main__':
    sys.exit(main())
