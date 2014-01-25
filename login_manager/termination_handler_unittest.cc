// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/termination_handler.h"

#include <signal.h>
#include <sys/types.h>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/time.h>
#include <chromeos/cryptohome.h>
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
