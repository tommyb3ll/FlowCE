#!/usr/bin/env python3
"""Prepare Bernard's 2026-10-17 INST for FlowCE's supported OS 5.8.5 route.

Usage: prepare_installer.py <official INST.8xp> <output INST.8xp>
See app_tools/INST.md for provenance, hashes and validation limits.
"""
import hashlib
from pathlib import Path
import sys

SOURCE_SHA256 = '499158dc437dfb24f1e051c7094a0925e1d6aceaf399108bd95fb4a4fb894ad7'
OUTPUT_SHA256 = 'c585df72bbdb08f4b24751c60d7f5c7747a75dcee868f62a75258fc85e830a1f'


def prepare(data):
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError('Unexpected upstream INST; review its code before updating the pinned hash')
    if int.from_bytes(data[-2:], 'little') != sum(data[55:-2]) & 65535:
        raise ValueError('Invalid TI file checksum')
    result = bytearray(data)
    # ld a,(iy+8); cp 5; jp nc,return. Change only the ceiling: reject 5.8.6+.
    # Keep the upstream OS/model checks and Cesium port-unlock implementation.
    gate = bytes.fromhex('fd7e08fe05d207d1d1')
    if result.count(gate) != 1:
        raise ValueError('OS gate must occur exactly once')
    result[result.index(gate) + 4] = 6
    result[-2:] = (sum(result[55:-2]) & 65535).to_bytes(2, 'little')
    if hashlib.sha256(result).hexdigest() != OUTPUT_SHA256:
        raise ValueError('Prepared INST does not match the reviewed binary')
    return bytes(result)


if __name__ == '__main__':
    source, target = map(Path, sys.argv[1:3])
    target.write_bytes(prepare(source.read_bytes()))
    print('Prepared', target, OUTPUT_SHA256)
