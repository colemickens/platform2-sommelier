// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/key_generator.h"

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/basictypes.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_temp_dir.h>
#include <gtest/gtest.h>
#include <signal.h>
#include <unistd.h>

#include "login_manager/keygen_worker.h"
#include "login_manager/mock_child_job.h"
#include "login_manager/mock_child_process.h"
#include "login_manager/mock_system_utils.h"
#include "login_manager/session_manager_service.h"
#include "login_manager/system_utils.h"

namespace login_manager {
using ::testing::A;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;

class KeyGeneratorTest : public ::testing::Test {
 public:
  KeyGeneratorTest() : manager_(NULL) {}

  virtual ~KeyGeneratorTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    std::vector<ChildJobInterface*> jobs;
    manager_ = new SessionManagerService(jobs, &utils_);
  }

  virtual void TearDown() {
    manager_ = NULL;
  }

 protected:
  scoped_refptr<SessionManagerService> manager_;
  MockSystemUtils utils_;
  ScopedTempDir tmpdir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyGeneratorTest);
};

TEST_F(KeyGeneratorTest, GenerateKey) {
  uid_t kFakeUid = 7;
  scoped_ptr<MockChildJob> k_job(new MockChildJob);
  EXPECT_CALL(*k_job.get(), SetDesiredUid(kFakeUid)).Times(1);
  EXPECT_CALL(*k_job.get(), IsDesiredUidSet()).WillRepeatedly(Return(true));

  MockChildProcess proc(8, 0, manager_->test_api());
  EXPECT_CALL(utils_, fork()).WillOnce(Return(proc.pid()));

  KeyGenerator keygen(&utils_);
  keygen.InjectMockKeygenJob(k_job.release());
  ASSERT_TRUE(keygen.Start(kFakeUid, manager_.get()));
}

TEST_F(KeyGeneratorTest, KeygenTest) {
  const FilePath key_file_path(tmpdir_.path().AppendASCII("foo.pub"));
  ASSERT_EQ(keygen::generate(key_file_path.value()), 0);
  ASSERT_TRUE(file_util::PathExists(key_file_path));

  SystemUtils utils;
  int32 file_size = 0;
  ASSERT_TRUE(utils.EnsureAndReturnSafeFileSize(key_file_path, &file_size));
  ASSERT_GT(file_size, 0);
}

}  // namespace login_manager
