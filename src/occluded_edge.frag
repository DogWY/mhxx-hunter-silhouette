#version 410 core

// All attachments share the camera, depth range and viewport. Convert
// sampled depth to positive camera distance before applying the bias.
#ifdef DEKO3D
#extension GL_ARB_shading_language_420pack : require
layout(binding=0) uniform sampler2D hunterDepth;
layout(binding=1) uniform sampler2D largeMonsterDepth;
layout(binding=2) uniform sampler2D sceneDepth;
layout(location=0) flat in float clearDepth;
#else
uniform sampler2D hunterDepth;
uniform sampler2D largeMonsterDepth;
uniform sampler2D sceneDepth;
uniform float depthTolerance;
#ifdef TEST_NATIVE_DEPTH
layout(location=0) flat in float clearDepth;
#else
uniform float clearDepth;
#endif
// viewDistance = (x * sample + y) / (z * sample + w).
// Use (1,0,0,1) for already-linear distances.
uniform vec4 depthDecode;
#endif
#if defined(DEKO3D) || defined(TEST_NATIVE_DEPTH)
layout(location=1) flat in int outlineWidth;
#else
uniform int outlineWidth;
#endif
layout(location = 0) out vec4 color;

#ifndef DEKO3D
float viewDepth(float sampleDepth) {
    vec2 samplePair = vec2(sampleDepth, 1.0);
    return dot(depthDecode.xy, samplePair) / dot(depthDecode.zw, samplePair);
}

#endif

bool covered(ivec2 pixel, ivec2 size) {
    // Clamp at the viewport boundary so cropping doesn't create a new edge.
    return texelFetch(hunterDepth, clamp(pixel, ivec2(0), size - 1), 0).r != clearDepth;
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    float hunter = texelFetch(hunterDepth, pixel, 0).r;
    if (hunter == clearDepth) discard;
    float monster = texelFetch(largeMonsterDepth, pixel, 0).r;
    float scene = texelFetch(sceneDepth, pixel, 0).r;
    if (monster == clearDepth || scene == clearDepth) discard;
#if defined(DEKO3D) || defined(TEST_NATIVE_DEPTH)
    // Native D24S8 replay uses identical projection and depth quantization.
    float epsilon = 2.0 / 16777215.0;
    if (abs(scene - monster) > epsilon ||
        abs(hunter - clearDepth) + epsilon >= abs(scene - clearDepth)) discard;
#else
    hunter = viewDepth(hunter);
    monster = viewDepth(monster);
    scene = viewDepth(scene);
    if (hunter <= scene + depthTolerance ||
        abs(scene - monster) > depthTolerance)
        discard;
#endif

    ivec2 size = textureSize(hunterDepth, 0);
    int r = outlineWidth;
    int d = int(floor(float(r) * 0.70710678 + 0.5));
    if (covered(pixel + ivec2(r, 0), size) && covered(pixel - ivec2(r, 0), size) &&
        covered(pixel + ivec2(0, r), size) && covered(pixel - ivec2(0, r), size) &&
        covered(pixel + ivec2(d, d), size) && covered(pixel - ivec2(d, d), size) &&
        covered(pixel + ivec2(d, -d), size) && covered(pixel - ivec2(d, -d), size))
        discard;
    color = vec4(0.35, 1.0, 1.0, 1.0);
}
