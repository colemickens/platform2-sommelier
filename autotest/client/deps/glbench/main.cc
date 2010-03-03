// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "base/logging.h"

#include "main.h"
#include "shaders.h"
#include "yuv2rgb.h"


const int enabled_tests_max = 8;
const char *enabled_tests[enabled_tests_max+1] = {NULL};
int seconds_to_run = 0;

void RunTest(BenchFunc f, const char *name, float coefficient, bool inverse) {
  float slope;
  int64_t bias;

  if (enabled_tests[0]) {
    bool found = false;
    for (const char **e = enabled_tests; *e; e++)
      if (strstr(name, *e)) {
        found = true;
        break;
      }
    if (!found)
      return;
  }

  GLenum err = glGetError();
  if (err != 0) {
    printf("# %s failed, glGetError returned 0x%x.\n", name, err);
    // float() in python will happily parse Nan.
    printf("%s: Nan\n", name);
  } else {
    Bench(f, &slope, &bias);
    printf("%s: %g\n", name, coefficient * (inverse ? 1. / slope : slope));
  }
}

static int arg1 = 0;
static void *arg2 = NULL;


void SwapTestFunc(int iter) {
  for (int i = 0 ; i < iter; ++i) {
    SwapBuffers();
  }
}

void SwapTest() {
  RunTest(SwapTestFunc, "us_swap_swap", 1., false);
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
  RunTest(ClearTestFunc, "mpixels_sec_clear_color", g_width * g_height, true);

  arg1 = GL_DEPTH_BUFFER_BIT;
  RunTest(ClearTestFunc, "mpixels_sec_clear_depth", g_width * g_height, true);

  arg1 = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
  RunTest(ClearTestFunc, "mpixels_sec_clear_colordepth",
          g_width * g_height, true);

  arg1 = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
  RunTest(ClearTestFunc, "mpixels_sec_clear_depthstencil",
          g_width * g_height, true);

  arg1 = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
  RunTest(ClearTestFunc, "mpixels_sec_clear_colordepthstencil",
          g_width * g_height, true);
}


GLuint SetupVBO(GLenum target, GLsizeiptr size, const GLvoid *data) {
  GLuint buf = ~0;
  glGenBuffers(1, &buf);
  glBindBuffer(target, buf);
  glBufferData(target, size, data, GL_STATIC_DRAW);

  return glGetError() == 0 ? buf : 0;
}


GLuint SetupTexture(GLsizei size_log2) {
  GLsizei size = 1 << size_log2;
  GLuint name = ~0;
  glGenTextures(1, &name);
  glBindTexture(GL_TEXTURE_2D, name);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  unsigned char *pixels = new unsigned char[size * size * 4];
  if (!pixels)
    return 0;

  for (GLint level = 0; size > 0; level++, size /= 2) {
    unsigned char *p = pixels;
    for (int i = 0; i < size; i++) {
      for (int j = 0; j < size; j++) {
        *p++ = level %3 != 0 ? (i ^ j) << level : 0;
        *p++ = level %3 != 1 ? (i ^ j) << level : 0;
        *p++ = level %3 != 2 ? (i ^ j) << level : 0;
        *p++ = 255;
      }
    }
    if (size == 1) {
      unsigned char *p = pixels;
      *p++ = 255;
      *p++ = 255;
      *p++ = 255;
      *p++ = 255;
    }
    glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, size, size, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  }
  delete[] pixels;
  return name;
}


static void DrawArraysTestFunc(int iter);
static void FillRateTestNormal(const char *name, float coeff=1.f);
static void FillRateTestBlendDepth(const char *name);

void FillRateTest() {
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  GLfloat buffer_vertex[8] = {
    -1.f, -1.f,
    1.f,  -1.f,
    -1.f, 1.f,
    1.f,  1.f,
  };
  GLfloat buffer_texture[8] = {
    0.f, 0.f,
    1.f, 0.f,
    0.f, 1.f,
    1.f, 1.f,
  };
  glEnableClientState(GL_VERTEX_ARRAY);

  GLuint vbo_vertex = SetupVBO(GL_ARRAY_BUFFER,
                               sizeof(buffer_vertex), buffer_vertex);
  if (!vbo_vertex)
    printf("# Not Using VBO!\n");
  glVertexPointer(2, GL_FLOAT, 0, vbo_vertex ? 0 : buffer_vertex);

  GLuint vbo_texture = SetupVBO(GL_ARRAY_BUFFER,
                                sizeof(buffer_texture), buffer_texture);
  glTexCoordPointer(2, GL_FLOAT, 0, vbo_texture ? 0 : buffer_texture);

  glColor4f(1.f, 0.f, 0.f, 1.f);
  FillRateTestNormal("fill_solid");
  FillRateTestBlendDepth("fill_solid");

  glColor4f(1.f, 1.f, 1.f, 1.f);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glEnable(GL_TEXTURE_2D);

  GLuint texture = SetupTexture(9);
  FillRateTestNormal("fill_tex_nearest");

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  FillRateTestNormal("fill_tex_bilinear");

  // lod = 0.5
  glScalef(0.7071f, 0.7071f, 1.f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_NEAREST);
  FillRateTestNormal("fill_tex_trilinear_nearest_05", 0.7071f * 0.7071f);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  FillRateTestNormal("fill_tex_trilinear_linear_05", 0.7071f * 0.7071f);

  // lod = 0.4
  glLoadIdentity();
  glScalef(0.758f, 0.758f, 1.f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  FillRateTestNormal("fill_tex_trilinear_linear_04", 0.758f * 0.758f);

  // lod = 0.1
  glLoadIdentity();
  glScalef(0.933f, 0.933f, 1.f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  FillRateTestNormal("fill_tex_trilinear_linear_01", 0.933f * 0.933f);

  glDeleteBuffers(1, &vbo_vertex);
  glDeleteBuffers(1, &vbo_texture);
  glDeleteTextures(1, &texture);
}


static void FillRateTestNormal(const char *name, float coeff) {
  const int buffer_len = 64;
  char buffer[buffer_len];
  snprintf(buffer, buffer_len, "mpixels_sec_%s", name);
  RunTest(DrawArraysTestFunc, buffer, coeff * g_width * g_height, true);
}

static void FillRateTestBlendDepth(const char *name) {
  const int buffer_len = 64;
  char buffer[buffer_len];

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);
  snprintf(buffer, buffer_len, "mpixels_sec_%s_blended", name);
  RunTest(DrawArraysTestFunc, buffer, g_width * g_height, true);
  glDisable(GL_BLEND);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_NOTEQUAL);
  snprintf(buffer, buffer_len, "mpixels_sec_%s_depth_neq", name);
  RunTest(DrawArraysTestFunc, buffer, g_width * g_height, true);
  glDepthFunc(GL_NEVER);
  snprintf(buffer, buffer_len, "mpixels_sec_%s_depth_never", name);
  RunTest(DrawArraysTestFunc, buffer, g_width * g_height, true);
  glDisable(GL_DEPTH_TEST);
}


static void DrawArraysTestFunc(int iter) {
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glFlush();
  for (int i = 0; i < iter-1; ++i) {
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
}


static void DrawElementsTestFunc(int iter) {
  glDrawElements(GL_TRIANGLES, arg1, GL_UNSIGNED_INT, arg2);
  glFlush();
  for (int i = 0 ; i < iter-1; ++i) {
    glDrawElements(GL_TRIANGLES, arg1, GL_UNSIGNED_INT, arg2);
  }
}

// Generates a tautological lattice.
static void CreateLattice(GLfloat **vertices, GLsizeiptr *size,
                          GLfloat size_x, GLfloat size_y,
                          int width, int height)
{
  GLfloat *vptr = *vertices = new GLfloat[2 * (width + 1) * (height + 1)];
  for (int j = 0; j <= height; j++) {
    for (int i = 0; i <= width; i++) {
      *vptr++ = i * size_x;
      *vptr++ = j * size_y;
    }
  }
  *size = (vptr - *vertices) * sizeof(GLfloat);
}

// Generates a mesh of 2*width*height triangles.  The ratio of front facing to
// back facing triangles is culled_ratio/RAND_MAX.  Returns the number of
// vertices in the mesh.
static int CreateMesh(GLuint **indices, GLsizeiptr *size,
                      int width, int height, int culled_ratio) {
  srand(0);

  GLuint *iptr = *indices = new GLuint[2 * 3 * (width * height)];
  const int swath_height = 4;

  CHECK(width % swath_height == 0 && height % swath_height == 0);

  for (int j = 0; j < height; j += swath_height) {
    for (int i = 0; i < width; i++) {
      for (int j2 = 0; j2 < swath_height; j2++) {
        GLuint first = (j + j2) * (width + 1) + i;
        GLuint second = first + 1;
        GLuint third = first + (width + 1);
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
  *size = (iptr - *indices) * sizeof(GLuint);

  return iptr - *indices;
}

void TriangleSetupTest() {
  glViewport(-g_width, -g_height, g_width*2, g_height*2);

  // Larger meshes make this test too slow for devices that do 1 mtri/sec.
  GLint width = 64;
  GLint height = 64;

  GLfloat *vertices = NULL;
  GLsizeiptr vertex_buffer_size = 0;
  CreateLattice(&vertices, &vertex_buffer_size, 1.f / g_width, 1.f / g_height,
                width, height);
  GLuint vertex_buffer = SetupVBO(GL_ARRAY_BUFFER,
                                  vertex_buffer_size, vertices);
  glVertexPointer(2, GL_FLOAT, 0, vertex_buffer != 0 ? 0 : vertices);
  glEnableClientState(GL_VERTEX_ARRAY);

  GLuint *indices = NULL;
  GLuint index_buffer = 0;
  GLsizeiptr index_buffer_size = 0;

  {
    arg1 = CreateMesh(&indices, &index_buffer_size, width, height, 0);

    index_buffer = SetupVBO(GL_ELEMENT_ARRAY_BUFFER,
                            index_buffer_size, indices);
    arg2 = index_buffer ? 0 : indices;

    RunTest(DrawElementsTestFunc, "mtri_sec_triangle_setup", arg1 / 3, true);
    glEnable(GL_CULL_FACE);
    RunTest(DrawElementsTestFunc, "mtri_sec_triangle_setup_all_culled",
            arg1 / 3, true);
    glDisable(GL_CULL_FACE);

    glDeleteBuffers(1, &index_buffer);
    delete[] indices;
  }

  {
    glColor4f(0.f, 1.f, 1.f, 1.f);
    arg1 = CreateMesh(&indices, &index_buffer_size, width, height,
                      RAND_MAX / 2);

    index_buffer = SetupVBO(GL_ELEMENT_ARRAY_BUFFER,
                            index_buffer_size, indices);
    arg2 = index_buffer ? 0 : indices;

    glEnable(GL_CULL_FACE);
    RunTest(DrawElementsTestFunc, "mtri_sec_triangle_setup_half_culled",
            arg1 / 3, true);

    glDeleteBuffers(1, &index_buffer);
    delete[] indices;
  }

  glDeleteBuffers(1, &vertex_buffer);
  delete[] vertices;
}


void AttributeFetchShaderTest() {
  GLint width = 64;
  GLint height = 64;

  glViewport(-g_width, -g_height, g_width*2, g_height*2);

  GLfloat *vertices = NULL;
  GLsizeiptr vertex_buffer_size = 0;
  CreateLattice(&vertices, &vertex_buffer_size, 1.f / g_width, 1.f / g_height,
                width, height);
  GLuint vertex_buffer = SetupVBO(GL_ARRAY_BUFFER,
                                  vertex_buffer_size, vertices);

  GLuint *indices = NULL;
  GLuint index_buffer = 0;
  GLsizeiptr index_buffer_size = 0;

  // Everything will be back-face culled.
  arg1 = CreateMesh(&indices, &index_buffer_size, width, height, 0);
  index_buffer = SetupVBO(GL_ELEMENT_ARRAY_BUFFER,
                          index_buffer_size, indices);

  glEnable(GL_CULL_FACE);

  GLuint vertex_buffers[8];
  for (GLuint i = 0; i < sizeof(vertex_buffers)/sizeof(vertex_buffers[0]); i++)
    vertex_buffers[i] = vertex_buffer;

  ShaderProgram program = AttributeFetchShaderProgram(1, vertex_buffers);
  RunTest(DrawElementsTestFunc,
          "mvtx_sec_attribute_fetch_shader", arg1, true);
  DeleteShaderProgram(program);

  program = AttributeFetchShaderProgram(2, vertex_buffers);
  RunTest(DrawElementsTestFunc,
          "mvtx_sec_attribute_fetch_shader_2_attr", arg1, true);
  DeleteShaderProgram(program);

  program = AttributeFetchShaderProgram(4, vertex_buffers);
  RunTest(DrawElementsTestFunc,
          "mvtx_sec_attribute_fetch_shader_4_attr", arg1, true);
  DeleteShaderProgram(program);

  program = AttributeFetchShaderProgram(8, vertex_buffers);
  RunTest(DrawElementsTestFunc,
          "mvtx_sec_attribute_fetch_shader_8_attr", arg1, true);
  DeleteShaderProgram(program);

  glDeleteBuffers(1, &index_buffer);
  delete[] indices;

  glDeleteBuffers(1, &vertex_buffer);
  delete[] vertices;
}


void VaryingsAndDdxyShaderTest() {
  glViewport(-g_width, -g_height, g_width*2, g_height*2);

  const int c = 4;
  GLfloat *vertices = NULL;
  GLsizeiptr vertex_buffer_size = 0;
  CreateLattice(&vertices, &vertex_buffer_size, 1.f / c, 1.f / c, c, c);
  GLuint vertex_buffer = SetupVBO(GL_ARRAY_BUFFER,
                                  vertex_buffer_size, vertices);

  GLuint *indices = NULL;
  GLuint index_buffer = 0;
  GLsizeiptr index_buffer_size = 0;

  arg1 = CreateMesh(&indices, &index_buffer_size, c, c, 0);
  index_buffer = SetupVBO(GL_ELEMENT_ARRAY_BUFFER,
                          index_buffer_size, indices);

  ShaderProgram program = VaryingsShaderProgram(1, vertex_buffer);
  RunTest(DrawElementsTestFunc,
          "mpixels_sec_varyings_shader_1", g_width * g_height, true);
  DeleteShaderProgram(program);

  program = VaryingsShaderProgram(2, vertex_buffer);
  RunTest(DrawElementsTestFunc,
          "mpixels_sec_varyings_shader_2", g_width * g_height, true);
  DeleteShaderProgram(program);

  program = VaryingsShaderProgram(4, vertex_buffer);
  RunTest(DrawElementsTestFunc,
          "mpixels_sec_varyings_shader_4", g_width * g_height, true);
  DeleteShaderProgram(program);

  program = VaryingsShaderProgram(8, vertex_buffer);
  RunTest(DrawElementsTestFunc,
          "mpixels_sec_varyings_shader_8", g_width * g_height, true);
  DeleteShaderProgram(program);

  program = DdxDdyShaderProgram(true, vertex_buffer);
  RunTest(DrawElementsTestFunc,
          "mpixels_sec_ddx_shader", g_width * g_height, true);
  DeleteShaderProgram(program);

  program = DdxDdyShaderProgram(false, vertex_buffer);
  RunTest(DrawElementsTestFunc,
          "mpixels_sec_ddy_shader", g_width * g_height, true);
  DeleteShaderProgram(program);

  glDeleteBuffers(1, &index_buffer);
  delete[] indices;

  glDeleteBuffers(1, &vertex_buffer);
  delete[] vertices;
}

void YuvToRgbShaderTest() {
  size_t size = 0;
  GLuint texture = ~0;
  ShaderProgram program = 0;
  GLuint vertex_buffer = 0;
  GLfloat vertices[8] = {
    0.f, 0.f,
    1.f, 0.f,
    0.f, 1.0f,
    1.f, 1.0f,
  };

  char *pixels = static_cast<char *>(MmapFile(YUV2RGB_NAME, &size));
  if (!pixels) {
    printf("# Could not open image file: %s\n", YUV2RGB_NAME);
    goto done;
  }
  if (size != YUV2RGB_SIZE) {
    printf("# Image file of wrong size, got %ld, expected %d\n",
           size, YUV2RGB_SIZE);
    goto done;
  }

  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, YUV2RGB_WIDTH, YUV2RGB_HEIGHT,
               0, GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);

  glViewport(-YUV2RGB_WIDTH, -(YUV2RGB_HEIGHT*2/3),
             YUV2RGB_WIDTH*2, (YUV2RGB_HEIGHT*2/3)*2);
  vertex_buffer = SetupVBO(GL_ARRAY_BUFFER, sizeof(vertices), vertices);

  program = YuvToRgbShaderProgram(vertex_buffer,
                                  YUV2RGB_WIDTH, YUV2RGB_HEIGHT*2/3);
  if (program == 0) {
    printf("# Could not set up YUV shader.\n");
    goto done;
  }

  FillRateTestNormal("yuv_shader_1");

done:
  glDeleteTextures(1, &texture);
  glDeleteBuffers(1, &vertex_buffer);
  DeleteShaderProgram(program);
  munmap(pixels, size);
}


// TODO: get the path from a command line option or something.
void *MmapFile(const char *name, size_t *length) {
  int fd = open(name, O_RDONLY);
  if (fd == -1)
    return NULL;

  struct stat sb;
  CHECK(fstat(fd, &sb) != -1);

  char *mmap_ptr = static_cast<char *>(
    mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));

  close(fd);

  if (mmap_ptr)
    *length = sb.st_size;

  return mmap_ptr;
}


// TODO: use proper command line parsing library.
static void ParseArgs(int argc, char *argv[]) {
  const char **enabled_tests_ptr = enabled_tests;
  bool test_name_arg = false;
  bool duration_arg = false;
  for (int i = 0; i < argc; i++) {
    if (test_name_arg) {
      test_name_arg = false;
      *enabled_tests_ptr++ = argv[i];
      if (enabled_tests_ptr - enabled_tests >= enabled_tests_max)
        break;
    } else if (duration_arg) {
      duration_arg = false;
      seconds_to_run = atoi(argv[i]);
    } else if (strcmp("-t", argv[i]) == 0) {
      test_name_arg = true;
    } else if (strcmp("-d", argv[i]) == 0) {
      duration_arg = true;
    }
  }
  *enabled_tests_ptr++ = NULL;
}


int main(int argc, char *argv[]) {
  ParseArgs(argc, argv);
  if (!Init()) {
    printf("# Failed to initialize.\n");
    return 1;
  }

  void (*test[])() = {
    SwapTest,
    ClearTest,
    FillRateTest,
    TriangleSetupTest,
    AttributeFetchShaderTest,
    VaryingsAndDdxyShaderTest,
    YuvToRgbShaderTest,
  };

  uint64_t done = GetUTime() + 1000000ULL * seconds_to_run;
  do {
    for (unsigned int i = 0; i < sizeof(test) / sizeof(*test); i++) {
      InitContext();
      test[i]();
      GLenum err = glGetError();
      if (err != 0)
        printf("# glGetError returned non-zero: 0x%x", err);
      DestroyContext();
    }
  } while (GetUTime() < done);

  return 0;
}
