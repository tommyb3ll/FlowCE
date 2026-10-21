# Install FlowCE on OS 5.8.5

The website reads the calculator's OS and selects **arTIfiCE v3** for 5.8.5. It supplies and transfers
all required files, prepares RAM, and starts INST automatically. Follow the two short prompts
to open Inequalz and enable AsmHook2. A reset may also require reconnecting the calculator.
OS 5.8.4 and earlier, including 5.8.4.0058, use **arTIfiCE v2.1** instead.
The instructions below are the alternative **TI Connect CE** route.
Keep your current OS while troubleshooting; an upgrade is not a confirmed fix for INST errors.

Back up your calculator first. FlowCE fills most of the archive and replaces the other apps.

1. **Clear the archive before starting:** `2nd` `+` → `7:Reset…` → right arrow to
   **ARCHIVE** → `3:Both…` → `2:Reset`.
2. **Set up arTIfiCE v3** using [its official tutorial](https://yvantt.github.io/arTIfiCE/):
   - Install TI's Inequalz app.
   - Reset RAM, then send v3's `arTIfiCE.8xp` **to RAM**. Its program name is **PPPP**.
   - Open Inequalz, dismiss its splash screen, and stay on the Functions (Y=) screen.
   - Now send `INEQUVAR.8xv`. The calculator installs AsmHook2 and restarts.
   - Press **ON** if the screen stays black after the restart.
3. **Enable assembly programs:** `apps` → **AsmHook2**. Dismiss “Installation successful!”
   and return to the home screen.
4. **Send FlowCE:** download [FlowCE](https://tommyb3ll.github.io/FlowCE/files/FlowCE.b84)
   or [FlowCE Python](https://tommyb3ll.github.io/FlowCE/files/FlowCE-python.b84)
   and send the bundle with TI Connect CE. **INST goes in RAM; AppIns files go in Archive.**
   Do not reset RAM or erase apps again between enabling AsmHook2 and running INST.
5. **Run INST by name:** `prgm` → select **INST** → `enter` → `enter`.
   The bundle also contains program **A** for older OS versions; do not launch A on 5.8.5.
   At “enter: install app”, press `enter`. Let the countdown finish without touching the calculator.
6. At **“Success! Will now reset”**, press `enter`. After **“RAM Cleared”**, press `clear`,
   then `apps` → **FlowCE**. If it freezes at the success message, use the reset button on the back.

## What was verified

On October 21, 2026, FlowCE **2.1** and FlowCE Python **1.9.1** passed eight native/automatic
installation checks in CEmu on **OS 5.8.2.0029 and 5.8.5.0074**, with **boot code 5.6.1.0006**.
The installed apps matched the original images byte for byte. Checks also covered arithmetic,
a derivative, a definite integral, a limit, exiting and reopening, plus FlowCE's steps viewer.

The website bundles now use Bernard Parisse's updated INST with Cesium's port-unlock code,
adapted to retain OS 5.8.5 support. Only INST changed; the apps and arTIfiCE are unchanged.
Use the website downloads above for this installer; historical GitHub release assets are unchanged.

This is emulator evidence for those OS and boot-code combinations. It does not yet verify newer
boot-code variants or reproduce the reported INST rejection on some OS 5.8.4.0058 calculators.
If INST says **“Unknown OS version”**, [report your model, OS and boot-code versions](https://github.com/tommyb3ll/FlowCE/issues/new/choose)
from the calculator's About screen. Changing arTIfiCE versions alone may not fix that boot-code issue.
