// WaterInfosBlock — must match environment::WaterInfos exactly (48 bytes, 3 float4s).
// std140 offsets:
//   [  0] water_params      vec4  (.rgb = water colour, .a = water level)
//   [ 16] sky_zenith_color  vec4  (.rgb = sky zenith colour, .a = unused)
//   [ 32] sun_direction     vec4  (.xyz = sun dir, .w = unused)
layout(std140, binding = 9) uniform WaterInfosBlock {
    vec4  water_params;       // .rgb = water_color, .a = water_level
    vec4  sky_zenith_color;   // .rgb = sky zenith, .a = unused
    vec4  sun_direction;      // .xyz = sun direction, .w = unused
};
