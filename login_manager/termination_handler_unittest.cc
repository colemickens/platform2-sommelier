// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/termination_handler.h"

#include <base/basictypes.h>
#include <base/message_loop.h>
#include <gtest/gtest.h>

#include "login_manager/mock_process_manager_service.h"

namespace login_manager {

class TerminationHandlerTest : public ::testing::Test {
 public:
  TerminationHandlerTest() : handler_(&mock_manager_) {}
  virtual ~TerminationHandlerTest() {}

  virtual void SetUp() {
    handler_.Init();
  }

  virtual void TearDown() {
    TerminationHandler::RevertHandlers();
  }

 protected:
  MessageLoopForIO loop_;
  MockProcessManagerService mock_manager_;
  TerminationHandler handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TerminationHandlerTest);
};

TEST_F(TerminationHandlerTest, DataOnPipe) {
  EXPECT_CALL(mock_manager_, ScheduleShutdown()).Times(1);
  handler_.OnFileCanReadWithoutBlocking(0);
}

}  // namespace login_manager
