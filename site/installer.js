// The FlowCE web installer: checks the calculator, erases it (after the user agrees), sends the
// version picked (files/FlowCE.b84, or files/KhiCAS-Flow.b84 for KhiCAS + Flow) over WebUSB
// and opens its installer, which then needs one key press on the calculator. Steps and texts:
// index.html.
import { Calculator, KEY, LinkError, TI_CE, readBundle, readTIFile, readAppFile } from './dusb.js';

const $ = id => document.getElementById(id);
const esc = s => String(s).replace(/[<>&]/g, c => ({ '<': '&lt;', '>': '&gt;', '&': '&amp;' }[c]));
const step = name => document.querySelector(`.step[data-step="${name}"]`);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const MB = n => (n / 1e6).toFixed(2) + ' MB';
const V2_MAX = [5, 8, 4], V3_OS = [5, 8, 5], OS_MIN = [5, 3, 0];
const INEQUALZ_SHA = '1899cf87281941bde537c184a92dd9435ff0aff7bca0899da25c654ed4d9fdca';
const V3_FILES = [
  ['arTIfiCE.8xp', '98cbadab42f34d63542d3648f97ff4ad20527ff39c35df12bbc00842454d262b', 'PPPP'],
  ['INEQUVAR.8xv', 'cc47cdda6fef69590a5377c095443e1cda911454ef68c173f0de5d17be2a0000', 'INEQUVAR'],
];
// Windows binds the calculator to TI's driver: the old one (tiehdusb) can't be used by browsers
const WINDOWS = /Windows/i.test(navigator.userAgentData ? navigator.userAgentData.platform : navigator.userAgent);

// The versions the page installs, with their version texts (site/build.sh puts them in files/)
const BUILDS = { // (app: its name in the calculator's apps menu)
  normal: { file: 'FlowCE.b84', ver: 'version.txt', name: 'FlowCE', app: 'FlowCE' },
  khicas: { file: 'KhiCAS-Flow.b84', ver: 'version-khicas.txt', name: 'KhiCAS + Flow', app: 'KhiFlow' },
};
const versions = {};      // their version texts once read ('' if the site doesn't have it)
let build = 'normal';     // the version picked

let calc = null;          // the connected calculator
let vars = null;          // the bundle's variables, in sending order
let varsOf = null;        // the version they are from
let autoInst = false;     // INST is patched (below): one key starts it, and it restarts the calculator
let state = null;         // the last check: { blocked, needErase }
let busy = false;         // installing
let waitingReset = false; // the installer runs on the calculator: its restart ends the install
let awaitingReconnect = false; // erased: waiting for the calculator to come back
let inequalz = null;          // verified TI app supplied by the site's build
let v3Files = null;
let v3Stage = null;           // ram-reset -> inequalz -> asmhook -> native INST
let v3Device = null;          // reuse permission when the same device stays connected

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

// a step of an install, counted on the page's visitor counter (GoatCounter, index.html), with the version
// picked: connect, start, sent (the files are on the calculator), done (Finished), error, download (the
// TI Connect link). Blocked or absent: nothing.
function count(what) {
  try {
    if (window.goatcounter && window.goatcounter.count)
      window.goatcounter.count({ path: `${what}-${build}`, title: `${what} (${BUILDS[build].name})`, event: true });
  } catch (e) { /* (counting never stops an install) */ }
}

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

function installRoute(os) {
  if (!os || os.length < 3 || !os.slice(0, 3).every(Number.isInteger) || cmp(os, OS_MIN) < 0) return null;
  if (cmp(os, V3_OS) === 0) return 'v3';
  return cmp(os, V2_MAX) <= 0 ? 'v2' : null;
}

async function digest(bytes) {
  return Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', bytes)), b => b.toString(16).padStart(2, '0')).join('');
}

async function loadV3() {
  if (v3Files) return v3Files;
  const loaded = [];
  for (const [file, hash, name] of V3_FILES) {
    const res = await fetch('artifice-v3/' + file);
    if (!res.ok) throw new Error('Could not download arTIfiCE v3. Try again.');
    const bytes = new Uint8Array(await res.arrayBuffer());
    if (await digest(bytes) !== hash) throw new Error('The arTIfiCE v3 download is damaged. Reload the page.');
    const vars = readTIFile(bytes);
    if (vars.length !== 1 || vars[0].name !== name) throw new Error('Unexpected arTIfiCE v3 file.');
    loaded.push({ ...vars[0], archived: false });
  }
  const res = await fetch('artifice-v3/Inequalz.8ek');
  if (!res.ok) throw new Error('Could not load the installer files. Try again.');
  const bytes = new Uint8Array(await res.arrayBuffer());
  if (bytes.length !== 44811 || await digest(bytes) !== INEQUALZ_SHA)
    throw new Error('The installer download is damaged. Reload the page and try again.');
  const app = readAppFile(bytes);
  if (app.name !== 'Inequalz') throw new Error('Unexpected installer prerequisite.');
  inequalz = app;
  v3Files = loaded;
  return loaded;
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
  if (e && e.code === 'calc' && v3Stage)
    return 'The calculator refused a file. Keep AsmHook2 installed. Use the TI Connect CE instructions below to finish, or reload to restart the setup.';
  if (e && e.code === 'calc')
    return `The calculator refused a file. Usually part of ${BUILDS[build].name} is already on it: erase it, then try again.`;
  if (e && e.code === 'short')
    return 'The calculator still doesn\'t have enough free memory: some apps stay after the erase (on some calculators, ' +
      'TI\'s language apps). Delete them in TI Connect CE (select them in the calculator\'s file list, then Delete), or on ' +
      'the calculator: <kbd>2nd</kbd> <kbd>+</kbd> (MEM) → 2: Mem Mgmt/Del… → Apps…, then <kbd>del</kbd> on each. ' +
      'Then press Connect calculator again.';
  if (e && e.name === 'NetworkError')
    return 'The calculator was disconnected. Plug it back in and press Connect calculator again.';
  if (e && e.name === 'SecurityError')
    return 'The browser blocked the connection to the calculator.';
  return msg;
}

// The message, and the browser's own words under it (for reports)
function problem(e) {
  count('error');
  if (WINDOWS && e && e.code === 'claim') $('win-note').hidden = false;
  const raw = esc(e && e.message || e);
  const f = friendly(e);
  return `<span class="bad">${f}</span>` + (f === raw || (e && e.code === 'short') ? '' : `<div class="fine">Details: ${raw}</div>`);
}

async function loadBundle() {
  const b = build;
  if (vars && varsOf === b) return vars;
  const res = await fetch('files/' + BUILDS[b].file, { cache: 'no-store' });
  if (!res.ok) throw new Error(`could not download ${BUILDS[b].name} (${res.status})`);
  const files = await readBundle(await res.arrayBuffer());
  const list = [];
  for (const f of files) {
    if (/\.8x[pv]$/i.test(f.name)) list.push(...readTIFile(f.bytes));
  }
  // the programs (arTIfiCE "A", INST) first, then AppIns00, AppIns01... in order
  list.sort((a, b) => (a.name.startsWith('AppIns') - b.name.startsWith('AppIns')) || a.name.localeCompare(b.name));
  const inst = list.find(v => v.name === 'INST');
  if (list.filter(v => v.name.startsWith('AppIns')).length < 2 || !inst)
    throw new Error(`the ${BUILDS[build].name} download is incomplete`);
  const patched = patchInst(inst.data);
  if (patched) inst.data = patched;
  autoInst = !!patched;
  vars = list;
  varsOf = b;
  return vars;
}

// INST counts down from the last AppIns' number (42 for 43 of them) to 0
const countdown = () => (vars ? vars.filter(v => v.name.startsWith('AppIns')).length - 1 : 42);

// The version picked, in the texts that name it: the install button, "Sends ...", the download
function showBuild() {
  const b = BUILDS[build];
  $('btn-install').textContent = 'Install ' + b.name;
  $('version').textContent = (b.name + ' ' + (versions[build] || '')).trim();
  const dl = $('dl');
  dl.href = 'files/' + b.file;
  dl.textContent = b.file;
  dl.setAttribute('download', b.file);
  document.querySelectorAll('.app').forEach(e => { e.textContent = b.app; }); // (its name in the apps menu)
  document.querySelectorAll('.opt').forEach(o => o.classList.toggle('on', o.querySelector('input').checked));
}

// Another version picked: if a calculator is connected, check it again (the memory it needs)
async function choose(b) {
  if (b === build) return;
  if (busy || waitingReset || v3Stage) { // not during an install: the radios are disabled then anyway
    document.querySelector(`input[name="build"][value="${build}"]`).checked = true;
    return;
  }
  build = b;
  showBuild();
  if (!calc) return;
  try {
    say('st-connect', 'Checking the calculator…');
    await loadBundle();
    await check(calc);
  } catch (e) {
    say('st-connect', problem(e));
  }
}

// (nothing to click while INST runs: whatever reaches the calculator then could cancel its reset)
function updateInstall() {
  const locked = busy || waitingReset || !!v3Stage;
  $('btn-install').disabled = locked || !calc || !state || state.blocked ||
    ((state.needErase || state.route === 'v3') && !$('erase-ok').checked);
  document.querySelectorAll('input[name="build"]').forEach(r => { r.disabled = locked || r.dataset.off === '1'; });
  $('btn-v3-next').disabled = busy;
}

// The calculator's OS and free memory: what the install has to do
async function check(c = calc) {
  const info = await c.info();
  const route = installRoute(info.os), blocked = !route;
  // v3 temporarily needs Inequalz and AsmHook2 alongside the AppIns files.
  const need = vars.filter(v => v.archived).reduce((a, v) => a + v.data.length + 32, 0) + (route === 'v3' ? 96 * 1024 : 0);
  const lines = [`${info.product}, OS ${info.osText}`];
  if (route === 'v3') lines.push('Installer selected for OS 5.8.5. Click Install; the page gets the files for you.');
  else if (route === 'v2') lines.push('Using arTIfiCE v2.1 for this OS.');
  else lines.push('<span class="bad">This OS version could not be verified.</span> No installer will be sent. See <a href="#os-help">OS help</a>.');
  $('finish-intro').hidden = route === 'v3';
  $('install-gif').hidden = route === 'v3';
  // only the free archive decides (the RAM figure a real calculator reports is not its free RAM:
  // it read as too little even right after a full reset, and the page erased again and again)
  console.info('FlowCE: calculator', info);
  const needErase = info.freeFlash !== null && info.freeFlash < need;
  if (info.freeFlash !== null)
    lines.push(`Free memory: ${MB(info.freeFlash)}` + (needErase ? `; ${BUILDS[build].name} needs ${MB(need)}, so the calculator's archive will be erased.` : ` (enough for ${BUILDS[build].name}).`));
  state = { blocked, needErase, route };
  say('st-connect', (blocked ? '' : '<span class="ok">Connected.</span>') + `<ul>${lines.map(l => `<li>${l}</li>`).join('')}</ul>`);
  if (!blocked) {
    mark('connect', 'done');
    mark('install', 'active');
  }
  $('erase-part').hidden = !(needErase || route === 'v3');
  $('erase-label').textContent = route === 'v3'
    ? 'I backed up my calculator and allow its RAM, apps and archived files to be erased'
    : 'Erase all apps and archived files on my calculator';
  updateInstall();
  return state;
}

async function connect() {
  if (busy || v3Stage || waitingReset) return;
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
    count('connect');
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

// Still short after the erase: the reset kept apps (TI's language apps on a friend's calculator, OS
// 5.3.1, 2026-10-07): the message says how to delete them (branch installer-dusb deletes them over
// USB, as TI Connect CE does: waiting for a test on a real calculator)
const stillShort = () => new LinkError('short', 'not enough free memory after the erase');

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
  const list = state.route === 'v3'
    ? vars.filter(v => v.name.startsWith('AppIns') || v.name === 'INST')
      .map(v => ({ ...v, archived: v.name !== 'INST' }))
    : vars;
  const total = list.reduce((a, v) => a + v.data.length, 0);
  let done = 0;
  $('progress').hidden = false;
  for (let i = 0; i < list.length; i++) {
    const v = list[i];
    say('st-install', `Sending ${BUILDS[build].name}: ${v.name} (${i + 1} of ${list.length})…`);
    await c.sendVar(v, n => {
      $('bar').style.width = (100 * (done + n) / total).toFixed(1) + '%';
    });
    done += v.data.length;
  }
  $('bar').style.width = '100%';
}

async function release(c) {
  if (state && state.route === 'v3') v3Device = c.dev;
  if (calc === c) calc = null;
  await c.close();
}

function v3Prompt(stage, html, label) {
  v3Stage = stage;
  $('v3-guidance').hidden = false;
  say('st-install', 'Complete the calculator step below.');
  say('st-v3', html);
  $('btn-v3-next').textContent = label;
  updateInstall();
}

async function prepareV3(c) {
  await loadV3(); // all downloads are verified before changing the calculator
  say('st-install', 'Sending TI Inequalz…');
  await c.sendVar(inequalz);
  v3Stage = 'ram-reset';
  say('st-install', 'Preparing the calculator. It may show “RAM Cleared”…');
  let ready = c;
  try {
    for (const k of [KEY.quit, KEY.clear, KEY.mem, KEY.k7, KEY.k1]) {
      await c.pressKey(k); await sleep(700);
    }
    await c.pressKey(KEY.k2, 180000); await sleep(1500); await c.ping(10000);
  } catch (e) { await release(c); ready = null; }
  if (!ready) {
    v3Prompt('ram-reset', 'Check the calculator shows <strong>RAM Cleared</strong>, then reconnect below. ' +
      'Press <kbd>on</kbd> if needed. <details><summary>RAM Cleared never appeared?</summary>' +
      'Reset RAM: <kbd>2nd</kbd> <kbd>+</kbd> → <kbd>7</kbd> → <kbd>1</kbd> → <kbd>2</kbd>, then reconnect.</details>', 'RAM cleared — reconnect');
    return;
  }
  await openV3Screen(ready);
}

async function openV3Screen(c) {
  await c.pressKey(KEY.clear);
  await c.sendVar(v3Files[0]);
  await release(c);
  v3Prompt('inequalz', '<kbd>apps</kbd> → <strong>Inequalz</strong> → <kbd>enter</kbd>. Stay on its ' +
    '<strong>Functions (Y=) screen</strong>.', 'Ready — continue');
}

async function continueV3() {
  if (busy || !v3Stage || waitingReset) return;
  const stage = v3Stage;
  busy = true; updateInstall();
  let c;
  try {
    const remembered = await navigator.usb.getDevices();
    if (v3Device && remembered.includes(v3Device)) {
      c = new Calculator(v3Device); await c.open();
    } else {
      c = await Calculator.request(); // reset may require the browser's device picker again
    }
  } catch (e) {
    done();
    if (e && e.name === 'NotFoundError') return;
    say('st-install', problem(e)); return;
  }
  try {
    calc = c;
    await c.ping();
    // Never continue a prepared session on a different OS, or erase the newly installed hook.
    const info = await c.info();
    if (installRoute(info.os) !== 'v3') throw new Error('This session needs the OS 5.8.5 calculator. Reload to start with a different OS.');
    if (stage === 'ram-reset') {
      await openV3Screen(c);
    } else if (stage === 'inequalz') {
      say('st-install', 'Sending the arTIfiCE v3 trigger…');
      await c.sendVar(v3Files[1]);
      await release(c);
      v3Prompt('asmhook', 'The calculator restarts. Press <kbd>on</kbd> if the screen is black. ' +
        '<kbd>apps</kbd> → <strong>AsmHook2</strong>, then dismiss “Installation successful!” and return home. ' +
        'Keep AsmHook2 installed; do not reset RAM again.', 'AsmHook2 enabled — continue');
    } else if (stage === 'asmhook') {
      const need = vars.filter(v => v.name.startsWith('AppIns')).reduce((a, v) => a + v.data.length + 32, 0);
      if (info.freeFlash === null || info.freeFlash < need)
        throw new Error('Not enough archive space. Delete other apps while keeping AsmHook2, then try this button again. Do not reset RAM.');
      await sendAll(c);
      waitingReset = true;
      mark('install', 'done'); mark('finish', 'active');
      $('v3-guidance').hidden = true;
      $('btn-finished').hidden = false;
      say('st-install', '<span class="ok">Files sent.</span> Finish on the calculator below.');
      count('sent');
      say('st-finish', '<kbd>prgm</kbd> → select <strong>INST by name</strong> → <kbd>enter</kbd> → <kbd>enter</kbd>. ' +
        `At “enter: install app”, press <kbd>enter</kbd>. Wait for ${countdown()} to count down to 0. ` +
        'At “Success! Will now reset”, press <kbd>enter</kbd>. After “RAM Cleared”, <kbd>clear</kbd>, ' +
        `then <kbd>apps</kbd> → <strong>${BUILDS[build].app}</strong>.`);
      if (autoInst) {
        say('st-finish', 'Installing automatically. Leave the calculator alone until “RAM Cleared”. ' +
          `Then <kbd>clear</kbd>, <kbd>apps</kbd> → <strong>${BUILDS[build].app}</strong>.`);
        try {
          for (const k of [KEY.clear, KEY.prgm, KEY.enter, KEY.enter]) {
            await c.pressKey(k); await sleep(700);
          }
        } catch (e) {
          say('st-finish', 'Check the calculator. If the countdown started, leave it alone until “RAM Cleared”. ' +
            'Otherwise <kbd>prgm</kbd> → <strong>INST</strong> → <kbd>enter</kbd> → <kbd>enter</kbd> starts it.');
        }
      }
      await release(c);
    }
  } catch (e) {
    say('st-install', problem(e) + (stage === 'inequalz'
      ? '<p>If the calculator restarted and AsmHook2 appears in Apps, enable it and use <a href="#manual-585">the manual finish instructions</a>.</p>' : ''));
    if (c) await release(c);
  } finally { done(); }
}

// The rest of the install, once the calculator has room: send, open the arTIfiCE shell
async function proceed(c) {
  if (state.blocked) throw new Error('The detected OS has no verified installer.');
  if (state.route === 'v3') return prepareV3(c);
  await sendAll(c);
  say('st-install', 'Opening the installer on the calculator…');
  for (const k of [KEY.clear, KEY.prgm, KEY.enter, KEY.enter]) {
    await c.pressKey(k);
    await sleep(700);
  }
  say('st-install', `<span class="ok">${BUILDS[build].name} is on the calculator.</span>`);
  count('sent');
  mark('install', 'done');
  mark('finish', 'active');
  waitingReset = true;
  $('btn-finished').hidden = false;
  // The shell and INST don't read the USB: a key sent now would wait, and land after INST, where it
  // could replace the reset INST leaves for the OS. So nothing more is sent.
  say('st-finish', autoInst
    ? '<strong>Press <kbd>enter</kbd> on the calculator</strong> (the arTIfiCE shell shows INST). That\'s the only key: ' +
      `it installs ${BUILDS[build].name}, counting down from ${countdown()} to 0, and restarts by itself (<em>"RAM Cleared"</em>). Don't touch it meanwhile.`
    : `On the calculator, press <kbd>enter</kbd> to start INST, then <kbd>enter</kbd> again to install. It counts down from ${countdown()} to 0. ` +
      'At <em>"Success! Will now reset"</em>, press <kbd>enter</kbd>, then <kbd>mode</kbd>.');
}

async function install() {
  if (busy || !calc || !state || state.blocked) return;
  if (v3Stage || waitingReset) return;
  if ((state.needErase || state.route === 'v3') && !$('erase-ok').checked) return;
  count('start');
  busy = true;
  updateInstall();
  $('btn-connect').disabled = true;
  let c = calc;
  try {
    if (state.route === 'v3') await loadV3(); // fail before an archive erase if files are unavailable
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
      if (state.blocked) throw new Error('The calculator OS changed. Reload before installing.');
      if (state.needErase) throw stillShort();
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
    if (state.blocked) throw new Error('The calculator OS changed. Reload before installing.');
    if (state.needErase) throw stillShort();
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
  $('btn-connect').disabled = waitingReset || !!v3Stage;
  updateInstall();
}

function finished() {
  waitingReset = false;
  v3Stage = null;
  $('btn-finished').hidden = true;
  mark('finish', 'done');
  count('done');
  $('btn-connect').disabled = busy;
  say('st-finish', `<span class="ok">${BUILDS[build].name} is ready.</span>`);
  updateInstall();
}

function init() {
  document.querySelectorAll('.step').forEach((li, i) => { li.dataset.n = String(i + 1); });
  mark('connect', 'active');

  // the version: picked here, or by a link (?build=khicas; ?build=python, FlowCE Python's links: its
  // successor, KhiCAS + Flow; ?build=steps, from the betas: FlowCE); one this site doesn't have is off
  const radios = document.querySelectorAll('input[name="build"]');
  const asked = new URLSearchParams(location.search).get('build');
  const want = asked === 'python' ? 'khicas' : asked;
  if (Object.prototype.hasOwnProperty.call(BUILDS, want)) build = want;
  radios.forEach(r => {
    r.checked = r.value === build;
    r.addEventListener('change', () => { if (r.checked) choose(r.value); });
  });
  for (const k of Object.keys(BUILDS)) {
    fetch('files/' + BUILDS[k].ver).then(r => (r.ok ? r.text() : '')).catch(() => '').then(t => {
      versions[k] = t.trim();
      document.querySelectorAll(`[data-ver="${k}"]`).forEach(e => { e.textContent = versions[k]; });
      if (!versions[k] && k !== 'normal') {
        const r = document.querySelector(`input[name="build"][value="${k}"]`);
        r.dataset.off = '1';
        r.closest('.opt').classList.add('off');
        if (build === k) {
          build = 'normal';
          document.querySelector('input[name="build"][value="normal"]').checked = true;
        }
      }
      updateInstall();
      showBuild();
    });
  }
  showBuild();

  if (!('usb' in navigator)) {
    $('no-usb').hidden = false;
    $('btn-connect').disabled = true;
    $('manual').open = true;
    return;
  }
  $('btn-connect').addEventListener('click', connect);
  $('dl').addEventListener('click', () => count('download'));
  $('btn-install').addEventListener('click', install);
  $('btn-reconnect').addEventListener('click', reconnectClick);
  $('btn-v3-next').addEventListener('click', continueV3);
  $('btn-finished').addEventListener('click', finished);
  $('erase-ok').addEventListener('change', updateInstall);

  navigator.usb.addEventListener('disconnect', e => {
    if (!calc || e.device !== calc.dev) return;
    calc = null;
    if (waitingReset) say('st-finish', 'The USB connection ended. Check the calculator: after “RAM Cleared”, ' +
      `press <kbd>clear</kbd>, then <kbd>apps</kbd> → <strong>${BUILDS[build].app}</strong>. If INST has not finished, ` +
      'follow its screen. A disconnected cable alone does not mean the installation finished.');
    else if (v3Stage) say('st-install', 'Connection ended. Complete the calculator step below, then use its Continue button.');
    else if (!busy) say('st-connect', 'The calculator was disconnected. Plug it back in: this page picks it up again.');
    updateInstall();
  });
  // a calculator this page already used is plugged in again (or restarts): pick it up
  navigator.usb.addEventListener('connect', async () => {
    if (busy || calc || !state || v3Stage || waitingReset || awaitingReconnect) return;
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
