// wgsl.h - C/C++ header for writing shaders that compile to WASM and transpile to WGSL
//
// Usage:
//   #include "wgsl.h"
//   vec3 p(1.0f, 2.0f, 3.0f);
//   float a = atan2(p.y, p.x);
//   vec3 n = normalize(cross(a, b));

#pragma once

// ============================================================
// WGSL built-in imports (→ WASM imports → WGSL GPU instructions)
// ============================================================

extern "C" {
  // Trigonometric
  float sinf(float);
  float cosf(float);
  float tanf(float);
  float asinf(float);
  float acosf(float);
  float atanf(float);
  float atan2f(float, float);

  // Exponential
  float expf(float);
  float exp2f(float);
  float logf(float);
  float log2f(float);
  float powf(float, float);
}

// ============================================================
// Forward declarations
// ============================================================

struct vec3;
struct vec4;

// ============================================================
// vec2
// ============================================================

struct vec2 {
    float x, y;
    vec2() : x(0), y(0) {}
    vec2(float s) : x(s), y(s) {}
    vec2(float x, float y) : x(x), y(y) {}
    vec2 operator+(vec2 b)  const { return {x+b.x, y+b.y}; }
    vec2 operator-(vec2 b)  const { return {x-b.x, y-b.y}; }
    vec2 operator*(vec2 b)  const { return {x*b.x, y*b.y}; }
    vec2 operator/(vec2 b)  const { return {x/b.x, y/b.y}; }
    vec2 operator+(float s) const { return {x+s, y+s}; }
    vec2 operator-(float s) const { return {x-s, y-s}; }
    vec2 operator*(float s) const { return {x*s, y*s}; }
    vec2 operator/(float s) const { return {x/s, y/s}; }
    vec2 operator-()        const { return {-x, -y}; }
    vec2& operator+=(vec2 b) { x+=b.x; y+=b.y; return *this; }
    vec2& operator-=(vec2 b) { x-=b.x; y-=b.y; return *this; }
    vec2& operator*=(float s) { x*=s; y*=s; return *this; }
};

static vec2 operator+(float s, vec2 v) { return {s+v.x, s+v.y}; }
static vec2 operator-(float s, vec2 v) { return {s-v.x, s-v.y}; }
static vec2 operator*(float s, vec2 v) { return {s*v.x, s*v.y}; }

// ============================================================
// vec3
// ============================================================

struct vec3 {
    float x, y, z;
    vec3() : x(0), y(0), z(0) {}
    vec3(float s) : x(s), y(s), z(s) {}
    vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    vec3(vec2 xy, float z) : x(xy.x), y(xy.y), z(z) {}
    vec3(float x, vec2 yz) : x(x), y(yz.x), z(yz.y) {}
    vec3 operator+(vec3 b)  const { return {x+b.x, y+b.y, z+b.z}; }
    vec3 operator-(vec3 b)  const { return {x-b.x, y-b.y, z-b.z}; }
    vec3 operator*(vec3 b)  const { return {x*b.x, y*b.y, z*b.z}; }
    vec3 operator/(vec3 b)  const { return {x/b.x, y/b.y, z/b.z}; }
    vec3 operator+(float s) const { return {x+s, y+s, z+s}; }
    vec3 operator-(float s) const { return {x-s, y-s, z-s}; }
    vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
    vec3 operator/(float s) const { return {x/s, y/s, z/s}; }
    vec3 operator-()        const { return {-x, -y, -z}; }
    vec3& operator+=(vec3 b) { x+=b.x; y+=b.y; z+=b.z; return *this; }
    vec3& operator-=(vec3 b) { x-=b.x; y-=b.y; z-=b.z; return *this; }
    vec3& operator*=(float s) { x*=s; y*=s; z*=s; return *this; }
    vec3& operator*=(vec3 b) { x*=b.x; y*=b.y; z*=b.z; return *this; }
};

static vec3 operator+(float s, vec3 v) { return {s+v.x, s+v.y, s+v.z}; }
static vec3 operator-(float s, vec3 v) { return {s-v.x, s-v.y, s-v.z}; }
static vec3 operator*(float s, vec3 v) { return {s*v.x, s*v.y, s*v.z}; }

// ============================================================
// vec4
// ============================================================

struct vec4 {
    float x, y, z, w;
    vec4() : x(0), y(0), z(0), w(0) {}
    vec4(float s) : x(s), y(s), z(s), w(s) {}
    vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    vec4(vec3 v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}
    vec4(vec2 xy, vec2 zw) : x(xy.x), y(xy.y), z(zw.x), w(zw.y) {}
    vec4 operator+(vec4 b)  const { return {x+b.x, y+b.y, z+b.z, w+b.w}; }
    vec4 operator-(vec4 b)  const { return {x-b.x, y-b.y, z-b.z, w-b.w}; }
    vec4 operator*(vec4 b)  const { return {x*b.x, y*b.y, z*b.z, w*b.w}; }
    vec4 operator+(float s) const { return {x+s, y+s, z+s, w+s}; }
    vec4 operator-(float s) const { return {x-s, y-s, z-s, w-s}; }
    vec4 operator*(float s) const { return {x*s, y*s, z*s, w*s}; }
    vec4 operator/(float s) const { return {x/s, y/s, z/s, w/s}; }
    vec4 operator-()        const { return {-x, -y, -z, -w}; }
    vec4& operator+=(vec4 b) { x+=b.x; y+=b.y; z+=b.z; w+=b.w; return *this; }
    vec4& operator-=(vec4 b) { x-=b.x; y-=b.y; z-=b.z; w-=b.w; return *this; }
    vec4& operator*=(float s) { x*=s; y*=s; z*=s; w*=s; return *this; }
};

static vec4 operator+(float s, vec4 v) { return {s+v.x, s+v.y, s+v.z, s+v.w}; }
static vec4 operator-(float s, vec4 v) { return {s-v.x, s-v.y, s-v.z, s-v.w}; }
static vec4 operator*(float s, vec4 v) { return v * s; }

// ============================================================
// mat2
// ============================================================

struct mat2 {
    float a, b, c, d; // column-major: col0=(a,b), col1=(c,d)
    mat2(float a, float b, float c, float d) : a(a), b(b), c(c), d(d) {}
};

static vec2 operator*(vec2 v, mat2 m) {
    return {v.x*m.a + v.y*m.b, v.x*m.c + v.y*m.d};
}

// ============================================================
// Scalar math — GLSL-style wrappers
// ============================================================

// Trigonometric (→ WASM imports → WGSL sin/cos/etc.)
static float sin(float x)            { return sinf(x); }
static float cos(float x)            { return cosf(x); }
static float tan(float x)            { return tanf(x); }
static float asin(float x)           { return asinf(x); }
static float acos(float x)           { return acosf(x); }
static float atan(float x)           { return atanf(x); }
static float atan(float y, float x)  { return atan2f(y, x); }
static float atan2(float y, float x) { return atan2f(y, x); }

// Exponential (→ WASM imports → WGSL exp/log/etc.)
static float exp(float x)            { return expf(x); }
static float exp2(float x)           { return exp2f(x); }
static float log(float x)            { return logf(x); }
static float log2(float x)           { return log2f(x); }
static float pow(float x, float y)   { return powf(x, y); }

// Native WASM instructions (f32.abs, f32.sqrt, f32.ceil, etc.)
static float abs(float x)   { return __builtin_fabsf(x); }
static float sqrt(float x)  { return __builtin_sqrtf(x); }
static float ceil(float x)  { return __builtin_ceilf(x); }
static float floor(float x) { return __builtin_floorf(x); }
static float trunc(float x) { return __builtin_truncf(x); }
static float round(float x) { return __builtin_roundf(x); }
static float min(float a, float b) { return __builtin_fminf(a, b); }
static float max(float a, float b) { return __builtin_fmaxf(a, b); }

// Utility
static float clamp(float x, float lo, float hi) {
    return min(max(x, lo), hi);
}
static float fract(float x) { return x - floor(x); }
static float mod(float x, float y) { return x - y * floor(x / y); }
static float sign(float x) { return (x > 0.0f) ? 1.0f : ((x < 0.0f) ? -1.0f : 0.0f); }
static float step(float edge, float x) { return (x < edge) ? 0.0f : 1.0f; }
static float mix(float a, float b, float t) { return a + t * (b - a); }
static float smoothstep(float e0, float e1, float x) {
    float t = clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
static float radians(float deg) { return deg * 0.01745329252f; }

// ============================================================
// vec2 math
// ============================================================

static float dot(vec2 a, vec2 b) { return a.x*b.x + a.y*b.y; }
static float length(vec2 v) { return sqrt(dot(v, v)); }
static float distance(vec2 a, vec2 b) { return length(a - b); }
static vec2 normalize(vec2 v) { float l = length(v); return {v.x/l, v.y/l}; }
static vec2 abs(vec2 v) { return {abs(v.x), abs(v.y)}; }
static vec2 floor(vec2 v) { return {floor(v.x), floor(v.y)}; }
static vec2 ceil(vec2 v) { return {ceil(v.x), ceil(v.y)}; }
static vec2 fract(vec2 v) { return {fract(v.x), fract(v.y)}; }
static vec2 mod(vec2 v, float m) { return {mod(v.x, m), mod(v.y, m)}; }
static vec2 mod(vec2 v, vec2 m) { return {mod(v.x, m.x), mod(v.y, m.y)}; }
static vec2 min(vec2 a, vec2 b) { return {min(a.x, b.x), min(a.y, b.y)}; }
static vec2 max(vec2 a, vec2 b) { return {max(a.x, b.x), max(a.y, b.y)}; }
static vec2 clamp(vec2 v, vec2 lo, vec2 hi) { return {clamp(v.x,lo.x,hi.x), clamp(v.y,lo.y,hi.y)}; }
static vec2 clamp(vec2 v, float lo, float hi) { return {clamp(v.x,lo,hi), clamp(v.y,lo,hi)}; }
static vec2 mix(vec2 a, vec2 b, float t) { return {mix(a.x,b.x,t), mix(a.y,b.y,t)}; }
static vec2 step(vec2 edge, vec2 x) { return {step(edge.x,x.x), step(edge.y,x.y)}; }
static vec2 sin(vec2 v) { return {sinf(v.x), sinf(v.y)}; }
static vec2 cos(vec2 v) { return {cosf(v.x), cosf(v.y)}; }

// ============================================================
// vec3 math
// ============================================================

static float dot(vec3 a, vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static float length(vec3 v) { return sqrt(dot(v, v)); }
static float distance(vec3 a, vec3 b) { return length(a - b); }
static vec3 normalize(vec3 v) { float l = length(v); return v / l; }
static vec3 cross(vec3 a, vec3 b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
static vec3 abs(vec3 v) { return {abs(v.x), abs(v.y), abs(v.z)}; }
static vec3 floor(vec3 v) { return {floor(v.x), floor(v.y), floor(v.z)}; }
static vec3 fract(vec3 v) { return {fract(v.x), fract(v.y), fract(v.z)}; }
static vec3 mod(vec3 v, float m) { return {mod(v.x,m), mod(v.y,m), mod(v.z,m)}; }
static vec3 min(vec3 a, vec3 b) { return {min(a.x,b.x), min(a.y,b.y), min(a.z,b.z)}; }
static vec3 max(vec3 a, vec3 b) { return {max(a.x,b.x), max(a.y,b.y), max(a.z,b.z)}; }
static vec3 clamp(vec3 v, float lo, float hi) { return {clamp(v.x,lo,hi), clamp(v.y,lo,hi), clamp(v.z,lo,hi)}; }
static vec3 mix(vec3 a, vec3 b, float t) { return a + (b - a) * t; }
static vec3 mix(vec3 a, vec3 b, vec3 t) { return {mix(a.x,b.x,t.x), mix(a.y,b.y,t.y), mix(a.z,b.z,t.z)}; }
static vec3 pow(vec3 v, vec3 e) { return {pow(v.x,e.x), pow(v.y,e.y), pow(v.z,e.z)}; }
static vec3 sin(vec3 v) { return {sinf(v.x), sinf(v.y), sinf(v.z)}; }
static vec3 cos(vec3 v) { return {cosf(v.x), cosf(v.y), cosf(v.z)}; }
static vec3 step(float edge, vec3 v) { return {step(edge,v.x), step(edge,v.y), step(edge,v.z)}; }

// ============================================================
// vec4 math
// ============================================================

static vec4 abs(vec4 v) { return {abs(v.x), abs(v.y), abs(v.z), abs(v.w)}; }
static vec4 fract(vec4 v) { return {fract(v.x), fract(v.y), fract(v.z), fract(v.w)}; }
static vec4 floor(vec4 v) { return {floor(v.x), floor(v.y), floor(v.z), floor(v.w)}; }
static vec4 mix(vec4 a, vec4 b, float t) { return a + (b - a) * t; }
static vec4 cos(vec4 v) { return {cosf(v.x), cosf(v.y), cosf(v.z), cosf(v.w)}; }
static vec4 sin(vec4 v) { return {sinf(v.x), sinf(v.y), sinf(v.z), sinf(v.w)}; }
static float dot(vec4 a, vec4 b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }
