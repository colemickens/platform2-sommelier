// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Application that displays graphics using OpenGL [ES] with the intent
// of being used in functional tests.

#include <gflags/gflags.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cmath>

#include "main.h"
#include "utils.h"


GLuint GenerateAndBindTexture() {
  GLuint name = ~0;
  glGenTextures(1, &name);
  glBindTexture(GL_TEXTURE_2D, name);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  return name;
}


unsigned char* CreateBitmap(int w, int h) {
  unsigned char* bitmap = new unsigned char[w * h];
  unsigned char* pixel = bitmap;
  float w2 = w/2.0f;
  float h2 = h/2.0f;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      // Fill with soft ellipse
      float dx = fabs((x - w2) / w2);
      float dy = fabs((y - h2) / h2);
      float dist2 = dx*dx + dy*dy;
      if (dist2 > 1.f)
        dist2 = 1.f;
      *pixel = (1.f-dist2) * 255.f;
      pixel++;
    }
  }
  return bitmap;
}


const char kVertexShader[] =
    "attribute vec4 c;"
    "void main() {"
    "    gl_Position = c;"
    "    gl_TexCoord[0] = vec4(c.y, c.x, 0.0, 0.0);"
    "}";

const char kFragmentShader[] =
    "uniform sampler2D tex;"
    "void main() {"
    "    gl_FragColor = texture2D(tex, gl_TexCoord[0].xy);"
    "}";


// If refresh is set to zero, we enable vsync.  Otherwise we redraw that many
// times a second.
DEFINE_int32(seconds_to_run, 10, "seconds to run application for");

int main(int argc, char* argv[]) {
  g_width = -1;
  g_height = -1;
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (!Init()) {
    printf("# Failed to initialize.\n");
    return 1;
  }

  InitContext();
  glViewport(-g_width, -g_height, g_width*2, g_height*2);

  unsigned char* bitmap = CreateBitmap(g_height, g_width);
  GLuint texture = GenerateAndBindTexture();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, g_height, g_width, 0,
               GL_LUMINANCE, GL_UNSIGNED_BYTE, bitmap);

  GLfloat vertices[8] = {
    0.f, 0.f,
    1.f, 0.f,
    0.f, 1.f,
    1.f, 1.f,
  };

  GLuint program = InitShaderProgram(kVertexShader, kFragmentShader);
  int attribute_index = glGetAttribLocation(program, "c");
  glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, vertices);
  glEnableVertexAttribArray(attribute_index);

  int texture_sampler = glGetUniformLocation(program, "tex");
  glUniform1f(texture_sampler, 0);

  uint64_t wait_until_done = GetUTime() + 1000000ULL * FLAGS_seconds_to_run;
  do {
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    SwapBuffers();
  } while (GetUTime() < wait_until_done);

  glDeleteTextures(1, &texture);
  DestroyContext();
  return 0;
}
