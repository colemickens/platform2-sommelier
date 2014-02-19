// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/child_exit_handler.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <vector>

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/message_loop.h>
#include <gtest/gtest.h>

#include "login_manager/fake_job_manager.h"
#include "login_manager/mock_system_utils.h"

namespace login_manager {
using ::testing::Return;
using ::testing::_;

static const pid_t kDummyPid = 4;

class ChildExitHandlerTest : public ::testing::Test {
 public:
  ChildExitHandlerTest() : handler_(&mock_utils_), fake_manager_(kDummyPid) {}
  virtual ~ChildExitHandlerTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    std::vector<JobManagerInterface*> managers;
    managers.push_back(&fake_manager_);
    handler_.Init(managers);
  }

  virtual void TearDown() {
    ChildExitHandler::RevertHandlers();
  }

 protected:
  MessageLoopForIO loop_;
  MockSystemUtils mock_utils_;
  ChildExitHandler handler_;
  FakeJobManager fake_manager_;
  base::ScopedTempDir temp_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChildExitHandlerTest);
};

TEST_F(ChildExitHandlerTest, DataOnPipe) {
  siginfo_t canned = {};
  canned.si_pid = kDummyPid;
  canned.si_signo = SIGCHLD;
  canned.si_code = CLD_EXITED;
  canned.si_status = 1;

  base::FilePath scratch;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(), &scratch));
  ASSERT_TRUE(file_util::WriteFile(scratch,
                                   reinterpret_cast<char*>(&canned),
                                   sizeof(canned)));
  int canned_fd = open(scratch.value().c_str(), O_RDONLY);
  ASSERT_GT(canned_fd, 0);

  EXPECT_CALL(mock_utils_, ChildIsGone(kDummyPid, _)).WillOnce(Return(true));

  handler_.OnFileCanReadWithoutBlocking(canned_fd);
  EXPECT_EQ(canned.si_pid, fake_manager_.last_status().si_pid);
  EXPECT_EQ(canned.si_signo, fake_manager_.last_status().si_signo);
  EXPECT_EQ(canned.si_code, fake_manager_.last_status().si_code);
  EXPECT_EQ(canned.si_status, fake_manager_.last_status().si_status);
  ASSERT_EQ(close(canned_fd), 0);
}

}  // namespace login_manager
