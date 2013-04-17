// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/key_generator.h"

#include <signal.h>
#include <unistd.h>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/time.h>
#include <chromeos/cryptohome.h>
#include <gtest/gtest.h>

#include "login_manager/child_job.h"
#include "login_manager/keygen_worker.h"
#include "login_manager/matchers.h"
#include "login_manager/mock_child_job.h"
#include "login_manager/mock_child_process.h"
#include "login_manager/mock_nss_util.h"
#include "login_manager/mock_process_manager_service.h"
#include "login_manager/mock_system_utils.h"
#include "login_manager/nss_util.h"
#include "login_manager/system_utils.h"

namespace login_manager {
using chromeos::cryptohome::home::GetUserPathPrefix;
using chromeos::cryptohome::home::SetUserHomePrefix;
using chromeos::cryptohome::home::SetSystemSalt;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;

class KeyGeneratorTest : public ::testing::Test {
 public:
  KeyGeneratorTest()
      : original_user_prefix_(GetUserPathPrefix()),
        fake_salt_("fake salt") {
  }

  virtual ~KeyGeneratorTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    SetUserHomePrefix(tmpdir_.path().value() + "/");
    SetSystemSalt(&fake_salt_);
  }

  virtual void TearDown() {
    SetUserHomePrefix(original_user_prefix_.value());
    SetSystemSalt(NULL);
  }

 protected:
  int PackStatus(int status) { return __W_EXITCODE(status, 0); }

  MockSystemUtils utils_;
  base::ScopedTempDir tmpdir_;
  const base::FilePath original_user_prefix_;
  std::string fake_salt_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyGeneratorTest);
};

TEST_F(KeyGeneratorTest, KeygenEndToEndTest) {
  MockProcessManagerService manager;

  uid_t kFakeUid = 7;
  pid_t kDummyPid = 4;
  std::string fake_ownername("user");

  scoped_ptr<MockChildJob> k_job(new MockChildJob);
  EXPECT_CALL(*k_job.get(), SetDesiredUid(kFakeUid)).Times(1);
  EXPECT_CALL(*k_job.get(), IsDesiredUidSet()).WillRepeatedly(Return(true));
  EXPECT_CALL(utils_, fork()).WillOnce(Return(kDummyPid));

  KeyGenerator keygen(&utils_, &manager);
  keygen.InjectMockKeygenJob(k_job.release());

  manager.ExpectAdoptAndAbandon(kDummyPid);
  EXPECT_CALL(manager,
              ProcessNewOwnerKey(StrEq(fake_ownername),
                                 PathStartsWith(tmpdir_.path())))
      .Times(1);

  ASSERT_TRUE(keygen.Start(fake_ownername, kFakeUid));
  KeyGenerator::HandleKeygenExit(kDummyPid,
                                 PackStatus(0),
                                 &keygen);
}

TEST_F(KeyGeneratorTest, GenerateKey) {
  MockNssUtil nss;
  EXPECT_CALL(nss, GetNssdbSubpath()).Times(1);
  ON_CALL(nss, GenerateKeyPairForUser(_))
      .WillByDefault(InvokeWithoutArgs(MockNssUtil::CreateShortKey));
  EXPECT_CALL(nss, GenerateKeyPairForUser(_)).Times(1);

  const FilePath key_file_path(tmpdir_.path().AppendASCII("foo.pub"));
  ASSERT_EQ(keygen::GenerateKey(key_file_path ,tmpdir_.path(), &nss), 0);
  ASSERT_TRUE(file_util::PathExists(key_file_path));

  SystemUtils utils;
  int32 file_size = 0;
  ASSERT_TRUE(utils.EnsureAndReturnSafeFileSize(key_file_path, &file_size));
  ASSERT_GT(file_size, 0);
}

}  // namespace login_manager
