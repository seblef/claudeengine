// Per-light constant buffer (UBO slot 4).
// type: 0=global, 1=omni, 2=circle_spot, 3=rect_spot.
// cast_shadow: 1.0 when a shadow map is assigned this frame; 0.0 otherwise.
// light_vp: light-space view-projection matrix (row_major, matches core::Mat4f).
layout(std140, binding = 4) uniform LightInfosBlock {
    vec3  position;    float type;
    vec3  color;       float intensity;
    vec3  direction;   float range;
    float inner_angle; float outer_angle; float h_angle; float v_angle;
    vec3  ambient;     float cast_shadow;
    layout(row_major) mat4 light_vp;
    float shadow_bias;
    float li_pad1_; float li_pad2_; float li_pad3_;
};
