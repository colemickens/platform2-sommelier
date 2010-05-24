uniform float imageHeight;

attribute vec4 c;

varying vec2 lineCounter;
varying vec2 yPlane;
varying vec2 uPlane;
varying vec2 vPlane;

void main() {
  gl_Position = c;
#if defined(I915_WORKAROUND)
  gl_TexCoord[0].xy = vec2(c.y * imageHeight / 4.0, 0.0);
  gl_TexCoord[0].zw = vec2(c.x, (2.0 * c.y + 1.0) / 3.0);
  gl_TexCoord[1].xy = vec2(c.x / 2.0, (c.y + 1.0) / 6.0);
  gl_TexCoord[1].zw = vec2(c.x / 2.0, c.y / 6.0);
#else
  lineCounter = vec2(c.y * imageHeight / 4.0, 0.0);
  yPlane = vec2(c.x, (2.0 * c.y + 1.0) / 3.0);
  uPlane = vec2(c.x / 2.0, (c.y + 1.0) / 6.0);
  vPlane = vec2(c.x / 2.0, c.y / 6.0);
#endif
}

