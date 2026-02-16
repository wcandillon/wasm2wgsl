// SDF Primitives raymarcher — ported from Shadertoy GLSL to C++
// Original: https://www.shadertoy.com/view/Xds3zN by Inigo Quilez
// The MIT License — Copyright © 2013 Inigo Quilez

#include "wgsl.h"

#define INLINE __attribute__((always_inline))

#ifndef SAMPLES
#define SAMPLES 1
#endif

// ~~~~~~~~ Helpers ~~~~~~~~

INLINE float dot2(vec2 v) { return dot(v, v); }
INLINE float dot2(vec3 v) { return dot(v, v); }
INLINE float ndot(vec2 a, vec2 b) { return a.x * b.x - a.y * b.y; }

// ~~~~~~~~ SDF Primitives ~~~~~~~~

INLINE float sdPlane(vec3 p) {
    return p.y;
}

INLINE float sdSphere(vec3 p, float s) {
    return length(p) - s;
}

INLINE float sdBox(vec3 p, vec3 b) {
    vec3 d = abs(p) - b;
    return min(max(d.x, max(d.y, d.z)), 0.0f) + length(max(d, vec3(0.0f)));
}

INLINE float sdBoxFrame(vec3 p, vec3 b, float e) {
    p = abs(p) - b;
    vec3 q = abs(p + vec3(e)) - vec3(e);
    return min(min(
        length(max(vec3(p.x, q.y, q.z), vec3(0.0f))) + min(max(p.x, max(q.y, q.z)), 0.0f),
        length(max(vec3(q.x, p.y, q.z), vec3(0.0f))) + min(max(q.x, max(p.y, q.z)), 0.0f)),
        length(max(vec3(q.x, q.y, p.z), vec3(0.0f))) + min(max(q.x, max(q.y, p.z)), 0.0f));
}

INLINE float sdEllipsoid(vec3 p, vec3 r) {
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    return k0 * (k0 - 1.0f) / k1;
}

INLINE float sdTorus(vec3 p, vec2 t) {
    return length(vec2(length(vec2(p.x, p.z)) - t.x, p.y)) - t.y;
}

INLINE float sdCappedTorus(vec3 p, vec2 sc, float ra, float rb) {
    p.x = abs(p.x);
    float k;
    if (sc.y * p.x > sc.x * p.y) k = dot(vec2(p.x, p.y), sc);
    else                          k = length(vec2(p.x, p.y));
    return sqrt(dot(p, p) + ra * ra - 2.0f * ra * k) - rb;
}

INLINE float sdHexPrism(vec3 p, vec2 h) {
    vec3 k(-0.8660254f, 0.5f, 0.57735f);
    p = abs(p);
    // p.xy -= 2.0*min(dot(k.xy, p.xy), 0.0)*k.xy
    float t = 2.0f * min(dot(vec2(k.x, k.y), vec2(p.x, p.y)), 0.0f);
    p.x -= t * k.x;
    p.y -= t * k.y;
    vec2 d(
        length(vec2(p.x, p.y) - vec2(clamp(p.x, -k.z * h.x, k.z * h.x), h.x)) * sign(p.y - h.x),
        p.z - h.y);
    return min(max(d.x, d.y), 0.0f) + length(max(d, vec2(0.0f)));
}

INLINE float sdOctogonPrism(vec3 p, float r, float h) {
    vec3 k(-0.9238795325f, 0.3826834323f, 0.4142135623f);
    p = abs(p);
    // p.xy -= 2.0*min(dot(vec2(k.x,k.y),p.xy),0.0)*vec2(k.x,k.y)
    { float t = 2.0f * min(dot(vec2(k.x, k.y), vec2(p.x, p.y)), 0.0f);
      p.x -= t * k.x; p.y -= t * k.y; }
    // p.xy -= 2.0*min(dot(vec2(-k.x,k.y),p.xy),0.0)*vec2(-k.x,k.y)
    { float t = 2.0f * min(dot(vec2(-k.x, k.y), vec2(p.x, p.y)), 0.0f);
      p.x -= t * (-k.x); p.y -= t * k.y; }
    // p.xy -= vec2(clamp(p.x, -k.z*r, k.z*r), r)
    p.x -= clamp(p.x, -k.z * r, k.z * r);
    p.y -= r;
    vec2 d(length(vec2(p.x, p.y)) * sign(p.y), p.z - h);
    return min(max(d.x, d.y), 0.0f) + length(max(d, vec2(0.0f)));
}

INLINE float sdCapsule(vec3 p, vec3 a, vec3 b, float r) {
    vec3 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0f, 1.0f);
    return length(pa - ba * h) - r;
}

// vertical round cone
INLINE float sdRoundCone(vec3 p, float r1, float r2, float h) {
    vec2 q(length(vec2(p.x, p.z)), p.y);
    float b = (r1 - r2) / h;
    float a = sqrt(1.0f - b * b);
    float k = dot(q, vec2(-b, a));
    float result;
    if (k < 0.0f)       result = length(q) - r1;
    else if (k > a * h) result = length(q - vec2(0.0f, h)) - r2;
    else                 result = dot(q, vec2(a, b)) - r1;
    return result;
}

// arbitrary round cone
INLINE float sdRoundCone(vec3 p, vec3 a, vec3 b, float r1, float r2) {
    vec3 ba = b - a;
    float l2 = dot(ba, ba);
    float rr = r1 - r2;
    float a2 = l2 - rr * rr;
    float il2 = 1.0f / l2;
    vec3 pa = p - a;
    float y = dot(pa, ba);
    float z = y - l2;
    float x2 = dot2(pa * l2 - ba * y);
    float y2 = y * y * l2;
    float z2 = z * z * l2;
    float k = sign(rr) * rr * rr * x2;
    float result;
    if (sign(z) * a2 * z2 > k)      result = sqrt(x2 + z2) * il2 - r2;
    else if (sign(y) * a2 * y2 < k) result = sqrt(x2 + y2) * il2 - r1;
    else                             result = (sqrt(x2 * a2 * il2) + y * rr) * il2 - r1;
    return result;
}

INLINE float sdTriPrism(vec3 p, vec2 h) {
    float k = sqrt(3.0f);
    h.x *= 0.5f * k;
    p.x /= h.x;
    p.y /= h.x;
    p.x = abs(p.x) - 1.0f;
    p.y = p.y + 1.0f / k;
    if (p.x + k * p.y > 0.0f) {
        float tx = (p.x - k * p.y) / 2.0f;
        float ty = (-k * p.x - p.y) / 2.0f;
        p.x = tx; p.y = ty;
    }
    p.x -= clamp(p.x, -2.0f, 0.0f);
    float d1 = length(vec2(p.x, p.y)) * sign(-p.y) * h.x;
    float d2 = abs(p.z) - h.y;
    return length(max(vec2(d1, d2), vec2(0.0f))) + min(max(d1, d2), 0.0f);
}

// vertical cylinder
INLINE float sdCylinder(vec3 p, vec2 h) {
    vec2 d = abs(vec2(length(vec2(p.x, p.z)), p.y)) - h;
    return min(max(d.x, d.y), 0.0f) + length(max(d, vec2(0.0f)));
}

// arbitrary orientation cylinder
INLINE float sdCylinder(vec3 p, vec3 a, vec3 b, float r) {
    vec3 pa = p - a;
    vec3 ba = b - a;
    float baba = dot(ba, ba);
    float paba = dot(pa, ba);
    float x = length(pa * baba - ba * paba) - r * baba;
    float y = abs(paba - baba * 0.5f) - baba * 0.5f;
    float x2 = x * x;
    float y2 = y * y * baba;
    float d;
    if (max(x, y) < 0.0f) {
        d = -min(x2, y2);
    } else {
        float dx = 0.0f; if (x > 0.0f) dx = x2;
        float dy = 0.0f; if (y > 0.0f) dy = y2;
        d = dx + dy;
    }
    return sign(d) * sqrt(abs(d)) / baba;
}

// vertical cone
INLINE float sdCone(vec3 p, vec2 c, float h) {
    vec2 q = vec2(c.x, -c.y) * (h / c.y);
    vec2 w(length(vec2(p.x, p.z)), p.y);
    vec2 a = w - q * clamp(dot(w, q) / dot(q, q), 0.0f, 1.0f);
    vec2 b = w - q * vec2(clamp(w.x / q.x, 0.0f, 1.0f), 1.0f);
    float kk = sign(q.y);
    float d = min(dot(a, a), dot(b, b));
    float s = max(kk * (w.x * q.y - w.y * q.x), kk * (w.y - q.y));
    return sqrt(d) * sign(s);
}

// vertical capped cone
INLINE float sdCappedCone(vec3 p, float h, float r1, float r2) {
    vec2 q(length(vec2(p.x, p.z)), p.y);
    vec2 k1(r2, h);
    vec2 k2(r2 - r1, 2.0f * h);
    float rsel = r2; if (q.y < 0.0f) rsel = r1;
    vec2 ca(q.x - min(q.x, rsel), abs(q.y) - h);
    vec2 cb = q - k1 + k2 * clamp(dot(k1 - q, k2) / dot2(k2), 0.0f, 1.0f);
    float s = 1.0f; if (cb.x < 0.0f && ca.y < 0.0f) s = -1.0f;
    return s * sqrt(min(dot2(ca), dot2(cb)));
}

// arbitrary capped cone
INLINE float sdCappedCone(vec3 p, vec3 a, vec3 b, float ra, float rb) {
    float rba = rb - ra;
    float baba = dot(b - a, b - a);
    float papa = dot(p - a, p - a);
    float paba = dot(p - a, b - a) / baba;
    float x = sqrt(papa - paba * paba * baba);
    float rsel = rb; if (paba < 0.5f) rsel = ra;
    float cax = max(0.0f, x - rsel);
    float cay = abs(paba - 0.5f) - 0.5f;
    float kk = rba * rba + baba;
    float f = clamp((rba * (x - ra) + paba * baba) / kk, 0.0f, 1.0f);
    float cbx = x - ra - f * rba;
    float cby = paba - f;
    float s = 1.0f; if (cbx < 0.0f && cay < 0.0f) s = -1.0f;
    return s * sqrt(min(cax * cax + cay * cay * baba, cbx * cbx + cby * cby * baba));
}

INLINE float sdSolidAngle(vec3 pos, vec2 c, float ra) {
    vec2 p(length(vec2(pos.x, pos.z)), pos.y);
    float l = length(p) - ra;
    float m = length(p - c * clamp(dot(p, c), 0.0f, ra));
    return max(l, m * sign(c.y * p.x - c.x * p.y));
}

INLINE float sdOctahedron(vec3 p, float s) {
    p = abs(p);
    float m = p.x + p.y + p.z - s;
    float result;
    if (3.0f * p.x < m) {
        vec3 q(p.x, p.y, p.z);
        float k = clamp(0.5f * (q.z - q.y + s), 0.0f, s);
        result = length(vec3(q.x, q.y - s + k, q.z - k));
    } else if (3.0f * p.y < m) {
        vec3 q(p.y, p.z, p.x);
        float k = clamp(0.5f * (q.z - q.y + s), 0.0f, s);
        result = length(vec3(q.x, q.y - s + k, q.z - k));
    } else if (3.0f * p.z < m) {
        vec3 q(p.z, p.x, p.y);
        float k = clamp(0.5f * (q.z - q.y + s), 0.0f, s);
        result = length(vec3(q.x, q.y - s + k, q.z - k));
    } else {
        result = m * 0.57735027f;
    }
    return result;
}

INLINE float sdPyramid(vec3 p, float h) {
    float m2 = h * h + 0.25f;
    // p.xz = abs(p.xz)
    p.x = abs(p.x); p.z = abs(p.z);
    // p.xz = (p.z>p.x) ? p.zx : p.xz
    if (p.z > p.x) { float t = p.x; p.x = p.z; p.z = t; }
    p.x -= 0.5f; p.z -= 0.5f;
    vec3 q(p.z, h * p.y - 0.5f * p.x, h * p.x + 0.5f * p.y);
    float s = max(-q.x, 0.0f);
    float t = clamp((q.y - 0.5f * p.z) / (m2 + 0.25f), 0.0f, 1.0f);
    float a = m2 * (q.x + s) * (q.x + s) + q.y * q.y;
    float b = m2 * (q.x + 0.5f * t) * (q.x + 0.5f * t) + (q.y - m2 * t) * (q.y - m2 * t);
    float d2 = min(a, b); if (min(q.y, -q.x * m2 - q.y * 0.5f) > 0.0f) d2 = 0.0f;
    return sqrt((d2 + q.z * q.z) / m2) * sign(max(q.z, -p.y));
}

INLINE float sdRhombus(vec3 p, float la, float lb, float h, float ra) {
    p = abs(p);
    vec2 b(la, lb);
    float f = clamp(ndot(b, b - 2.0f * vec2(p.x, p.z)) / dot(b, b), -1.0f, 1.0f);
    vec2 q(
        length(vec2(p.x, p.z) - 0.5f * b * vec2(1.0f - f, 1.0f + f)) *
            sign(p.x * b.y + p.z * b.x - b.x * b.y) - ra,
        p.y - h);
    return min(max(q.x, q.y), 0.0f) + length(max(q, vec2(0.0f)));
}

INLINE float sdHorseshoe(vec3 p, vec2 c, float r, float le, vec2 w) {
    p.x = abs(p.x);
    float l = length(vec2(p.x, p.y));
    // p.xy = mat2(-c.x, c.y, c.y, c.x) * p.xy
    { float tx = -c.x * p.x + c.y * p.y;
      float ty =  c.y * p.x + c.x * p.y;
      p.x = tx; p.y = ty; }
    // p.xy = vec2((p.y>0||p.x>0)?p.x:l*sign(-c.x), (p.x>0)?p.y:l)
    { float tx = l * sign(-c.x); if (p.y > 0.0f || p.x > 0.0f) tx = p.x;
      float ty = l; if (p.x > 0.0f) ty = p.y;
      p.x = tx; p.y = ty; }
    // p.xy = vec2(p.x, abs(p.y-r)) - vec2(le, 0.0)
    p.x = p.x - le;
    p.y = abs(p.y - r);
    vec2 q(length(max(vec2(p.x, p.y), vec2(0.0f))) + min(0.0f, max(p.x, p.y)), p.z);
    vec2 d = abs(q) - w;
    return min(max(d.x, d.y), 0.0f) + length(max(d, vec2(0.0f)));
}

INLINE float sdU(vec3 p, float r, float le, vec2 w) {
    { float tmp = length(vec2(p.x, p.y)); if (p.y > 0.0f) tmp = abs(p.x); p.x = tmp; }
    p.x = abs(p.x - r);
    p.y = p.y - le;
    float k = max(p.x, p.y);
    float qx = length(max(vec2(p.x, p.y), vec2(0.0f))); if (k < 0.0f) qx = -k;
    vec2 q(qx, abs(p.z));
    vec2 d = q - w;
    return length(max(d, vec2(0.0f))) + min(max(d.x, d.y), 0.0f);
}

// ~~~~~~~~ Operations ~~~~~~~~

INLINE vec2 opU(vec2 d1, vec2 d2) {
    vec2 result = d2; if (d1.x < d2.x) result = d1;
    return result;
}

// ~~~~~~~~ Scene ~~~~~~~~

INLINE vec2 map(vec3 pos) {
    vec2 res(pos.y, 0.0f);

    // bounding box
    if (sdBox(pos - vec3(-2.0f, 0.3f, 0.25f), vec3(0.3f, 0.3f, 1.0f)) < res.x) {
        res = opU(res, vec2(sdSphere(pos - vec3(-2.0f, 0.25f, 0.0f), 0.25f), 26.9f));
        // .xzy swizzle
        vec3 rp = pos - vec3(-2.0f, 0.25f, 1.0f);
        res = opU(res, vec2(sdRhombus(vec3(rp.x, rp.z, rp.y), 0.15f, 0.25f, 0.04f, 0.08f), 17.0f));
    }

    // bounding box
    if (sdBox(pos - vec3(0.0f, 0.3f, -1.0f), vec3(0.35f, 0.3f, 2.5f)) < res.x) {
        res = opU(res, vec2(sdCappedTorus(
            (pos - vec3(0.0f, 0.30f, 1.0f)) * vec3(1.0f, -1.0f, 1.0f),
            vec2(0.866025f, -0.5f), 0.25f, 0.05f), 25.0f));
        res = opU(res, vec2(sdBoxFrame(pos - vec3(0.0f, 0.25f, 0.0f),
            vec3(0.3f, 0.25f, 0.2f), 0.025f), 16.9f));
        res = opU(res, vec2(sdCone(pos - vec3(0.0f, 0.45f, -1.0f),
            vec2(0.6f, 0.8f), 0.45f), 55.0f));
        res = opU(res, vec2(sdCappedCone(pos - vec3(0.0f, 0.25f, -2.0f),
            0.25f, 0.25f, 0.1f), 13.67f));
        res = opU(res, vec2(sdSolidAngle(pos - vec3(0.0f, 0.0f, -3.0f),
            vec2(3.0f, 4.0f) / 5.0f, 0.4f), 49.13f));
    }

    // bounding box
    if (sdBox(pos - vec3(1.0f, 0.3f, -1.0f), vec3(0.35f, 0.3f, 2.5f)) < res.x) {
        // .xzy swizzle
        vec3 tp = pos - vec3(1.0f, 0.30f, 1.0f);
        res = opU(res, vec2(sdTorus(vec3(tp.x, tp.z, tp.y), vec2(0.25f, 0.05f)), 7.1f));
        res = opU(res, vec2(sdBox(pos - vec3(1.0f, 0.25f, 0.0f),
            vec3(0.3f, 0.25f, 0.1f)), 3.0f));
        res = opU(res, vec2(sdCapsule(pos - vec3(1.0f, 0.0f, -1.0f),
            vec3(-0.1f, 0.1f, -0.1f), vec3(0.2f, 0.4f, 0.2f), 0.1f), 31.9f));
        res = opU(res, vec2(sdCylinder(pos - vec3(1.0f, 0.25f, -2.0f),
            vec2(0.15f, 0.25f)), 8.0f));
        res = opU(res, vec2(sdHexPrism(pos - vec3(1.0f, 0.2f, -3.0f),
            vec2(0.2f, 0.05f)), 18.4f));
    }

    // bounding box
    if (sdBox(pos - vec3(-1.0f, 0.35f, -1.0f), vec3(0.35f, 0.35f, 2.5f)) < res.x) {
        res = opU(res, vec2(sdPyramid(pos - vec3(-1.0f, -0.6f, -3.0f), 1.0f), 13.56f));
        res = opU(res, vec2(sdOctahedron(pos - vec3(-1.0f, 0.15f, -2.0f), 0.35f), 23.56f));
        res = opU(res, vec2(sdTriPrism(pos - vec3(-1.0f, 0.15f, -1.0f),
            vec2(0.3f, 0.05f)), 43.5f));
        res = opU(res, vec2(sdEllipsoid(pos - vec3(-1.0f, 0.25f, 0.0f),
            vec3(0.2f, 0.25f, 0.05f)), 43.17f));
        res = opU(res, vec2(sdHorseshoe(pos - vec3(-1.0f, 0.25f, 1.0f),
            vec2(cos(1.3f), sin(1.3f)), 0.2f, 0.3f, vec2(0.03f, 0.08f)), 11.5f));
    }

    // bounding box
    if (sdBox(pos - vec3(2.0f, 0.3f, -1.0f), vec3(0.35f, 0.3f, 2.5f)) < res.x) {
        res = opU(res, vec2(sdOctogonPrism(pos - vec3(2.0f, 0.2f, -3.0f),
            0.2f, 0.05f), 51.8f));
        res = opU(res, vec2(sdCylinder(pos - vec3(2.0f, 0.14f, -2.0f),
            vec3(0.1f, -0.1f, 0.0f), vec3(-0.2f, 0.35f, 0.1f), 0.08f), 31.2f));
        res = opU(res, vec2(sdCappedCone(pos - vec3(2.0f, 0.09f, -1.0f),
            vec3(0.1f, 0.0f, 0.0f), vec3(-0.2f, 0.40f, 0.1f), 0.15f, 0.05f), 46.1f));
        res = opU(res, vec2(sdRoundCone(pos - vec3(2.0f, 0.15f, 0.0f),
            vec3(0.1f, 0.0f, 0.0f), vec3(-0.1f, 0.35f, 0.1f), 0.15f, 0.05f), 51.7f));
        res = opU(res, vec2(sdRoundCone(pos - vec3(2.0f, 0.20f, 1.0f),
            0.2f, 0.1f, 0.3f), 37.0f));
    }

    return res;
}

// ~~~~~~~~ Ray-box intersection ~~~~~~~~

INLINE vec2 iBox(vec3 ro, vec3 rd, vec3 rad) {
    vec3 m = vec3(1.0f) / rd;
    vec3 n = m * ro;
    vec3 k = abs(m) * rad;
    vec3 t1 = -n - k;
    vec3 t2 = -n + k;
    return vec2(max(max(t1.x, t1.y), t1.z), min(min(t2.x, t2.y), t2.z));
}

// ~~~~~~~~ Raycast ~~~~~~~~

INLINE vec2 raycast(vec3 ro, vec3 rd) {
    vec2 res(-1.0f, -1.0f);
    float tmin = 1.0f;
    float tmax = 20.0f;

    // raytrace floor plane
    float tp1 = (0.0f - ro.y) / rd.y;
    if (tp1 > 0.0f) {
        tmax = min(tmax, tp1);
        res = vec2(tp1, 1.0f);
    }

    // raymarch primitives
    vec2 tb = iBox(ro - vec3(0.0f, 0.4f, -0.5f), rd, vec3(2.5f, 0.41f, 3.0f));
    if (tb.x < tb.y && tb.y > 0.0f && tb.x < tmax) {
        tmin = max(tb.x, tmin);
        tmax = min(tb.y, tmax);
        float t = tmin;
        for (int i = 0; i < 70 && t < tmax; i++) {
            vec2 h = map(ro + rd * t);
            if (abs(h.x) < (0.0001f * t)) {
                res = vec2(t, h.y);
                break;
            }
            t += h.x;
        }
    }

    return res;
}

// ~~~~~~~~ Soft shadow ~~~~~~~~

INLINE float calcSoftshadow(vec3 ro, vec3 rd, float mint, float tmax) {
    float tp = (0.8f - ro.y) / rd.y;
    if (tp > 0.0f) tmax = min(tmax, tp);

    float res = 1.0f;
    float t = mint;
    for (int i = 0; i < 24; i++) {
        float h = map(ro + rd * t).x;
        float s = clamp(8.0f * h / t, 0.0f, 1.0f);
        res = min(res, s);
        t += clamp(h, 0.01f, 0.2f);
        if (res < 0.004f || t > tmax) break;
    }
    res = clamp(res, 0.0f, 1.0f);
    return res * res * (3.0f - 2.0f * res);
}

// ~~~~~~~~ Normal (tetrahedron technique) ~~~~~~~~

INLINE vec3 calcNormal(vec3 pos) {
    vec3 n(0.0f);
    for (int i = 0; i < 4; i++) {
        vec3 e = (vec3(
            (float)(((i + 3) >> 1) & 1),
            (float)((i >> 1) & 1),
            (float)(i & 1)) * 2.0f - vec3(1.0f)) * 0.5773f;
        n += e * map(pos + e * 0.0005f).x;
    }
    return normalize(n);
}

// ~~~~~~~~ Ambient occlusion ~~~~~~~~

INLINE float calcAO(vec3 pos, vec3 nor) {
    float occ = 0.0f;
    float sca = 1.0f;
    for (int i = 0; i < 5; i++) {
        float h = 0.01f + 0.12f * (float)i / 4.0f;
        float d = map(pos + nor * h).x;
        occ += (h - d) * sca;
        sca *= 0.95f;
        if (occ > 0.35f) break;
    }
    return clamp(1.0f - 3.0f * occ, 0.0f, 1.0f) * (0.5f + 0.5f * nor.y);
}

// ~~~~~~~~ Checker pattern with filtering ~~~~~~~~

INLINE float checkersGradBox(vec2 p, vec2 dpdx, vec2 dpdy) {
    vec2 w = abs(dpdx) + abs(dpdy) + vec2(0.001f);
    vec2 i = (abs(fract((p - w * 0.5f) * 0.5f) - vec2(0.5f))
            - abs(fract((p + w * 0.5f) * 0.5f) - vec2(0.5f))) * 2.0f / w;
    return 0.5f - 0.5f * i.x * i.y;
}

// ~~~~~~~~ Render ~~~~~~~~

INLINE vec3 render(vec3 ro, vec3 rd, vec3 rdx, vec3 rdy) {
    // background
    vec3 col = vec3(0.7f, 0.7f, 0.9f) - vec3(max(rd.y, 0.0f) * 0.3f);

    // raycast scene
    vec2 res = raycast(ro, rd);
    float t = res.x;
    float m = res.y;

    if (m > -0.5f) {
        vec3 pos = ro + rd * t;
        vec3 nor(0.0f, 1.0f, 0.0f);
        if (m >= 1.5f) nor = calcNormal(pos);
        // reflect(rd, nor)
        vec3 ref = rd - nor * (2.0f * dot(nor, rd));

        // material
        col = vec3(0.2f) + sin(vec3(m * 2.0f) + vec3(0.0f, 1.0f, 2.0f)) * 0.2f;
        float ks = 1.0f;

        if (m < 1.5f) {
            // project pixel footprint into the plane
            vec3 dpdx = rd * (ro.y / rd.y) - rdx * (ro.y / rdx.y);
            vec3 dpdy = rd * (ro.y / rd.y) - rdy * (ro.y / rdy.y);
            float f = checkersGradBox(vec2(pos.x, pos.z) * 3.0f,
                                      vec2(dpdx.x, dpdx.z) * 3.0f,
                                      vec2(dpdy.x, dpdy.z) * 3.0f);
            col = vec3(0.15f + f * 0.05f);
            ks = 0.4f;
        }

        // lighting
        float occ = calcAO(pos, nor);
        vec3 lin(0.0f);

        // sun
        {
            vec3 lig = normalize(vec3(-0.5f, 0.4f, -0.6f));
            vec3 hal = normalize(lig - rd);
            float dif = clamp(dot(nor, lig), 0.0f, 1.0f);
            dif *= calcSoftshadow(pos, lig, 0.02f, 2.5f);
            float spe = pow(clamp(dot(nor, hal), 0.0f, 1.0f), 16.0f);
            spe *= dif;
            spe *= 0.04f + 0.96f * pow(clamp(1.0f - dot(hal, lig), 0.0f, 1.0f), 5.0f);
            lin += col * 2.20f * dif * vec3(1.30f, 1.00f, 0.70f);
            lin += vec3(5.00f * spe) * vec3(1.30f, 1.00f, 0.70f) * ks;
        }
        // sky
        {
            float dif = sqrt(clamp(0.5f + 0.5f * nor.y, 0.0f, 1.0f));
            dif *= occ;
            float spe = smoothstep(-0.2f, 0.2f, ref.y);
            spe *= dif;
            spe *= 0.04f + 0.96f * pow(clamp(1.0f + dot(nor, rd), 0.0f, 1.0f), 5.0f);
            spe *= calcSoftshadow(pos, ref, 0.02f, 2.5f);
            lin += col * 0.60f * dif * vec3(0.40f, 0.60f, 1.15f);
            lin += vec3(2.00f * spe) * vec3(0.40f, 0.60f, 1.30f) * ks;
        }
        // back
        {
            float dif = clamp(dot(nor, normalize(vec3(0.5f, 0.0f, 0.6f))), 0.0f, 1.0f)
                      * clamp(1.0f - pos.y, 0.0f, 1.0f);
            dif *= occ;
            lin += col * 0.55f * dif * vec3(0.25f);
        }
        // sss
        {
            float dif = pow(clamp(1.0f + dot(nor, rd), 0.0f, 1.0f), 2.0f);
            dif *= occ;
            lin += col * 0.25f * dif;
        }

        col = lin;
        col = mix(col, vec3(0.7f, 0.7f, 0.9f), 1.0f - exp(-0.0001f * t * t * t));
    }

    return clamp(col, 0.0f, 1.0f);
}

// ~~~~~~~~ Camera ~~~~~~~~

INLINE void setCamera(vec3 ro, vec3 ta, float cr, vec3& cu, vec3& cv, vec3& cw) {
    cw = normalize(ta - ro);
    vec3 cp(sin(cr), cos(cr), 0.0f);
    cu = normalize(cross(cw, cp));
    cv = cross(cu, cw);
}

INLINE vec3 camMul(vec3 cu, vec3 cv, vec3 cw, vec3 v) {
    return cu * v.x + cv * v.y + cw * v.z;
}

// ~~~~~~~~ Per-sample rendering ~~~~~~~~

INLINE vec3 renderSample(vec2 fragCoord, vec2 R, float iTime,
                         vec3 ro, vec3 cu, vec3 cv, vec3 cw) {
    vec2 p = (fragCoord * 2.0f - R) / R.y;
    float fl = 2.5f;
    vec3 rd = camMul(cu, cv, cw, normalize(vec3(p.x, p.y, fl)));

    // ray differentials
    vec2 px = (vec2(fragCoord.x + 1.0f, fragCoord.y) * 2.0f - R) / R.y;
    vec2 py = (vec2(fragCoord.x, fragCoord.y + 1.0f) * 2.0f - R) / R.y;
    vec3 rdx = camMul(cu, cv, cw, normalize(vec3(px.x, px.y, fl)));
    vec3 rdy = camMul(cu, cv, cw, normalize(vec3(py.x, py.y, fl)));

    vec3 col = render(ro, rd, rdx, rdy);
    col = pow(col, vec3(0.4545f));
    return col;
}

// ~~~~~~~~ Entry point ~~~~~~~~

extern "C"
void mainImage(vec4* fragColor, float fragCoordX, float fragCoordY,
               float iResolutionX, float iResolutionY, float iTime)
{
    vec2 R(iResolutionX, iResolutionY);
    vec2 fc(fragCoordX, fragCoordY);

    float time = 32.0f + iTime * 1.5f;

    // camera
    vec3 ta(0.25f, -0.75f, -0.75f);
    vec3 ro = ta + vec3(4.5f * cos(0.1f * time), 2.2f, 4.5f * sin(0.1f * time));
    vec3 cu, cv, cw;
    setCamera(ro, ta, 0.0f, cu, cv, cw);

    // supersampling AA
    vec3 col(0.0f);
#if SAMPLES > 1
    col = renderSample(fc + vec2(-0.25f, -0.25f), R, iTime, ro, cu, cv, cw)
        + renderSample(fc + vec2( 0.25f, -0.25f), R, iTime, ro, cu, cv, cw)
        + renderSample(fc + vec2(-0.25f,  0.25f), R, iTime, ro, cu, cv, cw)
        + renderSample(fc + vec2( 0.25f,  0.25f), R, iTime, ro, cu, cv, cw);
    col = col * 0.25f;
#else
    col = renderSample(fc, R, iTime, ro, cu, cv, cw);
#endif

    *fragColor = vec4(col, 1.0f);
}
