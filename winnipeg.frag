#version 450

layout(push_constant) uniform upc {
  float aspect;
  float angle;
  float scale;
  vec2 pos;
} pc;
layout(set = 0, binding = 0) uniform sampler2D smp_movie;
layout(set = 0, binding = 1) uniform sampler2D smp_overlay;

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

vec4 overlay(vec2 p) {
  vec2 uv = p * 0.5 + 0.5;
  return texture(smp_overlay, uv);
}

void main() {
  vec2 p = frag_coord * vec2(pc.aspect, 1);
  p /= pc.scale;

  float t = pc.angle;
  p = mat2(cos(t), -sin(t), sin(t), cos(t)) * p;

  vec3 mov = movie(p);
  vec4 ovl = overlay(p);

  vec3 rgb;
  rgb = mix(mov, ovl.rgb, ovl.a);

  frag_colour = vec4(rgb, 1);
}
