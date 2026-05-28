// WindInfosBlock — must match environment::WindInfos exactly (16 bytes, 1 float4).
// std140 offsets:
//   [  0] wind_xz       vec2  (wind direction × speed, XZ plane, m/s)
//   [  8] wind_strength float (scalar speed magnitude, m/s)
//   [ 12] wind_time     float (accumulated simulation time, s)
layout(std140, binding = 7) uniform WindInfosBlock {
    vec2  wind_xz;       // wind direction × speed (m/s)
    float wind_strength; // scalar magnitude (m/s)
    float wind_time;     // accumulated simulation time (s)
};
