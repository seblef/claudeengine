// Per-light constant buffer (UBO slot 4).
// type: 0=global, 1=omni, 2=circle_spot, 3=rect_spot.
layout(std140, binding = 4) uniform LightInfosBlock {
    vec3  position;    float type;
    vec3  color;       float intensity;
    vec3  direction;   float range;
    float inner_angle; float outer_angle; float h_angle; float v_angle;
    vec3  ambient;     float pad_;
};
