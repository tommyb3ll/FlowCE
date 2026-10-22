# pack_app.py size <body.bin>            -> the address to link the code at (LOAD_ADDR)
# pack_app.py pack <body.bin> <entry> <load_addr> <out _DEMO.bin>
# The app image for CE toolchain v15 builds (tools/dev/wsl_build_v15.sh); body.bin is the linked
# app (objcopy -O binary). KhiCAS's installer (INST + AppIns) takes the image that reloc.cc makes
# from the fasmg link: a TI app header (0x100 bytes), an 'eZ8' header (0x2a), the code, the data,
# a footer (0x104), the whole ending at 0x3AFFFD in flash. With GNU ld the code is linked at its
# final address directly (no relocation table); the toolchain's crt0 (HAS_APPLICATION) copies
# .data itself, so the OS copies no data (size 0).
import os, struct, sys

# the name in the APPS menu (8 letters at most) and the description: the build's (KhiCAS + Flow's:
# tools/dev/wsl_build_v15.sh), else FlowCE's
NAME, VERSION = os.environ.get('APP_NAME', 'FlowCE').encode()[:8], b'5.0.0.0000'
INFO = os.environ.get('APP_INFO', 'FlowCE - calculus CAS - GNU GPL').encode() + b'\0'
END = 0x3AFFFD
FOOTER = b'\x02\x3e\x01\x00' + b'\xff' * 256
HDR = 0x12a

def total(body_len):
    return HDR + body_len + len(INFO) + len(FOOTER)

def u24(v):
    assert 0 <= v < 1 << 24
    return struct.pack('<I', v)[:3]

def pack(body, entry, load_addr):
    # the link address is aligned down to 16 (section alignment padding then matches the first
    # link's, at 0): 0xFF bytes after the body make the app end at END again
    pad = END - load_addr - len(body) - len(INFO) - len(FOOTER)
    assert 0 <= pad < 16, pad
    body = body + bytes([0xff]) * pad
    n = total(len(body))
    start = END - n
    assert load_addr == start + HDR, 'linked at %x, expected %x' % (load_addr, start + HDR)
    sig = HDR + len(body) + len(INFO)            # file offset of the footer
    h = bytearray()
    h += b'\x81\x0f' + struct.pack('>I', sig - 6)
    h += b'\x81\x12\x13\x0f'
    h += b'\x81\x2d' + bytes([len(VERSION) + 1]) + VERSION + b'\x00'
    h += b'\x81\x32\x59\x00'
    h += bytes([0x81, 0x40 | len(NAME)]) + NAME
    h += b'\x81\xa1\x07'
    h += b'\x03\x26\x09\x04'
    h += b'\x21\xbb\x6e\xdc'
    free = 247 - len(h)
    h += b'\x00\x0d' + bytes([free]) + b'\xff' * free
    h += b'\x81\x7f' + struct.pack('>I', sig - 0x100)
    assert len(h) == 0x100, hex(len(h))
    info_off = HDR + len(body) - 0x100
    data_off = info_off + len(INFO)
    h += b'eZ8' + NAME.ljust(8, b'\0') + b'\x00\x03' + b'\x00\x00' + u24(0)
    h += u24(0x2a) + u24(data_off) + u24(0) + u24(entry - load_addr + 0x2a) + u24(0) + u24(0)
    h += u24(info_off) + u24(0)
    assert len(h) == HDR, hex(len(h))
    out = bytes(h) + body + INFO + FOOTER
    assert len(out) == n and start + len(out) == END
    return out

if sys.argv[1] == 'size':
    body = open(sys.argv[2], 'rb').read()
    print('0x%06X' % ((END - total(len(body)) + HDR) & ~15))
else:
    body = open(sys.argv[2], 'rb').read()
    entry, load_addr = int(sys.argv[3], 0), int(sys.argv[4], 0)
    data = pack(body, entry, load_addr)
    open(sys.argv[5], 'wb').write(data)
    print('wrote %s: %d B, app at 0x%06X..0x%06X, entry 0x%06X' % (sys.argv[5], len(data), END - len(data), END, entry))
