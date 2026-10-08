#!/usr/bin/env python3
"""Rewrites #include directives in the CrypTool sources to portable spellings.

Backslashes become slashes, './' prefixes are dropped and quoted includes are matched
case-insensitively against the files on disk so they also resolve on case-sensitive file systems.
"""
import os
import re
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DIRS = ['CrypTool', 'CrypTool/zip', 'ChallengeResponse', 'libVolRen']
SPECIAL = {
    '../aes/mars/mars.h': '"mars.h"',
    '../aes/twofish/twofish.h': '"Twofish.h"',
    '../aes/serpent/serpent.h': '"Serpent.h"',
    '../aes/rijndael/rijndael-api-fst.h': '"Rijndael-api-fst.h"',
    '../aes/rc6/rc6.h': '"RC6.h"',
    '../libntl/include/ntl/version.h': '<NTL/version.h>',
    '../libmiracl/include/big.h': '"big.h"',
    'libanalyse/la_string.h': '"libanalyse/la_string.h"',
    'libanalyse/analyse.h': '"libanalyse/analyse.h"',
    'libanalyse/ngram.h': '"libanalyse/NGram.h"',
    'sys/timeb.h': '<sys/timeb.h>',
}
INCLUDE_RE = re.compile(rb'^(\s*#\s*include\s*)"([^"]+)"', re.M)


def listing(directory, cache={}):
    if directory not in cache:
        try:
            cache[directory] = {name.lower(): name for name in os.listdir(directory)}
        except OSError:
            cache[directory] = {}
    return cache[directory]


def resolve(base, rel):
    parts = [p for p in rel.split('/') if p not in ('', '.')]
    current = base
    out = []
    for part in parts:
        if part == '..':
            current = os.path.dirname(current)
            out.append('..')
            continue
        actual = listing(current).get(part.lower())
        if actual is None:
            return None
        current = os.path.join(current, actual)
        out.append(actual)
    return '/'.join(out)


def fix_file(path, search_dirs):
    data = open(path, 'rb').read()

    def repl(match):
        prefix, target = match.group(1), match.group(2).decode('latin-1')
        norm = target.replace('\\', '/')
        while norm.startswith('./'):
            norm = norm[2:]
        special = SPECIAL.get(norm.lower())
        if special:
            return prefix + special.encode()
        for base in [os.path.dirname(path)] + search_dirs:
            resolved = resolve(base, norm)
            if resolved:
                return prefix + ('"%s"' % resolved).encode('latin-1')
        if norm != target:
            return prefix + ('"%s"' % norm).encode('latin-1')
        return match.group(0)

    new = INCLUDE_RE.sub(repl, data)
    if new != data:
        open(path, 'wb').write(new)
        return True
    return False


def main():
    search = [os.path.join(REPO, 'CrypTool')]
    changed = 0
    for d in DIRS:
        full = os.path.join(REPO, d)
        for name in sorted(os.listdir(full)):
            if name.lower().endswith(('.cpp', '.c', '.h', '.hpp', '.cc', '.inl')):
                if fix_file(os.path.join(full, name), search):
                    changed += 1
    print('changed', changed, 'files', file=sys.stderr)


if __name__ == '__main__':
    main()
