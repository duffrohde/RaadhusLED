#define OFFSET 0
#define SCALE 1
#define ANTIALIAS 3

uniform float iGlobalTime;

vec3 effect(vec2 coord) {
  float t = iGlobalTime;
  vec2 c = coord - vec2(15,29);
  float a = atan(c.x,c.y);
  float a1 = (a / (3.14159265358979*2.0))+0.5;
  float r = length(c);
  float r0 = 5.0 + 3.0 * sin(3.0*a + t*1.3) + 3.0 * sin(3.0*a + t*3.7);
  float i = clamp(1.0 - (r-r0)*(r-r0)*0.02, 0.0, 1.0);
  return i*(vec3(sin(a*2.0+t),sin(a*3.0+t),sin(a*7.0+t))*0.5+0.5);
}

void main(void)
{
  ivec2 pixel = ivec2(gl_FragCoord.xy / float(SCALE)) - ivec2(OFFSET);
  if (pixel.x < 0 || pixel.x >= 54 || pixel.y < 0 || pixel.y >= 58) {
    gl_FragColor = vec4(0,0,0,0);
    return;
  }

  vec3 accum = vec3(0);
  for (int yi = 0 ; yi < ANTIALIAS ; yi++) {
    for (int xi = 0 ; xi < ANTIALIAS ; xi++) {
      vec2 coord = vec2(pixel) + vec2(0.5 / float(ANTIALIAS)) + vec2(xi,yi) / float(ANTIALIAS);
      accum += effect(coord);
    }
  }

  gl_FragColor = vec4(accum * (1.0 / float(ANTIALIAS * ANTIALIAS)), 1.0);
}
