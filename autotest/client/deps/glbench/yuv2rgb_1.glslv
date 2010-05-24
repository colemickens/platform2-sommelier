attribute vec4 c;
varying vec4 v1;

void main() {
  gl_Position = c;
#if defined(I915_WORKAROUND)
  gl_TexCoord[0] = c;
#else
  v1 = c;
#endif
}
