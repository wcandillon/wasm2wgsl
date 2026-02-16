// Chess Queen raymarcher — ported from Shadertoy GLSL to C++
// https://www.shadertoy.com/view/3sVfW3

#include "wgsl.h"

#define INLINE __attribute__((always_inline))

#define I_MAX 1024
#define FAR 1000.0f
#define EPS 0.01f
#define PI 3.141592653589793f
#define FOCAL 2.0f

#ifndef SAMPLES
#define SAMPLES 4
#endif

// ~~~~~~~~ CAMERA ~~~~~~~~
// Applies the rotation matrix from rot(angle) directly to a vector,
// avoiding a mat3 struct (which would go to WASM linear memory).
INLINE vec3 rotVec(vec2 angle, vec3 v) {
    vec2 cc = cos(angle);
    vec2 ss = sin(angle);
    // mat3 columns:
    //   col0 = (cc.x,       ss.x*ss.y,  -cc.y*ss.x)
    //   col1 = (0,           cc.y,        ss.y      )
    //   col2 = (ss.x,       -ss.y*cc.x,   cc.x*cc.y)
    return vec3(
        cc.x * v.x                + ss.x * v.z,
        ss.x * ss.y * v.x + cc.y * v.y - ss.y * cc.x * v.z,
       -cc.y * ss.x * v.x + ss.y * v.y + cc.x * cc.y * v.z
    );
}

// ~~~~~~~~ SDFs and operations ~~~~~~~~
INLINE float sdSphere(vec3 o, float r, vec3 p) {
    return length(p - o) - r;
}

INLINE float sdBox(vec3 b, vec3 p) {
    vec3 q = abs(p) - b;
    return length(max(q, vec3(0.0f))) + min(max(q.x, max(q.y, q.z)), 0.0f);
}

INLINE float sdRoundedBox(vec3 b, float r, vec3 p) {
    return sdBox(b, p) - r;
}

INLINE float sdHPlane(float h, vec3 p) {
    return p.y - h;
}

INLINE float sdCappedCylinder(float h, float r, vec3 p) {
    vec2 d = abs(vec2(length(vec2(p.x, p.z)), p.y)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0f) + length(max(d, vec2(0.0f)));
}

INLINE float sdEllipsoid(vec3 r, vec3 p) {
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    return k0 * (k0 - 1.0f) / k1;
}

INLINE float sdTorus(vec2 t, vec3 p) {
    vec2 q(length(vec2(p.x, p.z)) - t.x, p.y);
    return length(q) - t.y;
}

INLINE float sdRoundCone(vec3 a, vec3 b, float r1, float r2, vec3 p) {
    vec3 ba = b - a;
    float l2 = dot(ba, ba);
    float rr = r1 - r2;
    float a2 = l2 - rr * rr;
    float il2 = 1.0f / l2;

    vec3 pa = p - a;
    float y = dot(pa, ba);
    float z = y - l2;
    vec3 x = pa * l2 - ba * y;
    float x2 = dot(x, x);
    float y2 = y * y * l2;
    float z2 = z * z * l2;

    float k = sign(rr) * rr * rr * x2;
    if (sign(z) * a2 * z2 > k) return sqrt(x2 + z2) * il2 - r2;
    if (sign(y) * a2 * y2 < k) return sqrt(x2 + y2) * il2 - r1;
    return (sqrt(x2 * a2 * il2) + y * rr) * il2 - r1;
}

INLINE float sdCone(vec2 c, float h, vec3 p) {
    float q = length(vec2(p.x, p.z));
    return max(dot(c, vec2(q, p.y)), -h - p.y);
}

INLINE float smin(float a, float b, float k) {
    float h = max(k - abs(a - b), 0.0f) / k;
    return min(a, b) - h * h * k * (1.0f / 4.0f);
}

INLINE float smax(float a, float b, float k) {
    k *= 1.4f;
    float h = max(k - abs(a - b), 0.0f);
    return max(a, b) + h * h * h / (6.0f * k * k);
}

// ~~~~~~~~ Scene ~~~~~~~~
INLINE void map(vec3 p, float& d, float& id) {
    d = FAR;
    id = -1.0f;

    // board
    {
        float d0 = sdRoundedBox(vec3(8.2f, 0.35f, 8.2f), 0.1f, p - vec3(0.0f, -1.5f, 0.0f));
        if (d0 < d) { d = d0; id = 0.5f; }
    }

    // queen
    {
        vec3 qp = p + vec3(1.0f, 0.0f, 1.0f);

        // body
        vec3 p0 = qp - vec3(0.0f, 0.5f, 0.0f);
        float r = 0.28f + pow(0.4f - p0.y, 2.0f) / 6.0f;
        float d0 = sdCappedCylinder(1.5f, r, p0) - 0.02f;

        // head
        vec3 p1 = qp - vec3(0.0f, 1.9f, 0.0f);
        float d1 = sdCappedCylinder(0.2f, r - 0.1f, p1);
        d0 = smax(d0, -d1, 0.03f);

        vec3 p2 = qp - vec3(0.0f, 2.05f, 0.0f);
        float a = mod(atan(p2.z, p2.x) + PI / 8.0f, PI / 4.0f) - PI / 8.0f;
        float l = length(vec2(p2.x, p2.z));
        vec3 p2r(p2.y, l * cos(a), l * sin(a));
        float d2 = sdCappedCylinder(0.6f, 0.12f, p2r);
        d0 = smax(d0, -d2, 0.07f);

        vec3 p3 = qp - vec3(0.0f, 2.15f, 0.0f);
        float d3 = sdCone(vec2(sin(PI / 5.0f), cos(PI / 5.0f)), 0.22f, p3);
        d0 = smin(d0, d3, 0.05f);

        float d4 = sdSphere(vec3(0.0f, 2.18f, 0.0f), 0.09f, qp);
        d0 = smin(d0, d4, 0.03f);

        vec3 p5 = qp - vec3(0.0f, 1.4f, 0.0f);
        float d5 = sdEllipsoid(vec3(0.5f, 0.07f, 0.5f), p5);
        d0 = smin(d0, d5, 0.03f);

        vec3 p6 = qp - vec3(0.0f, 1.51f, 0.0f);
        float d6 = sdEllipsoid(vec3(0.42f, 0.07f, 0.42f), p6);
        d0 = smin(d0, d6, 0.03f);

        // base
        vec3 p7 = qp - vec3(0.0f, -1.0f, 0.0f);
        float d7 = sdTorus(vec2(0.43f, 0.5f), p7);
        d7 = max(d7, -sdHPlane(0.0f, p7));
        d0 = smin(d0, d7, 0.05f);

        float d8 = sdEllipsoid(vec3(0.77f, 0.08f, 0.77f), qp - vec3(0.0f, -0.55f, 0.0f));
        d0 = smin(d0, d8, 0.05f);

        // stripes
        float d9 = sdTorus(vec2(0.586f, 0.01f), qp - vec3(0.0f, -0.425f, 0.0f));
        d0 = smax(d0, -d9, 0.05f);
        float d10 = sdTorus(vec2(0.553f, 0.01f), qp - vec3(0.0f, -0.345f, 0.0f));
        d0 = smax(d0, -d10, 0.05f);

        if (d0 < d) { d = d0; id = 1.5f; }
    }
}

INLINE float mapDist(vec3 p) {
    float d, id;
    map(p, d, id);
    return d;
}

INLINE vec3 gradient(vec3 p) {
    float h = EPS * EPS;
    return normalize(
        vec3( 1,-1,-1) * mapDist(p + vec3( 1,-1,-1) * h) +
        vec3(-1,-1, 1) * mapDist(p + vec3(-1,-1, 1) * h) +
        vec3(-1, 1,-1) * mapDist(p + vec3(-1, 1,-1) * h) +
        vec3( 1, 1, 1) * mapDist(p + vec3( 1, 1, 1) * h)
    );
}

INLINE void raymarch(vec3 ro, vec3 rd, float& t, float& outDist, float& outId) {
    t = 0.0f;
    vec3 p = ro + rd * t;
    map(p, outDist, outId);
    float isInside = sign(outDist);

    for (int i = 0; i < I_MAX; i++) {
        float inc = isInside * outDist;
        if (t + inc < FAR && abs(outDist) > EPS) {
            t += inc;
            p = ro + rd * t;
            map(p, outDist, outId);
        } else {
            if (t + inc > FAR) {
                outId = -1.0f;
            }
            break;
        }
    }
}

// ~~~~~~~~ Per-sample rendering ~~~~~~~~
INLINE vec3 render(vec2 fragCoord, vec2 R, float iTime) {
    vec2 uv = (2.0f * fragCoord - R) / R.y;

    // Camera — auto-rotate, always above the board
    vec2 angle(iTime * 0.5f, -0.35f);
    vec3 ro = rotVec(angle, vec3(0.0f, 0.0f, 5.0f));
    ro += vec3(-1.0f, 2.0f, -1.0f);
    vec3 rd = rotVec(angle, normalize(vec3(uv.x, uv.y, -FOCAL)));

    vec3 mainLight = normalize(vec3(1.0f));

    float t, dist, id;
    raymarch(ro, rd, t, dist, id);
    vec3 p = ro + rd * t;
    vec3 normal = gradient(p);

    // Cyan background gradient
    vec3 sky = mix(vec3(0.3f, 0.6f, 1.0f), vec3(0.0f, 0.55f, 0.65f), 0.5f + 0.5f * rd.y);

    vec3 col(0.0f);

    if (id < 0.0f) {
        // skybox
        col = sky;
    } else if (id < 1.0f) {
        // board
        if (abs(p.x) > 8.0f || abs(p.z) > 8.0f) {
            // Board sides
            col = vec3(0.72f, 0.53f, 0.34f) * max(0.3f, dot(normal, mainLight));
        } else {
            vec2 ss = sin(0.5f * PI * vec2(p.x, p.z));
            float checker = sign(ss.x) * sign(ss.y);
            col = checker < 0.0f ? vec3(0.05f) : vec3(0.9f);
            col *= max(0.2f, dot(normal, mainLight));
        }
    } else if (id < 2.0f) {
        // chess piece
        col = vec3(0.95f, 0.95f, 0.85f) * max(0.2f, dot(normal, mainLight));
        col += vec3(0.1f) * max(0.0f, dot(normal, -mainLight));
        col += 0.1f * vec3(0.7f, 0.43f, 0.3f) * max(0.0f, dot(normal, vec3(0.0f, 1.0f, 0.0f)));
    }

    col = pow(col, vec3(0.5f));
    return col;
}

// ~~~~~~~~ Entry point ~~~~~~~~
extern "C"
void mainImage(vec4* fragColor, float fragCoordX, float fragCoordY,
               float iResolutionX, float iResolutionY, float iTime)
{
    vec2 R(iResolutionX, iResolutionY);
    vec2 fc(fragCoordX, fragCoordY);

    // Supersampling AA
    vec3 col(0.0f);
#if SAMPLES > 1
    col = render(fc + vec2(-0.25f, -0.25f), R, iTime)
        + render(fc + vec2( 0.25f, -0.25f), R, iTime)
        + render(fc + vec2(-0.25f,  0.25f), R, iTime)
        + render(fc + vec2( 0.25f,  0.25f), R, iTime);
    col = col * 0.25f;
#else
    col = render(fc, R, iTime);
#endif

    *fragColor = vec4(col, 1.0f);
}
