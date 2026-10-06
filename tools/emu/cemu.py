#!/usr/bin/env python3
"""Headless CEmu driver for testing KhiCAS (runs in WSL).

Wraps CEmu's `tests/autotester/cemu-headless` (built in $KB/CEmu). The ROM is the user's own dump
and lives OUTSIDE the repo ($KHICAS_ROM); emulator states (*.ce) live in $KB/emu/states.

Python API:   e = Emu(image='x.ce') | Emu(rom=ROM);  e.key('enter'); e.type('2x+1'); e.shot('name');
              ms = e.wait_stable();  e.save('y.ce');  e.close()
CLI (one checkpointed step per call, handy from a shell):
  cemu.py <in: state.ce|rom> <out: state.ce|-> <shot-dir> step [step ...]
  steps:  run:MS  key:NAME  keys:N1,N2,..  type:TEXT  shot:NAME  send:ram|archive|auto:PATH
          wait[:MAXMS]  reset  hash
"""
import os, struct, subprocess, sys, zlib

KB = os.environ.get('KB', '/home/bell/khicas-review-build')
HEADLESS = os.environ.get('CEMU_HEADLESS', KB + '/CEmu/tests/autotester/cemu-headless')
ROM = os.environ.get('KHICAS_ROM', '/mnt/c/Users/tjb43/Downloads/Projects/khicas/rom.rom')
KEY_GAP_MS = 60   # idle time after each key release so KhiCAS/OS can process it

# KhiCAS EN key map (src/k_csdk.c getkey): text char -> key presses.
_ALPHA = dict(zip('abcdefghijklmnopqrstuvwxyz',
                  ['math', 'apps', 'prgm', 'inv', 'sin', 'cos', 'tan', '^', 'sq', 'comma', '(', ')', '/',
                   'log', '7', '8', '9', '*', 'ln', '4', '5', '6', '-', 'sto', '1', '2']))
CHARKEYS = {c: ['alpha', k] for c, k in _ALPHA.items()}
CHARKEYS.update({d: [d] for d in '0123456789'})
CHARKEYS.update({'+': ['+'], '-': ['-'], '*': ['*'], '/': ['/'], '^': ['^'], '(': ['('], ')': [')'],
                 ',': ['comma'], '.': ['.'], ' ': ['alpha', '0'], '"': ['alpha', '+'], ':': ['alpha', '.'],
                 '?': ['alpha', '(-)'], '_': ['(-)'], '[': ['2nd', '*'], ']': ['2nd', '-'],
                 '{': ['2nd', '('], '}': ['2nd', ')'], 'π': ['2nd', '^'], 'θ': ['alpha', '3'],
                 '=': ['2nd', 'math']})


def bmp2png(src, dst, scale=1):
    d = open(src, 'rb').read()
    off, = struct.unpack_from('<I', d, 10)
    w, h = struct.unpack_from('<ii', d, 18)
    bpr = struct.unpack_from('<H', d, 28)[0] // 8
    stride = (w * bpr + 3) & ~3
    rows = []
    for y in range(abs(h)):
        sy = (abs(h) - 1 - y) if h > 0 else y
        r = d[off + sy * stride: off + sy * stride + w * bpr]
        px = bytearray()
        for x in range(w):
            px += bytes((r[x * bpr + 2], r[x * bpr + 1], r[x * bpr])) * scale
        rows += [b'\x00' + bytes(px)] * scale
    def chunk(t, p):
        return struct.pack('>I', len(p)) + t + p + struct.pack('>I', zlib.crc32(t + p) & 0xffffffff)
    open(dst, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w * scale, abs(h) * scale, 8, 2, 0, 0, 0))
                          + chunk(b'IDAT', zlib.compress(b''.join(rows), 9)) + chunk(b'IEND', b''))


def map_symbol(name, variant='en', mapfile=None):
    """Address of a symbol in a linker map (statics included), or None. mapfile: the map saved with
    an emulator state (install.py); default: the last build's."""
    try:
        for line in open(mapfile or f'{KB}/out/{variant}/DEMO.map', errors='replace'):
            parts = line.split()
            if len(parts) >= 3 and parts[0] == name and parts[1] == '=':
                return int(parts[2], 16)
    except OSError:
        pass
    return None


class Emu:
    def __init__(self, image=None, rom=None, shotdir='/tmp', verbose=False):
        args = [HEADLESS] + (['--image', image] if image else ['--rom', rom or ROM])
        # stderr carries core diagnostics and the calculator's dbg_printf console output
        self.errlog = f'/tmp/cemu_stderr_{os.getpid()}.log'
        self.p = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=open(self.errlog, 'w'), text=True, bufsize=1)
        ready = self.p.stdout.readline()
        if not ready.startswith('CEMU_HEADLESS_READY'):
            raise RuntimeError('cemu-headless failed to start: ' + ready)
        self.shotdir, self.verbose, self.ms = shotdir, verbose, 0
        # the linker map of the build in this state (saved by install.py), for probe addresses
        m = image[:-3] + '.map' if image and image.endswith('.ce') else None
        self.mapfile = m if m and os.path.exists(m) else None

    def cmd(self, line):
        self.p.stdin.write(line + '\n'); self.p.stdin.flush()
        r = self.p.stdout.readline().strip()
        if self.verbose: print('>', line, '->', r)
        if not r.startswith('OK'):
            raise RuntimeError(f'{line!r} -> {r!r}')
        return r

    def run(self, ms):
        self.cmd(f'run {ms}'); self.ms += ms

    def key(self, name, hold=80, settle=False):
        """Press+release. With settle=True, keep running until the screen stops changing
        (KhiCAS polls the keypad, so a press made while it is still redrawing is lost).
        Returns the settle latency in emulated ms (time until the last screen change)."""
        self.cmd(f'key {name} {hold}'); self.ms += hold
        if not settle:
            self.run(KEY_GAP_MS)
            return 0
        t = self.wait_idle(timeout=5000)
        if t is not None:
            self.run(120)  # human pace: typing the instant KhiCAS polls mangled inputs (2026-10-06)
            return t
        return self.wait_stable(step=20, stable=4, timeout=5000)

    def keys(self, names, settle=False):
        return [self.key(n, settle=settle) for n in names]

    def type(self, text, settle=True):
        """Type text with the KhiCAS key map; `{name}` or `{n1,n2}` presses raw emulator keys
        (e.g. `{(-)}5`, `{sin}`, `{2nd,^}`, `{up}`). Returns per-key settle latencies (ms)."""
        lat = []
        i = 0
        while i < len(text):
            c = text[i]
            if c == '{' and '}' in text[i + 1:]:
                j = text.index('}', i + 1)
                lat += self.keys(text[i + 1:j].split(','), settle=settle)
                i = j + 1
                continue
            if c not in CHARKEYS:
                raise ValueError(f'no key mapping for {c!r}')
            lat += self.keys(CHARKEYS[c], settle=settle)
            i += 1
            # 2D input: ^ opens an exponent template; as a user would, leave it with RIGHT after a
            # one-token exponent (x^2+1). ^(...) needs nothing: ( reuses the group, ) leaves it.
            if c == '^' and i < len(text) and text[i] not in '({':
                j = i
                while j < len(text) and (text[j].isalnum() or text[j] in '.π'):
                    j += 1
                if j > i:
                    for cj in text[i:j]:
                        lat += self.keys(CHARKEYS[cj], settle=settle)
                    lat += self.keys(['right'], settle=settle)
                    i = j
        self.key_latencies = getattr(self, 'key_latencies', []) + lat
        return lat

    def polls(self):
        """getkey's poll counter (src/k_csdk.c getkey_polls), None if this build has none."""
        if not hasattr(self, '_polls_addr'):
            self._polls_addr = map_symbol('_getkey_polls', mapfile=self.mapfile)
        if self._polls_addr is None:
            return None
        b = self.peek(self._polls_addr, 3)
        return b[0] | b[1] << 8 | b[2] << 16

    def wait_idle(self, step=5, timeout=300000):
        """Run until KhiCAS polls the keypad again (ready for the next key; a key pressed while it is
        busy is lost). Returns the emulated ms waited, None without the counter (old builds)."""
        c0 = self.polls()
        if c0 is None:
            return None
        t = 0
        while t < timeout:
            self.run(step); t += step
            if self.polls() != c0:
                return t
        return t

    def hash(self):
        return self.cmd('screen-hash').split()[-1]

    def shot(self, name, scale=2):
        bmp = f'/tmp/cemu_{os.getpid()}.bmp'
        self.cmd(f'screenshot {bmp}')
        path = os.path.join(self.shotdir, name + '.png')
        bmp2png(bmp, path, scale)
        return path

    def wait_stable(self, step=50, stable=10, timeout=300000):
        """Run until the screen hash is unchanged for `stable` steps. Returns emulated ms until the last change."""
        last, same, t, t_change = self.hash(), 0, 0, 0
        while t < timeout and same < stable:
            self.run(step); t += step
            h = self.hash()
            if h == last:
                same += 1
            else:
                last, same, t_change = h, 0, t
        return t_change

    def send(self, path, dest='auto'):
        self.cmd(f'send-file {dest} {path}')

    def reset(self):
        self.cmd('reset')

    def save(self, path):
        self.cmd(f'save-state {path}')

    def peek(self, addr, n=1):
        return bytes.fromhex(self.cmd(f'peek {addr:X} {n}').split()[-1])

    def poke(self, addr, data):
        self.cmd(f'poke {addr:X} {bytes(data).hex()}')

    def paste(self, text):
        """Put text on the console's edit line at once, the caret at its end (ASCII). For corpora
        in linear syntax: typed keys go through the 2D editing rules (mathedit.cc), where / makes
        a fraction of what follows, so 1/2+1 typed is 1/(2+1). The console must be idle on its
        edit line; the next key (EXE) evaluates it."""
        sym = lambda n: map_symbol(n, mapfile=self.mapfile)
        el = sym('__ZL9Edit_Line') or sym('_Edit_Line')
        ll, ln, cur = sym('_Last_Line'), sym('_Line'), sym('_Cursor')
        u24 = lambda b: b[0] | b[1] << 8 | b[2] << 16
        p24 = lambda v: bytes([v & 255, v >> 8 & 255, v >> 16 & 255])
        b = text.replace('π', 'pi').encode('utf-8')        # the pi key types pi
        self.poke(u24(self.peek(el, 3)), b + bytes(1))
        rec = u24(self.peek(ln, 3)) + 13 * u24(self.peek(ll, 3))  # struct line: str, readonly, type, start_col, disp_len
        n = len(b)
        sc = max(0, n - 38)                                      # COL_DISP_MAX - 1
        self.poke(rec + 7, p24(sc))
        self.poke(rec + 10, p24(n))
        self.poke(cur, p24(n - sc))                              # Cursor.x (Cursor.y: the edit line's row)

    # The ROM dump is from a friend's TI-84 Plus CE on OS 5.8.2.0029; the user's own calculator runs
    # OS 5.8.0.0022. os_GetSystemInfo() copies a version template from flash on every call, so we patch
    # that template (after boot; the boot code verifies the OS only at boot) to report the user's version.
    # Restore before anything resets the calculator, or the boot code will reject the OS.
    OSVER_ADDR = 0x8CA11            # revision byte + 16-bit build in OS 5.8.2.0029's template

    def os_fake(self):
        assert self.peek(self.OSVER_ADDR - 2, 5) in (bytes([5, 8, 2, 0x1D, 0]), bytes([5, 8, 0, 0x16, 0])), 'unknown OS'
        self.poke(self.OSVER_ADDR, [0, 0x16])        # 5.8.0.0022

    def os_real(self):
        self.poke(self.OSVER_ADDR, [2, 0x1D])        # 5.8.2.0029

    def debug_output(self):
        """Calculator debug console (dbg_printf) + core messages, minus routine [CEmu] lines."""
        try:
            return [l.rstrip('\n') for l in open(self.errlog, errors='replace') if not l.startswith('[CEmu]')]
        except OSError:
            return []

    def close(self):
        try:
            self.p.stdin.write('quit\n'); self.p.stdin.flush(); self.p.wait(10)
        except Exception:
            self.p.kill()


def main(argv):
    src, dst, shotdir, steps = argv[0], argv[1], argv[2], []
    for s in argv[3:]:
        if s.startswith('@'):          # steps file, one step per line (see emu.sh)
            steps += [l.rstrip('\r\n') for l in open(s[1:], encoding='utf-8')]
        else:
            steps.append(s)
    steps = [s for s in steps if s]
    os.makedirs(shotdir, exist_ok=True)
    if dst != '-':
        os.makedirs(os.path.dirname(dst), exist_ok=True)
    e = Emu(rom=src, shotdir=shotdir) if src.endswith('.rom') else Emu(image=src, shotdir=shotdir)
    try:
        for s in steps:
            op, _, arg = s.partition(':')
            if op == 'run': e.run(int(arg))
            elif op == 'key':
                name, _, hold = arg.partition(':'); e.key(name, int(hold) if hold else 80)
            elif op == 'keys': e.keys(arg.split(','))
            elif op == 'type':
                lat = e.type(arg)
                print(f'typed {len(arg)} chars: key latency max {max(lat)} ms, mean {sum(lat) // len(lat)} ms')
            elif op == 'shot': print('shot', e.shot(arg))
            elif op == 'send':
                dest, _, path = arg.partition(':'); e.send(path, dest)
            elif op == 'wait': print('stable after', e.wait_stable(timeout=int(arg) if arg else 300000), 'ms')
            elif op == 'reset': e.reset()
            elif op == 'hash': print('hash', e.hash())
            elif op == 'peek':
                a, _, n = arg.partition(':'); print('peek', a, e.peek(int(a, 16), int(n or 1)).hex())
            elif op == 'poke':
                a, _, h = arg.partition(':'); e.poke(int(a, 16), bytes.fromhex(h))
            elif op == 'osfake': e.os_fake()
            elif op == 'osreal': e.os_real()
            else: raise ValueError('unknown step ' + s)
        if dst != '-':
            e.save(dst)
        print(f'ok: {len(steps)} steps, {e.ms} ms emulated')
    finally:
        e.close()
        dbg = e.debug_output()
        if dbg:
            print(f'--- calculator debug output ({len(dbg)} lines, last 40) ---')
            print('\n'.join(dbg[-40:]))


if __name__ == '__main__':
    main(sys.argv[1:])
