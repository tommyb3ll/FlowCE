#!/usr/bin/env python3
"""Proof that the alg2-steps change to the factoring goldens is only the new lines.
usage: golden_diff_proof.py <git revision of the old goldens>   (run in the repository root)
For every problem of every golden except callers and todo_multivar: the new block, with its
"Check: multiply the factors", "SOAP:" and "Quadratic form:" step lines removed and the step count in
its header reduced by their number, must equal the old block byte for byte. callers.txt must be
identical as a whole. Prints "N problems, M unexplained differences" (and the checks' counts)."""
import re, subprocess, sys, glob, os

NEW = ('  Check: multiply the factors:', '  SOAP: Same, Opposite, Always Positive:', '  Quadratic form: substitute u:')


def blocks(text):
    out, cur = {}, None
    for line in text.split('\n'):
        if line.startswith('#'):
            continue
        if line.startswith('== '):
            cur = line.split('   [')[0]
            out[cur] = [line]
        elif cur is not None and line != '':
            out[cur].append(line)
    return out


def old_text(rev, path):
    return subprocess.run(['git', 'show', '%s:%s' % (rev, path)], capture_output=True, check=True).stdout.decode('utf-8').replace('\r\n', '\n')


def main(rev):
    n = bad = 0
    removed = dict.fromkeys(NEW, 0)
    for path in sorted(glob.glob('tests/host/factor/golden/*.txt')):
        name = os.path.basename(path)[:-4]
        new = open(path, encoding='utf-8', newline='').read().replace('\r\n', '\n')
        try:
            old = old_text(rev, path.replace(chr(92), '/'))
        except subprocess.CalledProcessError:
            print('%s: new golden, not in %s (not compared)' % (name, rev))
            continue
        if name == 'callers':
            same = new == old
            print('callers: %s' % ('byte-identical' if same else 'DIFFERENT'))
            bad += not same
            continue
        if name == 'todo_multivar':
            continue
        bo, bn = blocks(old), blocks(new)
        if list(bo) != list(bn):
            print('%s: the problems differ' % name)
            bad += 1
            continue
        for p in bo:
            n += 1
            nb = [bn[p][0]] + [l for l in bn[p][1:] if not l.startswith(NEW)]
            k = len(bn[p]) - len(nb)
            for l in bn[p][1:]:
                for t in NEW:
                    removed[t] += l.startswith(t)
            m = re.match(r'^(.*\[ok, )(\d+)( steps.*)$', nb[0])
            if m:
                nb[0] = m.group(1) + str(int(m.group(2)) - k) + m.group(3)
            if nb != bo[p]:
                bad += 1
                print('UNEXPLAINED %s: %s' % (name, p))
    print('%d problems, %d unexplained differences' % (n, bad))
    print('lines removed: ' + ', '.join('%s %d' % (t.strip().split(':')[0], c) for t, c in removed.items()))
    return 1 if bad else 0


sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else 'HEAD'))
