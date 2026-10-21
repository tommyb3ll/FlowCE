#!/usr/bin/env python3
"""The English catalog's descriptions, packed (DeltaCE): src/menufr.cc writes each one as
D("...") and src/catalog_text.h, written here, holds them compressed. Byte-pair encoding: a byte the
descriptions never use stands for a pair of bytes (each of which may stand for a pair again);
menufr.cc's catalog_desc() expands one into a buffer. D() finds a description's packed text at
compile time by a hash of its words (FNV-1a, 32 bits); a description missing from the header stops
the build (run this again). 10.6 KB of text were 6.2 KB (2026-10-19).
usage: tools/catpack.py [src/menufr.cc] [src/catalog_text.h]   (the header is rewritten only if it changed)"""
import collections, os, re, sys

SRC = sys.argv[1] if len(sys.argv) > 1 else 'src/menufr.cc'
OUT = sys.argv[2] if len(sys.argv) > 2 else 'src/catalog_text.h'


def c_string(lit):
    """the bytes of a C string literal's body (escapes as the compiler reads them)"""
    out, i = bytearray(), 0
    while i < len(lit):
        c = lit[i]
        if c != 0x5C:  # backslash
            out.append(c); i += 1; continue
        e = lit[i + 1]
        if e == ord('x'):
            j = i + 2
            while j < len(lit) and chr(lit[j]) in '0123456789abcdefABCDEF':
                j += 1
            out.append(int(lit[i + 2:j], 16) & 0xFF); i = j
        elif chr(e) in '01234567':
            j = i + 1
            while j < len(lit) and j < i + 4 and chr(lit[j]) in '01234567':
                j += 1
            out.append(int(lit[i + 1:j], 8) & 0xFF); i = j
        else:
            out.append({'n': 10, 't': 9, 'r': 13, '0': 0, 'a': 7, 'b': 8, 'f': 12, 'v': 11}.get(chr(e), e)); i += 2
    return bytes(out)


def fnv(b):
    h = 2166136261
    for c in b:
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return h


src = open(SRC, 'rb').read()
descs = []  # distinct descriptions, in order
for m in re.finditer(rb'\bD\(\s*"((?:[^"\\]|\\.)*)"\s*\)', src):
    d = c_string(m.group(1))
    if d not in descs:
        descs.append(d)
keys = [fnv(d) for d in descs]
if len(set(keys)) != len(keys):
    sys.exit('catpack: two descriptions share a hash: change one of them')

# byte-pair encoding over all of them (a code never crosses a description's end)
used = set(b for d in descs for b in d)
free = [b for b in range(1, 256) if b not in used]
seqs = [list(d) for d in descs]
pairs = []
for code in free:
    count = collections.Counter()
    for s in seqs:
        for i in range(len(s) - 1):
            count[(s[i], s[i + 1])] += 1
    if not count:
        break
    (a, b), n = count.most_common(1)[0]
    if n < 4:  # (a code costs 2 bytes in the table)
        break
    pairs.append((code, a, b))
    for s in seqs:
        i = 0
        while i < len(s) - 1:
            if s[i] == a and s[i + 1] == b:
                s[i:i + 2] = [code]
            i += 1

offs, blob = [], bytearray()
for s in seqs:
    offs.append(len(blob))
    blob += bytes(s) + b'\0'
if len(blob) > 65535:
    sys.exit('catpack: over 64 KB')


def lit(b):
    """a C string literal of bytes: octal escapes, so a digit after one is never read into it"""
    out, line = [], '"'
    for c in b:
        ch = chr(c)
        t = ch if 32 <= c < 127 and ch not in '"\\?' else '\\%03o' % c
        if len(line) + len(t) > 96:
            out.append(line + '"'); line = '"'
        line += t
    out.append(line + '"')
    return '\n  '.join(out)


code_of = [0] * 256
for k, (code, a, b) in enumerate(pairs):
    code_of[code] = k + 1
raw = sum(len(d) + 1 for d in descs)
packed = len(blob) + 2 * len(pairs) + 256  # (cat_keys and cat_offs are for the compiler only)
h = ['// catalog_text.h - written by tools/catpack.py from src/menufr.cc\'s D("...") descriptions: do not',
     '// edit (edit menufr.cc and run tools/catpack.py; scripts/build_v15.sh does). %d descriptions,' % len(descs),
     '// %d bytes of text in %d on the calculator (cat_keys and cat_offs serve the compiler only)' % (raw, packed),
     '#define CAT_N %d' % len(descs),
     '// a byte that stands for a pair: its number in cat_pair plus 1, else 0 (a byte as it is)',
     'static const unsigned char cat_code[256] = {' + ','.join(str(c) for c in code_of) + '};',
     'static const unsigned char cat_pair[%d][2] = {' % max(1, len(pairs)) + ','.join('{%d,%d}' % (a, b) for _, a, b in pairs) + '};',
     'static const char cat_text[] =\n  ' + lit(bytes(blob)) + ';',
     '// each description\'s FNV-1a hash (32 bits) and where its packed text starts',
     'static constexpr unsigned long cat_keys[CAT_N] = {' + ','.join('%dul' % k for k in keys) + '};',
     'static constexpr unsigned short cat_offs[CAT_N] = {' + ','.join(str(o) for o in offs) + '};', '']
text = '\n'.join(h)
if not os.path.exists(OUT) or open(OUT).read() != text:
    open(OUT, 'w').write(text)
    print(f'catpack: {len(descs)} descriptions, {raw} bytes in {packed} -> {OUT}')
