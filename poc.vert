#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 i_pos;

layout(location = 0) out vec2 frag_coord;

void main() {
  vec2 p = position + i_pos;
  gl_Position = vec4(p, 0, 1);
  frag_coord = p;
}
