#version 450

layout(location = 0) in vec4 vertexColor;
layout(location = 1) in vec2 rawUV;
layout(location = 2) flat in uint tpage;
layout(location = 3) flat in uint clut;
layout(location = 4) flat in uint textureMode;
layout(location = 5) flat in uint subPixelMask;
layout(location = 6) flat in uint texelShift;
layout(location = 7) flat in uint indexShift;
layout(location = 8) flat in uint indexMask;
layout(location = 9) flat in uint dither;
layout(location = 10) flat in ivec2 pageBase;
layout(location = 11) flat in uvec4 texWindow;

layout(set = 2, binding = 0) uniform sampler2D texVram;
layout(set = 2, binding = 1) uniform sampler2DArray texHd;
layout(set = 2, binding = 2) uniform sampler2DArray texImage;

uvec2 resolveTexel() {
    vec2 uv = rawUV + 1.0 / 512.0;
    uvec2 texel = uvec2(clamp(floor(uv), vec2(0.0), vec2(255.0)));
    return (texel & texWindow.xy) | texWindow.zw;
}

layout(location = 0) out vec4 FragColor;

uint rgb5551ToU16(vec4 c) {
    uint r = uint(c.r * 31.0 + 0.5);
    uint g = uint(c.g * 31.0 + 0.5);
    uint b = uint(c.b * 31.0 + 0.5);
    uint a = uint(c.a + 0.5);
    return r | (g << 5u) | (b << 10u) | (a << 15u);
}

const mat4 ditherMatrix = mat4(
    -4.0, +0.0, -3.0, +1.0,
    +2.0, -2.0, +3.0, -1.0,
    -3.0, +1.0, -4.0, +0.0,
    +3.0, -1.0, +2.0, -2.0);
vec3 applyDither(vec3 c) {
    if (dither == 0u) return c;
    int dx = int(gl_FragCoord.x) & 3;
    int dy = int(gl_FragCoord.y) & 3;
    float off = ditherMatrix[dy][dx];
    vec3 c8 = c * 255.0 + off;
    vec3 c5 = clamp(floor(c8 / 8.0), 0.0, 31.0);
    return c5 / 31.0;
}

const float HD_CELL = 64.0;
const float HD_ART = 72.0;
const float HD_SLOT = 76.0;
const float HD_BORDER = 2.0;
const float HD_PAGE = 1024.0;

vec4 hdSample(vec2 texel, uint layer) {
    return textureLod(texHd, vec3(texel / HD_PAGE, float(layer)), 0.0).bgra;
}

void hdOutput(vec4 hd);

void drawHd() {
    uint layer = clut >> 8u;
    uint cell = clut & 0xFFu;
    vec2 origin = vec2(float(cell & 15u), float(cell >> 4u)) * HD_SLOT + HD_BORDER;
    vec2 art = rawUV * (HD_ART / HD_CELL);
    vec2 dx = dFdx(art);
    vec2 dy = dFdy(art);
    vec4 hd;
    if (max(length(dx), length(dy)) > 1.5) {
        vec2 a = (dx + dy) * 0.25;
        vec2 b = (dx - dy) * 0.25;
        hd = (hdSample(origin + clamp(art + a, vec2(0.5), vec2(HD_ART - 0.5)), layer) +
              hdSample(origin + clamp(art - a, vec2(0.5), vec2(HD_ART - 0.5)), layer) +
              hdSample(origin + clamp(art + b, vec2(0.5), vec2(HD_ART - 0.5)), layer) +
              hdSample(origin + clamp(art - b, vec2(0.5), vec2(HD_ART - 0.5)), layer)) * 0.25;
    } else {
        hd = hdSample(origin + clamp(art, vec2(0.5), vec2(HD_ART - 0.5)), layer);
    }
    hdOutput(hd);
}

void drawImage(vec2 gradX, vec2 gradY) {
    vec2 size = vec2(textureSize(texImage, 0).xy);
    vec4 hd = textureGrad(texImage, vec3(rawUV / size, float(clut)), gradX / size, gradY / size).bgra;
    hdOutput(hd);
}

void hdOutput(vec4 hd) {
    if (hd.a <= 0.0) {
        FragColor = vec4(0);
        return;
    }
    vec3 modColor = min(hd.rgb * vertexColor.rgb, vec3(hd.a));
    if (vertexColor.a < 0.75) {
        uint abr = (tpage & 0x60u) >> 5u;
        if (abr == 0u) {
            FragColor = vec4(modColor * 0.5, 0.5 * hd.a);
        } else if (abr == 3u) {
            FragColor = vec4(modColor * 0.25, 0.0);
        } else {
            FragColor = vec4(modColor, 0.0);
        }
    } else {
        FragColor = vec4(modColor, hd.a);
    }
}

void main() {
    vec2 gradX = dFdx(rawUV);
    vec2 gradY = dFdy(rawUV);
    if (textureMode == 3u) {
        drawHd();
        return;
    }
    if (textureMode == 4u) {
        drawImage(gradX, gradY);
        return;
    }
    vec4 texColor;
    if (textureMode == 0u) {
        texColor = vec4(1, 1, 1, 2);
    } else if (textureMode == 1u) {
        texColor = texelFetch(texVram, pageBase + ivec2(resolveTexel()), 0);
    } else {
        ivec2 texel = ivec2(resolveTexel());
        uint subPixel = uint(texel.x) & subPixelMask;
        ivec2 texelPos = ivec2(texel.x >> texelShift, texel.y);
        uint word16 = rgb5551ToU16(texelFetch(texVram, pageBase + texelPos, 0));
        uint colorIdx = (word16 >> (subPixel * indexShift)) & indexMask;
        ivec2 clutBase = ivec2((clut % 64u) * 16u, clut / 64u);
        texColor = texelFetch(texVram, clutBase + ivec2(colorIdx, 0), 0);
    }
    if (texColor == vec4(0, 0, 0, 0)) {
        FragColor = vec4(0);
        return;
    }
    bool isSemiTrans = vertexColor.a < 0.75;
    bool colorSemiTrans = texColor.a > 0;
    vec3 modColor;
    if (textureMode == 0u) {
        vec3 col8 = floor(vertexColor.rgb * 255.0 + 0.5);
        modColor = dither != 0u ? col8 / 255.0 : floor(col8 / 8.0) / 31.0;
    } else {
        vec3 tex5 = floor(texColor.rgb * 31.0 + 0.5);
        vec3 col8 = min(floor(vertexColor.rgb * 127.5 + 0.5), vec3(255.0));
        vec3 prod8 = min(tex5 * col8 / 16.0, vec3(255.0));
        modColor = dither != 0u ? prod8 / 255.0
                                : floor(prod8 / 8.0) / 31.0;
    }
    modColor = applyDither(modColor);
    if (colorSemiTrans && isSemiTrans) {
        uint abr = (tpage & 0x60u) >> 5u;
        if (abr == 0u) {
            FragColor = vec4(modColor * 0.5, 0.5);
        } else if (abr == 1u) {
            FragColor = vec4(modColor, 0.0);
        } else if (abr == 2u) {
            FragColor = vec4(modColor, 0.0);
        } else {
            FragColor = vec4(modColor * 0.25, 0.0);
        }
    } else {
        FragColor = vec4(modColor, 1.0);
    }
}
