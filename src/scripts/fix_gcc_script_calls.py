#!/usr/bin/env python3
"""Expand Commands-> calls for GCC (no default args on function pointers)."""
import re
import glob
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

def fix_file(path):
    with open(path, 'r', encoding='latin-1') as f:
        text = f.read()
    orig = text

    # Join_Conversation with exactly 2 arguments (not already 5)
    text = re.sub(
        r'Commands->Join_Conversation\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*\)(?!\s*,)',
        r'Commands->Join_Conversation(\1, \2, true, true, true)',
        text,
    )

    # Start_Conversation with 1 argument only
    text = re.sub(
        r'Commands->Start_Conversation\s*\(\s*([^,()]+)\s*\)(?!\s*,)',
        r'Commands->Start_Conversation(\1, 0)',
        text,
    )

    # Send_Custom_Event with 4 arguments (not already 5)
    text = re.sub(
        r'Commands->Send_Custom_Event\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*([^,()]+)\s*\)(?!\s*,)',
        r'Commands->Send_Custom_Event(\1, \2, \3, \4, 0)',
        text,
    )

    # Apply_Damage with 3 arguments (no damager)
    text = re.sub(
        r'Commands->Apply_Damage\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*"([^"]+)"\s*\)',
        r'Commands->Apply_Damage(\1, \2, "\3", NULL)',
        text,
    )

    # Create_Conversation with 1 argument
    text = re.sub(
        r'Commands->Create_Conversation\s*\(\s*"([^"]+)"\s*\)',
        r'Commands->Create_Conversation("\1", 0, 0, true)',
        text,
    )

    if text != orig:
        with open(path, 'w', encoding='latin-1') as f:
            f.write(text)
        return True
    return False

def main():
    n = 0
    for path in glob.glob(os.path.join(SCRIPT_DIR, '*.cpp')):
        if fix_file(path):
            n += 1
    print(f'fixed {n} files')

if __name__ == '__main__':
    main()
