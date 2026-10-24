// Test only: a fake WebUSB with a fake TI-84 Plus CE (FakeCE from fake_ce_test.js), for the installer.
// ?erase=stay  the archive reset keeps the USB connection (busy for a while, then answers)
// ?erase=drop  the reset restarts the USB: disconnect, and the browser forgets the device
// ?kept=N      N apps (TI's language apps on a friend's OS 5.3.1) survive the archive reset
// ?nodelete=1  and the calculator refuses to delete them over D-USB
// ?badmenu=1   MEM's A key does not open the Apps list (the page's key deletion must stop at once)
(() => {
  const P = new URLSearchParams(location.search);
  const MODE = P.get('erase') || 'stay';
  const ERASE_MS = +(P.get('ms') || 4000);
  const KEPT = +(P.get('kept') || 0), NODELETE = P.get('nodelete') === '1', BADMENU = P.get('badmenu') === '1';
  const OS = (P.get('os') || '5.8.0.22').split('.').map(Number);
  // TI's six language apps and their sizes (the user's own backup, listed by OS 5.8.2 in CEmu)
  const LANG = [['Deutsch', 46763], ['Espanol', 46986], ['Francais', 51309], ['Nederlan', 47654], ['Portug', 52123], ['Svenska', 47445]];
  const startFiles = () => [
    { name: 'GAME', type: 0x05, size: 20000, archived: true },
    { name: 'CabriJr', type: 0x24, size: 120000, archived: true },
    ...LANG.slice(0, KEPT).map(([name, size]) => ({ name, type: 0x24, size, archived: true, kept: true })),
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
      this.menus(d[4]);
      const k = this.keys, n = k.length;
      if (k[n - 1] === 0x90 && (k[n - 2] === 0x91 || k[n - 2] === 0x8f)) { // 2:Reset after 3:Both or 1:All Memory
        this.erased = k[n - 2] === 0x91 ? 'archive' : 'all';
        this.files = this.files.filter(f => f.kept);
        this.freeFlash = 3014656 - this.files.reduce((a, f) => a + f.size, 0);
        if (MODE === 'drop') setTimeout(() => window.fakeDrop(), 300);
        else this.busyUntil = Date.now() + ERASE_MS;
      }
    }
    // MEM's Mem Management/Delete... Apps list, as TI-OS 5.8.2 in CEmu (2026-10-24): MEM, 2, A (Apps); del
    // asks Are You Sure?, 2 (Yes) deletes the first app of the list; del on the empty list goes home (2 then
    // types a 2). ?badmenu=1: A does not open the Apps list, 2 opens Real... (del and 2 there delete a real
    // variable: realDeleted counts them, the page must never get there)
    menus(key) {
      const s = this.screen;
      if (key === 0x40 || key === 0x09) this.screen = 'home'; // quit, clear
      else if (key === 0x36) this.screen = 'mem';
      else if (s === 'mem') this.screen = key === 0x90 ? 'memmgmt' : 'other';
      else if (s === 'memmgmt' && key === 0x9a && !BADMENU) this.screen = 'apps';
      else if (s === 'memmgmt' && key === 0x90) this.screen = 'real';
      else if (s === 'apps' && key === 0x0a) this.screen = this.files.some(f => f.type === 0x24) ? 'confirm' : 'home';
      else if (s === 'confirm' && key === 0x90) {
        const i = this.files.findIndex(f => f.type === 0x24);
        this.freeFlash += this.files[i].size;
        this.files.splice(i, 1);
        this.screen = 'apps';
      } else if (s === 'real' && key === 0x0a) this.screen = 'confirmReal';
      else if (s === 'confirmReal' && key === 0x90) { this.realDeleted = (this.realDeleted || 0) + 1; this.screen = 'real'; }
    }
  }

  const usb = new EventTarget();
  let remembered = true;
  window.fake = new FakeCE2({ os: OS, freeFlash: +(P.get('flash') || 400000), files: startFiles() });
  usb.requestDevice = async () => window.fake;
  usb.getDevices = async () => (remembered && !window.fake.gone ? [window.fake] : []);
  const fire = (type, device) => { const e = new Event(type); e.device = device; usb.dispatchEvent(e); };
  // the device restarts its USB: gone; it comes back as a new device the browser has forgotten
  window.fakeDrop = () => {
    const old = window.fake;
    old.gone = true; old.opened = false;
    fire('disconnect', old);
    window.fake = new FakeCE2({ os: OS, freeFlash: old.freeFlash, files: old.files });
    window.fake.previous = old;
    remembered = false;
  };
  Object.defineProperty(navigator, 'usb', { value: usb, configurable: true });
})();
