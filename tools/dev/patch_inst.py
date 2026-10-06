#!/usr/bin/env python3
"""Remove the installer's OS gate (the user asked for it on 2026-10-06): INST refused OS 5.8.2 and
later 5.8.x on the TI-84 Plus CE.

In INST.8xp (from the original project, no source published), after printing
"hardware=%i os=%i.%i.%i" it does:
    ld a,(iy+4) / or a / jr nz      ; TI-83 Premium CE: no gate
    ld a,(iy+6) / cp 5 / jr nz      ; OS 5.
    ld a,(iy+7) / cp 8 / jr nz      ; OS 5.8.
    ld a,(iy+8) / cp 2 / jp nc,X    ; 5.8.2 and later: return the error ("Unknown OS version")
The patch turns that jp nc into four NOPs, then rewrites the file checksum. The install itself
runs the same code on every OS version.

usage: patch_inst.py <INST.8xp in> <INST.8xp out>
"""
import sys

HEADER = 55 + 17 + 2           # file header, variable entry, data size
BASE = 0xD1A87F                 # load address of the program's first byte (EF 7B)
SITE = 0xD1CECC                 # jp nc,D1D107
GATE = bytes.fromhex('d2 07 d1 d1')
CONTEXT = bytes.fromhex('fd 7e 08 fe 02')  # ld a,(iy+8) / cp 2, just before


def main():
    src, dst = sys.argv[1], sys.argv[2]
    d = bytearray(open(src, 'rb').read())
    i = HEADER + SITE - BASE
    if d[i:i + 4] == bytes(4):
        print('already patched')
    else:
        assert d[i - 5:i] == CONTEXT and d[i:i + 4] == GATE, 'unexpected INST.8xp (not the known build)'
        d[i:i + 4] = bytes(4)
    d[-2:] = (sum(d[55:-2]) & 0xFFFF).to_bytes(2, 'little')  # the 8xp checksum
    open(dst, 'wb').write(d)
    print('patched', dst)


if __name__ == '__main__':
    main()
