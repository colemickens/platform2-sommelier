// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "base/logging.h"

#include "main.h"
#include "utils.h"
#include "xlib_window.h"

#include "teartest.h"


static Pixmap pixmap = 0;
static int shift_uniform = 0;

GLuint GenerateAndBindTexture() {
  GLuint name = ~0;
  glGenTextures(1, &name);
  glBindTexture(GL_TEXTURE_2D, name);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  return name;
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


void AllocatePixmap() {
  XWindowAttributes attributes;
  XGetWindowAttributes(g_xlib_display, g_xlib_window, &attributes);
  pixmap = XCreatePixmap(g_xlib_display, g_xlib_window,
                         g_height, g_width, attributes.depth);
}

void InitializePixmap() {
  GC gc = DefaultGC(g_xlib_display, 0);
  XSetForeground(g_xlib_display, gc, 0xffffff);
  XFillRectangle(g_xlib_display, pixmap, gc, 0, 0, g_height, g_width);
  UpdatePixmap(0);
}

void UpdatePixmap(int i) {
  static int last_i = 0;
  GC gc = DefaultGC(g_xlib_display, 0);
  XSetForeground(g_xlib_display, gc, 0xffffff);
  XDrawLine(g_xlib_display, pixmap, gc,
            0, last_i, g_height - 1, last_i);
  XDrawLine(g_xlib_display, pixmap, gc,
            0, last_i + 4, g_height - 1, last_i + 4);

  XSetForeground(g_xlib_display, gc, 0x000000);
  XDrawLine(g_xlib_display, pixmap, gc, 0, i, g_height - 1, i);
  XDrawLine(g_xlib_display, pixmap, gc, 0, i + 4, g_height - 1, i + 4);

  last_i = i;
}

void CopyPixmapToTexture() {
  XImage *xim = XGetImage(g_xlib_display, pixmap, 0, 0, g_height, g_width,
                          AllPlanes, ZPixmap);
  CHECK(xim != NULL);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, g_height, g_width, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, (void*)(&xim->data[0]));
  XDestroyImage(xim);
}


bool UpdateUniform(TestState state, int shift) {
  switch (state) {
    case TestStart:
      printf("# Plain texture draw.\n");
      InitializePixmap();
      CopyPixmapToTexture();
      break;

    case TestLoop:
      glUniform1f(shift_uniform, 1.f / g_width * shift);
      break;

    case TestStop:
      glUniform1f(shift_uniform, 0.f);
      break;
  }
  return true;
}

bool UpdateTexImage2D(TestState state, int shift) {
  switch (state) {
    case TestStart:
      printf("# Full texture update.\n");
      InitializePixmap();
      CopyPixmapToTexture();
      break;

    case TestLoop: {
      UpdatePixmap(shift);
      // TODO: it's probably much cheaper to not use Pixmap and XImage.
      CopyPixmapToTexture();
    }

    case TestStop:
      break;
  }
  return true;
}


Test test[] = {
  UpdateUniform,
  UpdateTexImage2D,
  UpdateBindTexImage
};

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

  GLuint texture = GenerateAndBindTexture();

  AllocatePixmap();
  InitNative(pixmap);

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

  shift_uniform = glGetUniformLocation(program, "shift");
  SwapInterval(g_sleep_duration ? 0 : 1);

  for (unsigned int i = 0; i < sizeof(test)/sizeof(*test); i++)
  {
    XEvent event;
    if (!test[i](TestStart, 0))
      continue;

    Bool got_event = False;
    for (int x = 0; !got_event; x = (x + 4) % (2 * g_width)) {
      const int shift = x < g_width ? x : 2 * g_width - x;

      test[i](TestLoop, shift);

      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glFlush();

      if (g_sleep_duration)
        nanosleep(g_sleep_duration, NULL);

      SwapBuffers();

      got_event = XCheckWindowEvent(g_xlib_display, g_xlib_window,
                                    KeyPressMask, &event);
    }

    test[i](TestStop, 0);
  }

  // TODO: clean teardown.

  glDeleteTextures(1, &texture);
  DestroyContext();
  return 0;
}
