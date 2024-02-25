#version 450

layout(set = 0, binding = 0) uniform sampler2D smp_movie;

layout(location = 0) out vec4 frag_colour;

layout(location = 0) in vec2 frag_coord;

void main() {
  vec2 p = frag_coord * 0.5 + 0.5;
  vec4 c = texture(smp_movie, p);
  c = pow(c, vec4(2.2));
  frag_colour = c;
}
