#version 450

layout(push_constant) uniform upc { float aspect; float time; } pc;
layout(set = 0, binding = 0) uniform sampler2D movie;

layout(location = 0) in vec2 frag_coord;

layout(location = 0) out vec4 frag_colour;

const mat3 yuv2rgb = mat3(
  vec3(1.0, 1.0, 1.0),
  vec3(0.0, -0.344, 1.770),
  vec3(1.403, -0.714, 0.0)
);
void main() {
  vec2 p = frag_coord;
  vec2 uv = p * 0.5 + 0.5;

  vec3 yuv = texture(movie, uv).rgb;
  yuv.gb -= 0.5;

  vec3 rgb = yuv2rgb * yuv;

  rgb.r = 1.0 * yuv.r + 1.5748 * yuv.b;
  rgb.g = 1.0 * yuv.r - 0.1773 * yuv.g - 0.4681 * yuv.b;
  rgb.b = 1.0 * yuv.r + 1.8556 * yuv.g;

  rgb = clamp(rgb, 0.0, 1.0);

  frag_colour = vec4(rgb, 1);
}
