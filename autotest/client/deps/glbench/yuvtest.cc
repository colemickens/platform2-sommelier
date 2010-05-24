// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <sys/mman.h>

#include "base/logging.h"

#include "main.h"
#include "testbase.h"
#include "utils.h"
#include "yuv2rgb.h"

namespace glbench {

class YuvToRgbTest : public DrawArraysTestFunc {
 public:
  YuvToRgbTest(int type, const char* name) : type_(type), name_(name) {}
  virtual ~YuvToRgbTest() {}
  virtual bool Run();

 private:
  int type_;
  const char* name_;
  DISALLOW_COPY_AND_ASSIGN(YuvToRgbTest);
};


GLuint YuvToRgbShaderProgram(int type, GLuint vertex_buffer,
                                    int width, int height) {
  const char *vertex = type == 1 ? YUV2RGB_VERTEX_1 : YUV2RGB_VERTEX_2;
  const char *fragment = type == 1 ? YUV2RGB_FRAGMENT_1 : YUV2RGB_FRAGMENT_2;
  size_t size_vertex = 0;
  size_t size_fragment = 0;
  char *yuv_to_rgb_vertex = static_cast<char *>(
      MmapFile(vertex, &size_vertex));
  char *yuv_to_rgb_fragment = static_cast<char *>(
      MmapFile(fragment, &size_fragment));
  GLuint program = 0;

  if (!yuv_to_rgb_fragment || !yuv_to_rgb_vertex)
    goto done;

  {
#if defined(I915_WORKAROUND)
    const char* header = "#define I915_WORKAROUND 1\n";
#else
    const char* header = NULL;
#endif
    program = InitShaderProgramWithHeader(header, yuv_to_rgb_vertex,
                                          yuv_to_rgb_fragment);

    int imageWidthUniform = glGetUniformLocation(program, "imageWidth");
    int imageHeightUniform = glGetUniformLocation(program, "imageHeight");
    int textureSampler = glGetUniformLocation(program, "textureSampler");
    int evenLinesSampler = glGetUniformLocation(program, "paritySampler");

    glUniform1f(imageWidthUniform, width);
    glUniform1f(imageHeightUniform, height);
    glUniform1i(textureSampler, 0);
    glUniform1i(evenLinesSampler, 1);

    int attribute_index = glGetAttribLocation(program, "c");
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(attribute_index);
    return program;
  }


done:
  munmap(yuv_to_rgb_fragment, size_fragment);
  munmap(yuv_to_rgb_fragment, size_vertex);
  return program;
}


bool YuvToRgbTest::Run() {
  size_t size = 0;
  GLuint texture[2];
  GLuint program = 0;
  GLuint vertex_buffer = 0;
  GLfloat vertices[8] = {
    0.f, 0.f,
    1.f, 0.f,
    0.f, 1.f,
    1.f, 1.f,
  };
  char evenodd[2] = {0, 255};
  const int pixel_height = YUV2RGB_HEIGHT * 2 / 3;

  char *pixels = static_cast<char *>(MmapFile(YUV2RGB_NAME, &size));
  if (!pixels) {
    printf("# Could not open image file: %s\n", YUV2RGB_NAME);
    goto done;
  }
  if (size != YUV2RGB_SIZE) {
    printf("# Image file of wrong size, got %d, expected %d\n",
           static_cast<int>(size), YUV2RGB_SIZE);
    goto done;
  }

  glGenTextures(2, texture);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture[0]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, YUV2RGB_WIDTH, YUV2RGB_HEIGHT,
               0, GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, texture[1]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 2, 1,
               0, GL_LUMINANCE, GL_UNSIGNED_BYTE, evenodd);

  glViewport(-YUV2RGB_WIDTH, -pixel_height, YUV2RGB_WIDTH*2, pixel_height * 2);
  vertex_buffer = SetupVBO(GL_ARRAY_BUFFER, sizeof(vertices), vertices);

  program = YuvToRgbShaderProgram(type_, vertex_buffer,
                                  YUV2RGB_WIDTH, pixel_height);

  if (program) {
    FillRateTestNormalSubWindow(name_, std::min(YUV2RGB_WIDTH, g_width),
                                std::min(pixel_height, g_height));
  } else {
    printf("# Could not set up YUV shader.\n");
  }

done:
  glDeleteProgram(program);
  glDeleteTextures(2, texture);
  glDeleteBuffers(1, &vertex_buffer);
  munmap(pixels, size);

  return true;
}


TestBase* GetYuvToRgbTest(int type, const char* name) {
  return new YuvToRgbTest(type, name);
}

} // namespace glbench
