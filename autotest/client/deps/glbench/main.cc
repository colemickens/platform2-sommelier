// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "main.h"


float RunTest(BenchFunc f) {
  float slope;
  int64_t bias;
  Bench(f, &slope, &bias);
  return slope;
}

static int arg1 = 0;
static void *arg2 = NULL;


void SwapTestFunc(int iter) {
  for (int i = 0 ; i < iter; ++i) {
    SwapBuffers();
  }
}

void SwapTest() {
  float x = RunTest(SwapTestFunc);
  printf("us_swap_swap: %g\n", x);
}


void ClearTestFunc(int iter) {
  GLbitfield mask = arg1;
  glClear(mask);
  glFlush();  // Kick GPU as soon as possible
  for (int i = 0 ; i < iter-1; ++i) {
    glClear(mask);
  }
}


void ClearTest() {
  arg1 = GL_COLOR_BUFFER_BIT;
  float x = RunTest(ClearTestFunc);
  printf("mpixels_sec_clear_color: %g\n", g_width * g_height / x);

  arg1 = GL_DEPTH_BUFFER_BIT;
  x = RunTest(ClearTestFunc);
  printf("mpixels_sec_clear_depth: %g\n", g_width * g_height / x);

  arg1 = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
  x = RunTest(ClearTestFunc);
  printf("mpixels_sec_clear_colordepth: %g\n", g_width * g_height / x);

  arg1 = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
  x = RunTest(ClearTestFunc);
  printf("mpixels_sec_clear_depthstencil: %g\n", g_width * g_height / x);

  arg1 = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
  x = RunTest(ClearTestFunc);
  printf("mpixels_sec_clear_colordepthstencil: %g\n", g_width * g_height / x);
}


GLuint SetupVBO(GLenum target, GLsizeiptr size, const GLvoid *data) {
  GLuint buf = ~0;
  glGenBuffers(1, &buf);
  glBindBuffer(target, buf);
  glBufferData(target, size, data, GL_STATIC_DRAW);

  return glGetError() == 0 ? buf : 0;
}


GLuint SetupTexture(GLsizei width, GLsizei height) {
  GLuint name = ~0;
  glGenTextures(1, &name);
  glBindTexture(GL_TEXTURE_2D, name);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  unsigned char *pixels = new unsigned char[width * height * 4];
  if (!pixels)
    return 0;

  unsigned char *p = pixels;
  for (int i = 0; i < width; i++) {
    for (int j = 0; j < height; j++) {
      for (int k = 0; k < 4; k++)
        *p++ = i ^ j;
    }
  }
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  delete[] pixels;

  assert(glGetError() == 0);
  return name;
}


static void FSQuad(int iter);
static void FillRateTest2(const char *name);

void FillRateTest() {
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  GLfloat buffer[8] = {
    -1.f, -1.f,
    1.f,  -1.f,
    -1.f, 1.f,
    1.f,  1.f,
  };
  glVertexPointer(2, GL_FLOAT, 0, buffer);
  glTexCoordPointer(2, GL_FLOAT, 0, buffer);
  glEnableClientState(GL_VERTEX_ARRAY);

  GLuint vbo = SetupVBO(GL_ARRAY_BUFFER, sizeof(buffer), buffer);
  if (vbo)
    printf("# Using VBO.\n");
  glVertexPointer(2, GL_FLOAT, 0, vbo ? 0 : buffer);
  glTexCoordPointer(2, GL_FLOAT, 0, vbo ? 0 : buffer);

  glColor4f(1.f, 0.f, 0.f, 1.f);
  FillRateTest2("fill_solid");

  glColor4f(1.f, 1.f, 1.f, 1.f);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glEnable(GL_TEXTURE_2D);

  GLuint texture = SetupTexture(256, 256);
  FillRateTest2("fill_tex_nearest");

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  FillRateTest2("fill_tex_bilinear");

  glDeleteBuffers(1, &vbo);
  glDeleteTextures(1, &texture);
}


static void FillRateTest2(const char *name) {
  float x = 0.f;

  {
    x = RunTest(FSQuad);
    printf("mpixels_sec_%s_opaque: %g\n", name, g_width * g_height / x);
  }

  {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    x = RunTest(FSQuad);
    printf("mpixels_sec_%s_blended: %g\n", name, g_width * g_height / x);
    glDisable(GL_BLEND);
  }

  {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_NOTEQUAL);
    x = RunTest(FSQuad);
    printf("mpixels_sec_%s_depth_neq: %g\n", name, g_width * g_height / x);
    glDepthFunc(GL_NEVER);
    x = RunTest(FSQuad);
    printf("mpixels_sec_%s_depth_never: %g\n", name, g_width * g_height / x);
    glDisable(GL_DEPTH_TEST);
  }
}


static void FSQuad(int iter) {
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glFlush();
  for (int i = 0; i < iter-1; ++i) {
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
}


static void TriangleSetupTestFunc(int iter) {
  glDrawElements(GL_TRIANGLES, arg1, GL_UNSIGNED_INT, arg2);
  glFlush();
  for (int i = 0 ; i < iter-1; ++i) {
    glDrawElements(GL_TRIANGLES, arg1, GL_UNSIGNED_INT, arg2);
  }
}

// Generates a mesh of 2*width*height triangles.  The ratio of front facing to
// back facing triangles is culled_ratio/RAND_MAX.  Returns the number of
// vertices in the mesh.
static int CreateMesh(GLuint *indices,
                      int width, int height, int culled_ratio) {
  srand(0);

  GLuint *iptr = indices;
  const int swath_height = 4;
  for (int j = 0; j < height - swath_height; j += swath_height) {
    for (int i = 0; i < width; i++) {
      for (int j2 = 0; j2 < swath_height; j2++) {
        GLuint first = (j+j2) * width + i;
        GLuint second = first + 1;
        GLuint third = first + width;
        GLuint fourth = third + 1;

        bool flag = rand() < culled_ratio;
        *iptr++ = first;
        *iptr++ = flag ? second : third;
        *iptr++ = flag ? third : second;

        *iptr++ = fourth;
        *iptr++ = flag ? third : second;
        *iptr++ = flag ? second : third;
      }
    }
  }

  return iptr - indices;
}

void TriangleSetupTest() {
  glViewport(-g_width, -g_height, g_width*2, g_height*2);
  glScalef(1.f / g_width, 1.f / g_height, 1.f);

  // Larger meshes make this test too slow for devices that do 1 mtri/sec.
  GLint width = 64;
  GLint height = 64;

  GLint *vertices = new GLint[2 * width * height];
  GLint *vptr = vertices;
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      *vptr++ = i;
      *vptr++ = j;
    }
  }

  GLuint vbo_v = SetupVBO(GL_ARRAY_BUFFER,
                          (vptr - vertices) * sizeof(GLint), vertices);
  glVertexPointer(2, GL_INT, 0, vbo_v != 0 ? 0 : vertices);
  glEnableClientState(GL_VERTEX_ARRAY);

  GLuint *indices = new GLuint[2 * 3 * (width * height)];

  arg1 = CreateMesh(indices, width, height, 0);
  GLuint vbo_i = SetupVBO(GL_ELEMENT_ARRAY_BUFFER,
                          arg1*sizeof(GLuint), indices);
  arg2 = vbo_i ? 0 : indices;

  float x = RunTest(TriangleSetupTestFunc);
  printf("mtri_sec_triangle_setup: %g\n", (arg1 / 3) / x);
  glEnable(GL_CULL_FACE);
  x = RunTest(TriangleSetupTestFunc);
  printf("mtri_sec_triangle_setup_all_culled: %g\n", (arg1 / 3) / x);
  glDeleteBuffers(1, &vbo_i);

  arg1 = CreateMesh(indices, width, height, RAND_MAX / 2);
  vbo_i = SetupVBO(GL_ELEMENT_ARRAY_BUFFER, arg1*sizeof(GLuint), indices);
  arg2 = vbo_i ? 0 : indices;
  x = RunTest(TriangleSetupTestFunc);
  printf("mtri_sec_triangle_setup_half_culled: %g\n", (arg1 / 3) / x);

  delete[] vertices;
  delete[] indices;
  glDeleteBuffers(1, &vbo_v);
  glDeleteBuffers(1, &vbo_i);
}


int main(int argc, char *argv[]) {
  if (!Init()) {
    printf("# Failed to initialize.\n");
    return 1;
  }

  void (*test[])() = {
    SwapTest,
    ClearTest,
    FillRateTest,
    TriangleSetupTest,
  };

  for (unsigned int i = 0; i < sizeof(test) / sizeof(*test); i++) {
    InitContext();
    test[i]();
    DestroyContext();
  }

  return 0;
}
