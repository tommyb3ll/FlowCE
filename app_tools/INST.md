# FlowCE app installer

`INST.8xp` uses Bernard Parisse's updated installer with Cesium's port-unlock code, posted in
[FlowCE issue #1](https://github.com/tommyb3ll/FlowCE/issues/1#issuecomment-6322117239).
This addresses the installation mechanism implicated in newer factory boot-code failures;
the displayed OS version alone does not establish compatibility.

## Reproduce this binary

Extract `INST.8xp` from Bernard's [KhiCAS bundle](https://www-fourier.univ-grenoble-alpes.fr/~parisse/ti/webinst/files/khicas84.b84),
then run `python3 tools/dev/prepare_installer.py upstream/INST.8xp app_tools/INST.8xp`.
The script requires the reviewed input hash and verifies the exact output hash.

- Upstream file: 11,752 bytes, SHA256 `499158dc437dfb24f1e051c7094a0925e1d6aceaf399108bd95fb4a4fb894ad7`.
- FlowCE file: SHA256 `c585df72bbdb08f4b24751c60d7f5c7747a75dcee868f62a75258fc85e830a1f`.
- Upstream build comment: `2026-10-17T15:53:45+0200 c522052c`.

Upstream still silently returns on OS 5.8.5 before reaching the installation prompt. FlowCE
already supports launching on that OS through arTIfiCE v3/AsmHook2. One byte at file offset
`0x2696` changes `cp 5` to `cp 6`, allowing 5.8.5 while retaining the rejection of 5.8.6+ in
that gate. The TI file checksum is recomputed. The new port-unlock implementation is unchanged.
This supersedes the old `tools/dev/patch_inst.py` patch, which removed an older OS gate entirely.

The website's existing prompt/success/exit patches still match this binary exactly. Those
patches automate installation; they do not implement the newer boot-code unlock.

## Distribution and validation

`site/build.sh` uses `tools/dev/repack_installer.py` to replace only INST in downloaded release
bundles. AppIns data, arTIfiCE and ZIP entry order stay unchanged. The website and its manual
bundle downloads therefore use the same current installer. Historical GitHub release assets
remain their original files. New source builds take INST from this directory.

Emulator checks cover genuine OS 5.8.2.0029 and 5.8.5.0074 on boot 5.6.1.0006, both current app
variants, and native/website-patched INST. Full installed app bytes and math/steps/reopening
are checked. This does not establish physical WebUSB behavior or newer boot-code compatibility;
an affected device or its legitimate dump is still needed. Do not recommend an OS upgrade as
a guaranteed workaround for an installation failure.
