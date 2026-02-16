# wasm2wgsl

A WASM to WGSL experimental/toy transpiler that converts WebAssembly code into native WebGPU compute shaders.

We provide a `wgsl.h` header for GLSL-style vector/matrix types and invocations of WGSL built-ins.

## How to use it

1. Install LLVM: brew install llvm
2. Compile a shader: ./build.sh examples/shader.cpp
3. Serve with a web server: `npx serve .` 
4. Open http://localhost:8000/?wasm=examples/shader.wasm

## Usage

### 1. Write your shader

Create a C/C++ file with a `mainImage` function. Include `wgsl.h` for GLSL-style types:

```cpp
#include "wgsl.h"

extern "C" void mainImage(vec4* fragColor, float fragCoordX, float fragCoordY,
                          float iResolutionX, float iResolutionY, float iTime) {
    vec2 uv = vec2(fragCoordX, fragCoordY) / vec2(iResolutionX, iResolutionY);

    // Time-varying color
    vec3 col = 0.5f + 0.5f * cos(iTime + vec3(uv.x, uv.y, uv.x) + vec3(0.0f, 2.0f, 4.0f));

    *fragColor = vec4(col, 1.0f);
}
```

### 2. Compile to WASM

```bash
./build.sh your_shader.cpp
# Outputs: your_shader.wasm
```

### 3. Run in browser

Serve the files with a local web server and open `index.html`:

```bash
python3 -m http.server 8000
# Open http://localhost:8000/?wasm=your_shader.wasm
```

## Supported WASM Operations

The transpiler supports a comprehensive set of WASM instructions:

- **Variables**: local.get/set/tee, global.get/set
- **Constants**: i32.const, f32.const
- **Memory**: i32.load/store, f32.load/store
- **Comparisons**: all i32 and f32 comparison operators
- **Arithmetic**: add, sub, mul, div, rem
- **Bitwise**: and, or, xor, shl, shr
- **Float unary**: abs, neg, ceil, floor, trunc, round, sqrt
- **Control flow**: block, loop, if/else, br, br_if, return
- **Conversions**: i32.trunc_f32, f32.convert_i32, reinterpret

## WASM Import to WGSL Built-in Mapping

Functions declared as `extern "C"` in your shader become WASM imports, which the transpiler maps to WGSL built-ins:

| WASM Import | WGSL Built-in |
|-------------|---------------|
| `sinf` | `sin` |
| `cosf` | `cos` |
| `tanf` | `tan` |
| `asinf` | `asin` |
| `acosf` | `acos` |
| `atanf` | `atan` |
| `atan2f` | `atan2` |
| `expf` | `exp` |
| `exp2f` | `exp2` |
| `logf` | `log` |
| `log2f` | `log2` |
| `powf` | `pow` |
| `fminf` | `min` |
| `fmaxf` | `max` |

## Examples

The `examples/` directory contains sample shaders:

- `shader.c` - Simple color animation
- `mandelbrot.c` - Classic Mandelbrot fractal with animated zoom

## License

MIT
