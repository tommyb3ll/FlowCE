#!/usr/bin/env python3
"""Install a KhiCAS build into the emulator exactly like the user does on the real calculator
(arTIfiCE v2.1 shell -> INST), then launch KhiCAS and save the ready console as a state.

Usage (WSL): install.py [variant=en] [state=base] [shotdir=/tmp]
Needs: $KB/out/<variant>/AppIns*.8xv (tools/dev/build.sh), $KB/emu/arTIfiCE.8xp, the ROM ($KHICAS_ROM).
"""
import glob, os, shutil, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB, ROM

variant = sys.argv[1] if len(sys.argv) > 1 else 'en'
state = sys.argv[2] if len(sys.argv) > 2 else 'base'
shots = sys.argv[3] if len(sys.argv) > 3 else '/tmp'
apps = sorted(glob.glob(f'{KB}/out/{variant}/AppIns*.8xv'))
if not apps:
    sys.exit(f'no AppIns files in {KB}/out/{variant} - run tools/dev/build.sh first')

e = Emu(rom=ROM, shotdir=shots)
try:
    e.run(3000); e.key('clear'); e.run(500)
    e.send(f'{KB}/emu/arTIfiCE.8xp', 'ram')
    e.send(f'{KB}/src/app_tools/INST.8xp', 'ram')
    for a in apps:
        e.send(a, 'archive')
    e.run(500); e.key('prgm'); e.run(300); e.key('enter'); e.run(300); e.key('enter'); e.run(6000)  # arTIfiCE shell
    e.os_fake(); e.run(300); e.key('enter', 40); e.run(3000)          # INST passes its OS check -> prompt
    e.os_real(); e.run(200); e.key('enter', 40); e.run(2000)          # install (OS restored before INST's reset)
    e.wait_stable(timeout=400000)
    if not any('written' in l for l in e.debug_output()):
        e.shot('install_failed'); sys.exit('INST did not finish writing (see install_failed.png)')
    # INST says 'Success! Will now reset'. Do not press ENTER there: INST then returns into flash it
    # has just overwritten with KhiCAS (where the arTIfiCE shell was), and whatever KhiCAS code lies
    # at that address runs - harmless for most builds, a hang for some (2026-10-05: one landed in
    # giac's root finder). A hardware reset (the button on the back) is always safe.
    e.reset(); e.run(6000)
    e.key('clear'); e.run(300); e.os_fake()
    e.key('apps'); e.run(1000); e.key('2'); e.run(1500)                # APPS -> 2:KhiCAS
    e.key('enter'); e.run(1500)                                        # dismiss the splash screen
    # KhiCAS must be running: its F-key bar (bottom rows) is pink (classic) or its status bar is
    # light (Focus); the TI home screen has a white bottom and a dark status bar.
    # (One build hung at startup on 2026-10-05 and the old script saved the state anyway.)
    bmp = f'/tmp/install_bar_{os.getpid()}.bmp'
    e.cmd(f'screenshot {bmp}')
    d = open(bmp, 'rb').read(); off = struct.unpack_from('<I', d, 10)[0]
    bar = d[off: off + 6 * 320 * 3]                                     # BMP rows are bottom-up
    top = d[off + 226 * 320 * 3: off + 236 * 320 * 3]                   # status bar rows 4-13
    focus_ui = sum(top) > len(top) * 200                                # Focus: a light status bar
    if bar.count(255) > len(bar) * 0.95 and not focus_ui:
        e.shot(f'install_{state}_notstarted')
        sys.exit('KhiCAS did not start (see install_%s_notstarted.png)' % state)
    print('installed', len(apps), 'AppIns ->', e.shot(f'install_{state}'))
    e.save(f'{KB}/emu/states/{state}.ce')
    shutil.copy(f'{KB}/out/{variant}/DEMO.map', f'{KB}/emu/states/{state}.map') # probe addresses
finally:
    e.close()
