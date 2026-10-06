#!/usr/bin/env python3
"""The install, recorded in the emulator as captioned GIFs for the README: what the calculator
shows at each step and which keys to press. The same key presses as install.py, from a fresh ROM.

usage (WSL): installgif.py <outdir> [variant=en]
Writes install_1_reset.gif, install_3_run.gif, install_5_start.gif (and the frames).
"""
import glob
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB, ROM
from PIL import Image, ImageDraw, ImageFont

FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
BAR = 64  # caption bar height (2x screenshots: 640x480)


class Rec:
    def __init__(self, e, out):
        self.e, self.out, self.frames, self.n = e, out, [], 0

    def frame(self, ms, caption):
        self.n += 1
        self.frames.append((self.e.shot(f'f{self.n:04d}'), ms, caption))

    def save(self, name):
        font = ImageFont.truetype(FONT, 26)
        imgs = []
        for path, ms, cap in self.frames:
            shot = Image.open(path).convert('RGB')
            im = Image.new('RGB', (shot.width, shot.height + BAR), (31, 41, 55))
            im.paste(shot, (0, 0))
            d = ImageDraw.Draw(im)
            w = d.textlength(cap, font=font)
            d.text(((shot.width - w) / 2, shot.height + (BAR - 30) / 2), cap, font=font, fill=(255, 255, 255))
            imgs.append(im.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE))
        out = os.path.join(self.out, name)
        imgs[0].save(out, save_all=True, append_images=imgs[1:], duration=[ms for _, ms, _ in self.frames],
                     loop=0, optimize=True, disposal=1)
        print(name, len(imgs), 'frames', os.path.getsize(out), 'B', flush=True)
        self.frames = []


def press(e, key, ms=600):
    e.key(key, 80)
    e.run(ms)


def main():
    out = sys.argv[1]
    variant = sys.argv[2] if len(sys.argv) > 2 else 'en'
    os.makedirs(out, exist_ok=True)
    apps = sorted(glob.glob(f'{KB}/out/{variant}/AppIns*.8xv'))
    e = Emu(rom=ROM, shotdir=out)
    r = Rec(e, out)
    try:
        e.run(3000); e.key('clear'); e.run(500)
        # step 1: clear the memory
        r.frame(1400, 'Step 1: clear the memory')
        press(e, '2nd', 200); press(e, '+'); r.frame(1300, '2nd  +   (MEM)')
        press(e, '7'); r.frame(1300, '7: Reset...')
        press(e, 'right', 400); press(e, 'right'); r.frame(1300, 'right arrow twice: ALL')
        press(e, '1'); r.frame(1600, '1: All Memory...')
        press(e, '2', 3000); r.frame(800, '2: Reset')
        e.run(3000); r.frame(2200, 'Memory cleared')
        r.save('install_1_reset.gif')
        # step 2 (on the computer): send FlowCE.b84
        e.key('clear'); e.run(500)
        e.send(f'{KB}/emu/arTIfiCE.8xp', 'ram')
        e.send(f'{KB}/src/app_tools/INST.8xp', 'ram')
        for a in apps:
            e.send(a, 'archive')
        e.run(500)
        # step 3: run the installer
        r.frame(1400, 'Step 3: run the installer')
        press(e, 'prgm'); r.frame(1500, 'prgm')
        press(e, 'enter'); r.frame(1300, 'enter')
        press(e, 'enter', 6000); r.frame(1800, 'enter: the arTIfiCE shell opens')
        press(e, 'enter', 3000); r.frame(1800, 'enter: INST starts')
        press(e, 'enter', 1000); r.frame(1500, 'enter: install')
        for _ in range(150):  # the countdown, fast-forwarded
            e.run(4000)
            if any('written' in l for l in e.debug_output()):
                break
            r.frame(140, "Installing... don't touch it")
        e.run(3000); r.frame(3000, 'Success! Press enter')
        # (enter resets the calculator on real hardware; it can freeze instead, as it does in the
        # emulator: then the reset button on the back. The GIF shows that reset.)
        e.reset(); e.run(6000); r.frame(2600, 'Reset done: FlowCE is installed')
        r.save('install_3_run.gif')
        # step 5: start FlowCE
        e.key('clear'); e.run(600)
        r.frame(1200, 'Step 5: start FlowCE')
        press(e, 'clear'); press(e, 'apps', 1000); r.frame(1600, 'apps')
        e.cmd('key 2 80'); e.ms += 80
        for _ in range(14):
            e.run(100)
            r.frame(100, '2: FlowCE')
        e.run(1500); r.frame(2500, 'FlowCE is ready')
        r.save('install_5_start.gif')
    finally:
        e.close()


if __name__ == '__main__':
    main()
