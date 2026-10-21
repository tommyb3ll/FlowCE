#!/usr/bin/env python3
"""Stage the official signed prerequisite for the browser; verify before publishing it.
Usage: fetch_inequalz.py <output-directory> [existing-local-file]
"""
import hashlib
from pathlib import Path
import sys
from urllib.request import urlopen

URL = ('https://education.ti.com/download/en/ed-tech/8C7FB96338F9469BB9D970AFC034EC39/'
       '9E4EDC4007024B58AE81D60571B97E7B/Inequalz_CE_5.1.0.0110.8ek')
SHA = '1899cf87281941bde537c184a92dd9435ff0aff7bca0899da25c654ed4d9fdca'
out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
if len(sys.argv) > 2:
    data = Path(sys.argv[2]).read_bytes()
else:
    with urlopen(URL, timeout=60) as response:
        data = response.read(44812)
if len(data) != 44811 or hashlib.sha256(data).hexdigest() != SHA:
    raise SystemExit('Official Inequalz download did not match the pinned file; refusing to stage it.')
(out / 'Inequalz.8ek').write_bytes(data)
print('Staged official Inequalz CE 5.1.0.0110 (SHA-256 verified).')
