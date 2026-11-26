#version 410 core
layout(location=0) flat out float clearDepth;
void main() {
    // firstVertex encodes the camera's forward/reverse depth clear value.
    int corner = gl_VertexID;
    clearDepth = corner >= 3 ? 1.0 : 0.0;
    if (corner >= 3) corner -= 3;
    gl_Position = vec4(corner == 1 ? 3.0 : -1.0,
                       corner == 2 ? 3.0 : -1.0, 0.0, 1.0);
}
