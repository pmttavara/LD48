cbuffer pos {
    float2 pos;
}
struct vs_in {
    float4 pos: POS;
    float4 color: COLOR;
};
struct vs_out {
    float4 color: COLOR0;
    float4 pos: SV_Position;
};
vs_out vsmain(vs_in inp) {
    vs_out outp;
    outp.pos = inp.pos;
    outp.pos.xy += pos;
    outp.color = inp.color;
    return outp;
}
float4 fsmain(float4 color: COLOR0): SV_Target0 {
    return color;
}
