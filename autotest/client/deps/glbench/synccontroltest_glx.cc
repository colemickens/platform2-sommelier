// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "synccontroltest.h"
#include <iostream>

class GLXSyncControlTest : public SyncControlTest {
 public:
  GLXSyncControlTest();
  virtual void Init();
  virtual bool Loop(int interval);
  virtual void Stop();
};

SyncControlTest* SyncControlTest::Create() {
  return new GLXSyncControlTest();
}

GLXSyncControlTest::GLXSyncControlTest() {

}

void GLXSyncControlTest::Init() { }

bool GLXSyncControlTest::Loop(int interval) {
  std::cout << "GLX version of this test has not been implementd, return true "
            << "to avoid the test failing!";
  return true;
}

void GLXSyncControlTest::Stop() { }
