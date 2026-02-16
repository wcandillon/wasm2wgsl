// XorDev "Cosmic" shader — glowing rings
// Pure shader logic — all types and math provided by wgsl.h

#include "wgsl.h"

extern "C" void mainImage(vec4* O, float Fx, float Fy,
                          float Rx, float Ry, float iTime) {
    vec2 r(Rx, Ry);
    vec2 p = (vec2(Fx, Fy) - r * 0.6f) * mat2(1, -1, 2, 2);

    // I = p / (r+r-p).y  — constant across iterations
    vec2 I = p / (r + r - p).y;
    float l80 = length(I) * 80.0f;
    float ang = atan2(I.y, I.x);
    float rt  = 40.0f / r.y;

    vec4 o(0);

    // 30 ring iterations — #pragma forces full compile-time unroll
    #pragma clang loop unroll(full)
    for (int i = 1; i <= 30; i++) {
        float fi = (float)i;
        float a = ang * ceil(fi * 0.1f) + iTime * sin(fi * fi) + fi * fi;

        o += 0.2f / (abs(l80 - fi) + rt)
           * clamp(cos(a), 0.0f, 0.6f)
           * (cos(a - fi + vec4(0, 1, 2, 0)) + 1.0f);
    }

    *O = o;
}
