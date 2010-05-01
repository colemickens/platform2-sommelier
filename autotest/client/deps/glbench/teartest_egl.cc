// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "teartest.h"


class PixmapToTextureTestEGL : public Test {
 public:
  PixmapToTextureTestEGL() {}
  virtual bool Start() {
    printf("# PixmapToTextureTestEGL not implemented.\n");
    return false;
  }
  virtual bool Loop(int shift) { return false; }
  virtual void Stop() {}
};

Test* GetPixmapToTextureTestEGL() {
  return new PixmapToTextureTestEGL();
}
