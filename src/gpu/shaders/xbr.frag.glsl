#version 450
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
layout(set = 2, binding = 0) uniform sampler2D inputImage;
layout(set = 3, binding = 0) uniform Parameters { vec4 sizes; };
layout(location = 0) out vec4 FragColor;
#define CORNER_C
#define SMOOTH_TIPS
#define XBR_Y_WEIGHT 48.0
#define XBR_EQ_THRESHOLD 15.0
#define lv2_cf 2.0
#define small_details 0.0
#define mul(a,b) (b*a)
const vec3 rgbw = vec3(14.352, 28.176, 5.472);
const  vec4 Ao = vec4( 1.0, -1.0, -1.0, 1.0 );
const  vec4 Bo = vec4( 1.0,  1.0, -1.0,-1.0 );
const  vec4 Co = vec4( 1.5,  0.5, -0.5, 0.5 );
const  vec4 Ax = vec4( 1.0, -1.0, -1.0, 1.0 );
const  vec4 Bx = vec4( 0.5,  2.0, -0.5,-2.0 );
const  vec4 Cx = vec4( 1.0,  1.0, -0.5, 0.0 );
const  vec4 Ay = vec4( 1.0, -1.0, -1.0, 1.0 );
const  vec4 By = vec4( 2.0,  0.5, -2.0,-0.5 );
const  vec4 Cy = vec4( 2.0,  0.0, -1.0, 0.5 );
const  vec4 Ci = vec4(0.25, 0.25, 0.25, 0.25);

const vec3 Y = vec3(0.2126, 0.7152, 0.0722);

vec4 df(vec4 A, vec4 B)
{
    return vec4(abs(A-B));
}

vec4 diff(vec4 A, vec4 B)
{
    return vec4(notEqual(A, B));
}

vec4 eq(vec4 A, vec4 B)
{
    return (step(df(A, B), vec4(XBR_EQ_THRESHOLD)));
}

vec4 neq(vec4 A, vec4 B)
{
    return (vec4(1.0, 1.0, 1.0, 1.0) - eq(A, B));
}

vec4 wd(vec4 a, vec4 b, vec4 c, vec4 d, vec4 e, vec4 f, vec4 g, vec4 h)
{
    return (df(a,b) + df(a,c) + df(d,e) + df(d,f) + 4.0*df(g,h));
}

vec4 weighted_distance(vec4 a, vec4 b, vec4 c, vec4 d, vec4 e, vec4 f, vec4 g, vec4 h, vec4 i, vec4 j, vec4 k, vec4 l)
{
	return (df(a,b) + df(a,c) + df(d,e) + df(d,f) + df(i,j) + df(k,l) + 2.0*df(g,h));
}

float c_df(vec3 c1, vec3 c2) 
{
      vec3 df = abs(c1 - c2);
      return df.r + df.g + df.b;
}


vec3 samplePixel(ivec2 pixel, ivec2 offset)
{
    return texelFetch(inputImage, clamp(pixel + offset, ivec2(0),
        ivec2(sizes.xy) - ivec2(1)), 0).rgb;
}
void main()
{
    vec2 pos = gl_FragCoord.xy * sizes.xy / sizes.zw;
    ivec2 pixel = ivec2(floor(pos));
    vec2 fp = fract(pos);
    float scale = max(1.0, min(sizes.z / sizes.x, sizes.w / sizes.y));
    vec4 delta = vec4(1.0 / scale);
    vec4 delta_l = vec4(0.5, 1.0, 0.5, 1.0) / scale;
    vec4 delta_u = delta_l.yxwz;
    vec4 edri, edr, edr_l, edr_u, px;
    vec4 irlv0, irlv1, irlv2l, irlv2u;
    vec4 fx, fx_l, fx_u;
    vec3 A1 = samplePixel(pixel, ivec2(-1, -2));
    vec3 B1 = samplePixel(pixel, ivec2(0, -2));
    vec3 C1 = samplePixel(pixel, ivec2(1, -2));
    vec3 A = samplePixel(pixel, ivec2(-1, -1));
    vec3 B = samplePixel(pixel, ivec2(0, -1));
    vec3 C = samplePixel(pixel, ivec2(1, -1));
    vec3 D = samplePixel(pixel, ivec2(-1, 0));
    vec3 E = samplePixel(pixel, ivec2(0, 0));
    vec3 F = samplePixel(pixel, ivec2(1, 0));
    vec3 G = samplePixel(pixel, ivec2(-1, 1));
    vec3 H = samplePixel(pixel, ivec2(0, 1));
    vec3 I = samplePixel(pixel, ivec2(1, 1));
    vec3 G5 = samplePixel(pixel, ivec2(-1, 2));
    vec3 H5 = samplePixel(pixel, ivec2(0, 2));
    vec3 I5 = samplePixel(pixel, ivec2(1, 2));
    vec3 A0 = samplePixel(pixel, ivec2(-2, -1));
    vec3 D0 = samplePixel(pixel, ivec2(-2, 0));
    vec3 G0 = samplePixel(pixel, ivec2(-2, 1));
    vec3 C4 = samplePixel(pixel, ivec2(2, -1));
    vec3 F4 = samplePixel(pixel, ivec2(2, 0));
    vec3 I4 = samplePixel(pixel, ivec2(2, 1));

    vec4 b  = vec4(dot(B ,rgbw), dot(D ,rgbw), dot(H ,rgbw), dot(F ,rgbw));
    vec4 c  = vec4(dot(C ,rgbw), dot(A ,rgbw), dot(G ,rgbw), dot(I ,rgbw));
    vec4 d  = b.yzwx;
    vec4 e  = vec4(dot(E,rgbw));
    vec4 f  = b.wxyz;
    vec4 g  = c.zwxy;
    vec4 h  = b.zwxy;
    vec4 i  = c.wxyz;
    
	vec4 i4, i5, h5, f4;
	
	float y_weight = XBR_Y_WEIGHT;
	
	if (small_details < 0.5)
	{
		i4 = vec4(dot(I4,rgbw), dot(C1,rgbw), dot(A0,rgbw), dot(G5,rgbw));
		i5 = vec4(dot(I5,rgbw), dot(C4,rgbw), dot(A1,rgbw), dot(G0,rgbw));
		h5 = vec4(dot(H5,rgbw), dot(F4,rgbw), dot(B1,rgbw), dot(D0,rgbw));
	}
	else
	{
		i4 = mul( mat4x3(I4, C1, A0, G5), y_weight * Y );
		i5 = mul( mat4x3(I5, C4, A1, G0), y_weight * Y );
		h5 = mul( mat4x3(H5, F4, B1, D0), y_weight * Y );
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
    vec4 c1 = i4.yzwx;
    vec4 g0 = i5.wxyz;
    irlv1     = (irlv0  *  ( neq(f,b) * neq(h,d) + eq(e,i) * neq(f,i4) * neq(h,i5) + eq(e,g) + eq(e,c) ) * (diff(f,f4) * diff(f,i) + diff(h,h5) * diff(h,i) + diff(h,g) + diff(f,c) + eq(b,c1) * eq(d,g0)));
#endif
#ifdef CORNER_C
    irlv1     = (irlv0  * ( neq(f,b) * neq(f,c) + neq(h,d) * neq(h,g) + eq(e,i) * (neq(f,f4) * neq(f,i4) + neq(h,h5) * neq(h,i5)) + eq(e,g) + eq(e,c)) );
#endif

    irlv2l = diff(e,g) * diff(d,g);
    irlv2u = diff(e,c) * diff(b,c);

    vec4 fx45i = clamp((fx   + delta   -Co - Ci)/(2.0*delta  ), 0.0, 1.0);
    vec4 fx45  = clamp((fx   + delta   -Co     )/(2.0*delta  ), 0.0, 1.0);
    vec4 fx30  = clamp((fx_l + delta_l -Cx     )/(2.0*delta_l), 0.0, 1.0);
    vec4 fx60  = clamp((fx_u + delta_u -Cy     )/(2.0*delta_u), 0.0, 1.0);

    vec4 wd1, wd2;
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
    edr   = step(wd1 + vec4(0.1, 0.1, 0.1, 0.1), wd2) * step(vec4(0.5, 0.5, 0.5, 0.5), irlv1);
    edr_l = step( lv2_cf*df(f,g), df(h,c) ) * irlv2l * edr;
    edr_u = step( lv2_cf*df(h,c), df(f,g) ) * irlv2u * edr;

    fx45  = edr   * fx45;
    fx30  = edr_l * fx30;
    fx60  = edr_u * fx60;
    fx45i = edri  * fx45i;

    px = step(df(e,f), df(e,h));

#ifdef SMOOTH_TIPS
    vec4 maximos = max(max(fx30, fx60), max(fx45, fx45i));
#endif
#ifndef SMOOTH_TIPS
    vec4 maximos = max(max(fx30, fx60), fx45);
#endif

    vec3 res1 = E;
    res1 = mix(res1, mix(H, F, px.x), maximos.x);
    res1 = mix(res1, mix(B, D, px.z), maximos.z);
    
    vec3 res2 = E;
    res2 = mix(res2, mix(F, B, px.y), maximos.y);
    res2 = mix(res2, mix(D, H, px.w), maximos.w);
    
    vec3 res = mix(res1, res2, step(c_df(E, res1), c_df(E, res2)));


    FragColor = vec4(res, 1.0);
}
