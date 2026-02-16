// E1M1 - Hangar — by @P_Malin
// https://www.shadertoy.com/view/lsSXzD
#include "wgsl.h"

#define AI __attribute__((always_inline))

#define ENABLE_NUKAGE_SECTORS
#define ENABLE_START_SECTORS
#define ENABLE_SPRITES
#define DEMO_CAMERA
#define INTRO_EFFECT
#define DRAW_SKY
#define HEAD_BOB
#define PIXELATE_IMAGE
#define QUANTIZE_FINAL_IMAGE
#define QUANTIZE_TEXTURES
#define PIXELATE_TEXTURES

#ifndef ENABLE_EXTRA_NUKAGE_SECTORS
#define CLOSE_NUKAGE_SECTOR
#endif
#ifndef ENABLE_SECTOR_31
#define CLOSE_START_SECTOR
#endif

#define FAR_CLIP 10000.0f

static const float kDepthFadeScale = 1.0f / 3500.0f;
static const float kExtraLight = 0.0f;
static const float kC = 1.0f / 16.0f;

struct Ray { vec3 vRayOrigin; vec3 vRayDir; };

// ============================================================
// Texture IDs
// ============================================================
#define TEX_X 0.0f
#define TEX_F_SKY1 1.0f
#define TEX_NUKAGE3 2.0f
#define TEX_FLOOR7_1 3.0f
#define TEX_FLAT5_5 4.0f
#define TEX_FLOOR4_8 5.0f
#define TEX_CEIL3_5 6.0f
#define TEX_TLITE6_4 7.0f
#define TEX_FLAT14 8.0f
#define TEX_FLOOR7_2 9.0f
#define TEX_STEP2 10.0f
#define TEX_TLITE6_1 11.0f
#define TEX_DOOR3 12.0f
#define TEX_LITE3 13.0f
#define TEX_STARTAN3 14.0f
#define TEX_BROWN1 15.0f
#define TEX_DOORSTOP 16.0f
#define TEX_COMPUTE2 17.0f
#define TEX_STEP6 18.0f
#define TEX_BROWN144 19.0f
#define TEX_SUPPORT2 20.0f
#define TEX_STARG3 21.0f
#define TEX_DOORTRAK 22.0f
#define TEX_SLADWALL 23.0f
#define TEX_TEKWALL4 24.0f
#define TEX_SW1COMP 25.0f
#define TEX_BIGDOOR2 26.0f
#define TEX_BAR1A 32.0f
#define TEX_PLAYW 33.0f

// ============================================================
// Hash / noise
// ============================================================

AI static float hash(float p) {
    vec2 p2 = fract(vec2(p * 5.3983f, p * 5.4427f));
    float d = dot(vec2(p2.y, p2.x), p2 + vec2(21.5351f, 14.3137f));
    p2 = p2 + d;
    return fract(p2.x * p2.y * 95.4337f);
}

AI static float hash2D(vec2 p) {
    return hash(dot(p, vec2(1.0f, 41.0f)));
}

AI static float noise1D(float p) {
    float fl = floor(p);
    float h0 = hash(fl);
    float h1 = hash(fl + 1.0f);
    float fr = p - fl;
    float fr2 = fr * fr;
    float fr3 = fr2 * fr;
    float t1 = 3.0f * fr2 - 2.0f * fr3;
    return h0 * (1.0f - t1) + h1 * t1;
}

AI static float noise2D(vec2 p, float r) {
    vec2 fl = floor(p);
    float h00 = hash2D(mod(fl + vec2(0.0f, 0.0f), r));
    float h10 = hash2D(mod(fl + vec2(1.0f, 0.0f), r));
    float h01 = hash2D(mod(fl + vec2(0.0f, 1.0f), r));
    float h11 = hash2D(mod(fl + vec2(1.0f, 1.0f), r));
    vec2 fr = p - fl;
    vec2 fr2 = fr * fr;
    vec2 fr3 = fr2 * fr;
    vec2 t1 = 3.0f * fr2 - 2.0f * fr3;
    vec2 t0 = 1.0f - t1;
    return h00*t0.x*t0.y + h10*t1.x*t0.y + h01*t0.x*t1.y + h11*t1.x*t1.y;
}

AI static float fbm(vec2 p, float per) {
    float val = 0.0f, tot = 0.0f, mag = 0.5f;
    p = p + 0.5f;
    p = p * (1.0f / 8.0f);
    val += noise2D(p, 4.0f)*mag; tot+=mag; p=p*2.0f+1.234f; mag*=per;
    val += noise2D(p, 8.0f)*mag; tot+=mag; p=p*2.0f+2.456f; mag*=per;
    val += noise2D(p,16.0f)*mag; tot+=mag; p=p*2.0f+3.678f; mag*=per;
    val += noise2D(p,32.0f)*mag; tot+=mag;
    return val * (1.0f / tot);
}

// ============================================================
// Texture helpers
// ============================================================

AI static vec3 Quantize(vec3 col) {
    return floor(col * 48.0f + 0.5f) * (1.0f / 48.0f);
}

AI static float Indent(vec2 tc, vec2 vHigh, vec2 vLow, float fHi, float fLo) {
    vec2 vMin = min(vLow, vHigh);
    vec2 vMax = max(vLow, vHigh);
    if (tc.x < vMin.x || tc.x > vMax.x || tc.y < vMin.y || tc.y > vMax.y)
        return 1.0f;
    if (tc.x == vHigh.x || tc.y == vHigh.y) return fHi;
    if (tc.x == vLow.x  || tc.y == vLow.y)  return fLo;
    return 1.0f;
}

AI static float wrap(float x, float r) {
    return fract(x * (1.0f / r)) * r;
}

AI static vec4 Hexagon(vec2 vUV) {
    float fRow = floor(vUV.y);
    vec2 vLocalUV = vUV;
    float fRowEven = wrap(fRow, 2.0f);
    if (fRowEven < 0.5f) vLocalUV.x += 0.5f;
    vec2 vIndex = floor(vLocalUV);
    vec2 vTileUV = fract(vLocalUV);
    {
        float m = 2.0f / 3.0f, c = 2.0f / 3.0f;
        if ((vTileUV.x * m + c) < vTileUV.y) {
            if (fRowEven < 0.5f) vIndex.x -= 1.0f;
            fRowEven = 1.0f - fRowEven;
            vIndex.y += 1.0f;
        }
    }
    {
        float m = -2.0f / 3.0f, c = 4.0f / 3.0f;
        if ((vTileUV.x * m + c) < vTileUV.y) {
            if (fRowEven >= 0.5f) vIndex.x += 1.0f;
            fRowEven = 1.0f - fRowEven;
            vIndex.y += 1.0f;
        }
    }
    vec2 vCenter = vIndex - vec2(0.0f, -1.0f / 3.0f);
    if (fRowEven > 0.5f) vCenter.x += 0.5f;
    vec2 vDelta = vUV - vCenter;
    float d1 = vDelta.x;
    float d2 = dot(vDelta, normalize(vec2(2.0f/3.0f, 1.0f))) * 0.9f;
    float d3 = dot(vDelta, normalize(vec2(-2.0f/3.0f, 1.0f))) * 0.9f;
    float fDist = max(abs(d1), max(abs(d2), abs(d3)));
    float fTest = max(max(-d1, -d2), d3);
    return vec4(vIndex.x, vIndex.y, abs(fDist), fTest);
}

AI static vec4 CosApprox(vec4 x) {
    x = abs(fract(x * 0.5f) * 2.0f - 1.0f);
    vec4 x2 = x * x;
    return x2 * 3.0f - 2.0f * x2 * x;
}

// ============================================================
// Texture functions
// ============================================================

AI static vec3 TexNukage3(vec2 tc, float rnd) {
    float fBlend = smoothstep(0.8f, 0.0f, rnd);
    fBlend = min(fBlend, smoothstep(1.0f, 0.8f, rnd));
    fBlend *= 1.5f;
    return mix(vec3(11.0f,23.0f,7.0f), vec3(46.0f,83.0f,39.0f), fBlend) / 255.0f;
}

AI static void AddMountain(float& fShade, vec2 vUV, float rnd, float hrnd,
                           float fXPos, float fWidth, float fHeight, float fFog) {
    float fYPos = 1.0f - smoothstep(0.0f, 1.0f,
        abs(fract(fXPos - vUV.x + vUV.y*0.05f + 0.5f) - 0.5f) * fWidth);
    fYPos += hrnd * 0.05f + rnd * 0.05f;
    fYPos *= fHeight;
    float fDist = fYPos - vUV.y;
    if (fDist > 0.0f) {
        fShade = rnd * ((1.0f - clamp(sqrt(fDist)*2.0f, 0.0f, 1.0f))*0.3f + 0.1f);
        fShade = mix(fShade, 0.6f + 0.1f*rnd, fFog);
    }
}

AI static vec3 TexFSky1(vec2 tc, float rnd, float hrnd) {
    float fShade = 0.6f + 0.1f * rnd;
    vec2 vUV = tc * vec2(1.0f/256.0f, 1.0f/128.0f);
    vUV.y = 1.0f - vUV.y;
    AddMountain(fShade, vUV, rnd, hrnd, 0.25f, 1.0f, 0.85f, 0.5f);
    AddMountain(fShade, vUV, rnd, hrnd, 1.5f, 4.0f, 0.78f, 0.2f);
    AddMountain(fShade, vUV, rnd, hrnd, 1.94f, 2.51f, 0.8f, 0.0f);
    return vec3(fShade);
}

AI static vec3 TexFloor7_1(vec2 tc, float rnd) {
    return mix(vec3(51.0f,43.0f,19.0f), vec3(79.0f,59.0f,35.0f), rnd*rnd*2.5f) / 255.0f;
}

AI static vec3 TexFlat5_5(vec2 tc, float rnd) {
    vec3 col = mix(vec3(63.0f,47.0f,23.0f), vec3(147.0f,123.0f,99.0f), rnd) / 255.0f;
    col = col * (mod(tc.x, 2.0f) * 0.15f + 0.85f);
    return col;
}

AI static vec3 TexFloor4_8(vec2 tc, float rnd) {
    vec3 col = mix(vec3(30.0f), vec3(150.0f), rnd*rnd) / 255.0f;
    vec4 vHex = Hexagon(vec2(tc.y, tc.x) / 32.0f);
    float fShadow = clamp((0.5f - vHex.z)*15.0f, 0.0f, 1.0f)*0.5f + 0.5f;
    float fHighlight = 1.0f + clamp(1.0f - abs(0.45f - vHex.w)*32.0f, 0.0f, 1.0f)*0.5f;
    col = col * (clamp((0.5f - vHex.z)*2.0f, 0.0f, 1.0f)*0.25f + 0.75f);
    col = col * fHighlight;
    col = col * fShadow;
    return col;
}

AI static vec3 TexCeil3_5(vec2 tc, float rnd) {
    vec2 vTC = tc;
    vTC.x -= 17.0f;
    float yOff = 11.0f;
    if (vTC.x >= 0.0f && vTC.x < 32.0f) yOff = 58.0f;
    vTC.y -= yOff;
    vTC.x = mod(vTC.x, 32.0f);
    vTC.y = mod(vTC.y, 64.0f);
    vec2 vBoxClosest = clamp(vTC, vec2(4.0f), vec2(28.0f, 60.0f));
    vec2 vDelta = vTC - vBoxClosest;
    float fDist2 = dot(vDelta, vDelta);
    float fMed1 = 55.0f / 255.0f;
    float fShade = fMed1;
    fShade = mix(fShade, 59.0f/255.0f, smoothstep(0.6f, 0.45f, rnd));
    fShade = mix(fShade, 47.0f/255.0f, smoothstep(0.45f, 0.35f, rnd));
    fShade = mix(fShade, 47.0f/255.0f, step(1.5f, fDist2));
    fShade = mix(fShade, 39.0f/255.0f, step(13.5f, fDist2));
    vec3 col = vec3(fShade);
    if (vTC.x < 12.0f || vTC.x > 20.0f || vTC.y < 12.0f || vTC.y > 52.0f) {
        float fRRow = floor(mod(vTC.y - 3.5f, 7.5f));
        float fRCol = mod(vTC.x - 15.0f, 10.0f);
        if (fRRow == 2.0f && fRCol == 0.0f) col = col - 0.05f;
        if (fRRow <= 2.0f && fRCol <= 2.0f) {
            vec2 vOff(fRRow - 1.0f, fRCol - 1.0f);
            float d2 = dot(vOff, vOff) / 2.0f;
            col = col + clamp(1.0f - d2, 0.0f, 1.0f) * 0.05f;
        }
    }
    return col;
}

AI static vec3 TexFlat14(vec2 tc, float rnd) {
    return mix(vec3(0.0f, 0.0f, 35.0f/255.0f), vec3(0.0f, 0.0f, 200.0f/255.0f), rnd*rnd);
}

AI static vec3 TexDoor3(vec2 tc, float rnd, float hrnd) {
    float fStreak = clamp(abs(fract(hrnd + rnd) - 0.5f)*3.0f, 0.0f, 1.0f);
    fStreak = fStreak * fStreak;
    float fShade = 1.0f - abs((tc.y / 72.0f) - 0.5f) * 2.0f;
    fShade = fShade * fShade * 0.2f + 0.3f;
    fShade *= (hrnd * 0.2f + 0.8f);
    fShade *= Indent(tc, vec2(8.0f), vec2(56.0f, 56.0f), 0.8f, 1.2f);
    fShade += rnd * 0.1f;
    float fStreakTop = smoothstep(32.0f, 0.0f, tc.y);
    float fStreakBot = smoothstep(40.0f, 72.0f, tc.y);
    fShade *= 1.0f - fStreak * max(fStreakTop, fStreakBot) * 0.2f;
    return vec3(fShade);
}

// --- Approximated textures (source was truncated) ---

AI static vec3 TexLite3(vec2 tc, float rnd) {
    float u = mod(tc.x, 16.0f) / 16.0f;
    float v = mod(tc.y, 128.0f) / 128.0f;
    float bright = smoothstep(0.3f, 0.5f, 1.0f - abs(u - 0.5f)*2.0f);
    bright *= smoothstep(0.1f, 0.3f, v) * smoothstep(0.9f, 0.7f, v);
    return mix(vec3(0.12f, 0.12f, 0.10f), vec3(0.85f, 0.80f, 0.65f), bright);
}

AI static vec3 TexStarTan3(vec2 tc, float rnd) {
    float panel = step(mod(tc.x, 64.0f), 1.0f) * 0.12f;
    float vline = step(mod(tc.y, 32.0f), 0.5f) * 0.06f;
    float shade = rnd * 0.4f + 0.4f - panel - vline;
    return vec3(shade * 0.65f, shade * 0.50f, shade * 0.35f);
}

AI static vec3 TexBrown1(vec2 tc, float rnd) {
    float shade = rnd * 0.35f + 0.25f;
    shade *= 1.0f - step(mod(tc.y, 64.0f), 0.5f) * 0.08f;
    return vec3(shade * 0.60f, shade * 0.45f, shade * 0.30f);
}

AI static vec3 TexDoorStop(vec2 tc, float rnd) {
    float shade = rnd * 0.2f + 0.35f;
    shade *= 1.0f - step(mod(tc.x, 8.0f), 0.5f) * 0.1f;
    return vec3(shade * 0.55f, shade * 0.43f, shade * 0.30f);
}

AI static vec3 TexCompute2(vec2 tc, float rnd) {
    float screen = step(4.0f, mod(tc.x, 64.0f)) * step(mod(tc.x, 64.0f), 60.0f)
                 * step(4.0f, mod(tc.y, 128.0f)) * step(mod(tc.y, 128.0f), 124.0f);
    vec3 frame = vec3(0.25f, 0.22f, 0.18f);
    float scanline = (sin(tc.y * 3.14159f * 2.0f) * 0.1f + 0.9f);
    vec3 scr = vec3(0.05f, 0.25f * rnd * scanline, 0.12f * scanline);
    return mix(frame, scr, screen);
}

AI static vec3 TexStep2(vec2 tc, float rnd) {
    return mix(vec3(0.30f), vec3(0.55f), rnd) * vec3(0.85f, 0.80f, 0.75f);
}

AI static vec3 TexStep6(vec2 tc, float rnd) {
    float edge = step(mod(tc.y, 16.0f), 1.0f) * 0.1f;
    return (mix(vec3(0.28f), vec3(0.50f), rnd) - edge) * vec3(0.80f, 0.75f, 0.70f);
}

AI static vec3 TexTlite6_1(vec2 tc, float rnd) {
    float v = mod(tc.y, 16.0f) / 16.0f;
    float bright = smoothstep(0.2f, 0.5f, 1.0f - abs(v - 0.5f) * 2.0f);
    return mix(vec3(0.15f), vec3(0.90f, 0.85f, 0.70f), bright);
}

AI static vec3 TexTlite6_4(vec2 tc, float rnd) {
    return TexTlite6_1(tc, rnd);
}

AI static vec3 TexFloor7_2(vec2 tc, float rnd) {
    return mix(vec3(59.0f,47.0f,23.0f), vec3(91.0f,71.0f,43.0f), rnd*rnd*2.0f) / 255.0f;
}

AI static vec3 TexBrown144(vec2 tc, float rnd) {
    float shade = rnd * 0.3f + 0.3f;
    return vec3(shade * 0.58f, shade * 0.42f, shade * 0.28f);
}

AI static vec3 TexSupport2(vec2 tc, float rnd) {
    return mix(vec3(0.25f), vec3(0.50f), rnd) * vec3(0.75f, 0.70f, 0.65f);
}

AI static vec3 TexStarg3(vec2 tc, float rnd) {
    float panel = step(mod(tc.x, 64.0f), 1.0f) * 0.08f;
    float shade = rnd * 0.3f + 0.35f - panel;
    return vec3(shade * 0.70f, shade * 0.68f, shade * 0.65f);
}

AI static vec3 TexDoorTrak(vec2 tc, float rnd) {
    return mix(vec3(0.15f), vec3(0.35f), rnd) * vec3(0.70f, 0.65f, 0.60f);
}

AI static vec3 TexSladWall(vec2 tc, float rnd) {
    float shade = rnd * 0.3f + 0.25f;
    return vec3(shade * 0.45f, shade * 0.55f, shade * 0.35f);
}

AI static vec3 TexTekWall4(vec2 tc, float rnd) {
    float panel = step(2.0f, mod(tc.x, 64.0f)) * step(mod(tc.x, 64.0f), 62.0f)
                * step(2.0f, mod(tc.y, 128.0f)) * step(mod(tc.y, 128.0f), 126.0f);
    return mix(vec3(0.28f, 0.26f, 0.24f), vec3(0.45f, 0.42f, 0.38f), rnd * panel);
}

AI static vec3 TexSW1Comp(vec2 tc, float rnd) {
    return TexTekWall4(tc, rnd) + vec3(0.1f, 0.0f, 0.0f);
}

AI static vec3 TexBigDoor2(vec2 tc, float rnd) {
    float shade = rnd * 0.25f + 0.30f;
    float panel = step(8.0f, mod(tc.x, 128.0f)) * step(mod(tc.x, 128.0f), 120.0f);
    shade = shade * (panel * 0.2f + 0.8f);
    return vec3(shade * 0.55f, shade * 0.52f, shade * 0.48f);
}

AI static vec3 TexBar1A(vec2 tc, float rnd) {
    float stripe = step(mod(tc.y, 11.0f), 3.0f);
    return mix(vec3(0.15f, 0.35f, 0.10f), vec3(0.40f, 0.28f, 0.12f), stripe*0.6f + rnd*0.3f);
}

AI static vec3 TexPlayW(vec2 tc, float rnd) {
    return mix(vec3(0.35f, 0.22f, 0.15f), vec3(0.55f, 0.35f, 0.25f), rnd*0.8f);
}

// ============================================================
// SampleTexture
// ============================================================

AI static vec3 SampleTexture(float fTex, vec2 vUV) {
    vec2 tc = vUV;
#ifdef PIXELATE_TEXTURES
    tc = floor(tc);
#endif
    float rnd = fbm(tc, 0.5f);
    float hrnd = noise1D(tc.y * 0.1f);

    vec3 vResult(0.0f);
    if (fTex < 0.5f) return vResult; // TEX_X
    if (fTex < 1.5f) return TexFSky1(tc, rnd, hrnd);
    if (fTex < 2.5f) return TexNukage3(tc, rnd);
    if (fTex < 3.5f) return TexFloor7_1(tc, rnd);
    if (fTex < 4.5f) return TexFlat5_5(tc, rnd);
    if (fTex < 5.5f) return TexFloor4_8(tc, rnd);
    if (fTex < 6.5f) return TexCeil3_5(tc, rnd);
    if (fTex < 7.5f) return TexTlite6_4(tc, rnd);
    if (fTex < 8.5f) return TexFlat14(tc, rnd);
    if (fTex < 9.5f) return TexFloor7_2(tc, rnd);
    if (fTex < 10.5f) return TexStep2(tc, rnd);
    if (fTex < 11.5f) return TexTlite6_1(tc, rnd);
    if (fTex < 12.5f) return TexDoor3(tc, rnd, hrnd);
    if (fTex < 13.5f) return TexLite3(tc, rnd);
    if (fTex < 14.5f) return TexStarTan3(tc, rnd);
    if (fTex < 15.5f) return TexBrown1(tc, rnd);
    if (fTex < 16.5f) return TexDoorStop(tc, rnd);
    if (fTex < 17.5f) return TexCompute2(tc, rnd);
    if (fTex < 18.5f) return TexStep6(tc, rnd);
    if (fTex < 19.5f) return TexBrown144(tc, rnd);
    if (fTex < 20.5f) return TexSupport2(tc, rnd);
    if (fTex < 21.5f) return TexStarg3(tc, rnd);
    if (fTex < 22.5f) return TexDoorTrak(tc, rnd);
    if (fTex < 23.5f) return TexSladWall(tc, rnd);
    if (fTex < 24.5f) return TexTekWall4(tc, rnd);
    if (fTex < 25.5f) return TexSW1Comp(tc, rnd);
    if (fTex < 26.5f) return TexBigDoor2(tc, rnd);
    if (fTex < 32.5f) return TexBar1A(tc, rnd);
    if (fTex < 33.5f) return TexPlayW(tc, rnd);

#ifdef QUANTIZE_TEXTURES
    vResult = Quantize(vResult);
#endif
    return vResult;
}

// ============================================================
// Geometry primitives
// ============================================================

AI static float Cross2d(vec2 vA, vec2 vB) {
    return vA.x * vB.y - vA.y * vB.x;
}

AI static void BeginSector(vec4& vSS, vec2 vSH, const Ray& r) {
    vec2 tmp = (vSH - r.vRayOrigin.y) / r.vRayDir.y;
    vSS.x = tmp.x; vSS.y = tmp.y;
    vSS.z = 0.0f; vSS.w = 0.0f;
}

AI static void Null(vec4& vSS, int iAx, int iAy, int iBx, int iBy, const Ray& r) {
    vec2 vA((float)iAx, (float)iAy);
    vec2 vB((float)iBx, (float)iBy);
    vec2 vD = vB - vA;
    vec2 rOxz(r.vRayOrigin.x, r.vRayOrigin.z);
    vec2 rDxz(r.vRayDir.x, r.vRayDir.z);
    vec2 vOA = vA - rOxz;
    float fDenom = Cross2d(rDxz, vD);
    float fRcpDenom = 1.0f / fDenom;
    float fHitT = Cross2d(vOA, vD) * fRcpDenom;
    float fHitU = Cross2d(vOA, rDxz) * fRcpDenom;
    if (fHitT > 0.0f && fHitU >= 0.0f && fHitU < 1.0f) {
        vec2 s = step(vec2(vSS.x, vSS.y), vec2(fHitT));
        vSS.z += s.x; vSS.w += s.y;
    }
}

AI static void Wall(float& fT, vec4& vInf, vec4& vSS,
    int iAx, int iAy, int iBx, int iBy, int iLen,
    float fLt, vec2 vSH, float fTex, const Ray& r)
{
    vec2 vA((float)iAx, (float)iAy);
    vec2 vB((float)iBx, (float)iBy);
    float fLen = (float)iLen;
    vec2 vD = vB - vA;
    vec2 rOxz(r.vRayOrigin.x, r.vRayOrigin.z);
    vec2 rDxz(r.vRayDir.x, r.vRayDir.z);
    vec2 vOA = vA - rOxz;
    float fDenom = Cross2d(rDxz, vD);
    float fRcpDenom = 1.0f / fDenom;
    float fHitT = Cross2d(vOA, vD) * fRcpDenom;
    float fHitU = Cross2d(vOA, rDxz) * fRcpDenom;
    if (fHitT > 0.0f && fHitU >= 0.0f && fHitU < 1.0f) {
        vec2 s = step(vec2(vSS.x, vSS.y), vec2(fHitT));
        vSS.z += s.x; vSS.w += s.y;
        if (fHitT < fT && fDenom < 0.0f) {
            float fHitY = r.vRayDir.y * fHitT + r.vRayOrigin.y;
            if (fHitY > vSH.x && fHitY < vSH.y) {
                fT = fHitT;
                vInf = vec4(fHitU * fLen, fHitY, fLt, fTex);
            }
        }
    }
}

AI static void Open(float& fT, vec4& vInf, vec4& vSS,
    int iAx, int iAy, int iBx, int iBy, int iLen,
    float fLt, vec2 vSH, int iLH, int iUH,
    float fLTex, float fUTex, const Ray& r)
{
    vec2 vA((float)iAx, (float)iAy);
    vec2 vB((float)iBx, (float)iBy);
    float fLen = (float)iLen;
    float fUH = (float)iUH, fLH = (float)iLH;
    vec2 vD = vB - vA;
    vec2 rOxz(r.vRayOrigin.x, r.vRayOrigin.z);
    vec2 rDxz(r.vRayDir.x, r.vRayDir.z);
    vec2 vOA = vA - rOxz;
    float fDenom = Cross2d(rDxz, vD);
    float fRcpDenom = 1.0f / fDenom;
    float fHitT = Cross2d(vOA, vD) * fRcpDenom;
    float fHitU = Cross2d(vOA, rDxz) * fRcpDenom;
    if (fHitT > 0.0f && fHitU >= 0.0f && fHitU < 1.0f) {
        vec2 s = step(vec2(vSS.x, vSS.y), vec2(fHitT));
        vSS.z += s.x; vSS.w += s.y;
        if (fHitT < fT && fDenom < 0.0f) {
            float fHitY = r.vRayDir.y * fHitT + r.vRayOrigin.y;
            if (fHitY > vSH.x && fHitY < vSH.y) {
                if (fHitY < fLH) {
                    fT = fHitT;
                    vInf = vec4(fHitU*fLen, fHitY - fLH, fLt, fLTex);
                }
                if (fHitY > fUH) {
                    fT = fHitT;
                    vInf = vec4(fHitU*fLen, fHitY - fUH, fLt, fUTex);
                }
            }
        }
    }
}

AI static void Upper(float& fT, vec4& vInf, vec4& vSS,
    int iAx, int iAy, int iBx, int iBy, int iLen,
    float fLt, vec2 vSH, int iUH, float fUTex, const Ray& r)
{
    vec2 vA((float)iAx, (float)iAy);
    vec2 vB((float)iBx, (float)iBy);
    float fLen = (float)iLen, fUH = (float)iUH;
    vec2 vD = vB - vA;
    vec2 rOxz(r.vRayOrigin.x, r.vRayOrigin.z);
    vec2 rDxz(r.vRayDir.x, r.vRayDir.z);
    vec2 vOA = vA - rOxz;
    float fDenom = Cross2d(rDxz, vD);
    float fRcpDenom = 1.0f / fDenom;
    float fHitT = Cross2d(vOA, vD) * fRcpDenom;
    float fHitU = Cross2d(vOA, rDxz) * fRcpDenom;
    if (fHitT > 0.0f && fHitU >= 0.0f && fHitU < 1.0f) {
        vec2 s = step(vec2(vSS.x, vSS.y), vec2(fHitT));
        vSS.z += s.x; vSS.w += s.y;
        if (fHitT < fT && fDenom < 0.0f) {
            float fHitY = r.vRayDir.y * fHitT + r.vRayOrigin.y;
            if (fHitY < vSH.y && fHitY > fUH) {
                fT = fHitT;
                vInf = vec4(fHitU*fLen, fHitY - fUH, fLt, fUTex);
            }
        }
    }
}

AI static void Lower(float& fT, vec4& vInf, vec4& vSS,
    int iAx, int iAy, int iBx, int iBy, int iLen,
    float fLt, vec2 vSH, int iLH, float fLTex, const Ray& r)
{
    vec2 vA((float)iAx, (float)iAy);
    vec2 vB((float)iBx, (float)iBy);
    float fLen = (float)iLen, fLH = (float)iLH;
    vec2 vD = vB - vA;
    vec2 rOxz(r.vRayOrigin.x, r.vRayOrigin.z);
    vec2 rDxz(r.vRayDir.x, r.vRayDir.z);
    vec2 vOA = vA - rOxz;
    float fDenom = Cross2d(rDxz, vD);
    float fRcpDenom = 1.0f / fDenom;
    float fHitT = Cross2d(vOA, vD) * fRcpDenom;
    float fHitU = Cross2d(vOA, rDxz) * fRcpDenom;
    if (fHitT > 0.0f && fHitU >= 0.0f && fHitU < 1.0f) {
        vec2 s = step(vec2(vSS.x, vSS.y), vec2(fHitT));
        vSS.z += s.x; vSS.w += s.y;
        if (fHitT < fT && fDenom < 0.0f) {
            float fHitY = r.vRayDir.y * fHitT + r.vRayOrigin.y;
            if (fHitY > vSH.x && fHitY < fLH) {
                fT = fHitT;
                vInf = vec4(fHitU*fLen, fHitY - fLH, fLt, fLTex);
            }
        }
    }
}

AI static void EndSector(float& fT, vec4& vInf, vec4 vSS,
    float fLt, vec2 vFCTex, const Ray& r)
{
    vec2 vInOut = fract(vec2(vSS.z, vSS.w) * 0.5f) * 2.0f;
    if (fT > vSS.x) {
        if (vInOut.x > 0.5f && vSS.x > 0.0f) {
            vec3 vFloorPos = r.vRayOrigin + r.vRayDir * vSS.x;
            if (r.vRayOrigin.y > vFloorPos.y) {
                fT = vSS.x;
                vInf = vec4(vFloorPos.x, vFloorPos.z, fLt, vFCTex.x);
            }
        }
    }
    if (fT > vSS.y) {
        if (vInOut.y > 0.5f && vSS.y > 0.0f) {
            vec3 vCeilPos = r.vRayOrigin + r.vRayDir * vSS.y;
            if (r.vRayOrigin.y < vCeilPos.y) {
                fT = vSS.y;
                vInf = vec4(vCeilPos.x, vCeilPos.z, fLt, vFCTex.y);
            }
        }
    }
}

// ============================================================
// Sprites
// ============================================================

#ifdef ENABLE_SPRITES

AI static void Sprite(float& fST, vec4& vSHI, vec2 vSpriteDir,
    int iX, int iY, int iZ, vec2 vSize, float fLt, float fTex, const Ray& r)
{
    vec3 vPos((float)iX, (float)iY, (float)iZ);
    fST = FAR_CLIP;
    vSHI = vec4(0.0f);
    vec2 vA = vec2(vPos.x, vPos.z) - vSpriteDir * 0.5f * vSize.x;
    vec2 vB = vec2(vPos.x, vPos.z) + vSpriteDir * 0.5f * vSize.x;
    vec2 vD = vB - vA;
    vec2 rOxz(r.vRayOrigin.x, r.vRayOrigin.z);
    vec2 rDxz(r.vRayDir.x, r.vRayDir.z);
    vec2 vOA = vA - rOxz;
    float rcpdenom = 1.0f / Cross2d(rDxz, vD);
    float fHitT = Cross2d(vOA, vD) * rcpdenom;
    if (fHitT > 0.0f) {
        float fHitU = Cross2d(vOA, rDxz) * rcpdenom;
        if (fHitU >= 0.0f && fHitU < 1.0f) {
            float fHitY = r.vRayDir.y * fHitT + r.vRayOrigin.y;
            if (fHitT < fST && fHitY > vPos.y && fHitY < (vPos.y + vSize.y)) {
                fST = fHitT;
                vSHI = vec4(fHitU * vSize.x, fHitY - vPos.y, fLt, fTex);
            }
        }
    }
}

AI static bool MaskBarrel(vec2 tc) {
    vec2 vSize(23.0f, 32.0f);
    tc = floor(tc);
    vec2 vWrap = fract((tc + vec2(2.0f, 1.0f)) / vSize) * vSize;
    return (vWrap.x >= 4.0f) || (vWrap.y >= 2.0f);
}

AI static bool MaskCorpseSprite(vec2 tc) {
    vec2 vUV = tc / vec2(57.0f, 22.0f);
    vec2 vOffset = vUV * 2.0f - vec2(1.0f, 0.8f);
    float fDist = dot(vOffset, vOffset);
    vec4 ca = CosApprox(vec4(tc.x, tc.y, tc.x, tc.y) * vec4(0.55f, 0.41f, 0.25f, 0.1f));
    fDist += dot(ca, vec4(0.2f * -vOffset.y));
    return fDist < 0.4f;
}

AI static void BarrelSprite(float& fT, vec4& vInf, vec2 vSpriteDir,
    int iX, int iY, int iZ, float fLt, const Ray& r)
{
    float fST; vec4 vSHI;
    Sprite(fST, vSHI, vSpriteDir, iX, iY, iZ, vec2(23.0f, 32.0f), fLt, TEX_BAR1A, r);
    if (fST < fT && MaskBarrel(vec2(vSHI.x, vSHI.y))) {
        fT = fST; vInf = vSHI;
    }
}

AI static void CorpseSprite(float& fT, vec4& vInf, vec2 vSpriteDir,
    int iX, int iY, int iZ, float fLt, const Ray& r)
{
    float fST; vec4 vSHI;
    Sprite(fST, vSHI, vSpriteDir, iX, iY, iZ, vec2(57.0f, 22.0f), fLt, TEX_PLAYW, r);
    if (fST < fT && MaskCorpseSprite(vec2(vSHI.x, vSHI.y))) {
        fT = fST; vInf = vSHI;
    }
}

#endif // ENABLE_SPRITES

// ============================================================
// Sectors
// ============================================================

#ifdef ENABLE_NUKAGE_SECTORS
AI static void Sector0(float& fT, vec4& vInf, const Ray& r) {
    vec4 vSS; float fLt=1.0f; vec2 vSH(-80.0f, 216.0f);
    BeginSector(vSS, vSH, r);
    Lower(fT,vInf,vSS,1520,-3168,1672,-3104,164,fLt,vSH,-56,TEX_BROWN144,r);
    Lower(fT,vInf,vSS,1672,-3104,1896,-3104,224,fLt-kC,vSH,-56,TEX_BROWN144,r);
    Lower(fT,vInf,vSS,1896,-3104,2040,-3144,149,fLt,vSH,-56,TEX_BROWN144,r);
    Lower(fT,vInf,vSS,2040,-3144,2128,-3272,155,fLt,vSH,-56,TEX_BROWN144,r);
    Lower(fT,vInf,vSS,2128,-3272,2064,-3408,150,fLt,vSH,-56,TEX_BROWN144,r);
    Lower(fT,vInf,vSS,2064,-3408,1784,-3448,282,fLt,vSH,-56,TEX_BROWN144,r);
    Lower(fT,vInf,vSS,1784,-3448,1544,-3384,248,fLt,vSH,-56,TEX_BROWN144,r);
    Lower(fT,vInf,vSS,1544,-3384,1520,-3168,217,fLt,vSH,-56,TEX_BROWN144,r);
    EndSector(fT,vInf,vSS,fLt,vec2(TEX_NUKAGE3,TEX_F_SKY1),r);
}

AI static void Sector1(float& fT, vec4& vInf, const Ray& r) {
    vec4 vSS; float fLt=1.0f; vec2 vSH(-56.0f, 216.0f);
    BeginSector(vSS, vSH, r);
    Open(fT,vInf,vSS,1376,-3200,1376,-3104,96,fLt+kC,vSH,8,192,TEX_STARTAN3,TEX_STARTAN3,r);
    Open(fT,vInf,vSS,1376,-3360,1376,-3264,96,fLt+kC,vSH,8,192,TEX_STARTAN3,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,1376,-3264,1376,-3200,64,fLt+kC,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,1376,-3104,1376,-2944,160,fLt+kC,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,1376,-2944,1472,-2880,115,fLt,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,1856,-2880,1920,-2920,75,fLt,vSH,TEX_STARTAN3,r);
    Null(vSS,1520,-3168,1672,-3104,r);
    Null(vSS,1672,-3104,1896,-3104,r);
    Null(vSS,1896,-3104,2040,-3144,r);
    Null(vSS,2040,-3144,2128,-3272,r);
    Null(vSS,2128,-3272,2064,-3408,r);
    Null(vSS,2064,-3408,1784,-3448,r);
    Null(vSS,1784,-3448,1544,-3384,r);
    Null(vSS,1544,-3384,1520,-3168,r);
    Wall(fT,vInf,vSS,2736,-3360,2736,-3648,288,fLt+kC,vSH,TEX_STARTAN3,r);
#ifdef CLOSE_NUKAGE_SECTOR
    Wall(fT,vInf,vSS,2736,-3648,1376,-3648,1360,fLt,vSH,TEX_STARTAN3,r);
#else
    Null(vSS,2736,-3648,2240,-3648,r);
    Null(vSS,2240,-3648,1984,-3648,r);
    Null(vSS,1984,-3648,1376,-3648,r);
#endif
    Wall(fT,vInf,vSS,2240,-2920,2272,-3008,93,fLt,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,2272,-3008,2432,-3112,190,fLt,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,2432,-3112,2736,-3112,304,fLt-kC,vSH,TEX_STARTAN3,r);
    Open(fT,vInf,vSS,2736,-3112,2736,-3360,248,fLt+kC,vSH,0,136,TEX_STARTAN3,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,1376,-3648,1376,-3360,288,fLt+kC,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,1472,-2880,1856,-2880,384,fLt-kC,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,1920,-2920,2240,-2920,320,fLt-kC,vSH,TEX_STARTAN3,r);
    EndSector(fT,vInf,vSS,fLt,vec2(TEX_FLOOR7_1,TEX_F_SKY1),r);
}
#endif

#ifdef ENABLE_START_SECTORS
AI static void Sector3(float& fT, vec4& vInf, const Ray& r) {
    vec4 vSS; float fLt=1.0f; vec2 vSH(8.0f, 192.0f);
    BeginSector(vSS, vSH, r);
    Null(vSS,1344,-3264,1344,-3360,r);
    Null(vSS,1376,-3360,1376,-3264,r);
    Wall(fT,vInf,vSS,1344,-3264,1376,-3264,32,fLt-kC,vSH,TEX_DOORSTOP,r);
    Wall(fT,vInf,vSS,1376,-3360,1344,-3360,32,fLt-kC,vSH,TEX_DOORSTOP,r);
    EndSector(fT,vInf,vSS,fLt,vec2(TEX_FLAT5_5,TEX_FLAT5_5),r);
}

AI static void Sector5(float& fT, vec4& vInf, const Ray& r) {
    vec4 vSS; float fLt=1.0f; vec2 vSH(8.0f, 192.0f);
    BeginSector(vSS, vSH, r);
    Null(vSS,1344,-3104,1344,-3200,r);
    Null(vSS,1376,-3200,1376,-3104,r);
    Wall(fT,vInf,vSS,1376,-3200,1344,-3200,32,fLt-kC,vSH,TEX_DOORSTOP,r);
    Wall(fT,vInf,vSS,1344,-3104,1376,-3104,32,fLt-kC,vSH,TEX_DOORSTOP,r);
    EndSector(fT,vInf,vSS,fLt,vec2(TEX_FLAT5_5,TEX_FLAT5_5),r);
}

AI static void Sector24(float& fT, vec4& vInf, const Ray& r) {
    vec4 vSS; float fLt=0.565f; vec2 vSH(0.0f, 144.0f);
    BeginSector(vSS, vSH, r);
    Wall(fT,vInf,vSS,1216,-2880,1248,-2528,353,fLt,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,1384,-2592,1344,-2880,290,fLt,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,1472,-2560,1384,-2592,93,fLt,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,1248,-2528,1472,-2432,243,fLt,vSH,TEX_STARTAN3,r);
    Upper(fT,vInf,vSS,1344,-2880,1216,-2880,128,fLt-kC,vSH,72,TEX_STARTAN3,r);
    Upper(fT,vInf,vSS,1472,-2432,1472,-2560,128,fLt+kC,vSH,88,TEX_STARTAN3,r);
    EndSector(fT,vInf,vSS,fLt,vec2(TEX_FLOOR4_8,TEX_CEIL3_5),r);
}

AI static void Sector27(float& fT, vec4& vInf, const Ray& r) {
    vec4 vSS; float fLt=0.878f; vec2 vSH(-16.0f, 200.0f);
    BeginSector(vSS, vSH, r);
    Wall(fT,vInf,vSS,1216,-3392,1216,-3360,32,fLt+kC,vSH,TEX_BROWN1,r);
    Wall(fT,vInf,vSS,1216,-3360,1184,-3360,32,fLt-kC,vSH,TEX_BROWN1,r);
    Wall(fT,vInf,vSS,1184,-3104,1216,-3104,32,fLt-kC,vSH,TEX_BROWN1,r);
    Wall(fT,vInf,vSS,1216,-3104,1216,-3072,32,fLt+kC,vSH,TEX_BROWN1,r);
    Open(fT,vInf,vSS,1344,-3264,1344,-3360,96,fLt+kC,vSH,8,192,TEX_STARTAN3,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,1344,-3200,1344,-3264,64,fLt+kC,vSH,TEX_STARTAN3,r);
    Open(fT,vInf,vSS,1344,-3104,1344,-3200,96,fLt+kC,vSH,8,192,TEX_STARTAN3,TEX_STARTAN3,r);
    Open(fT,vInf,vSS,1344,-3360,1216,-3392,131,fLt,vSH,0,72,TEX_STEP6,TEX_STARTAN3,r);
    Open(fT,vInf,vSS,1216,-3072,1344,-3104,131,fLt,vSH,0,72,TEX_STEP6,TEX_STARTAN3,r);
    Open(fT,vInf,vSS,928,-3104,1184,-3104,256,fLt-kC,vSH,-8,120,TEX_STEP6,TEX_STARTAN3,r);
    Open(fT,vInf,vSS,1184,-3360,928,-3360,256,fLt-kC,vSH,-8,120,TEX_STEP6,TEX_STARTAN3,r);
    Open(fT,vInf,vSS,928,-3360,928,-3104,256,fLt+kC,vSH,-8,120,TEX_STEP6,TEX_STARTAN3,r);
    EndSector(fT,vInf,vSS,fLt,vec2(TEX_FLAT14,TEX_CEIL3_5),r);
}

AI static void Sector28(float& fT, vec4& vInf, const Ray& r) {
    vec4 vSS; float fLt=0.753f; vec2 vSH(-8.0f, 120.0f);
    BeginSector(vSS, vSH, r);
    Wall(fT,vInf,vSS,928,-3392,928,-3360,32,fLt+kC,vSH,TEX_BROWN1,r);
    Wall(fT,vInf,vSS,928,-3360,896,-3360,32,fLt-kC,vSH,TEX_BROWN1,r);
    Wall(fT,vInf,vSS,1184,-3360,1184,-3392,32,fLt+kC,vSH,TEX_BROWN1,r);
    Wall(fT,vInf,vSS,896,-3104,928,-3104,32,fLt-kC,vSH,TEX_BROWN1,r);
    Wall(fT,vInf,vSS,928,-3104,928,-3072,32,fLt+kC,vSH,TEX_BROWN1,r);
    Wall(fT,vInf,vSS,1184,-3072,1184,-3104,32,fLt+kC,vSH,TEX_BROWN1,r);
    Open(fT,vInf,vSS,1184,-3392,928,-3392,256,fLt-kC,vSH,0,72,TEX_STEP6,TEX_COMPUTE2,r);
    Null(vSS,928,-3104,1184,-3104,r);
    Null(vSS,1184,-3360,928,-3360,r);
    Null(vSS,928,-3360,928,-3104,r);
    Open(fT,vInf,vSS,896,-3360,896,-3104,256,fLt+kC,vSH,0,72,TEX_STEP6,TEX_COMPUTE2,r);
    Open(fT,vInf,vSS,928,-3072,1184,-3072,256,fLt-kC,vSH,0,72,TEX_STEP6,TEX_COMPUTE2,r);
    EndSector(fT,vInf,vSS,fLt,vec2(TEX_FLAT14,TEX_CEIL3_5),r);
}

AI static void Sector29(float& fT, vec4& vInf, const Ray& r) {
    vec4 vSS; float fLt=0.565f; vec2 vSH(0.0f, 72.0f);
    BeginSector(vSS, vSH, r);
    Wall(fT,vInf,vSS,1152,-3648,1088,-3648,64,fLt-kC,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,1024,-3648,960,-3648,64,fLt-kC,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,1280,-3552,1152,-3648,160,fLt,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,960,-3648,832,-3552,160,fLt,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,1344,-3552,1280,-3552,64,fLt-kC,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,832,-3552,704,-3552,128,fLt-kC,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,896,-3392,928,-3392,32,fLt-kC,vSH,TEX_BROWN1,r);
    Wall(fT,vInf,vSS,896,-3360,896,-3392,32,fLt+kC,vSH,TEX_BROWN1,r);
    Wall(fT,vInf,vSS,1184,-3392,1216,-3392,32,fLt-kC,vSH,TEX_BROWN1,r);
    Wall(fT,vInf,vSS,896,-3072,896,-3104,32,fLt+kC,vSH,TEX_BROWN1,r);
    Wall(fT,vInf,vSS,928,-3072,896,-3072,32,fLt-kC,vSH,TEX_BROWN1,r);
    Wall(fT,vInf,vSS,1216,-3072,1184,-3072,32,fLt-kC,vSH,TEX_BROWN1,r);
    Wall(fT,vInf,vSS,1344,-2880,1344,-3104,224,fLt+kC,vSH,TEX_STARTAN3,r);
    Null(vSS,1184,-3392,928,-3392,r);
    Null(vSS,1344,-3360,1216,-3392,r);
    Null(vSS,1216,-3072,1344,-3104,r);
    Wall(fT,vInf,vSS,704,-2944,832,-2944,128,fLt-kC,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,832,-2944,968,-2880,150,fLt,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,968,-2880,1216,-2880,248,fLt-kC,vSH,TEX_STARTAN3,r);
    Null(vSS,1088,-3648,1024,-3648,r);
    Null(vSS,896,-3360,896,-3104,r);
    Null(vSS,928,-3072,1184,-3072,r);
#ifdef CLOSE_START_SECTOR
    Wall(fT,vInf,vSS,704,-3552,704,-2944,608,fLt+kC,vSH,TEX_STARTAN3,r);
#else
    Wall(fT,vInf,vSS,704,-3552,704,-3360,192,fLt+kC,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,704,-3104,704,-2944,160,fLt+kC,vSH,TEX_STARTAN3,r);
    Null(vSS,704,-3104,704,-3360,r);
#endif
    Null(vSS,1344,-2880,1216,-2880,r);
    Wall(fT,vInf,vSS,1344,-3360,1344,-3392,32,fLt+kC,vSH,TEX_STARTAN3,r);
    Wall(fT,vInf,vSS,1344,-3392,1344,-3552,160,fLt+kC,vSH,TEX_STARTAN3,r);
    EndSector(fT,vInf,vSS,fLt,vec2(TEX_FLOOR4_8,TEX_CEIL3_5),r);
}

AI static void Sector30(float& fT, vec4& vInf, const Ray& r, float iTime) {
    vec4 vSS;
    float fLt = (hash(floor(iTime * 10.0f)) > 0.3f) ? 0.565f : 1.0f;
    vec2 vSH(0.0f, 72.0f);
    BeginSector(vSS, vSH, r);
    Wall(fT,vInf,vSS,1088,-3680,1024,-3680,64,fLt-kC,vSH,TEX_DOOR3,r);
    Wall(fT,vInf,vSS,1024,-3680,1024,-3648,32,fLt+kC,vSH,TEX_LITE3,r);
    Wall(fT,vInf,vSS,1088,-3648,1088,-3680,32,fLt+kC,vSH,TEX_LITE3,r);
    Null(vSS,1088,-3648,1024,-3648,r);
    EndSector(fT,vInf,vSS,fLt,vec2(TEX_FLOOR4_8,TEX_CEIL3_5),r);
}
#endif // ENABLE_START_SECTORS

// ============================================================
// MapIntersect
// ============================================================

AI static void MapIntersect(float& fT, vec4& vInf, const Ray& r, float iTime) {
    vInf = vec4(0.0f);
    fT = FAR_CLIP;
#ifdef ENABLE_NUKAGE_SECTORS
    Sector0(fT, vInf, r);
    Sector1(fT, vInf, r);
#endif
#ifdef ENABLE_START_SECTORS
    Sector3(fT, vInf, r);
    Sector5(fT, vInf, r);
    Sector24(fT, vInf, r);
    Sector27(fT, vInf, r);
    Sector28(fT, vInf, r);
    Sector29(fT, vInf, r);
    Sector30(fT, vInf, r, iTime);
#endif
}

// ============================================================
// Camera
// ============================================================

AI static vec3 GetCameraRayDir(vec2 vWindow, vec3 vCamPos, vec3 vCamTgt) {
    vec3 vFwd = normalize(vCamTgt - vCamPos);
    vec3 vRight = normalize(cross(vec3(0.0f, 1.0f, 0.0f), vFwd));
    vec3 vUp = normalize(cross(vFwd, vRight));
    return normalize(vRight * vWindow.x + vUp * vWindow.y + vFwd * 1.8f);
}

AI static void DemoCamera(float time, vec3& vCamPos, vec3& vCamTgt) {
    vCamPos = vec3(1050.0f, 30.0f, -3616.0f);
    vCamTgt = vec3(1050.0f, 30.0f, -3500.0f);
#ifdef DEMO_CAMERA
    float t = time - 5.0f;
    if (t > 0.0f) t = mod(t, 28.0f) + 5.0f;
    vCamTgt = mix(vCamTgt, vec3(1834.0f,30.0f,-3264.0f), smoothstep(5.0f,10.0f,t));
    vCamPos = mix(vCamPos, vec3(1280.0f,30.0f,-3350.0f), smoothstep(8.0f,13.0f,t));
    vCamTgt = mix(vCamTgt, vec3(1280.0f,30.0f,-2976.0f), smoothstep(11.0f,16.0f,t));
    vCamPos = mix(vCamPos, vec3(1280.0f,30.0f,-2976.0f), smoothstep(13.0f,19.0f,t));
    vCamTgt = mix(vCamTgt, vec3(768.0f,30.0f,-3050.0f), smoothstep(16.0f,20.0f,t));
    vCamPos = mix(vCamPos, vec3(832.0f,30.0f,-3020.0f), smoothstep(19.0f,23.0f,t));
    vCamTgt = mix(vCamTgt, vec3(1256.0f,30.0f,-3648.0f), smoothstep(20.0f,25.0f,t));
    vCamPos = mix(vCamPos, vec3(768.0f,30.0f,-3424.0f), smoothstep(23.0f,28.0f,t));
    vCamPos = mix(vCamPos, vec3(1050.0f,30.0f,-3616.0f), smoothstep(25.0f,30.0f,t));
    vCamTgt = mix(vCamTgt, vec3(1050.0f,30.0f,-3500.0f), smoothstep(28.0f,33.0f,t));
#endif
}

// ============================================================
// DrawScene
// ============================================================

AI static vec3 DrawScene(vec3 vFwd, vec2 vUV, const Ray& r, float iTime) {
    float fClosestT;
    vec4 vHitInfo;
    float fNoFog = 0.0f;

    MapIntersect(fClosestT, vHitInfo, r, iTime);

#ifdef ENABLE_SPRITES
    vec2 vSpriteDir = -normalize(vec2(-vFwd.z, vFwd.x));
    BarrelSprite(fClosestT, vHitInfo, vSpriteDir, 1088, 0, -2944, 0.565f, r);
    BarrelSprite(fClosestT, vHitInfo, vSpriteDir,  864, 0, -3328, 0.565f, r);
    BarrelSprite(fClosestT, vHitInfo, vSpriteDir, 1312,-16,-3264, 0.878f, r);
    CorpseSprite(fClosestT, vHitInfo, vSpriteDir, 1024,-16,-3264, 0.878f, r);
#endif

    vHitInfo.z = clamp(vHitInfo.z + kExtraLight, 0.0f, 1.0f);

#ifdef DRAW_SKY
    float fDoSky = step(0.9f, vHitInfo.w) * step(vHitInfo.w, 1.1f);
    fNoFog = max(fNoFog, fDoSky);
    float fSkyU = (atan(vFwd.x, vFwd.z) * 512.0f / radians(180.0f)) + vUV.x * 320.0f;
    float fSkyV = vUV.y * 240.0f;
    vHitInfo = mix(vHitInfo, vec4(fSkyU, fSkyV, 1.0f, 1.0f), fDoSky);
#endif

#ifdef INTRO_EFFECT
    float fEffectOffset = max(iTime - 1.0f, 0.0f) - hash(vUV.x);
    vec2 vEffectUV = vUV;
    vEffectUV.y += clamp(fEffectOffset, 0.0f, 1.0f);
    float fDoEffect = step(vEffectUV.y, 1.0f);
    vHitInfo = mix(vHitInfo, vec4(vEffectUV.x*128.0f, vEffectUV.y*128.0f, 1.0f, 3.0f), fDoEffect);
    fNoFog = max(fNoFog, fDoEffect);
#endif

    float fLightLevel = clamp(vHitInfo.z, 0.0f, 1.0f);
    float fDepth = dot(r.vRayDir, vFwd) * fClosestT;
    float fDepthFade = fDepth * kDepthFadeScale;
    float fApplyFog = 1.0f - fNoFog;
    fLightLevel = clamp(fLightLevel - fDepthFade * fApplyFog, 0.0f, 1.0f);

    vec3 vResult = SampleTexture(vHitInfo.w, vec2(vHitInfo.x, vHitInfo.y)) * fLightLevel;
    vResult = clamp(vResult * 1.2f, 0.0f, 1.0f);

#ifdef QUANTIZE_FINAL_IMAGE
    vResult = Quantize(vResult);
#endif

    return vResult;
}

// ============================================================
// mainImage — entry point
// ============================================================

extern "C"
void mainImage(vec4* fragColor, float fragCoordX, float fragCoordY,
               float iResolutionX, float iResolutionY, float iTime)
{
    vec2 vOrigUV(fragCoordX / iResolutionX, fragCoordY / iResolutionY);

#ifdef PIXELATE_IMAGE
    vec2 vFakeRes(320.0f, 240.0f);
    vec2 vUV = floor(vOrigUV * vFakeRes + 0.5f) / vFakeRes;
#else
    vec2 vUV = vOrigUV;
#endif

    vec3 vCameraPos(0.0f);
    vec3 vCameraTarget(0.0f);

    DemoCamera(iTime, vCameraPos, vCameraTarget);

#ifdef HEAD_BOB
    vec2 vStart(1050.0f, -3616.0f);
    float fBob = sin(length(vec2(vCameraPos.x, vCameraPos.z) - vStart) * 0.04f) * 4.0f;
    vCameraPos.y += fBob;
    vCameraTarget.y += fBob;
#endif

    float aspect = iResolutionX / iResolutionY;
    vec2 vWindowCoord((vUV.x * 2.0f - 1.0f) * aspect, vUV.y * 2.0f - 1.0f);

    Ray r;
    r.vRayOrigin = vCameraPos;
    r.vRayDir = GetCameraRayDir(vWindowCoord, vCameraPos, vCameraTarget);

    vec3 vFwd = normalize(vCameraTarget - vCameraPos);

    vec3 vResult = DrawScene(vFwd, vUV, r, iTime);

    fragColor->x = vResult.x;
    fragColor->y = vResult.y;
    fragColor->z = vResult.z;
    fragColor->w = 1.0f;
}
