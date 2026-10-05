#!/usr/bin/env python3
"""Install a KhiCAS build into the emulator exactly like the user does on the real calculator
(arTIfiCE v2.1 shell -> INST), then launch KhiCAS and save the ready console as a state.

Usage (WSL): install.py [variant=en] [state=base] [shotdir=/tmp]
Needs: $KB/out/<variant>/AppIns*.8xv (tools/dev/build.sh), $KB/emu/arTIfiCE.8xp, the ROM ($KHICAS_ROM).
"""
import glob, os, sys
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
    e.key('enter'); e.run(6000)                                        # INST resets the calculator
    e.key('clear'); e.run(300); e.os_fake()
    e.key('apps'); e.run(1000); e.key('2'); e.run(1500)                # APPS -> 2:KhiCAS
    e.key('enter'); e.run(1500)                                        # dismiss the splash screen
    print('installed', len(apps), 'AppIns ->', e.shot(f'install_{state}'))
    e.save(f'{KB}/emu/states/{state}.ce')
finally:
    e.close()
