// MaterialInfosBlock — must match renderer::MaterialInfos exactly (64 bytes, 4 float4s).
// std140 offsets:
//   [  0] diffuse_color   vec3  (16-byte slot; 4th component = Vec3f.w_=0, unused)
//   [ 16] emissive_color  vec3  (16-byte slot)
//   [ 32] ambient_color   vec3  (16-byte slot)
//   [ 48] shininess       float
layout(std140, binding = 3) uniform MaterialInfosBlock {
    vec3  diffuse_color;
    vec3  emissive_color;
    vec3  ambient_color;
    float shininess;
};
