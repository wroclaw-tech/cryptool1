#!/usr/bin/env python3
"""Qualifies message map handlers (ON_xxx(..., OnFoo) -> ON_xxx(..., &CClass::OnFoo)).

MSVC accepts bare member names in message map macros; standard C++ needs &Class::member.
"""
import os
import re
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DIRS = ['CrypTool', 'ChallengeResponse', 'libVolRen', 'aestool', 'NumberShark']
BEGIN_RE = re.compile(rb'BEGIN_MESSAGE_MAP\s*\(\s*(\w+)\s*,')
END_RE = re.compile(rb'END_MESSAGE_MAP\s*\(\s*\)')
ENTRY_RE = re.compile(rb'^(\s*ON_(?!EVENT\b)\w+\s*\(.*,\s*)([A-Za-z_]\w*)(\s*\)[^\n]*)$', re.M)


def fix(data):
    out = []
    pos = 0
    changed = False
    while True:
        m = BEGIN_RE.search(data, pos)
        if not m:
            out.append(data[pos:])
            break
        end = END_RE.search(data, m.end())
        if not end:
            out.append(data[pos:])
            break
        cls = m.group(1)
        body = data[m.end():end.start()]
        new_body = ENTRY_RE.sub(lambda e: e.group(1) + b'&' + cls + b'::' + e.group(2) + e.group(3), body)
        changed |= new_body != body
        out.append(data[pos:m.end()])
        out.append(new_body)
        pos = end.start()
    return b''.join(out), changed


def main():
    count = 0
    for d in DIRS:
        full = os.path.join(REPO, d)
        for name in sorted(os.listdir(full)):
            if name.lower().endswith(('.cpp', '.h')):
                path = os.path.join(full, name)
                data = open(path, 'rb').read()
                new, changed = fix(data)
                if changed:
                    open(path, 'wb').write(new)
                    count += 1
    print('qualified handlers in', count, 'files', file=sys.stderr)


if __name__ == '__main__':
    main()
