#!/usr/bin/env python3
"""Builds the help index used by the mfcwx HTML Help replacement from an HTML Help Workshop project.

The index maps HH_HELP_CONTEXT ids to topic files (the .hhp [MAP]/[ALIAS] sections, with
CrypTool_helpIDs.h recreated from resource.h like makehm + hh_generator.pl do), ALink and keyword
names to the pages declaring them, and records the default topic, contents and index files.
It also creates hid_load_readme.html from the readme text (like readme.pl).

  build_help_index.py --help-dir CrypTool/hlp_en --resource-header CrypTool/resource.h
                      --readme setup/template-en/ReadMe-en.txt --out-dir build/data/hlp_en [--mirror]

--mirror turns the output directory into a copy of the help directory made of symbolic links
(development data directory); otherwise only the generated files are written.
"""
import argparse
import html
import os
import re
import shutil
import sys

INDEX_NAME = 'CrypTool.helpindex'
README_NAME = 'hid_load_readme.html'
README_TEMPLATE = README_NAME + '.in'
MIRROR_EXCLUDE = re.compile(r'\.(vcproj|vcxproj.*|mak|dsp)$', re.I)

# The makehm calls of CrypTool/MakeHtmlHelp.bat.
MAKEHM_RULES = [('ID_', 'HID_', 0x10000), ('IDM_', 'HIDM_', 0x10000), ('IDP_', 'HIDP_', 0x30000),
                ('IDR_', 'HIDR_', 0x20000), ('IDD_', 'HIDD_', 0x20000), ('IDW_', 'HIDW_', 0x50000)]

DEFINE_RE = re.compile(r'^\s*#\s*define\s+(\w+)\s+(0[xX][0-9a-fA-F]+|\d+)[uUlL]*\s*(?:$|//|/\*)')
MAP_LINE_RE = re.compile(r'^\s*(\w+)\s+(0[xX][0-9a-fA-F]+|\d+)\s*$')
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+[<"]?([^>"\s]+)[>"]?\s*$', re.I)
ALIAS_RE = re.compile(r'^\s*(\w+)\s*=\s*(\S+)\s*$')
CHARSET_RE = re.compile(rb'<meta[^>]+charset\s*=\s*["\']?([\w-]+)', re.I)
TITLE_RE = re.compile(r'<title[^>]*>(.*?)</title\s*>', re.I | re.S)
PARAM_RE = re.compile(r'<param\s+([^>]*)>', re.I)
ATTR_RE = re.compile(r'(\w+)\s*=\s*(?:"([^"]*)"|\'([^\']*)\'|([^\s>]+))')


def read_text(path):
    with open(path, 'rb') as f:
        return f.read().decode('latin-1')


class Listing:
    """Case-insensitive view of a directory: topic names in the project are not reliably cased."""

    def __init__(self, directory, extra=()):
        self.directory = directory
        self.names = {}
        for name in sorted(os.listdir(directory)):
            self.names.setdefault(name.lower(), name)
        for name in extra:
            self.names.setdefault(name.lower(), name)

    def find(self, name):
        return self.names.get(name.replace('\\', '/').lower())

    def path(self, name):
        real = self.find(name)
        return os.path.join(self.directory, real) if real else None


def parse_hhp(path):
    sections = {}
    current = None
    for line in read_text(path).splitlines():
        stripped = line.strip()
        if stripped.startswith('[') and stripped.endswith(']'):
            current = stripped[1:-1].upper()
            sections.setdefault(current, [])
        elif current is not None and stripped:
            sections[current].append(stripped)
    options = {}
    for line in sections.get('OPTIONS', []):
        key, sep, value = line.partition('=')
        if sep:
            options[key.strip().lower()] = value.strip()
    return sections, options


def window_caption(sections, name):
    for line in sections.get('WINDOWS', []):
        key, sep, value = line.partition('=')
        if sep and key.strip().lower() == name.lower():
            m = re.match(r'\s*"([^"]*)"', value)
            if m:
                return m.group(1)
    return ''


def parse_defines(text):
    for line in text.splitlines():
        m = DEFINE_RE.match(line)
        if m:
            yield m.group(1), int(m.group(2), 0)


def makehm(resource_header):
    """CrypTool_helpIDs.h as produced by makehm + hh_generator.pl from resource.h."""
    out = []
    for name, value in parse_defines(read_text(resource_header)):
        for prefix, hprefix, offset in MAKEHM_RULES:
            if name.startswith(prefix):
                out.append((hprefix + name[len(prefix):], value + offset))
                break
    return out


def collect_map(sections, listing, resource_header, problems):
    defines = []
    for line in sections.get('MAP', []):
        m = INCLUDE_RE.match(line)
        if m:
            path = listing.path(m.group(1))
            if path:
                defines += list(parse_defines(read_text(path)))
            elif m.group(1).lower() == 'cryptool_helpids.h' and resource_header:
                defines += makehm(resource_header)
            else:
                problems.append('[MAP] include not found: %s' % m.group(1))
            continue
        m = DEFINE_RE.match(line) or MAP_LINE_RE.match(line)
        if m:
            defines.append((m.group(1), int(m.group(2), 0)))
    return defines


def collect_aliases(sections, listing, problems):
    aliases = {}

    def add_lines(lines):
        for line in lines:
            if line.startswith(';') or line.startswith('//'):
                continue
            m = ALIAS_RE.match(line)
            if m:
                aliases.setdefault(m.group(1).upper(), m.group(2))

    for line in sections.get('ALIAS', []):
        m = INCLUDE_RE.match(line)
        if m:
            path = listing.path(m.group(1))
            if path:
                add_lines(read_text(path).splitlines())
            else:
                problems.append('[ALIAS] include not found: %s' % m.group(1))
        else:
            add_lines([line])
    return aliases


def resolve_topic(listing, target):
    target = target.replace('\\', '/')
    page, sep, anchor = target.partition('#')
    real = listing.find(page)
    if not real:
        base, ext = os.path.splitext(page)
        alternative = {'.htm': '.html', '.html': '.htm'}.get(ext.lower())
        real = listing.find(base + alternative) if alternative else None
    return real + sep + anchor if real else None


def decode_page(data):
    m = CHARSET_RE.search(data)
    encoding = m.group(1).decode('ascii').lower() if m else 'cp1252'
    try:
        return data.decode(encoding)
    except (LookupError, UnicodeDecodeError):
        return data.decode('cp1252', errors='replace')


def clean(value):
    return ' '.join(html.unescape(value).split())


def scan_pages(listing, generated):
    titles, alinks, keywords = {}, [], []
    for real in sorted(set(listing.names.values())):
        if not re.search(r'\.html?$', real, re.I):
            continue
        source = generated.get(real) or os.path.join(listing.directory, real)
        with open(source, 'rb') as f:
            text = decode_page(f.read())
        m = TITLE_RE.search(text)
        titles[real] = clean(re.sub(r'<[^>]*>', '', m.group(1))) if m else ''
        for pm in PARAM_RE.finditer(text):
            attrs = {a.group(1).lower(): next(v for v in a.groups()[1:] if v is not None)
                     for a in ATTR_RE.finditer(pm.group(1))}
            name = attrs.get('name', '').strip().lower()
            value = clean(attrs.get('value', ''))
            if not value:
                continue
            if name == 'alink name':
                alinks.append((value, real))
            elif name == 'keyword':
                keywords.append((value, real))
    return titles, alinks, keywords


def make_readme(template_path, readme_path):
    if not os.path.exists(readme_path):
        readme_path = re.sub(r'-..([./\\])', r'-en\1', readme_path)
    text = read_text(readme_path).replace('\r\n', '\n').replace('\r', '\n')
    text = text.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
    text = re.sub(r'^( *)([.A-Z0-9]+)( +\.\.\.\.? +)(.*)$', r'\1\2\3<a href="#s\2">\4</a>', text, flags=re.M)
    text = re.sub(r'^([.A-Z0-9]+\.) ([^.].*)', r'<h2><a name="s\1">\1</a> \2</h2>', text, flags=re.M)
    text = re.sub(r'----*', '', text)
    return read_text(template_path).replace('__BODY__', text, 1).encode('latin-1')


def tsv(*fields):
    return '\t'.join(f.replace('\t', ' ').replace('\n', ' ') for f in fields) + '\n'


def build(args):
    help_dir = os.path.abspath(args.help_dir)
    problems = []
    generated = {}
    template = os.path.join(help_dir, README_TEMPLATE)
    if args.readme and os.path.exists(template):
        generated[README_NAME] = make_readme(template, args.readme)

    listing = Listing(help_dir, extra=generated)
    hhp_name = args.project or next((n for n in listing.names.values() if n.lower().endswith('.hhp')), None)
    if not hhp_name:
        sys.exit('build_help_index: no .hhp project in %s' % help_dir)
    sections, options = parse_hhp(os.path.join(help_dir, hhp_name))

    defines = collect_map(sections, listing, args.resource_header, problems)
    aliases = collect_aliases(sections, listing, problems)

    candidates = {}
    unresolved = []
    for name, value in defines:
        target = aliases.get(name.upper())
        topic = resolve_topic(listing, target if target else name.lower() + '.htm')
        if topic:
            entries = candidates.setdefault(value, [])
            if (name, topic) not in entries:
                entries.append((name, topic))
        else:
            unresolved.append((name, value, target))

    contexts = {}
    collisions = []
    for value, entries in candidates.items():
        if len({t for _, t in entries}) > 1:
            collisions.append((value, entries))
        # AFX_HIDD_* and HIDR_* ids can collide with CrypTool's dialogs and commands, which F1 asks for
        best = sorted(entries, key=lambda e: 0 if e[0].startswith(('HIDD_', 'HID_')) else 1)[0]
        contexts[value] = best[1]

    out_dir = os.path.abspath(args.out_dir)
    prepare_out_dir(out_dir, help_dir, args.mirror, generated)
    generated_paths = {}
    for name, data in generated.items():
        generated_paths[name] = os.path.join(out_dir, name)
        write(generated_paths[name], data)

    titles, alinks, keywords = scan_pages(listing, generated_paths)

    default_topic = resolve_topic(listing, options.get('default topic', '')) or ''
    caption = window_caption(sections, options.get('default window', '')) or options.get('title', '')
    lines = [tsv('CrypToolHelpIndex', '1'),
             tsv('title', caption),
             tsv('default', default_topic)]
    for key, kind in (('contents file', 'contents'), ('index file', 'index')):
        real = listing.find(options.get(key, ''))
        if real:
            lines.append(tsv(kind, real))
    for value in sorted(contexts):
        lines.append(tsv('context', str(value), contexts[value]))
    for keyword, page in sorted(set(alinks), key=lambda e: (e[0].lower(), e[1])):
        lines.append(tsv('alink', keyword, page))
    for keyword, page in sorted(set(keywords), key=lambda e: (e[0].lower(), e[1])):
        lines.append(tsv('keyword', keyword, page))
    for page in sorted(titles):
        lines.append(tsv('topic', page, titles[page]))
    write(os.path.join(out_dir, INDEX_NAME), ''.join(lines).encode('utf-8'))

    if args.stats or args.verbose:
        print('%s: %d context ids mapped (%d MAP symbols, %d unresolved, %d ids with conflicting topics), '
              '%d ALinks (%d names), %d keywords, %d pages, default topic %s' % (
                  os.path.basename(help_dir), len(contexts), len(defines), len(unresolved), len(collisions),
                  len(set(alinks)), len({a.lower() for a, _ in alinks}), len(set(keywords)), len(titles),
                  default_topic or '-'))
    if args.verbose:
        for name, value, target in unresolved:
            print('  unresolved: %s (0x%X)%s' % (name, value, ' -> ' + target if target else ''))
        for value, entries in collisions:
            print('  conflict 0x%X: %s' % (value, ', '.join('%s=%s' % e for e in entries)))
        for p in problems:
            print('  ' + p)


def write(path, data):
    with open(path, 'wb') as f:
        f.write(data)


def prepare_out_dir(out_dir, help_dir, mirror, generated):
    if mirror and os.path.islink(out_dir):
        os.unlink(out_dir)
    os.makedirs(out_dir, exist_ok=True)
    if os.path.realpath(out_dir) == os.path.realpath(help_dir):
        sys.exit('build_help_index: refusing to write into the help sources (%s)' % help_dir)
    if not mirror:
        return
    wanted = {n for n in os.listdir(help_dir) if not MIRROR_EXCLUDE.search(n) and n not in generated}
    for name in os.listdir(out_dir):
        path = os.path.join(out_dir, name)
        if os.path.islink(path) and (name not in wanted or not os.path.exists(path)):
            os.unlink(path)
    for name in sorted(wanted):
        link = os.path.join(out_dir, name)
        if os.path.lexists(link):
            continue
        src = os.path.join(help_dir, name)
        try:
            os.symlink(src, link)
        except OSError:
            (shutil.copytree if os.path.isdir(src) else shutil.copy2)(src, link)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--help-dir', required=True, help='HTML Help Workshop project directory (hlp_en, ...)')
    p.add_argument('--project', help='.hhp file name (default: the only .hhp in --help-dir)')
    p.add_argument('--resource-header', help='resource.h to recreate CrypTool_helpIDs.h from')
    p.add_argument('--readme', help='readme text for hid_load_readme.html')
    p.add_argument('--out-dir', required=True)
    p.add_argument('--mirror', action='store_true', help='link the help files into --out-dir')
    p.add_argument('--stats', action='store_true', help='print a summary')
    p.add_argument('--verbose', action='store_true', help='print the summary and every problem')
    build(p.parse_args())


if __name__ == '__main__':
    main()
