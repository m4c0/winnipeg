#version 450

layout(push_constant) uniform upc { float aspect; float time; } pc;
layout(set = 0, binding = 0) uniform sampler2D smp_movie;

layout(location = 0) in vec2 frag_coord;

layout(location = 0) out vec4 frag_colour;

vec3 movie(vec2 p) {
  vec2 uv = p;
  uv = uv / vec2(pc.aspect, 1);

  vec2 pmsk = step(abs(uv), vec2(1.0));
  float mmsk = pmsk.x * pmsk.y;

  uv = uv * 0.5 + 0.5;

  vec3 m_rgb = texture(smp_movie, uv).rgb;

  vec3 rgb = mix(vec3(0), m_rgb, mmsk);
  rgb = pow(rgb, vec3(2.2));
  return rgb;
}

void main() {
  vec2 p = frag_coord * vec2(pc.aspect, 1);

  float t = 3.14159265358979323;
  p = mat2(cos(t), -sin(t), sin(t), cos(t)) * p;

  vec3 rgb = movie(p);

  frag_colour = vec4(rgb, 1);
}
