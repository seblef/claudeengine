// Terrain wireframe fragment shader — editor debug overlay.
//
// Outputs a solid white colour for every fragment.
// Used with glPolygonMode(GL_LINE) to render terrain patch edges as
// a flat-colour wireframe overlay, with no G-buffer writes.

#version 460 core

layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(1.0, 1.0, 1.0, 1.0);
}
