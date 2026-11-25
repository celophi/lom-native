#version 450

layout(location = 0) in ivec2 pos;
layout(location = 1) in uvec4 tex;
layout(location = 2) in vec4 color;
layout(location = 3) in uvec4 twin;

layout(set = 1, binding = 0) uniform UBO { vec2 drawOffset, halfPixel, targetSize; };

layout(location = 0) out vec4 vertexColor;
layout(location = 1) out vec2 rawUV;
layout(location = 2) flat out uint tpage;
layout(location = 3) flat out uint clut;
layout(location = 4) flat out uint textureMode;
layout(location = 5) flat out uint subPixelMask;
layout(location = 6) flat out uint texelShift;
layout(location = 7) flat out uint indexShift;
layout(location = 8) flat out uint indexMask;
layout(location = 9) flat out uint dither;
layout(location = 10) flat out ivec2 pageBase;
layout(location = 11) flat out uvec4 texWindow;

void main() {
    vec2 shift = (tex.w & 0x2000u) != 0u ? halfPixel : vec2(0.0);
    float x = ((float(pos.x) + drawOffset.x + shift.x) / (targetSize.x / 2.0)) - 1.0;
    float y = ((float(pos.y) + drawOffset.y + shift.y) / (targetSize.y / 2.0)) - 1.0;
    gl_Position = vec4(x, -y, 0.0, 1.0);
    vertexColor = color;
    clut = tex.z;
    uint texWord = tex.w;
    tpage = texWord & 0x1FFu;
    dither = (texWord & 0x4000u) != 0u ? 1u : 0u;
    rawUV = vec2(tex.xy) + shift - halfPixel;
    subPixelMask = 0u;
    texelShift = 0u;
    indexShift = 0u;
    indexMask = 0u;
    if ((texWord & 0x1000u) != 0u) {
        textureMode = (texWord & 0x0800u) != 0u ? 4u : 3u;
        vertexColor.rgb *= 2.0;
        rawUV = vec2(tex.xy);
    } else if ((texWord & 0x8000u) != 0u) {
        textureMode = 0u;
    } else if ((tpage & 0x180u) >= 0x100u) {
        textureMode = 1u;
        vertexColor.rgb *= 2.0;
    } else {
        textureMode = 2u;
        vertexColor.rgb *= 2.0;
        if ((tpage & 0x80u) != 0u) {
            subPixelMask = 1u;
            texelShift = 1u;
            indexShift = 8u;
            indexMask = 0xFFu;
        } else {
            subPixelMask = 3u;
            texelShift = 2u;
            indexShift = 4u;
            indexMask = 0xFu;
        }
    }
    pageBase = ivec2(int((tpage % 32u) % 16u) * 64, int((tpage % 32u) / 16u) * 256);
    texWindow = twin;
}
