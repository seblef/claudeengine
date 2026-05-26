// VertexTerrain layout: XZ position only (loc 0).
// Y is derived in the vertex shader from the heightmap; UVs are derived from
// XZ and the patch scale uniform.
layout(location = 0) in vec2 in_position;
