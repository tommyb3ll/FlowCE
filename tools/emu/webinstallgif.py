#!/usr/bin/env python3
"""Step 3 of the web installer, recorded in the emulator as a captioned GIF for the page: the
arTIfiCE shell the page opens, the one enter, INST (patched as the page sends it) counting down and
resetting the calculator by itself, then apps -> FlowCE.

usage (WSL): webinstallgif.py <outdir> <FlowCE.b84>      writes install_web.gif (and the frames)
"""
import os
import sys
import tempfile
import zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, ROM
from installgif import Rec, press
from instpatch import patch


def main():
    out, b84 = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    tmp = tempfile.mkdtemp()
    with zipfile.ZipFile(b84) as z:
        names = [n for n in z.namelist() if n.lower().endswith(('.8xp', '.8xv'))]
        z.extractall(tmp, names)
    inst = os.path.join(tmp, 'INST.8xp')
    patched = patch(open(inst, 'rb').read())
    open(inst, 'wb').write(patched)
    progs = sorted(n for n in names if n.lower().endswith('.8xp'))
    apps = sorted(n for n in names if n.lower().startswith('appins'))
    e = Emu(rom=ROM, shotdir=out)
    r = Rec(e, out)
    try:
        e.run(3000); e.key('clear'); e.run(500)
        for n in progs:                      # what the page sends, in its order
            e.send(os.path.join(tmp, n), 'auto')
        for n in apps:
            e.send(os.path.join(tmp, n), 'archive')
        e.run(500)
        for k in ('clear', 'prgm', 'enter', 'enter'):   # what the page presses
            press(e, k, 700)
        e.run(5000)
        r.frame(2600, 'The page opens the arTIfiCE shell')
        press(e, 'enter', 1500); r.frame(1600, 'Press enter: the only key')
        for _ in range(150):                 # the countdown, as it comes
            e.run(500)
            if any('written' in l for l in e.debug_output()):
                break
            r.frame(160, "Installing... don't touch it")
        e.run(2500); r.frame(3000, 'It restarts by itself: installed')
        press(e, 'clear', 400); press(e, 'apps', 1000); r.frame(1500, 'apps')
        e.cmd('key 2 80'); e.ms += 80
        for _ in range(14):
            e.run(100)
            r.frame(100, '2: FlowCE')
        e.run(1500); r.frame(2600, 'FlowCE is ready')
        r.save('install_web.gif')
    finally:
        e.close()


if __name__ == '__main__':
    main()
