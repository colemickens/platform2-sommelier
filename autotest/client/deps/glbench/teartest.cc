// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "main.h"
#include "utils.h"
#include "xlib_window.h"


GLuint GenerateAndBindTexture() {
  GLuint name = ~0;
  glGenTextures(1, &name);
  glBindTexture(GL_TEXTURE_2D, name);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  return name;
}


char *CreateBitmap(int w, int h) {
  char *bitmap = new char[w * h];
  memset(bitmap, 255, w * h);
  memset(bitmap + w * (1 + h / 2), 0, w);
  memset(bitmap + w * (5 + h / 2), 0, w);
  return bitmap;
}


const char *vertex_shader =
    "attribute vec4 c;"
    "uniform float shift;"
    "void main() {"
    "    gl_Position = c;"
    "    gl_TexCoord[0] = vec4(c.y, c.x - shift, 0.0, 0.0);"
    "}";

const char *fragment_shader =
    "uniform sampler2D tex;"
    "void main() {"
    "    gl_FragColor = texture2D(tex, gl_TexCoord[0].xy);"
    "}";


// If refresh is set to zero, we enable vsync.  Otherwise we redraw that many
// times a second.
struct timespec* g_sleep_duration = NULL;
static void ParseArgs(int argc, char* argv[]) {
  bool refresh_arg = false;
  for (int i = 0; i < argc; i++) {
    if (refresh_arg) {
      refresh_arg = false;

      int refresh = atoi(argv[i]);
      if (refresh > 1) {
        delete g_sleep_duration;
        g_sleep_duration = new struct timespec;
        g_sleep_duration->tv_sec = 0;
        g_sleep_duration->tv_nsec = static_cast<long>(1.e9 / refresh);
      } else {
        printf("-r requires integer greater than one.\n");
      }
    } else if (strcmp("-o", argv[i]) == 0) {
      g_override_redirect = true;
    } else if (strcmp("-r", argv[i]) == 0) {
      refresh_arg = true;
    }
  }
}


int main(int argc, char* argv[]) {
  g_override_redirect = false;
  g_height = -1;
  ParseArgs(argc, argv);
  if (!Init()) {
    printf("# Failed to initialize.\n");
    return 1;
  }

  InitContext();
  glViewport(-g_width, -g_height, g_width*2, g_height*2);

  char *bitmap = CreateBitmap(g_height, g_width*2);
  GLuint texture = GenerateAndBindTexture();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, g_height, g_width, 0,
               GL_LUMINANCE, GL_UNSIGNED_BYTE, bitmap + g_height * g_width);

  GLfloat vertices[8] = {
    0.f, 0.f,
    1.f, 0.f,
    0.f, 1.f,
    1.f, 1.f,
  };

  GLuint program = InitShaderProgram(vertex_shader, fragment_shader);
  int attribute_index = glGetAttribLocation(program, "c");
  glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, 0, vertices);
  glEnableVertexAttribArray(attribute_index);

  int texture_sampler = glGetUniformLocation(program, "tex");
  glUniform1f(texture_sampler, 0);

  int shift_uniform = glGetUniformLocation(program, "shift");
  int i = 0;
  SwapInterval(g_sleep_duration ? 0 : 1);
  for (;;) {
    XEvent event;
    Bool got_event = XCheckWindowEvent(g_xlib_display, g_xlib_window,
                                       KeyPressMask, &event);
    if (got_event)
      break;
    glClear(GL_COLOR_BUFFER_BIT);
    glUniform1f(shift_uniform, 1.f / g_width *
                (i < g_width ? i : 2 * g_width - i));
    i = (i + 4) % (2 * g_width);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glFlush();

    if (g_sleep_duration)
      nanosleep(g_sleep_duration, NULL);

    SwapBuffers();
  }

  glDeleteTextures(1, &texture);
  DestroyContext();
  return 0;
}
