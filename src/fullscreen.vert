#version 410 core
layout(location=0) flat out float clearDepth;
layout(location=1) flat out int outlineWidth;
void main() {
    // Bits 0-1 select the triangle corner, bit 2 selects the clear value.
    int corner = gl_VertexID & 3;
    clearDepth = (gl_VertexID & 4) != 0 ? 1.0 : 0.0;
    outlineWidth = 1 + (gl_VertexID >> 3);
    gl_Position = vec4(corner == 1 ? 3.0 : -1.0,
                       corner == 2 ? 3.0 : -1.0, 0.0, 1.0);
}
