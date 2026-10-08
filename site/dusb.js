// dusb.js - the TI-84 Plus CE's USB link protocol (DUSB) over WebUSB: just what the FlowCE
// installer needs (ping, parameters, sending variables, pressing keys).
//
// Adapted from ticalc-usb by Tim Franssen (https://github.com/Timendus/ticalc-usb,
// GPL-3.0-or-later), with details from tilibs (https://github.com/debrouxl/tilibs, GPL-2.0-or-later):
// the CE's variable type attribute (0xF0 0x0F 0x00 type, calc_84p.cc) and the OS version
// parameters (get_version). FlowCE is free software under the GNU GPL, version 3 or later.

// TI-84 Plus CE and TI-83 Premium CE (all editions) share these USB ids
export const TI_CE = { vendorId: 0x0451, productId: 0xe008 };

const RAW = { BUF_REQ: 1, BUF_ALLOC: 2, DATA: 3, DATA_LAST: 4, ACK: 5 };
const VIRT = {
  PING: 0x0001, PARM_REQ: 0x0007, PARM_DATA: 0x0008, RTS: 0x000b, VAR_CNTS: 0x000d,
  EXECUTE: 0x0011, MODE_SET: 0x0012, DATA_ACK: 0xaa00, DELAY_ACK: 0xbb00, EOT: 0xdd00, ERROR: 0xee00,
};
const MODE_NORMAL = [0, 3, 0, 1, 0, 0, 0, 0, 0x07, 0xd0];
export const PID = { PRODUCT_NAME: 0x0002, OS_VERSION: 0x000b, FREE_RAM: 0x000e, FREE_FLASH: 0x0011, OS_BUILD: 0x0048 };
// TI-OS key codes (tilibs keys83p.h)
export const KEY = {
  right: 0x01, left: 0x02, up: 0x03, down: 0x04, enter: 0x05, clear: 0x09, apps: 0x27, prgm: 0x2d,
  mem: 0x36, quit: 0x40, resetMem: 0x4e, k1: 0x8f, k2: 0x90, k3: 0x91, k7: 0x95,
};

const be = (n, len) => Array.from({ length: len }, (_, i) => Math.floor(n / 2 ** (8 * (len - 1 - i))) & 255);
const num = bytes => Array.from(bytes).reduce((a, b) => a * 256 + b, 0);
const ascii = s => Array.from(s, c => c.charCodeAt(0) & 255);
const sleep = ms => new Promise(r => setTimeout(r, ms));

export class LinkError extends Error {
  constructor(code, message) {
    super(message);
    this.code = code;
  }
}

export class Calculator {
  constructor(device) {
    this.dev = device;
  }

  get name() {
    return this.dev.productName || 'TI-84 Plus CE';
  }

  // The browser asks the user to pick the calculator (needs a click)
  static async request() {
    const device = await navigator.usb.requestDevice({ filters: [TI_CE] });
    const calc = new Calculator(device);
    await calc.open();
    return calc;
  }

  async open() {
    const d = this.dev;
    try {
      if (!d.opened) await d.open();
      if (d.configuration === null) await d.selectConfiguration(1);
      const iface = d.configuration.interfaces[0];
      await d.claimInterface(iface.interfaceNumber);
      this.iface = iface.interfaceNumber;
      const eps = iface.alternates[0].endpoints;
      this.inEp = eps.find(e => e.direction === 'in');
      this.outEp = eps.find(e => e.direction === 'out');
    } catch (e) {
      // Windows: another driver (TI Connect CE's) owns the calculator, or another tab has it
      throw new LinkError('claim', String(e && e.message || e));
    }
    this.buf = await this.bufferSize();
  }

  async close() {
    try {
      await this.dev.releaseInterface(this.iface);
    } catch (e) { /* already gone */ }
    try {
      await this.dev.close();
    } catch (e) { /* already gone */ }
  }

  /* raw packets: size (4 bytes, big-endian), type (1 byte), data */

  async rawSend(type, data, timeout = 10000) {
    const pkt = new Uint8Array(5 + data.length);
    pkt.set(be(data.length, 4));
    pkt[4] = type;
    pkt.set(data, 5);
    const r = await withTimeout(this.dev.transferOut(this.outEp.endpointNumber, pkt), timeout);
    if (r.status !== 'ok') throw new LinkError('io', 'send: ' + r.status);
  }

  async rawRecv(timeout = 10000) {
    // a raw packet can span several USB packets: read until its declared size has arrived
    const chunks = [];
    let got = 0, want = Infinity;
    while (got < want) {
      const r = await withTimeout(this.dev.transferIn(this.inEp.endpointNumber, this.inEp.packetSize), timeout);
      if (r.status !== 'ok') throw new LinkError('io', 'receive: ' + r.status);
      const c = new Uint8Array(r.data.buffer, r.data.byteOffset, r.data.byteLength);
      chunks.push(c);
      got += c.length;
      if (want === Infinity && got >= 5) {
        const all = concat(chunks);
        want = 5 + num(all.slice(0, 4));
      }
    }
    const p = concat(chunks);
    return { type: p[4], data: p.slice(5) };
  }

  async bufferSize() {
    await this.rawSend(RAW.BUF_REQ, [0, 0, 4, 0]); // ask for 1024 bytes
    const r = await this.rawRecv();
    if (r.type !== RAW.BUF_ALLOC) throw new LinkError('protocol', 'no buffer size');
    return num(r.data);
  }

  /* virtual packets: size (4), type (2), data; cut in raw packets of the buffer size, each acked */

  async send(type, data = [], onBytes, timeout = 10000) {
    const v = new Uint8Array(6 + data.length);
    v.set(be(data.length, 4));
    v.set(be(type, 2), 4);
    v.set(data, 6);
    const step = this.buf - 5;
    for (let off = 0; off < v.length; off += step) {
      const last = off + step >= v.length;
      await this.rawSend(last ? RAW.DATA_LAST : RAW.DATA, v.subarray(off, off + step), timeout);
      const ack = await this.rawRecv(timeout);
      if (ack.type !== RAW.ACK) throw new LinkError('protocol', 'expected an ack');
      if (onBytes) onBytes(Math.min(off + step, v.length) - 6);
    }
  }

  async recv(timeout = 30000) {
    for (;;) {
      const parts = [];
      for (;;) {
        const r = await this.rawRecv(timeout);
        if (r.type !== RAW.DATA && r.type !== RAW.DATA_LAST) throw new LinkError('protocol', 'unexpected packet ' + r.type);
        parts.push(r.data);
        await this.rawSend(RAW.ACK, [0xe0, 0x00]);
        if (r.type === RAW.DATA_LAST) break;
      }
      const v = concat(parts);
      const type = num(v.slice(4, 6)), data = v.slice(6);
      if (type === VIRT.DELAY_ACK) { // the calculator is busy (writing to flash): wait as asked
        await sleep(Math.max(10, num(data) / 1000));
        continue;
      }
      if (type === VIRT.ERROR) throw new LinkError('calc', 'the calculator refused (error ' + num(data) + ')');
      return { type, data };
    }
  }

  async expect(type, timeout) {
    const r = await this.recv(timeout);
    if (r.type !== type) throw new LinkError('protocol', 'expected ' + type.toString(16) + ', got ' + r.type.toString(16));
    return r;
  }

  /* commands */

  // answered as soon as the calculator is back on an OS screen: a long timeout waits out a busy one
  async ping(timeout = 30000) {
    await this.send(VIRT.PING, MODE_NORMAL, null, timeout);
    await this.expect(VIRT.MODE_SET, timeout);
  }

  // [{ id, ok, bytes }]
  async params(ids) {
    await this.send(VIRT.PARM_REQ, [...be(ids.length, 2), ...ids.flatMap(id => be(id, 2))]);
    const { data: d } = await this.expect(VIRT.PARM_DATA);
    const out = [];
    let j = 2;
    for (let i = 0, n = num(d.slice(0, 2)); i < n; i++) {
      const id = num(d.slice(j, j + 2)), ok = d[j + 2] === 0;
      j += 3;
      let bytes = new Uint8Array(0);
      if (ok) {
        const size = num(d.slice(j, j + 2));
        bytes = d.slice(j + 2, j + 2 + size);
        j += 2 + size;
      }
      out.push({ id, ok, bytes });
    }
    return out;
  }

  // { product, os: [major, minor, patch, build] | null, osText, freeFlash, freeRam }
  async info() {
    const p = await this.params([PID.PRODUCT_NAME, PID.OS_BUILD, PID.OS_VERSION, PID.FREE_FLASH, PID.FREE_RAM]);
    const get = id => p.find(x => x.id === id && x.ok);
    const name = get(PID.PRODUCT_NAME), build = get(PID.OS_BUILD), ver = get(PID.OS_VERSION);
    let os = null;
    if (ver && ver.bytes.length >= 3) {
      const b = ver.bytes;
      os = [b[1], b[2], b.length >= 4 ? b[3] : 0, build && build.bytes.length === 2 ? num(build.bytes) : 0];
    }
    const flash = get(PID.FREE_FLASH), ram = get(PID.FREE_RAM);
    return {
      product: name ? String.fromCharCode(...Array.from(name.bytes).filter(c => c)) : this.name,
      os,
      osText: os ? `${os[0]}.${os[1]}.${os[2]}` + (os[3] ? '.' + String(os[3]).padStart(4, '0') : '') : 'unknown',
      freeFlash: flash ? num(flash.bytes) : null,
      freeRam: ram ? num(ram.bytes) : null,
    };
  }

  // var: { name, type, data, version, archived }; onBytes(n): bytes of the data sent so far
  async sendVar(v, onBytes) {
    const attrs = [
      ...be(3, 2),
      ...be(0x02, 2), ...be(4, 2), 0xf0, 0x0f, 0x00, v.type, // variable type (CE)
      ...be(0x03, 2), ...be(1, 2), v.archived ? 1 : 0, // archived
      ...be(0x08, 2), ...be(4, 2), 0, 0, 0, v.version || 0, // version
    ];
    await this.send(VIRT.RTS, [...be(v.name.length, 2), ...ascii(v.name), 0, ...be(v.data.length, 4), 1, ...attrs]);
    await this.expect(VIRT.DATA_ACK);
    await this.send(VIRT.VAR_CNTS, v.data, onBytes);
    await this.expect(VIRT.DATA_ACK, 60000); // archived: the calculator writes it to flash first
    await this.send(VIRT.EOT);
  }

  async pressKey(code, timeout = 30000) {
    await this.send(VIRT.EXECUTE, [0, 0, 3, 0, code], null, Math.min(timeout, 10000));
    await this.expect(VIRT.DATA_ACK, timeout);
  }
}

// The variables of a TI-83 Plus family file (.8xp, .8xv): [{ name, type, version, archived, data }]
export function readTIFile(bytes) {
  const sig = String.fromCharCode(...bytes.slice(0, 8));
  if (sig !== '**TI83F*') throw new Error('not a TI-84 Plus file');
  const len = bytes[53] | bytes[54] << 8;
  const out = [];
  for (let p = 55; p < 55 + len;) {
    const head = bytes[p] | bytes[p + 1] << 8; // 11 or 13 (with version and flag)
    const size = bytes[p + 2] | bytes[p + 3] << 8;
    const type = bytes[p + 4];
    let name = '';
    for (let i = 0; i < 8 && bytes[p + 5 + i]; i++) name += String.fromCharCode(bytes[p + 5 + i]);
    const version = head === 13 ? bytes[p + 13] : 0;
    const archived = head === 13 ? (bytes[p + 14] & 0x80) !== 0 : false;
    const start = p + 2 + head + 2; // past the header and the data size (repeated)
    out.push({ name, type, version, archived, data: bytes.slice(start, start + size) });
    p = start + size;
  }
  return out;
}

// The files of a TI bundle (.b84: a zip, deflated), with the browser's own inflater
export async function readBundle(buf) {
  const dv = new DataView(buf);
  let e = buf.byteLength - 22;
  while (e >= 0 && dv.getUint32(e, true) !== 0x06054b50) e--;
  if (e < 0) throw new Error('not a zip file');
  const count = dv.getUint16(e + 10, true);
  let p = dv.getUint32(e + 16, true);
  const files = [];
  for (let i = 0; i < count; i++) {
    const method = dv.getUint16(p + 10, true), csize = dv.getUint32(p + 20, true), usize = dv.getUint32(p + 24, true);
    const nlen = dv.getUint16(p + 28, true), xlen = dv.getUint16(p + 30, true), clen = dv.getUint16(p + 32, true);
    const local = dv.getUint32(p + 42, true);
    const name = new TextDecoder().decode(new Uint8Array(buf, p + 46, nlen));
    p += 46 + nlen + xlen + clen;
    const start = local + 30 + dv.getUint16(local + 26, true) + dv.getUint16(local + 28, true);
    const raw = new Uint8Array(buf, start, csize);
    let bytes;
    if (method === 0) bytes = raw.slice();
    else if (method === 8) {
      const stream = new Blob([raw]).stream().pipeThrough(new DecompressionStream('deflate-raw'));
      bytes = new Uint8Array(await new Response(stream).arrayBuffer());
    } else throw new Error('unsupported zip method ' + method);
    if (bytes.length !== usize) throw new Error('damaged file in the bundle: ' + name);
    files.push({ name, bytes });
  }
  return files;
}

function concat(chunks) {
  const out = new Uint8Array(chunks.reduce((a, c) => a + c.length, 0));
  let o = 0;
  for (const c of chunks) {
    out.set(c, o);
    o += c.length;
  }
  return out;
}

function withTimeout(promise, ms) {
  let t;
  return Promise.race([
    promise,
    new Promise((_, reject) => { t = setTimeout(() => reject(new LinkError('timeout', 'the calculator stopped answering')), ms); }),
  ]).finally(() => clearTimeout(t));
}
