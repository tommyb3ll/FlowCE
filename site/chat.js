// The page's chat, beside the page: the button in the corner opens it (so does the top bar's Chat, or
// a link to #chat). Anyone picks a name and writes; everyone sees the last 100 messages, live.
// Stored in a Firebase Realtime Database (CONFIG: the project flowce-chat); its rules (chat-rules.json,
// pasted in Firebase's console) are the real limits: a name of 1-24 characters, a message of 1-300,
// no links, one message every 10 minutes per visitor, never edited, deleted by its author or by the
// page's owner in the console. Swear words and slurs are starred out where they show. A message 3
// visitors reported is hidden (chat/reports in the console: its count; delete it to show the message
// again), and so is one a visitor reported, for them. Reading needs no sign-in; a visitor is signed
// in anonymously (no account, nothing personal) when they first write or report, and stays so in
// this browser (their messages: deletable) until Firebase clears accounts unused for 30 days. The
// chat connects when it is first opened (the free plan: 100 connections at once). ?chatmock: a chat
// in this page's memory only (tests).
const CONFIG = { // (Project settings > Your apps > FlowCE site; public: the rules are the protection)
  apiKey: 'AIzaSyCA7l10IpiZ6Y8FWl9VYesYdQP_glLTz0A',
  authDomain: 'flowce-chat.firebaseapp.com',
  databaseURL: 'https://flowce-chat-default-rtdb.firebaseio.com',
  projectId: 'flowce-chat',
  appId: '1:712025051453:web:9b086cc6fc1aa009cff554',
};
const SDK = 'https://www.gstatic.com/firebasejs/10.12.2/';
const MAX = 100, NAME = 24, TEXT = 300, GAP = 600000, HIDE = 3, LINK = /(https?:|www\.)/i;
const $ = id => document.getElementById(id);
const MOCK = new URLSearchParams(location.search).has('chatmock');
const store = { // (none in some private windows: then only this visit remembers)
  get(k) { try { return localStorage.getItem(k); } catch (e) { return null; } },
  set(k, v) { try { localStorage.setItem(k, v); } catch (e) { /* */ } },
};

async function firebase() {
  const [{ initializeApp }, fa, db] = await Promise.all(
    ['firebase-app', 'firebase-auth', 'firebase-database'].map(m => import(SDK + m + '.js')));
  const app = initializeApp(CONFIG);
  const auth = fa.initializeAuth(app, { persistence: [fa.indexedDBLocalPersistence, fa.browserLocalPersistence] });
  await auth.authStateReady();
  let me = auth.currentUser ? auth.currentUser.uid : null; // (signed in on an earlier visit)
  const signIn = async () => { if (!me) me = (await fa.signInAnonymously(auth)).user.uid; };
  const d = db.getDatabase(app), chat = db.ref(d, 'chat');
  return {
    get me() { return me; },
    watch(add, drop, reports) {
      const q = db.query(db.ref(d, 'chat/messages'), db.orderByChild('t'), db.limitToLast(MAX));
      db.onChildAdded(q, s => add(s.key, s.val()));
      db.onChildRemoved(q, s => drop(s.key));
      db.onValue(db.ref(d, 'chat/reports'), s => reports(s.val() || {}));
    },
    async send(n, m) { // (the message and the time of one's last one, together: the rules check both)
      await signIn();
      const id = db.push(db.ref(d, 'chat/messages')).key, t = db.serverTimestamp();
      await db.update(chat, { [`messages/${id}`]: { n, m, u: me, t }, [`last/${me}`]: t });
    },
    remove(id) { return db.remove(db.ref(d, `chat/messages/${id}`)); },
    async report(id) { // (once per visitor; who reported stays private, only the count shows)
      await signIn();
      await db.update(chat, { [`reporters/${id}/${me}`]: true, [`reports/${id}`]: db.increment(1) });
    },
  };
}

function memory() {
  const subs = [], reports = {};
  let k = 0;
  return {
    me: 'me',
    watch(add, drop, rep) {
      subs.push([add, drop, rep]);
      add('s1', { n: 'Sam', m: 'Does it work on the TI-84 Plus CE Python?', u: 'sam', t: Date.now() - 7200000 });
    },
    async send(n, m) { const id = 'm' + ++k, v = { n, m, u: 'me', t: Date.now() }; subs.forEach(s => s[0](id, v)); },
    async remove(id) { subs.forEach(s => s[1](id)); },
    async report(id) { reports[id] = (reports[id] || 0) + 1; subs.forEach(s => s[2]({ ...reports })); },
  };
}

// swear words and slurs, starred out where they show (in rot13, to keep them out of the source): a *
// lets other letters stand before or after a word, letters may repeat ("fuuuck"), and 0 1 3 4 5 7 @ $
// (and ! before a letter) stand for letters
const rot13 = s => s.replace(/[a-z]/g, c => String.fromCharCode((c.charCodeAt(0) - 84) % 26 + 97));
const BAD = new RegExp(rot13('*shpx* *fuvg* *ovgpu* *nffubyr* nff qhzonff wnpxnff fznegnff sngnff nffung nefr ' +
  'nefrubyr onfgneq obyybpxf pbpx phz phag qvpx qvpxurnq qvyqb wvmm cbea* cvff* chffl chffvrf fyhg* gjng jnax* ' +
  'juber oybjwbo unaqwbo avtt* avtyrg snt sntt* ergneq ergneqrq genaal genaavrf xvxr fcvp puvax pbba tbbx jrgonpx ' +
  'ornare qlxr gbjryurnq enturnq wvtnobb xlf').split(' ').map(w => (w[0] === '*' ? '[a-z]*' : '\\b')
  + w.replace(/\*/g, '').replace(/[a-z]/g, '$&+') + (w.endsWith('*') ? '[a-z]*' : '(?:e?s)?\\b')).join('|'), 'g');
const LEET = { 0: 'o', 1: 'i', 3: 'e', 4: 'a', 5: 's', 7: 't', '@': 'a', $: 's' };
function clean(s) {
  let low = '';
  for (let i = 0; i < s.length; i++) { // (one character for one: the matches' places are the text's)
    const c = s[i], l = c.toLowerCase();
    low += LEET[c] || (c === '!' && /[a-z]/i.test(s[i + 1] || '') ? 'i' : l.length === 1 ? l : c);
  }
  let out = '', at = 0;
  for (const m of low.matchAll(BAD)) {
    out += s.slice(at, m.index) + s[m.index] + '*'.repeat(m[0].length - 1);
    at = m.index + m[0].length;
  }
  return out + s.slice(at);
}

const ago = t => {
  const s = (Date.now() - t) / 1000;
  return s < 60 ? 'just now' : s < 3600 ? `${Math.floor(s / 60)} min ago` : s < 86400 ? `${Math.floor(s / 3600)} h ago`
    : new Date(t).toLocaleDateString();
};

function count(what) { // (the page's visitor counter, GoatCounter: index.html)
  try { if (window.goatcounter && window.goatcounter.count) window.goatcounter.count({ path: what, title: what, event: true }); } catch (e) { /* */ }
}

async function start(panel, opener) {
  const log = $('chat-log'), form = $('chat-form'), name = $('chat-name'), text = $('chat-text'), status = $('chat-status');
  const say = (s, err) => { status.textContent = s; status.classList.toggle('err', !!err); };
  let chat, counts = {}, reported = [];
  try { reported = JSON.parse(store.get('flowce-chat-reported')) || []; } catch (e) { /* */ }
  const hidden = id => counts[id] >= HIDE || reported.includes(id);
  const items = new Map();
  const add = (id, v) => { // (a message sent from this page shows at once, before the database has it)
    if (!v || typeof v.n !== 'string' || typeof v.m !== 'string' || items.has(id)) return;
    const mine = v.u === chat.me, atEnd = log.scrollTop + log.clientHeight >= log.scrollHeight - 8;
    const li = document.createElement('li'), head = document.createElement('div');
    const who = document.createElement('strong'), when = document.createElement('span'), body = document.createElement('p');
    const act = document.createElement('button');
    li.className = 'msg' + (mine ? ' mine' : '');
    li.hidden = hidden(id);
    head.className = 'msg-head';
    who.textContent = clean(v.n);
    when.className = 'msg-time';
    when.dataset.t = String(v.t);
    when.textContent = ago(v.t);
    act.type = 'button';
    if (mine) { // one's own: deletable
      act.className = 'msg-act msg-del'; act.title = 'Delete your message'; act.textContent = '×';
      act.addEventListener('click', () => chat.remove(id).catch(() => say('Not deleted.', true)));
    } else { // anyone else's: reportable
      act.className = 'msg-act msg-flag'; act.title = 'Report this message'; act.setAttribute('aria-label', 'Report this message');
      act.addEventListener('click', () => report(id));
    }
    head.append(who, when, act);
    body.textContent = clean(v.m);
    li.append(head, body);
    log.append(li);
    items.set(id, li);
    if (atEnd || mine) log.scrollTop = log.scrollHeight;
    if (panel.hidden && !li.hidden) opener.classList.add('unread');
  };
  const drop = id => { // (deleted, or refused by the database)
    const li = items.get(id);
    if (li) { li.remove(); items.delete(id); }
  };
  const recount = c => {
    counts = c;
    items.forEach((li, id) => { li.hidden = hidden(id); });
  };
  const report = async id => {
    if (!reported.includes(id)) reported = [...reported, id].slice(-200);
    store.set('flowce-chat-reported', JSON.stringify(reported));
    recount(counts);
    say('Reported, and hidden for you. Thank you.');
    try {
      await chat.report(id);
      count('chat-report');
    } catch (e) { /* (reported before, or already deleted) */ }
  };
  try {
    chat = MOCK ? memory() : await firebase();
    chat.watch(add, drop, recount);
  } catch (e) {
    say('The chat could not connect. Reload to try again.', true);
    return;
  }

  // one message every 10 minutes: the time left shows in the empty message box
  let last = 0, tick = 0;
  const left = () => Math.max(0, Math.max(last, Number(store.get('flowce-chat-last')) || 0) + GAP - Date.now());
  const wait = () => {
    const w = left();
    text.placeholder = w > 0 ? `Your next message in ${Math.ceil(w / 60000)} min` : 'Say something';
    clearTimeout(tick);
    if (w > 0) tick = setTimeout(wait, (w - 1) % 60000 + 1);
  };
  wait();
  setInterval(() => log.querySelectorAll('[data-t]').forEach(s => { s.textContent = ago(Number(s.dataset.t)); }), 60000);

  name.value = store.get('flowce-chat-name') || '';
  form.addEventListener('submit', async e => {
    e.preventDefault();
    const n = name.value.trim().slice(0, NAME), m = text.value.trim().slice(0, TEXT);
    if (!n) { name.focus(); return; }
    if (!m) { text.focus(); return; }
    if (LINK.test(n) || LINK.test(m)) { say('Links aren\'t allowed in the chat.', true); return; }
    const w = left();
    if (w > 0) { say(`One message every 10 minutes: your next one in ${Math.ceil(w / 60000)} min.`, true); return; }
    store.set('flowce-chat-name', n);
    say('');
    text.value = '';
    last = Date.now();
    store.set('flowce-chat-last', String(last));
    wait();
    try {
      await chat.send(n, m);
      count('chat-message');
    } catch (err) {
      text.value = m;
      if (err && err.code === 'PERMISSION_DENIED') { say('Not sent: one message every 10 minutes.', true); return; }
      last = 0; // (not signed in: try again)
      store.set('flowce-chat-last', '0');
      wait();
      say('Not sent. Try again in a moment.', true);
    }
  });
  form.querySelectorAll('input, button').forEach(el => { el.disabled = false; });
  if (matchMedia('(pointer: fine)').matches) (name.value ? text : name).focus(); // (a phone's keyboard: when tapped)
}

function init() {
  const panel = $('chat'), opener = $('chat-open');
  if (!panel || !opener || (!CONFIG && !MOCK)) return; // (no database: no chat)
  let started = false;
  const open = () => {
    panel.hidden = false;
    opener.hidden = true;
    opener.classList.remove('unread');
    const log = $('chat-log');
    log.scrollTop = log.scrollHeight;
    if (!started) { started = true; start(panel, opener); count('chat-open'); return; }
    const box = $('chat-name').value ? $('chat-text') : $('chat-name');
    if (!box.disabled && matchMedia('(pointer: fine)').matches) box.focus();
  };
  const close = () => {
    panel.hidden = true;
    opener.hidden = false;
    opener.focus();
  };
  opener.hidden = false;
  opener.addEventListener('click', open);
  $('chat-close').addEventListener('click', close);
  panel.addEventListener('keydown', e => { if (e.key === 'Escape') close(); });
  document.querySelectorAll('a[href="#chat"]').forEach(a => {
    a.hidden = false;
    a.addEventListener('click', e => { e.preventDefault(); open(); });
  });
  if (location.hash === '#chat') open();
}

init();
