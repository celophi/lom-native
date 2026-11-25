/*
   Hyllian's xBR-lv2 Shader
   
   Copyright (C) 2011-2016 Hyllian - sergiogdb@gmail.com

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.

   Incorporates some of the ideas from SABR shader. Thanks to Joshua Street.
*/
#define CORNER_C
#define SMOOTH_TIPS
#define XBR_Y_WEIGHT 48.0
#define XBR_EQ_THRESHOLD 15.0
#define lv2_cf 2.0
#define small_details 0.0
static const float3 rgbw = float3(14.352, 28.176, 5.472);
static const float4 Ao = float4( 1.0, -1.0, -1.0, 1.0 );
static const float4 Bo = float4( 1.0,  1.0, -1.0,-1.0 );
static const float4 Co = float4( 1.5,  0.5, -0.5, 0.5 );
static const float4 Ax = float4( 1.0, -1.0, -1.0, 1.0 );
static const float4 Bx = float4( 0.5,  2.0, -0.5,-2.0 );
static const float4 Cx = float4( 1.0,  1.0, -0.5, 0.0 );
static const float4 Ay = float4( 1.0, -1.0, -1.0, 1.0 );
static const float4 By = float4( 2.0,  0.5, -2.0,-0.5 );
static const float4 Cy = float4( 2.0,  0.0, -1.0, 0.5 );
static const float4 Ci = float4(0.25, 0.25, 0.25, 0.25);

static const float3 Y = float3(0.2126, 0.7152, 0.0722);

Texture2D<float4> inputImage : register(t0, space2);
SamplerState inputSampler : register(s0, space2);

cbuffer Parameters : register(b0, space3)
{
    float4 sizes;
};

float4 df(float4 A, float4 B)
{
    return float4(abs(A-B));
}

float4 diff(float4 A, float4 B)
{
    return float4(A != B);
}

float4 eq(float4 A, float4 B)
{
    return (step(df(A, B), (float4)XBR_EQ_THRESHOLD));
}

float4 neq(float4 A, float4 B)
{
    return (float4(1.0, 1.0, 1.0, 1.0) - eq(A, B));
}

float4 wd(float4 a, float4 b, float4 c, float4 d, float4 e, float4 f, float4 g, float4 h)
{
    return (df(a,b) + df(a,c) + df(d,e) + df(d,f) + 4.0*df(g,h));
}

float4 weighted_distance(float4 a, float4 b, float4 c, float4 d, float4 e, float4 f, float4 g, float4 h, float4 i, float4 j, float4 k, float4 l)
{
    return (df(a,b) + df(a,c) + df(d,e) + df(d,f) + df(i,j) + df(k,l) + 2.0*df(g,h));
}

float c_df(float3 c1, float3 c2)
{
    float3 df = abs(c1 - c2);
    return df.r + df.g + df.b;
}

float3 samplePixel(int2 pixel, int2 offset)
{
    int2 at = clamp(pixel + offset, int2(0, 0), int2(sizes.xy) - int2(1, 1));
    return inputImage.Load(int3(at, 0)).rgb;
}

float4 main(float4 position : SV_Position) : SV_Target0
{
    float2 pos = position.xy * sizes.xy / sizes.zw;
    int2 pixel = int2(floor(pos));
    float2 fp = frac(pos);
    float scale = max(1.0, min(sizes.z / sizes.x, sizes.w / sizes.y));
    float4 delta = (float4)(1.0 / scale);
    float4 delta_l = float4(0.5, 1.0, 0.5, 1.0) / scale;
    float4 delta_u = delta_l.yxwz;
    float4 edri, edr, edr_l, edr_u, px;
    float4 irlv0, irlv1, irlv2l, irlv2u;
    float4 fx, fx_l, fx_u;
    float3 A1 = samplePixel(pixel, int2(-1, -2));
    float3 B1 = samplePixel(pixel, int2(0, -2));
    float3 C1 = samplePixel(pixel, int2(1, -2));
    float3 A = samplePixel(pixel, int2(-1, -1));
    float3 B = samplePixel(pixel, int2(0, -1));
    float3 C = samplePixel(pixel, int2(1, -1));
    float3 D = samplePixel(pixel, int2(-1, 0));
    float3 E = samplePixel(pixel, int2(0, 0));
    float3 F = samplePixel(pixel, int2(1, 0));
    float3 G = samplePixel(pixel, int2(-1, 1));
    float3 H = samplePixel(pixel, int2(0, 1));
    float3 I = samplePixel(pixel, int2(1, 1));
    float3 G5 = samplePixel(pixel, int2(-1, 2));
    float3 H5 = samplePixel(pixel, int2(0, 2));
    float3 I5 = samplePixel(pixel, int2(1, 2));
    float3 A0 = samplePixel(pixel, int2(-2, -1));
    float3 D0 = samplePixel(pixel, int2(-2, 0));
    float3 G0 = samplePixel(pixel, int2(-2, 1));
    float3 C4 = samplePixel(pixel, int2(2, -1));
    float3 F4 = samplePixel(pixel, int2(2, 0));
    float3 I4 = samplePixel(pixel, int2(2, 1));

    float4 b  = float4(dot(B ,rgbw), dot(D ,rgbw), dot(H ,rgbw), dot(F ,rgbw));
    float4 c  = float4(dot(C ,rgbw), dot(A ,rgbw), dot(G ,rgbw), dot(I ,rgbw));
    float4 d  = b.yzwx;
    float4 e  = (float4)dot(E,rgbw);
    float4 f  = b.wxyz;
    float4 g  = c.zwxy;
    float4 h  = b.zwxy;
    float4 i  = c.wxyz;

    float4 i4, i5, h5, f4;

    float y_weight = XBR_Y_WEIGHT;

    if (small_details < 0.5)
    {
        i4 = float4(dot(I4,rgbw), dot(C1,rgbw), dot(A0,rgbw), dot(G5,rgbw));
        i5 = float4(dot(I5,rgbw), dot(C4,rgbw), dot(A1,rgbw), dot(G0,rgbw));
        h5 = float4(dot(H5,rgbw), dot(F4,rgbw), dot(B1,rgbw), dot(D0,rgbw));
    }
    else
    {
        i4 = mul( float4x3(I4, C1, A0, G5), y_weight * Y );
        i5 = mul( float4x3(I5, C4, A1, G0), y_weight * Y );
        h5 = mul( float4x3(H5, F4, B1, D0), y_weight * Y );
    }
    f4 = h5.yzwx;

    fx   = (Ao*fp.y+Bo*fp.x);
    fx_l = (Ax*fp.y+Bx*fp.x);
    fx_u = (Ay*fp.y+By*fp.x);

    irlv1 = irlv0 = diff(e,f) * diff(e,h);

#ifdef CORNER_B
    irlv1      = (irlv0 * ( neq(f,b) * neq(h,d) + eq(e,i) * neq(f,i4) * neq(h,i5) + eq(e,g) + eq(e,c) ) );
#endif
#ifdef CORNER_D
    float4 c1 = i4.yzwx;
    float4 g0 = i5.wxyz;
    irlv1     = (irlv0  *  ( neq(f,b) * neq(h,d) + eq(e,i) * neq(f,i4) * neq(h,i5) + eq(e,g) + eq(e,c) ) * (diff(f,f4) * diff(f,i) + diff(h,h5) * diff(h,i) + diff(h,g) + diff(f,c) + eq(b,c1) * eq(d,g0)));
#endif
#ifdef CORNER_C
    irlv1     = (irlv0  * ( neq(f,b) * neq(f,c) + neq(h,d) * neq(h,g) + eq(e,i) * (neq(f,f4) * neq(f,i4) + neq(h,h5) * neq(h,i5)) + eq(e,g) + eq(e,c)) );
#endif

    irlv2l = diff(e,g) * diff(d,g);
    irlv2u = diff(e,c) * diff(b,c);

    float4 fx45i = saturate((fx   + delta   -Co - Ci)/(2.0*delta  ));
    float4 fx45  = saturate((fx   + delta   -Co     )/(2.0*delta  ));
    float4 fx30  = saturate((fx_l + delta_l -Cx     )/(2.0*delta_l));
    float4 fx60  = saturate((fx_u + delta_u -Cy     )/(2.0*delta_u));

    float4 wd1, wd2;
    if (small_details < 0.5)
    {
        wd1 = wd( e, c,  g, i, h5, f4, h, f);
        wd2 = wd( h, d, i5, f, i4,  b, e, i);
    }
    else
    {
        wd1 = weighted_distance( e, c, g, i, f4, h5, h, f, b, d, i4, i5);
        wd2 = weighted_distance( h, d, i5, f, b, i4, e, i, g, h5, c, f4);
    }

    edri  = step(wd1, wd2) * irlv0;
    edr   = step(wd1 + float4(0.1, 0.1, 0.1, 0.1), wd2) * step(float4(0.5, 0.5, 0.5, 0.5), irlv1);
    edr_l = step( lv2_cf*df(f,g), df(h,c) ) * irlv2l * edr;
    edr_u = step( lv2_cf*df(h,c), df(f,g) ) * irlv2u * edr;

    fx45  = edr   * fx45;
    fx30  = edr_l * fx30;
    fx60  = edr_u * fx60;
    fx45i = edri  * fx45i;

    px = step(df(e,f), df(e,h));

#ifdef SMOOTH_TIPS
    float4 maximos = max(max(fx30, fx60), max(fx45, fx45i));
#endif
#ifndef SMOOTH_TIPS
    float4 maximos = max(max(fx30, fx60), fx45);
#endif

    float3 res1 = E;
    res1 = lerp(res1, lerp(H, F, (float3)px.x), (float3)maximos.x);
    res1 = lerp(res1, lerp(B, D, (float3)px.z), (float3)maximos.z);

    float3 res2 = E;
    res2 = lerp(res2, lerp(F, B, (float3)px.y), (float3)maximos.y);
    res2 = lerp(res2, lerp(D, H, (float3)px.w), (float3)maximos.w);

    float3 res = lerp(res1, res2, (float3)step(c_df(E, res1), c_df(E, res2)));

    return float4(res, 1.0);
}
