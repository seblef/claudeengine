// UIProjectionInfosBlock — orthographic matrix mapping screen pixels to NDC.
// Slot 4 is reserved exclusively for this block.
// Pixel (0,0) = top-left → NDC (-1,+1); pixel (W,H) = bottom-right → NDC (+1,-1).
layout(std140, row_major, binding = 4) uniform UIProjectionInfosBlock {
    mat4 u_ortho;
};
