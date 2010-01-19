// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_CHILD_JOB_H_
#define LOGIN_MANAGER_MOCK_CHILD_JOB_H_

#include "login_manager/child_job.h"

#include <gmock/gmock.h>

namespace login_manager {
class MockChildJob : public ChildJob {
 public:
   MockChildJob() { }
   ~MockChildJob() { }
   MOCK_METHOD0(ShouldRun, bool());
   MOCK_METHOD0(Toggle, void());
   MOCK_METHOD0(Run, void());
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_CHILD_JOB_H_
