#version 460 core

// Depth is written implicitly by the rasterizer from gl_Position.z.
// No color outputs — this shader is used with a depth-only FBO.
void main() {}
