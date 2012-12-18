// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"

#include "testbase.h"
#include "utils.h"

namespace glbench {

class GLInterfaceTest : public TestBase {
 public:
  GLInterfaceTest() : index_buffer_object_(0),
               vertex_buffer_object_(0),
               num_indices_(0),
               shader_program_(0),
               attribute_index_(0) {}
  virtual ~GLInterfaceTest() {}
  virtual bool TestFunc(int iter) = 0;
  virtual bool Run();
  virtual const char* Name() const = 0;

 protected:
  // Callback for GL rendering function to be run before GLX/EGL calls.
  base::Callback<void(void)> render_func_;

 private:
  // For calling some GL operations before GLX/EGL calls.
  void SetupGLRendering();
  void RenderGLSimple();
  void CleanupGLRendering();

  // For GL rendering.
  GLuint index_buffer_object_;
  GLuint vertex_buffer_object_;
  GLsizei num_indices_;
  GLuint shader_program_;
  GLint attribute_index_;

  DISALLOW_COPY_AND_ASSIGN(GLInterfaceTest);
};

} // namespace glbench
