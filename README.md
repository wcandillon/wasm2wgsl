# wasm2wgsl

A WASM to WGSL experimental/toy transpiler that converts WebAssembly code into native WebGPU compute shaders.

We provide a `wgsl.h` header for GLSL-style vector/matrix types and invocations of WGSL built-ins.

## How to use it

1. Install LLVM: brew install llvm
2. Compile a shader: ./build.sh examples/raymarch.cpp
3. Serve with a web server: `npx serve .` 
4. Open http://localhost:3000/?wasm=examples/raymarch.wasm

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
./build.sh example/shader2.cpp
# Outputs: your_shader.wasm
```

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
