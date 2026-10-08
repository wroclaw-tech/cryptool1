#!/usr/bin/env python3
"""Runs the compile command of one source file from build/compile_commands.json with -fsyntax-only."""
import json
import shlex
import subprocess
import sys

build = sys.argv[1]
target = sys.argv[2]
extra = sys.argv[3:]
cc = json.load(open(build + '/compile_commands.json'))
entry = [x for x in cc if x['file'].endswith(target)][0]
args = shlex.split(entry['command'])
out = []
skip = False
for a in args:
    if skip:
        skip = False
        continue
    if a in ('-o', '-MF', '-MT'):
        skip = True
        continue
    if a in ('-MD', '-c'):
        continue
    out.append(a)
out += ['-fsyntax-only'] + extra
r = subprocess.run(out, cwd=entry['directory'], capture_output=True, text=True)
sys.stdout.write(r.stderr)
sys.exit(r.returncode)
