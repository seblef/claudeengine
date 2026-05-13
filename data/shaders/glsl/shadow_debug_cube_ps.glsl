// Fragment shader for cube shadow-map debug thumbnails.
// Samples a cube depth texture with compare mode disabled (bound as samplerCube)
// and displays the raw depth of a chosen face as greyscale.
//
// u_face: cube face index matching GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_idx:
//   0 = +X,  1 = -X,  2 = +Y,  3 = -Y,  4 = +Z,  5 = -Z
//
// UV [0,1]×[0,1] is mapped to a 3-D direction that lies within the requested
// face using the standard OpenGL cube-map s/t conventions.

#version 460 core
layout(binding = 0) uniform samplerCube u_depth_cube;
uniform int u_face;

in  vec2 v_uv;
out vec4 out_color;

vec3 face_dir(vec2 uv) {
    float s = uv.x * 2.0 - 1.0;
    float t = uv.y * 2.0 - 1.0;
    switch (u_face) {
        case 0:  return vec3( 1.0,  -t, -s);  // +X: s→-Z, t→-Y
        case 1:  return vec3(-1.0,  -t,  s);  // -X: s→+Z, t→-Y
        case 2:  return vec3(   s, 1.0,  t);  // +Y: s→+X, t→+Z
        case 3:  return vec3(   s,-1.0, -t);  // -Y: s→+X, t→-Z
        case 4:  return vec3(   s,  -t, 1.0); // +Z: s→+X, t→-Y
        default: return vec3(  -s,  -t,-1.0); // -Z: s→-X, t→-Y
    }
}

void main() {
    float d = texture(u_depth_cube, face_dir(v_uv)).r;
    out_color = vec4(d, d, d, 1.0);
}
