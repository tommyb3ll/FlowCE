#!/usr/bin/env python3
"""INST, patched as the web installer (site/installer.js, INST_PATCHES) sends it: no "enter: install
app" prompt, no wait at "Success! Will now reset", and out to the OS home screen instead of back into
the arTIfiCE shell, so the OS runs the kResetMem key INST leaves and the calculator resets by itself.
INST reads keys with os_GetCSC, which keys sent over USB never reach.

usage: instpatch.py <INST.8xp> <out.8xp>      (or import: patch(bytes) -> bytes of the 8xp)
"""
import sys

# (name, pattern, offset of the change in it, new bytes); the same bytes as INST_PATCHES
PATCHES = [
    # call os_GetCSC; cp sk_Enter; jr z,install; cp sk_Clear; jr nz,prompt -> ld a,sk_Enter
    ('prompt', bytes.fromhex('cd3c1d02 fe09 2815 fe0f 20f4'), 0, bytes.fromhex('3e090000')),
    # xor a; (ix-146) = a (success); jr wait-for-a-key -> jr past the wait, to the flag test
    ('success', bytes.fromhex('af 016effff ed2200 09 77 1810 21'), 10, bytes.fromhex('1817')),
    # ...; call ClrLCDFull; call HomeUp; call DrawStatusBar; pop hl; ret -> jp JForceCmdNoChar
    ('exit', bytes.fromhex('fdcb09a6 fdcb03c6 cd080802 cd280802 cd3c1a02 e1 c9'), 8, bytes.fromhex('c3600102')),
]
# where the success jumps land, from the start of its pattern: the wait loop, then the flag test
# that sets kbdKey = kResetMem
CHECKS = {'success': [(28, bytes.fromhex('cd3c1d02 b7 28f9')),
                      (35, bytes.fromhex('016effff ed2200 09 cb46 2011 3e4e 328c05d0'))]}


def patch(data):
    data = bytearray(data)
    for name, pat, off, new in PATCHES:
        i = data.find(pat)
        if i < 0 or data.find(pat, i + 1) >= 0:
            raise ValueError(f'INST {name}: pattern not found exactly once')
        for at, want in CHECKS.get(name, []):
            if data[i + at:i + at + len(want)] != want:
                raise ValueError(f'INST {name}: unexpected code at +{at}')
        data[i + off:i + off + len(new)] = new
    data[-2:] = (sum(data[0x37:-2]) & 0xFFFF).to_bytes(2, 'little')  # the 8xp checksum
    return bytes(data)


if __name__ == '__main__':
    src, dst = sys.argv[1:3]
    open(dst, 'wb').write(patch(open(src, 'rb').read()))
