#version 450

layout(push_constant) uniform upc { float aspect; float time; } pc;
layout(set = 0, binding = 0) uniform sampler2D movie;

layout(location = 0) in vec2 frag_coord;

layout(location = 0) out vec4 frag_colour;

void main() {
  vec2 p = frag_coord;
  vec2 uv = p * 0.5 + 0.5;

  vec3 yuv = texture(movie, uv).rgb;

  frag_colour = vec4(yuv, 1);
}
