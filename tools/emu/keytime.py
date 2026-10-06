"""Per-key busy time of the console: loads an emulator state, presses keys one at a time and
prints how long KhiCAS stays busy after each press (until getkey polls the keypad again), the
80 ms key hold included. A key pressed while KhiCAS is busy is lost (os_GetCSC does not buffer),
so these times bound how fast the user can type.

  wsl python3 tools/emu/keytime.py [state=fx4] [key key ...]
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB

state = sys.argv[1] if len(sys.argv) > 1 else 'fx4'
seq = sys.argv[2:] or ['alpha', 'sq', 'alpha', 'log', 'alpha', 'sin', '^', '2', 'right', '+', '1',
                       'alpha', 'sin', '(', 'alpha', 'sto', ')', 'enter']
e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir='/tmp')
try:
    if e.polls() is None:
        sys.exit('no _getkey_polls in the map: rebuild')
    e.wait_idle(timeout=5000)
    total = 0
    for k in seq:
        e.cmd(f'key {k} 80'); e.ms += 80
        t = e.wait_idle(step=2, timeout=60000)
        busy = max(80, t + 80 - 10) if t > 10 else 80
        total += busy
        print(f'{k:6s} busy ~{busy:5d} ms after press', flush=True)
    print(f'total {total} ms for {len(seq)} keys')
finally:
    e.close()
