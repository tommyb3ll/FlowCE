// Test only: a fake WebUSB with a fake TI-84 Plus CE (FakeCE from fake_ce_test.js), for the installer.
// ?erase=stay  the archive reset keeps the USB connection (busy for a while, then answers)
// ?erase=drop  the reset restarts the USB: disconnect, and the browser forgets the device
// ?kept=N      N apps (TI's language apps on a friend's OS 5.3.1) survive the archive reset
// ?nodelete=1  and the calculator refuses to delete them
(() => {
  const P = new URLSearchParams(location.search);
  const MODE = P.get('erase') || 'stay';
  const ERASE_MS = +(P.get('ms') || 4000);
  const KEPT = +(P.get('kept') || 0), NODELETE = P.get('nodelete') === '1';
  const LANG = ['Deutsch', 'Espanol', 'Francais', 'Svenska', 'Portugal'];
  const startFiles = () => [
    { name: 'GAME', type: 0x05, size: 20000, archived: true },
    { name: 'CabriJr', type: 0x24, size: 120000, archived: true },
    ...LANG.slice(0, KEPT).map(name => ({ name, type: 0x24, size: 90000, archived: true, kept: true })),
  ];
  const sleep = ms => new Promise(r => setTimeout(r, ms));
  const gone = () => new DOMException('The device was disconnected.', 'NetworkError');

  class FakeCE2 extends window.FakeCE {
    constructor(opts) {
      super(opts);
      this.vendorId = 0x0451; this.productId = 0xe008;
      this.busyUntil = 0; this.gone = false; this.erased = null; this.launchKeys = null;
    }
    async transferIn(ep, len) {
      for (;;) {
        if (this.gone) throw gone();
        if (this.out.length && Date.now() >= this.busyUntil) return super.transferIn(ep, len);
        await sleep(20);
      }
    }
    async transferOut(ep, bytes) {
      while (Date.now() < this.busyUntil) { // the calculator doesn't read the USB while it erases
        if (this.gone) throw gone();
        await sleep(20);
      }
      if (this.gone) throw gone();
      return super.transferOut(ep, bytes);
    }
    onVirt(type, d, size) {
      if (type === 0x0010 && NODELETE) return this.emitVirt(0xee00, [0, 0x15]); // refused
      super.onVirt(type, d, size);
      if (type !== 0x0011) return;
      const k = this.keys, n = k.length;
      if (k[n - 1] === 0x90 && (k[n - 2] === 0x91 || k[n - 2] === 0x8f)) { // 2:Reset after 3:Both or 1:All Memory
        this.erased = k[n - 2] === 0x91 ? 'archive' : 'all';
        this.files = this.files.filter(f => f.kept);
        this.freeFlash = 3014656 - this.files.reduce((a, f) => a + f.size, 0);
        if (MODE === 'drop') setTimeout(() => window.fakeDrop(), 300);
        else this.busyUntil = Date.now() + ERASE_MS;
      }
    }
  }

  const usb = new EventTarget();
  let remembered = true;
  window.fake = new FakeCE2({ freeFlash: +(P.get('flash') || 400000), files: startFiles() });
  usb.requestDevice = async () => window.fake;
  usb.getDevices = async () => (remembered && !window.fake.gone ? [window.fake] : []);
  const fire = (type, device) => { const e = new Event(type); e.device = device; usb.dispatchEvent(e); };
  // the device restarts its USB: gone; it comes back as a new device the browser has forgotten
  window.fakeDrop = () => {
    const old = window.fake;
    old.gone = true; old.opened = false;
    fire('disconnect', old);
    window.fake = new FakeCE2({ freeFlash: old.freeFlash, files: old.files });
    window.fake.previous = old;
    remembered = false;
  };
  Object.defineProperty(navigator, 'usb', { value: usb, configurable: true });
})();
