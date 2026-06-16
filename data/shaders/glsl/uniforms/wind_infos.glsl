// WindInfosBlock — must match environment::WindInfos exactly (32 bytes, 2 float4s).
// std140 offsets:
//   [  0] wind_xz          vec2  (wind direction × speed, XZ plane, m/s)
//   [  8] wind_strength     float (scalar speed magnitude, m/s)
//   [ 12] wind_time         float (accumulated simulation time, s)
//   [ 16] wind_displacement vec2  (accumulated XZ displacement, m)
layout(std140, binding = 7) uniform WindInfosBlock {
    vec2  wind_xz;          // wind direction × speed (m/s)
    float wind_strength;    // scalar magnitude (m/s)
    float wind_time;        // accumulated simulation time (s)
    vec2  wind_displacement; // accumulated XZ displacement (m), for cloud UV scroll
};
