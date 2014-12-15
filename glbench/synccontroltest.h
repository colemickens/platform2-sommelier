// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BENCH_GL_SYNCCONTROLTEST_H_
#define BENCH_GL_SYNCCONTROLTEST_H_

class SyncControlTest {
 public:
  static SyncControlTest* Create();
  SyncControlTest() { }
  virtual ~SyncControlTest() { }
  virtual void Init() = 0;
  virtual bool Loop(int interval) = 0;
};

#endif  //  BENCH_GL_SYNCCONTROLTEST_H_
