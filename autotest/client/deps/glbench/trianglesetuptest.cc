// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "main.h"
#include "testbase.h"
#include "utils.h"


namespace glbench {


class TriangleSetupTest : public DrawElementsTestFunc {
 public:
  TriangleSetupTest() {}
  virtual ~TriangleSetupTest() {}
  virtual bool Run();
  virtual const char* Name() const { return "triangle_setup"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TriangleSetupTest);
};


bool TriangleSetupTest::Run() {
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
  glVertexPointer(2, GL_FLOAT, 0, 0);
  glEnableClientState(GL_VERTEX_ARRAY);

  GLuint *indices = NULL;
  GLuint index_buffer = 0;
  GLsizeiptr index_buffer_size = 0;

  {
    count_ = CreateMesh(&indices, &index_buffer_size, width, height, 0);

    index_buffer = SetupVBO(GL_ELEMENT_ARRAY_BUFFER,
                            index_buffer_size, indices);
    RunTest(this, "mtri_sec_triangle_setup", count_ / 3, true);
    glEnable(GL_CULL_FACE);
    RunTest(this, "mtri_sec_triangle_setup_all_culled", count_ / 3, true);
    glDisable(GL_CULL_FACE);

    glDeleteBuffers(1, &index_buffer);
    delete[] indices;
  }

  {
    glColor4f(0.f, 1.f, 1.f, 1.f);
    count_ = CreateMesh(&indices, &index_buffer_size, width, height,
                        RAND_MAX / 2);

    index_buffer = SetupVBO(GL_ELEMENT_ARRAY_BUFFER,
                            index_buffer_size, indices);
    glEnable(GL_CULL_FACE);
    RunTest(this, "mtri_sec_triangle_setup_half_culled", count_ / 3, true);

    glDeleteBuffers(1, &index_buffer);
    delete[] indices;
  }

  glDeleteBuffers(1, &vertex_buffer);
  delete[] vertices;
  return true;
}


TestBase* GetTriangleSetupTest() {
  return new TriangleSetupTest;
}


} // namespace glbench
