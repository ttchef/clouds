
#version 460

layout (location = 0) in vec3 in_pos;

vec3 vertices[3] = vec3[](
    vec3(-0.5, 0.5, 0.0),
    vec3( 0.5, 0.5, 0.0),  
    vec3( 0.0, -0.5, 0.0) 
);

void main() {
    gl_Position = vec4(vertices[gl_VertexIndex], 1.0);
}
