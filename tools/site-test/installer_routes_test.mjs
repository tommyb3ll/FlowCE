// Exercise the real installer and DUSB protocol against FakeCE, with official local fixtures.
// node tools/site-test/installer_routes_test.mjs <Inequalz.8ek> <FlowCE.b84>
import fs from 'node:fs/promises';
import vm from 'node:vm';
import assert from 'node:assert/strict';
import { webcrypto } from 'node:crypto';
const root = new URL('../../', import.meta.url);
const [appPath, bundlePath] = process.argv.slice(2);
if (!appPath || !bundlePath) throw new Error('Pass the official Inequalz .8ek and a FlowCE .b84 fixture.');
const appBytes = new Uint8Array(await fs.readFile(appPath));
const bundleBytes = await fs.readFile(bundlePath);
const protocol = await fs.readFile(new URL('site/dusb.js', root), 'utf8');
const dusb = await import('data:text/javascript;base64,' + Buffer.from(protocol).toString('base64'));
const bundleFiles = await dusb.readBundle(bundleBytes.buffer.slice(bundleBytes.byteOffset,bundleBytes.byteOffset+bundleBytes.byteLength));
const bundleInst = dusb.readTIFile(bundleFiles.find(f => /(^|\/)INST\.8xp$/i.test(f.name)).bytes)[0];
const fakeContext = vm.createContext({ window: {} });
vm.runInContext(await fs.readFile(new URL('tools/site-test/fake_ce_test.js', root), 'utf8'), fakeContext);
const FakeCE = fakeContext.window.FakeCE;
const source = (await fs.readFile(new URL('site/installer.js', root), 'utf8'))
  .replace(/^import .*;\r?\n/m, '').replace(/init\(\);\s*$/, '');

async function session(os, { freeFlash = 4000000, dropOnErase = false, dropOnRam = false, corruptApp = false } = {}) {
  class Board extends FakeCE {
    onVirt(type, data, size) {
      super.onVirt(type, data, size);
      if (type === 0x0011 && this.keys.slice(-2).join() === [dusb.KEY.k3,dusb.KEY.k2].join()) {
        this.freeFlash = 3014656;
        this.erases = (this.erases || 0) + 1;
        this.dropNextPing = dropOnErase;
      }
      if (type === 0x0011 && this.keys.slice(-2).join() === [dusb.KEY.k1,dusb.KEY.k2].join()) this.dropNextPing = dropOnRam;
    }
  }
  const fake = new Board({ os, freeFlash });
  const elements = new Map(), usbEvents = new Map();
  function element(id) {
    if (!elements.has(id)) {
      const classes = new Set();
      elements.set(id, { hidden: false, checked: false, disabled: false, files: [], style: {}, dataset: { n: '1' },
        classes, classList: { add: x => classes.add(x), remove: (...xs) => xs.forEach(x => classes.delete(x)), toggle() {} },
        addEventListener() {}, setAttribute() {}, scrollIntoView() {}, focus() {},
        querySelector: sub => element(id + sub), closest: sub => element(id + sub) });
    }
    return elements.get(id);
  }
  let cancel = false;
  class Calculator extends dusb.Calculator {
    async ping(...args) {
      if (fake.dropNextPing) { fake.dropNextPing = false; throw new DOMException('Reset USB', 'NetworkError'); }
      return super.ping(...args);
    }
    static async request() {
      if (cancel) { cancel = false; throw new DOMException('Cancelled', 'NotFoundError'); }
      const c = new Calculator(fake); await c.open(); return c;
    }
  }
  const c = await Calculator.request();
  const context = vm.createContext({ ...dusb, Calculator, crypto: webcrypto, current: c, console: { info() {}, warn() {} },
    navigator: { userAgent: 'test', usb: { addEventListener: (name, fn) => usbEvents.set(name, fn), getDevices: async () => [] } },
    document: { getElementById: element, querySelector: element, querySelectorAll: () => [] },
    window: { addEventListener() {} }, location: { search: '' }, URLSearchParams,
    setTimeout: fn => setTimeout(fn, 0), clearTimeout, setInterval, clearInterval,
    fetch: async path => {
      if (path.endsWith('.b84')) return new Response(bundleBytes);
      if (path === 'artifice-v3/Inequalz.8ek') return new Response(corruptApp ? appBytes.slice(0,-1) : appBytes);
      if (path.endsWith('.txt')) return new Response('test');
      return new Response(await fs.readFile(new URL('site/' + path, root)));
    },
  });
  vm.runInContext(source, context);
  const run = text => vm.runInContext(text, context);
  await run('loadBundle()');
  run('init(); calc=current;');
  await run('check(calc)');
  return { fake, element, run, usbEvents, cancel: () => { cancel = true; }, context, c, Calculator };
}

// OS detection selects exactly one route and refuses missing/future metadata.
for (const [os, expected] of [[[5,8,4,58],'v2'], [[5,8,5,74],'v3'], [[5,8,6,0],null], [[5,2,0,0],null]]) {
  const s = await session(os);
  assert.equal(s.run('state.route'), expected);
  assert.equal(s.run('state.blocked'), !expected);
  if (!expected) { await s.run('install()'); assert.equal(s.fake.vars.length, 0); }
}
const unknown = await session([5,8,5,74]);
await unknown.run('check({info:async()=>({product:"CE",os:null,osText:"unknown",freeFlash:4000000})})');
assert.equal(unknown.run('state.blocked'), true);
console.log('OS routing: 5.8.4.0058=v2; 5.8.5.0074=v3; unknown/future/too old blocked');

const s = await session([5,8,5,74]);
await s.run('install()'); assert.equal(s.fake.vars.length, 0, 'RAM reset requires consent');
s.element('erase-ok').checked = true;
await s.run('install()');
assert.equal(s.run('v3Stage'), 'inequalz');
assert.equal(s.fake.vars.map(v => v.name).join(), 'Inequalz,PPPP');
assert.equal(s.fake.vars[0].type, 0x24);
s.cancel(); await s.run('continueV3()'); assert.equal(s.run('v3Stage'), 'inequalz');
assert.equal(s.fake.vars[1].archived, false);
await s.run('continueV3()');
assert.equal(s.run('v3Stage'), 'asmhook');
assert.equal(s.fake.vars.map(v => v.name).join(), 'Inequalz,PPPP,INEQUVAR');
assert.equal(s.fake.vars[2].archived, false);
// A different OS must not receive the prepared v3 payload or FlowCE files.
s.fake.os = [5,8,4,58]; await s.run('continueV3()'); assert.equal(s.fake.vars.length, 3);
s.fake.os = [5,8,5,74];
// A space error after the hook must not erase the hook or send a partial bundle.
s.fake.freeFlash = 100; await s.run('continueV3()'); assert.equal(s.fake.vars.length, 3);
s.fake.freeFlash = 4000000; await s.run('continueV3()');
assert.equal(s.run('waitingReset'), true);
assert.equal(s.fake.vars.some(v => v.name === 'A'), false);
assert.equal(s.fake.vars.find(v => v.name === 'INST').archived, false);
assert.ok(s.fake.vars.filter(v => v.name.startsWith('AppIns')).every(v => v.archived));
assert.equal(s.fake.keys.filter(k => k === dusb.KEY.mem).length, 1, 'RAM reset occurs only before the payload, never after AsmHook2');
assert.equal(s.fake.keys.slice(-4).join(), [dusb.KEY.clear,dusb.KEY.prgm,dusb.KEY.enter,dusb.KEY.enter].join());
const original = new Uint8Array(bundleInst.data);
assert.notDeepEqual(new Uint8Array(s.fake.vars.find(v => v.name === 'INST').data), original);
assert.deepEqual(new Uint8Array(s.fake.vars.find(v => v.name === 'INST').data), new Uint8Array(s.run('vars.find(v=>v.name==="INST").data')));
s.run('calc=current;');
s.usbEvents.get('disconnect')({ device: s.fake });
assert.equal(s.run('waitingReset'), true, 'disconnect is not proof of completion');
assert.equal(s.element('.step[data-step="finish"]').classes.has('done'), false);
s.run('finished()'); assert.equal(s.run('waitingReset'), false);
console.log('v3: files fetched automatically -> signed app -> automatic RAM reset -> PPPP -> two calculator prompts -> automatic INST + AppIns');
console.log('v3 recovery: picker cancel, changed OS, low space, disconnect checked; no v2 payload or extra reset');

const old = await session([5,8,4,58]);
await old.run('install()');
assert.ok(old.fake.vars.some(v => v.name === 'A'));
assert.equal(old.fake.vars.some(v => v.name === 'PPPP' || v.name === 'INEQUVAR'), false);
assert.equal(old.fake.keys.join(), [dusb.KEY.clear,dusb.KEY.prgm,dusb.KEY.enter,dusb.KEY.enter].join());
assert.equal(old.run('waitingReset'), true);
assert.throws(() => dusb.readAppFile(appBytes.slice(0,-1)), /incomplete/);
console.log('v2: original shell flow retained; incomplete signed app rejected');

for (const dropOnErase of [false,true]) {
  const erase = await session([5,8,5,74], {freeFlash:100,dropOnErase});
  await erase.run('install()');
  assert.equal(erase.fake.keys.length,0,'erase requires consent');
  erase.element('erase-ok').checked = true;
  await erase.run('install()');
  if (dropOnErase) {
    assert.equal(erase.run('awaitingReconnect'),true);
    assert.equal(erase.fake.vars.length,0,'no files sent while reset USB is disconnected');
    erase.context.returned = await erase.Calculator.request();
    await erase.run('resume(returned)');
  }
  assert.equal(erase.fake.erases,1);
  assert.equal(erase.run('v3Stage'),'inequalz');
  assert.equal(erase.fake.vars.map(v=>v.name).join(),'Inequalz,PPPP');
}
console.log('archive reset: consent required; v3 resumes correctly with USB retained or reconnected');

const ram = await session([5,8,5,74],{dropOnRam:true});
ram.element('erase-ok').checked = true;
await ram.run('install()');
assert.equal(ram.run('v3Stage'),'ram-reset');
assert.equal(ram.fake.vars.map(v=>v.name).join(),'Inequalz');
await ram.run('continueV3()');
assert.equal(ram.run('v3Stage'),'inequalz');
assert.equal(ram.fake.vars.map(v=>v.name).join(),'Inequalz,PPPP');
const bad = await session([5,8,5,74],{freeFlash:100,corruptApp:true});
bad.element('erase-ok').checked = true;
await bad.run('install()');
assert.equal(bad.fake.vars.length,0);
assert.equal(bad.fake.keys.length,0,'damaged dependency fails before any erase or reset');
console.log('automatic setup: RAM reset reconnect recovery and dependency integrity before erase checked');
