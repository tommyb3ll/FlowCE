#!/usr/bin/env python3
"""Wrong-answer and weird-form hunt: FlowCE's answers (the host build's -k path: src/answer.cc,
the calculator's own code) checked numerically, independently of how giac found them, and their
printed form checked for oddities.

usage (WSL): numcheck.py problems.txt [more.txt ...] [-b binary] [-o report.txt] [-q]
problems: one per line, fields split by " ; " (# comments). Kinds and checks:
  int ; f            integrate(f,x): d/dx of the answer minus f at 5 points          (~0)
  defint ; f ; a ; b integrate(f,x,a,b): the answer minus gaussquad(f,x,a,b)       (~0)
  diff ; f           diff(f,x): the answer minus a central difference at 3 points  (~0)
  lim/limr/liml ; f ; a   limit(f,x,a[,1|-1]): the answer against f near a (or at 1e6, 1e8)
  sum ; f ; n0       sum(f,n,n0,inf): the answer against partial sums to 400 and 4000
  solve ; eq         solve(eq,x): each x=v put back into the equation              (~0)
  eval ; expr ; e    expr: the answer minus e (free variables at random values)    (~0)
  raw ; expr         expr: form and time only
Every answer is also checked for: an unevaluated integrate(/sum(/limit(, undef, unbalanced
parentheses, 1*, *1, ^1, /1, +-, --, empty (), a/b/c (reads as a/(bc)), i in a real problem,
length > 120, and time (> 30 ms on the PC, roughly > 30 s on the calculator).
Crashes (AddressSanitizer) and hangs (timeout) are found and reported per problem.
Output lines: WRONG, FAIL, WEIRD, SLOW, CRASH, HANG, ?? (no numeric verdict), then a summary.
"""
import os, random, re, subprocess, sys

B = '/home/bell/hostgiac/bin/hostgiac-worktree'
TMP = '/tmp/numcheck_%d' % os.getpid()
args = sys.argv[1:]
files, out, quiet = [], None, False
while args:
    a = args.pop(0)
    if a == '-b': B = args.pop(0)
    elif a == '-o': out = args.pop(0)
    elif a == '-q': quiet = True
    else: files.append(a)
report = []


def say(s):
    report.append(s)
    if not quiet:
        print(s, flush=True)


def run(exprs, flags, timeout=900):
    """{expr: (text, ms)} from the host build; a crash or a hang is isolated: the expression
    after the last answer is recorded as ('CRASH ...'|'HANG', -1) and the rest run again"""
    res, todo = {}, list(dict.fromkeys(exprs))
    while todo:
        f = TMP + '.in'
        open(f, 'w').write('\n'.join(todo) + '\n')
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
        try:
            p = subprocess.run([B, '-p', '-T'] + flags + ['-f', f], capture_output=True, text=True,
                               env=env, timeout=timeout, cwd='/tmp')
            outp, err, hung = p.stdout, p.stderr, False
        except subprocess.TimeoutExpired as e:
            outp = (e.stdout or b'').decode() if isinstance(e.stdout, bytes) else (e.stdout or '')
            err, hung = '', True
        done, cur = 0, None
        for line in outp.split('\n'):
            if cur is None and done < len(todo) and line.startswith(todo[done] + ' -> '):
                cur = line[len(todo[done]) + 4:]
            elif cur is not None:
                cur += ' ' + line
            else:
                continue
            m = re.search(r'  @@([0-9.]+)$', cur)
            if m:  # (a multi-line answer, a matrix: its rows joined)
                res[todo[done]] = (cur[:m.start()], float(m.group(1)))
                done, cur = done + 1, None
        if done >= len(todo):
            break
        bad = todo[done]
        if hung:
            res[bad] = ('HANG', -1.0)
        else:
            summ = re.search(r'SUMMARY: AddressSanitizer: (.*)', err)
            res[bad] = ('CRASH ' + (summ.group(1)[:100] if summ else (err.strip().split('\n') or [''])[-1][:100]), -1.0)
        todo = todo[done + 1:]
    return res


def num(s):
    s = s.strip().split(', ')[-1] if s.count(', ') else s.strip()
    try:
        return float(s)
    except ValueError:
        pass
    t = s.replace(' ', '').replace('*i', 'j')
    if t.endswith('i'):
        t = t[:-1] + ('1j' if t[:-1] in ('', '+', '-') or t[-2] in '+-' else 'j')
    try:  # a+b*i: its real part when b is tiny (round-off), else 'C' (not real here)
        z = complex(t)
    except ValueError:
        return None
    return z.real if abs(z.imag) < 1e-9 else 'C'


# ---- problems
probs = []
for fn in files:
    for line in open(fn, encoding='utf-8', errors='replace'):
        line = line.split(' #')[0].rstrip('\n')
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        p = [x.strip() for x in line.split(' ; ')]
        if p[0] not in ('int', 'defint', 'diff', 'lim', 'limr', 'liml', 'sum', 'solve', 'eval', 'raw'):
            say('?? bad line: ' + line)
            continue
        probs.append(p + [''] * 3)

inputs = []
for p in probs:
    k, a = p[0], p[1:]
    inputs.append({'int': f'integrate({a[0]},x)', 'defint': f'integrate({a[0]},x,{a[1]},{a[2]})',
                   'diff': f'diff({a[0]},x)', 'lim': f'limit({a[0]},x,{a[1]})',
                   'limr': f'limit({a[0]},x,{a[1]},1)', 'liml': f'limit({a[0]},x,{a[1]},-1)',
                   'sum': f'sum({a[0]},n,{a[1]},inf)', 'solve': f'solve({a[0]},x)',
                   'eval': a[0], 'raw': a[0]}[k])
ans = run(inputs, ['-k'])

# ---- form checks
FORM = [
    (r'\b(integrate|int|sum|limit|diff|solve|desolve|series)\(', 'unevaluated'),
    (r'\bundef\b', 'undef'), (r'(?<![\w.])1\*', '1*'), (r'\*1(?![\w.(^])', '*1'), (r'\^1(?![\w.(])', '^1'),
    (r'/1(?![\w.(])', '/1'), (r'\+-|-\+|--', 'sign pair'), (r'\(\)', 'empty ()'),
    (r'/[A-Za-z0-9_.^]+(\([^()]*\))?/[A-Za-z0-9(]', 'a/b/c'), (r'(?<![\w])i(?![\w(])', 'complex i'),
    (r'\bexp\(1\)', 'exp(1)'), (r'\bsqrt\((0|1|4|9|16|25)\)', 'sqrt of a square'),
]


def form_issues(t):
    iss = [name for pat, name in FORM if re.search(pat, t)]
    d = 0
    for c in t:
        d += (c in '([') - (c in ')]')
        if d < 0: break
    if d != 0: iss.append('unbalanced')
    if len(t) > 120: iss.append('long %d' % len(t))
    return iss


# ---- numeric checks: giac expressions, evaluated with plain giac (the -i pre-pass: 2x, xsin(x))
PTS = ['0.37', '0.83', '1.29', '2.17', '-0.61']
checks = {}  # check -> (index, what, tol)


def add(i, c, what, tol):
    checks.setdefault(c, []).append((i, what, tol))


ANSWER_TEXT = {}
for i, (p, inp) in enumerate(zip(probs, inputs)):
    k, a = p[0], p[1:]
    r, ms = ans.get(inp, ('', -1))
    ANSWER_TEXT[i] = r
    if ms < 0 or not r or r.startswith('does not exist') or r.startswith('diverges') or r.startswith('no ') \
            or r in ('Done', 'Interrupted', 'Out of memory', 'Graphic object'):
        continue
    rv = r.split('  ~ ')[0]
    if k == 'int':
        for x0 in PTS:
            add(i, f'evalf(subst(diff({rv},x)-({a[0]}),x={x0}))', 'd/dx at ' + x0, 1e-6)
    elif k == 'defint':
        add(i, f'evalf(({rv})-gaussquad({a[0]},x,{a[1]},{a[2]}))', 'gaussquad', 1e-4)
    elif k == 'diff':
        for x0 in PTS[:3]:
            add(i, f'evalf(subst({rv},x={x0})-(subst({a[0]},x={x0}+1e-6)-subst({a[0]},x={x0}-1e-6))/2e-6)', 'central diff at ' + x0, 1e-3)
    elif k in ('lim', 'limr', 'liml'):
        if a[1] in ('inf', '+inf', 'infinity', '+infinity'):
            pts = ['1e6', '1e8']
        elif a[1] in ('-inf', '-infinity'):
            pts = ['-1e6', '-1e8']
        else:
            sides = {'lim': '+-', 'limr': '+', 'liml': '-'}[k]
            pts = [f'({a[1]}){sd}{h}' for sd in sides for h in ('1e-3', '1e-4', '1e-6')]
        if 'infinity' in rv:
            for x0 in pts:
                if x0.endswith('1e-6') or x0.endswith('1e8'):
                    add(i, f'evalf(subst({a[0]},x={x0}))', 'f near a (infinite answer) ' + rv, 0)
        else:
            for x0 in pts:
                add(i, f'evalf(({rv})-subst({a[0]},x={x0}))', 'lim f at ' + x0, 1e-2)
    elif k == 'sum':
        n0 = a[1]
        if 'infinity' in rv:
            add(i, f's:=0.0;for(k:={n0}+200;k<={n0}+400;k++){{s:=s+evalf(subst({a[0]},n,k));}};s', 'terms 200..400 (infinite answer)', 0)
        else:
            for N in (400, 4000):
                add(i, f's:=evalf({rv});for(k:={n0};k<={n0}+{N};k++){{s:=s-evalf(subst({a[0]},n,k));}};s', 'S-S%d' % N, 0)
    elif k == 'solve':
        eq = a[0]
        if '<' in eq or '>' in eq:
            continue  # inequalities: form only
        lhs, rhs = (eq.split('=', 1) + ['0'])[:2] if '=' in eq else (eq, '0')
        sols = re.findall(r'x=([^,]+(?:\([^()]*\))?[^,]*)', rv)
        for s in sols[:6]:
            add(i, f'evalf(subst(({lhs})-({rhs}),x={s}))', 'x=' + s[:30] + ' put back', 1e-6)
    elif k == 'eval' and a[1]:
        ids = sorted(set(re.findall(r'\b([a-zA-Z])\b', rv + ' ' + a[1])) - set('ei'))
        sub = ','.join(f'{v}={random.Random(v).uniform(0.3, 1.7):.3f}' for v in ids if v not in 'nt') if ids else ''
        e = f'evalf(({rv})-({a[1]}))' if not sub else f'evalf(subst(({rv})-({a[1]}),[{sub}]))'
        add(i, e, 'answer minus expected', 1e-6)

vals = run(list(checks), ['-i'])

# ---- verdicts
bad = {}
for c, lst in checks.items():
    v, ms = vals.get(c, ('', -1))
    for i, what, tol in lst:
        x = num(v)
        if x == 'C':  # complex: outside the real domain at this point, no verdict
            continue
        if what.startswith('f near a (infinite answer)'):
            if x is not None and abs(x) < 1e4:
                bad.setdefault(i, []).append(f'WRONG? infinite answer but f={x:.4g} near a')
            continue
        if what.startswith('terms 200..400'):
            if x is not None and abs(x) < 1e-3:
                bad.setdefault(i, []).append(f'WRONG? +-infinity but the terms 200..400 add to {x:.3g}')
            continue
        if what in ('S-S400', 'S-S4000') or what.startswith('lim f at'):
            continue
        if x is None:
            if v.startswith('CRASH') or v == 'HANG':
                bad.setdefault(i, []).append(f'?? check {what}: {v}')
            elif 'undef' not in v and 'infinity' not in v and v != '':
                bad.setdefault(i, []).append(f'?? {what}: {v[:60]}')
            continue
        if abs(x) > tol * (1 + 0) and abs(x) > 1e-9:
            bad.setdefault(i, []).append(f'WRONG? {what}: off by {x:.3g}')
# limits: the best of the points of a side within 1e-2 (relative)
lims = {}
for c, lst in checks.items():
    for i, what, tol in lst:
        if what.startswith('lim f at'):
            x = num(vals.get(c, ('', -1))[0])
            side = '+' if '+1e-' in what else '-'
            if isinstance(x, float):
                lims.setdefault((i, side), []).append(abs(x))
for (i, side), xs in lims.items():
    L = num(ANSWER_TEXT[i].split('  ~ ')[0])
    if min(xs) > 1e-2 * (1 + abs(L if isinstance(L, float) else 0)):
        bad.setdefault(i, []).append(f'WRONG? f near a ({side}) stays {min(xs):.3g} away')
# sums: the partial sums S400, S4000 approach the answer
for c, lst in checks.items():
    for i, what, tol in lst:
        if what == 'S-S400':
            d400 = num(vals.get(c, ('', -1))[0])
            c2 = c.replace('+400;', '+4000;')
            d4000 = num(vals.get(c2, ('', -1))[0])
            if d400 is not None and d4000 is not None and abs(d4000) > 1e-3 and abs(d4000) > 0.2 * abs(d400) - 1e-9:
                bad.setdefault(i, []).append(f'WRONG? partial sums do not approach it: S-S400={d400:.3g}, S-S4000={d4000:.3g}')

nw = nf = nweird = nslow = ncrash = 0
for i, (p, inp) in enumerate(zip(probs, inputs)):
    r, ms = ans.get(inp, ('', -1))
    tag = []
    if r.startswith('CRASH') or r == 'HANG':
        say(f'{r.split()[0]}  {inp}  [{r}]')
        ncrash += 1
        continue
    iss = form_issues(r)
    if 'unevaluated' in iss or 'undef' in iss:
        say(f'FAIL  {inp}  ->  {r}')
        nf += 1
    elif iss:
        say(f'WEIRD {inp}  ->  {r}   [{", ".join(iss)}]')
        nweird += 1
    for b in bad.get(i, []):
        say(f'{"WRONG" if b.startswith("WRONG") else "??   "} {inp}  ->  {r}   [{b}]')
        nw += b.startswith('WRONG')
    if ms > 30:
        say(f'SLOW  {inp}  ->  {r[:80]}   [{ms:.0f} ms on the PC]')
        nslow += 1
say(f'{len(probs)} problems, {len(checks)} checks: {nw} wrong?, {nf} failed, {nweird} weird, {nslow} slow, {ncrash} crashed/hung')
if out:
    open(out, 'w').write('\n'.join(report) + '\n')
    # all answers, for reading
    open(out + '.answers', 'w').write('\n'.join(f'{inputs[i]} -> {ANSWER_TEXT.get(i, "")}' for i in range(len(probs))) + '\n')
