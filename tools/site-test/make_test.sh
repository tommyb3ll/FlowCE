#!/bin/bash
# The installer page with a fake calculator (no calculator, no USB): builds _site, then
# _site/test.html = index.html with fake_ce_test.js (a fake TI-84 Plus CE speaking DUSB) and
# fake_usb.js (a fake navigator.usb) loaded before installer.js. Serve _site (launch.json
# flowce-site-test, port 8643) and open test.html with options:
#   ?flash=N        free archive before the erase (default 400000: an erase is needed)
#   ?erase=stay     the archive reset keeps the USB connection (busy ms= milliseconds, default 4000)
#   ?erase=drop     the reset restarts the USB (the page shows Reconnect calculator)
#   ?kept=N         N apps (TI's language apps) survive the reset; ?nodelete=1: deleting them fails
# usage (WSL): tools/site-test/make_test.sh <FlowCE.b84> [version]
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
bash "$root/site/build.sh" "$1" "${2:-dev}"
cp "$here/fake_ce_test.js" "$here/fake_usb.js" "$root/_site/"
sed 's|<script type="module" src="installer.js"></script>|<script src="fake_ce_test.js"></script><script src="fake_usb.js"></script>\n<script type="module" src="installer.js"></script>|' \
  "$root/_site/index.html" > "$root/_site/test.html"
echo "test page: _site/test.html"
