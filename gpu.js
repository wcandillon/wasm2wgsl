// =============================================================================
// WebGPU init + render loop — uses dynamically generated compute shader
// =============================================================================

async function loadShader(url) {
  const res = await fetch(url);
  return res.text();
}

export async function initGPU(canvas, computeSrc) {
  const width = canvas.width;
  const height = canvas.height;

  if (!navigator.gpu) throw new Error('WebGPU not supported');
  const adapter = await navigator.gpu.requestAdapter();
  if (!adapter) throw new Error('No WebGPU adapter found');
  const device = await adapter.requestDevice();
  const ctx = canvas.getContext('webgpu');
  const format = navigator.gpu.getPreferredCanvasFormat();
  ctx.configure({ device, format, alphaMode: 'opaque' });

  const renderSrc = await loadShader('render.wgsl');

  // Only two buffers needed — no bytecode, no targets, no function table
  const outputBuffer = device.createBuffer({
    size: width * height * 4 * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
  });

  const uniformBuffer = device.createBuffer({
    size: 16,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
  });

  // Compute pipeline (from transpiled WGSL)
  const computeModule = device.createShaderModule({ code: computeSrc });
  const computePipeline = device.createComputePipeline({
    layout: 'auto',
    compute: { module: computeModule, entryPoint: 'main' },
  });

  // Render pipeline
  const renderModule = device.createShaderModule({ code: renderSrc });
  const renderPipeline = device.createRenderPipeline({
    layout: 'auto',
    vertex: { module: renderModule, entryPoint: 'vs' },
    fragment: { module: renderModule, entryPoint: 'fs', targets: [{ format }] },
    primitive: { topology: 'triangle-list' },
  });

  const computeBindGroup = device.createBindGroup({
    layout: computePipeline.getBindGroupLayout(0),
    entries: [
      { binding: 0, resource: { buffer: outputBuffer } },
      { binding: 1, resource: { buffer: uniformBuffer } },
    ],
  });

  const renderBindGroup = device.createBindGroup({
    layout: renderPipeline.getBindGroupLayout(0),
    entries: [
      { binding: 0, resource: { buffer: outputBuffer } },
      { binding: 1, resource: { buffer: uniformBuffer } },
    ],
  });

  return {
    device, ctx, uniformBuffer,
    computePipeline, computeBindGroup,
    renderPipeline, renderBindGroup,
    width, height,
  };
}

export function startRenderLoop(gpu) {
  const {
    device, ctx, uniformBuffer,
    computePipeline, computeBindGroup,
    renderPipeline, renderBindGroup,
    width, height,
  } = gpu;

  let lastTime = performance.now();
  let frameCount = 0;
  let fps = 0;

  async function render() {
    const time = performance.now() / 1000;
    const frameStart = performance.now();

    const buf = new ArrayBuffer(16);
    const fv = new Float32Array(buf);
    const uv = new Uint32Array(buf);
    fv[0] = time;
    fv[1] = width;
    fv[2] = height;
    uv[3] = 0;
    device.queue.writeBuffer(uniformBuffer, 0, buf);

    const encoder = device.createCommandEncoder();

    const computePass = encoder.beginComputePass();
    computePass.setPipeline(computePipeline);
    computePass.setBindGroup(0, computeBindGroup);
    computePass.dispatchWorkgroups(Math.ceil(width / 8), Math.ceil(height / 8));
    computePass.end();

    const renderPass = encoder.beginRenderPass({
      colorAttachments: [{
        view: ctx.getCurrentTexture().createView(),
        clearValue: { r: 0, g: 0, b: 0, a: 1 },
        loadOp: 'clear',
        storeOp: 'store',
      }],
    });
    renderPass.setPipeline(renderPipeline);
    renderPass.setBindGroup(0, renderBindGroup);
    renderPass.draw(6);
    renderPass.end();

    device.queue.submit([encoder.finish()]);
    await device.queue.onSubmittedWorkDone();

    const frameTime = performance.now() - frameStart;

    frameCount++;
    const now = performance.now();
    if (now - lastTime >= 1000) {
      fps = frameCount;
      frameCount = 0;
      lastTime = now;
    }

    document.getElementById('fps').textContent = `FPS: ${fps}`;
    document.getElementById('frametime').textContent = `Frame: ${frameTime.toFixed(2)}ms`;
    document.getElementById('time').textContent = `Time: ${time.toFixed(2)}s`;

    requestAnimationFrame(render);
  }

  requestAnimationFrame(render);
}
