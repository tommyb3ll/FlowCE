#!/usr/bin/env python3
"""The web installer's language-apps case on the emulated calculator, end to end, with the page's own D-USB
key packets (site/dusb.js pressKey): TI's apps sent to a fresh calculator, the page's archive reset (it keeps
TI's language apps), then the page's deletion (MEM, 2 Mem Management/Delete, A Apps, del and 2 (Yes) on each),
then FlowCE's files: all must fit. Screenshots of each stage.

Usage (WSL): langapps.py <dir of TI .8ek apps> <FlowCE files dir (AppIns*.8xv, INST.8xp)> <outdir>
Needs a CEmu built with tools/emu/cemu_dusbkey.py (its headless "dusb-key"): CEMU_HEADLESS=<that cemu-headless>.
TI's apps are TI's: take them from your own calculator (TI Connect CE backup); they are not in the repo.
"""
import glob, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, ROM

apps_dir, files_dir, out = sys.argv[1:4]
os.makedirs(out, exist_ok=True)
KEY = {'quit': 0x40, 'clear': 0x09, 'mem': 0x36, 'right': 0x01, 'k2': 0x90, 'k3': 0x91, 'k7': 0x95, 'capA': 0x9A, 'del': 0x0A}
e = Emu(rom=ROM, shotdir=out)
try:
    e.run(3000); e.key('clear'); e.run(500)
    apps = sorted(glob.glob(f'{apps_dir}/*.8ek'))
    for a in apps:  # one at a time, the cable unplugged after each (else TI-OS stays on "Receiving...")
        for attempt in range(3):
            try:
                e.send(a, 'archive'); break
            except RuntimeError:
                e.run(3000)
        e.run(8000); e.cmd('usb disconnect'); e.run(1500)
    e.key('clear'); e.run(800)
    print(len(apps), 'apps sent')

    def press(name, ms=800):
        r = e.cmd(f'dusb-key {KEY[name]:02x}')
        if not r.startswith('OK'):
            raise RuntimeError(f'{name}: {r}')
        e.run(ms)

    for k in ['quit', 'clear', 'mem', 'k7', 'right', 'k3']:  # the page's erase (installer.js erase)
        press(k)
    press('k2', 1500)
    h0, waited = e.hash(), 0
    while e.hash() == h0 and waited < 240000:  # "Please wait..." until "Arc Vars & Apps Cleared"
        e.run(1000); waited += 1000
    e.run(1500); e.shot('a_erased')
    for k in ['quit', 'clear', 'mem', 'k2', 'capA']:  # the page's deleteApps: the Apps list
        press(k)
    e.shot('b_kept_apps')
    for i in range(len(apps) + 1):  # (more pairs than apps: the page stops when the free memory stops growing)
        press('del'); press('k2', 1500)
    press('quit'); press('clear')
    e.cmd('usb disconnect'); e.run(1000)
    e.key('2nd'); e.key('+'); e.run(800); e.key('2'); e.run(1500); e.shot('c_after_delete')
    e.key('2nd'); e.key('mode'); e.run(800); e.key('clear'); e.run(800)
    files = [f'{files_dir}/INST.8xp'] + sorted(glob.glob(f'{files_dir}/AppIns*.8xv'))
    bad = []
    for f in files:
        try:
            e.send(f, 'ram' if f.endswith('.8xp') else 'archive')
        except RuntimeError:
            bad.append(os.path.basename(f))
    e.cmd('usb disconnect'); e.run(1000); e.key('clear'); e.run(500)
    e.key('2nd'); e.key('+'); e.run(800); e.key('2'); e.run(1500); e.shot('d_flowce_sent')
    print('FlowCE files sent:', len(files) - len(bad), 'of', len(files), 'refused:', bad)
    print('PASS' if not bad else 'FAIL')
finally:
    e.close()
