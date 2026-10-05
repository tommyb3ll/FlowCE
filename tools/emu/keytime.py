import sys
sys.path.insert(0, '/mnt/c/Users/tjb43/Downloads/Projects/khicas/KhiCAS-v2/tools/emu')
from cemu import Emu, KB
e = Emu(image=f'{KB}/emu/states/fx3.ce', shotdir='/tmp')
try:
    seq = ['alpha', 'sto', 'alpha', 'sto', '^', '2', 'right', '+', '1', 'alpha', 'sin', '(', 'alpha', 'sto', ')', 'enter']
    for k in seq:
        e.cmd(f'key {k} 80'); e.ms += 80
        t = e.wait_stable(step=5, stable=20, timeout=20000)
        print(f'{k:6s} busy {t+80:5d} ms after press')
finally:
    e.close()
