// WaterInfosBlock — must match environment::WaterInfos exactly (112 bytes, 7 float4s).
// std140 offsets:
//   [  0] water_params       vec4  (.rgb = water colour, .a = water level)
//   [ 16] sky_zenith_color   vec4  (.rgb = sky zenith, .a = roughness)
//   [ 32] sun_params         vec4  (.xyz = sun direction, .w = sun intensity)
//   [ 48] refraction_params  vec4  (.x = refraction_strength, .y = absorption_scale,
//                                   .z = foam_height_thresh, .w = foam_shoreline_depth)
//   [ 64] foam_params        vec4  (.x = foam_steepness_thresh, .y = foam_speed,
//                                   .z = normal_scale1, .w = normal_scale2)
//   [ 80] scroll_params      vec4  (.x = normal_scroll_speed1, .y = normal_scroll_speed2,
//                                   .z = lod_near_dist, .w = lod_far_dist)
//   [ 96] dir_params         vec4  (.xy = normalised layer-1 scroll dir,
//                                   .zw = normalised layer-2 scroll dir)
layout(std140, binding = 9) uniform WaterInfosBlock {
    vec4  water_params;       // .rgb = water_color, .a = water_level
    vec4  sky_zenith_color;   // .rgb = sky zenith, .a = roughness
    vec4  sun_params;         // .xyz = sun direction, .w = sun_intensity
    vec4  refraction_params;  // .x = refraction_strength, .y = absorption_scale,
                              // .z = foam_height_thresh, .w = foam_shoreline_depth
    vec4  foam_params;        // .x = foam_steepness_thresh, .y = foam_speed,
                              // .z = normal_scale1, .w = normal_scale2
    vec4  scroll_params;      // .x = normal_scroll_speed1, .y = normal_scroll_speed2,
                              // .z = lod_near_dist, .w = lod_far_dist
    vec4  dir_params;         // .xy = layer-1 scroll direction (normalised),
                              // .zw = layer-2 scroll direction (normalised)
};
