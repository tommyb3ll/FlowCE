#!/usr/bin/env python3
"""Many problems on the emulated calculator itself, compared with the PC build's answers.

Each problem is pasted on the console's edit line and evaluated; its answer is read from the
calculator's memory (the history line's text) and timed in emulated ms (enter -> the keypad is
polled again: what the user waits). The PC build's -k path runs the same src/answer.cc, so the
answers should be the same: a difference is the calculator's own (32-bit floats, the eZ80's
memory, giac's session order), and the times show what is slow on the real thing.

Usage (WSL): batch.py <state> <out> problems.txt [more ...] [-t S] [-n N] [-r R] [-s skip]
  problems: numcheck corpora (kind ; expr ; ...), tools/emu/cases/*.tsv (name<TAB>expr; key
            sequences skipped) or plain inputs, one per line
  -t S   ON interrupts a problem after S emulated seconds (default 90)
  -n N   only the first N problems;  -s K  skip the first K
  -r R   a fresh emulator (the state's console) every R problems (default 20)
  -b BIN the PC build (default /home/bell/hostgiac/bin/hostgiac-worktree)
Writes <out> ("input -> calculator answer  @@emulated ms") and <out>.diff: the problems whose
answers differ from the PC's (DIFF), took over 20 s (SLOW), were interrupted (TIMEOUT) or
stopped the calculator (HANG: no answer, keypad not polled after ON).
"""
import os, re, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB, map_symbol

args = sys.argv[1:]
state, out, files = args.pop(0), args.pop(0), []
T, N, R, SKIP, HOST = 90, 10 ** 9, 20, 0, '/home/bell/hostgiac/bin/hostgiac-worktree'
while args:
    a = args.pop(0)
    if a == '-t': T = int(args.pop(0))
    elif a == '-n': N = int(args.pop(0))
    elif a == '-r': R = int(args.pop(0))
    elif a == '-s': SKIP = int(args.pop(0))
    elif a == '-b': HOST = args.pop(0)
    else: files.append(a)

KINDS = {'int': lambda a: f'integrate({a[0]},x)', 'defint': lambda a: f'integrate({a[0]},x,{a[1]},{a[2]})',
         'diff': lambda a: f'diff({a[0]},x)', 'lim': lambda a: f'limit({a[0]},x,{a[1]})',
         'limr': lambda a: f'limit({a[0]},x,{a[1]},1)', 'liml': lambda a: f'limit({a[0]},x,{a[1]},-1)',
         'sum': lambda a: f'sum({a[0]},n,{a[1]},inf)', 'solve': lambda a: f'solve({a[0]},x)',
         'eval': lambda a: a[0], 'raw': lambda a: a[0]}
inputs = []
for fn in files:
    for line in open(fn, encoding='utf-8', errors='replace'):
        line = line.rstrip('\r\n')
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        if '\t' in line:                       # cases/*.tsv: name<TAB>input[<TAB>keys]
            x = line.split('\t')[1]
            if '{' not in x:
                inputs.append(x)
            continue
        p = [x.strip() for x in line.split(' #')[0].split(' ; ')]
        if len(p) == 1:
            inputs.append(p[0])
        elif p[0] in KINDS:
            inputs.append(KINDS[p[0]](p[1:] + [''] * 3))
inputs = list(dict.fromkeys(inputs))[SKIP:SKIP + N]


def host(exprs):
    """{expr: answer} from the PC build (a crash or a hang skips that expression)"""
    res, todo = {}, list(exprs)
    while todo:
        f = f'/tmp/batch_{os.getpid()}.in'
        open(f, 'w').write('\n'.join(todo) + '\n')
        try:
            o = subprocess.run([HOST, '-p', '-k', '-f', f], capture_output=True, text=True, timeout=600,
                               env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'), cwd='/tmp').stdout
        except subprocess.TimeoutExpired as e:
            o = e.stdout.decode() if isinstance(e.stdout, bytes) else (e.stdout or '')
        done = 0
        for l in o.split('\n'):
            if done < len(todo) and l.startswith(todo[done] + ' -> '):
                res[todo[done]] = l[len(todo[done]) + 4:]
                done += 1
        if done < len(todo):
            res[todo[done]] = '(PC: crash or hang)'
        todo = todo[done + 1:]
    return res


class Calc:
    """FlowCE's console on the emulator, restarted from the state on demand"""
    def __init__(self):
        self.e = None

    def start(self):
        if self.e:
            self.e.close()
        self.e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir=os.path.dirname(os.path.abspath(out)))
        sym = lambda n: map_symbol(n, mapfile=self.e.mapfile)
        self.ll, self.ln, self.polls = sym('_Last_Line'), sym('_Line'), sym('_getkey_polls')
        self.e.run(800)
        self.e.wait_idle(timeout=5000)
        self.used = 0

    def u24(self, a):
        b = self.e.peek(a, 3)
        return b[0] | b[1] << 8 | b[2] << 16

    def text(self, k):
        """the history line k's text"""
        p, s = self.u24(self.u24(self.ln) + 13 * k), b''
        while p and len(s) < 8192:
            b = self.e.peek(p + len(s), 256)
            s += b
            if 0 in b:
                break
        return s.split(b'\0')[0].decode('utf-8', 'replace')

    def wait(self, limit_ms):
        """emulated ms until the keypad is polled again, None after limit_ms"""
        c0, t = self.u24(self.polls), 0
        while t < limit_ms:
            step = 10 if t < 1000 else 50 if t < 10000 else 250
            self.e.run(step)
            t += step
            if self.u24(self.polls) != c0:
                return t
        return None

    def solve(self, x):
        if not self.e or self.used >= R:
            self.start()
        self.used += 1
        self.e.paste(x)
        self.e.key('enter')
        t = self.wait(T * 1000)
        tag = ''
        if t is None:
            self.e.key('on', hold=400)           # ON: giac's control_c stops the calculation
            if self.wait(30000) is None:
                self.e.shot('hang_%d' % len(rows))
                self.e.close(); self.e = None
                return 'HANG', -1
            tag, t = 'TIMEOUT', T * 1000
            self.used = R                        # (a fresh calculator for the next one)
        self.e.run(100)
        return tag + self.text(self.u24(self.ll) - 1), t


t0 = time.time()
pc = host(inputs)
print(f'{len(inputs)} problems; PC answers in {time.time() - t0:.0f} s', flush=True)
calc, rows = Calc(), []
fo = open(out, 'w')
try:
    for i, x in enumerate(inputs):
        a, ms = calc.solve(x)
        rows.append((x, a, ms))
        fo.write(f'{x} -> {a}  @@{ms}\n'); fo.flush()
        p = pc.get(x, '').split('  ~ ')[0]
        print(f'{i + 1:4} {ms:7} ms  {"" if a == p else "DIFF "}{x} -> {a[:100]}', flush=True)
finally:
    if calc.e:
        calc.e.close()
fo.close()
rep = []
for x, a, ms in rows:
    p = pc.get(x, '').split('  ~ ')[0]
    if a == 'HANG':
        rep.append(f'HANG    {x}   [PC: {p}]')
    elif a.startswith('TIMEOUT'):
        rep.append(f'TIMEOUT {x} -> {a[7:]}   [after {T} s; PC: {p}]')
    elif a != p:
        rep.append(f'DIFF    {x}\n    calc: {a}\n    PC:   {p}')
    if ms > 20000:
        rep.append(f'SLOW    {x}   [{ms / 1000:.1f} s]')
ts = sorted(ms for _, _, ms in rows if ms >= 0)
rep.append(f'{len(rows)} problems in {time.time() - t0:.0f} s wall: '
           f'{sum(r.startswith("DIFF") for r in rep)} differ, {sum(r.startswith("SLOW") for r in rep)} slow, '
           f'{sum(r.startswith("TIMEOUT") for r in rep)} timed out, {sum(r.startswith("HANG") for r in rep)} hung; '
           f'median {ts[len(ts) // 2] / 1000 if ts else 0:.1f} s, max {ts[-1] / 1000 if ts else 0:.1f} s emulated')
open(out + '.diff', 'w').write('\n'.join(rep) + '\n')
print(rep[-1])
