// =============================================================================
// WASM → WGSL ahead-of-time transpiler
// Converts WASM bytecode into native WGSL instructions (no interpreter loop)
// =============================================================================

// ---- helpers for reading immediates from bytecode ----

function readLebU(bytes, pc) {
  let r = 0, s = 0;
  while (true) {
    const b = bytes[pc.v++];
    r |= (b & 0x7f) << s;
    if (!(b & 0x80)) return r >>> 0;
    s += 7;
  }
}

function readLebS(bytes, pc) {
  let r = 0, s = 0, b;
  do { b = bytes[pc.v++]; r |= (b & 0x7f) << s; s += 7; } while (b & 0x80);
  if (s < 32 && (b & 0x40)) r |= -(1 << s);
  return r;
}

function readF32(bytes, pc) {
  const buf = new ArrayBuffer(4);
  const u8 = new Uint8Array(buf);
  u8[0] = bytes[pc.v++]; u8[1] = bytes[pc.v++];
  u8[2] = bytes[pc.v++]; u8[3] = bytes[pc.v++];
  return new Float32Array(buf)[0];
}

function formatF32(val) {
  if (Object.is(val, -0)) return '-0.0';
  if (val === Infinity) return '3.40282346638528859812e+38';   // f32 max
  if (val === -Infinity) return '-3.40282346638528859812e+38'; // f32 min
  if (Number.isNaN(val)) return '0.0';                         // NaN → 0
  const s = val.toString();
  if (s.includes('.') || s.includes('e') || s.includes('E')) return s;
  return s + '.0';
}

function wgslType(wasmValType) {
  return wasmValType === 0x7f /* i32 */ ? 'u32' : 'f32';
}

// ---- WASM import → WGSL built-in mapping ----

const WGSL_BUILTINS = {
  // Trigonometric
  sinf: 'sin', cosf: 'cos', tanf: 'tan',
  asinf: 'asin', acosf: 'acos', atanf: 'atan',
  atan2f: 'atan2',
  // Exponential
  expf: 'exp', exp2f: 'exp2',
  logf: 'log', log2f: 'log2',
  powf: 'pow',
  // Min/max (clang may emit calls instead of native f32.min/f32.max)
  fminf: 'min', fmaxf: 'max',
};

// ---- transpile a single function body ----

function transpileBody(bodyBytes, allLocalTypes, globals, funcImports, types) {
  const lines = [];
  const stack = []; // { name: string, type: 'u32' | 'f32' }
  const usedGlobals = new Set(); // Track which globals are used
  let tc = 0;
  const pc = { v: 0 };

  // ---- control flow state ----
  const labelStack = []; // { kind: 'block'|'loop'|'if', label: string, hasElse?: boolean }
  let labelCount = 0;
  let needsCfFlags = false;

  function readBlockType() {
    const bt = bodyBytes[pc.v];
    if (bt === 0x40) { pc.v++; return 'void'; }           // void
    if (bt >= 0x7c && bt <= 0x7f) { pc.v++; return bt; }  // value type
    readLebS(bodyBytes, pc);                                // type index (s33)
    return 'void';
  }

  function tmp(type) {
    const name = `t${tc++}`;
    return { name, type };
  }

  function localT(idx) { return wgslType(allLocalTypes[idx]); }

  function globalT(idx) {
    usedGlobals.add(idx);
    // Get type from globals array if available, default to u32
    if (globals && globals[idx]) {
      return globals[idx].type === 0x7d ? 'f32' : 'u32';
    }
    return 'u32';
  }

  while (pc.v < bodyBytes.length) {
    const op = bodyBytes[pc.v++];

    switch (op) {

      // ---- variables ----

      case 0x20: { // local.get
        const idx = readLebU(bodyBytes, pc);
        stack.push({ name: `l${idx}`, type: localT(idx) });
        break;
      }
      case 0x21: { // local.set
        const idx = readLebU(bodyBytes, pc);
        const val = stack.pop();
        const targetType = localT(idx);
        let valExpr = val.name;
        // Handle type mismatch
        if (val.type !== targetType) {
          if (targetType === 'f32' && val.type === 'u32') {
            valExpr = `bitcast<f32>(${val.name})`;
          } else if (targetType === 'u32' && val.type === 'f32') {
            valExpr = `bitcast<u32>(${val.name})`;
          }
        }
        lines.push(`l${idx} = ${valExpr};`);
        break;
      }
      case 0x22: { // local.tee
        const idx = readLebU(bodyBytes, pc);
        const val = stack[stack.length - 1];
        const targetType = localT(idx);
        let valExpr = val.name;
        // Handle type mismatch
        if (val.type !== targetType) {
          if (targetType === 'f32' && val.type === 'u32') {
            valExpr = `bitcast<f32>(${val.name})`;
          } else if (targetType === 'u32' && val.type === 'f32') {
            valExpr = `bitcast<u32>(${val.name})`;
          }
        }
        lines.push(`l${idx} = ${valExpr};`);
        break;
      }
      case 0x23: { // global.get
        const idx = readLebU(bodyBytes, pc);
        stack.push({ name: `g${idx}`, type: globalT(idx) });
        break;
      }
      case 0x24: { // global.set
        const idx = readLebU(bodyBytes, pc);
        usedGlobals.add(idx);
        const val = stack.pop();
        const targetType = globalT(idx);
        let valExpr = val.name;
        // Handle type mismatch
        if (val.type !== targetType) {
          if (targetType === 'f32' && val.type === 'u32') {
            valExpr = `bitcast<f32>(${val.name})`;
          } else if (targetType === 'u32' && val.type === 'f32') {
            valExpr = `bitcast<u32>(${val.name})`;
          }
        }
        lines.push(`g${idx} = ${valExpr};`);
        break;
      }

      // ---- constants ----

      case 0x41: { // i32.const
        const val = readLebS(bodyBytes, pc);
        const t = tmp('u32');
        lines.push(`let ${t.name}: u32 = ${(val >>> 0)}u;`);
        stack.push(t);
        break;
      }
      case 0x43: { // f32.const
        const val = readF32(bodyBytes, pc);
        const t = tmp('f32');
        lines.push(`let ${t.name}: f32 = ${formatF32(val)};`);
        stack.push(t);
        break;
      }

      // ---- memory ----

      case 0x28: { // i32.load
        readLebU(bodyBytes, pc); const off = readLebU(bodyBytes, pc);
        const addr = stack.pop();
        const t = tmp('u32');
        let addrExpr = addr.type === 'f32' ? `bitcast<u32>(${addr.name})` : addr.name;
        lines.push(`let ${t.name}: u32 = mem[(${addrExpr} + ${off}u) / 4u];`);
        stack.push(t);
        break;
      }
      case 0x2a: { // f32.load
        readLebU(bodyBytes, pc); const off = readLebU(bodyBytes, pc);
        const addr = stack.pop();
        const t = tmp('f32');
        let addrExpr = addr.type === 'f32' ? `bitcast<u32>(${addr.name})` : addr.name;
        lines.push(`let ${t.name}: f32 = bitcast<f32>(mem[(${addrExpr} + ${off}u) / 4u]);`);
        stack.push(t);
        break;
      }
      case 0x36: { // i32.store
        readLebU(bodyBytes, pc); const off = readLebU(bodyBytes, pc);
        const val = stack.pop(); const addr = stack.pop();
        let addrExpr = addr.type === 'f32' ? `bitcast<u32>(${addr.name})` : addr.name;
        let valExpr = val.type === 'f32' ? `bitcast<u32>(${val.name})` : val.name;
        lines.push(`mem[(${addrExpr} + ${off}u) / 4u] = ${valExpr};`);
        break;
      }
      case 0x38: { // f32.store
        readLebU(bodyBytes, pc); const off = readLebU(bodyBytes, pc);
        const val = stack.pop(); const addr = stack.pop();
        let addrExpr = addr.type === 'f32' ? `bitcast<u32>(${addr.name})` : addr.name;
        let valExpr = val.type === 'u32' ? `bitcast<f32>(${val.name})` : val.name;
        lines.push(`mem[(${addrExpr} + ${off}u) / 4u] = bitcast<u32>(${valExpr});`);
        break;
      }

      // ---- parametric ----

      case 0x1a: { stack.pop(); break; } // drop
      case 0x1b: { // select
        const cond = stack.pop();
        const val2 = stack.pop();
        const val1 = stack.pop();
        const t = tmp(val1.type);
        // Ensure both values have the same type for WGSL select
        let v1 = val1.name;
        let v2 = val2.name;
        if (val1.type !== val2.type) {
          // Type mismatch - coerce to val1's type
          if (val1.type === 'f32' && val2.type === 'u32') {
            v2 = `bitcast<f32>(${val2.name})`;
          } else if (val1.type === 'u32' && val2.type === 'f32') {
            v2 = `bitcast<u32>(${val2.name})`;
          }
        }
        lines.push(`let ${t.name}: ${t.type} = select(${v2}, ${v1}, ${cond.name} != 0u);`);
        stack.push(t);
        break;
      }

      // ---- i32 comparison ----

      case 0x45: { // i32.eqz
        const a = stack.pop();
        const t = tmp('u32');
        let aExpr = a.type === 'f32' ? `bitcast<u32>(${a.name})` : a.name;
        lines.push(`let ${t.name}: u32 = select(0u, 1u, ${aExpr} == 0u);`);
        stack.push(t);
        break;
      }
      case 0x46: case 0x47: case 0x48: case 0x49:
      case 0x4a: case 0x4b: case 0x4c: case 0x4d:
      case 0x4e: case 0x4f: {
        const b = stack.pop(); const a = stack.pop();
        const t = tmp('u32');
        // Ensure both operands are u32
        let aExpr = a.type === 'f32' ? `bitcast<u32>(${a.name})` : a.name;
        let bExpr = b.type === 'f32' ? `bitcast<u32>(${b.name})` : b.name;
        const cmpOps = {
          0x46: [`${aExpr}`, '==', `${bExpr}`],
          0x47: [`${aExpr}`, '!=', `${bExpr}`],
          0x48: [`bitcast<i32>(${aExpr})`, '<',  `bitcast<i32>(${bExpr})`],
          0x49: [`${aExpr}`, '<',  `${bExpr}`],
          0x4a: [`bitcast<i32>(${aExpr})`, '>',  `bitcast<i32>(${bExpr})`],
          0x4b: [`${aExpr}`, '>',  `${bExpr}`],
          0x4c: [`bitcast<i32>(${aExpr})`, '<=', `bitcast<i32>(${bExpr})`],
          0x4d: [`${aExpr}`, '<=', `${bExpr}`],
          0x4e: [`bitcast<i32>(${aExpr})`, '>=', `bitcast<i32>(${bExpr})`],
          0x4f: [`${aExpr}`, '>=', `${bExpr}`],
        };
        const [la, o, lb] = cmpOps[op];
        lines.push(`let ${t.name}: u32 = select(0u, 1u, ${la} ${o} ${lb});`);
        stack.push(t);
        break;
      }

      // ---- f32 comparison ----

      case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f: case 0x60: {
        const b = stack.pop(); const a = stack.pop();
        const ops = { 0x5b:'==', 0x5c:'!=', 0x5d:'<', 0x5e:'>', 0x5f:'<=', 0x60:'>=' };
        const t = tmp('u32');
        // Ensure both operands are f32
        let aExpr = a.type === 'u32' ? `bitcast<f32>(${a.name})` : a.name;
        let bExpr = b.type === 'u32' ? `bitcast<f32>(${b.name})` : b.name;
        lines.push(`let ${t.name}: u32 = select(0u, 1u, ${aExpr} ${ops[op]} ${bExpr});`);
        stack.push(t);
        break;
      }

      // ---- i32 arithmetic ----

      case 0x6a: case 0x6b: case 0x6c: {
        const ops = { 0x6a: '+', 0x6b: '-', 0x6c: '*' };
        const b = stack.pop(); const a = stack.pop();
        const t = tmp('u32');
        // Ensure both operands are u32
        let aExpr = a.type === 'f32' ? `bitcast<u32>(${a.name})` : a.name;
        let bExpr = b.type === 'f32' ? `bitcast<u32>(${b.name})` : b.name;
        lines.push(`let ${t.name}: u32 = ${aExpr} ${ops[op]} ${bExpr};`);
        stack.push(t);
        break;
      }
      case 0x6d: { // i32.div_s
        const b = stack.pop(); const a = stack.pop();
        const t = tmp('u32');
        let aExpr = a.type === 'f32' ? `bitcast<u32>(${a.name})` : a.name;
        let bExpr = b.type === 'f32' ? `bitcast<u32>(${b.name})` : b.name;
        lines.push(`let ${t.name}: u32 = bitcast<u32>(bitcast<i32>(${aExpr}) / bitcast<i32>(${bExpr}));`);
        stack.push(t);
        break;
      }
      case 0x6f: { // i32.rem_s
        const b = stack.pop(); const a = stack.pop();
        const t = tmp('u32');
        let aExpr = a.type === 'f32' ? `bitcast<u32>(${a.name})` : a.name;
        let bExpr = b.type === 'f32' ? `bitcast<u32>(${b.name})` : b.name;
        lines.push(`let ${t.name}: u32 = bitcast<u32>(bitcast<i32>(${aExpr}) % bitcast<i32>(${bExpr}));`);
        stack.push(t);
        break;
      }
      case 0x71: { // i32.and
        const b = stack.pop(); const a = stack.pop();
        const t = tmp('u32');
        let aExpr = a.type === 'f32' ? `bitcast<u32>(${a.name})` : a.name;
        let bExpr = b.type === 'f32' ? `bitcast<u32>(${b.name})` : b.name;
        lines.push(`let ${t.name}: u32 = ${aExpr} & ${bExpr};`);
        stack.push(t);
        break;
      }
      case 0x72: { // i32.or
        const b = stack.pop(); const a = stack.pop();
        const t = tmp('u32');
        let aExpr = a.type === 'f32' ? `bitcast<u32>(${a.name})` : a.name;
        let bExpr = b.type === 'f32' ? `bitcast<u32>(${b.name})` : b.name;
        lines.push(`let ${t.name}: u32 = ${aExpr} | ${bExpr};`);
        stack.push(t);
        break;
      }
      case 0x73: { // i32.xor
        const b = stack.pop(); const a = stack.pop();
        const t = tmp('u32');
        let aExpr = a.type === 'f32' ? `bitcast<u32>(${a.name})` : a.name;
        let bExpr = b.type === 'f32' ? `bitcast<u32>(${b.name})` : b.name;
        lines.push(`let ${t.name}: u32 = ${aExpr} ^ ${bExpr};`);
        stack.push(t);
        break;
      }
      case 0x74: { // i32.shl
        const b = stack.pop(); const a = stack.pop();
        const t = tmp('u32');
        let aExpr = a.type === 'f32' ? `bitcast<u32>(${a.name})` : a.name;
        let bExpr = b.type === 'f32' ? `bitcast<u32>(${b.name})` : b.name;
        lines.push(`let ${t.name}: u32 = ${aExpr} << (${bExpr} & 31u);`);
        stack.push(t);
        break;
      }
      case 0x75: { // i32.shr_s
        const b = stack.pop(); const a = stack.pop();
        const t = tmp('u32');
        let aExpr = a.type === 'f32' ? `bitcast<u32>(${a.name})` : a.name;
        let bExpr = b.type === 'f32' ? `bitcast<u32>(${b.name})` : b.name;
        lines.push(`let ${t.name}: u32 = bitcast<u32>(bitcast<i32>(${aExpr}) >> (${bExpr} & 31u));`);
        stack.push(t);
        break;
      }
      case 0x76: { // i32.shr_u
        const b = stack.pop(); const a = stack.pop();
        const t = tmp('u32');
        let aExpr = a.type === 'f32' ? `bitcast<u32>(${a.name})` : a.name;
        let bExpr = b.type === 'f32' ? `bitcast<u32>(${b.name})` : b.name;
        lines.push(`let ${t.name}: u32 = ${aExpr} >> (${bExpr} & 31u);`);
        stack.push(t);
        break;
      }

      // ---- f32 unary ----

      case 0x8b: { const v = stack.pop(); const t = tmp('f32'); const vE = v.type === 'u32' ? `bitcast<f32>(${v.name})` : v.name; lines.push(`let ${t.name}: f32 = abs(${vE});`);   stack.push(t); break; }
      case 0x8c: { const v = stack.pop(); const t = tmp('f32'); const vE = v.type === 'u32' ? `bitcast<f32>(${v.name})` : v.name; lines.push(`let ${t.name}: f32 = -(${vE});`);     stack.push(t); break; }
      case 0x8d: { const v = stack.pop(); const t = tmp('f32'); const vE = v.type === 'u32' ? `bitcast<f32>(${v.name})` : v.name; lines.push(`let ${t.name}: f32 = ceil(${vE});`);  stack.push(t); break; }
      case 0x8e: { const v = stack.pop(); const t = tmp('f32'); const vE = v.type === 'u32' ? `bitcast<f32>(${v.name})` : v.name; lines.push(`let ${t.name}: f32 = floor(${vE});`); stack.push(t); break; }
      case 0x8f: { const v = stack.pop(); const t = tmp('f32'); const vE = v.type === 'u32' ? `bitcast<f32>(${v.name})` : v.name; lines.push(`let ${t.name}: f32 = trunc(${vE});`); stack.push(t); break; }
      case 0x90: { const v = stack.pop(); const t = tmp('f32'); const vE = v.type === 'u32' ? `bitcast<f32>(${v.name})` : v.name; lines.push(`let ${t.name}: f32 = round(${vE});`); stack.push(t); break; }
      case 0x91: { const v = stack.pop(); const t = tmp('f32'); const vE = v.type === 'u32' ? `bitcast<f32>(${v.name})` : v.name; lines.push(`let ${t.name}: f32 = sqrt(${vE});`);  stack.push(t); break; }

      // ---- f32 binary ----

      case 0x92: case 0x93: case 0x94: case 0x95: {
        const ops = { 0x92: '+', 0x93: '-', 0x94: '*', 0x95: '/' };
        const b = stack.pop(); const a = stack.pop();
        const t = tmp('f32');
        // Ensure both operands are f32
        let aExpr = a.type === 'u32' ? `bitcast<f32>(${a.name})` : a.name;
        let bExpr = b.type === 'u32' ? `bitcast<f32>(${b.name})` : b.name;
        lines.push(`let ${t.name}: f32 = ${aExpr} ${ops[op]} ${bExpr};`);
        stack.push(t);
        break;
      }
      case 0x96: { // f32.min
        const b = stack.pop(); const a = stack.pop();
        const t = tmp('f32');
        let aExpr = a.type === 'u32' ? `bitcast<f32>(${a.name})` : a.name;
        let bExpr = b.type === 'u32' ? `bitcast<f32>(${b.name})` : b.name;
        lines.push(`let ${t.name}: f32 = min(${aExpr}, ${bExpr});`);
        stack.push(t);
        break;
      }
      case 0x97: { // f32.max
        const b = stack.pop(); const a = stack.pop();
        const t = tmp('f32');
        let aExpr = a.type === 'u32' ? `bitcast<f32>(${a.name})` : a.name;
        let bExpr = b.type === 'u32' ? `bitcast<f32>(${b.name})` : b.name;
        lines.push(`let ${t.name}: f32 = max(${aExpr}, ${bExpr});`);
        stack.push(t);
        break;
      }

      // ---- conversions ----

      case 0xa8: { // i32.trunc_f32_s
        const v = stack.pop(); const t = tmp('u32');
        if (v.type === 'f32') {
          lines.push(`let ${t.name}: u32 = bitcast<u32>(i32(trunc(${v.name})));`);
        } else {
          // Already an integer, just do signed reinterpret
          lines.push(`let ${t.name}: u32 = ${v.name};`);
        }
        stack.push(t);
        break;
      }
      case 0xa9: { // i32.trunc_f32_u
        const v = stack.pop(); const t = tmp('u32');
        if (v.type === 'f32') {
          lines.push(`let ${t.name}: u32 = u32(trunc(${v.name}));`);
        } else {
          lines.push(`let ${t.name}: u32 = ${v.name};`);
        }
        stack.push(t);
        break;
      }
      case 0xb2: { // f32.convert_i32_s
        const v = stack.pop(); const t = tmp('f32');
        if (v.type === 'u32') {
          lines.push(`let ${t.name}: f32 = f32(bitcast<i32>(${v.name}));`);
        } else {
          // Already f32
          lines.push(`let ${t.name}: f32 = ${v.name};`);
        }
        stack.push(t);
        break;
      }
      case 0xb3: { // f32.convert_i32_u
        const v = stack.pop(); const t = tmp('f32');
        if (v.type === 'u32') {
          lines.push(`let ${t.name}: f32 = f32(${v.name});`);
        } else {
          lines.push(`let ${t.name}: f32 = ${v.name};`);
        }
        stack.push(t);
        break;
      }
      case 0xbc: { // i32.reinterpret_f32
        const v = stack.pop();
        const t = tmp('u32');
        if (v.type === 'f32') {
          lines.push(`let ${t.name}: u32 = bitcast<u32>(${v.name});`);
        } else {
          lines.push(`let ${t.name}: u32 = ${v.name};`);
        }
        stack.push(t);
        break;
      }
      case 0xbe: { // f32.reinterpret_i32
        const v = stack.pop();
        const t = tmp('f32');
        if (v.type === 'u32') {
          lines.push(`let ${t.name}: f32 = bitcast<f32>(${v.name});`);
        } else {
          lines.push(`let ${t.name}: f32 = ${v.name};`);
        }
        stack.push(t);
        break;
      }

      // ---- 0xFC prefix (saturating truncations) ----

      case 0xfc: {
        const sub = readLebU(bodyBytes, pc);
        if (sub === 0) { // i32.trunc_sat_f32_s
          const v = stack.pop(); const t = tmp('u32');
          if (v.type === 'f32') {
            lines.push(`let ${t.name}: u32 = bitcast<u32>(i32(trunc(${v.name})));`);
          } else {
            lines.push(`let ${t.name}: u32 = ${v.name};`);
          }
          stack.push(t);
        } else if (sub === 1) { // i32.trunc_sat_f32_u
          const v = stack.pop(); const t = tmp('u32');
          if (v.type === 'f32') {
            lines.push(`let ${t.name}: u32 = u32(trunc(${v.name}));`);
          } else {
            lines.push(`let ${t.name}: u32 = ${v.name};`);
          }
          stack.push(t);
        }
        break;
      }

      // ---- call (import → WGSL built-in) ----

      case 0x10: { // call
        const funcIdx = readLebU(bodyBytes, pc);
        const numImports = funcImports ? funcImports.length : 0;
        if (funcIdx < numImports) {
          const imp = funcImports[funcIdx];
          const wgslName = WGSL_BUILTINS[imp.name];
          const funcType = types[imp.typeIdx];
          if (wgslName) {
            const args = [];
            for (let j = 0; j < funcType.params.length; j++) {
              args.unshift(stack.pop());
            }
            const argStr = args.map(a =>
              a.type === 'u32' ? `bitcast<f32>(${a.name})` : a.name
            ).join(', ');
            if (funcType.results.length > 0) {
              const t = tmp('f32');
              lines.push(`let ${t.name}: f32 = ${wgslName}(${argStr});`);
              stack.push(t);
            } else {
              lines.push(`${wgslName}(${argStr});`);
            }
          } else {
            console.warn(`Transpiler: unknown import "${imp.name}" at funcIdx ${funcIdx}`);
            const funcType2 = types[imp.typeIdx];
            for (let j = 0; j < funcType2.params.length; j++) stack.pop();
            if (funcType2.results.length > 0) {
              const t = tmp('f32');
              lines.push(`let ${t.name}: f32 = 0.0; // unknown import: ${imp.name}`);
              stack.push(t);
            }
          }
        } else {
          console.warn(`Transpiler: call to local function ${funcIdx} not supported`);
        }
        break;
      }

      // ---- control flow ----

      case 0x02: { // block
        readBlockType();
        const label = `blk${labelCount++}`;
        labelStack.push({ kind: 'block', label });
        lines.push(`loop { // ${label}`);
        break;
      }
      case 0x03: { // loop
        readBlockType();
        const label = `lp${labelCount++}`;
        labelStack.push({ kind: 'loop', label });
        lines.push(`loop { // ${label}`);
        break;
      }
      case 0x04: { // if
        readBlockType();
        const cond = stack.pop();
        const label = `if${labelCount++}`;
        let condExpr = cond.type === 'f32' ? `bitcast<u32>(${cond.name})` : cond.name;
        labelStack.push({ kind: 'if', label, hasElse: false });
        lines.push(`loop { // ${label}`);
        lines.push(`if ${condExpr} != 0u {`);
        break;
      }
      case 0x05: { // else
        const top = labelStack[labelStack.length - 1];
        top.hasElse = true;
        lines.push(`} else {`);
        break;
      }
      case 0x0b: { // end
        if (labelStack.length === 0) break; // end of function
        const entry = labelStack.pop();
        if (entry.kind === 'if') {
          lines.push(`}`); // close if or else
          lines.push(`break; // end ${entry.label}`);
          lines.push(`}`); // close loop wrapper
        } else {
          lines.push(`break; // end ${entry.label}`);
          lines.push(`}`); // close loop
        }
        // Flag check for multi-level br propagation
        if (needsCfFlags && labelStack.length > 0) {
          lines.push(`if cf_exit > 0u { cf_exit = cf_exit - 1u; if cf_exit == 0u && cf_cont == 1u { continue; } else { break; } }`);
        }
        break;
      }
      case 0x0c: { // br
        const depth = readLebU(bodyBytes, pc);
        const target = labelStack[labelStack.length - 1 - depth];
        if (depth === 0) {
          lines.push(target.kind === 'loop' ? `continue;` : `break;`);
        } else {
          needsCfFlags = true;
          lines.push(`cf_exit = ${depth}u; cf_cont = ${target.kind === 'loop' ? '1u' : '0u'}; break;`);
        }
        break;
      }
      case 0x0d: { // br_if
        const depth = readLebU(bodyBytes, pc);
        const cond = stack.pop();
        const target = labelStack[labelStack.length - 1 - depth];
        let condExpr = cond.type === 'f32' ? `bitcast<u32>(${cond.name})` : cond.name;
        if (depth === 0) {
          lines.push(`if ${condExpr} != 0u { ${target.kind === 'loop' ? 'continue' : 'break'}; }`);
        } else {
          needsCfFlags = true;
          lines.push(`if ${condExpr} != 0u { cf_exit = ${depth}u; cf_cont = ${target.kind === 'loop' ? '1u' : '0u'}; break; }`);
        }
        break;
      }
      case 0x0f: { // return
        lines.push(`return;`);
        break;
      }
      case 0x00: break; // unreachable
      case 0x01: break; // nop

      default:
        console.warn(`Transpiler: unhandled opcode 0x${op.toString(16)} at offset ${pc.v - 1}`);
    }
  }

  return { lines, usedGlobals, needsCfFlags };
}

// ---- generate the complete compute shader ----

export function generateComputeShader(wasm) {
  const mainExport = wasm.exports.find(e => e.name === 'mainImage' && e.kind === 0);
  if (!mainExport) throw new Error('No mainImage export found');
  const mainFuncIdx = mainExport.index;

  const numImportedFuncs = wasm.imports.filter(i => i.kind === 0).length;
  const codeIdx = mainFuncIdx - numImportedFuncs;
  const entry = wasm.codes[codeIdx];
  const typeIdx = wasm.functions[codeIdx];
  const type = wasm.types[typeIdx];

  const allLocalTypes = [...type.params, ...entry.localTypes];
  const funcImports = wasm.imports.filter(i => i.kind === 0);
  const { lines: bodyLines, usedGlobals, needsCfFlags } = transpileBody(entry.bodyBytes, allLocalTypes, wasm.globals, funcImports, wasm.types);

  // Declare global variables that are used
  const globalDecls = Array.from(usedGlobals).sort((a, b) => a - b).map(idx => {
    // Get initial value from wasm.globals if available
    let initValStr = '65536u'; // Default stack pointer value
    let globalType = 'u32';
    if (wasm.globals && wasm.globals[idx]) {
      const g = wasm.globals[idx];
      globalType = g.type === 0x7d ? 'f32' : 'u32';
      if (g.initVal !== undefined) {
        initValStr = globalType === 'f32' ? `${g.initVal}` : `${g.initVal >>> 0}u`;
      }
    }
    return `  var g${idx}: ${globalType} = ${initValStr};`;
  }).join('\n');

  // Declare local variables with proper types
  const localDecls = allLocalTypes.map((t, i) => {
    const wt = wgslType(t);
    const init = wt === 'u32' ? '0u' : '0.0';
    return `  var l${i}: ${wt} = ${init};`;
  }).join('\n');

  // Control flow flag variables (for multi-level br propagation)
  const cfDecls = needsCfFlags
    ? '  var cf_exit: u32 = 0u;\n  var cf_cont: u32 = 0u;\n'
    : '';

  const body = bodyLines.map(l => '  ' + l).join('\n');

  return `struct Uniforms {
  time: f32,
  width: f32,
  height: f32,
  pad: u32,
}

@group(0) @binding(0) var<storage, read_write> output: array<f32>;
@group(0) @binding(1) var<uniform> uniforms: Uniforms;

@compute @workgroup_size(8, 8)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
  let px = gid.x;
  let py = gid.y;
  let W = u32(uniforms.width);
  let H = u32(uniforms.height);
  if (px >= W || py >= H) { return; }

  var mem: array<u32, 4096>;

  // Global variables (WASM globals, e.g., stack pointer)
${globalDecls}

  // Local variables (from WASM function signature + body)
${localDecls}
${cfDecls}
  // Initialize mainImage parameters
  l0 = 0u;                     // output pointer
  l1 = f32(px) + 0.5;         // fragCoordX
  l2 = uniforms.height - f32(py) - 0.5;  // fragCoordY (flip Y)
  l3 = uniforms.width;        // iResolutionX
  l4 = uniforms.height;       // iResolutionY
  l5 = uniforms.time;         // iTime

  // --- transpiled WASM bytecode (native WGSL, no interpreter) ---
${body}

  // Write output from mem[0..3]
  let oidx = (py * W + px) * 4u;
  output[oidx]      = bitcast<f32>(mem[0]);
  output[oidx + 1u] = bitcast<f32>(mem[1]);
  output[oidx + 2u] = bitcast<f32>(mem[2]);
  output[oidx + 3u] = bitcast<f32>(mem[3]);
}
`;
}
