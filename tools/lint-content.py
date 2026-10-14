#!/usr/bin/env python3
"""Text lint for DO-OCRACY (FINAL-frontend.md section 10, FINAL-space.md check 13).

    python3 -I tools/lint-content.py [FILE ...]

With no files it checks src/content.h, src/glyphs.h, src/levels.c, src/game.c,
src/draw.c, src/main.c, README.md, HANDOFF.md and every text file under src/,
tools/ and docs/. Rules:

  DASH     U+2013 or U+2014 anywhere
  EMOJI    any code point at or above U+1F000, or U+2600..U+27BF
  BYTE     a byte outside 32..126 inside a C string literal
  CAPS     a lower case letter in a game string (content.h and levels.c
           literals, and game_toast / ticker / snprintf text in game.c);
           printf conversions such as %d and %s are allowed
  GLYPH    a GLYPH_SRC row in glyphs.h that is not 16 characters
  FLASH    a FLASH_CAPS card of more than 6 words
  TOAST    a TOAST_ constant over 64 characters (the drawer's cut)
  BANNER   a *_BANNER string over 48 characters
  AWARD    an AWARD_ string over 64 characters with %d as two digits

Prints one line per finding and exits 1 if there are any.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME_FILES = ['src/content.h', 'src/levels.c']
LITERAL = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
CONV = re.compile(r'%[-+ 0#]*\d*(?:\.\d+)?[a-z]')
findings = []


def report(path, line, rule, msg):
    findings.append(f'{os.path.relpath(path, ROOT)}:{line}: {rule} {msg}')


def strip_comments(src):
    """C source with comments blanked, newlines kept so line numbers hold."""
    out, i, n = [], 0, len(src)
    while i < n:
        if src.startswith('/*', i):
            j = src.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r'[^\n]', ' ', src[i:j]))
            i = j
        elif src.startswith('//', i):
            j = src.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i))
            i = j
        elif src[i] == '"':
            m = LITERAL.match(src, i)
            if not m:
                out.append(src[i]); i += 1; continue
            out.append(m.group(0)); i = m.end()
        elif src[i] == "'":
            j = i + 1
            while j < n and src[j] != "'":
                j += 2 if src[j] == '\\' else 1
            out.append(src[i:j + 1]); i = j + 1
        else:
            out.append(src[i]); i += 1
    return ''.join(out)


def lineno(src, pos):
    return src.count('\n', 0, pos) + 1


def check_text(path, text):
    for no, line in enumerate(text.split('\n'), 1):
        for ch in line:
            o = ord(ch)
            if o in (0x2013, 0x2014):
                report(path, no, 'DASH', repr(line.strip()[:60])); break
            if o >= 0x1F000 or 0x2600 <= o <= 0x27BF:
                report(path, no, 'EMOJI', repr(line.strip()[:60])); break


def joined_literals(code, start):
    """Adjacent C literals from start, joined, for one constant."""
    s, m = '', LITERAL.match(code, start)
    pos = start
    while m:
        s += m.group(1)
        pos = m.end()
        nxt = re.match(r'\s*', code[pos:])
        m = LITERAL.match(code, pos + nxt.end())
    return s


def check_c(path, raw):
    code = strip_comments(raw)
    rel = os.path.relpath(path, ROOT)
    game = rel in GAME_FILES
    code = re.sub(r'(?m)^\s*#\s*include[^\n]*', lambda x: ' ' * len(x.group(0)), code)
    for m in LITERAL.finditer(code):
        body = m.group(1)
        for b in body.encode('utf-8'):
            if b < 32 or b > 126:
                report(path, lineno(code, m.start()), 'BYTE', repr(body[:50])); break
        if game:
            plain = CONV.sub('', body.replace('\\n', ' '))
            if re.search(r'[a-z]', plain):
                report(path, lineno(code, m.start()), 'CAPS', repr(body[:50]))
    if rel == 'src/game.c':
        for m in re.finditer(r'\b(game_toast|ticker|ticker_sub|pop)\s*\(([^;]*)\);', code):
            for lit in LITERAL.finditer(m.group(2)):
                plain = CONV.sub('', lit.group(1))
                if re.search(r'[a-z]', plain):
                    report(path, lineno(code, m.start()), 'CAPS', repr(lit.group(1)[:50]))
    if rel == 'src/glyphs.h':
        for m in LITERAL.finditer(code):
            if len(m.group(1)) != 16:
                report(path, lineno(code, m.start()), 'GLYPH', f'row of {len(m.group(1))}: {m.group(1)!r}')
    if rel == 'src/content.h':
        for m in re.finditer(r'\b(TOAST_\w+|\w+_BANNER\w*|AWARD_\w+)\s*(?:\[[^\]]*\])?\s*=\s*', code):
            name = m.group(1)
            if code[m.end()] == '{':
                end = code.find('};', m.end())
                texts = [(x.group(1), x.start()) for x in LITERAL.finditer(code, m.end(), end)]
            elif code[m.end()] == '"':
                texts = [(joined_literals(code, m.end()), m.end())]
            else:
                continue
            for text, pos in texts:
                n = len(text.replace('%d', '99').replace('%s', 'XXXXXXXXXXXX'))
                if name.startswith('TOAST_') and n > 64:
                    report(path, lineno(code, pos), 'TOAST', f'{name} is {n} characters')
                elif '_BANNER' in name and n > 48:
                    report(path, lineno(code, pos), 'BANNER', f'{name} is {n} characters')
                elif name.startswith('AWARD_') and n > 64:
                    report(path, lineno(code, pos), 'AWARD', f'{name} is {n} characters')
        m = re.search(r'FLASH_CAPS\s*\[[^\]]*\]\s*\[[^\]]*\]\s*=\s*\{', code)
        if m:
            end = code.find('};', m.end())
            for card in re.finditer(r'\{([^{}]*)\}', code[m.end():end]):
                words = ' '.join(x.group(1) for x in LITERAL.finditer(card.group(1))).split()
                if len(words) > 6:
                    report(path, lineno(code, m.end() + card.start()), 'FLASH', f'{len(words)} words: {" ".join(words)}')


def default_files():
    files = ['README.md', 'HANDOFF.md']
    for d in ('src', 'tools', 'docs'):
        for base, dirs, names in os.walk(os.path.join(ROOT, d)):
            dirs[:] = [x for x in dirs if x not in ('gen', 'node_modules')]
            for name in names:
                if name.endswith(('.c', '.h', '.cpp', '.md', '.mjs', '.py', '.sh', '.json', '.txt')):
                    files.append(os.path.relpath(os.path.join(base, name), ROOT))
    return files


def main(argv):
    files = argv or default_files()
    for f in files:
        path = os.path.join(ROOT, f) if not os.path.isabs(f) else f
        with open(path, encoding='utf-8', errors='replace') as fh:
            text = fh.read()
        check_text(path, text)
        if path.endswith(('.c', '.h', '.cpp')):
            check_c(path, text)
    for line in findings:
        print(line)
    print(f'lint-content: {len(findings)} finding(s) in {len(files)} file(s)')
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
