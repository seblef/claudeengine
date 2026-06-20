// Fragment shader for 2D UI text glyphs.
// The font atlas is a single-channel R8 greyscale texture; the red channel
// drives alpha so the text color is applied as a tint over transparent quads.

#version 460 core

uniform sampler2D u_atlas;

in vec2 v_uv;
in vec4 v_color;

out vec4 out_color;

void main() {
    // Red channel = coverage; multiply text color alpha by coverage.
    float coverage = texture(u_atlas, v_uv).r;
    out_color = vec4(v_color.rgb, v_color.a * coverage);
}
