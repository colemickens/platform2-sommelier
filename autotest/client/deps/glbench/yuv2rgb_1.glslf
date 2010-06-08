/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This is a conversion of a cg shader from Chrome:
 * http://src.chromium.org/viewvc/chrome/trunk/src/o3d/samples/shaders/yuv2rgb.shader
 */

/*
 * This shader takes a Y'UV420p image as a single greyscale plane, and
 * converts it to RGB by sampling the correct parts of the image, and
 * by converting the colorspace to RGB on the fly.
 */

/*
 * These represent the image dimensions of the SOURCE IMAGE (not the
 * Y'UV420p image).  This is the same as the dimensions of the Y'
 * portion of the Y'UV420p image.  They are set from JavaScript.
 */
uniform float imageWidth;
uniform float imageHeight;

/*
 * This is the texture sampler where the greyscale Y'UV420p image is
 * accessed.
 */
uniform sampler2D textureSampler;

#if defined (USE_UNIFORM_MATRIX)
uniform mat4 conversion;
#endif

varying vec4 v1;

/**
 * This fetches an individual Y pixel from the image, given the current
 * texture coordinates (which range from 0 to 1 on the source texture
 * image).  They are mapped to the portion of the image that contains
 * the Y component.
 *
 * @param position This is the position of the main image that we're
 *        trying to render, in parametric coordinates.
 */
float getYPixel(vec2 position) {
  position.y = 1. - (position.y * 2.0 / 3.0 + 1.0 / 3.0);
  return texture2D(textureSampler, position).x;
}

/**
 * This does the crazy work of calculating the planar position (the
 * position in the byte stream of the image) of the U or V pixel, and
 * then converting that back to x and y coordinates, so that we can
 * account for the fact that V is appended to U in the image, and the
 * complications that causes (see below for a diagram).
 *
 * @param position This is the position of the main image that we're
 *        trying to render, in pixels.
 *
 * @param planarOffset This is an offset to add to the planar address
 *        we calculate so that we can find the U image after the V
 *        image.
 */
vec2 mapCommon(vec2 position, float planarOffset) {
  planarOffset += imageWidth * floor(position.y / 2.0) / 2.0 +
                  floor((imageWidth - 1.0 - position.x) / 2.0);
  float x = floor(imageWidth - 1.0 - floor(mod(planarOffset, imageWidth)));
  float y = floor(planarOffset / imageWidth);
  return vec2((x + 0.5) / imageWidth, 1. - (y + 0.5) / (1.5 * imageHeight));
}

/**
 * This is a helper function for mapping pixel locations to a texture
 * coordinate for the U plane.
 *
 * @param position This is the position of the main image that we're
 *        trying to render, in pixels.
 */
vec2 mapU(vec2 position) {
  float planarOffset = (imageWidth * imageHeight) / 4.0;
  return mapCommon(position, planarOffset);
}

/**
 * This is a helper function for mapping pixel locations to a texture
 * coordinate for the V plane.
 *
 * @param position This is the position of the main image that we're
 *        trying to render, in pixels.
 */
vec2 mapV(vec2 position) {
  return mapCommon(position, 0.0);
}

/**
 * Given the texture coordinates, our pixel shader grabs the right
 * value from each channel of the source image, converts it from Y'UV
 * to RGB, and returns the result.
 *
 * Each U and V pixel provides color information for a 2x2 block of Y
 * pixels.  The U and V planes are just appended to the Y image.
 *
 * For images that have a height divisible by 4, things work out nicely.
 * For images that are merely divisible by 2, it's not so nice
 * (and YUV420 doesn't work for image sizes not divisible by 2).
 *
 * Here is a 6x6 image, with the layout of the planes of U and V.
 * Notice that the V plane starts halfway through the last scanline
 * that has U on it.
 *
 * 1  +---+---+---+---+---+---+
 *    | Y | Y | Y | Y | Y | Y |
 *    +---+---+---+---+---+---+
 *    | Y | Y | Y | Y | Y | Y |
 *    +---+---+---+---+---+---+
 *    | Y | Y | Y | Y | Y | Y |
 *    +---+---+---+---+---+---+
 *    | Y | Y | Y | Y | Y | Y |
 *    +---+---+---+---+---+---+
 *    | Y | Y | Y | Y | Y | Y |
 *    +---+---+---+---+---+---+
 *    | Y | Y | Y | Y | Y | Y |
 * .3 +---+---+---+---+---+---+
 *    | U | U | U | U | U | U |
 *    +---+---+---+---+---+---+
 *    | U | U | U | V | V | V |
 *    +---+---+---+---+---+---+
 *    | V | V | V | V | V | V |
 * 0  +---+---+---+---+---+---+
 *    0                       1
 *
 * Here is a 4x4 image, where the U and V planes are nicely split into
 * separable blocks.
 *
 * 1  +---+---+---+---+
 *    | Y | Y | Y | Y |
 *    +---+---+---+---+
 *    | Y | Y | Y | Y |
 *    +---+---+---+---+
 *    | Y | Y | Y | Y |
 *    +---+---+---+---+
 *    | Y | Y | Y | Y |
 * .3 +---+---+---+---+
 *    | U | U | U | U |
 *    +---+---+---+---+
 *    | V | V | V | V |
 * 0  +---+---+---+---+
 *    0               1
 *
 */
void main() {
  /*
   * Calculate what image pixel we're on, since we have to calculate
   * the location in the image stream, using floor in several places
   * which makes it hard to use parametric coordinates.
   */
#if defined(I915_WORKAROUND)
  vec2 pixelPosition = vec2(floor(imageWidth * gl_TexCoord[0].x),
                            floor(imageHeight * gl_TexCoord[0].y));
#else
  vec2 pixelPosition = vec2(floor(imageWidth * v1.x),
                            floor(imageHeight * v1.y));
#endif

  /*
   * We can use the parametric coordinates to get the Y channel, since it's
   * a relatively normal image.
   */
#if defined(I915_WORKAROUND)
  float yChannel = getYPixel(vec2(gl_TexCoord[0]));
#else
  float yChannel = getYPixel(vec2(v1));
#endif

  /*
   * As noted above, the U and V planes are smashed onto the end of
   * the image in an odd way (in our 2D texture mapping, at least), so
   * these mapping functions take care of that oddness.
   */
  float uChannel = texture2D(textureSampler, mapU(pixelPosition)).x;
  float vChannel = texture2D(textureSampler, mapV(pixelPosition)).x;

  /*
   * This does the colorspace conversion from Y'UV to RGB as a matrix
   * multiply.  It also does the offset of the U and V channels from
   * [0,1] to [-.5,.5] as part of the transform.
   */
  vec4 channels = vec4(yChannel, uChannel, vChannel, 1.0);
#if !defined(USE_UNIFORM_MATRIX)
  mat4 conversion = mat4( 1.0,    1.0,    1.0,   0.0,
                          0.0,   -0.344,  1.772, 0.0,
                          1.402, -0.714,  0.0,   0.0,
                         -0.701,  0.529, -0.886, 1.0);
#endif

  gl_FragColor = conversion * channels;
}
