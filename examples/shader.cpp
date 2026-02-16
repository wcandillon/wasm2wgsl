// Simple color animation shader using wgsl.h
// Demonstrates basic usage of the wasm2wgsl transpiler

#include "wgsl.h"

extern "C" void mainImage(vec4* fragColor, float fragCoordX, float fragCoordY,
                          float iResolutionX, float iResolutionY, float iTime) {
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = vec2(fragCoordX, fragCoordY) / vec2(iResolutionX, iResolutionY);

    // Time-varying pixel color using cosine palette
    vec3 col = 0.5f + 0.5f * cos(iTime + vec3(uv.x, uv.y, uv.x) + vec3(0.0f, 2.0f, 4.0f));

    // Output to screen
    *fragColor = vec4(col, 1.0f);
}
