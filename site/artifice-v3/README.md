# arTIfiCE v3 CE files

Unmodified files from YvanTT's official v3 CE release:
https://github.com/YvanTT/arTIfiCE/releases/download/v3/arTIfiCE_v3_CE.zip

Instructions and credits: https://yvantt.github.io/arTIfiCE/

SHA-256:

- arTIfiCE.8xp: `98cbadab42f34d63542d3648f97ff4ad20527ff39c35df12bbc00842454d262b`
- INEQUVAR.8xv: `cc47cdda6fef69590a5377c095443e1cda911454ef68c173f0de5d17be2a0000`

Only OS 5.8.5 uses these files. Earlier supported OS versions use the v2.1 file already in the
FlowCE bundle. The browser verifies these hashes before starting the v3 install.
The site build also stages the unmodified official TI Inequalz CE 5.1.0.0110 app as Inequalz.8ek,
using site/fetch_inequalz.py. It verifies SHA-256
`1899cf87281941bde537c184a92dd9435ff0aff7bca0899da25c654ed4d9fdca` before staging.
Inequalz is TI software, separate from FlowCE's GPL source. Its original signed data is unchanged.
The browser verifies it again before any erase or transfer. Users do not download or select a file.
