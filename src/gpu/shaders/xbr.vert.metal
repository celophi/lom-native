#include <metal_stdlib>
using namespace metal;

struct VertexOutput
{
    float4 position [[position]];
};

vertex VertexOutput main0(uint vertex_id [[vertex_id]])
{
    float2 p = float2((int(vertex_id) << 1) & 2, int(vertex_id) & 2);
    VertexOutput out;
    out.position = float4(p * 2.0 - 1.0, 0.0, 1.0);
    return out;
}
