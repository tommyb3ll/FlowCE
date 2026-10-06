// The FlowCE web installer: checks the calculator, erases it (after the user agrees), sends the
// release bundle (files/FlowCE.b84) over WebUSB and starts its installer. Steps and texts: index.html.
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
let state = null;         // the last check: { blocked, needErase }
let busy = false;         // installing
let waitingReset = false; // the installer runs on the calculator: its restart ends the install
let awaitingReconnect = false; // erased: waiting for the calculator to come back

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
  if (list.filter(v => v.name.startsWith('AppIns')).length < 2 || !list.some(v => v.name === 'INST'))
    throw new Error('the FlowCE download is incomplete');
  vars = list;
  return vars;
}

function updateInstall() {
  $('btn-install').disabled = busy || !calc || !state || state.blocked || (state.needErase && !$('erase-ok').checked);
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
  const needErase = info.freeFlash !== null && info.freeFlash < need;
  if (info.freeFlash !== null)
    lines.push(`Free memory: ${MB(info.freeFlash)}` + (needErase ? `; FlowCE needs ${MB(need)}, so the calculator will be erased.` : ' (enough for FlowCE).'));
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

// After a reset the calculator comes back by itself (sometimes only once its message is dismissed):
// keep trying the calculators this page may use, for up to ms
async function reconnect(ms) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
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

// Reset All Memory with remote keys, as a person would: quit, MEM, 7 Reset..., right right (ALL),
// 1 All Memory..., 2 Reset. The calculator restarts.
async function erase(c) {
  const keys = [KEY.quit, KEY.clear, KEY.mem, KEY.k7, KEY.right, KEY.right, KEY.k1, KEY.k2];
  for (let i = 0; i < keys.length; i++) {
    try {
      await c.pressKey(keys[i]);
    } catch (e) {
      if (i < keys.length - 1) throw e; // the last key resets the calculator: losing it there is expected
    }
    await sleep(800);
  }
  await c.close();
  if (calc === c) calc = null;
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

// INST runs inside the arTIfiCE shell, which may not take keys from the computer: try, briefly
async function startInst(c) {
  try {
    await sleep(5000); // the shell loads
    await c.pressKey(KEY.enter, 4000); // INST, the shell's only program
    await sleep(3000);
    await c.pressKey(KEY.enter, 4000); // "enter: install app"
    return true;
  } catch (e) {
    return false;
  }
}

// The rest of the install, once the calculator has room: send, open arTIfiCE, try to start INST
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
  say('st-finish', 'Starting the installer…');
  const auto = await Promise.race([startInst(c), sleep(16000).then(() => false)]);
  say('st-finish', (auto
    ? 'The installer is running: it counts down from 42 to 0.'
    : 'On the calculator, press <kbd>enter</kbd> to start INST, then <kbd>enter</kbd> again to install. It counts down from 42 to 0.') +
    ' At <em>"Success! Will now reset"</em>, press <kbd>enter</kbd>, then <kbd>mode</kbd>.');
}

async function install() {
  if (busy || !calc || !state || state.blocked) return;
  busy = true;
  updateInstall();
  $('btn-connect').disabled = true;
  const c = calc;
  try {
    if (state.needErase) {
      say('st-install', 'Erasing the calculator…');
      await erase(c);
      // the calculator has no USB serial number: the browser forgets it when it restarts and has
      // to be asked again (a click); keep looking meanwhile, in case this browser does remember it
      awaitingReconnect = true;
      $('btn-reconnect').hidden = false;
      say('st-install', 'The calculator is restarting. When it shows <em>"Memory cleared"</em>, press <kbd>enter</kbd> on it, ' +
        'then click <strong>Reconnect calculator</strong> and pick it again (the browser asks once more after a restart).');
      reconnect(120000).then(c2 => { if (c2 && awaitingReconnect) resume(c2); });
      return; // busy until resume()
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
  $('btn-connect').disabled = false;
  updateInstall();
}

function finished() {
  waitingReset = false;
  mark('finish', 'done');
  say('st-finish', '<span class="ok">The calculator restarted: FlowCE is installed.</span> Press <kbd>apps</kbd> → <strong>FlowCE</strong>.');
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
