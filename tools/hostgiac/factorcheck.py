#!/usr/bin/env python3
"""Factoring-steps checker: every displayed step of factor(...) proved algebraically valid by a
second evaluation that does not go through src/steps.cc (the host binary's plain evaluator, -p -f).

usage: factorcheck.py <binary> <corpus> [more ...] [-q] [-j N] [--golden]
       factorcheck.py --selftest <binary> <dir>
corpus: one problem per line, factor(...) (# comments and blank lines skipped), an optional
  "  # expect: steps|nosteps-ok|prime [; factors it was built from [; note]]". A line that is not
  exactly factor(...) with balanced parentheses, or has an unknown tag, is a FAIL (malformed line).
  With a factor list and steps the list's product must equal the input and (tag steps) have as many
  non-constant factors as the last product.
  --known FILE: problems tagged steps or prime that have none today: KNOWN-NOSTEPS, not a failure; if one
  now has steps it is checked and reported NOW-HAS-STEPS.   --known-wrong FILE: problems (one per line)
  that fail a check because of an engine defect written up in a report: FAIL is shown as KNOWN-WRONG,
  not counted as a failure, and the last line says how many there are.   --golden: the files are golden files (their
  "== problem   [...]" headers are the problems, no tags).
Each problem runs as `<binary> -S -e "<problem>"` (60 s timeout). Its lines are classified by the
step contract (the shapes below) and checked:
  identity chain  A=B=C        every side equals the first (normal(A-B) is 0); its left side is the
                               input or divides it; the last chain that starts at the input ends
                               at the input; its non-constant factors are as many as the answer's
  facts           p*q=r,p+q=s  each equality true
  roots           x=(3+-sqrt(5))/2   counted, not expanded (the identity line after them is checked)
  bare polynomial              equals the input (or one of its divisors)
  "does not factor" step       only when the FlowCE answer is the input itself, unfactored: the same
                               expansion, one non-constant factor, no number pulled out
  "Rearrange the terms" line   both sides have the same four terms
  final product                as many non-constant factors as the plain evaluator's factor(input)
  "A rational root" line       p(r)=0 as written: true (an identity chain ending in 0)
  "Quadratic form" line        P=Q,u=x^k: Q with u put back as x^k is P, and P divides the input
  "SOAP" line                  +b,-ab,+b^2: the sign of b is the cubes' (sum +, difference -), the middle term's
                               is the opposite, the last is +; b is a term of the first factor of the cubes
                               line before it, the other two are terms of its second factor
  "Check: multiply the factors" (P)=(Q): the last step of every result that has steps (not "does not
                               factor"); P is the last product of the chain before it (as many non-constant
                               factors as the answer), P = Q, and Q is the input
Output: one line per problem  PASS|FAIL|NOSTEPS  problem  [reason], then
  N problems: p pass, f fail, n nosteps   and a line of what was verified.  Exit 1 on any FAIL.
--selftest: <dir>/*.txt are saved -S outputs, the first line "# selftest: pass" or
  "# selftest: fail <words of the reason>"; they go through the same parser and checks and must be
  flagged exactly as declared.
"""
import os, re, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor

TIMEOUT = 60
STATS = dict(identity=0, sides=0, facts=0, roots=0, bare=0, chains=0, divides=0, counts=0, flists=0, qform=0, soap=0, check=0)
TAGS = ('steps', 'nosteps-ok', 'prime')


# ---------------------------------------------------------------- text helpers
def toplevel(s, seps):
    """positions of the characters of `seps` outside any bracket"""
    d, out = 0, []
    for i, c in enumerate(s):
        if c in '([{':
            d += 1
        elif c in ')]}':
            d -= 1
        elif d == 0 and c in seps:
            out.append(i)
    return out


def split_top(s, sep):
    cuts = [i for i in toplevel(s, sep)]
    if sep == '=':  # (not <=, >=, ==, !=)
        cuts = [i for i in cuts if (i == 0 or s[i - 1] not in '<>!=') and (i + 1 >= len(s) or s[i + 1] != '=')]
    parts, last = [], 0
    for i in cuts:
        parts.append(s[last:i])
        last = i + 1
    parts.append(s[last:])
    return [p.strip() for p in parts]


def explicit(s):
    """a factor list as a corpus writes it (3x*y*(2x+3y), -(x+1)^3): a digit or a ) followed by a
    letter, a digit or ( is a product, as the engine's pre-pass reads it"""
    s = re.sub(r'(\d)([A-Za-z(])', r'\1*\2', s)
    return re.sub(r'\)([A-Za-z\d(])', r')*\1', s)


def balanced_call(s):
    """s is factor( ... ) and the first ( closes at the last character"""
    if not (s.startswith('factor(') and s.endswith(')')):
        return False
    d = 0
    for i, c in enumerate(s[6:], 6):
        d += (c == '(') - (c == ')')
        if d == 0 and i < len(s) - 1:
            return False
        if d < 0:
            return False
    return d == 0


def implicit(s):
    """giac prints 2000(x-1) and (a)(b) for products; make the multiplication explicit"""
    return re.sub(r'(\d)\(', r'\1*(', re.sub(r'\)\(', ')*(', s))


def has_var(s):
    return bool(re.search(r'\b(?!pi\b)[A-Za-z_]\w*\b(?!\s*\()', s))


def strip_parens(s):
    s = s.strip()
    while s.startswith('(') and s.endswith(')'):
        d = 0
        for i, c in enumerate(s):
            d += (c == '(') - (c == ')')
            if d == 0 and i < len(s) - 1:
                return s
        s = s[1:-1].strip()
    return s


def count_factors(e):
    """the number of non-constant factors of a product as printed (a power counted by its
    exponent, a sum or a variable as one, numbers as none): the engine's nfactors, on text"""
    e = strip_parens(implicit(e))
    if e.startswith('-'):
        e = e[1:]
    if not has_var(e):
        return 0
    if [i for i in toplevel(e, '+-') if i > 0 and e[i - 1] not in '*/^(']:
        return 1  # a sum
    n = 0
    for piece in split_top(e, '*'):
        for k, q in enumerate(split_top(piece, '/')):
            if k:
                continue  # (a denominator: not a factor to count)
            q = q.strip()
            if q.startswith('-'):
                q = q[1:]
            pw = split_top(q, '^')  # (before the parentheses go: (2*x-y^2) is one sum, not a square)
            q = strip_parens(q)
            if len(pw) == 2 and re.fullmatch(r'\d+', strip_parens(pw[1])) and has_var(pw[0]):
                n += int(strip_parens(pw[1])) * count_factors(pw[0])
            elif len(split_top(q, '*')) > 1:
                n += count_factors(q)
            else:
                n += has_var(q)
    return n


# ---------------------------------------------------------------- parsing of -S output
class Step:
    def __init__(self, title, depth, math, cont):
        self.title, self.depth, self.math, self.cont = title, depth, math, cont


class Result:
    def __init__(self):
        self.problem, self.status, self.nsteps, self.steps, self.answer = '', '', 0, [], ''


HEAD = re.compile(r'^== (.*?)   \[(ok|NO STEPS|[^,\]]*)(?:, (\d+) steps(?:, [\d.]+ ms)?)?\]$')


def parse(text):
    r = Result()
    base = ''  # (the title a "= ..." line continues: the SOAP, quadratic form and check lines are not continued, the step before them is)
    for line in text.split('\n'):
        line = line.rstrip('\r')
        m = HEAD.match(line)
        if m:
            r.problem, r.status = m.group(1), m.group(2)
            r.nsteps = int(m.group(3)) if m.group(3) else 0
            continue
        if line.startswith('   FlowCE answer: '):
            r.answer = line[len('   FlowCE answer: '):]
            continue
        m = re.match(r'^( +)= (.*)$', line)
        if m and r.steps and len(m.group(1)) % 4 == 2:
            r.steps.append(Step(base, (len(m.group(1)) - 2) // 4, m.group(2), True))
            continue
        m = re.match(r'^( +)(\S.*?):  (\S.*)$', line)
        if m and len(m.group(1)) % 4 == 2 and not line.startswith('   '):
            r.steps.append(Step(m.group(2), (len(m.group(1)) - 2) // 4, m.group(3), False))
            if not m.group(2).startswith(('SOAP', 'Quadratic form', 'Check:')):
                base = m.group(2)
    return r


# ---------------------------------------------------------------- classification and checks
def kind_of(title, math, in_roots):
    """'bare' | 'roots' | 'facts' | 'identity' | 'qform' | 'soap' | 'check' | None (not recognised)"""
    for k, t in (('qform', 'Quadratic form'), ('soap', 'SOAP'), ('check', 'Check:')):
        if title.startswith(t):
            return k
    if re.match(r'(No real roots|Does not factor)', title) and not split_top(math, '='):
        return 'bare'
    if '±' in math or (in_roots and re.match(r'^[A-Za-z]\w*=', math)) or \
            (title.startswith('No whole numbers') and re.match(r'^[A-Za-z]\w*=', math)):
        return 'roots'
    eqs = split_top(math, '=')
    commas = split_top(math, ',')
    if len(commas) > 1 and all(len(split_top(c, '=')) > 1 for c in commas):
        return 'facts'
    if len(eqs) > 1 and len(commas) == 1:
        return 'identity'
    if len(eqs) == 1 and len(commas) == 1:
        return 'bare'
    return None


class Check:
    """one giac expression and what is to be proved: zero (result "0"), whole (result is a number
    or a quotient of numbers: a divisor test) or is_input (result "0": informational)"""
    def __init__(self, expr, what, line_no, mode='zero'):
        self.expr, self.what, self.line_no, self.mode, self.value = expr, what, line_no, mode, None


def terms_of(side):
    """the signed terms of a sum as text, sorted (blanks and one pair of outer parentheses dropped)"""
    side = strip_parens(side.replace(' ', ''))
    cuts = [i for i in toplevel(side, '+-') if i > 0 and side[i - 1] not in '*/^(']
    parts, last = [], 0
    for i in cuts:
        parts.append(side[last:i])
        last = i
    parts.append(side[last:])
    return sorted(p[1:] if p.startswith('+') else p for p in parts)


def rearrange_error(sides):
    """a "Rearrange the terms" line: the same four terms on both sides, in another order"""
    if len(sides) != 2:
        return 'rearrange line is not "A=B"'
    a, b = terms_of(sides[0]), terms_of(sides[1])
    if len(a) != 4:
        return 'rearranged sum has %d terms, not 4' % len(a)
    if a != b:
        return 'the two sides do not have the same four terms'
    return None


def unfactored_error(ans):
    """why a FlowCE answer cannot be "does not factor" (None when it can): it must be the input as
    one non-constant factor with no number pulled out (no product, no quotient, no leading
    minus sign in front of a bracket)"""
    a = strip_parens(implicit(ans)) if ans else ''
    if not a:
        return None
    if count_factors(a) != 1:
        return 'has %d non-constant factors' % count_factors(a)
    def is_sum(e):
        e = strip_parens(e)
        return bool([i for i in toplevel(e, '+-') if i > 0 and e[i - 1] not in '*/^('])
    if is_sum(a):
        return None
    pieces = split_top(a, '*') + split_top(a, '/')
    if any(not has_var(q) for q in pieces) and any(is_sum(q) for q in pieces):
        return 'is a product or quotient (a number pulled out)'
    if a.startswith('-') and a[1:].startswith('(') and is_sum(a[1:]):
        return 'has a minus sign pulled out'
    return None


def soap_errors(steps, n, where):
    """a SOAP line "+b,-ab,+b^2" after a sum or difference of cubes line: the signs are the rule's and
    the three terms are terms of the cubes line's two factors"""
    k = n - 1
    while k > 0 and steps[k].cont:  # (the whole line after the cubes line comes first when other factors are there)
        k -= 1
    prev = steps[k] if n else None
    if not prev or prev.title not in ('Sum of cubes', 'Difference of cubes'):
        return ['%s: SOAP line is not right after a sum or difference of cubes line' % where]
    parts = split_top(steps[n].math, ',')
    if len(parts) != 3 or not all(re.match(r'^[+-]\S+$', p) for p in parts):
        return ['%s: SOAP line is not three signed terms: %s' % (where, steps[n].math[:60])]
    fs = split_top(strip_parens(implicit(split_top(prev.math, '=')[-1])), '*')
    if len(fs) != 2:
        return ['%s: the cubes line does not end in two factors' % where]
    sm = prev.title.startswith('Sum')
    errs = []
    if parts[0][0] != ('+' if sm else '-') or parts[1][0] != ('-' if sm else '+') or parts[2][0] != '+':
        errs.append('%s: SOAP signs are wrong for a %s of cubes: %s' % (where, 'sum' if sm else 'difference', steps[n].math[:60]))
    t = [x.lstrip('+') for x in parts]
    if t[0] not in terms_of(fs[0]) or t[1] not in terms_of(fs[1]) or t[2] not in terms_of(fs[1]):
        errs.append('%s: SOAP terms are not terms of the factors %s: %s' % (where, prev.math.split('=')[-1][:50], steps[n].math[:40]))
    return errs


def check_errors(r, n, where, inp, checks):
    """the "Check: multiply the factors" step (P)=(Q): last, after a chain whose last product is P, and
    P = Q = the input (the last two by evaluation)"""
    errs = []
    s = r.steps[n]
    if n != len(r.steps) - 1:
        errs.append('%s: the check step is not the last step' % where)
    sides = split_top(s.math, '=')
    if len(sides) != 2:
        return errs + ['%s: check step is not "product=polynomial": %s' % (where, s.math[:60])]
    prev = [x for x in r.steps[:n] if kind_of(x.title, x.math, False) == 'identity']
    if not prev:
        return errs + ['%s: nothing to check: no chain before the check step' % where]
    last = split_top(prev[-1].math, '=')[-1]
    checks.append(Check('normal((%s)-(%s))' % (implicit(sides[0]), implicit(last)), '%s: the check step does not multiply the last product %s: %s' % (where, last[:40], sides[0][:40]), n))
    if r.answer and count_factors(sides[0]) != count_factors(r.answer):
        errs.append('%s: the check step has %d non-constant factors, the answer %d' % (where, count_factors(sides[0]), count_factors(r.answer)))
    a, b = implicit(sides[0]), implicit(sides[1])
    checks.append(Check('normal((%s)-(%s))' % (a, b), '%s: the product is not equal to the polynomial: %s' % (where, s.math[:80]), n))
    checks.append(Check('normal((%s)-(%s))' % (inp, b), '%s: the polynomial of the check step is not the input: %s' % (where, sides[1][:60]), n))
    return errs


def build(r, factors=None):
    """(checks, errors, info) for a parsed result: the giac expressions to evaluate and the
    problems found without any"""
    checks, errors = [], []
    inp = implicit(r.problem[len('factor('):-1]) if r.problem.startswith('factor(') else None
    if inp is None:
        return checks, ['not a factor(...) problem'], {}
    info = dict(ident=0, facts=0, roots=0, bare=0, qform=0, soap=0, check=0)
    in_roots = False
    for n, s in enumerate(r.steps):
        if not s.cont:
            in_roots = s.title.startswith('No whole numbers')
        kind = kind_of(s.title, s.math, in_roots)
        where = 'line %d' % (n + 1)
        if kind is None:
            errors.append('%s: not a contract shape: %s' % (where, s.math[:60]))
        elif kind == 'roots':
            info['roots'] += 1
        elif kind in ('qform', 'soap', 'check'):
            if s.cont:
                errors.append('%s: a "%s" line cannot be continued' % (where, s.title[:20]))
                continue
            info[kind] += 1
            if kind == 'qform':
                m = re.match(r'^(.+?)=(.+),(\w+)=(.+)$', s.math)
                if not m:
                    errors.append('%s: quadratic form line is not "P=Q,u=x^k": %s' % (where, s.math[:60]))
                    continue
                p, q, u, sb = (implicit(g) for g in m.groups())
                checks.append(Check('normal((%s)-(subst(%s,%s,%s)))' % (p, q, u, sb), '%s: Q with %s put back is not P: %s' % (where, u, s.math[:70]), n))
                checks.append(Check('denom(normal((%s)/(%s)))' % (inp, p), '%s: P does not divide the input: %s' % (where, p[:60]), n, 'divides'))
            elif kind == 'soap':
                errors.extend(soap_errors(r.steps, n, where))
            else:
                errors.extend(check_errors(r, n, where, inp, checks))
        elif kind == 'bare':
            info['bare'] += 1
            m = implicit(s.math)
            if re.match(r'(No real roots|Does not factor)', s.title):
                err = unfactored_error(r.answer)
                if err:
                    errors.append('%s: "%s" step but the answer %s: %s' % (where, s.title[:20], err, r.answer[:60]))
                elif r.answer:
                    checks.append(Check('normal((%s)-(%s))' % (inp, implicit(r.answer)), '%s: "does not factor" step but the answer is not the input expanded' % where, n, 'dnf'))
            checks.append(Check('normal((%s)-(%s))' % (inp, m), '%s: bare polynomial differs from the input' % where, n, 'bare0'))
            checks.append(Check('denom(normal((%s)/(%s)))' % (inp, m), '%s: bare polynomial does not divide the input' % where, n, 'bare1'))
        elif kind == 'facts':
            info['facts'] += 1
            for c in split_top(s.math, ','):
                a, b = split_top(c, '=')[:2]
                checks.append(Check('normal((%s)-(%s))' % (implicit(a), implicit(b)), '%s: false fact %s' % (where, c), n, 'fact'))
        else:
            info['ident'] += 1
            sides = [implicit(x) for x in split_top(s.math, '=')]
            if s.title.startswith('Rearrange the terms') and not s.cont:
                err = rearrange_error(sides)
                if err:
                    errors.append('%s: %s: %s' % (where, err, s.math[:70]))
            for i, o in enumerate(sides[1:], 1):
                checks.append(Check('normal((%s)-(%s))' % (sides[0], o), '%s: side %d is not equal to the first side: %s' % (where, i + 1, s.math[:90]), n))
            checks.append(Check('normal((%s)-(%s))' % (inp, sides[0]), where, n, 'isinput'))
            checks.append(Check('denom(normal((%s)/(%s)))' % (inp, sides[0]), '%s: left side does not divide the input: %s' % (where, sides[0][:60]), n, 'divides'))
    if r.steps:
        checks.append(Check('factor(%s)' % inp, 'plain factor', -2, 'pfactor'))
    if factors:
        checks.append(Check('normal((%s)-(%s))' % (inp, explicit(factors)), 'factor list: its product is not the input: %s' % factors[:60], -1, 'flist'))
    return checks, errors, info


def evaluate(binary, exprs):
    """{index: result text} from the plain evaluator, one process for all of them"""
    if not exprs:
        return {}
    fd, path = tempfile.mkstemp(suffix='.in', prefix='factorcheck_')
    os.close(fd)
    try:
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write('\n'.join(exprs) + '\n')
        p = subprocess.run([binary, '-p', '-f', path], capture_output=True, timeout=TIMEOUT * 2, cwd=tempfile.gettempdir())
        out = p.stdout.decode('utf-8', 'replace').split('\n')
    finally:
        os.unlink(path)
    res, k = {}, 0
    for line in out:
        line = line.rstrip('\r')
        if k < len(exprs) and line.startswith(exprs[k] + ' -> '):
            res[k] = line[len(exprs[k]) + 4:].strip()
            k += 1
    return res


NUM = re.compile(r'^-?\d+(/\d+)?$')


def judge(r, checks, errors, info, values, tag, factors=None):
    """(verdict, reason, stats) from the evaluated checks"""
    st = dict(identity=0, sides=0, facts=info['facts'], roots=info['roots'], bare=0, chains=0, divides=0, counts=0, flists=0,
              qform=info['qform'], soap=info['soap'], check=info['check'])
    for c, v in zip(checks, values):
        c.value = v
    fails = list(errors)
    inputs = {}
    for c in checks:
        if c.value is None:
            fails.append('%s: evaluator gave no result' % c.what)
        elif c.mode == 'flist':
            if c.value != '0':
                fails.append('%s [= %s]' % (c.what, c.value[:40]))
            else:
                st['flists'] += 1
        elif c.mode in ('zero', 'fact', 'dnf'):
            if c.value != '0':
                fails.append('%s [= %s]' % (c.what, c.value[:40]))
            else:
                st['sides'] += c.mode == 'zero'
        elif c.mode == 'isinput':
            inputs[c.line_no] = (c.value == '0')
        elif c.mode == 'divides':
            if not NUM.match(c.value):
                # (a constant denominator is fine: 1/2*(x^2-4) over x^2-4 is 1/2)
                fails.append('%s [quotient has denominator %s]' % (c.what, c.value[:40]))
            else:
                st['divides'] += 1
        elif c.mode == 'bare0':
            inputs[('bare', c.line_no)] = (c.value == '0')
        elif c.mode == 'bare1':
            ok = inputs.get(('bare', c.line_no)) or NUM.match(c.value or '')
            if not ok:
                fails.append(c.what)
            else:
                st['bare'] += 1
    # the identity lines: how many verified, and the last one that starts at the input
    idents = [(n, s) for n, s in enumerate(r.steps) if kind_of(s.title, s.math, s.title.startswith('No whole numbers')) == 'identity']
    st['identity'] = len([1 for (n, _) in idents])
    start = [n for (n, _) in idents if inputs.get(n)]
    if r.status == 'ok' and idents and not start:
        fails.append('no line shows input = product')
    if r.status == 'ok' and start:
        st['chains'] += 1
        last = r.steps[start[-1]]
        final = split_top(last.math, '=')[-1]
        nf, na = count_factors(final), count_factors(r.answer)
        if r.answer and nf != na:
            fails.append('last product has %d non-constant factors, the answer %d: %s' % (nf, na, final[:60]))
        else:
            st['counts'] += 1
        pf = [c.value for c in checks if c.mode == 'pfactor']
        if pf and pf[0] is not None:
            np_ = count_factors(pf[0])
            if np_ != nf:
                fails.append('last product has %d non-constant factors, the plain evaluator factor() %d: %s' % (nf, np_, pf[0][:60]))
            else:
                st['counts'] += 1
        if factors and tag == 'steps':
            nl = count_factors(explicit(factors))
            if nl != nf:
                fails.append('the factor list has %d non-constant factors, the last product %d: %s' % (nl, nf, final[:60]))
    if r.status == 'ok' and r.steps and not re.match(r'(No real roots|Does not factor)', r.steps[0].title):
        nck = len([1 for x in r.steps if x.title.startswith('Check:')])
        if nck != 1 or not r.steps[-1].title.startswith('Check:'):
            fails.append('a result with steps must end with exactly one "Check: multiply the factors" step (%d found)' % nck)
    if r.status == 'ok' and r.nsteps != len(r.steps):
        fails.append('header says %d steps, %d lines found' % (r.nsteps, len(r.steps)))
    if tag == 'prime':
        if not (len(r.steps) == 1 and re.match(r'(No real roots|Does not factor)', r.steps[0].title)):
            fails.append('expect prime: %d steps, first "%s"' % (len(r.steps), r.steps[0].title if r.steps else ''))
    return fails, st


def check_one(binary, problem, tag, text=None, factors=None, known=()):
    """(verdict, reason, stats, had_steps) for one problem; text: a saved -S output (self-test).
    Verdicts: PASS, FAIL, NOSTEPS, KNOWN-NOSTEPS (tagged steps or prime, listed in the known file),
    NOW-HAS-STEPS (listed there but has steps: checked like any other, PASS if it holds)"""
    if text is None:
        try:
            p = subprocess.run([binary, '-S', '-e', problem], capture_output=True, timeout=TIMEOUT, cwd=tempfile.gettempdir())
            text = p.stdout.decode('utf-8', 'replace')
            if p.returncode != 0 and '== ' not in text:
                return 'FAIL', 'crash (exit %d)' % p.returncode, None, False
        except subprocess.TimeoutExpired:
            return 'FAIL', 'timeout (%d s)' % TIMEOUT, None, False
    r = parse(text)
    if not r.status:
        return 'FAIL', 'no header in the output', None, False
    if problem and r.problem and r.problem != problem:
        return 'FAIL', 'output is for %s' % r.problem, None, False
    if r.status != 'ok':
        if r.status == 'NO STEPS':
            if tag in ('steps', 'prime') and problem in known:
                return 'KNOWN-NOSTEPS', '', None, False
            if tag in ('steps', 'prime'):
                return 'FAIL', 'expect: %s but NO STEPS' % tag, None, False
            return 'NOSTEPS', '', None, False
        return 'FAIL', 'status %s' % r.status, None, False
    if not r.steps:
        return 'FAIL', 'ok but no step lines parsed', None, True
    checks, errors, info = build(r, factors)
    try:
        res = evaluate(binary, [c.expr for c in checks])
        values = [res.get(i) for i in range(len(checks))]
    except subprocess.TimeoutExpired:
        return 'FAIL', 'evaluator timeout', None, True
    fails, st = judge(r, checks, errors, info, values, tag, factors)
    if fails:
        return 'FAIL', '; '.join(fails[:3]) + (' (+%d more)' % (len(fails) - 3) if len(fails) > 3 else ''), st, True
    return ('NOW-HAS-STEPS' if tag in ('steps', 'prime') and problem in known else 'PASS'), '', st, True


# ---------------------------------------------------------------- corpora
def read_corpus(files, golden):
    """[(problem, tag, factors, error)]. A line: factor(...)  [# expect: TAG [; factors [; note]]]"""
    out = []
    for fn in files:
        for line in open(fn, encoding='utf-8', errors='replace'):
            line = line.rstrip('\r\n')
            if golden:
                m = re.match(r'^== (.*?)   \[', line)
                if m:
                    out.append((m.group(1), None, None, None))
                continue
            if not line.strip() or line.lstrip().startswith('#'):
                continue
            prob, tag, fl, err = line.strip(), None, None, None
            m = re.match(r'^(.*?)\s*#\s*(.*)$', line)
            if m:
                prob = m.group(1).strip()
                t = re.match(r'^expect:\s*(\S+)\s*(?:;\s*(.*))?$', m.group(2))
                if not t:
                    err = 'comment is not "expect: TAG [; factors]": # ' + m.group(2)[:40]
                else:
                    tag = t.group(1)
                    fl = t.group(2).split(' ; ')[0].strip() if t.group(2) else None
                    if tag not in TAGS:
                        err = 'unknown tag "%s"' % tag
            if err is None and not balanced_call(prob):
                err = 'the problem is not exactly factor(...) with balanced parentheses'
            out.append((prob, tag, fl or None, err))
    return out


def read_corpus_line(line):
    fd, path = tempfile.mkstemp(suffix='.txt', prefix='factorcheck_line_')
    os.close(fd)
    try:
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(line + '\n')
        return read_corpus([path], False)[0]
    finally:
        os.unlink(path)


def tally(total, st):
    if st:
        for k, v in st.items():
            total[k] += v


def selftest(binary, d):
    names = sorted(f for f in os.listdir(d) if f.endswith('.txt'))
    bad, nbad, ngood = 0, 0, 0
    for fn in names:
        text = open(os.path.join(d, fn), encoding='utf-8').read()
        lines = text.split('\n')
        m = re.match(r'# selftest: (pass|fail)(.*)', lines[0])
        if not m:
            print('FAIL  %s  no "# selftest:" first line' % fn)
            bad += 1
            continue
        want, words = m.group(1), m.group(2).strip().split()
        if fn.startswith('line_'):  # line 2: a corpus line (tag, factor list, malformed); the rest: its -S output
            prob, tag, fl, err = read_corpus_line(lines[1])
            if err:
                verdict, reason = 'FAIL', 'malformed line: ' + err
            else:
                verdict, reason = check_one(binary, prob, tag, '\n'.join(lines[2:]), fl)[:2]
        else:
            verdict, reason = check_one(binary, None, None, text)[:2]
        good = (verdict == 'PASS') if want == 'pass' else (verdict == 'FAIL' and all(w in reason for w in words))
        if want == 'pass':
            ngood += 1
        else:
            nbad += 1
        print('%s  %s  (expected %s: checker said %s %s)' % ('PASS' if good else 'FAIL', fn, want, verdict, reason[:100]))
        bad += not good
    if not nbad or not ngood:
        print('FAIL  the self-test needs both right and wrong outputs')
        bad += 1
    print('selftest: %d outputs (%d right, %d wrong), %d mistakes' % (len(names), ngood, nbad, bad))
    return 1 if bad else 0


def main(argv):
    quiet, jobs, golden, files, binary, self_dir, known, wrong = False, 8, False, [], None, None, set(), set()
    a = list(argv)
    while a:
        x = a.pop(0)
        if x == '-q':
            quiet = True
        elif x == '-j':
            jobs = int(a.pop(0))
        elif x == '--golden':
            golden = True
        elif x == '--known':  # the problems tagged steps or prime that have none today (not a failure)
            for ln in open(a.pop(0), encoding='utf-8'):
                if ln.strip() and not ln.startswith('#'):
                    known.add(ln.rstrip('\r\n'))
        elif x == '--known-wrong':
            for ln in open(a.pop(0), encoding='utf-8'):
                if ln.strip() and not ln.startswith('#'):
                    wrong.add(ln.rstrip('\r\n'))
        elif x == '--selftest':
            self_dir = True
        elif binary is None:
            binary = x
        else:
            files.append(x)
    if binary is None or not files:
        print(__doc__)
        return 2
    if self_dir:
        return selftest(binary, files[0])
    probs = read_corpus(files, golden)

    def one(pt):
        if pt[3]:
            return 'FAIL', 'malformed line: ' + pt[3], None, False
        return check_one(binary, pt[0], pt[1], None, pt[2], known)
    with ThreadPoolExecutor(max_workers=jobs) as ex:
        results = list(ex.map(one, probs))
    cnt = dict.fromkeys(('PASS', 'FAIL', 'NOSTEPS', 'KNOWN-NOSTEPS', 'NOW-HAS-STEPS', 'KNOWN-WRONG'), 0)
    total = dict(STATS)
    tagged = with_steps = 0
    for (p, tag, _, _), (v, why, st, had) in zip(probs, results):
        if v == 'FAIL' and p in wrong:
            v = 'KNOWN-WRONG'
        cnt[v] += 1
        tally(total, st)
        if tag == 'steps':
            tagged += 1
            with_steps += had
        if not quiet or v in ('FAIL', 'NOW-HAS-STEPS', 'KNOWN-WRONG'):
            print('%-7s %s%s' % (v, p, '  [%s]' % why if why else ''))
    print('%d problems: %d pass, %d fail, %d nosteps, %d known-nosteps, %d now-has-steps' %
          (len(probs), cnt['PASS'] + cnt['NOW-HAS-STEPS'], cnt['FAIL'], cnt['NOSTEPS'], cnt['KNOWN-NOSTEPS'], cnt['NOW-HAS-STEPS']))
    print('steps-tagged: %d with steps / %d' % (with_steps, tagged))
    print('verified: %d identity lines (%d sides compared), %d facts, %d roots lines, %d bare polynomials, '
          '%d chains to the input, %d divisor tests, %d factor counts, %d factor lists, '
          '%d quadratic-form lines, %d SOAP lines, %d check steps' %
          (total['identity'], total['sides'], total['facts'], total['roots'], total['bare'], total['chains'], total['divides'], total['counts'], total['flists'], total['qform'], total['soap'], total['check']))
    print('known-wrong: %d' % cnt['KNOWN-WRONG'])
    return 1 if cnt['FAIL'] else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
