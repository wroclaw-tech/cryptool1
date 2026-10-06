#!/usr/bin/env python3
"""Convert a Windows resource script (.rc) into C++ tables for mfcwx::rc (see mfcwx/rcdata.h)."""

import argparse
import codecs
import os
import re
import sys

WS_POPUP = 0x80000000
WS_CHILD = 0x40000000
WS_VISIBLE = 0x10000000
WS_CAPTION = 0x00C00000
WS_BORDER = 0x00800000
WS_SYSMENU = 0x00080000
WS_GROUP = 0x00020000
WS_TABSTOP = 0x00010000
DS_SETFONT = 0x40
SS_LEFT, SS_CENTER, SS_RIGHT, SS_ICON = 0, 1, 2, 3
(BS_PUSHBUTTON, BS_DEFPUSHBUTTON, BS_CHECKBOX, BS_AUTOCHECKBOX, BS_RADIOBUTTON, BS_3STATE,
 BS_AUTO3STATE, BS_GROUPBOX, BS_USERBUTTON, BS_AUTORADIOBUTTON, BS_PUSHBOX) = range(11)
ES_LEFT = 0
CBS_SIMPLE = 0x1
CBS_TYPEMASK = 0x3
LBS_NOTIFY = 0x1
SBS_HORZ = 0

MF_GRAYED = 0x1
MF_DISABLED = 0x2
MF_CHECKED = 0x8
MF_POPUP = 0x10
MF_MENUBARBREAK = 0x20
MF_MENUBREAK = 0x40
MF_SEPARATOR = 0x800
MF_HELP = 0x4000

FVIRTKEY = 0x01
FNOINVERT = 0x02
FSHIFT = 0x04
FCONTROL = 0x08
FALT = 0x10

CHILD = WS_CHILD | WS_VISIBLE

TEXT_CONTROLS = {
    'LTEXT': ('Static', SS_LEFT | WS_GROUP),
    'RTEXT': ('Static', SS_RIGHT | WS_GROUP),
    'CTEXT': ('Static', SS_CENTER | WS_GROUP),
    'PUSHBUTTON': ('Button', BS_PUSHBUTTON | WS_TABSTOP),
    'DEFPUSHBUTTON': ('Button', BS_DEFPUSHBUTTON | WS_TABSTOP),
    'PUSHBOX': ('Button', BS_PUSHBOX | WS_TABSTOP),
    'USERBUTTON': ('Button', BS_USERBUTTON | WS_TABSTOP),
    'CHECKBOX': ('Button', BS_CHECKBOX | WS_TABSTOP),
    'AUTOCHECKBOX': ('Button', BS_AUTOCHECKBOX | WS_TABSTOP),
    'STATE3': ('Button', BS_3STATE | WS_TABSTOP),
    'AUTO3STATE': ('Button', BS_AUTO3STATE | WS_TABSTOP),
    'RADIOBUTTON': ('Button', BS_RADIOBUTTON),
    'AUTORADIOBUTTON': ('Button', BS_AUTORADIOBUTTON),
    'GROUPBOX': ('Button', BS_GROUPBOX),
}

PLAIN_CONTROLS = {
    'EDITTEXT': ('Edit', ES_LEFT | WS_BORDER | WS_TABSTOP),
    'COMBOBOX': ('ComboBox', WS_TABSTOP),
    'LISTBOX': ('ListBox', LBS_NOTIFY | WS_BORDER),
    'SCROLLBAR': ('ScrollBar', SBS_HORZ),
}

CLASS_NAMES = {'BUTTON': 'Button', 'EDIT': 'Edit', 'STATIC': 'Static', 'LISTBOX': 'ListBox',
               'SCROLLBAR': 'ScrollBar', 'COMBOBOX': 'ComboBox'}
CLASS_ORDINALS = {0x80: 'Button', 0x81: 'Edit', 0x82: 'Static', 0x83: 'ListBox', 0x84: 'ScrollBar',
                  0x85: 'ComboBox'}

MENU_OPTIONS = {'GRAYED': MF_GRAYED, 'INACTIVE': MF_DISABLED, 'CHECKED': MF_CHECKED,
                'MENUBARBREAK': MF_MENUBARBREAK, 'MENUBREAK': MF_MENUBREAK, 'HELP': MF_HELP}
ACCEL_OPTIONS = {'VIRTKEY': FVIRTKEY, 'ASCII': 0, 'NOINVERT': FNOINVERT, 'SHIFT': FSHIFT,
                 'CONTROL': FCONTROL, 'ALT': FALT}

MEMORY_OPTIONS = {'DISCARDABLE', 'MOVEABLE', 'FIXED', 'PURE', 'IMPURE', 'PRELOAD', 'LOADONCALL',
                  'SHARED', 'NONSHARED'}
FILE_TYPES = {'BITMAP', 'ICON', 'CURSOR', 'FONT', 'FONTDIR', 'HTML', 'MESSAGETABLE', 'ANICURSOR',
              'ANIICON', 'MANIFEST', 'PLUGPLAY', 'VXD', 'RCDATA', 'DLGINCLUDE'}
SKIP_TYPES = {'TEXTINCLUDE', 'DESIGNINFO', 'AFX_DIALOG_LAYOUT'}
TOP_TYPES = {'DIALOG', 'DIALOGEX', 'MENU', 'MENUEX', 'ACCELERATORS', 'TOOLBAR', 'DLGINIT',
             'VERSIONINFO'} | FILE_TYPES | SKIP_TYPES
RT_NAMES = {1: 'CURSOR', 2: 'BITMAP', 3: 'ICON', 4: 'MENU', 5: 'DIALOG', 6: 'STRING', 7: 'FONTDIR',
            8: 'FONT', 9: 'ACCELERATOR', 10: 'RCDATA', 11: 'MESSAGETABLE', 12: 'GROUP_CURSOR',
            14: 'GROUP_ICON', 16: 'VERSION', 17: 'DLGINCLUDE', 19: 'PLUGPLAY', 20: 'VXD',
            21: 'ANICURSOR', 22: 'ANIICON', 23: 'HTML', 24: 'MANIFEST', 240: 'DLGINIT', 241: 'TOOLBAR'}

DLGINIT_TEXT_MESSAGES = {0x0401, 0x0403, 0x1234}

BUILTIN_SYMBOLS = {
    'LANG_NEUTRAL': 0x00, 'LANG_CHINESE': 0x04, 'LANG_CZECH': 0x05, 'LANG_DANISH': 0x06,
    'LANG_GERMAN': 0x07, 'LANG_GREEK': 0x08, 'LANG_ENGLISH': 0x09, 'LANG_SPANISH': 0x0a,
    'LANG_FINNISH': 0x0b, 'LANG_FRENCH': 0x0c, 'LANG_HUNGARIAN': 0x0e, 'LANG_ITALIAN': 0x10,
    'LANG_JAPANESE': 0x11, 'LANG_KOREAN': 0x12, 'LANG_DUTCH': 0x13, 'LANG_NORWEGIAN': 0x14,
    'LANG_POLISH': 0x15, 'LANG_PORTUGUESE': 0x16, 'LANG_RUSSIAN': 0x19, 'LANG_CROATIAN': 0x1a,
    'LANG_SERBIAN': 0x1a, 'LANG_SWEDISH': 0x1d, 'LANG_TURKISH': 0x1f,
    'SUBLANG_NEUTRAL': 0x00, 'SUBLANG_DEFAULT': 0x01, 'SUBLANG_SYS_DEFAULT': 0x02,
    'SUBLANG_GERMAN': 0x01, 'SUBLANG_GERMAN_SWISS': 0x02, 'SUBLANG_GERMAN_AUSTRIAN': 0x03,
    'SUBLANG_ENGLISH_US': 0x01, 'SUBLANG_ENGLISH_UK': 0x02, 'SUBLANG_FRENCH': 0x01,
    'SUBLANG_FRENCH_SWISS': 0x04, 'SUBLANG_SPANISH': 0x01, 'SUBLANG_SPANISH_MODERN': 0x03,
    'SUBLANG_SERBIAN_LATIN': 0x02, 'SUBLANG_SERBIAN_CYRILLIC': 0x03, 'SUBLANG_POLISH_POLAND': 0x01,
    'SUBLANG_GREEK_GREECE': 0x01, 'SUBLANG_RUSSIAN_RUSSIA': 0x01,
    'VS_VERSION_INFO': 1,
}

LANG_CODE_BY_ID = {(0x07, 0x02): 'de-CH', (0x1a, 0x01): 'hr'}
LANG_CODE_BY_PRIMARY = {
    0x04: 'zh', 0x05: 'cs', 0x06: 'da', 0x07: 'de', 0x08: 'el', 0x09: 'en', 0x0a: 'es', 0x0b: 'fi',
    0x0c: 'fr', 0x0e: 'hu', 0x10: 'it', 0x11: 'ja', 0x12: 'ko', 0x13: 'nl', 0x14: 'no', 0x15: 'pl',
    0x16: 'pt', 0x19: 'ru', 0x1a: 'rs', 0x1d: 'sv', 0x1f: 'tr',
}

HEADER_ALIASES = {name: 'mfcwx/winconst.h' for name in (
    'windows.h', 'winres.h', 'winresrc.h', 'winuser.h', 'winver.h', 'commctrl.h', 'winnt.h',
    'windef.h', 'richedit.h', 'dlgs.h', 'winuser.rh', 'commctrl.rh', 'winnt.rh')}
RC_EXTENSIONS = {'.rc', '.rc2', '.rc3', '.dlg'}

CAST_TYPES = {
    'BYTE': (8, False), 'UCHAR': (8, False), 'unsigned char': (8, False), 'char': (8, True),
    'WORD': (16, False), 'USHORT': (16, False), 'WCHAR': (16, False), 'ATOM': (16, False),
    'unsigned short': (16, False), 'SHORT': (16, True), 'short': (16, True),
    'DWORD': (32, False), 'UINT': (32, False), 'ULONG': (32, False), 'unsigned': (32, False),
    'unsigned int': (32, False), 'unsigned long': (32, False), 'COLORREF': (32, False),
    'LCID': (32, False), 'int': (32, True), 'INT': (32, True), 'LONG': (32, True),
    'long': (32, True), 'BOOL': (32, True),
}

PREC = {'||': 1, '&&': 2, '|': 3, '^': 4, '&': 5, '==': 6, '!=': 6, '<': 7, '>': 7, '<=': 7,
        '>=': 7, '<<': 8, '>>': 8, '+': 9, '-': 9, '*': 10, '/': 10, '%': 10}


class Tok:
    __slots__ = ('k', 'v', 's', 'x', 'cp', 'ln', 'fi')

    def __init__(self, k, v, s, x, cp, ln, fi):
        self.k = k
        self.v = v
        self.s = s
        self.x = x
        self.cp = cp
        self.ln = ln
        self.fi = fi


EOF_TOK = Tok('eof', None, '<end of input>', None, 0, 0, 0)

LEX_RE = re.compile(r'''
    (?P<ws>[ \t\f\v\x00\x1a]+)
  | (?P<lc>//.*)
  | (?P<bc>/\*)
  | (?P<s>L?"(?:[^"\\]|\\.|"")*")
  | (?P<su>L?"(?:[^"\\]|\\.|"")*)
  | (?P<n>0[xX][0-9A-Fa-f]*[uUlL]*|[0-9]+[uUlL]*)
  | (?P<i>[A-Za-z_][A-Za-z0-9_]*)
  | (?P<o><<|>>|<=|>=|==|!=|&&|\|\||.)
''', re.X)


def lex(text, out, cp, ln, fi, unterminated=None):
    pos = 0
    n = len(text)
    match = LEX_RE.match
    append = out.append
    while pos < n:
        m = match(text, pos)
        g = m.lastgroup
        pos = m.end()
        if g == 'ws':
            continue
        if g == 'lc':
            break
        if g == 'bc':
            e = text.find('*/', pos)
            if e < 0:
                return True
            pos = e + 2
            continue
        s = m.group()
        if g == 'i':
            append(Tok('id', s, s, None, cp, ln, fi))
        elif g == 'n':
            body = s.rstrip('uUlL')
            suffix = s[len(body):].lower()
            if body[:2] in ('0x', '0X'):
                v = int(body[2:], 16) if len(body) > 2 else 0
            else:
                v = int(body)
            append(Tok('num', v, s, 'l' in suffix, cp, ln, fi))
        elif g == 's':
            wide = s[0] == 'L'
            append(Tok('str', s[2 if wide else 1:-1], s, wide, cp, ln, fi))
        elif g == 'su':
            wide = s[0] == 'L'
            if unterminated is not None:
                unterminated(ln, fi)
            append(Tok('str', s[2 if wide else 1:], s, wide, cp, ln, fi))
        else:
            append(Tok('op', s, s, None, cp, ln, fi))
    return False


def describe(t):
    if t.k == 'eof':
        return 'end of input'
    if t.k == 'str':
        return 'string'
    return "'%s'" % t.s


class Diag:
    def __init__(self, quiet):
        self.quiet = quiet
        self.errors = 0
        self.warnings = 0

    def error(self, loc, msg):
        self.errors += 1
        print('%s: error: %s' % (loc, msg), file=sys.stderr)

    def warn(self, loc, msg):
        self.warnings += 1
        if not self.quiet:
            print('%s: warning: %s' % (loc, msg), file=sys.stderr)

    def note(self, msg):
        if not self.quiet:
            print('rc2cpp: %s' % msg, file=sys.stderr)


class EvalError(Exception):
    pass


class ParseError(Exception):
    def __init__(self, tok, msg):
        Exception.__init__(self, msg)
        self.tok = tok
        self.msg = msg


class Cursor:
    __slots__ = ('t', 'i', 'n', 'eof')

    def __init__(self, toks, eof=None):
        self.t = toks
        self.i = 0
        self.n = len(toks)
        self.eof = eof or EOF_TOK

    def peek(self, k=0):
        j = self.i + k
        return self.t[j] if j < self.n else self.eof

    def next(self):
        i = self.i
        if i < self.n:
            self.i = i + 1
            return self.t[i]
        return self.eof

    def at_eof(self):
        return self.i >= self.n


def apply_cast(name, v):
    spec = CAST_TYPES.get(name)
    if spec is None:
        return v
    bits, signed = spec
    v &= (1 << bits) - 1
    if signed and v >> (bits - 1):
        v -= 1 << bits
    return v


def binop(o, a, b):
    if o == '|':
        return a | b
    if o == '&':
        return a & b
    if o == '+':
        return a + b
    if o == '-':
        return a - b
    if o == '^':
        return a ^ b
    if o == '<<':
        return a << (b & 63)
    if o == '>>':
        return a >> (b & 63)
    if o == '*':
        return a * b
    if o in ('/', '%'):
        if b == 0:
            raise EvalError('division by zero')
        q = abs(a) // abs(b)
        if (a < 0) != (b < 0):
            q = -q
        return q if o == '/' else a - q * b
    if o == '==':
        return int(a == b)
    if o == '!=':
        return int(a != b)
    if o == '<':
        return int(a < b)
    if o == '>':
        return int(a > b)
    if o == '<=':
        return int(a <= b)
    if o == '>=':
        return int(a >= b)
    if o == '&&':
        return int(bool(a) and bool(b))
    if o == '||':
        return int(bool(a) or bool(b))
    raise EvalError("unsupported operator '%s'" % o)


class Evaluator:
    def __init__(self, macros):
        self.macros = macros
        self.expanding = set()
        self.cache = None
        self.saw_long = False
        self.undefined = {}
        self.undefined_hits = 0
        self.builtins_used = set()

    def freeze(self):
        self.cache = {}

    def eval_pp(self, toks):
        cur = Cursor(toks)
        v = self.expr(cur, 1, True)
        if not cur.at_eof():
            raise EvalError('unexpected %s in #if expression' % describe(cur.peek()))
        return v

    def expr(self, cur, min_prec=1, pp=False):
        v = self.unary(cur, pp)
        while True:
            t = cur.peek()
            if t.k != 'op':
                return v
            p = PREC.get(t.v)
            if p is None or p < min_prec:
                return v
            cur.next()
            v = binop(t.v, v, self.expr(cur, p + 1, pp))

    def expect_close(self, cur):
        t = cur.next()
        if t.k != 'op' or t.v != ')':
            raise EvalError("expected ')' but found %s" % describe(t))

    def unary(self, cur, pp=False):
        t = cur.next()
        k = t.k
        if k == 'num':
            if t.x:
                self.saw_long = True
            return t.v
        if k == 'id':
            if pp and t.v == 'defined':
                nt = cur.peek()
                paren = nt.k == 'op' and nt.v == '('
                if paren:
                    cur.next()
                n = cur.next()
                if n.k != 'id':
                    raise EvalError("expected identifier after 'defined'")
                if paren:
                    self.expect_close(cur)
                return 1 if n.v in self.macros else 0
            if not pp and t.v.upper() == 'NOT' and t.v not in self.macros:
                return ~self.unary(cur, pp)
            return self.ident(t, cur, pp)
        if k == 'op':
            o = t.v
            if o == '(':
                cast = self.try_cast(cur)
                if cast is not None:
                    return apply_cast(cast, self.unary(cur, pp))
                v = self.expr(cur, 1, pp)
                self.expect_close(cur)
                return v
            if o == '-':
                return -self.unary(cur, pp)
            if o == '+':
                return self.unary(cur, pp)
            if o == '~':
                return ~self.unary(cur, pp)
            if o == '!':
                return 0 if self.unary(cur, pp) else 1
        if k == 'eof':
            raise EvalError('unexpected end of expression')
        raise EvalError('unexpected %s in expression' % describe(t))

    def is_object_macro(self, name):
        m = self.macros.get(name)
        return m is not None and m[0] is None

    def is_symbol(self, name):
        return self.is_object_macro(name) or name in BUILTIN_SYMBOLS

    def try_cast(self, cur):
        j = 0
        words = []
        while True:
            t = cur.peek(j)
            if t.k == 'id' and not self.is_symbol(t.v):
                words.append(t.v)
            elif t.k == 'op' and t.v == '*' and words:
                words.append('*')
            else:
                break
            j += 1
        if not words:
            return None
        t = cur.peek(j)
        if t.k != 'op' or t.v != ')':
            return None
        a = cur.peek(j + 1)
        if not (a.k in ('num', 'id') or (a.k == 'op' and a.v in ('(', '-', '~', '+', '!'))):
            return None
        for _ in range(j + 1):
            cur.next()
        return ' '.join(words)

    def collect_args(self, cur):
        cur.next()
        args = [[]]
        depth = 1
        while True:
            t = cur.next()
            if t.k == 'eof':
                raise EvalError('unterminated macro argument list')
            if t.k == 'op':
                if t.v == '(':
                    depth += 1
                elif t.v == ')':
                    depth -= 1
                    if depth == 0:
                        break
                elif t.v == ',' and depth == 1:
                    args.append([])
                    continue
            args[-1].append(t)
        return args

    def ident(self, t, cur, pp):
        name = t.v
        m = self.macros.get(name)
        if m is None:
            if pp:
                return 0
            b = BUILTIN_SYMBOLS.get(name)
            if b is not None:
                self.builtins_used.add(name)
                return b
            self.undefined_hits += 1
            rec = self.undefined.get(name)
            if rec is None:
                self.undefined[name] = [1, t]
            else:
                rec[0] += 1
            return 0
        params, body = m
        if params is not None:
            nt = cur.peek()
            if nt.k != 'op' or nt.v != '(':
                if not pp:
                    self.undefined_hits += 1
                    self.undefined.setdefault(name, [0, t])[0] += 1
                return 0
            args = self.collect_args(cur)
            index = {p: i for i, p in enumerate(params)}
            expanded = []
            for bt in body:
                if bt.k == 'id' and bt.v in index and index[bt.v] < len(args):
                    expanded.extend(args[index[bt.v]])
                else:
                    expanded.append(bt)
            body = expanded
        elif self.cache is not None and not pp:
            c = self.cache.get(name)
            if c is not None:
                if c[1]:
                    self.saw_long = True
                return c[0]
        if name in self.expanding:
            raise EvalError("recursive macro '%s'" % name)
        self.expanding.add(name)
        saved = self.saw_long
        self.saw_long = False
        try:
            if body:
                sub = Cursor(body)
                v = self.expr(sub, 1, pp)
                if not sub.at_eof():
                    raise EvalError("macro '%s' is not a constant expression" % name)
            else:
                v = 0
        finally:
            self.expanding.discard(name)
            long_seen = self.saw_long
            self.saw_long = saved or long_seen
        if params is None and self.cache is not None and not pp:
            self.cache[name] = (v, long_seen)
        return v


def codec_for(cp):
    if cp == 65001:
        return 'utf-8'
    name = 'cp%d' % cp
    try:
        codecs.lookup(name)
        return name
    except LookupError:
        return None


STRING_ESC_RE = re.compile(r'""|\\(?:[0-7]{1,3}|[xX][0-9A-Fa-f]{1,2}|.)')
STRING_ESC_WIDE_RE = re.compile(r'""|\\(?:[0-7]{1,3}|[xX][0-9A-Fa-f]{1,4}|.)')
SIMPLE_ESCAPES = {'n': '\n', 't': '\t', 'r': '\r', '\\': '\\', '"': '"',
                  'a': '\x08'}  # rc.exe maps \a to 0x08, not BEL


def _esc_value(e):
    if e == '""':
        return 34, False
    c = e[1]
    if c in SIMPLE_ESCAPES:
        return ord(SIMPLE_ESCAPES[c]), False
    if c in '01234567':
        return int(e[1:], 8), True
    if c in 'xX' and len(e) > 2:
        return int(e[2:], 16), True
    return None, False


def _narrow_sub(m):
    v, _ = _esc_value(m.group())
    if v is None:
        return m.group()
    return chr(v & 0xFF)


def string_bytes(raw, wide, cp):
    if not wide and cp != 1200:
        if '\\' in raw or '"' in raw:
            raw = STRING_ESC_RE.sub(_narrow_sub, raw)
        return raw.encode('latin-1')
    return decode_rc_string(raw, cp, wide).encode('utf-16-le')


def decode_rc_string(raw, cp, wide):
    if not wide and cp != 1200:
        b = string_bytes(raw, False, cp)
        return b.decode(codec_for(cp) or 'cp1252', 'replace')
    out = []
    pos = 0
    pending = []

    def flush():
        if pending:
            s = ''.join(pending)
            if cp == 1200:
                out.append(s)
            else:
                out.append(s.encode('latin-1').decode(codec_for(cp) or 'cp1252', 'replace'))
            del pending[:]

    for m in STRING_ESC_WIDE_RE.finditer(raw):
        pending.append(raw[pos:m.start()])
        pos = m.end()
        v, numeric = _esc_value(m.group())
        if v is None:
            pending.append(m.group())
        elif numeric:
            flush()
            out.append(chr(v))
        else:
            pending.append(chr(v))
    pending.append(raw[pos:])
    flush()
    return ''.join(out)


def read_source(path):
    data = open(path, 'rb').read()
    if data.startswith(b'\xff\xfe'):
        return data[2:].decode('utf-16-le', 'replace'), 1200
    if data.startswith(b'\xfe\xff'):
        return data[2:].decode('utf-16-be', 'replace'), 1200
    if data.startswith(b'\xef\xbb\xbf'):
        return data[3:].decode('latin-1'), 65001
    return data.decode('latin-1'), None


class DirCache:
    def __init__(self):
        self.cache = {}

    def names(self, d):
        r = self.cache.get(d)
        if r is None:
            try:
                r = os.listdir(d)
            except OSError:
                r = []
            self.cache[d] = r
        return r

    def resolve(self, base, rel):
        cur = base
        out = []
        for part in rel.split('/'):
            if part in ('', '.'):
                continue
            if part == '..':
                cur = os.path.dirname(cur)
                out.append('..')
                continue
            names = self.names(cur)
            if part in names:
                actual = part
            else:
                low = part.lower()
                matches = sorted(n for n in names if n.lower() == low)
                if not matches:
                    return None
                actual = matches[0]
            out.append(actual)
            cur = os.path.join(cur, actual)
        return out


class Preprocessor:
    def __init__(self, include_dirs, diag, default_cp, dirs):
        self.include_dirs = include_dirs
        self.diag = diag
        self.default_cp = default_cp
        self.cp = default_cp
        self.dirs = dirs
        self.macros = {}
        self.tokens = []
        self.files = []
        self.deps = []
        self.once = set()
        self.stack = []
        self.skipped_includes = []
        self.ev = Evaluator(self.macros)

    def define_cmdline(self, spec):
        name, _, value = spec.partition('=')
        toks = []
        lex(value if _ else '1', toks, 0, 0, 0)
        self.macros[name.strip()] = (None, toks)

    def find_include(self, name, cur_dir, angle):
        rel = name.replace('\\', '/')
        if os.path.isabs(rel):
            return rel if os.path.isfile(rel) else None
        dirs = []
        if not angle:
            dirs.append(cur_dir)
            dirs.extend(reversed(self.stack))
        dirs.extend(self.include_dirs)
        for d in dirs:
            p = os.path.join(d, rel)
            if os.path.isfile(p):
                return p
            parts = self.dirs.resolve(d, rel)
            if parts:
                p = os.path.join(d, *parts)
                if os.path.isfile(p):
                    return p
        alias = HEADER_ALIASES.get(os.path.basename(rel).lower())
        if alias and alias != rel:
            return self.find_include(alias, cur_dir, True)
        return None

    def process_file(self, path, rc_mode):
        real = os.path.realpath(path)
        if real in self.once:
            return
        if len(self.stack) > 40:
            self.diag.error(path, 'includes nested too deeply')
            return
        text, forced_cp = read_source(path)
        fi = len(self.files)
        self.files.append(path)
        self.deps.append(os.path.abspath(path))
        saved_cp = self.cp
        if forced_cp is not None:
            self.cp = forced_cp
        cur_dir = os.path.dirname(os.path.abspath(path))
        self.stack.append(cur_dir)
        diag = self.diag
        toks = self.tokens
        sink = []

        def unterminated(ln, fi_):
            diag.warn('%s:%d' % (self.files[fi_], ln), 'unterminated string literal')

        lines = text.split('\n')
        n = len(lines)
        cond = []
        active = True
        in_block = False
        i = 0
        while i < n:
            line = lines[i]
            ln = i + 1
            i += 1
            if line.endswith('\r'):
                line = line[:-1]
            if in_block:
                e = line.find('*/')
                if e < 0:
                    continue
                line = ' ' * (e + 2) + line[e + 2:]
                in_block = False
            stripped = line.lstrip()
            if stripped.startswith('#'):
                while line.endswith('\\') and i < n:
                    line = line[:-1] + lines[i].rstrip('\r')
                    i += 1
                loc = '%s:%d' % (path, ln)
                m = re.match(r'\s*#\s*([A-Za-z_]\w*)?(.*)$', line)
                name = m.group(1) or ''
                rest = m.group(2)
                if name in ('if', 'ifdef', 'ifndef'):
                    if not active:
                        cond.append([False, True, False])
                    else:
                        if name == 'if':
                            val = self.eval_if(rest, ln, fi, loc)
                        else:
                            mm = re.match(r'\s*([A-Za-z_]\w*)', rest)
                            if not mm:
                                diag.warn(loc, '#%s without identifier' % name)
                                val = False
                            else:
                                val = (mm.group(1) in self.macros) != (name == 'ifndef')
                        cond.append([val, val, True])
                    active = cond[-1][0]
                elif name == 'elif':
                    if not cond:
                        diag.warn(loc, '#elif without #if')
                        continue
                    top = cond[-1]
                    if top[2] and not top[1]:
                        top[0] = self.eval_if(rest, ln, fi, loc)
                        top[1] = top[0]
                    else:
                        top[0] = False
                    active = top[0]
                elif name == 'else':
                    if not cond:
                        diag.warn(loc, '#else without #if')
                        continue
                    top = cond[-1]
                    top[0] = top[2] and not top[1]
                    top[1] = True
                    active = top[0]
                elif name == 'endif':
                    if not cond:
                        diag.warn(loc, '#endif without #if')
                        continue
                    cond.pop()
                    active = cond[-1][0] if cond else True
                elif not active:
                    pass
                elif name == 'define':
                    mm = re.match(r'\s*([A-Za-z_]\w*)(\([^)]*\))?(.*)$', rest)
                    if not mm:
                        diag.warn(loc, 'malformed #define')
                        continue
                    params = None
                    if mm.group(2) is not None:
                        params = [p.strip() for p in mm.group(2)[1:-1].split(',') if p.strip()]
                    body = []
                    in_block = lex(mm.group(3), body, 0, ln, fi)
                    old = self.macros.get(mm.group(1))
                    if old is not None and (old[0] != params or [t.s for t in old[1]] != [t.s for t in body]):
                        diag.warn(loc, "macro '%s' redefined" % mm.group(1))
                    self.macros[mm.group(1)] = (params, body)
                    continue
                elif name == 'undef':
                    mm = re.match(r'\s*([A-Za-z_]\w*)', rest)
                    if mm:
                        self.macros.pop(mm.group(1), None)
                elif name == 'include':
                    mm = re.match(r'\s*(?:"([^"]*)"|<([^>]*)>)', rest)
                    if not mm:
                        diag.warn(loc, 'malformed #include')
                        continue
                    inc = mm.group(1) if mm.group(1) is not None else mm.group(2)
                    found = self.find_include(inc, cur_dir, mm.group(1) is None)
                    if found is None:
                        self.skipped_includes.append((loc, inc))
                    else:
                        ext = os.path.splitext(found)[1].lower()
                        self.process_file(found, rc_mode and ext in RC_EXTENSIONS)
                    rest = rest[mm.end():]
                elif name == 'pragma':
                    mm = re.match(r'\s*code_page\s*\(\s*(\w+)\s*\)', rest)
                    if mm:
                        v = mm.group(1)
                        if v.isdigit() and (int(v) == 1200 or codec_for(int(v))):
                            self.cp = int(v)
                        elif v.upper() == 'DEFAULT':
                            self.cp = self.default_cp
                        else:
                            diag.warn(loc, 'unsupported code page %s' % v)
                    elif re.match(r'\s*once\b', rest):
                        self.once.add(real)
                elif name == 'error':
                    diag.warn(loc, '#error%s' % rest)
                elif name in ('warning', 'line', ''):
                    pass
                else:
                    diag.warn(loc, 'ignoring unknown directive #%s' % name)
                if '/*' in rest:
                    del sink[:]
                    in_block = lex(rest, sink, 0, ln, fi)
                continue
            if active and rc_mode:
                in_block = lex(line, toks, self.cp, ln, fi, unterminated)
            elif '/*' in line:
                del sink[:]
                in_block = lex(line, sink, 0, ln, fi)
        if cond:
            diag.warn(path, 'unterminated #if at end of file')
        self.stack.pop()
        if forced_cp is not None:
            self.cp = saved_cp

    def eval_if(self, rest, ln, fi, loc):
        toks = []
        lex(rest, toks, 0, ln, fi)
        try:
            return bool(self.ev.eval_pp(toks))
        except EvalError as e:
            self.diag.warn(loc, 'cannot evaluate #if expression (%s); treating as false' % e)
            return False


def lang_code(primary, sub):
    if primary == 0:
        return 'neutral'
    c = LANG_CODE_BY_ID.get((primary, sub))
    if c:
        return c
    c = LANG_CODE_BY_PRIMARY.get(primary)
    if c:
        return c
    return 'lang%04x' % ((sub << 10) | primary)


def to_s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def to_s32(v):
    v &= 0xFFFFFFFF
    return v - 0x100000000 if v & 0x80000000 else v


def ctrl_id(v):
    v = to_s32(v)
    return -1 if v in (-1, 0xFFFF) else v


class Rec:
    def __init__(self, **kw):
        self.__dict__.update(kw)


class LangData:
    def __init__(self, code, langid):
        self.code = code
        self.langid = langid
        self.cp = None
        self.dialogs = []
        self.menus = []
        self.strings = {}
        self.accels = []
        self.toolbars = []
        self.files = []
        self.dlginits = []
        self.version = None
        self.keys = set()


def is_begin(t):
    return (t.k == 'id' and t.v.upper() == 'BEGIN') or (t.k == 'op' and t.v == '{')


def is_end(t):
    return (t.k == 'id' and t.v.upper() == 'END') or (t.k == 'op' and t.v == '}')


def kw(t):
    return t.v.upper() if t.k == 'id' else None


class RcParser:
    def __init__(self, pp, rc_path, diag, dirs):
        self.pp = pp
        self.ev = pp.ev
        self.diag = diag
        self.dirs = dirs
        last = pp.tokens[-1] if pp.tokens else EOF_TOK
        self.cur = Cursor(pp.tokens, Tok('eof', None, '<end of input>', None, 0, last.ln, last.fi))
        self.rc_dir = os.path.dirname(os.path.abspath(rc_path))
        self.langs = {}
        self.lang = None
        self.string_named = {}
        self.missing_files = {}
        self.skipped = []
        self.decode_errors = 0

    def loc(self, t):
        return '%s:%d' % (self.pp.files[t.fi] if self.pp.files else '?', t.ln)

    def parse(self):
        c = self.cur
        while c.peek().k != 'eof':
            start = c.i
            try:
                self.statement()
            except ParseError as e:
                self.diag.error(self.loc(e.tok), e.msg)
                self.recover(start)

    def recover(self, start):
        c = self.cur
        if c.i <= start:
            c.i = start + 1
        while c.peek().k != 'eof':
            t = c.peek()
            prev = c.t[c.i - 1]
            if prev.ln != t.ln or prev.fi != t.fi:
                u = kw(t)
                if u in ('STRINGTABLE', 'LANGUAGE'):
                    return
                t2 = c.peek(1)
                if t2.k == 'id' and t2.ln == t.ln and t2.v.upper() in TOP_TYPES and t.k in ('id', 'num', 'str'):
                    return
            c.next()

    def expr(self, min_prec=1):
        t = self.cur.peek()
        try:
            return self.ev.expr(self.cur, min_prec)
        except EvalError as e:
            raise ParseError(t, str(e))

    def expr_sym(self):
        start = self.cur.i
        t = self.cur.peek()
        v = self.expr()
        return v, (t.v if self.cur.i == start + 1 and t.k == 'id' else None)

    def unary(self):
        t = self.cur.peek()
        try:
            return self.ev.unary(self.cur)
        except EvalError as e:
            raise ParseError(t, str(e))

    def style_expr(self, value):
        c = self.cur
        while True:
            t = c.peek()
            if t.k == 'id' and t.v.upper() == 'NOT' and t.v not in self.ev.macros:
                c.next()
                value &= ~self.unary()
            else:
                value |= self.expr(PREC['^'])
            nt = c.peek()
            if nt.k == 'op' and nt.v == '|':
                c.next()
                continue
            return value & 0xFFFFFFFF

    def accept_op(self, v):
        t = self.cur.peek()
        if t.k == 'op' and t.v == v:
            self.cur.next()
            return True
        return False

    def expect_begin(self):
        t = self.cur.next()
        if not is_begin(t):
            raise ParseError(t, 'expected BEGIN but found %s' % describe(t))

    def text(self, t):
        s = decode_rc_string(t.v, t.cp, t.x)
        if '�' in s:
            self.decode_errors += 1
            self.diag.warn(self.loc(t), 'string contains bytes undefined in code page %d' % t.cp)
        return s

    def macro_string(self, t):
        m = self.ev.macros.get(t.v) if t.k == 'id' else None
        if m is not None and m[0] is None and len(m[1]) == 1 and m[1][0].k == 'str':
            return self.text(m[1][0])
        return None

    def string(self):
        t = self.cur.next()
        if t.k == 'str':
            return self.text(t)
        s = self.macro_string(t)
        if s is None:
            raise ParseError(t, 'expected string but found %s' % describe(t))
        return s

    def coords(self, n):
        out = []
        for i in range(n):
            if i:
                self.accept_op(',')
            out.append(to_s16(self.expr()))
        return out

    def skip_block(self):
        self.expect_begin()
        depth = 1
        while depth:
            t = self.cur.next()
            if t.k == 'eof':
                raise ParseError(t, 'unterminated BEGIN block')
            if is_begin(t):
                depth += 1
            elif is_end(t):
                depth -= 1

    def get_lang(self, primary, sub):
        primary &= 0x3FF
        sub &= 0x3F
        code = lang_code(primary, sub)
        L = self.langs.get(code)
        if L is None:
            L = LangData(code, (sub << 10) | primary if code != 'neutral' else 0)
            self.langs[code] = L
        return L

    def language_args(self):
        p = self.expr()
        self.accept_op(',')
        s = self.expr()
        return self.get_lang(p, s)

    def target(self, override, tok):
        L = override or self.lang
        if L is None:
            L = self.get_lang(0, 0)
        if L.cp is None:
            L.cp = tok.cp
        return L

    def add_unique(self, L, kind, rid, rname, tok):
        key = (kind, rid, rname.upper() if rname else None)
        if key in L.keys:
            self.diag.warn(self.loc(tok), 'duplicate %s resource %s in language %s ignored'
                           % (kind, rname or rid, L.code))
            return False
        L.keys.add(key)
        return True

    def resource_options(self):
        override = None
        c = self.cur
        while True:
            u = kw(c.peek())
            if u in MEMORY_OPTIONS:
                c.next()
            elif u == 'LANGUAGE':
                c.next()
                override = self.language_args()
            elif u in ('CHARACTERISTICS', 'VERSION'):
                c.next()
                self.expr()
            else:
                return override

    def name_ref(self):
        c = self.cur
        t = c.peek()
        if t.k == 'str' or self.macro_string(t) is not None:
            return 0, self.string(), None
        if t.k == 'id':
            if self.ev.is_symbol(t.v):
                return self.expr() & 0xFFFF, None, t.v
            c.next()
            self.string_named[t.v] = self.string_named.get(t.v, 0) + 1
            return 0, t.v, t.v
        if t.k == 'num' or (t.k == 'op' and t.v in ('(', '-', '~', '+')):
            return self.expr() & 0xFFFF, None, None
        raise ParseError(t, 'expected resource name but found %s' % describe(t))

    def statement(self):
        c = self.cur
        t = c.peek()
        u = kw(t)
        if u == 'LANGUAGE':
            c.next()
            self.lang = self.language_args()
            return
        if u == 'STRINGTABLE':
            c.next()
            self.parse_stringtable(t)
            return
        if u in ('VERSION', 'CHARACTERISTICS'):
            c.next()
            self.expr()
            return
        rid, rname, sym = self.name_ref()
        tt = c.next()
        if tt.k == 'id':
            ut = tt.v.upper()
            if ut in ('DIALOG', 'DIALOGEX'):
                return self.parse_dialog(rid, rname, sym, t, ut == 'DIALOGEX')
            if ut in ('MENU', 'MENUEX'):
                return self.parse_menu(rid, rname, sym, t, ut == 'MENUEX')
            if ut == 'ACCELERATORS':
                return self.parse_accelerators(rid, rname, sym, t)
            if ut == 'TOOLBAR':
                return self.parse_toolbar(rid, rname, sym, t)
            if ut == 'DLGINIT':
                return self.parse_dlginit(rid, rname, sym, t)
            if ut == 'VERSIONINFO':
                return self.parse_versioninfo(t)
            if ut in SKIP_TYPES:
                self.resource_options()
                if is_begin(c.peek()):
                    self.skip_block()
                return
            if ut in FILE_TYPES:
                typename = ut
            elif self.ev.is_symbol(tt.v):
                c.i -= 1
                v = self.expr() & 0xFFFF
                typename = RT_NAMES.get(v, '#%d' % v)
            else:
                typename = tt.v
        elif tt.k == 'str':
            typename = self.text(tt)
        elif tt.k == 'num':
            typename = RT_NAMES.get(tt.v, '#%d' % tt.v)
        else:
            raise ParseError(tt, 'expected resource type but found %s' % describe(tt))
        self.parse_file_resource(typename, rid, rname, sym, t)

    def parse_file_resource(self, typename, rid, rname, sym, tok):
        c = self.cur
        override = self.resource_options()
        t = c.peek()
        if t.k == 'str':
            path = self.string()
        elif is_begin(t):
            self.skip_block()
            self.skipped.append((self.loc(tok), '%s resource %s with inline data'
                                 % (typename, rname or sym or rid)))
            return
        else:
            parts = []
            while c.peek().k != 'eof' and c.peek().ln == t.ln and c.peek().fi == t.fi:
                parts.append(c.next().s)
            path = ''.join(parts)
            self.diag.warn(self.loc(t), 'unquoted file name %r' % path)
        L = self.target(override, tok)
        if not self.add_unique(L, typename, rid, rname, tok):
            return
        rel = path.replace('\\', '/')
        if os.path.isabs(rel) or re.match(r'^[A-Za-z]:/', rel):
            self.diag.warn(self.loc(tok), 'absolute resource path %r' % path)
            resolved = rel if os.path.exists(rel) else None
        else:
            parts = self.dirs.resolve(self.rc_dir, rel)
            resolved = '/'.join(parts) if parts else None
            if resolved is not None and not os.path.isfile(os.path.join(self.rc_dir, resolved)):
                resolved = None
        if resolved is None:
            resolved = '/'.join(p for p in rel.split('/') if p not in ('', '.'))
            if resolved not in self.missing_files:
                self.missing_files[resolved] = self.loc(tok)
                self.diag.warn(self.loc(tok), 'resource file not found: %s' % path)
        L.files.append(Rec(type=typename, id=rid, name=rname, path=resolved, sym=sym,
                           exists=resolved not in self.missing_files))

    def parse_stringtable(self, tok):
        c = self.cur
        override = self.resource_options()
        self.expect_begin()
        L = self.target(override, tok)
        while not is_end(c.peek()):
            it = c.peek()
            if it.k == 'eof':
                raise ParseError(it, 'unterminated STRINGTABLE')
            hits = self.ev.undefined_hits
            sid, sym = self.expr_sym()
            sid &= 0xFFFF
            self.accept_op(',')
            text = self.string()
            if self.ev.undefined_hits != hits:
                self.skipped.append((self.loc(it), 'string %s with undefined id' % (sym or '')))
            elif sid in L.strings:
                self.diag.warn(self.loc(it), 'duplicate string id %d (%s) in language %s ignored'
                               % (sid, sym or '', L.code))
            else:
                L.strings[sid] = (text, sym)
        c.next()

    def parse_dialog(self, rid, rname, sym, tok, ex):
        c = self.cur
        override = self.resource_options()
        x, y, cx, cy = self.coords(4)
        if self.accept_op(','):
            self.expr()
        style = None
        exstyle = 0
        caption = None
        font = None
        menu = (0, None)
        while True:
            t = c.peek()
            if is_begin(t):
                break
            u = kw(t)
            c.next()
            if u == 'STYLE':
                style = self.style_expr(0)
            elif u == 'EXSTYLE':
                exstyle = self.style_expr(0)
            elif u == 'CAPTION':
                caption = self.string()
            elif u == 'FONT':
                size = self.expr()
                self.accept_op(',')
                face = self.string()
                weight = italic = 0
                if self.accept_op(','):
                    weight = self.expr()
                    if self.accept_op(','):
                        italic = self.expr()
                        if self.accept_op(','):
                            self.expr()
                font = (to_s16(size), face, to_s16(weight), 1 if italic & 0xFF else 0)
            elif u == 'MENU':
                mid, mname, _ = self.name_ref()
                menu = (mid, mname)
            elif u == 'CLASS':
                _, cname, _ = self.name_ref()
                self.skipped.append((self.loc(t), 'CLASS %s of dialog %s' % (cname, rname or sym or rid)))
            elif u == 'LANGUAGE':
                override = self.language_args()
            elif u in ('CHARACTERISTICS', 'VERSION'):
                self.expr()
            elif u in MEMORY_OPTIONS:
                pass
            else:
                raise ParseError(t, 'unexpected %s in dialog header' % describe(t))
        if style is None:
            style = WS_POPUP | WS_BORDER | WS_SYSMENU
        if caption is not None:
            style |= WS_CAPTION
        if font is not None:
            style |= DS_SETFONT
        self.expect_begin()
        controls = []
        while not is_end(c.peek()):
            controls.append(self.parse_control())
        c.next()
        L = self.target(override, tok)
        if self.add_unique(L, 'DIALOG', rid, rname, tok):
            L.dialogs.append(Rec(id=rid, name=rname, sym=sym, x=x, y=y, cx=cx, cy=cy, style=style,
                                 exstyle=exstyle, caption=caption, font=font, menu=menu,
                                 controls=controls))

    def ctrl_text(self):
        c = self.cur
        t = c.peek()
        if t.k == 'str' or self.macro_string(t) is not None:
            return self.string()
        if t.k == 'id' and not self.ev.is_symbol(t.v):
            c.next()
            return t.v
        return '#%d' % (self.expr() & 0xFFFF)

    def ctrl_class(self):
        c = self.cur
        t = c.next()
        if t.k == 'str':
            name = self.text(t)
        elif t.k == 'id':
            name = self.macro_string(t)
            if name is None:
                name = t.v
        elif t.k == 'num':
            return CLASS_ORDINALS.get(t.v, '#%d' % t.v)
        else:
            raise ParseError(t, 'expected control class but found %s' % describe(t))
        return CLASS_NAMES.get(name.upper(), name)

    def ctrl_tail(self, style, exstyle):
        if self.accept_op(','):
            style = self.style_expr(style)
            if self.accept_op(','):
                exstyle = self.style_expr(exstyle)
                if self.accept_op(','):
                    self.expr()
        return style, exstyle

    def parse_control(self):
        c = self.cur
        t = c.next()
        u = kw(t)
        exstyle = 0
        if u == 'CONTROL':
            text = self.ctrl_text()
            self.accept_op(',')
            cid, sym = self.expr_sym()
            self.accept_op(',')
            cls = self.ctrl_class()
            self.accept_op(',')
            style = self.style_expr(CHILD)
            self.accept_op(',')
            x, y, cx, cy = self.coords(4)
            if self.accept_op(','):
                exstyle = self.style_expr(0)
                if self.accept_op(','):
                    self.expr()
        elif u in TEXT_CONTROLS:
            cls, default = TEXT_CONTROLS[u]
            text = self.ctrl_text()
            self.accept_op(',')
            cid, sym = self.expr_sym()
            self.accept_op(',')
            x, y, cx, cy = self.coords(4)
            style, exstyle = self.ctrl_tail(CHILD | default, 0)
        elif u in PLAIN_CONTROLS:
            cls, default = PLAIN_CONTROLS[u]
            text = None
            cid, sym = self.expr_sym()
            self.accept_op(',')
            x, y, cx, cy = self.coords(4)
            style, exstyle = self.ctrl_tail(CHILD | default, 0)
            if u == 'COMBOBOX' and not style & CBS_TYPEMASK:
                style |= CBS_SIMPLE
        elif u == 'ICON':
            cls = 'Static'
            text = self.ctrl_text()
            self.accept_op(',')
            cid, sym = self.expr_sym()
            self.accept_op(',')
            x, y = self.coords(2)
            cx = cy = 0
            style = CHILD | SS_ICON
            if self.accept_op(','):
                cx, cy = self.coords(2)
                style, exstyle = self.ctrl_tail(style, 0)
        else:
            raise ParseError(t, 'unknown control statement %s' % describe(t))
        if is_begin(c.peek()):
            self.skip_block()
            self.skipped.append((self.loc(t), 'control data block of %s' % (sym or cid)))
        return Rec(cls=cls, text=text, id=ctrl_id(cid), x=x, y=y, cx=cx, cy=cy,
                   style=style & 0xFFFFFFFF, exstyle=exstyle & 0xFFFFFFFF, sym=sym)

    def menu_options(self):
        c = self.cur
        flags = 0
        while True:
            t = c.peek()
            if t.k == 'op' and t.v in (',', '|') and kw(c.peek(1)) in MENU_OPTIONS:
                c.next()
                continue
            u = kw(t)
            if u in MENU_OPTIONS:
                c.next()
                flags |= MENU_OPTIONS[u]
                continue
            return flags

    def ex_fields(self, count):
        vals = [0] * count
        syms = [None] * count
        for i in range(count):
            if not self.accept_op(','):
                break
            t = self.cur.peek()
            if (t.k == 'op' and t.v == ',') or is_begin(t) or is_end(t):
                continue
            vals[i], syms[i] = self.expr_sym()
        return vals, syms

    def menu_items(self, items, depth, ex):
        c = self.cur
        self.expect_begin()
        while not is_end(c.peek()):
            t = c.next()
            u = kw(t)
            if u == 'MENUITEM':
                if kw(c.peek()) == 'SEPARATOR':
                    c.next()
                    items.append((depth, 0, MF_SEPARATOR, None, None))
                    continue
                text = self.string()
                if ex:
                    (mid, mtype, state), syms = self.ex_fields(3)
                    items.append((depth, to_s32(mid), (mtype | state) & 0xFFFFFFFF, text, syms[0]))
                else:
                    self.accept_op(',')
                    mid, sym = self.expr_sym()
                    items.append((depth, mid & 0xFFFF, self.menu_options(), text, sym))
            elif u == 'POPUP':
                text = self.string()
                if ex:
                    (mid, mtype, state, _), syms = self.ex_fields(4)
                    items.append((depth, to_s32(mid), MF_POPUP | ((mtype | state) & 0xFFFFFFFF), text, syms[0]))
                else:
                    items.append((depth, 0, MF_POPUP | self.menu_options(), text, None))
                self.menu_items(items, depth + 1, ex)
            elif t.k == 'eof':
                raise ParseError(t, 'unterminated menu')
            else:
                raise ParseError(t, 'unexpected %s in menu' % describe(t))
        c.next()

    def parse_menu(self, rid, rname, sym, tok, ex):
        override = self.resource_options()
        items = []
        self.menu_items(items, 0, ex)
        L = self.target(override, tok)
        if self.add_unique(L, 'MENU', rid, rname, tok):
            L.menus.append(Rec(id=rid, name=rname, sym=sym, items=items))

    def parse_accelerators(self, rid, rname, sym, tok):
        c = self.cur
        override = self.resource_options()
        self.expect_begin()
        items = []
        while not is_end(c.peek()):
            t = c.peek()
            if t.k == 'eof':
                raise ParseError(t, 'unterminated ACCELERATORS')
            event = None
            if t.k == 'str':
                c.next()
                event = self.text(t)
            else:
                keyval = self.expr()
            self.accept_op(',')
            cmd, csym = self.expr_sym()
            flags = 0
            while True:
                nt = c.peek()
                if nt.k == 'op' and nt.v in (',', '|') and kw(c.peek(1)) in ACCEL_OPTIONS:
                    c.next()
                    continue
                u = kw(nt)
                if u in ACCEL_OPTIONS:
                    c.next()
                    flags |= ACCEL_OPTIONS[u]
                    continue
                break
            if event is not None:
                if len(event) == 2 and event[0] == '^':
                    ch = event[1].upper()
                    key = ord(ch) - 0x40
                    if not 0 < key < 0x20:
                        self.diag.warn(self.loc(t), 'invalid control character %r' % event)
                        key &= 0x1F
                elif len(event) == 1:
                    key = ord(event.upper() if flags & FVIRTKEY else event)
                else:
                    self.diag.warn(self.loc(t), 'invalid accelerator key %r' % event)
                    key = ord(event[0]) if event else 0
            else:
                key = keyval & 0xFFFF
            items.append((key, cmd & 0xFFFF, flags, csym))
        c.next()
        L = self.target(override, tok)
        if self.add_unique(L, 'ACCELERATORS', rid, rname, tok):
            L.accels.append(Rec(id=rid, name=rname, sym=sym, items=items))

    def parse_toolbar(self, rid, rname, sym, tok):
        c = self.cur
        self.resource_options()
        w, h = self.coords(2)
        override = self.resource_options()
        self.expect_begin()
        buttons = []
        while not is_end(c.peek()):
            t = c.next()
            u = kw(t)
            if u == 'BUTTON':
                bid, bsym = self.expr_sym()
                buttons.append((bid & 0xFFFF, bsym))
            elif u == 'SEPARATOR':
                buttons.append((0, None))
            else:
                raise ParseError(t, 'unexpected %s in TOOLBAR' % describe(t))
        c.next()
        L = self.target(override, tok)
        if self.add_unique(L, 'TOOLBAR', rid, rname, tok):
            L.toolbars.append(Rec(id=rid, name=rname, sym=sym, w=w, h=h, buttons=buttons))

    def parse_dlginit(self, rid, rname, sym, tok):
        c = self.cur
        override = self.resource_options()
        self.expect_begin()
        data = bytearray()
        while not is_end(c.peek()):
            t = c.peek()
            if t.k == 'eof':
                raise ParseError(t, 'unterminated DLGINIT')
            if t.k == 'str':
                c.next()
                data += string_bytes(t.v, t.x, t.cp)
            else:
                self.ev.saw_long = False
                v = self.expr()
                if self.ev.saw_long:
                    data += (v & 0xFFFFFFFF).to_bytes(4, 'little')
                else:
                    data += (v & 0xFFFF).to_bytes(2, 'little')
            self.accept_op(',')
        c.next()
        L = self.target(override, tok)
        codec = codec_for(tok.cp) or 'cp1252'
        off = 0
        n = len(data)
        while off + 2 <= n:
            ctl = int.from_bytes(data[off:off + 2], 'little')
            off += 2
            if ctl == 0:
                break
            if off + 6 > n:
                self.diag.warn(self.loc(tok), 'truncated DLGINIT data')
                break
            msg = int.from_bytes(data[off:off + 2], 'little')
            length = int.from_bytes(data[off + 2:off + 6], 'little')
            off += 6
            payload = bytes(data[off:off + length])
            off += length
            if msg in DLGINIT_TEXT_MESSAGES:
                text = payload.split(b'\0', 1)[0].decode(codec, 'replace')
                L.dlginits.append(Rec(dialog=rid, control=ctrl_id(ctl), text=text, sym=sym))
            else:
                self.skipped.append((self.loc(tok), 'DLGINIT entry with message 0x%04X for control %d of %s (%d bytes)'
                                     % (msg, ctl, rname or sym or rid, length)))
        if rname:
            self.diag.warn(self.loc(tok), 'DLGINIT for string-named dialog %s stored with dialogId 0' % rname)

    def version_block(self):
        c = self.cur
        self.expect_begin()
        items = []
        while not is_end(c.peek()):
            t = c.next()
            u = kw(t)
            if u == 'BLOCK':
                name = self.string()
                while self.accept_op(','):
                    self.expr()
                items.append(('BLOCK', name, self.version_block()))
            elif u == 'VALUE':
                key = self.string()
                vals = []
                while self.accept_op(','):
                    if c.peek().k == 'str':
                        text = self.string()
                        while c.peek().k == 'str':
                            text += self.string()
                        vals.append(text)
                    else:
                        vals.append(self.expr())
                items.append(('VALUE', key, vals))
            elif t.k == 'eof':
                raise ParseError(t, 'unterminated VERSIONINFO block')
            else:
                raise ParseError(t, 'unexpected %s in VERSIONINFO' % describe(t))
        c.next()
        return items

    def parse_versioninfo(self, tok):
        c = self.cur
        override = self.resource_options()
        fixed = {'FILEVERSION': [0, 0, 0, 0], 'PRODUCTVERSION': [0, 0, 0, 0]}
        while not is_begin(c.peek()):
            t = c.next()
            u = kw(t)
            if u in fixed:
                vals = [self.expr()]
                while self.accept_op(','):
                    vals.append(self.expr())
                fixed[u] = ([v & 0xFFFF for v in vals] + [0, 0, 0, 0])[:4]
            elif u in ('FILEFLAGSMASK', 'FILEFLAGS', 'FILEOS', 'FILETYPE', 'FILESUBTYPE'):
                self.expr()
            elif u == 'LANGUAGE':
                override = self.language_args()
            else:
                raise ParseError(t, 'unexpected %s in VERSIONINFO header' % describe(t))
        items = self.version_block()
        pairs = []
        for kind, name, children in items:
            if kind == 'BLOCK' and name.lower() == 'stringfileinfo':
                for k2, n2, c2 in children:
                    if k2 == 'BLOCK':
                        for k3, key, vals in c2:
                            if k3 == 'VALUE' and all(isinstance(v, str) for v in vals):
                                pairs.append((key.split('\0', 1)[0], ''.join(vals).split('\0', 1)[0]))
                        break
                break
        L = self.target(override, tok)
        if L.version is None:
            L.version = Rec(file=fixed['FILEVERSION'], product=fixed['PRODUCTVERSION'], pairs=pairs)


ESCAPE_TABLE = []
for _b in range(256):
    if _b == 0x22:
        ESCAPE_TABLE.append('\\"')
    elif _b == 0x5C:
        ESCAPE_TABLE.append('\\\\')
    elif _b == 0x0A:
        ESCAPE_TABLE.append('\\n')
    elif _b == 0x09:
        ESCAPE_TABLE.append('\\t')
    elif _b == 0x0D:
        ESCAPE_TABLE.append('\\r')
    elif 0x20 <= _b < 0x7F:
        ESCAPE_TABLE.append(chr(_b))
    else:
        ESCAPE_TABLE.append('\\%03o' % _b)


def c_str(s):
    if s is None:
        return 'nullptr'
    pieces = list(map(ESCAPE_TABLE.__getitem__, s.encode('utf-8')))
    chunks = [''.join(pieces[i:i + 400]) for i in range(0, len(pieces), 400)] or ['']
    out = []
    for ch in chunks:
        while '??' in ch:
            ch = ch.replace('??', '?\\?')
        out.append('"%s"' % ch)
    return ' '.join(out)


def c_ident(s):
    return re.sub(r'[^A-Za-z0-9_]', '_', s)


def sym_comment(sym):
    return ' // %s' % sym if sym else ''


def u32(v):
    return '0x%08Xu' % (v & 0xFFFFFFFF)


def emit_language(L, module, rc_name, default_cp):
    o = []
    w = o.append
    var = 'g_rcLang_%s_%s' % (module, c_ident(L.code))
    w('// Generated by rc2cpp.py from %s, language %s. Do not edit.' % (rc_name, L.code))
    w('#include "mfcwx/rcdata.h"')
    w('')
    w('namespace {')
    w('')
    w('using namespace mfcwx::rc;')
    w('')
    for i, d in enumerate(L.dialogs):
        if not d.controls:
            continue
        w('const Control kDialog%dControls[] = {' % i)
        for ct in d.controls:
            w('    {%s, %s, %d, %d, %d, %d, %d, %s, %s},%s' % (
                c_str(ct.cls), c_str(ct.text), ct.id, ct.x, ct.y, ct.cx, ct.cy, u32(ct.style),
                u32(ct.exstyle), sym_comment(ct.sym)))
        w('};')
        w('')
    if L.dialogs:
        w('const Dialog kDialogs[] = {')
        for i, d in enumerate(L.dialogs):
            size, face, weight, italic = d.font if d.font else (0, None, 0, 0)
            w('    {%d, %s, %d, %d, %d, %d, %s, %s, %s, %s, %d, %d, %d, %s, %d, %s, %d},%s' % (
                d.id, c_str(d.name), d.x, d.y, d.cx, d.cy, u32(d.style), u32(d.exstyle),
                c_str(d.caption), c_str(face), size, weight, italic, c_str(d.menu[1]), d.menu[0],
                'kDialog%dControls' % i if d.controls else 'nullptr', len(d.controls),
                sym_comment(d.sym)))
        w('};')
        w('')
    for i, m in enumerate(L.menus):
        w('const MenuItem kMenu%dItems[] = {%s' % (i, sym_comment(m.sym)))
        for depth, mid, flags, text, sym in m.items:
            w('    {%d, %d, 0x%X, %s},%s' % (depth, mid, flags, c_str(text), sym_comment(sym)))
        w('};')
        w('')
    if L.menus:
        w('const Menu kMenus[] = {')
        for i, m in enumerate(L.menus):
            w('    {%d, %s, kMenu%dItems, %d},%s' % (m.id, c_str(m.name), i, len(m.items), sym_comment(m.sym)))
        w('};')
        w('')
    strings = sorted(L.strings.items())
    if strings:
        w('const StringEntry kStrings[] = {')
        for sid, (text, sym) in strings:
            w('    {%d, %s},%s' % (sid, c_str(text), sym_comment(sym)))
        w('};')
        w('')
    for i, a in enumerate(L.accels):
        w('const Accel kAccel%dItems[] = {%s' % (i, sym_comment(a.sym)))
        for key, cmd, flags, sym in a.items:
            w('    {0x%02X, %d, 0x%02X},%s' % (key, cmd, flags, sym_comment(sym)))
        w('};')
        w('')
    if L.accels:
        w('const AccelTable kAccels[] = {')
        for i, a in enumerate(L.accels):
            w('    {%d, %s, kAccel%dItems, %d},%s' % (a.id, c_str(a.name), i, len(a.items), sym_comment(a.sym)))
        w('};')
        w('')
    for i, tb in enumerate(L.toolbars):
        w('const int kToolbar%dButtons[] = {%s' % (i, sym_comment(tb.sym)))
        for bid, sym in tb.buttons:
            w('    %d,%s' % (bid, sym_comment(sym)))
        w('};')
        w('')
    if L.toolbars:
        w('const Toolbar kToolbars[] = {')
        for i, tb in enumerate(L.toolbars):
            w('    {%d, %s, %d, %d, %s, %d},%s' % (tb.id, c_str(tb.name), tb.w, tb.h,
                                                  'kToolbar%dButtons' % i if tb.buttons else 'nullptr',
                                                  len(tb.buttons), sym_comment(tb.sym)))
        w('};')
        w('')
    if L.files:
        w('const FileResource kFiles[] = {')
        for f in L.files:
            w('    {%s, %d, %s, %s},%s' % (c_str(f.type), f.id, c_str(f.name), c_str(f.path), sym_comment(f.sym)))
        w('};')
        w('')
    if L.dlginits:
        w('const DlgInitEntry kDlgInits[] = {')
        for e in L.dlginits:
            w('    {%d, %d, %s},%s' % (e.dialog, e.control, c_str(e.text), sym_comment(e.sym)))
        w('};')
        w('')
    if L.version:
        v = L.version
        if v.pairs:
            w('const char* const kVersionKeys[] = {')
            for key, val in v.pairs:
                w('    %s, %s,' % (c_str(key), c_str(val)))
            w('};')
            w('')
        w('const VersionInfo kVersion = {{%s}, {%s}, %s, %d};' % (
            ', '.join(str(x) for x in v.file), ', '.join(str(x) for x in v.product),
            'kVersionKeys' if v.pairs else 'nullptr', len(v.pairs)))
        w('')
    w('} // namespace')
    w('')
    w('extern const mfcwx::rc::Language %s;' % var)
    w('const mfcwx::rc::Language %s = {' % var)
    w('    %s, 0x%04X, %d,' % (c_str(L.code), L.langid, L.cp or default_cp))
    w('    %s, %d,' % ('kDialogs' if L.dialogs else 'nullptr', len(L.dialogs)))
    w('    %s, %d,' % ('kMenus' if L.menus else 'nullptr', len(L.menus)))
    w('    %s, %d,' % ('kStrings' if strings else 'nullptr', len(strings)))
    w('    %s, %d,' % ('kAccels' if L.accels else 'nullptr', len(L.accels)))
    w('    %s, %d,' % ('kToolbars' if L.toolbars else 'nullptr', len(L.toolbars)))
    w('    %s, %d,' % ('kFiles' if L.files else 'nullptr', len(L.files)))
    w('    %s, %d,' % ('kDlgInits' if L.dlginits else 'nullptr', len(L.dlginits)))
    w('    %s,' % ('&kVersion' if L.version else 'nullptr'))
    w('};')
    w('')
    return '\n'.join(o)


def emit_module(langs, module, rc_name):
    o = []
    w = o.append
    w('// Generated by rc2cpp.py from %s. Do not edit.' % rc_name)
    w('#include "mfcwx/rcdata.h"')
    w('')
    names = ['g_rcLang_%s_%s' % (module, c_ident(L.code)) for L in langs]
    for n in names:
        w('extern const mfcwx::rc::Language %s;' % n)
    if names:
        w('')
        w('namespace {')
        w('const mfcwx::rc::Language* const kLanguages[] = {')
        for n in names:
            w('    &%s,' % n)
        w('};')
        w('} // namespace')
    w('')
    w('extern const mfcwx::rc::Module g_rcModule_%s;' % module)
    w('const mfcwx::rc::Module g_rcModule_%s = {%s, %s, %d};' % (
        module, c_str(module), 'kLanguages' if names else 'nullptr', len(names)))
    w('')
    return '\n'.join(o)


def output_names(module, langs):
    names = ['%s_res.cpp' % module]
    names += ['%s_res_%s.cpp' % (module, L.code) for L in langs]
    names.append('%s_files.txt' % module)
    return names


def write_if_changed(path, content):
    try:
        with open(path, 'r', encoding='utf-8', newline='') as f:
            if f.read() == content:
                return False
    except OSError:
        pass
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content)
    return True


def write_file(path, content):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content)


def print_stats(rc_name, module, langs, out=sys.stdout):
    cols = ('dialogs', 'controls', 'menus', 'items', 'strings', 'accels', 'keys', 'toolbars', 'files',
            'dlginit', 'version')
    print('%s (module %s): %d language(s)' % (rc_name, module, len(langs)), file=out)
    print('%-8s %-6s %-5s ' % ('lang', 'langid', 'cp') + ' '.join('%8s' % c for c in cols), file=out)
    total = [0] * len(cols)
    for L in langs:
        row = [len(L.dialogs), sum(len(d.controls) for d in L.dialogs), len(L.menus),
               sum(len(m.items) for m in L.menus), len(L.strings), len(L.accels),
               sum(len(a.items) for a in L.accels), len(L.toolbars), len(L.files), len(L.dlginits),
               1 if L.version else 0]
        total = [a + b for a, b in zip(total, row)]
        print('%-8s 0x%04X %-5s ' % (L.code, L.langid, L.cp) + ' '.join('%8d' % v for v in row), file=out)
    print('%-8s %-6s %-5s ' % ('total', '', '') + ' '.join('%8d' % v for v in total), file=out)


def depfile_escape(p):
    return p.replace('\\', '/').replace(' ', '\\ ').replace('#', '\\#').replace('$', '$$')


def main(argv=None):
    ap = argparse.ArgumentParser(description='Convert a Windows .rc resource script into mfcwx::rc C++ tables.')
    ap.add_argument('rc', help='resource script to convert')
    ap.add_argument('-m', '--module', help='module name used in file and symbol names (default: .rc base name)')
    ap.add_argument('-o', '--out-dir', help='directory for the generated files')
    ap.add_argument('-I', dest='include_dirs', action='append', default=[], metavar='DIR',
                    help='additional include directory')
    ap.add_argument('-D', dest='defines', action='append', default=[], metavar='NAME[=VALUE]')
    ap.add_argument('-U', dest='undefines', action='append', default=[], metavar='NAME')
    ap.add_argument('--header', action='append', default=[], help='header read for #defines before the .rc')
    ap.add_argument('--no-default-headers', action='store_true', help='do not pre-include mfcwx/winconst.h')
    ap.add_argument('--codepage', type=int, default=1252, help='code page before the first #pragma code_page')
    ap.add_argument('--stats', action='store_true', help='print per-language resource counts')
    ap.add_argument('--list-outputs', action='store_true', help='print the files that would be generated and exit')
    ap.add_argument('--outputs-file', help='file holding the output list; fail if the list changed')
    ap.add_argument('--depfile', help='write a Makefile-style dependency file')
    ap.add_argument('-q', '--quiet', action='store_true', help='only print errors and summaries')
    ap.add_argument('-v', '--verbose', action='store_true', help='list every undefined/string-named symbol')
    args = ap.parse_args(argv)

    rc_path = args.rc
    if not os.path.isfile(rc_path):
        print('rc2cpp: error: cannot open %s' % rc_path, file=sys.stderr)
        return 1
    rc_name = os.path.basename(rc_path)
    module = c_ident(args.module or os.path.splitext(rc_name)[0])
    if not module or module[0].isdigit():
        module = '_' + module
    script_dir = os.path.dirname(os.path.abspath(__file__))
    include_dirs = [os.path.abspath(d) for d in args.include_dirs]
    default_inc = os.path.normpath(os.path.join(script_dir, '..', 'mfcwx', 'include'))
    if os.path.isdir(default_inc) and default_inc not in include_dirs:
        include_dirs.append(default_inc)

    diag = Diag(args.quiet or args.list_outputs)
    dirs = DirCache()
    if codec_for(args.codepage) is None:
        print('rc2cpp: error: unsupported code page %d' % args.codepage, file=sys.stderr)
        return 1
    pp = Preprocessor(include_dirs, diag, args.codepage, dirs)
    pp.define_cmdline('_WIN32')
    pp.define_cmdline('RC_INVOKED')
    for d in args.defines:
        pp.define_cmdline(d)
    for u in args.undefines:
        pp.macros.pop(u, None)
    if not args.no_default_headers:
        hdr = pp.find_include('mfcwx/winconst.h', os.path.dirname(os.path.abspath(rc_path)), True)
        if hdr:
            pp.process_file(hdr, False)
        else:
            diag.warn(rc_name, 'mfcwx/winconst.h not found in include path')
    for h in args.header:
        pp.process_file(h, False)
    pp.process_file(rc_path, True)
    pp.ev.freeze()

    parser = RcParser(pp, rc_path, diag, dirs)
    parser.parse()
    langs = list(parser.langs.values())
    outs = output_names(module, langs)
    out_dir = args.out_dir
    out_paths = [os.path.join(out_dir, n) if out_dir else n for n in outs]
    listing = ''.join(p.replace('\\', '/') + '\n' for p in out_paths)

    if args.list_outputs:
        if diag.errors:
            return 1
        if args.outputs_file:
            os.makedirs(os.path.dirname(os.path.abspath(args.outputs_file)), exist_ok=True)
            write_if_changed(args.outputs_file, listing)
        sys.stdout.write(listing)
        return 0

    if diag.errors:
        print('rc2cpp: %d error(s) in %s; nothing generated' % (diag.errors, rc_name), file=sys.stderr)
        return 1

    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
        write_file(os.path.join(out_dir, outs[0]), emit_module(langs, module, rc_name))
        for L, name in zip(langs, outs[1:-1]):
            write_file(os.path.join(out_dir, name), emit_language(L, module, rc_name, args.codepage))
        files = sorted({f.path for L in langs for f in L.files if f.exists})
        write_file(os.path.join(out_dir, outs[-1]), ''.join(f + '\n' for f in files))
        if args.depfile:
            deps = sorted(set(pp.deps) | {os.path.abspath(__file__)})
            write_file(args.depfile, '%s: %s\n' % (
                depfile_escape(os.path.abspath(out_paths[0])),
                ' \\\n  '.join(depfile_escape(d) for d in deps)))

    if args.stats:
        print_stats(rc_name, module, langs)

    report(parser, pp, diag, args.verbose, rc_name)

    if args.outputs_file:
        try:
            with open(args.outputs_file, encoding='utf-8') as f:
                previous = f.read()
        except OSError:
            previous = None
        if previous is not None and previous != listing:
            write_file(args.outputs_file, listing)
            print('rc2cpp: error: the set of languages in %s changed; re-run CMake (the next build does it '
                  'automatically)' % rc_name, file=sys.stderr)
            return 1
        if previous is None:
            write_file(args.outputs_file, listing)
    return 0


def report(parser, pp, diag, verbose, rc_name):
    ev = pp.ev
    if ev.undefined:
        items = sorted(ev.undefined.items())
        if diag.quiet and not verbose:
            print('rc2cpp: warning: %s: %d undefined symbol(s) evaluated as 0 (%s%s)' % (
                rc_name, len(items),
                ', '.join(n for n, _ in items[:5]), ', ...' if len(items) > 5 else ''), file=sys.stderr)
        else:
            print('rc2cpp: warning: %d undefined symbol(s) evaluated as 0:' % len(items), file=sys.stderr)
            for name, (count, tok) in items:
                print('  %-40s %5d use(s), first at %s' % (name, count, parser.loc(tok)), file=sys.stderr)
    if parser.missing_files:
        print('rc2cpp: warning: %d referenced file(s) not found' % len(parser.missing_files), file=sys.stderr)
    if ev.builtins_used:
        diag.note('symbols taken from the built-in fallback table (missing from the headers): %s'
                  % ', '.join(sorted(ev.builtins_used)))
    if parser.string_named:
        names = sorted(parser.string_named)
        shown = names if verbose else names[:12]
        diag.note('%d identifier(s) without #define used as string resource names: %s%s'
                  % (len(names), ', '.join(shown), '' if len(shown) == len(names) else ', ...'))
    for loc, inc in pp.skipped_includes:
        diag.note('%s: include file "%s" not found, skipped' % (loc, inc))
    if parser.skipped:
        if verbose:
            for loc, what in parser.skipped:
                diag.note('%s: skipped %s' % (loc, what))
        else:
            diag.note('%d construct(s) skipped (use -v to list them)' % len(parser.skipped))

if __name__ == '__main__':
    sys.exit(main())
