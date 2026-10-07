const be = (n, len) => Array.from({ length: len }, (_, i) => Math.floor(n / 2 ** (8 * (len - 1 - i))) & 255);
const num = b => Array.from(b).reduce((a, x) => a * 256 + x, 0);
class FakeCE {
  constructor({ bufSize = 1023, packetSize = 64, delayEvery = 7, os = [5, 8, 0, 22], freeFlash = 3014656, files = [] } = {}) {
    Object.assign(this, { bufSize, packetSize, delayEvery, os, freeFlash, files }); // files: { name, type, size, archived, kept }
    this.deleted = [];
    this.opened = false;
    this.configuration = null;
    this.out = [];          // USB packets waiting for transferIn
    this.vparts = [];       // raw data chunks of the virtual packet being received
    this.vars = [];         // received variables
    this.keys = [];
    this.pending = null;    // the variable announced by RTS
    this.n = 0;
    this.awaitAck = 0;      // our raw data packets the host must ack
  }
  async open() { this.opened = true; }
  async selectConfiguration() {
    this.configuration = { interfaces: [{ interfaceNumber: 0, alternates: [{ endpoints: [
      { direction: 'in', endpointNumber: 1, packetSize: this.packetSize },
      { direction: 'out', endpointNumber: 2, packetSize: this.packetSize }] }] }] };
  }
  async claimInterface() {}
  async releaseInterface() {}
  async close() {}

  // the calculator sends a raw packet, cut in USB packets of packetSize
  emitRaw(type, data) {
    const p = [...be(data.length, 4), type, ...data];
    for (let i = 0; i < p.length; i += this.packetSize) this.out.push(new Uint8Array(p.slice(i, i + this.packetSize)));
  }
  emitVirt(type, data = []) {
    const v = [...be(data.length, 4), ...be(type, 2), ...data];
    this.emitRaw(4, v); // fits in one raw packet here
    this.awaitAck++;
  }

  async transferIn(ep, len) {
    if (!this.out.length) throw new Error('host read with nothing to read (would hang)');
    const d = this.out.shift();
    return { status: 'ok', data: new DataView(d.buffer) };
  }

  async transferOut(ep, bytes) {
    const b = Array.from(bytes);
    const size = num(b.slice(0, 4)), type = b[4], data = b.slice(5);
    if (data.length !== size) throw new Error(`raw packet size field ${size} but ${data.length} bytes`);
    if (type === 1) { this.emitRaw(2, be(this.bufSize, 4)); return { status: 'ok' }; }   // buffer size
    if (type === 5) { if (!this.awaitAck--) throw new Error('unexpected ack'); return { status: 'ok' }; }
    if (type !== 3 && type !== 4) throw new Error('unexpected raw type ' + type);
    if (b.length > this.bufSize + 5) throw new Error(`raw packet of ${b.length} bytes > buffer ${this.bufSize}`);
    this.vparts.push(...data);
    this.emitRaw(5, [0xe0, 0x00]); // raw ack
    if (type === 4) { const v = this.vparts; this.vparts = []; this.onVirt(num(v.slice(4, 6)), v.slice(6), num(v.slice(0, 4))); }
    return { status: 'ok' };
  }

  onVirt(type, d, size) {
    if (d.length !== size) throw new Error(`virtual packet size ${size} but ${d.length} bytes`);
    // "wait 5 ms" before some answers (EOT gets no answer at all)
    if (type !== 0xdd00 && ++this.n % this.delayEvery === 0) this.emitVirt(0xbb00, be(5000, 4));
    switch (type) {
      case 0x0001: // ping
        if (d.join() !== [0, 3, 0, 1, 0, 0, 0, 0, 7, 0xd0].join()) throw new Error('bad ping mode');
        return this.emitVirt(0x0012, d);
      case 0x0007: { // parameter request
        const ids = []; for (let i = 0, n = num(d.slice(0, 2)); i < n; i++) ids.push(num(d.slice(2 + 2 * i, 4 + 2 * i)));
        const val = {
          0x0002: [...Array.from('TI-84 Plus CE', c => c.charCodeAt(0)), 0],
          0x0048: be(this.os[3], 2),
          0x000b: [0, this.os[0], this.os[1], this.os[2]],
          0x0011: be(this.freeFlash, 8),
        };
        const out = [...be(ids.length, 2)];
        for (const id of ids) out.push(...be(id, 2), ...(val[id] ? [0, ...be(val[id].length, 2), ...val[id]] : [1]));
        return this.emitVirt(0x0008, out);
      }
      case 0x000b: { // RTS
        const nl = num(d.slice(0, 2)), name = String.fromCharCode(...d.slice(2, 2 + nl));
        if (d[2 + nl] !== 0) throw new Error('name not followed by 0');
        let j = 3 + nl;
        const vsize = num(d.slice(j, j + 4)); j += 4;
        if (d[j++] !== 1) throw new Error('not silent mode');
        const nat = num(d.slice(j, j + 2)); j += 2;
        const at = {};
        for (let i = 0; i < nat; i++) { const id = num(d.slice(j, j + 2)), s = num(d.slice(j + 2, j + 4)); at[id] = d.slice(j + 4, j + 4 + s); j += 4 + s; }
        if (j !== d.length) throw new Error('RTS has trailing bytes');
        if (at[2].join() !== [0xf0, 0x0f, 0x00, at[2][3]].join()) throw new Error('bad CE type attribute');
        this.pending = { name, size: vsize, type: at[2][3], archived: at[3][0] === 1, version: at[8][3] };
        return this.emitVirt(0xaa00, [0, 1]);
      }
      case 0x000d: // variable contents
        if (!this.pending) throw new Error('contents without RTS');
        if (d.length !== this.pending.size) throw new Error(`announced ${this.pending.size}, got ${d.length}`);
        this.vars.push({ ...this.pending, data: d });
        this.pending = null;
        return this.emitVirt(0xaa00, [0, 1]);
      case 0x0009: { // directory listing (tilibs dusb_cmd_s_dirlist_request: size, type, archived)
        if (d.join() !== [0, 0, 0, 3, 0, 1, 0, 2, 0, 3, 0, 1, 0, 1, 0, 1, 1].join()) throw new Error('bad dir request ' + d.join());
        for (const f of this.files) {
          const nm = Array.from(f.name, c => c.charCodeAt(0));
          this.emitVirt(0x000a, [0, nm.length, ...nm, 0, 0, 3,
            0, 1, 0, 0, 4, ...be(f.size, 4), 0, 2, 0, 0, 4, 0xf0, 0x0f, 0x00, f.type, 0, 3, 0, 0, 1, f.archived ? 1 : 0]);
        }
        return this.emitVirt(0xdd00);
      }
      case 0x0010: { // modify variable with no new name: delete (tilibs calc_84p del_var)
        const nl = d[1], name = String.fromCharCode(...d.slice(2, 2 + nl)), t = d[12 + nl];
        const want = [0, nl, ...d.slice(2, 2 + nl), 0, 0, 1, 0, 0x11, 0, 4, 0xf0, 0x0b, 0x00, t, 1, 0, 0, 0, 0];
        if (d.join() !== want.join()) throw new Error('bad delete packet ' + d.join());
        const i = this.files.findIndex(f => f.name === name && f.type === t);
        if (i < 0) return this.emitVirt(0xee00, [0, 0x0e]);
        this.freeFlash += this.files[i].size;
        this.files.splice(i, 1);
        this.deleted.push(name);
        return this.emitVirt(0xaa00, [0, 1]);
      }
      case 0xdd00: return; // EOT: no answer
      case 0x0011: // execute
        if (d.slice(0, 4).join() !== [0, 0, 3, 0].join()) throw new Error('bad execute');
        this.keys.push(d[4]);
        return this.emitVirt(0xaa00, [0, 1]);
      default: throw new Error('unknown virtual type ' + type.toString(16));
    }
  }
}
window.FakeCE = FakeCE;
