#!/usr/bin/env python3
"""Where the calculator spends its time on one problem: a sampling profiler on the emulator.

The problem is pasted and evaluated; every few emulated ms the eZ80's program counter is read
(self time: the function running) and the stack is scanned for return addresses into the app
(inclusive time: every function with a frame on the stack, once a sample). A return address is
a word just after a CALL instruction (checked in the code); stale ones add a little noise.

The app runs where INST put it in flash, not at its link addresses: the offset is read while
the problem runs from giac::control_c_hook, which main.cc points to focus_busy_tick. Code
outside the app is reported as the OS.

Usage (WSL): profile.py <state> <problem file> [-i MS] [-t S] [-n TOP]
  -i  sampling interval in emulated ms (default 20);  -t  stop after S emulated seconds (120)
"""
import bisect, os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB, map_symbol

args = sys.argv[1:]
state, pfile = args.pop(0), args.pop(0)
IV, T, TOP = 20, 120, 30
while args:
    a = args.pop(0)
    if a == '-i': IV = int(args.pop(0))
    elif a == '-t': T = int(args.pop(0))
    elif a == '-n': TOP = int(args.pop(0))

e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir='/tmp')
syms = []
for line in open(e.mapfile, errors='replace'):
    p = line.split()
    if len(p) >= 3 and p[1] == '=' and re.fullmatch(r'[0-9A-Fa-f]{6}', p[2]):
        a = int(p[2], 16)
        if a < 0xD00000 and not p[0].startswith('___len') and p[0] != '__app_version':
            syms.append((a, p[0]))
syms.sort()
addrs = [s[0] for s in syms]
code_hi = map_symbol('app.signature', mapfile=e.mapfile) or addrs[-1]
sym = lambda n: map_symbol(n, mapfile=e.mapfile)
hook, tick, polls = sym('__ZN4giac14control_c_hookE'), sym('__Z15focus_busy_tickv'), sym('_getkey_polls')
u24 = lambda b, i=0: b[i] | b[i + 1] << 8 | b[i + 2] << 16
CALLS = {0xCD, 0xC4, 0xCC, 0xD4, 0xDC, 0xE4, 0xEC, 0xF4, 0xFC}
off = None
opcache = {}


def fn_of(pc):
    a = pc - off
    if not 0 <= a < code_hi:
        return '(OS)'
    i = bisect.bisect_right(addrs, a) - 1
    return syms[i][1] if i >= 0 else '?'


def is_ret(v):  # v follows a CALL (4 bytes in ADL mode)
    if v not in opcache:
        opcache[v] = e.peek(v - 4, 1)[0] in CALLS
    return opcache[v]


self_n, incl_n, n, t = {}, {}, 0, 0
try:
    e.run(800)
    e.wait_idle(timeout=5000)
    top = int(re.search(r'sp=([0-9A-F]+)', e.cmd('regs')).group(1), 16) + 1024  # main's frames
    e.paste(open(pfile).read().strip())
    e.key('enter')
    c0 = e.peek(polls, 3)
    while t < T * 1000:
        e.run(IV)
        t += IV
        if e.peek(polls, 3) != c0:
            break
        if off is None:
            h = u24(e.peek(hook, 3))
            if not h:
                continue
            off = h - tick
        r = e.cmd('regs')
        pc = int(re.search(r'pc=([0-9A-F]+)', r).group(1), 16)
        sp = int(re.search(r'sp=([0-9A-F]+)', r).group(1), 16)
        n += 1
        f = fn_of(pc)
        self_n[f] = self_n.get(f, 0) + 1
        seen = {f}
        if 0xD00000 <= sp < top:
            b = e.peek(sp, min(top - sp, 8000))
            for i in range(len(b) - 2):
                v = u24(b, i)
                if 0 <= v - off < code_hi and is_ret(v):
                    seen.add(fn_of(v))
        for g in seen:
            incl_n[g] = incl_n.get(g, 0) + 1
finally:
    e.close()


def dem(names):
    try:
        out = subprocess.run(['c++filt'], input='\n'.join(x[1:] if x.startswith('__Z') else x for x in names),
                             capture_output=True, text=True).stdout.split('\n')
        return [o[:110] for o in out]
    except OSError:
        return names


print(f'{n} samples every {IV} ms: {t / 1000:.1f} s emulated{" (still running)" if t >= T * 1000 else ""}'
      f'{"" if off is not None else " (no offset: control_c_hook never set)"}')
for title, d in (('self (running)', self_n), ('inclusive (on the stack)', incl_n)):
    top_n = sorted(d.items(), key=lambda x: -x[1])[:TOP]
    print(f'--- {title}')
    for (name, c), dn in zip(top_n, dem([x[0] for x in top_n])):
        print(f'{100 * c / max(n, 1):5.1f}%  {dn}')
