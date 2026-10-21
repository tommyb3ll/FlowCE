#!/usr/bin/env python3
"""Where the time goes after a key (a menu opening, enter on a problem): profile.py's sampling,
around keys instead of one calculation.

The app's place in flash is found first from giac::control_c_hook while a short problem runs (as
profile.py does). Then, R times on a fresh state: the setup keys, then the timed key; every IV
emulated ms the program counter (self) and the return addresses on the stack (inclusive) are
sampled until the app has polled the keypad steadily again (the screen done), at most W ms.

Usage (WSL): uiprof.py <state> <setup keys|-> <timed key> [-r R] [-i MS] [-w MS] [-n TOP]
  setup keys: comma-separated key names, p:<text> to paste and enter, t:<text> to paste only
  (- for none)
"""
import bisect, os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB, map_symbol

args = sys.argv[1:]
state, setup, key = args.pop(0), args.pop(0), args.pop(0)
R, IV, W, TOP, IDLE_MS, WHO = 3, 2, 3000, 30, 0, None
while args:
    a = args.pop(0)
    if a == '-r': R = int(args.pop(0))
    elif a == '-i': IV = int(args.pop(0))
    elif a == '-w': W = int(args.pop(0))
    elif a == '-n': TOP = int(args.pop(0))
    elif a == "-idle": IDLE_MS = int(args.pop(0))  # idle ms before the setup (focus_warm works ahead)
    elif a == "-who": WHO = args.pop(0)  # its callers: the frame above it, per sample under it (a symbol name)

mapfile = f'{KB}/emu/states/{state}.map'
syms = []
for line in open(mapfile, errors='replace'):
    p = line.split()
    if len(p) >= 3 and p[1] == '=' and re.fullmatch(r'[0-9A-Fa-f]{6}', p[2]):
        a = int(p[2], 16)
        if a < 0xD00000 and not p[0].startswith('___len') and p[0] != '__app_version':
            syms.append((a, p[0]))
syms.sort()
addrs = [s[0] for s in syms]
sym = lambda n: map_symbol(n, mapfile=mapfile)
code_hi = sym('app.signature') or addrs[-1]
hook, tick, polls = sym('__ZN4giac14control_c_hookE'), sym('__Z15focus_busy_tickv'), sym('_getkey_polls')
u24 = lambda b, i=0: b[i] | b[i + 1] << 8 | b[i + 2] << 16
CALLS = {0xCD, 0xC4, 0xCC, 0xD4, 0xDC, 0xE4, 0xEC, 0xF4, 0xFC}
off = None
self_n, incl_n, n, ms, span, osms = {}, {}, 0, [], [], []
who_n = {}


app_lo = min(a for a, _ in syms if a >= 0x100000)  # below: the OS's entry points (_boot_..., _os_...)
IDLE = ('(OS)', '___sleep_common', '_clock', '_atomic_load_increasing_32')  # getkey's wait


def fn_of(pc):
    a = pc - off
    if not app_lo <= a < code_hi:
        return '(OS)'
    i = bisect.bisect_right(addrs, a) - 1
    return syms[i][1] if i >= 0 else '?'


def idle(f):
    return f.startswith(IDLE)


def do_setup(e):
    for s in [] if setup == '-' else re.split(r'(?<!\\),', setup):  # (\, = a comma in the text)
        s = s.replace('\\,', ',')
        if s.startswith('p:'):
            e.paste(s[2:]); e.key('enter')
        elif s.startswith('t:'):  # typed, not entered
            e.paste(s[2:])
        else:
            e.key(s)
        e.wait_idle(timeout=30000); e.run(300)


for rep in range(R):
    e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir='/tmp')
    try:
        e.run(800); e.wait_idle(timeout=5000)
        top = int(re.search(r'sp=([0-9A-F]+)', e.cmd('regs')).group(1), 16) + 1024
        if off is None:  # the app's offset: from the hook while a problem runs
            e.paste('integrate(x^7*e^x,x)'); e.key('enter')
            for _ in range(400):
                e.run(5)
                h = u24(e.peek(hook, 3))
                if h:
                    off = h - tick
                    break
            e.wait_idle(timeout=30000); e.run(300)
            e.close()
            e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir='/tmp')
            e.run(800); e.wait_idle(timeout=5000)
        if IDLE_MS:
            e.run(IDLE_MS)
        opcache = {}
        do_setup(e)
        e.cmd(f'keydown {key}')  # (sampled while held too: e.key's hold and 60 ms gap hid the work's start)
        t, quiet, incs, last, busy = 0, 0, 0, u24(e.peek(polls, 3)), 0
        first, lastb, os_t = None, None, []
        while t < W:
            e.run(IV)
            t += IV
            if t == 30:
                e.cmd(f'keyup {key}')
            r = e.cmd('regs')
            pc = int(re.search(r'pc=([0-9A-F]+)', r).group(1), 16)
            ix = int(re.search(r'ix=([0-9A-F]+)', r).group(1), 16)
            f = fn_of(pc)
            c = u24(e.peek(polls, 3))
            if os.environ.get('UIPROF_TRACE') and t <= 200:
                print(f'  t={t} {f} polls={c}', flush=True)
            if idle(f):  # getkey waits (sleep), polling every 10 ms: done when it stays so through two polls
                if f == '(OS)':  # (the OS's helpers too: divisions, multiplications called by the app)
                    os_t.append(t)
                quiet += IV
                incs += c != last
                last = c
                if quiet >= 30 and incs >= 2 and (busy or t >= 300):  # (not before the key was read)
                    break
                continue
            if first is None:
                first = t
            lastb = t
            quiet, incs, last = 0, 0, c
            busy += IV
            n += 1
            self_n[f] = self_n.get(f, 0) + 1
            seen, chain = {f}, [f]
            for _ in range(64):  # the frames: ix -> (saved ix, return address)
                if not 0xD00000 <= ix < top:
                    break
                b = e.peek(ix, 6)
                ret, ix2 = u24(b, 3), u24(b, 0)
                if app_lo <= ret - off < code_hi:
                    seen.add(fn_of(ret))
                    chain.append(fn_of(ret))
                if ix2 <= ix:
                    break
                ix = ix2
            for g in seen:
                incl_n[g] = incl_n.get(g, 0) + 1
            if WHO and WHO in chain:  # (the outermost WHO: recursion counted once)
                k = len(chain) - 1 - chain[::-1].index(WHO)
                c2 = chain[k + 1] if k + 1 < len(chain) else '(top)'
                who_n[c2] = who_n.get(c2, 0) + 1
        ms.append(busy)
        # span: the first to the last sample in the app's code (OS calls inside it included); os: those
        span.append(lastb - first + IV if first is not None else 0)
        osms.append(IV * sum(1 for x in os_t if first is not None and first <= x <= lastb))
    finally:
        e.close()


def dem(names):
    try:
        out = subprocess.run(['c++filt'], input='\n'.join(x[1:] if x.startswith('__Z') else x for x in names),
                             capture_output=True, text=True).stdout.split('\n')
        return [o[:110] for o in out]
    except OSError:
        return names


print(f'{key} after {setup}: {R} runs, busy {ms} ms; {n} samples every {IV} ms (offset {off}); span {span} ms, os {osms} ms')
for title, d in (('self (running)', self_n), ('inclusive (on the stack)', incl_n)) + ((('callers of ' + str(WHO), who_n),) if WHO else ()):
    top_n = sorted(d.items(), key=lambda x: -x[1])[:TOP]
    print(f'--- {title}')
    for (name, c), dn in zip(top_n, dem([x[0] for x in top_n])):
        print(f'{100 * c / max(n, 1):5.1f}%  {dn}')
