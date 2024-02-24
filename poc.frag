#version 450

layout(location = 0) out vec4 frag_colour;

layout(location = 0) in vec2 frag_coord;

void main() {
  vec2 p = frag_coord * 0.5 + 0.5;
  frag_colour = vec4(p, 1, 1);
}
