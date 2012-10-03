// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_CHILD_JOB_H_
#define LOGIN_MANAGER_MOCK_CHILD_JOB_H_

#include "login_manager/child_job.h"

#include <gmock/gmock.h>
#include <string>
#include <vector>

namespace login_manager {
class MockChildJob : public ChildJobInterface {
 public:
  MockChildJob();
  virtual ~MockChildJob();
  MOCK_CONST_METHOD0(ShouldStop, bool());
  MOCK_METHOD0(RecordTime, void());
  MOCK_METHOD0(Run, void());
  MOCK_METHOD1(StartSession, void(const std::string&));
  MOCK_METHOD0(StopSession, void());
  MOCK_CONST_METHOD0(GetDesiredUid, uid_t());
  MOCK_METHOD1(SetDesiredUid, void(uid_t));
  MOCK_CONST_METHOD0(IsDesiredUidSet, bool());
  MOCK_CONST_METHOD0(GetName, const std::string());
  MOCK_METHOD1(SetArguments, void(const std::string&));
  MOCK_METHOD1(SetExtraArguments, void(const std::vector<std::string>&));
  MOCK_METHOD1(AddOneTimeArgument, void(const std::string&));
  MOCK_METHOD0(ClearOneTimeArgument, void());
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_CHILD_JOB_H_
