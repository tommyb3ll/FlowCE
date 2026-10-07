// The FlowCE web installer: checks the calculator, erases it (after the user agrees), sends the
// release bundle (files/FlowCE.b84) over WebUSB and opens its installer, which then needs one key
// press on the calculator. Steps and texts: index.html.
import { Calculator, KEY, TI_CE, readBundle, readTIFile } from './dusb.js';

const $ = id => document.getElementById(id);
const step = name => document.querySelector(`.step[data-step="${name}"]`);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const MB = n => (n / 1e6).toFixed(2) + ' MB';
const OS_MAX = [5, 8, 4], OS_MIN = [5, 3, 0];
// Windows binds the calculator to TI's driver: the old one (tiehdusb) can't be used by browsers
const WINDOWS = /Windows/i.test(navigator.userAgentData ? navigator.userAgentData.platform : navigator.userAgent);

let calc = null;          // the connected calculator
let vars = null;          // the bundle's variables, in sending order
let autoInst = false;     // INST is patched (below): one key starts it, and it restarts the calculator
let state = null;         // the last check: { blocked, needErase }
let busy = false;         // installing
let waitingReset = false; // the installer runs on the calculator: its restart ends the install
let awaitingReconnect = false; // erased: waiting for the calculator to come back

// INST (KhiCAS's app_tools) waits for enter at "enter: install app" and for a key at "Success! Will
// now reset", then returns into the arTIfiCE shell, which needs mode before the OS runs the
// kResetMem key INST leaves for it. It reads the keyboard with os_GetCSC, which keys sent from the
// computer never reach. So INST is patched as it is sent, where exactly these bytes are found (the
// release's INST stays as it is): no prompt, no wait, and out to the home screen, which resets.
const INST_PATCHES = [
  // the prompt: call os_GetCSC; cp sk_Enter; jr z,install; cp sk_Clear; jr nz,prompt
  // -> ld a,sk_Enter: install at once
  { find: [0xcd, 0x3c, 0x1d, 0x02, 0xfe, 0x09, 0x28, 0x15, 0xfe, 0x0f, 0x20, 0xf4], at: 0, put: [0x3e, 0x09, 0x00, 0x00] },
  // "Success!": xor a; (ix-146) = a, success; jr wait-for-a-key -> jr past the wait, to the test of
  // that flag, which sets kbdKey = kResetMem. Checked: the wait (+28) and the test (+35) are there.
  {
    find: [0xaf, 0x01, 0x6e, 0xff, 0xff, 0xed, 0x22, 0x00, 0x09, 0x77, 0x18, 0x10, 0x21], at: 10, put: [0x18, 0x17],
    check: [[28, [0xcd, 0x3c, 0x1d, 0x02, 0xb7, 0x28, 0xf9]],
      [35, [0x01, 0x6e, 0xff, 0xff, 0xed, 0x22, 0x00, 0x09, 0xcb, 0x46, 0x20, 0x11, 0x3e, 0x4e, 0x32, 0x8c, 0x05, 0xd0]]],
  },
  // the program's exit: ...; call ClrLCDFull; call HomeUp; call DrawStatusBar; pop hl; ret (into
  // the shell) -> jp JForceCmdNoChar: the OS home screen, which runs the pending kResetMem
  {
    find: [0xfd, 0xcb, 0x09, 0xa6, 0xfd, 0xcb, 0x03, 0xc6, 0xcd, 0x08, 0x08, 0x02, 0xcd, 0x28, 0x08, 0x02, 0xcd, 0x3c, 0x1a, 0x02, 0xe1, 0xc9],
    at: 8, put: [0xc3, 0x60, 0x01, 0x02],
  },
];

function indexOf(a, pat, from = 0) {
  outer: for (let i = from; i <= a.length - pat.length; i++) {
    for (let j = 0; j < pat.length; j++) if (a[i + j] !== pat[j]) continue outer;
    return i;
  }
  return -1;
}

// INST's data, patched; null if it isn't the INST these patches were made for
function patchInst(data) {
  const d = data.slice();
  for (const p of INST_PATCHES) {
    const i = indexOf(d, p.find);
    if (i < 0 || indexOf(d, p.find, i + 1) >= 0) return null;
    for (const [off, bytes] of p.check || []) if (indexOf(d.subarray(i + off, i + off + bytes.length), bytes) !== 0) return null;
    d.set(p.put, i + p.at);
  }
  return d;
}

function mark(name, how) {
  const li = step(name);
  li.classList.remove('active', 'done');
  if (how) li.classList.add(how);
  li.querySelector('.num').textContent = how === 'done' ? '✓' : li.dataset.n;
}

function say(id, html) {
  $(id).innerHTML = html;
}

function cmp(a, b) {
  for (let i = 0; i < 3; i++) if (a[i] !== b[i]) return a[i] - b[i];
  return 0;
}

// Plain-English messages for what can go wrong
function friendly(e) {
  const msg = String(e && e.message || e);
  if (e && e.code === 'claim')
    return WINDOWS
      ? 'Windows is using an older TI USB driver that browsers can\'t open. Install TI Connect CE 6.1 or newer ' +
        '(free from TI, see the note at the top of the installer), unplug the calculator, plug it back in and try again.'
      : 'The browser can\'t open the calculator: another program may be using it. Close TI Connect CE, unplug the ' +
        'calculator, plug it back in and try again (on Linux, see "If something goes wrong" below).';
  if (e && e.code === 'timeout')
    return 'The calculator stopped answering. Make sure it is on the home screen, unplug it and plug it back in, then press Connect calculator again.';
  if (e && e.code === 'calc')
    return 'The calculator refused a file. Usually part of FlowCE is already on it: erase it, then try again.';
  if (e && e.name === 'NetworkError')
    return 'The calculator was disconnected. Plug it back in and press Connect calculator again.';
  if (e && e.name === 'SecurityError')
    return 'The browser blocked the connection to the calculator.';
  return msg;
}

// The message, and the browser's own words under it (for reports)
function problem(e) {
  const raw = String(e && e.message || e).replace(/[<>&]/g, c => ({ '<': '&lt;', '>': '&gt;', '&': '&amp;' }[c]));
  const f = friendly(e);
  return `<span class="bad">${f}</span>` + (f === raw ? '' : `<div class="fine">Details: ${raw}</div>`);
}

async function loadBundle() {
  if (vars) return vars;
  const res = await fetch('files/FlowCE.b84');
  if (!res.ok) throw new Error('could not download FlowCE (' + res.status + ')');
  const files = await readBundle(await res.arrayBuffer());
  const list = [];
  for (const f of files) {
    if (/\.8x[pv]$/i.test(f.name)) list.push(...readTIFile(f.bytes));
  }
  // the programs (arTIfiCE "A", INST) first, then AppIns00..42 in order
  list.sort((a, b) => (a.name.startsWith('AppIns') - b.name.startsWith('AppIns')) || a.name.localeCompare(b.name));
  const inst = list.find(v => v.name === 'INST');
  if (list.filter(v => v.name.startsWith('AppIns')).length < 2 || !inst)
    throw new Error('the FlowCE download is incomplete');
  const patched = patchInst(inst.data);
  if (patched) inst.data = patched;
  autoInst = !!patched;
  vars = list;
  return vars;
}

// (nothing to click while INST runs: whatever reaches the calculator then could cancel its reset)
function updateInstall() {
  $('btn-install').disabled = busy || waitingReset || !calc || !state || state.blocked || (state.needErase && !$('erase-ok').checked);
}

// The calculator's OS and free memory: what the install has to do
async function check(c = calc) {
  const info = await c.info();
  const need = vars.filter(v => v.archived).reduce((a, v) => a + v.data.length + 32, 0);
  const lines = [`${info.product}, OS ${info.osText}`];
  let blocked = false;
  if (info.os && cmp(info.os, OS_MAX) > 0) {
    blocked = true;
    lines.push(`<span class="bad">OS ${info.osText} isn't supported yet.</span> The installer uses arTIfiCE v2.1, which works up to OS 5.8.4.`);
  } else if (info.os && cmp(info.os, OS_MIN) < 0) {
    lines.push(`OS ${info.osText} is older than the versions FlowCE was tested on (5.3 to 5.8.4); it may not work.`);
  }
  // only the free archive decides (the RAM figure a real calculator reports is not its free RAM:
  // it read as too little even right after a full reset, and the page erased again and again)
  console.info('FlowCE: calculator', info);
  const needErase = info.freeFlash !== null && info.freeFlash < need;
  if (info.freeFlash !== null)
    lines.push(`Free memory: ${MB(info.freeFlash)}` + (needErase ? `; FlowCE needs ${MB(need)}, so the calculator's archive will be erased.` : ' (enough for FlowCE).'));
  state = { blocked, needErase };
  say('st-connect', (blocked ? '' : '<span class="ok">Connected.</span>') + `<ul>${lines.map(l => `<li>${l}</li>`).join('')}</ul>`);
  if (!blocked) {
    mark('connect', 'done');
    mark('install', 'active');
  }
  $('erase-part').hidden = !needErase;
  updateInstall();
  return state;
}

async function connect() {
  if (busy) return;
  say('st-connect', '');
  let c;
  try {
    c = await Calculator.request(); // must come first: the browser needs the click
  } catch (e) {
    if (e && e.name === 'NotFoundError') return; // the picker was closed
    say('st-connect', problem(e));
    return;
  }
  if (calc && calc !== c) await calc.close();
  calc = c;
  try {
    say('st-connect', 'Checking the calculator…');
    await loadBundle();
    await c.ping();
    await check(c);
  } catch (e) {
    say('st-connect', problem(e));
  }
}

// After a reset the calculator comes back by itself: keep trying the calculators this page may use,
// for up to ms or while wanted()
async function reconnect(ms, wanted = () => true) {
  const end = Date.now() + ms;
  while (Date.now() < end && wanted()) {
    const devs = (await navigator.usb.getDevices()).filter(d => d.vendorId === TI_CE.vendorId && d.productId === TI_CE.productId);
    for (const d of devs) {
      const c = new Calculator(d);
      try {
        await c.open();
        await c.ping(4000);
        return c;
      } catch (e) {
        await c.close();
      }
    }
    await sleep(1500);
  }
  return null;
}

// Reset the archive with remote keys, as a person would: quit, MEM, 7 Reset..., right (ARCHIVE),
// 3 Both..., 2 Reset (what INST asks for: apps and archived variables go, RAM stays). Then the
// calculator either answers again on the same connection (c), or restarts its USB, and the
// browser has to be given it again (null). Throws if nothing was erased.
async function erase(c) {
  for (const k of [KEY.quit, KEY.clear, KEY.mem, KEY.k7, KEY.right, KEY.k3]) {
    await c.pressKey(k);
    await sleep(800);
  }
  try {
    await c.pressKey(KEY.k2, 180000); // Reset: acknowledged, then the calculator erases
    await sleep(1500);
    await c.ping(180000); // answered once the erase is done (a timer for the slowest calculator)
    return c;
  } catch (e) {
    console.warn('FlowCE: the calculator left after the erase:', e);
    await c.close(); // cancels what is still waiting
    if (calc === c) calc = null;
    return null;
  }
}

// "text 0:42", every second, until the returned function is called
function ticking(id, text) {
  const t0 = Date.now();
  const show = () => {
    const s = Math.round((Date.now() - t0) / 1000);
    say(id, `${text} ${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`);
  };
  show();
  const t = setInterval(show, 1000);
  return () => clearInterval(t);
}

async function sendAll(c) {
  const total = vars.reduce((a, v) => a + v.data.length, 0);
  let done = 0;
  $('progress').hidden = false;
  for (let i = 0; i < vars.length; i++) {
    const v = vars[i];
    say('st-install', `Sending FlowCE: ${v.name} (${i + 1} of ${vars.length})…`);
    await c.sendVar(v, n => {
      $('bar').style.width = (100 * (done + n) / total).toFixed(1) + '%';
    });
    done += v.data.length;
  }
  $('bar').style.width = '100%';
}

// The rest of the install, once the calculator has room: send, open the arTIfiCE shell
async function proceed(c) {
  await sendAll(c);
  say('st-install', 'Opening the installer on the calculator…');
  for (const k of [KEY.clear, KEY.prgm, KEY.enter, KEY.enter]) {
    await c.pressKey(k);
    await sleep(700);
  }
  say('st-install', '<span class="ok">FlowCE is on the calculator.</span>');
  mark('install', 'done');
  mark('finish', 'active');
  waitingReset = true;
  // The shell and INST don't read the USB: a key sent now would wait, and land after INST, where it
  // could replace the reset INST leaves for the OS. So nothing more is sent.
  say('st-finish', autoInst
    ? '<strong>Press <kbd>enter</kbd> on the calculator</strong> (the arTIfiCE shell shows INST). That\'s the only key: ' +
      'it installs FlowCE, counting down from 42 to 0, and restarts by itself (<em>"RAM Cleared"</em>). Don\'t touch it meanwhile.'
    : 'On the calculator, press <kbd>enter</kbd> to start INST, then <kbd>enter</kbd> again to install. It counts down from 42 to 0. ' +
      'At <em>"Success! Will now reset"</em>, press <kbd>enter</kbd>, then <kbd>mode</kbd>.');
}

async function install() {
  if (busy || !calc || !state || state.blocked) return;
  busy = true;
  updateInstall();
  $('btn-connect').disabled = true;
  let c = calc;
  try {
    if (state.needErase) {
      const stop = ticking('st-install', 'Erasing the calculator. It shows <em>"Arc Vars &amp; Apps Cleared"</em> when it\'s done; nothing to press.');
      let c2;
      try {
        c2 = await erase(c);
      } finally {
        stop();
      }
      if (!c2) {
        // the calculator restarted its USB. It has no serial number, so the browser forgot it and has
        // to be asked again (a click): the one button left, green; keep looking meanwhile, in case
        // this browser does remember it
        awaitingReconnect = true;
        const b = $('btn-reconnect');
        $('btn-install').hidden = true;
        b.hidden = false;
        b.classList.add('attn');
        b.scrollIntoView({ block: 'center', behavior: 'smooth' });
        b.focus({ preventScroll: true });
        say('st-install', '<strong>Erased. Now click the green Reconnect calculator button</strong> and pick the calculator ' +
          'in the list: it restarted its USB connection, so the browser asks once more. Nothing to press on the calculator.');
        reconnect(180000, () => awaitingReconnect).then(c3 => { if (c3 && awaitingReconnect) resume(c3); });
        return; // busy until resume()
      }
      c = calc = c2;
      say('st-install', 'Erased. Checking the calculator…');
      await check(c);
      if (state.needErase) throw new Error('The calculator still doesn\'t have enough free memory. Erase it on the calculator (see below), then press Connect calculator again.');
    }
    await proceed(c);
  } catch (e) {
    say('st-install', problem(e));
  }
  done();
}

// After the erase: the calculator is back
async function resume(c) {
  if (!awaitingReconnect) return;
  awaitingReconnect = false;
  $('btn-reconnect').hidden = true;
  $('btn-reconnect').classList.remove('attn');
  $('btn-install').hidden = false;
  calc = c;
  try {
    say('st-install', 'Checking the calculator…');
    await c.ping();
    await check(c);
    if (state.needErase) throw new Error('The calculator still doesn\'t have enough free memory. Erase it on the calculator (see below), then press Connect calculator again.');
    await proceed(c);
  } catch (e) {
    say('st-install', problem(e));
  }
  done();
}

async function reconnectClick() {
  if (!awaitingReconnect) return;
  let c;
  try {
    c = await Calculator.request();
  } catch (e) {
    if (e && e.name === 'NotFoundError') return; // the picker was closed
    say('st-install', problem(e));
    return;
  }
  await resume(c);
}

function done() {
  busy = false;
  $('btn-connect').disabled = waitingReset;
  updateInstall();
}

function finished() {
  waitingReset = false;
  mark('finish', 'done');
  $('btn-connect').disabled = busy;
  say('st-finish', '<span class="ok">The calculator restarted.</span> When it shows <em>"RAM Cleared"</em>, FlowCE is installed: press <kbd>apps</kbd> → <strong>FlowCE</strong>.');
}

function init() {
  document.querySelectorAll('.step').forEach((li, i) => { li.dataset.n = String(i + 1); });
  mark('connect', 'active');
  fetch('files/version.txt').then(r => (r.ok ? r.text() : '')).then(t => { $('version').textContent = t.trim(); }).catch(() => {});

  if (!('usb' in navigator)) {
    $('no-usb').hidden = false;
    $('btn-connect').disabled = true;
    $('manual').open = true;
    return;
  }
  if (WINDOWS) $('win-note').hidden = false;
  $('btn-connect').addEventListener('click', connect);
  $('btn-install').addEventListener('click', install);
  $('btn-reconnect').addEventListener('click', reconnectClick);
  $('erase-ok').addEventListener('change', updateInstall);

  navigator.usb.addEventListener('disconnect', e => {
    if (!calc || e.device !== calc.dev) return;
    calc = null;
    if (waitingReset) finished();
    else if (!busy) say('st-connect', 'The calculator was disconnected. Plug it back in: this page picks it up again.');
    updateInstall();
  });
  // a calculator this page already used is plugged in again (or restarts): pick it up
  navigator.usb.addEventListener('connect', async () => {
    if (busy || calc || !state) return;
    const c = await reconnect(20000);
    if (!c || calc) return;
    calc = c;
    try {
      await check(c);
    } catch (err) {
      say('st-connect', problem(err));
    }
  });
  window.addEventListener('beforeunload', ev => {
    if (busy) { ev.preventDefault(); ev.returnValue = ''; }
  });
}

init();
