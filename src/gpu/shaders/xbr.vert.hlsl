float4 main(uint vertex_id : SV_VertexID) : SV_Position
{
    float2 p = float2((int(vertex_id) << 1) & 2, int(vertex_id) & 2);
    return float4(p * 2.0 - 1.0, 0.0, 1.0);
}
