#version 450
layout(location=0) in vec3 position;
layout(push_constant) uniform Shadow { mat4 mvp; } shadow;
void main() { gl_Position = shadow.mvp * vec4(position,1.0); }
