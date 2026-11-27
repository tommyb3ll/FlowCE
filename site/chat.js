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
// unused for 30 days. Show earlier messages (at the top of the messages) reads the history before them, 100
// at a time, back to the first. chat.html is the chat page: the chat the whole height (no corner button), and
// tabs: the questions the creator answered (with his replies, searchable), the common questions (index.html's
// #faq, read from it).
// The creator (Tommy Bell) signs in with Google from the page opened with ?creator:
// the rules know his account; his messages are marked (TommyB3LL in gold, a Creator badge), have no
// limits and cannot be reported, he can delete any message, and no one else can take his name. He alone
// replies to a message (chat/replies/<its id>, gone with it): one line under it, "Reply from FlowCE" and
// its start, the whole reply when pressed. Before
// it is opened, the button reads who wrote last and how many are in it once (no connection kept: the
// free plan allows 100 at once). ?chatmock: a chat in this page's memory only (tests).
// The creator's tools, signed in: the creator's own messages and replies edited in place (their times kept); on
// chat.html's Answered tab, each question hidden or shown (chat/answered: false or true; neither: shown when a
// reply has a dozen characters and reports don't hide the question), its replies edited or deleted, the question
// deleted; the Admin tab (the creator's only): the messages waiting for an answer (answered there, or marked as
// needing none: chat/done), the problem reports (chat/bugs, their photos apart in chat/bugphotos: a status each,
// or deleted) and the reported messages (kept: their reports cleared; or deleted).
// The problem report form (index.html #report): what was typed, what came back, what's wrong, the version and
// the calculator, a photo of the screen (made smaller here: a JPEG under 380 KB); one a minute; only the
// creator reads them.
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
const REPLIER = 'FlowCE'; // (the creator's replies under a message: "Reply from FlowCE")
const FRESH = 43200000; // (in the chat: came in the last 12 hours; an older place is one a lost disconnect left)
const $ = id => document.getElementById(id);
const ARGS = new URLSearchParams(location.search), MOCK = ARGS.has('chatmock'), ASK = ARGS.has('creator');
const PAGE = document.body.classList.contains('chat-page'); // (chat.html)
const store = { // (none in some private windows: then only this visit remembers)
  get(k) { try { return localStorage.getItem(k); } catch (e) { return null; } },
  set(k, v) { try { localStorage.setItem(k, v); } catch (e) { /* */ } },
};
let CHAT = null; // (the database once connected: the chat page's tabs use it too)
let fbP = null;  // (one connection for the chat and the problem report form)
const connect = () => fbP || (fbP = (MOCK ? Promise.resolve(memory()) : firebase()).catch(e => { fbP = null; throw e; }));

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
    watch(add, drop, reports, online, replies, change) {
      const q = db.query(db.ref(d, 'chat/messages'), db.orderByChild('t'), db.limitToLast(MAX));
      db.onChildAdded(q, s => add(s.key, s.val()));
      db.onChildRemoved(q, s => drop(s.key));
      db.onChildChanged(q, s => change(s.key, s.val())); // (the creator's, edited)
      const r = db.ref(d, 'chat/replies'), each = s => replies(s.key, s.val()); // (a message's: all of them)
      db.onChildAdded(r, each);
      db.onChildChanged(r, each);
      db.onChildRemoved(r, s => replies(s.key, null));
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
    async earlier(before, n) { // the n messages before the time before, oldest first (the history, a page at a time)
      const s = await db.get(db.query(db.ref(d, 'chat/messages'), db.orderByChild('t'), db.endBefore(before), db.limitToLast(n)));
      const out = [];
      s.forEach(c => { out.push([c.key, c.val()]); });
      return out;
    },
    async remove(id) { // (its replies too; the creator: what else was kept about it, if the rules allow)
      await db.update(chat, { [`messages/${id}`]: null, [`replies/${id}`]: null });
      if (creator) await Promise.allSettled(['reports', 'reporters', 'answered', 'done'].map(k => db.remove(db.ref(d, `chat/${k}/${id}`))));
    },
    async reply(id, m) { await db.push(db.ref(d, `chat/replies/${id}`), { m, t: db.serverTimestamp() }); }, // (the creator)
    unreply(id, rid) { return db.remove(db.ref(d, `chat/replies/${id}/${rid}`)); },
    // the creator's tools: edits (the times kept), the marks (answered: shown or hidden on Answered; done: no
    // reply needed; null: none), reports cleared, everything read at once, the problem reports
    editMessage(id, m) { return db.update(db.ref(d, `chat/messages/${id}`), { m }); },
    editReply(id, rid, m) { return db.update(db.ref(d, `chat/replies/${id}/${rid}`), { m }); },
    mark(kind, ids, v) { return db.update(chat, Object.fromEntries([].concat(ids).map(id => [`${kind}/${id}`, v]))); },
    clearReports(id) { return db.update(chat, { [`reports/${id}`]: null, [`reporters/${id}`]: null }); },
    async all() {
      const get = async k => { try { return (await db.get(db.ref(d, `chat/${k}`))).val() || {}; } catch (e) { return {}; } };
      const ks = ['messages', 'replies', 'reports', 'answered', 'done', 'bugs'], vs = await Promise.all(ks.map(get));
      return Object.fromEntries(ks.map((k, i) => [k, vs[i]]));
    },
    async bugPhoto(id) { return (await db.get(db.ref(d, `chat/bugphotos/${id}`))).val(); },
    bugStatus(id, s) { return db.update(db.ref(d, `chat/bugs/${id}`), { status: s || null }); },
    removeBug(id) { return db.update(chat, { [`bugs/${id}`]: null, [`bugphotos/${id}`]: null }); },
    async bug(r, photo) { // (anyone: a problem report, its photo apart; one a minute)
      await signIn();
      const id = db.push(db.ref(d, 'chat/bugs')).key, t = db.serverTimestamp();
      const u = { [`bugs/${id}`]: { ...r, ...(photo ? { pic: true } : {}), t, u: me }, [`buglast/${me}`]: t };
      if (photo) u[`bugphotos/${id}`] = photo;
      await db.update(chat, u);
    },
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
  const subs = [], reports = { s5: 1 }, replies = {}, msgs = {}, marks = { answered: {}, done: {} }, bugs = {}, photos = {};
  let k = 0, creator = false, gave = false;
  const replied = id => subs.forEach(s => s[3](id, replies[id] ? JSON.parse(JSON.stringify(replies[id])) : null));
  const put = (id, v) => { msgs[id] = v; subs.forEach(s => s[0](id, v)); };
  const T = Date.now(), me = () => (creator ? 'tb' : 'me'); // (the creator: another account)
  bugs.b1 = { typed: 'factor(x^4-16)', got: '(x^2+4)(x^2-4)', issue: 'x^2-4 is not factored.', ver: 'FlowCE 3.0', model: 'TI-84 Plus CE', t: T - 5000000, u: 'zed' };
  return {
    get me() { return me(); },
    get creator() { return creator; },
    watch(add, drop, rep, online, rp, change) {
      subs.push([add, drop, rep, rp, change]);
      put('s1', { n: 'Sam', m: 'Does it work on the TI-84 Plus CE Python?', u: 'sam', t: T - 7200000 });
      put('s2', { n: CREATOR, m: 'FlowCE 2.3 is out: faster, and the themes look right on the real screen.', u: 'tb', t: T - 7000000, o: true });
      put('s3', { n: 'Maya', m: 'It runs on mine.', u: 'maya', t: T - 6900000 });
      put('s4', { n: 'Jo', m: 'How do I do partial fractions on it?', u: 'jo', t: T - 3600000 });
      put('s5', { n: 'Max', m: 'this is dumb', u: 'max', t: T - 1800000 });
      replies.s1 = { r0: { m: 'Yes: send FlowCE.b84 with TI Connect CE, or use the Install button on this page.', t: T - 7100000 } };
      replies.s3 = { r1: { m: 'Nice!', t: T - 6800000 } };
      replied('s1');
      replied('s3');
      rep({ ...reports });
      online({ me: Date.now(), sam: Date.now() });
    },
    async send(n, m) {
      const id = 'm' + ++k, v = { n, m, u: me(), t: Date.now() };
      if (creator) v.o = true;
      put(id, v);
    },
    async earlier(before) { // (?chatmock: one older message, once)
      if (gave) return [];
      gave = true;
      return [['s0', { n: 'Ana', m: 'Is there a list of common questions?', u: 'ana', t: before - 600000 }]];
    },
    async remove(id) {
      delete msgs[id]; delete replies[id]; delete reports[id]; delete marks.answered[id]; delete marks.done[id];
      subs.forEach(s => s[1](id)); replied(id); subs.forEach(s => s[2]({ ...reports }));
    },
    async reply(id, m) { replies[id] = { ...replies[id], ['r' + ++k]: { m, t: Date.now() } }; replied(id); },
    async unreply(id, rid) {
      if (replies[id]) delete replies[id][rid];
      if (replies[id] && !Object.keys(replies[id]).length) delete replies[id];
      replied(id);
    },
    async report(id) { reports[id] = (reports[id] || 0) + 1; subs.forEach(s => s[2]({ ...reports })); },
    async editMessage(id, m) { msgs[id] = { ...msgs[id], m }; subs.forEach(s => s[4](id, msgs[id])); },
    async editReply(id, rid, m) { replies[id][rid] = { ...replies[id][rid], m }; replied(id); },
    async mark(kind, ids, v) { [].concat(ids).forEach(id => { if (v === null) delete marks[kind][id]; else marks[kind][id] = v; }); },
    async clearReports(id) { delete reports[id]; subs.forEach(s => s[2]({ ...reports })); },
    async all() {
      const c = o => JSON.parse(JSON.stringify(o));
      return { messages: c(msgs), replies: c(replies), reports: c(reports), answered: c(marks.answered), done: c(marks.done), bugs: c(bugs) };
    },
    async bugPhoto(id) { return photos[id] || null; },
    async bugStatus(id, s) { if (s) bugs[id].status = s; else delete bugs[id].status; },
    async removeBug(id) { delete bugs[id]; delete photos[id]; },
    async bug(r, photo) {
      const id = 'b' + ++k;
      bugs[id] = { ...r, ...(photo ? { pic: true } : {}), t: Date.now(), u: me() };
      if (photo) photos[id] = photo;
    },
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
  const replied = new Map(), opened = new Set(); // (the creator's replies to a message: [id, {m, t}], oldest first; shown whole)
  let replyTo = null; // (the creator: the message his next send replies to)
  let oldest = null; // (the time of the oldest message shown: Show earlier messages reads before it)
  const more = document.createElement('li'), moreb = document.createElement('button');
  more.className = 'msg-more';
  moreb.type = 'button';
  moreb.textContent = 'Show earlier messages';
  more.append(moreb);
  log.prepend(more);
  const hidden = id => (!(people_.get(id) || {}).o && counts[id] >= HIDE) || reported.includes(id);
  const button = () => people(recent([...items].filter(([, li]) => !li.hidden).map(([id]) => people_.get(id))), online, unread);
  const atEnd = () => log.scrollTop + log.clientHeight >= log.scrollHeight - 8;
  const answers = id => { // under the message: "Reply from FlowCE" and its start on one line; pressed: the whole reply
    const li = items.get(id), rs = replied.get(id) || [];
    if (!li) return;
    let box = li.querySelector('.msg-reply');
    if (!rs.length) { if (box) box.remove(); return; }
    if (!box) { box = document.createElement('div'); box.className = 'msg-reply'; li.append(box); }
    const open = opened.has(id), tog = document.createElement('button'), from = document.createElement('span');
    tog.type = 'button';
    tog.className = 'reply-toggle';
    tog.setAttribute('aria-expanded', String(open));
    from.className = 'reply-from';
    from.textContent = rs.length > 1 ? `${rs.length} replies from ${REPLIER}` : `Reply from ${REPLIER}`;
    tog.append(from);
    if (!open) {
      const gist = document.createElement('span');
      gist.className = 'reply-gist';
      gist.textContent = clean(rs[rs.length - 1][1].m);
      tog.append(gist);
    }
    tog.addEventListener('click', () => { if (!opened.delete(id)) opened.add(id); answers(id); });
    box.replaceChildren(tog);
    if (open) rs.forEach(([rid, r]) => {
      const p = document.createElement('p'), when = document.createElement('span');
      p.className = 'reply-text';
      p.textContent = clean(r.m) + ' ';
      when.className = 'msg-time';
      when.dataset.t = String(r.t);
      when.textContent = ago(r.t);
      p.append(when);
      if (chat.creator) {
        const ed = document.createElement('button'), del = document.createElement('button');
        ed.type = 'button'; ed.className = 'msg-act msg-edit'; ed.title = 'Edit this reply'; ed.setAttribute('aria-label', 'Edit this reply');
        ed.addEventListener('click', () => editor(p, r.m, m => chat.editReply(id, rid, m)));
        del.type = 'button'; del.className = 'msg-act msg-del'; del.title = 'Delete this reply'; del.textContent = '×';
        del.addEventListener('click', () => chat.unreply(id, rid).catch(() => say('Not deleted.', true)));
        p.append(ed, del);
      }
      box.append(p);
    });
  };
  const reply = (id, v) => { // (the database: a message's replies, all of them, or none)
    const rs = Object.entries(v && typeof v === 'object' ? v : {})
      .filter(([, r]) => r && typeof r.m === 'string' && typeof r.t === 'number').sort((a, b) => a[1].t - b[1].t);
    const end = atEnd(), li = items.get(id);
    if (rs.length) replied.set(id, rs); else { replied.delete(id); opened.delete(id); }
    answers(id);
    if (end) log.scrollTop = log.scrollHeight;
    if (panel.hidden && rs.length && li && !li.hidden) { unread = true; button(); }
  };
  const replying = id => { // (the creator pressed a message's reply: his next send answers it; null: no more)
    replyTo = id && items.has(id) ? id : null;
    $('chat-replying').hidden = !replyTo;
    text.placeholder = replyTo ? `Reply as ${REPLIER}` : 'Message everyone';
    if (!replyTo) return;
    $('chat-replying-to').textContent = people_.get(id).n;
    $('chat-replying-gist').textContent = clean(vals.get(id).m);
    text.focus();
  };
  const add = (id, v, top) => { // (a message sent from this page shows at once, before the database has it; top: an
    if (!v || typeof v.n !== 'string' || typeof v.m !== 'string' || items.has(id)) return; // earlier one, above the others)
    const mine = v.u === chat.me, o = v.o === true, end = atEnd();
    if (typeof v.t === 'number' && (oldest === null || v.t < oldest)) oldest = v.t;
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
    if (chat.creator && !mine) { // the creator: a reply to it, under it
      const re = document.createElement('button');
      re.type = 'button'; re.className = 'msg-act msg-answer'; re.title = `Reply as ${REPLIER}`; re.setAttribute('aria-label', `Reply as ${REPLIER}`);
      re.addEventListener('click', () => replying(id));
      head.append(re);
    }
    if (mine && o && chat.creator) { // (the creator's own: edited in place)
      const ed = document.createElement('button');
      ed.type = 'button'; ed.className = 'msg-act msg-edit'; ed.title = 'Edit your message'; ed.setAttribute('aria-label', 'Edit your message');
      ed.addEventListener('click', () => editor(body, vals.get(id).m, m => chat.editMessage(id, m)));
      head.append(ed);
    }
    if (act.className) head.append(act);
    body.textContent = clean(v.m);
    main.append(head, body);
    if (mine) li.append(main); else li.append(face({ n, o }), main);
    if (top) more.after(li); else log.append(li);
    items.set(id, li);
    answers(id);
    if (top) return;
    if (end || mine) log.scrollTop = log.scrollHeight;
    if (panel.hidden && !li.hidden) unread = true;
    button();
  };
  moreb.addEventListener('click', async () => { // the history: the 100 before the oldest shown, the view kept where it was
    if (oldest === null || moreb.disabled) return;
    moreb.disabled = true;
    moreb.textContent = 'Loading…';
    try {
      const page = await chat.earlier(oldest, MAX), h0 = log.scrollHeight, s0 = log.scrollTop;
      for (let i = page.length - 1; i >= 0; --i) add(page[i][0], page[i][1], true); // (each above the one after it)
      log.scrollTop = s0 + log.scrollHeight - h0;
      const start_ = page.length < MAX;
      moreb.textContent = start_ ? 'The start of the chat' : 'Show earlier messages';
      moreb.disabled = start_;
      count('chat-earlier');
    } catch (e) {
      moreb.textContent = 'Show earlier messages';
      moreb.disabled = false;
      say('Could not load them. Try again.', true);
    }
  });
  const drop = id => { // (deleted, or refused by the database)
    const li = items.get(id);
    if (li) { li.remove(); items.delete(id); people_.delete(id); vals.delete(id); button(); }
    if (replyTo === id) replying(null);
  };
  const change = (id, v) => { // (the creator's message, edited)
    const li = items.get(id);
    if (!li || !v || typeof v.m !== 'string') return;
    vals.set(id, v);
    li.querySelector('.msg-main > p').textContent = clean(v.m);
  };
  const redraw = () => { // (signed in or out as the creator: whose messages are whose, which can go)
    const all = [...vals].sort((a, b) => a[1].t - b[1].t); // (the earlier ones read later: in their places)
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
    chat = await connect();
    CHAT = chat;
    chat.watch(add, drop, recount, seen, reply, change);
  } catch (e) {
    say('The chat could not connect. Reload to try again.', true);
    return;
  }
  adminSync();
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
    replying(null);
    named();
    redraw();
    adminSync();
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
    adminSync();
  });

  // spam: 2.5 s after one's last message at the soonest (it then goes by itself), at most 8 a minute,
  // not the same text twice in a minute; the database holds the same limits; the creator has none
  let times = [], lastm = '', pending = 0;
  const submit = () => (form.requestSubmit ? form.requestSubmit() : form.dispatchEvent(new Event('submit', { cancelable: true })));
  $('chat-reply-cancel').addEventListener('click', () => { replying(null); text.focus(); });
  text.addEventListener('keydown', e => { if (e.key === 'Escape' && replyTo) { e.stopPropagation(); replying(null); } });
  form.addEventListener('submit', async e => {
    e.preventDefault();
    if (pending) return;
    const n = chat.creator ? CREATOR : name.value.trim().slice(0, NAME), m = text.value.trim().slice(0, TEXT);
    if (chat.creator && replyTo) { // his reply, under the message, open
      const id = replyTo;
      if (!m) { text.focus(); return; }
      say('');
      text.value = '';
      opened.add(id);
      replying(null);
      try {
        await chat.reply(id, m);
      } catch (err) {
        text.value = m;
        replying(id);
        say('Not sent. Try again in a moment.', true);
      }
      return;
    }
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
  if (PAGE) { // (chat.html: the chat open the whole time, no corner button; the tabs)
    panel.hidden = false;
    start(panel);
    count('chat-page');
    tabs();
    return;
  }
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

// The creator's edit in place: the text in a box, Save and Cancel (Escape); saved: the box goes (the database
// tells the page the new text), not saved: said on the button
function editor(at, text, save, after) {
  if (at.nextElementSibling && at.nextElementSibling.classList.contains('ed')) return; // (open already)
  const box = document.createElement('form'), ta = document.createElement('textarea'), bar = document.createElement('div');
  const ok = document.createElement('button'), no = document.createElement('button');
  box.className = 'ed';
  ta.value = text;
  ta.maxLength = TEXT;
  ta.rows = Math.min(6, Math.max(2, Math.ceil(text.length / 56)));
  ta.setAttribute('aria-label', 'The new text');
  ok.type = 'submit'; ok.className = 'btn primary'; ok.textContent = 'Save';
  no.type = 'button'; no.className = 'btn'; no.textContent = 'Cancel';
  bar.className = 'ed-bar';
  bar.append(ok, no);
  box.append(ta, bar);
  at.hidden = true;
  at.after(box);
  ta.focus();
  const close = () => { box.remove(); at.hidden = false; };
  no.addEventListener('click', close);
  ta.addEventListener('keydown', e => {
    if (e.key === 'Escape') { e.stopPropagation(); close(); }
    if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); box.requestSubmit(); }
  });
  box.addEventListener('submit', async e => {
    e.preventDefault();
    const m = ta.value.trim().slice(0, TEXT);
    if (!m || m === text) { close(); return; }
    ok.disabled = true;
    ok.textContent = 'Saving…';
    try {
      await save(m);
      close();
      if (after) after();
    } catch (err) {
      ok.disabled = false;
      ok.textContent = 'Not saved: try again';
    }
  });
}

// small builders for the tabs
const el = (tag, cls, text) => { const e = document.createElement(tag); if (cls) e.className = cls; if (text !== undefined) e.textContent = text; return e; };
const tool = (text, fn, cls) => { const b = el('button', 'qa-tool' + (cls ? ' ' + cls : ''), text); b.type = 'button'; b.addEventListener('click', fn); return b; };
const day = t => new Date(t).toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: new Date(t).getFullYear() === new Date().getFullYear() ? undefined : 'numeric' });
function question(v, when) { // (a message as a question: its writer's circle and name, when, the text)
  const q = el('div', 'qa-q'), head = el('div', 'msg-head'), who = el('strong', v.o === true ? 'creator' : '', clean(v.n));
  head.append(who, el('span', 'msg-time', when || day(v.t)));
  q.append(face({ n: clean(v.n), o: v.o === true }), head, el('p', '', clean(v.m)));
  return q;
}
const cpSay = (s, err) => { const p = $('cp-status'); if (!p) return; p.textContent = s; p.classList.toggle('err', !!err); };
// a change of the creator's, then the tab drawn again; refused: said
async function act(fn, done, fail) {
  try { await fn(); if (done) cpSay(done); } catch (e) { cpSay(fail || 'That didn\'t work. Try again in a moment.', true); }
}

// The chat page's tabs: the live chat; the questions the creator answered, with the replies (the database's
// messages that have replies of a dozen characters or more, newest answer first, none of the hidden ones; read
// when the tab opens), with a search; the common questions (index.html's #faq, read from it once); the Admin tab
// (the creator only)
let showTab = () => {};
function tabs() {
  const bs = document.querySelectorAll('[data-tab]');
  let faqDone = false;
  showTab = (k, keep) => {
    if (k === 'admin' && !(CHAT && CHAT.creator)) k = 'live';
    bs.forEach(b => { const on = b.dataset.tab === k; b.setAttribute('aria-selected', String(on)); b.classList.toggle('on', on); });
    document.querySelectorAll('.cp-panel').forEach(p => { p.hidden = p.id !== 'tab-' + k; });
    cpSay('');
    if (k === 'answered') answered();
    if (k === 'admin') admin();
    if (k === 'faq' && !faqDone) {
      faqDone = true;
      fetch('index.html').then(r => r.text()).then(t => {
        const f = new DOMParser().parseFromString(t, 'text/html').getElementById('faq-list');
        if (f) $('faq-here').replaceChildren(document.importNode(f, true));
      }).catch(() => { faqDone = false; });
    }
    if (!keep) history.replaceState(null, '', k === 'live' ? location.pathname + location.search : location.search + '#' + k);
    if (k === 'live') { const log = $('chat-log'); log.scrollTop = log.scrollHeight; }
  };
  bs.forEach(b => b.addEventListener('click', () => showTab(b.dataset.tab)));
  const h = location.hash.slice(1);
  if (h === 'answered' || h === 'faq') showTab(h, true);
  const q = $('ans-search');
  if (q) q.addEventListener('input', () => {
    const w = q.value.trim().toLowerCase();
    $('ans-list').querySelectorAll('.qa').forEach(li => { li.hidden = !!w && !li.textContent.toLowerCase().includes(w); });
  });
  const r = $('ad-refresh');
  if (r) r.addEventListener('click', () => { cpSay(''); admin(); });
}

// Signed in as the creator or not: the Admin tab there or not (its count: what waits), Answered with the tools
// or without
function adminSync() {
  if (!PAGE) return;
  const b = $('tab-admin-btn'), boss = !!(CHAT && CHAT.creator);
  if (!b) return;
  b.hidden = !boss;
  if (!boss && !$('tab-admin').hidden) showTab('live');
  if (!$('tab-answered').hidden) answered();
  if (!boss) return;
  if (location.hash === '#admin' && $('tab-admin').hidden) showTab('admin', true);
  else if (!$('tab-admin').hidden) admin();
  else CHAT.all().then(x => adminCount(sortOut(x))).catch(() => { /* */ });
}
function adminCount(o) {
  const n = o.wait.length + o.bugs.filter(([, b]) => !b.status).length + o.rep.length, c = $('admin-count');
  c.textContent = n > 99 ? '99+' : String(n);
  c.hidden = !n;
}
// everything read, sorted out: the messages waiting for an answer (anyone's but the creator's, no reply, not
// marked as needing none), those marked so, the problem reports, the reported messages; newest first
function sortOut(x) {
  const ok = v => v && typeof v.n === 'string' && typeof v.m === 'string' && typeof v.t === 'number';
  const newest = (p, q) => q[1].t - p[1].t;
  const msgs = Object.entries(x.messages || {}).filter(([, v]) => ok(v));
  return {
    x,
    wait: msgs.filter(([id, v]) => v.o !== true && !x.replies[id] && !x.done[id]).sort(newest),
    done: msgs.filter(([id, v]) => v.o !== true && !x.replies[id] && x.done[id]).sort(newest),
    bugs: Object.entries(x.bugs || {}).filter(([, b]) => b && typeof b.issue === 'string' && typeof b.t === 'number').sort(newest),
    rep: msgs.filter(([id]) => (x.reports[id] || 0) > 0).sort(newest),
  };
}

// The Answered tab. Everyone: the questions with an answer shown (see the top); the creator: all that have a
// reply, the ones not shown to everyone marked so, with the tools (show or hide; edit or delete a reply; delete
// the question)
async function answered() {
  const list = $('ans-list'), boss = !!(CHAT && CHAT.creator);
  const get = p => fetch(`${CONFIG.databaseURL}/chat/${p}.json`).then(r => (r.ok ? r.json() : null)).catch(() => null);
  if (!list.children.length) list.innerHTML = '<li class="qa-wait">Loading the answers…</li>';
  try {
    let m, r, c, a;
    if (CHAT && (boss || MOCK)) ({ messages: m, replies: r, reports: c, answered: a } = await CHAT.all());
    else if (MOCK) [m, r, c, a] = [{}, {}, {}, {}];
    else [m, r, c, a] = await Promise.all(['messages', 'replies', 'reports', 'answered'].map(get));
    m = m || {}; r = r || {}; c = c || {}; a = a || {};
    const qs = [];
    for (const [id, rsObj] of Object.entries(r)) {
      const v = m[id];
      if (!v || typeof v.n !== 'string' || typeof v.m !== 'string') continue;
      const all = Object.entries(rsObj || {}).filter(([, x]) => x && typeof x.m === 'string' && typeof x.t === 'number').sort((p, q) => p[1].t - q[1].t);
      if (!all.length) continue;
      const long = all.filter(([, x]) => x.m.trim().length >= 12); // (not a word or two: ":(", "yes")
      const off = a[id] === false ? 'Hidden by you' : a[id] === true ? ''
        : v.o !== true && (c[id] || 0) >= HIDE ? 'Not shown: reported' : !long.length ? 'Not shown: short reply' : '';
      if (off && !boss) continue;
      qs.push({ id, v, rs: boss || a[id] === true ? all : long, off });
    }
    qs.sort((p, q) => q.rs[q.rs.length - 1][1].t - p.rs[p.rs.length - 1][1].t);
    const lis = qs.map(({ id, v, rs, off }) => {
      const li = el('li', 'qa' + (off ? ' off' : '')), q = question(v);
      li.append(q);
      if (boss) {
        const tools = el('div', 'qa-tools');
        if (off) tools.append(el('span', 'qa-off', off));
        tools.append(off ? tool('Show in Answered', () => act(() => CHAT.mark('answered', id, true), 'Shown in Answered.').then(answered))
          : tool('Hide from Answered', () => act(() => CHAT.mark('answered', id, false), 'Hidden from Answered.').then(answered)));
        tools.append(tool('Delete question', () => {
          if (confirm('Delete this question and its answers, for everyone?')) act(() => CHAT.remove(id), 'Deleted.').then(answered);
        }, 'danger'));
        q.append(tools);
      }
      rs.forEach(([rid, x]) => {
        const box = el('div', 'qa-a'), t = el('p', '', clean(x.m));
        box.append(el('span', 'reply-from', `Reply from ${REPLIER}`), t);
        if (boss) {
          const tools = el('div', 'qa-tools');
          tools.append(tool('Edit', () => editor(t, x.m, mm => CHAT.editReply(id, rid, mm), answered)),
            tool('Delete', () => { if (confirm('Delete this answer?')) act(() => CHAT.unreply(id, rid), 'Answer deleted.').then(answered); }, 'danger'));
          box.append(tools);
        }
        li.append(box);
      });
      return li;
    });
    list.replaceChildren(...lis);
    if (!lis.length) list.innerHTML = '<li class="qa-wait">No answers yet.</li>';
    const s = $('ans-search');
    if (s && s.value) s.dispatchEvent(new Event('input'));
  } catch (e) {
    list.innerHTML = '<li class="qa-wait">The answers could not be read. Reload to try again.</li>';
  }
}

// The Admin tab (the creator only): Waiting for an answer, Problem reports, Reported; read again on Refresh
// and after each change
let adPart = 'wait', adShown = 40;
const BUG_STATUS = [['', 'New'], ['seen', 'Looking into it'], ['fixed', 'Fixed'], ['nope', 'Not a bug']];
async function admin() {
  const body = $('ad-body');
  if (!body || !CHAT || !CHAT.creator) return;
  if (!body.children.length) body.innerHTML = '<p class="qa-wait">Loading…</p>';
  let o;
  try { o = sortOut(await CHAT.all()); } catch (e) { body.innerHTML = '<p class="qa-wait">Could not read them. Try Refresh.</p>'; return; }
  adminCount(o);
  const fresh = o.bugs.filter(([, b]) => !b.status).length;
  const parts = [['wait', 'Waiting for an answer', o.wait.length], ['bugs', 'Problem reports', fresh ? `${fresh} new` : o.bugs.length], ['rep', 'Reported', o.rep.length]];
  const seg = el('div', 'ad-seg');
  seg.setAttribute('role', 'tablist');
  parts.forEach(([k, name, n]) => {
    const b = el('button', k === adPart ? 'on' : '');
    b.type = 'button';
    b.setAttribute('role', 'tab');
    b.setAttribute('aria-selected', String(k === adPart));
    b.append(name, ' ', el('span', 'ad-n', String(n)));
    b.addEventListener('click', () => { adPart = k; adShown = 40; cpSay(''); admin(); });
    seg.append(b);
  });
  const list = el('ul', 'qa-list ad-list'), redo = () => admin();
  if (adPart === 'wait') {
    const head = el('div', 'ad-head');
    head.append(el('p', 'cp-lead', o.wait.length ? 'Messages with no reply yet, newest first.' : 'Nothing is waiting for an answer.'));
    if (o.wait.length > 1) head.append(tool('None of these need a reply', () => {
      if (confirm(`Mark all ${o.wait.length} as not needing a reply? They stay in the chat.`))
        act(() => CHAT.mark('done', o.wait.map(([id]) => id), true), 'Marked.').then(redo);
    }));
    o.wait.slice(0, adShown).forEach(([id, v]) => {
      const li = el('li', 'qa'), q = question(v, `${day(v.t)} · ${ago(v.t)}`), tools = el('div', 'qa-tools');
      tools.append(tool('Reply', () => replyBox(li, id)),
        tool('No reply needed', () => act(() => CHAT.mark('done', id, true)).then(redo)),
        tool('Delete', () => { if (confirm('Delete this message for everyone?')) act(() => CHAT.remove(id), 'Deleted.').then(redo); }, 'danger'));
      q.append(tools);
      li.append(q);
      list.append(li);
    });
    body.replaceChildren(seg, head, list);
    if (o.wait.length > adShown) body.append(tool(`Show ${Math.min(40, o.wait.length - adShown)} more`, () => { adShown += 40; admin(); }, 'ad-more'));
    if (o.done.length) {
      const d = el('details', 'ad-done'), dl = el('ul', 'qa-list ad-list');
      d.append(el('summary', '', `Marked as no reply needed (${o.done.length})`), dl);
      o.done.slice(0, 60).forEach(([id, v]) => {
        const li = el('li', 'qa'), q = question(v);
        const tools = el('div', 'qa-tools');
        tools.append(tool('Undo', () => act(() => CHAT.mark('done', id, null)).then(redo)));
        q.append(tools);
        li.append(q);
        dl.append(li);
      });
      body.append(d);
    }
  } else if (adPart === 'bugs') {
    const head = el('p', 'cp-lead', o.bugs.length ? 'From Report a bug on the main page. Only you see them.' : 'No problem reports yet. They come from Report a bug on the main page.');
    o.bugs.forEach(([id, b]) => {
      const li = el('li', 'qa bug-item' + (b.status ? '' : ' new')), top = el('div', 'msg-head'), dl = el('dl', 'bug-dl');
      top.append(el('strong', '', b.status ? (BUG_STATUS.find(s => s[0] === b.status) || ['', b.status])[1] : 'New'),
        el('span', 'msg-time', `${day(b.t)} · ${ago(b.t)}${b.ver ? ' · ' + b.ver : ''}${b.model ? ' · ' + b.model : ''}`));
      [['Typed', b.typed], ['Came back', b.got], ['What\'s wrong', b.issue]].forEach(([k, val]) => {
        if (!val) return;
        dl.append(el('dt', '', k), el('dd', '', val));
      });
      const tools = el('div', 'qa-tools'), sel = el('select', 'bug-state');
      sel.setAttribute('aria-label', 'Status');
      BUG_STATUS.forEach(([k, name]) => { const op = el('option', '', name); op.value = k; op.selected = (b.status || '') === k; sel.append(op); });
      sel.addEventListener('change', () => act(() => CHAT.bugStatus(id, sel.value)).then(redo));
      tools.append(sel);
      if (b.pic) {
        const pic = tool('Show the photo', async () => {
          pic.disabled = true;
          try {
            const src = await CHAT.bugPhoto(id);
            if (!src) throw new Error('none');
            const img = el('img', 'bug-pic');
            img.src = src;
            img.alt = 'The photo sent with this report';
            img.addEventListener('click', () => img.classList.toggle('big'));
            pic.replaceWith(img);
          } catch (e) { pic.disabled = false; cpSay('The photo could not be read.', true); }
        });
        tools.append(pic);
      }
      tools.append(tool('Delete', () => { if (confirm('Delete this problem report?')) act(() => CHAT.removeBug(id), 'Deleted.').then(redo); }, 'danger'));
      li.append(top, dl, tools);
      list.append(li);
    });
    body.replaceChildren(seg, head, list);
  } else {
    const head = el('p', 'cp-lead', o.rep.length ? `Messages someone reported. ${HIDE} reports hide one from everyone.` : 'No reported messages.');
    o.rep.forEach(([id, v]) => {
      const li = el('li', 'qa'), q = question(v), n = o.x.reports[id];
      const tools = el('div', 'qa-tools');
      tools.append(el('span', 'qa-off', `Reported ${n === 1 ? 'once' : n + ' times'}${n >= HIDE && v.o !== true ? ': hidden' : ''}`),
        tool('Keep it', () => act(() => CHAT.clearReports(id), 'Kept: its reports are cleared.').then(redo)),
        tool('Delete', () => { if (confirm('Delete this message for everyone?')) act(() => CHAT.remove(id), 'Deleted.').then(redo); }, 'danger'));
      q.append(tools);
      li.append(q);
      list.append(li);
    });
    body.replaceChildren(seg, head, list);
  }
}
// a reply written on the Admin tab: under the message, sent as a reply from FlowCE
function replyBox(li, id) {
  if (li.querySelector('.ed')) { li.querySelector('.ed textarea').focus(); return; }
  const anchor = li.querySelector('.ad-anchor') || li.appendChild(el('p', 'ad-anchor')); // (where the box opens)
  editor(anchor, '', m => CHAT.reply(id, m), () => { cpSay('Replied.'); admin(); });
  const f = li.querySelector('.ed');
  if (f) { f.querySelector('textarea').placeholder = `Reply as ${REPLIER}`; f.querySelector('button[type=submit]').textContent = 'Send reply'; }
}

// The problem report form (index.html #report): sent to the database (chat/bugs; the photo apart), one a minute
function reportForm() {
  const f = $('bug-form');
  if (!f || (!CONFIG && !MOCK)) return;
  const file = $('bug-file'), thumb = $('bug-thumb'), unpick = $('bug-unpick'), st = $('bug-status'), go = $('bug-send');
  const say = (s, err) => { st.textContent = s; st.classList.toggle('err', !!err); };
  let photo = null;
  // the version list: this page's versions (their version files)
  const ver = $('bug-ver');
  Promise.all(['version.txt', 'version-khicas.txt'].map(n => fetch('files/' + n, { cache: 'no-store' })
    .then(r => (r.ok ? r.text() : '')).catch(() => ''))).then(([a, b]) => {
    const opts = ver.querySelectorAll('option');
    if (a.trim() && /^[\w.\- ]{1,20}$/.test(a.trim())) opts[0].textContent = `FlowCE ${a.trim()}`;
    if (b.trim() && /^[\w.\- ]{1,20}$/.test(b.trim())) opts[1].textContent = `KhiCAS + Flow ${b.trim()}`;
  });
  file.addEventListener('change', async () => {
    const fl = file.files && file.files[0];
    if (!fl) return;
    say('Reading the photo…');
    try {
      photo = await shrink(fl);
      thumb.src = photo;
      thumb.hidden = false;
      unpick.hidden = false;
      say('');
    } catch (e) {
      photo = null;
      thumb.hidden = true;
      unpick.hidden = true;
      say('That photo couldn\'t be read. Try a screenshot, or a JPEG or PNG.', true);
    }
    file.value = '';
  });
  unpick.addEventListener('click', () => { photo = null; thumb.hidden = true; thumb.removeAttribute('src'); unpick.hidden = true; });
  f.addEventListener('submit', async e => {
    e.preventDefault();
    const typed = $('bug-typed').value.trim().slice(0, 300), got = $('bug-got').value.trim().slice(0, 300);
    const issue = $('bug-issue').value.trim().slice(0, 1000);
    if (!issue) { say('Say what\'s wrong first.', true); $('bug-issue').focus(); return; }
    const r = { issue };
    if (typed) r.typed = typed;
    if (got) r.got = got;
    const v = ver.options[ver.selectedIndex].textContent.trim().slice(0, 40), m = $('bug-model').value.trim().slice(0, 80);
    if (v) r.ver = v;
    if (m) r.model = m;
    go.disabled = true;
    say('Sending…');
    try {
      const c = await connect();
      await c.bug(r, photo);
      f.reset();
      photo = null;
      thumb.hidden = true;
      unpick.hidden = true;
      say('Sent. Thank you! Fixes show up in What\'s new.');
      count('bug-sent');
    } catch (err) {
      say(err && err.code === 'PERMISSION_DENIED' ? 'Not sent: one report a minute. Try again in a moment.' : 'Not sent. Try again in a moment.', true);
    }
    go.disabled = false;
  });
}
// a photo of the screen, smaller: at most 1280 pixels across, a JPEG under 380 KB
async function shrink(fl) {
  const url = URL.createObjectURL(fl);
  try {
    const img = new Image(); // (its load event: decode() can wait forever in a page out of view)
    await new Promise((ok, no) => { img.onload = ok; img.onerror = () => no(new Error('unreadable')); img.src = url; });
    let w = img.naturalWidth, h = img.naturalHeight;
    if (!w || !h) throw new Error('empty');
    for (let side = 1280; side >= 320; side = Math.round(side * 0.7)) {
      const k = Math.min(1, side / Math.max(w, h)), cv = document.createElement('canvas');
      cv.width = Math.max(1, Math.round(w * k));
      cv.height = Math.max(1, Math.round(h * k));
      const g = cv.getContext('2d');
      g.fillStyle = '#fff';
      g.fillRect(0, 0, cv.width, cv.height);
      g.drawImage(img, 0, 0, cv.width, cv.height);
      for (const qu of [0.85, 0.72, 0.6]) {
        const s = cv.toDataURL('image/jpeg', qu);
        if (s.startsWith('data:image/jpeg;base64,') && s.length <= 380000) return s;
      }
    }
    throw new Error('too big');
  } finally {
    URL.revokeObjectURL(url);
  }
}

init();
reportForm();
