// =============================================================================
// WASM Binary Parser — extracts types, imports, globals, exports, and code
// =============================================================================

export class WasmParser {
  constructor(buffer) {
    this.buf = new Uint8Array(buffer);
    this.pos = 0;
  }

  u8() { return this.buf[this.pos++]; }

  u32() {
    const v = this.buf[this.pos] | (this.buf[this.pos+1]<<8) |
              (this.buf[this.pos+2]<<16) | (this.buf[this.pos+3]<<24);
    this.pos += 4;
    return v >>> 0;
  }

  uleb() {
    let r = 0, s = 0;
    while (true) {
      const b = this.u8();
      r |= (b & 0x7f) << s;
      if (!(b & 0x80)) return r >>> 0;
      s += 7;
    }
  }

  sleb() {
    let r = 0, s = 0, b;
    do { b = this.u8(); r |= (b & 0x7f) << s; s += 7; } while (b & 0x80);
    if (s < 32 && (b & 0x40)) r |= -(1 << s);
    return r;
  }

  bytes(n) { const s = this.buf.slice(this.pos, this.pos + n); this.pos += n; return s; }
  str() { const n = this.uleb(); return new TextDecoder().decode(this.bytes(n)); }

  parse() {
    const magic = this.u32();
    const version = this.u32();
    const R = { types: [], imports: [], functions: [], globals: [], exports: [], codes: [] };

    while (this.pos < this.buf.length) {
      const id = this.u8();
      const size = this.uleb();
      const end = this.pos + size;
      switch (id) {
        case 1: this.#types(R); break;
        case 2: this.#imports(R); break;
        case 3: this.#functions(R); break;
        case 6: this.#globals(R); break;
        case 7: this.#exports(R); break;
        case 10: this.#code(R); break;
      }
      this.pos = end;
    }
    return R;
  }

  #types(R) {
    const n = this.uleb();
    for (let i = 0; i < n; i++) {
      this.u8(); // 0x60 = functype
      const pc = this.uleb(); const params = [];
      for (let j = 0; j < pc; j++) params.push(this.u8());
      const rc = this.uleb(); const results = [];
      for (let j = 0; j < rc; j++) results.push(this.u8());
      R.types.push({ params, results });
    }
  }

  #imports(R) {
    const n = this.uleb();
    for (let i = 0; i < n; i++) {
      const mod = this.str(), name = this.str(), kind = this.u8();
      if (kind === 0) { R.imports.push({ mod, name, kind: 0, typeIdx: this.uleb() }); }
      else if (kind === 1) { this.u8(); const f = this.uleb(); if (f & 1) this.uleb(); R.imports.push({ mod, name, kind: 1 }); }
      else if (kind === 2) { const f = this.uleb(); this.uleb(); if (f & 1) this.uleb(); R.imports.push({ mod, name, kind: 2 }); }
      else if (kind === 3) { this.u8(); this.u8(); R.imports.push({ mod, name, kind: 3 }); }
    }
  }

  #functions(R) {
    const n = this.uleb();
    for (let i = 0; i < n; i++) R.functions.push(this.uleb());
  }

  #globals(R) {
    const n = this.uleb();
    for (let i = 0; i < n; i++) {
      const type = this.u8(), mut = this.u8();
      const initOp = this.u8();
      let initVal = 0;
      if (initOp === 0x41) initVal = this.sleb();
      else if (initOp === 0x43) { initVal = this.u32(); }
      this.u8(); // end
      R.globals.push({ type, mut, initVal });
    }
  }

  #exports(R) {
    const n = this.uleb();
    for (let i = 0; i < n; i++) {
      const name = this.str(), kind = this.u8(), index = this.uleb();
      R.exports.push({ name, kind, index });
    }
  }

  #code(R) {
    const n = this.uleb();
    for (let i = 0; i < n; i++) {
      const bodySize = this.uleb(), bodyEnd = this.pos + bodySize;
      const ldCount = this.uleb();
      let extraLocals = 0;
      const localTypes = [];
      for (let j = 0; j < ldCount; j++) {
        const count = this.uleb();
        const type = this.u8();
        for (let k = 0; k < count; k++) localTypes.push(type);
        extraLocals += count;
      }
      const bodyBytes = this.buf.slice(this.pos, bodyEnd);
      R.codes.push({ extraLocals, localTypes, bodyBytes });
      this.pos = bodyEnd;
    }
  }
}
