// SkyInfosBlock — must match environment::SkyInfos exactly (32 bytes, 2 float4s).
// std140 offsets:
//   [  0] sun_direction     vec3  (std140 vec3 occupies a full 16-byte slot)
//   [ 12] si_pad0_          float
//   [ 16] time_of_day       float (hours, 0–24)
//   [ 20] turbidity         float (atmosphere haziness: 1.7 = clear, 10 = hazy)
//   [ 24] has_moon_texture  float (1.0 if moon texture is bound, 0.0 otherwise)
//   [ 28] si_pad2_          float
layout(std140, binding = 8) uniform SkyInfosBlock {
    vec3  sun_direction;
    float si_pad0_;
    float time_of_day;        // 0–24
    float turbidity;          // 1.7 = very clear, 2.0 = default, 10 = very hazy
    float has_moon_texture;   // 1.0 = sample moon_tex, 0.0 = plain disc fallback
    float si_pad2_;
};
