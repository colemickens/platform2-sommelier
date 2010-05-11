// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "base/logging.h"
#include "base/string_util.h"

#include "main.h"
#include "utils.h"
#include "xlib_window.h"

#include "teartest.h"

typedef std::map<std::string, Test*> TestMap;


DEFINE_int32(refresh, 0,
             "If 1 or more, target refresh rate; otherwise enable vsync.");

DEFINE_string(tests, "uniform,teximage2d,pixmap_to_texture",
              "Comma-separated list of tests to run.");

DEFINE_int32(seconds_to_run, 5, "Seconds to run a test case for.");

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


Pixmap AllocatePixmap() {
  XWindowAttributes attributes;
  XGetWindowAttributes(g_xlib_display, g_xlib_window, &attributes);
  Pixmap pixmap = XCreatePixmap(g_xlib_display, g_xlib_window,
                                g_height, g_width, attributes.depth);
  GC gc = DefaultGC(g_xlib_display, 0);
  XSetForeground(g_xlib_display, gc, 0xffffff);
  XFillRectangle(g_xlib_display, pixmap, gc, 0, 0, g_height, g_width);
  UpdatePixmap(pixmap, 0);
  return pixmap;
}


void UpdatePixmap(Pixmap pixmap, int i) {
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


void CopyPixmapToTexture(Pixmap pixmap) {
  XImage *xim = XGetImage(g_xlib_display, pixmap, 0, 0, g_height, g_width,
                          AllPlanes, ZPixmap);
  CHECK(xim != NULL);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, g_height, g_width, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, (void*)(&xim->data[0]));
  XDestroyImage(xim);
}


// This test needs shift_uniform from outside, don't add it right away.
class UniformTest : public Test {
 public:
  UniformTest() : pixmap_(0), shift_uniform_(-1) {}
  virtual bool Start() {
    printf("# Plain texture draw.\n");
    pixmap_ = AllocatePixmap();
    CopyPixmapToTexture(pixmap_);
    return true;
  }

  virtual bool Loop(int shift) {
    glUniform1f(shift_uniform_, 1.f / g_width * shift);
    return true;
  }

  virtual void Stop() {
    glUniform1f(shift_uniform_, 0.f);
    XFreePixmap(g_xlib_display, pixmap_);
  }

  void SetUniform(int shift_uniform) {
    shift_uniform_ = shift_uniform;
  }

 private:
  Pixmap pixmap_;
  int shift_uniform_;
};

Test* GetUniformTest(int uniform) {
  UniformTest* ret = new UniformTest();
  ret->SetUniform(uniform);
  return ret;
}


class TexImage2DTest : public Test {
 public:
  TexImage2DTest() : pixmap_(0) {}
  virtual bool Start() {
    printf("# Full texture update.\n");
    pixmap_ = AllocatePixmap();
    CopyPixmapToTexture(pixmap_);
    return true;
  }

  virtual bool Loop(int shift) {
    UpdatePixmap(pixmap_, shift);
    // TODO: it's probably much cheaper to not use Pixmap and XImage.
    CopyPixmapToTexture(pixmap_);
    return true;
  }

  virtual void Stop() {
    XFreePixmap(g_xlib_display, pixmap_);
  }

 private:
  Pixmap pixmap_;
};

Test* GetTexImage2DTest() {
  return new TexImage2DTest();
}


int main(int argc, char* argv[]) {
  struct timespec* sleep_duration = NULL;
  g_height = -1;
  TestMap test_map;

  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_refresh >= 1) {
    sleep_duration = new struct timespec;
    sleep_duration->tv_sec = 0;
    sleep_duration->tv_nsec = static_cast<long>(1.e9 / FLAGS_refresh);
  }
  if (!Init()) {
    printf("# Failed to initialize.\n");
    return 1;
  }

  InitContext();
  glViewport(-g_width, -g_height, g_width*2, g_height*2);

  GLuint texture = GenerateAndBindTexture();

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

  // UniformTest needs a uniform from the shader program.  Get the uniform
  // and instantiate the test.
  Test* uniform_test = GetUniformTest(glGetUniformLocation(program, "shift"));
  test_map["uniform"] = uniform_test;
  test_map["teximage2d"] = GetTexImage2DTest();
#ifdef USE_EGL
  test_map["pixmap_to_texture"] = GetPixmapToTextureTestEGL();
#else
  test_map["pixmap_to_texture"] = GetPixmapToTextureTest();
#endif

  SwapInterval(sleep_duration ? 0 : 1);

  std::vector<std::string> tests;
  SplitString(FLAGS_tests, ',', &tests);

  int return_code = 0;
  for (std::vector<std::string>::iterator it = tests.begin();
       it != tests.end();
       it++)
  {
    Test* t = test_map[*it];
    if (!t || !t->Start()) {
      return_code = 1;
      continue;
    }

    Bool got_event = False;
    uint64_t wait_until = GetUTime() + 1000000ULL * FLAGS_seconds_to_run;
    for (int x = 0;
         !got_event && GetUTime() < wait_until;
         x = (x + 4) % (2 * g_width))
    {
      const int shift = x < g_width ? x : 2 * g_width - x;

      t->Loop(shift);

      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glFlush();

      if (sleep_duration)
        nanosleep(sleep_duration, NULL);

      SwapBuffers();

      XEvent event;
      got_event = XCheckWindowEvent(g_xlib_display, g_xlib_window,
                                    KeyPressMask, &event);
    }

    t->Stop();
  }

  // TODO: cleaner teardown.

  glDeleteTextures(1, &texture);
  DestroyContext();
  return return_code;
}
