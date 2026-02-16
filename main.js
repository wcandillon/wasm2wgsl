import { WasmParser } from './wasm-parser.js';
import { generateComputeShader } from './transpiler.js';
import { initGPU, startRenderLoop } from './gpu.js';

const infoEl = document.getElementById('info');

try {
  // 1. Load and parse WASM binary
  const params = new URLSearchParams(location.search);
  const wasmFile = params.get('wasm') || 'examples/shader.wasm';
  const response = await fetch(wasmFile);
  const wasmBuffer = await response.arrayBuffer();
  const wasm = new WasmParser(wasmBuffer).parse();

  // 2. Transpile WASM → native WGSL (no interpreter!)
  const computeSrc = generateComputeShader(wasm);
  const lineCount = computeSrc.split('\n').length;

  console.log('=== Generated WGSL compute shader ===');
  console.log(computeSrc);

  const canvas = document.getElementById('canvas');
  canvas.width = canvas.clientWidth * devicePixelRatio;
  canvas.height = canvas.clientHeight * devicePixelRatio;
  infoEl.textContent =
    `Loaded ${wasmBuffer.byteLength} bytes of WASM | ` +
    `Transpiled to ${lineCount} lines of native WGSL | ` +
    `No interpreter loop — pure GPU instructions | ` +
    `${canvas.width}x${canvas.height} = ${(canvas.width * canvas.height).toLocaleString()} pixels/frame`;

  // 3. Initialise WebGPU with the generated shader
  const gpu = await initGPU(canvas, computeSrc);

  // 4. Go
  startRenderLoop(gpu);

} catch (e) {
  console.error(e);
  infoEl.textContent = `Error: ${e.message}`;
}
