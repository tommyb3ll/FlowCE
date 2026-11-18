#!/usr/bin/env python3
"""Textbook factoring problems in giac syntax, built from chosen factors multiplied out, so the right answer is known
before any engine runs (independent of src/steps.cc), for testing the steps engine.
usage: python -I gen_factor.py --family NAME [--seed N] [--n COUNT] [--tags]
       python -I gen_factor.py --all --outdir DIR [--seed N] [--n COUNT]     writes DIR/corpus_factor_<family>.txt
       python -I gen_factor.py --selfcheck
Each output line:  factor(<expanded>)   # expect: <tag> ; <the factors it was built from>
tag: steps (a textbook method applies and the project plans steps for it), prime (irreducible over Q: the engine must say
it does not factor), nosteps-ok (out of scope: steps optional). The factors listed are irreducible over Q (a constant
first, powers written ^k), so the number of non-constant factors, counting powers, is what giac's factor() must print.
--tags prefixes each line with its problem kind and a tab (diagnostics only; the corpus files never use it).
Deterministic per (family, seed); no duplicates inside a family; shuffled rounds over the kinds (equal shares)."""
import argparse
import math
import os
import random
import re
import sys
from fractions import Fraction as F
from math import gcd, isqrt

DEFAULT_SEED, DEFAULT_N = 1, 50
FAMILIES = ["gcf", "grouping", "a1", "ac", "dsq", "psq", "cubes", "combined", "prime", "edge", "multi3", "stress"]

# ---------------- polynomials: dict {exponent tuple: Fraction}, variables a b x y z ----------------
VARS = "abxyz"
NV = len(VARS)
ZERO = (0,) * NV
PRINT_ORDER = "xyzab"      # main variable first: descending order of x, then y, ...

def pclean(d): return {k: v for k, v in d.items() if v}
def cst(c): return pclean({ZERO: F(c)})
def var(v, e=1):
    k = [0] * NV
    k[VARS.index(v)] = e
    return {tuple(k): F(1)}
def padd(p, q):
    d = dict(p)
    for k, v in q.items():
        d[k] = d.get(k, 0) + v
    return pclean(d)
def pneg(p): return {k: -v for k, v in p.items()}
def pscale(p, c): return pclean({k: v * c for k, v in p.items()})
def pmul(p, q):
    d = {}
    for k1, v1 in p.items():
        for k2, v2 in q.items():
            k = tuple(a + b for a, b in zip(k1, k2))
            d[k] = d.get(k, 0) + v1 * v2
    return pclean(d)
def ppow(p, n):
    out = cst(1)
    for _ in range(n):
        out = pmul(out, p)
    return out
def lin(*terms, c=0):
    """lin(('x', 2), ('y', -3), c=5) = 2x-3y+5"""
    p = cst(c)
    for v, k in terms:
        p = padd(p, pscale(var(v), k))
    return p
def vpow(v, k, c=1): return pscale(var(v, k), c)          # c*v^k
def pkey(p): return tuple(sorted(p.items()))

# ---------------- printing: calculator style (6x, x*y, x^2, c/d) ----------------
def mono_text(k): return "*".join(v if e == 1 else f"{v}^{e}" for v, e in zip(VARS, k) if e)
def term_text(k, c):
    num, den = abs(c.numerator), c.denominator
    m = mono_text(k)
    body = str(num) if not m else m if num == 1 else f"{num}{m}"
    return body + (f"/{den}" if den > 1 else "")
def terms_text(items):
    """items: [(exponents, coefficient)] in the order to print."""
    return "".join(("-" if c < 0 else "" if i == 0 else "+") + term_text(k, c) for i, (k, c) in enumerate(items)) or "0"
def sorted_items(p):
    return sorted(p.items(), key=lambda kc: tuple(kc[0][VARS.index(v)] for v in PRINT_ORDER), reverse=True)
def poly_text(p): return terms_text(sorted_items(p))
def fac_text(p):
    items = sorted_items(p)
    return poly_text(p) if len(items) == 1 and items[0][1] == 1 else f"({poly_text(p)})"
def desc_text(const, atoms):
    body = "*".join(fac_text(p) if m == 1 else f"{fac_text(p)}^{m}" for p, m in atoms)
    if const == 1: return body
    if const == -1: return "-" + body
    if const.denominator == 1: return f"{const.numerator}*{body}"
    return f"({const.numerator}/{const.denominator})*{body}"

# ---------------- parsing (own printer test): + - * / ^ ( ), integers, one-letter variables, 3x implicit ----------------
_TOK = re.compile(r"\s*(\d+|[a-z]|[-+*/^()])")
def tokenize(s):
    s, pos, out = s.strip(), 0, []
    while pos < len(s):
        m = _TOK.match(s, pos)
        if not m: raise ValueError(f"cannot parse {s!r} at {pos}")
        out.append(m.group(1))
        pos = m.end()
    return out

def parse_poly(s):
    t, pos = tokenize(s), [0]
    def peek(): return t[pos[0]] if pos[0] < len(t) else None
    def eat():
        v = peek()
        pos[0] += 1
        return v
    def atom():
        v = eat()
        if v is None: raise ValueError(f"unexpected end in {s!r}")
        if v.isdigit(): return cst(int(v)), True
        if v.isalpha() and v in VARS: return var(v), False
        if v == "(":
            e = expr()
            if eat() != ")": raise ValueError(f"missing ) in {s!r}")
            return e, False
        raise ValueError(f"unexpected {v!r} in {s!r}")
    def power():
        base, isnum = atom()
        if peek() == "^":
            eat()
            paren = peek() == "("
            if paren: eat()
            e = eat()
            if paren and eat() != ")": raise ValueError(f"bad exponent in {s!r}")
            if e is None or not e.isdigit(): raise ValueError(f"bad exponent in {s!r}")
            return ppow(base, int(e)), False
        return base, isnum
    def term():
        v, prev_num = power()
        while True:
            n = peek()
            if n == "*":
                eat()
                u, prev_num = power()
                v = pmul(v, u)
            elif n == "/":
                eat()
                d, prev_num = power()
                if len(d) != 1 or ZERO not in d: raise ValueError(f"division by a non-constant in {s!r}")
                v = pscale(v, 1 / d[ZERO])
            elif prev_num and n is not None and (n.isalpha() or n == "("):
                u, prev_num = power()
                v = pmul(v, u)
            else:
                return v
    def expr():
        sgn = 1
        if peek() in ("+", "-"): sgn = -1 if eat() == "-" else 1
        v = pscale(term(), sgn)
        while peek() in ("+", "-"):
            sg = eat()
            u = term()
            v = padd(v, u if sg == "+" else pneg(u))
        return v
    out = expr()
    if pos[0] != len(t): raise ValueError(f"trailing text in {s!r}")
    return out

def split_terms(text):
    """The typed terms of a printed polynomial (no parentheses in it) -> [(exponents, coefficient)]."""
    out = []
    for piece in re.findall(r"[+-]?[^+-]+", text):
        (k, c), = parse_poly(piece).items()
        out.append((k, c))
    return out

# ---------------- irreducibility (own code: rational root test, binary quadratic forms) ----------------
def divisors(n):
    n, out, d = abs(n), set(), 1
    while d * d <= n:
        if n % d == 0: out |= {d, n // d}
        d += 1
    return sorted(out)

def uni_coeffs(p):
    """(variable, coefficients highest degree first) if p has exactly one variable, else None."""
    vs = {i for k in p for i, e in enumerate(k) if e}
    if len(vs) != 1: return None
    i = vs.pop()
    deg = max(k[i] for k in p)
    return VARS[i], [p.get(tuple(d if j == i else 0 for j in range(NV)), F(0)) for d in range(deg, -1, -1)]

def has_rational_root(cs):
    lcm = 1
    for c in cs: lcm = lcm * c.denominator // gcd(lcm, c.denominator)
    ints = [int(c * lcm) for c in cs]
    if ints[-1] == 0: return True
    for q in divisors(ints[0]):
        for pn in divisors(ints[-1]):
            for s in (1, -1):
                x, v = F(s * pn, q), F(0)
                for c in ints: v = v * x + c
                if v == 0: return True
    return False

def is_square(n): return n >= 0 and isqrt(n) ** 2 == n

def atom_status(p):
    """'ok' (proved irreducible and primitive), 'bad', or 'unknown' (not covered by the simple tests)."""
    if not p or set(p) == {ZERO}: return "bad"
    if any(c.denominator != 1 for c in p.values()): return "bad"
    g = 0
    for c in p.values(): g = gcd(g, int(c))
    if g != 1: return "bad"
    tdeg = max(sum(k) for k in p)
    if tdeg == 1: return "ok"
    u = uni_coeffs(p)
    if u:
        cs = u[1]
        if len(cs) - 1 == 1: return "ok"
        if len(cs) - 1 in (2, 3): return "bad" if has_rational_root(cs) else "ok"
        return "unknown"
    vs = sorted({i for k in p for i, e in enumerate(k) if e})
    if len(vs) == 2 and tdeg == 2 and all(sum(k) == 2 for k in p):      # binary quadratic form
        co = lambda a, b: int(p.get(tuple({vs[0]: a, vs[1]: b}.get(j, 0) for j in range(NV)), 0))
        a, b, c = co(2, 0), co(1, 1), co(0, 2)
        return "bad" if is_square(b * b - 4 * a * c) else "ok"
    return "unknown"

# ---------------- a problem ----------------
class Prob:
    def __init__(self, const, atoms, tag, label, items=None, split=0):
        merged = []
        for p, m in atoms:        # identical atoms merge into one power
            for e in merged:
                if e[0] == p:
                    e[1] += m
                    break
            else:
                merged.append([p, m])
        self.const, self.atoms, self.tag, self.label = F(const), [(p, m) for p, m in merged], tag, label
        poly = cst(self.const)
        for p, m in self.atoms: poly = pmul(poly, ppow(p, m))
        self.poly = poly
        self.items = items or sorted_items(poly)      # the typed order of the terms
        self.shuffled = items is not None
        self.text = terms_text(self.items)
        self.desc = desc_text(self.const, self.atoms)
        self.count = sum(m for p, m in self.atoms if set(p) != {ZERO})
        self.split = split      # giac splits a quadratic with real irrational roots over surds: this many factors in its answer
    def line(self):
        note = f" ; irreducible over Q, but giac splits it over surds into {self.split} factors" if self.split else ""
        return f"factor({self.text})   # expect: {self.tag} ; {self.desc}{note}"

def nz(r, lo, hi): return r.choice([v for v in range(lo, hi + 1) if v])
def coprime(*ns):
    g = 0
    for n in ns: g = gcd(g, n)
    return g == 1

# ---------------- cores: (constant, irreducible atoms) ----------------
VPAIRS = [("x", "y"), ("x", "y"), ("a", "b")]

def core_a1(r, two, kind):
    """(u+m*v)(u+n*v) or (x+m)(x+n); kind: pp, nn, mp (bigger one positive), mn (bigger one negative)."""
    while True:
        m, n = r.randint(1, 12), r.randint(1, 12)
        if m == n: continue
        m, n = {"pp": (m, n), "nn": (-m, -n), "mp": (max(m, n), -min(m, n)), "mn": (-max(m, n), min(m, n))}[kind]
        if m + n != 0: break
    if two:
        u, v = r.choice(VPAIRS)
        return F(1), [(lin((u, 1), (v, m)), 1), (lin((u, 1), (v, n)), 1)]
    return F(1), [(lin(("x", 1), c=m), 1), (lin(("x", 1), c=n), 1)]

def core_ac(r, two, sign):
    while True:
        p, q = r.randint(1, 5), r.randint(1, 5)
        m, n = nz(r, -9, 9), nz(r, -9, 9)
        if p * q > 1 and coprime(p, m) and coprime(q, n) and (p, m) != (q, n) and p * n + q * m != 0: break
    if two:
        u, v = r.choice(VPAIRS)
        return F(sign), [(lin((u, p), (v, m)), 1), (lin((u, q), (v, n)), 1)]
    return F(sign), [(lin(("x", p), c=m), 1), (lin(("x", q), c=n), 1)]

def core_dsq(r, kind):
    u, v = r.choice(VPAIRS)
    if kind in ("d1", "d1neg"):            # a^2x^2-b^2 (or b^2-a^2x^2)
        while True:
            a, b = r.randint(1, 8), r.randint(1, 12)
            if coprime(a, b): break
        return F(-1 if kind == "d1neg" else 1), [(lin(("x", a), c=-b), 1), (lin(("x", a), c=b), 1)]
    if kind == "d2":                       # a^2x^2-b^2y^2
        while True:
            a, b = r.randint(1, 7), r.randint(1, 7)
            if coprime(a, b): break
        return F(1), [(lin((u, a), (v, -b)), 1), (lin((u, a), (v, b)), 1)]
    if kind == "d4":                       # a^4x^4-b^4, two rounds
        while True:
            a, b = r.choice((1, 1, 1, 2, 3)), r.randint(1, 6)
            if coprime(a, b): break
        return F(1), [(lin(("x", a), c=-b), 1), (lin(("x", a), c=b), 1), (padd(vpow("x", 2, a * a), cst(b * b)), 1)]
    while True:                            # d4y: a^4x^4-b^4y^4
        a, b = r.choice((1, 1, 2, 3)), r.choice((1, 1, 2, 3))
        if coprime(a, b): break
    return F(1), [(lin((u, a), (v, -b)), 1), (lin((u, a), (v, b)), 1), (padd(vpow(u, 2, a * a), vpow(v, 2, b * b)), 1)]

def core_psq(r, two):
    while True:
        a, b = r.randint(1, 6), r.randint(1, 9)
        if coprime(a, b): break
    s = r.choice((1, -1))
    if two:
        u, v = r.choice(VPAIRS)
        return F(1), [(lin((u, a), (v, s * b)), 2)]
    return F(1), [(lin(("x", a), c=s * b), 2)]

def core_cubes(r, kind, sign):
    """a^3x^3 + sign*b^3 -> (ax+sign*b)(a^2x^2-sign*abx+b^2); kind c1 (one variable), c2 (two variables), c6 (x^6-b^6)."""
    if kind == "c6":
        b = r.randint(1, 5)
        return F(1), [(lin(("x", 1), c=-b), 1), (lin(("x", 1), c=b), 1),
                      (padd(padd(vpow("x", 2), vpow("x", 1, b)), cst(b * b)), 1),
                      (padd(padd(vpow("x", 2), vpow("x", 1, -b)), cst(b * b)), 1)]
    while True:
        a, b = r.randint(1, 4), r.randint(1, 6)
        if coprime(a, b): break
    u, v = ("x", None) if kind == "c1" else r.choice(VPAIRS)
    A, B = var(u, 1), pscale(var(v) if v else cst(1), b)
    A = pscale(A, a)
    quad = padd(padd(pmul(A, A), pscale(pmul(A, B), -sign)), pmul(B, B))
    return F(1), [(padd(A, pscale(B, sign)), 1), (quad, 1)]

# ---------------- gcf ----------------
def rest_one(r, v="x"):
    """A primitive polynomial in v with nonzero constant term, irreducible over Q (linear, p*v^2+q, or p*v^2+b*v+c, b^2<4pc)."""
    while True:
        t = r.choice(("lin", "lin", "bin", "quad"))
        if t == "lin":
            p, q = r.choice((1, 1, 1, 2, 3, 4, 5)), nz(r, -9, 9)
            if coprime(p, q): return lin((v, p), c=q)
        elif t == "bin":
            p, q = r.randint(1, 4), r.randint(1, 9)
            if coprime(p, q): return padd(vpow(v, 2, p), cst(q))
        else:
            p, b, c = r.choice((1, 1, 2, 3)), nz(r, -8, 8), r.randint(1, 9)
            if b * b < 4 * p * c and coprime(p, b, c): return padd(padd(vpow(v, 2, p), vpow(v, 1, b)), cst(c))

def gcf_num(r):
    return Prob(r.choice((2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 15)), [(rest_one(r), 1)], "steps", "num")
def gcf_num2(r):
    u, v = r.choice(VPAIRS)
    while True:
        p, q = r.choice((1, 1, 2, 3)), nz(r, -6, 6)
        if coprime(p, q): break
    return Prob(r.choice((2, 3, 4, 5, 6, 8, 10)), [(lin((u, p), (v, q)), 1)], "steps", "num2")
def gcf_var(r):
    return Prob(1, [(var("x"), r.randint(1, 4)), (rest_one(r), 1)], "steps", "var")
def gcf_both(r):
    return Prob(r.choice((2, 3, 4, 5, 6, 8, 9, 10, 12)), [(var("x"), r.randint(1, 3)), (rest_one(r), 1)], "steps", "both")
def gcf_neg(r):
    g, k = r.choice((1, 2, 3, 4, 5, 6, 8)), r.randint(0, 3)
    if g == 1 and k == 0: k = 1
    return Prob(-g, ([(var("x"), k)] if k else []) + [(rest_one(r), 1)], "steps", "neg")
def gcf_two(r, terms):
    u, v = r.choice((("x", "y"), ("x", "y"), ("a", "b")))
    while True:
        p, q, s = r.choice((1, 2, 3, 4, 5)), nz(r, -6, 6), nz(r, -9, 9)
        if coprime(p, q, s if terms == 3 else 1) and coprime(p, q): break
    rest = lin((u, p), (v, q), c=s if terms == 3 else 0)
    return Prob(r.choice((1, 1, 2, 3, 4, 5, 6, 8, 12)), [(var(u), r.randint(1, 3)), (var(v), r.randint(1, 3)), (rest, 1)],
                "steps", f"two{terms}")
def gcf_three(r):
    while True:
        ps = [r.choice((1, 2, 3, 4)), nz(r, -5, 5), nz(r, -5, 5)]
        n = r.choice((2, 3))
        if coprime(*ps[:n]): break
    rest = lin(*[(v, c) for v, c in zip("xyz", ps[:n])])
    return Prob(r.choice((1, 2, 3, 4, 6, 12)), [(var(v), r.randint(1, 2)) for v in "xyz"] + [(rest, 1)], "steps", "three")

# ---------------- grouping ----------------
def pair_gcd(t1, t2):
    return gcd(int(abs(t1[1])), int(abs(t2[1]))), tuple(min(a, b) for a, b in zip(t1[0], t2[0]))
def pair_rest(t1, t2):
    n, m = pair_gcd(t1, t2)
    return padd(pclean({tuple(a - b for a, b in zip(t1[0], m)): t1[1] / n}), pclean({tuple(a - b for a, b in zip(t2[0], m)): t2[1] / n}))
def pairing_works(t1, t2, t3, t4):
    """Grouping (t1,t2)(t3,t4): after taking out each pair's greatest common factor, the two remainders agree up to sign."""
    r1, r2 = pair_rest(t1, t2), pair_rest(t3, t4)
    return len(r1) > 1 and (r1 == r2 or r1 == pneg(r2))
def any_pairing(t):
    return any(pairing_works(t[i], t[j], t[k], t[l]) for i, j, k, l in ((0, 1, 2, 3), (0, 2, 1, 3), (0, 3, 1, 2)))

def pick_a(r, k):
    """a for x^k + a that giac keeps whole: k = 2 needs a > 0 (a negative a splits over surds), k = 3 needs a non-cube."""
    while True:
        a = nz(r, -9, 9)
        if k == 2 and a > 0 or k == 3 and round(abs(a) ** (1 / 3)) ** 3 != abs(a): return a

def grp_cubic(r, neg=False):
    """(p x^2 + a)(x + b), a > 0; neg: -(...), so that the second group has a negative common factor."""
    while True:
        p, a = r.choice((1, 1, 1, 2, 3)), r.randint(1, 9)
        if coprime(p, a): break
    return Prob(-1 if neg else 1, [(padd(vpow("x", 2, p), cst(a)), 1), (lin(("x", 1), c=nz(r, -9, 9)), 1)], "steps", "cubic-neg" if neg else "cubic")
def grp_deg(r, neg=False):
    """(x^3 + a)(x^2 + b), (x^3 + a)(x + b); a may be negative (second group negative), b > 0 for the quadratic."""
    k, j = r.choice(((3, 2), (3, 1)))
    a = pick_a(r, 3)
    while neg and a > 0: a = pick_a(r, 3)
    return Prob(1, [(padd(vpow("x", k), cst(a)), 1), (padd(vpow("x", j), cst(pick_a(r, 2) if j > 1 else nz(r, -9, 9))), 1)],
                "steps", f"deg{k}{j}-neg" if neg else f"deg{k}{j}")
def grp_two1(r, neg=False):
    u, v = r.choice(VPAIRS)
    a = nz(r, -9, -1) if neg else nz(r, -9, 9)
    return Prob(1, [(lin((u, 1), c=a), 1), (lin((v, 1), c=nz(r, -9, 9)), 1)], "steps", "xy-neg" if neg else "xy")
def grp_two2(r):
    (u1, v1), (u2, v2) = r.choice((("a", "b"), ("x", "y"))), r.choice((("x", "y"), ("a", "b")))
    if u1 == u2: (u2, v2) = ("x", "y") if u1 == "a" else ("a", "b")
    while True:
        p, q, s = r.choice((1, 1, 1, 2, 3)), r.choice((1, -1, 1, -1, 2, -2, 3)), r.choice((1, 1, -1))
        if coprime(p, q): break
    return Prob(1, [(lin((u1, p), (v1, q)), 1), (lin((u2, 1), (v2, s)), 1)], "steps", "ab-xy")
def grp_gcd_dsq(r):      # g*(x^2-c^2)(x+b): gcf, grouping, difference of squares
    c, b = r.randint(2, 6), nz(r, -9, 9)
    return Prob(r.choice((2, 3, 4, 5, -2, -3)), [(lin(("x", 1), c=-c), 1), (lin(("x", 1), c=c), 1), (lin(("x", 1), c=b), 1)], "steps", "gcd")

def shuffle_group(r, pr):
    """Typed order with the pairing (1,2)(3,4) failing (the terms are not sorted)."""
    items = list(pr.items)
    for _ in range(500):
        r.shuffle(items)
        if not pairing_works(*items[:4]) and any_pairing(items):
            out = Prob(pr.const, pr.atoms, pr.tag, pr.label, items)
            return out
    return pr

# ---------------- combined ----------------
def gcf_part(r, two):
    g = r.choice((1, 2, 2, 3, 3, 4, 5, 6, 7, 8, 9, 10, 12))
    neg = r.random() < .3
    if two:
        i, j = r.choice((0, 1, 1, 2)), r.choice((0, 1, 1, 2))
    else:
        i, j = r.choice((0, 1, 1, 2, 3)), 0
    if g == 1 and not i and not j: g = r.choice((2, 3, 4, 6))
    return F(-g if neg else g), i, j

def cmb_core(r, which):
    """A core by name; a trailing y means two variables."""
    return {"a1": lambda: core_a1(r, False, r.choice(("pp", "nn", "mp", "mn"))),
            "a1y": lambda: core_a1(r, True, r.choice(("pp", "nn", "mp", "mn"))),
            "ac": lambda: core_ac(r, False, 1), "acy": lambda: core_ac(r, True, 1),
            "dsq": lambda: core_dsq(r, r.choice(("d1", "d1", "d4"))), "dsqy": lambda: core_dsq(r, r.choice(("d2", "d2", "d4y"))),
            "psq": lambda: core_psq(r, False), "psqy": lambda: core_psq(r, True),
            "cubes": lambda: core_cubes(r, "c1", r.choice((1, -1))), "cubesy": lambda: core_cubes(r, "c2", r.choice((1, -1)))}[which]()

def cmb_gcf(which):
    def f(r):
        c0, atoms = cmb_core(r, which)
        names = sorted({n for p, _ in atoms for k in p for n, e in zip(VARS, k) if e})
        u, v = (names + ["y"])[:2] if names != ["x"] else ("x", "y")
        g, i, j = gcf_part(r, len(names) == 2)
        return Prob(g * c0, [(var(n), e) for n, e in zip((u, v), (i, j)) if e] + atoms, "steps", "gcf+" + which)
    return f

def cmb_group(r):
    base = grp_cubic(r)
    g, i, _ = gcf_part(r, False)
    return Prob(g, ([(var("x"), i)] if i else []) + base.atoms, "steps", "gcf+grouping")
def cmb_group_dsq(r):
    pr = grp_gcd_dsq(r)
    return Prob(pr.const, pr.atoms, "steps", "gcf+grouping+dsq")
def cmb_sq2(r):          # (x^2-b^2)^2, optional GCF
    b = r.randint(1, 4)
    g = r.choice((1, 1, 1, 2, 3, -1))
    return Prob(g, [(lin(("x", 1), c=-b), 2), (lin(("x", 1), c=b), 2)], "steps", "(x2-b2)^2")
def cmb_four_sym(r):     # (x^2-a^2)(x^2-b^2)
    a, b = r.sample(range(1, 8), 2)
    return Prob(r.choice((1, 1, 1, 2, -1)), [(lin(("x", 1), c=-a), 1), (lin(("x", 1), c=a), 1),
                                              (lin(("x", 1), c=-b), 1), (lin(("x", 1), c=b), 1)], "steps", "four-sym")
def cmb_rep(r):          # g x (a x + b)^2
    p, q = r.choice((1, 1, 2, 3)), nz(r, -7, 7)
    while not coprime(p, q): q = nz(r, -7, 7)
    return Prob(r.choice((1, 2, 3, 4, 5, -1, -2, -3)), [(var("x"), r.choice((1, 1, 2))), (lin(("x", p), c=q), 2)], "steps", "g*x*(px+q)^2")
def cmb_cube_rep(r):     # (x+m)^3
    return Prob(r.choice((1, 1, 1, 2, -1)), [(lin(("x", 1), c=nz(r, -6, 6)), 3)], "nosteps-ok", "(x+m)^3")
def cmb_four_gen(r):     # four distinct linear factors
    rs = r.sample([v for v in range(-6, 7) if v], 4)
    return Prob(1, [(lin(("x", 1), c=-s), 1) for s in rs], "nosteps-ok", "four-linear")
def cmb_three_gen(r):    # three distinct linear factors, not a grouping cubic
    rs = r.sample([v for v in range(-6, 7) if v], 3)
    return Prob(r.choice((1, 1, 2, -1)), [(lin(("x", 1), c=-s), 1) for s in rs], "nosteps-ok", "three-linear")

# ---------------- prime ----------------
def pr_x2k(r): return Prob(1, [(padd(vpow("x", 2), cst(r.randint(1, 40))), 1)], "prime", "x2+k")
def pr_ax2k(r):
    while True:
        p, q = r.randint(2, 6), r.randint(1, 25)
        if coprime(p, q): return Prob(1, [(padd(vpow("x", 2, p), cst(q)), 1)], "prime", "ax2+k")
def pr_quad_neg(r):
    while True:
        p, b, c = r.choice((1, 1, 1, 2, 3, 4)), nz(r, -9, 9), r.randint(1, 25)
        if b * b < 4 * p * c and coprime(p, b, c):
            return Prob(1, [(padd(padd(vpow("x", 2, p), vpow("x", 1, b)), cst(c)), 1)], "prime", "ax2+bx+c")
def pr_quad_pos(r):
    while True:
        b, c = r.randint(-6, 6), nz(r, -9, 9)
        if b * b - 4 * c > 0 and not is_square(b * b - 4 * c):
            return Prob(1, [(padd(padd(vpow("x", 2), vpow("x", 1, b)), cst(c)), 1)], "nosteps-ok", "irrational-roots", split=2)
def pr_sumsq(r):
    u, v = r.choice(VPAIRS)
    while True:
        p, q = r.choice((1, 1, 1, 2, 3, 4, 5)), r.choice((1, 1, 2, 3, 4, 5, 9))
        if coprime(p, q): return Prob(1, [(padd(vpow(u, 2, p), vpow(v, 2, q)), 1)], "prime", "px2+qy2")
def pr_xy(r):
    u, v = r.choice(VPAIRS)
    while True:
        p, b, c = r.choice((1, 1, 1, 2, 3)), nz(r, -5, 5), r.randint(1, 9)
        if b * b < 4 * p * c and coprime(p, b, c):
            q = padd(padd(vpow(u, 2, p), pscale(pmul(var(u), var(v)), b)), vpow(v, 2, c))
            return Prob(1, [(q, 1)], "prime", "x2+bxy+cy2")
def pr_cubic(r):
    while True:
        p, b, c, d = r.choice((1, 1, 1, 2, 3)), r.randint(-9, 9), r.randint(-9, 9), nz(r, -9, 9)
        q = padd(padd(padd(vpow("x", 3, p), vpow("x", 2, b)), vpow("x", 1, c)), cst(d))
        if coprime(p, b, c, d) and not has_rational_root(uni_coeffs(q)[1]): return Prob(1, [(q, 1)], "prime", "cubic")
def pr_lin(r):
    while True:
        p, q = r.randint(2, 7), nz(r, -9, 9)
        if coprime(p, q): return Prob(1, [(lin(("x", p), c=q), 1)], "prime", "px+q")
def pr_x3k(r): return Prob(1, [(padd(vpow("x", 3), cst(pick_a(r, 3))), 1)], "prime", "x3+k")
def pr_monic(r): return Prob(1, [(lin(("x", 1), c=nz(r, -9, 9)), 1)], "nosteps-ok", "x+k")

# ---------------- edge ----------------
def e_zero_cubes(r):
    b, s = r.randint(1, 9), r.choice((1, -1))
    return Prob(1, [(lin(("x", 1), c=s * b), 1), (padd(padd(vpow("x", 2), vpow("x", 1, -s * b)), cst(b * b)), 1)], "steps", "x3+-b3")
def e_zero_sq(r):
    return Prob(1, [(padd(vpow("x", 2), cst(r.randint(1, 7))), 2)], "steps", "(x2+c)^2")
def e_zero_gcf(r):
    k = r.randint(1, 3)
    return Prob(1, [(var("x"), k), (padd(vpow("x", 2), cst(r.randint(1, 9))), 1)], "steps", "x^k(x2+c)")
def e_zero_dsq(r):
    k = r.choice((3, 5))
    b = r.randint(1, 6)
    return Prob(1, [(var("x"), 1), (lin(("x", 1), c=-b), 1), (lin(("x", 1), c=b), 1)] +
                ([(padd(vpow("x", 2), cst(b * b)), 1)] if k == 5 else []), "steps", f"x^{k}-b2x")
def e_frac_a(r):
    b = r.randint(2, 8)
    return Prob(F(1, b * b), [(lin(("x", 1), c=-b), 1), (lin(("x", 1), c=b), 1)], "nosteps-ok", "x2/b2-1")
def e_frac_b(r):
    a = r.randint(2, 8)
    return Prob(F(1, a * a), [(lin(("x", a), c=-1), 1), (lin(("x", a), c=1), 1)], "nosteps-ok", "x2-1/a2")
def e_frac_c(r):
    d, m, n = r.choice((2, 3, 4, 5, 6)), nz(r, -6, 6), nz(r, -6, 6)
    if m == n or m + n == 0: return None
    return Prob(F(1, d), [(lin(("x", 1), c=m), 1), (lin(("x", 1), c=n), 1)], "nosteps-ok", "(x+m)(x+n)/d")
def e_frac_d(r):
    while True:
        p, q, d = r.randint(1, 5), nz(r, -9, 9), r.choice((2, 3, 4, 5, 6))
        if coprime(p, q) and gcd(p, d) == 1: return Prob(F(1, d), [(lin(("x", p), c=q), 1)], "nosteps-ok", "(px+q)/d")
def e_large_a1(r):
    while True:
        m, n = nz(r, -950, 950), nz(r, -950, 950)
        if m != n and m + n != 0 and abs(m * n) <= 10 ** 6 and abs(m * n) > 50000:
            return Prob(1, [(lin(("x", 1), c=m), 1), (lin(("x", 1), c=n), 1)], "steps", "large-a1")
def e_large_ac(r):
    while True:
        p, q, m, n = r.randint(1, 30), r.randint(2, 30), nz(r, -300, 300), nz(r, -300, 300)
        if coprime(p, m) and coprime(q, n) and (p, m) != (q, n) and p * n + q * m != 0 and 50000 < abs(p * q * m * n) <= 10 ** 6:
            return Prob(r.choice((1, -1)), [(lin(("x", p), c=m), 1), (lin(("x", q), c=n), 1)], "steps", "large-ac")
def e_large_dsq(r):
    while True:
        a, b = r.randint(1, 300), r.randint(100, 1000)
        if coprime(a, b) and a * a <= 10 ** 6: return Prob(1, [(lin(("x", a), c=-b), 1), (lin(("x", a), c=b), 1)], "steps", "large-dsq")
def e_large_gcf(r):
    p, q = r.randint(1, 9), nz(r, -9, 9)
    if not coprime(p, q): return None
    return Prob(r.choice((100, 125, 200, 250, 500, 1000, 1200)), [(lin(("x", p), c=q), 1)], "steps", "large-gcf")
def e_huge_a1(r):
    while True:
        m, n = r.randint(2500, 9000), -r.randint(2500, 9000)
        if m + n != 0 and abs(m * n) > 8 * 10 ** 6:
            return Prob(1, [(lin(("x", 1), c=m), 1), (lin(("x", 1), c=n), 1)], "nosteps-ok", "huge-a1")
def e_huge_dsq(r):
    b = r.randint(2900, 9000)
    return Prob(1, [(lin(("x", 1), c=-b), 1), (lin(("x", 1), c=b), 1)], "nosteps-ok", "huge-dsq")
def e_deg1_gcf(r):
    u, v = r.choice(VPAIRS)
    while True:
        p, q = r.randint(1, 6), nz(r, -9, 9)
        if coprime(p, q): break
    two = r.random() < .4
    return Prob(r.choice((2, 3, 4, 5, 6, 7, 9, -2, -3)), [(lin((u, p), (v, q)) if two else lin((u, p), c=q), 1)], "steps", "deg1-gcf")
def e_deg1_monic(r):
    q = nz(r, -9, 9)
    return Prob(r.choice((1, 1, -1)), [(lin(("x", 1), c=q), 1)], "nosteps-ok", "deg1-monic")
def e_deg1_prime(r):
    while True:
        p, q = r.randint(2, 7), nz(r, -9, 9)
        if coprime(p, q): return Prob(1, [(lin(("x", p), c=q), 1)], "prime", "deg1-px+q")
def e_ctp(r):
    t = r.choice(("x2k", "quad", "cubic"))
    atom = (pr_x2k(r) if t == "x2k" else pr_quad_neg(r) if t == "quad" else pr_cubic(r)).atoms[0][0]
    return Prob(r.choice((2, 3, 4, 5, 6, 7, 9, -2, -3, -5)), [(atom, 1)], "steps", "const*prime")


# ---------------- multi3: three variables (the plan promises steps for the GCF only) ----------------
def m3_rest(r, nvars):
    while True:
        cs = [r.choice((1, 1, 2, 3)), nz(r, -6, 6), nz(r, -6, 6)][:nvars]
        if coprime(*cs): break
    return lin(*[(v, c) for v, c in zip("xyz", cs)])
def m3_gcf(r, neg=False, partial=False):
    """g * monomial in x, y, z * a linear sum; partial: the monomial lacks z, which the sum holds"""
    if partial:
        p, q = r.choice((1, 2, 3)), nz(r, -9, 9)
        if not coprime(p, q): return None
        rest, names = lin(("z", p), c=q), "xy"
    else:
        rest, names = m3_rest(r, r.choice((2, 3))), "xyz"
    mono = [(var(v), r.randint(1, 3)) for v in names if partial or r.random() < .8]
    g = r.choice((1, 2, 3, 4, 5, 6, 8, 12))
    if not mono and g == 1: return None
    return Prob(-g if neg else g, mono + [(rest, 1)], "steps", "m3-neg" if neg else "m3-part" if partial else "m3-gcf")
def m3_num(r):
    return Prob(r.choice((2, 3, 4, 5, 6, 7, 9, 10)), [(m3_rest(r, 3), 1)], "steps", "m3-num")
def m3_grp(r, kind):
    c1 = nz(r, -6, 6)
    if kind == 0:
        a, b = lin(("x", 1), ("y", r.choice((1, 2, -1, 3)))), lin(("z", 1), c=c1)
    elif kind == 1:
        a, b = lin(("x", 1), ("z", r.choice((1, -1, 2)))), lin(("y", 1), c=c1)
    else:
        a, b = padd(pmul(var("x"), var("y")), pscale(var("z"), r.choice((1, -1, 2, 3)))), lin(("x", 1), c=c1)
    return Prob(1, [(a, 1), (b, 1)], "nosteps-ok", f"grp3-{kind}")
def m3_sq(r):
    return Prob(1, [(lin(("x", 1), ("y", nz(r, -3, 3)), ("z", nz(r, -3, 3))), 2)], "nosteps-ok", "(x+by+cz)^2")
def m3_dsq(r):
    b, c = r.randint(1, 4), r.randint(1, 4)
    return Prob(1, [(lin(("x", 1), ("y", b), ("z", c)), 1), (lin(("x", 1), ("y", -b), ("z", -c)), 1)], "nosteps-ok", "x2-(by+cz)2")
def m3_prime(r):
    a, b, c = r.choice((1, 2, 3)), r.choice((1, 2, 3, 5)), r.choice((1, 2, 3, 7))
    return Prob(1, [(padd(padd(vpow("x", 2, a), vpow("y", 2, b)), vpow("z", 2, c)), 1)], "nosteps-ok", "ax2+by2+cz2")
def m3_lin(r):
    return Prob(1, [(m3_rest(r, 3), 1)], "nosteps-ok", "linear-3")
def m3_prod(r):
    return Prob(1, [(m3_rest(r, 3), 1), (lin(("x", 1), ("y", nz(r, -4, 4)), ("z", nz(r, -4, 4))), 1)], "nosteps-ok", "(L1)(L2)")

# ---------------- stress: large coefficients, high degree, repeated factors, zero coefficients ----------------
def st_big_gcf(r):
    p, q = r.randint(1, 12), nz(r, -30, 30)
    if not coprime(p, q): return None
    k, g = r.randint(0, 4), r.choice((1000, 4096, 15625, 30030, 99999, 250000, 720720, 999999, 1000000))
    if g * max(abs(p), abs(q)) > 10 ** 6: return None
    return Prob(r.choice((1, -1)) * g, ([(var("x"), k)] if k else []) + [(lin(("x", p), c=q), 1)], "steps", "big-gcf")
def st_big_a1(r):
    while True:
        m, n = nz(r, -1000, 1000), nz(r, -1000, 1000)
        if m != n and m + n != 0 and 10 ** 4 < abs(m * n) <= 10 ** 6:
            return Prob(1, [(lin(("x", 1), c=m), 1), (lin(("x", 1), c=n), 1)], "steps", "big-a1")
def st_big_dsq(r):
    while True:
        a, b = r.randint(1, 1000), r.randint(1, 1000)
        if coprime(a, b): return Prob(1, [(lin(("x", a), c=-b), 1), (lin(("x", a), c=b), 1)], "steps", "big-dsq")
def st_big_psq(r):
    while True:
        a, b = r.randint(2, 1000), nz(r, -1000, 1000)
        if coprime(a, b) and 2 * a * abs(b) <= 10 ** 6 and a * a <= 10 ** 6: return Prob(1, [(lin(("x", a), c=b), 2)], "steps", "big-psq")
def st_big_cubes(r):
    a, b, s = r.randint(1, 10), r.randint(1, 100), r.choice((1, -1))
    if not coprime(a, b): return None
    return Prob(1, [(lin(("x", a), c=s * b), 1), (padd(padd(vpow("x", 2, a * a), vpow("x", 1, -s * a * b)), cst(b * b)), 1)], "steps", "big-cubes")
def st_rep(r):
    k, p, q = r.choice((2, 2, 3, 4)), r.choice((1, 1, 2, 3)), nz(r, -12, 12)
    if not coprime(p, q): return None
    g = r.choice((1, 1, 2, -3, 5))
    j = r.choice((0, 0, 1, 2))
    tag = "steps" if k == 2 or (k == 3 and p == 1) else "nosteps-ok"
    return Prob(g, ([(var("x"), j)] if j else []) + [(lin(("x", p), c=q), k)], tag, f"rep{k}")
def st_deg(r):
    n = r.randint(5, 8)
    return Prob(1, [(lin(("x", 1), c=-m), 1) for m in r.sample([v for v in range(-6, 7) if v], n)], "nosteps-ok", f"deg{n}-linear")
def st_sparse(r):
    m, a = r.choice((2, 3, 4)), None
    while a is None:
        a = r.randint(1, 40)
        if m == 4 and a == 4 or m == 3 and round(a ** (1 / 3)) ** 3 == a: a = None
    return Prob(r.choice((1, 1, 2, -5)), [(var("x"), r.randint(1, 4)), (padd(vpow("x", m), cst(a)), 1)], "steps", f"x^k(x^{m}+a)")
def st_binom(r):
    n = r.choice((4, 4, 6, 8))
    b = r.randint(1, {4: 9, 6: 9, 8: 5}[n])
    a = padd(vpow("x", 2), cst(b * b))
    atoms = {4: [(lin(("x", 1), c=-b), 1), (lin(("x", 1), c=b), 1), (a, 1)],
             6: [(lin(("x", 1), c=-b), 1), (lin(("x", 1), c=b), 1), (padd(padd(vpow("x", 2), vpow("x", 1, b)), cst(b * b)), 1),
                 (padd(padd(vpow("x", 2), vpow("x", 1, -b)), cst(b * b)), 1)],
             8: [(lin(("x", 1), c=-b), 1), (lin(("x", 1), c=b), 1), (a, 1), (padd(vpow("x", 4), cst(b ** 4)), 1)]}[n]
    return Prob(1, atoms, "steps" if n == 4 else "nosteps-ok", f"x^{n}-b^{n}")
def st_zero_mid(r):
    while True:
        a, b = pick_a(r, 3), pick_a(r, 3)
        if a != b: return Prob(1, [(padd(vpow("x", 3), cst(a)), 1), (padd(vpow("x", 3), cst(b)), 1)], "nosteps-ok", "(x3+a)(x3+b)")

# ---------------- families: (kind label, generator, cap) ----------------
INF = 10 ** 9
def K(label, fn, cap=INF): return (label, fn, cap)

def mk(core, tag="steps", label=""):
    return Prob(core[0], core[1], tag, label)

def kinds_for(name):
    if name == "gcf":
        return [K("num", gcf_num), K("num2", gcf_num2), K("var", gcf_var), K("both", gcf_both), K("neg", gcf_neg),
                K("two2", lambda r: gcf_two(r, 2)), K("two3", lambda r: gcf_two(r, 3)), K("three", gcf_three)]
    if name == "grouping":
        return [K("cubic", grp_cubic), K("cubic-neg", lambda r: grp_cubic(r, True)), K("deg", grp_deg), K("deg-neg", lambda r: grp_deg(r, True)), K("xy", grp_two1),
                K("xy-neg", lambda r: grp_two1(r, True)), K("ab-xy", grp_two2)]
    if name == "a1":
        return [K(f"a1-{k}", lambda r, k=k: mk(core_a1(r, False, k), label="a1-" + k)) for k in ("pp", "nn", "mp", "mn")] + \
               [K(f"a1y-{k}", lambda r, k=k: mk(core_a1(r, True, k), label="a1y-" + k)) for k in ("pp", "nn", "mp", "mn")]
    if name == "ac":
        return [K(f"ac{'y' if two else ''}{'-' if s < 0 else '+'}", lambda r, two=two, s=s: mk(core_ac(r, two, s), label="ac"))
                for two in (False, True) for s in (1, -1)]
    if name == "dsq":
        return [K(k, lambda r, k=k: mk(core_dsq(r, k), label=k)) for k in ("d1", "d1neg", "d2", "d4", "d4y")]
    if name == "psq":
        return [K("psq", lambda r: mk(core_psq(r, False), label="psq")), K("psqy", lambda r: mk(core_psq(r, True), label="psqy"))]
    if name == "cubes":
        return [K("c1+", lambda r: mk(core_cubes(r, "c1", 1), label="c1+")), K("c1-", lambda r: mk(core_cubes(r, "c1", -1), label="c1-")),
                K("c2+", lambda r: mk(core_cubes(r, "c2", 1), label="c2+")), K("c2-", lambda r: mk(core_cubes(r, "c2", -1), label="c2-")),
                K("c6", lambda r: mk(core_cubes(r, "c6", 1), label="c6"))]
    if name == "combined":
        return [K("gcf+" + w, cmb_gcf(w)) for w in ("a1", "a1y", "ac", "acy", "dsq", "dsqy", "psq", "psqy", "cubes", "cubesy")] + \
               [K("gcf+grouping", cmb_group), K("gcf+grouping+dsq", cmb_group_dsq), K("(x2-b2)^2", cmb_sq2), K("four-sym", cmb_four_sym),
                K("g*x*(px+q)^2", cmb_rep), K("(x+m)^3", cmb_cube_rep, 4), K("four-linear", cmb_four_gen, 3), K("three-linear", cmb_three_gen, 4)]
    if name == "prime":
        return [K("x2+k", pr_x2k), K("ax2+k", pr_ax2k), K("ax2+bx+c", pr_quad_neg), K("irrational-roots", pr_quad_pos, 4),
                K("px2+qy2", pr_sumsq), K("x2+bxy+cy2", pr_xy), K("cubic", pr_cubic), K("px+q", pr_lin), K("x3+k", pr_x3k),
                K("x+k", pr_monic, 3)]
    if name == "edge":
        return [K("x3+-b3", e_zero_cubes), K("(x2+c)^2", e_zero_sq), K("x^k(x2+c)", e_zero_gcf),
                K("x^k-b2x", e_zero_dsq), K("x2/b2-1", e_frac_a), K("x2-1/a2", e_frac_b), K("(x+m)(x+n)/d", e_frac_c),
                K("(px+q)/d", e_frac_d, 3), K("large-a1", e_large_a1), K("large-ac", e_large_ac), K("large-dsq", e_large_dsq),
                K("large-gcf", e_large_gcf), K("huge-a1", e_huge_a1, 3), K("huge-dsq", e_huge_dsq, 2), K("deg1-gcf", e_deg1_gcf),
                K("deg1-monic", e_deg1_monic, 3), K("deg1-px+q", e_deg1_prime, 3), K("const*prime", e_ctp)]
    if name == "multi3":
        return [K("m3-gcf", m3_gcf), K("m3-neg", lambda r: m3_gcf(r, True)), K("m3-part", lambda r: m3_gcf(r, False, True)), K("m3-num", m3_num),
                K("grp3-0", lambda r: m3_grp(r, 0)), K("grp3-1", lambda r: m3_grp(r, 1)), K("grp3-2", lambda r: m3_grp(r, 2)),
                K("(x+by+cz)^2", m3_sq), K("x2-(by+cz)2", m3_dsq, 12), K("ax2+by2+cz2", m3_prime, 6), K("linear-3", m3_lin), K("(L1)(L2)", m3_prod)]
    if name == "stress":
        return [K("big-gcf", st_big_gcf), K("big-a1", st_big_a1), K("big-dsq", st_big_dsq), K("big-psq", st_big_psq), K("big-cubes", st_big_cubes),
                K("rep", st_rep), K("deg-linear", st_deg), K("sparse", st_sparse), K("binom", st_binom, 12), K("zero-mid", st_zero_mid, 12)]
    raise SystemExit(f"unknown family {name!r}; choose from {', '.join(FAMILIES)}")

def generate(name, seed=DEFAULT_SEED, n=DEFAULT_N):
    r = random.Random(f"{seed}/{name}")
    kinds, seen, out, used, dead, order = kinds_for(name), set(), [], {}, set(), []
    while len(out) < n:
        if not order:
            order = [k for k in r.sample(kinds, len(kinds)) if k[0] not in dead]
            if not order: break
        label, fn, cap = order.pop()
        if used.get(label, 0) >= cap:
            dead.add(label)
            continue
        for _ in range(80):
            pr = fn(r)
            if pr is not None and pkey(pr.poly) not in seen: break
            pr = None
        if pr is None:
            dead.add(label)
            continue
        seen.add(pkey(pr.poly))
        if (name == "grouping" or pr.label.startswith("grp3")) and len(out) % 3 == 2: pr = shuffle_group(r, pr)
        used[label] = used.get(label, 0) + 1
        out.append(pr)
    return out

def header(name, seed, n):
    return f"# python tools/hostgiac/gen_factor.py --family {name} --seed {seed} --n {n}"
def render(name, seed, n, tags=False):
    lines = [header(name, seed, n), "# factor(<expanded>)   # expect: <steps|prime|nosteps-ok> ; <the irreducible factors it was built from>"]
    lines += [(f"{p.label}\t" if tags else "") + p.line() for p in generate(name, seed, n)]
    return "\n".join(lines) + "\n"

# ---------------- self-check ----------------
def selfcheck():
    fails, counts = [], {}
    def bad(name, msg): fails.append(f"{name}: {msg}")
    for name in FAMILIES:
        probs = generate(name)
        counts[name] = len(probs)
        if len(probs) < 40: bad(name, f"only {len(probs)} problems")
        texts, keys = set(), set()
        for p in probs:
            if p.tag not in ("steps", "prime", "nosteps-ok"): bad(name, f"tag {p.tag} in {p.line()}")
            if p.text in texts or pkey(p.poly) in keys: bad(name, f"duplicate {p.text}")
            texts.add(p.text)
            keys.add(pkey(p.poly))
            line = p.line()
            m = re.fullmatch(r"factor\((.*)\)   # expect: (\S+) ; ([^;]*?)(?: ; .*)?", line)
            if not m: bad(name, f"line format {line}"); continue
            try:
                poly, built = parse_poly(m.group(1)), parse_poly(m.group(3))
            except ValueError as e:
                bad(name, f"unparsable {line}: {e}"); continue
            if poly != p.poly: bad(name, f"printed text does not parse back to the polynomial: {line}")
            if poly != built: bad(name, f"factors do not multiply to the polynomial: {line}")
            for a, mult in p.atoms:
                if set(a) != {ZERO} and atom_status(a) == "bad": bad(name, f"factor {poly_text(a)} not irreducible/primitive in {line}")
            nvar = len({i for k in poly for i, e in enumerate(k) if e})
            if name == "prime" and nvar == 1:
                cs = uni_coeffs(poly)[1]
                deg = len(cs) - 1
                if deg in (2, 3) and has_rational_root(cs): bad(name, f"rational root in {line}")
                if deg == 2:
                    disc = int(cs[1] ** 2 - 4 * cs[0] * cs[2])
                    if m.group(2) == "prime" and disc >= 0: bad(name, f"prime-tagged quadratic with discriminant {disc}: {line}")
                    if m.group(2) != "prime" and (disc < 0 or is_square(disc)): bad(name, f"nosteps-ok quadratic with discriminant {disc}: {line}")
                if m.group(2) == "prime" and deg == 1 and cs[0] == 1: bad(name, f"monic linear tagged prime: {line}")
            if name == "prime" and p.count != 1: bad(name, f"prime problem with {p.count} factors")
            if name == "grouping":
                t = split_terms(m.group(1))
                if len(t) != 4 or not any_pairing(t): bad(name, f"no grouping pairing in {line}")
                elif p.shuffled == pairing_works(*t[:4]): bad(name, f"first pairing {'works' if p.shuffled else 'fails'} but shuffled={p.shuffled}: {line}")
        if name == "grouping":
            ns = sum(p.shuffled for p in probs)
            if abs(ns - len(probs) / 3) > 1.5: bad(name, f"{ns} of {len(probs)} shuffled")
        if render(name, DEFAULT_SEED, DEFAULT_N) != render(name, DEFAULT_SEED, DEFAULT_N): bad(name, "two runs differ")
        if render(name, DEFAULT_SEED, DEFAULT_N) == render(name, DEFAULT_SEED + 1, DEFAULT_N): bad(name, "seed has no effect")
    for name in FAMILIES:
        print(f"{name:10s} {counts[name]:3d} problems")
    if fails:
        print(f"SELFCHECK FAILED ({len(fails)})")
        for f in fails[:40]: print("  " + f)
        return 1
    print("SELFCHECK OK")
    return 0

def main():
    ap = argparse.ArgumentParser(description="Factoring problems built from chosen factors, in giac syntax.")
    ap.add_argument("--family", choices=FAMILIES)
    ap.add_argument("--all", action="store_true", help="write corpus_factor_<family>.txt for every family into --outdir")
    ap.add_argument("--outdir", default=".")
    ap.add_argument("--selfcheck", action="store_true")
    ap.add_argument("--seed", type=int, default=DEFAULT_SEED)
    ap.add_argument("--n", type=int, default=DEFAULT_N, help=f"problems per family (default {DEFAULT_N})")
    ap.add_argument("--tags", action="store_true", help="prefix each line with its problem kind and a tab")
    args = ap.parse_args()
    if args.selfcheck: sys.exit(selfcheck())
    if args.all:
        for name in FAMILIES:
            with open(os.path.join(args.outdir, f"corpus_factor_{name}.txt"), "w", newline="\n") as f:
                f.write(render(name, args.seed, args.n))
        return
    if not args.family: ap.error("give --family NAME, --all or --selfcheck")
    sys.stdout.reconfigure(newline="\n")
    sys.stdout.write(render(args.family, args.seed, args.n, args.tags))

if __name__ == "__main__":
    main()
