struct Uniforms {
  time: f32,
  width: f32,
  height: f32,
  pad: u32,
}

@group(0) @binding(0) var<storage, read> pixels: array<f32>;
@group(0) @binding(1) var<uniform> uniforms: Uniforms;

struct VOut {
  @builtin(position) pos: vec4<f32>,
  @location(0) uv: vec2<f32>,
}

@vertex
fn vs(@builtin(vertex_index) i: u32) -> VOut {
  var p = array<vec2<f32>, 6>(
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0),
    vec2(-1.0,  1.0), vec2(1.0, -1.0), vec2(1.0,  1.0)
  );
  var o: VOut;
  o.pos = vec4(p[i], 0.0, 1.0);
  o.uv = p[i] * 0.5 + 0.5;
  return o;
}

@fragment
fn fs(@location(0) uv: vec2<f32>) -> @location(0) vec4<f32> {
  let x = u32(uv.x * uniforms.width);
  let y = u32((1.0 - uv.y) * uniforms.height);
  let W = u32(uniforms.width);
  let idx = (y * W + x) * 4u;
  return vec4(pixels[idx], pixels[idx + 1u], pixels[idx + 2u], 1.0);
}
