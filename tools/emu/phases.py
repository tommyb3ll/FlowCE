"""Timeline of the work KhiCAS does for each key: samples the drawing-phase probe (focus.cc
focus_phase) and getkey's poll counter every millisecond of emulated time while a key is held
(80 ms) and after it, for a fixed window.

  wsl python3 tools/emu/phases.py <state> [window_ms=300] key [key ...]

Prints the phase changes (t:old>new, ms from the press) and the intervals during which getkey
polled the keypad (idle; a key pressed outside them is lost).
Phases (focus.cc): 1 focus_disp, 2 hero layout, 3 hero clear, 4 hero math, 5 caret, 6 entries
scanned, 7 entries drawn, 8 hero drawn, 10/11 F-key bar (check / draw), 12/13 status bar
(check / draw); 0 = not drawing.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB, map_symbol

state, args = sys.argv[1], sys.argv[2:]
window = int(args.pop(0)) if args and args[0].isdigit() else 300
e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir='/tmp')
PH = map_symbol('_focus_phase', mapfile=e.mapfile)
try:
    e.wait_idle(timeout=5000)
    e.run(30)
    for k in args:
        e.cmd(f'keydown {k}')
        last_ph, last_p = e.peek(PH, 1)[0], e.polls()
        events, polls = [], []
        for t in range(1, window + 1):
            if t == 80:
                e.cmd(f'keyup {k}')
            e.run(1)
            ph, p = e.peek(PH, 1)[0], e.polls()
            if ph != last_ph:
                events.append(f'{t}:{last_ph}>{ph}')
            if p != last_p:
                if polls and t - polls[-1][1] <= 12:
                    polls[-1][1] = t
                else:
                    polls.append([t, t])
            last_ph, last_p = ph, p
        busy_end = next((a for a, b in polls if a > 15), None)
        print(f'{k:6s} polling {" ".join(f"{a}-{b}" for a, b in polls)}  | ' + ' '.join(events), flush=True)
finally:
    e.close()
