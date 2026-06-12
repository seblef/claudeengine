// MaterialInfosBlock — must match renderer::MaterialInfos exactly (64 bytes, 4 float4s).
// std140 offsets (Color is alignas(16), 4 floats r,g,b,a — maps to vec4):
//   [  0] diffuse_color   vec4  (16 B — RGBA diffuse tint)
//   [ 16] emissive_color  vec4  (16 B — RGBA emissive scale)
//   [ 32] ambient_color   vec4  (16 B — RGBA ambient term)
//   [ 48] shininess       float ( 4 B — Blinn-Phong exponent)
//   [ 52] specular        float ( 4 B — specular intensity multiplier)
layout(std140, binding = 3) uniform MaterialInfosBlock {
    vec4  diffuse_color;
    vec4  emissive_color;
    vec4  ambient_color;
    float shininess;
    float specular;
};
