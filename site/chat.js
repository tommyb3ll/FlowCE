// The page's live chat with the other people on it, beside the page: the button in the corner (the
// last few who wrote, how many are in the chat now) opens it; so do the top bar's Live chat and links
// to #chat. Anyone picks a name and writes; everyone sees the last 100 messages, live. Stored in a
// Firebase Realtime Database (CONFIG: the project flowce-chat); its rules (chat-rules.json, pasted in
// Firebase's console) are the real limits: a name of 1-24 characters, a message of 1-300, no links,
// no spam (2 s between one's messages, 8 a minute, not the same text twice in a minute; no wait to see
// otherwise), never edited, deleted by its author or by the creator. Swear words and slurs are starred
// out where they show. A message 3 visitors reported is hidden (chat/reports in the console: its count;
// delete it to show the message again), and so is one a visitor reported, for them. A visitor is
// signed in anonymously (no account, nothing personal) when they open the chat, to be counted in it and
// to write, and stays so in this browser (their messages: deletable) until Firebase clears accounts
// unused for 30 days. The creator (Tommy Bell) signs in with Google from the page opened with ?creator:
// the rules know his account; his messages are marked (TommyB3LL in gold, a Creator badge), have no
// limits and cannot be reported, he can delete any message, and no one else can take his name. Before
// it is opened, the button reads who wrote last and how many are in it once (no connection kept: the
// free plan allows 100 at once). ?chatmock: a chat in this page's memory only (tests).
const CONFIG = { // (Project settings > Your apps > FlowCE site; public: the rules are the protection)
  apiKey: 'AIzaSyCA7l10IpiZ6Y8FWl9VYesYdQP_glLTz0A',
  authDomain: 'flowce-chat.firebaseapp.com',
  databaseURL: 'https://flowce-chat-default-rtdb.firebaseio.com',
  projectId: 'flowce-chat',
  appId: '1:712025051453:web:9b086cc6fc1aa009cff554',
};
const SDK = 'https://www.gstatic.com/firebasejs/10.12.2/';
const MAX = 100, NAME = 24, TEXT = 300, HIDE = 3, LINK = /(https?:|www\.)/i;
// spam: a message 2.5 s after one's last at the soonest (it then goes by itself), 8 a minute, not the
// same text twice in a minute (the rules: 2 s, 8 in a minute from its first, the same text a minute)
const GAP = 2500, BURST = 8, WIN = 60000;
const CREATOR = 'TommyB3LL'; // the creator's name: his alone, and its look-alikes (the rules refuse them too)
const TAKEN = /t[^a-z0-9]*[o0][^a-z0-9]*m[^a-z0-9]*m[^a-z0-9]*[yi][^a-z0-9]*b[^a-z0-9]*[3e][^a-z0-9]*[l1i][^a-z0-9]*[l1i]/i;
const FRESH = 43200000; // (in the chat: came in the last 12 hours; an older place is one a lost disconnect left)
const $ = id => document.getElementById(id);
const ARGS = new URLSearchParams(location.search), MOCK = ARGS.has('chatmock'), ASK = ARGS.has('creator');
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
  const d = db.getDatabase(app), chat = db.ref(d, 'chat');
  let me = auth.currentUser ? auth.currentUser.uid : null, creator = false, win = null, skew = 0, unwin = null;
  db.onValue(db.ref(d, '.info/serverTimeOffset'), s => { skew = s.val() || 0; });
  const who = async () => { // (this visitor: their count of the minute; the creator, when the rules let them read chat/creator)
    if (unwin) unwin();
    win = null;
    unwin = me ? db.onValue(db.ref(d, `chat/win/${me}`), s => { win = s.val(); }) : null;
    creator = false;
    if (me && auth.currentUser && !auth.currentUser.isAnonymous)
      try { await db.get(db.ref(d, 'chat/creator')); creator = true; } catch (e) { /* (another Google account) */ }
  };
  const signIn = async () => { if (!me) { me = (await fa.signInAnonymously(auth)).user.uid; await who(); } };
  const present = () => { // (in the chat while connected)
    if (!me) return;
    const here = db.ref(d, `chat/online/${me}`);
    db.onDisconnect(here).remove().then(() => db.set(here, db.serverTimestamp())).catch(() => { /* */ });
  };
  const away = () => (me ? db.remove(db.ref(d, `chat/online/${me}`)).catch(() => { /* */ }) : 0);
  if (me) await who(); // (signed in on an earlier visit)
  else try { await signIn(); } catch (e) { /* (reading works without; writing tries again) */ }
  return {
    get me() { return me; },
    get creator() { return creator; },
    watch(add, drop, reports, online) {
      const q = db.query(db.ref(d, 'chat/messages'), db.orderByChild('t'), db.limitToLast(MAX));
      db.onChildAdded(q, s => add(s.key, s.val()));
      db.onChildRemoved(q, s => drop(s.key));
      db.onValue(db.ref(d, 'chat/reports'), s => reports(s.val() || {}));
      db.onValue(db.ref(d, 'chat/online'), s => online(s.val() || {}));
      db.onValue(db.ref(d, '.info/connected'), s => { if (s.val() === true) present(); });
    },
    async send(n, m) {
      await signIn();
      const id = () => db.push(db.ref(d, 'chat/messages')).key, t = db.serverTimestamp();
      if (creator) { await db.set(db.ref(d, `chat/messages/${id()}`), { n, m, u: me, t, o: true }); return; } // (no limits)
      // the message, its time, the minute's count and the text, together: the rules check them all. A
      // refusal: the count read again, then (the minute's edge, the clocks a little apart) the other way
      let fresh = !win || Date.now() + skew - win.t >= WIN;
      for (let k = 1; ; ++k) {
        try {
          await db.update(chat, { [`messages/${id()}`]: { n, m, u: me, t }, [`last/${me}`]: t, [`lastm/${me}`]: m,
            [`win/${me}`]: fresh ? { t, n: 1 } : { t: win.t, n: win.n + 1 } });
          return;
        } catch (e) {
          if (k > 2 || !e || e.code !== 'PERMISSION_DENIED') throw e;
          win = (await db.get(db.ref(d, `chat/win/${me}`))).val();
          fresh = !win || (k === 1 ? Date.now() + skew - win.t >= WIN : !fresh);
        }
      }
    },
    remove(id) { return db.remove(db.ref(d, `chat/messages/${id}`)); },
    async report(id) { // (once per visitor; who reported stays private, only the count shows)
      await signIn();
      await db.update(chat, { [`reporters/${id}/${me}`]: true, [`reports/${id}`]: db.increment(1) });
    },
    async becomeCreator() { // Google, in a popup: true when the rules know the account
      await away();
      me = (await fa.signInWithPopup(auth, new fa.GoogleAuthProvider(), fa.browserPopupRedirectResolver)).user.uid;
      await who();
      present();
      return creator;
    },
    async leave() { // back to an anonymous visitor
      await away();
      await fa.signOut(auth);
      me = null;
      await who();
      try { await signIn(); present(); } catch (e) { /* (writing tries again) */ }
    },
  };
}

function memory() {
  const subs = [], reports = {};
  let k = 0, creator = false;
  return {
    me: 'me',
    get creator() { return creator; },
    watch(add, drop, rep, online) {
      subs.push([add, drop, rep]);
      add('s1', { n: 'Sam', m: 'Does it work on the TI-84 Plus CE Python?', u: 'sam', t: Date.now() - 7200000 });
      add('s2', { n: CREATOR, m: 'Yes: send the .b84 or use the Install button.', u: 'tb', t: Date.now() - 7000000, o: true });
      add('s3', { n: 'Maya', m: 'It runs on mine.', u: 'maya', t: Date.now() - 6900000 });
      online({ me: Date.now(), sam: Date.now() });
    },
    async send(n, m) {
      const id = 'm' + ++k, v = { n, m, u: 'me', t: Date.now() };
      if (creator) v.o = true;
      subs.forEach(s => s[0](id, v));
    },
    async remove(id) { subs.forEach(s => s[1](id)); },
    async report(id) { reports[id] = (reports[id] || 0) + 1; subs.forEach(s => s[2]({ ...reports })); },
    async becomeCreator() { creator = true; return true; },
    async leave() { creator = false; },
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

// a person: a circle with their name's first letter, in a color of the name's own (the creator's: gold)
const FACES = ['#c62828', '#b45309', '#2e7d32', '#00796b', '#1565c0', '#4f46e5', '#7b1fa2', '#ad1457'];
function face(p) {
  const f = document.createElement('span');
  let h = 0;
  for (const c of p.n) h = (h * 31 + c.codePointAt(0)) >>> 0;
  f.className = 'face' + (p.o ? ' creator' : '');
  if (!p.o) f.style.background = FACES[h % FACES.length];
  f.textContent = ([...p.n.trim()][0] || '?').toUpperCase();
  return f;
}
function badge() {
  const b = document.createElement('span');
  b.className = 'chat-badge';
  b.textContent = 'Creator';
  return b;
}
const here = o => Object.values(o || {}).filter(t => typeof t === 'number' && t > Date.now() - FRESH).length;
// the last few who wrote, newest first (each {n: the name, o: the creator})
const recent = ps => [...ps].reverse().filter((p, i, a) => a.findIndex(q => q.n === p.n) === i).slice(0, 3);
function people(ps, n, unread) { // the corner button: who wrote last, and how many are in the chat now
  const sub = $('chat-open-sub');
  $('chat-faces').replaceChildren(...ps.map(face));
  sub.textContent = unread ? 'New messages' : n > 0 ? `${n} online now` : 'with other FlowCE users';
  sub.classList.toggle('new', !!unread);
}
async function peek() { // (before the chat is opened: read once, no connection kept)
  const get = p => fetch(`${CONFIG.databaseURL}/chat/${p}`).then(r => (r.ok ? r.json() : null));
  try {
    const [m, o, r] = await Promise.all([get('messages.json?orderBy=%22t%22&limitToLast=20'),
      get('online.json'), get('reports.json')]);
    let mine = []; // (the ones this visitor reported: hidden for them)
    try { mine = JSON.parse(store.get('flowce-chat-reported')) || []; } catch (e) { /* */ }
    const list = Object.entries(m || {}).filter(([id, v]) => v && typeof v.n === 'string' && (v.o === true
      || !((r || {})[id] >= HIDE)) && !mine.includes(id)).map(([, v]) => v).sort((a, b) => a.t - b.t);
    people(recent(list.map(v => ({ n: clean(v.n), o: v.o === true }))), here(o));
  } catch (e) { /* (the button stays as it is) */ }
}

const ago = t => {
  const s = (Date.now() - t) / 1000;
  return s < 60 ? 'just now' : s < 3600 ? `${Math.floor(s / 60)} min ago` : s < 86400 ? `${Math.floor(s / 3600)} h ago`
    : new Date(t).toLocaleDateString();
};

function count(what) { // (the page's visitor counter, GoatCounter: index.html)
  try { if (window.goatcounter && window.goatcounter.count) window.goatcounter.count({ path: what, title: what, event: true }); } catch (e) { /* */ }
}

async function start(panel) {
  const log = $('chat-log'), form = $('chat-form'), name = $('chat-name'), text = $('chat-text'), status = $('chat-status');
  const say = (s, err) => { status.textContent = s; status.classList.toggle('err', !!err); };
  let chat, counts = {}, reported = [], online = 0, unread = false;
  try { reported = JSON.parse(store.get('flowce-chat-reported')) || []; } catch (e) { /* */ }
  const items = new Map(), people_ = new Map(), vals = new Map(); // (each message's li, its author: {n, o}, it)
  const hidden = id => (!(people_.get(id) || {}).o && counts[id] >= HIDE) || reported.includes(id);
  const button = () => people(recent([...items].filter(([, li]) => !li.hidden).map(([id]) => people_.get(id))), online, unread);
  const add = (id, v) => { // (a message sent from this page shows at once, before the database has it)
    if (!v || typeof v.n !== 'string' || typeof v.m !== 'string' || items.has(id)) return;
    const mine = v.u === chat.me, o = v.o === true, atEnd = log.scrollTop + log.clientHeight >= log.scrollHeight - 8;
    const li = document.createElement('li'), main = document.createElement('div'), head = document.createElement('div');
    const who = document.createElement('strong'), when = document.createElement('span'), body = document.createElement('p');
    const act = document.createElement('button');
    const n = clean(v.n);
    people_.set(id, { n, o });
    vals.set(id, v);
    li.className = 'msg' + (mine ? ' mine' : '') + (o ? ' by-creator' : '');
    li.hidden = hidden(id);
    main.className = 'msg-main';
    head.className = 'msg-head';
    who.textContent = mine ? 'You' : n;
    if (o) who.className = 'creator';
    when.className = 'msg-time';
    when.dataset.t = String(v.t);
    when.textContent = ago(v.t);
    act.type = 'button';
    if (mine || chat.creator) { // one's own, and for the creator any: deletable
      act.className = 'msg-act msg-del'; act.title = mine ? 'Delete your message' : 'Delete this message'; act.textContent = '×';
      act.addEventListener('click', () => chat.remove(id).catch(() => say('Not deleted.', true)));
    } else if (!o) { // anyone else's but the creator's: reportable
      act.className = 'msg-act msg-flag'; act.title = 'Report this message'; act.setAttribute('aria-label', 'Report this message');
      act.addEventListener('click', () => report(id));
    }
    head.append(who);
    if (o) head.append(badge());
    head.append(when);
    if (act.className) head.append(act);
    body.textContent = clean(v.m);
    main.append(head, body);
    if (mine) li.append(main); else li.append(face({ n, o }), main);
    log.append(li);
    items.set(id, li);
    if (atEnd || mine) log.scrollTop = log.scrollHeight;
    if (panel.hidden && !li.hidden) unread = true;
    button();
  };
  const drop = id => { // (deleted, or refused by the database)
    const li = items.get(id);
    if (li) { li.remove(); items.delete(id); people_.delete(id); vals.delete(id); button(); }
  };
  const redraw = () => { // (signed in or out as the creator: whose messages are whose, which can go)
    const all = [...vals];
    items.forEach(li => li.remove());
    items.clear(); people_.clear(); vals.clear();
    all.forEach(([id, v]) => add(id, v));
  };
  const recount = c => {
    counts = c;
    items.forEach((li, id) => { li.hidden = hidden(id); });
    button();
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
  const seen = o => {
    online = here(o);
    $('chat-sub').textContent = `Public chat with other FlowCE users · ${online} online`;
    button();
  };
  panel.addEventListener('chat-open', () => { unread = false; button(); });
  try {
    chat = MOCK ? memory() : await firebase();
    chat.watch(add, drop, recount, seen);
  } catch (e) {
    say('The chat could not connect. Reload to try again.', true);
    return;
  }
  setInterval(() => log.querySelectorAll('[data-t]').forEach(s => { s.textContent = ago(Number(s.dataset.t)); }), 60000);

  // the name: asked once, then "Posting as ..." (change: asked again); the creator: his, and sign out
  const as = $('chat-as'), rename = $('chat-rename'), signin = $('chat-creator');
  const named = () => {
    const n = chat.creator ? CREATOR : store.get('flowce-chat-name');
    name.hidden = !!n;
    as.hidden = !n;
    if (n) $('chat-as-name').textContent = chat.creator ? n : clean(n);
    $('chat-as-badge').hidden = !chat.creator;
    rename.textContent = chat.creator ? 'sign out' : 'change';
    signin.hidden = !ASK || chat.creator;
  };
  name.value = store.get('flowce-chat-name') || '';
  named();
  rename.addEventListener('click', async () => {
    if (!chat.creator) { name.hidden = false; as.hidden = true; name.focus(); name.select(); return; }
    await chat.leave();
    named();
    redraw();
    say('Signed out.');
  });
  signin.addEventListener('click', async () => {
    try {
      say(await chat.becomeCreator() ? 'Signed in: you are the creator.' : 'That Google account is not the creator\'s.', !chat.creator);
      if (!chat.creator) await chat.leave();
    } catch (e) {
      say('Not signed in.', true);
    }
    named();
    redraw();
  });

  // spam: 2.5 s after one's last message at the soonest (it then goes by itself), at most 8 a minute,
  // not the same text twice in a minute; the database holds the same limits; the creator has none
  let times = [], lastm = '', pending = 0;
  const submit = () => (form.requestSubmit ? form.requestSubmit() : form.dispatchEvent(new Event('submit', { cancelable: true })));
  form.addEventListener('submit', async e => {
    e.preventDefault();
    if (pending) return;
    const n = chat.creator ? CREATOR : name.value.trim().slice(0, NAME), m = text.value.trim().slice(0, TEXT);
    if (!n) { name.hidden = false; as.hidden = true; say('Pick a name first: everyone sees it.'); name.focus(); return; }
    if (!m) { text.focus(); return; }
    const now = Date.now();
    if (!chat.creator) {
      if (LINK.test(n) || LINK.test(m)) { say('Links aren\'t allowed in the chat.', true); return; }
      if (TAKEN.test(n)) { name.hidden = false; as.hidden = true; say('That name is the creator\'s: pick another.', true); name.focus(); return; }
      times = times.filter(t => t > now - WIN);
      const at = times.length ? times[times.length - 1] : 0;
      if (m === lastm && at) { say('You just sent that.', true); return; }
      if (times.length >= BURST) { say('That\'s a lot of messages: wait a few seconds.', true); return; }
      if (at + GAP > now) { say('One moment…'); pending = setTimeout(() => { pending = 0; submit(); }, at + GAP - now); return; }
      store.set('flowce-chat-name', n);
    }
    named();
    say('');
    text.value = '';
    times.push(now);
    lastm = m;
    try {
      await chat.send(n, m);
      count('chat-message');
    } catch (err) {
      text.value = m;
      times = times.filter(t => t !== now);
      lastm = '';
      say(err && err.code === 'PERMISSION_DENIED' ? 'Not sent: slow down a little.' : 'Not sent. Try again in a moment.', true);
    }
  });
  form.querySelectorAll('input, button').forEach(el => { el.disabled = false; });
  if (name.hidden && matchMedia('(pointer: fine)').matches) text.focus(); // (a phone's keyboard: when tapped)
}

function init() {
  const panel = $('chat'), opener = $('chat-open');
  if (!panel || !opener || (!CONFIG && !MOCK)) return; // (no database: no chat)
  let started = false;
  const open = () => {
    panel.hidden = false;
    opener.hidden = true;
    panel.dispatchEvent(new Event('chat-open'));
    const log = $('chat-log');
    log.scrollTop = log.scrollHeight;
    if (!started) { started = true; start(panel); count('chat-open'); return; }
    if ($('chat-name').hidden && matchMedia('(pointer: fine)').matches) $('chat-text').focus();
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
  if (location.hash === '#chat' || ASK) open();
  else if (!MOCK) peek();
}

init();
