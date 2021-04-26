#define V2 float2
#define V3 float3
#define V4 float4
#define f32 float

//@{

struct shd_Vs_Uniform {
    V2 pos;
    f32 theta;
     f32 scale;
    V2 camera_pos;
    f32 camera_scale;
    f32 r;
    f32 g;
    f32 b;
};

//@

V3 linear_srgb_to_oklab(V3 c) {
    f32 l = 0.4122214708f * c.r + 0.5363325363f * c.g + 0.0514459929f * c.b;
	f32 m = 0.2119034982f * c.r + 0.6806995451f * c.g + 0.1073969566f * c.b;
	f32 s = 0.0883024619f * c.r + 0.2817188376f * c.g + 0.6299787005f * c.b;
    
    f32 l_ = pow(l, 1.0/3);
    f32 m_ = pow(m, 1.0/3);
    f32 s_ = pow(s, 1.0/3);
    
    return V3(0.2104542553f*l_ + 0.7936177850f*m_ - 0.0040720468f*s_,
                  1.9779984951f*l_ - 2.4285922050f*m_ + 0.4505937099f*s_,
                  0.0259040371f*l_ + 0.7827717662f*m_ - 0.8086757660f*s_);
}

V3 oklab_to_linear_srgb(V3 c) {
    f32 l_ = c.r + 0.3963377774f * c.g + 0.2158037573f * c.b;
    f32 m_ = c.r - 0.1055613458f * c.g - 0.0638541728f * c.b;
    f32 s_ = c.r - 0.0894841775f * c.g - 1.2914855480f * c.b;
    
    f32 l = l_*l_*l_;
    f32 m = m_*m_*m_;
    f32 s = s_*s_*s_;
    
    return V3(+4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
                  -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
                  -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s);
}

cbuffer vs_uniform {
    shd_Vs_Uniform shd_vs_uniform;
}
struct vs_in {
    V4 pos: POS;
    V4 color: COLOR;
};
struct vs_out {
    V4 color: COLOR0;
    V4 pos: SV_Position;
};
vs_out vsmain(vs_in inp) {
    vs_out outp;
    outp.pos = inp.pos;
    f32 c = cos(-shd_vs_uniform.theta);
    f32 s = sin(-shd_vs_uniform.theta);
    float2x2 mat = float2x2(c, -s, s, c);
    outp.pos.xy = mul(outp.pos.xy, mat);
    outp.pos.xy *= shd_vs_uniform.scale;
    outp.pos.xy += shd_vs_uniform.pos;
    outp.pos.xy -= shd_vs_uniform.camera_pos;
    outp.pos.xy *= shd_vs_uniform.camera_scale;
    outp.color = inp.color;
    outp.color.r = shd_vs_uniform.r;
    outp.color.g = shd_vs_uniform.g;
    outp.color.b = shd_vs_uniform.b;
    outp.color.rgb = linear_srgb_to_oklab(outp.color.rgb);
    return outp;
}
V4 fsmain(V4 color: COLOR0): SV_Target0 {
    color.rgb = oklab_to_linear_srgb(color.rgb);
    return color;
}
