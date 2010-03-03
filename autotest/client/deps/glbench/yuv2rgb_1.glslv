#define I915_WORKAROUND 1

attribute vec4 c;
varying vec4 v1;

void main() {
  gl_Position = c;
#if I915_WORKAROUND
  gl_TexCoord[0] = c;
#else
  v1 = c;
#endif
}
