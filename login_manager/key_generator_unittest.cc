// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/key_generator.h"

#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
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

#include "login_manager/fake_child_process.h"
#include "login_manager/fake_generated_key_handler.h"
#include "login_manager/fake_generator_job.h"
#include "login_manager/keygen_worker.h"
#include "login_manager/matchers.h"
#include "login_manager/mock_nss_util.h"
#include "login_manager/mock_system_utils.h"
#include "login_manager/nss_util.h"
#include "login_manager/system_utils_impl.h"

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
  MockSystemUtils utils_;
  base::ScopedTempDir tmpdir_;
  const base::FilePath original_user_prefix_;
  std::string fake_salt_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyGeneratorTest);
};

TEST_F(KeyGeneratorTest, KeygenEndToEndTest) {
  FakeGeneratedKeyHandler handler;

  pid_t kDummyPid = 4;
  std::string fake_ownername("user");
  std::string fake_key_contents("stuff");
  siginfo_t fake_info;
  memset(&fake_info, 0, sizeof(siginfo_t));

  KeyGenerator keygen(getuid(), &utils_);
  keygen.set_delegate(&handler);
  keygen.InjectJobFactory(
      scoped_ptr<GeneratorJobFactoryInterface>(
          new FakeGeneratorJob::Factory(kDummyPid, "gen", fake_key_contents)));

  ASSERT_TRUE(keygen.Start(fake_ownername));
  keygen.HandleExit(fake_info);

  EXPECT_EQ(fake_ownername, handler.key_username());
  EXPECT_GT(handler.key_contents().size(), 0);
}

TEST_F(KeyGeneratorTest, GenerateKey) {
  MockNssUtil nss;
  EXPECT_CALL(nss, GetNssdbSubpath()).Times(1);
  ON_CALL(nss, GenerateKeyPairForUser(_))
      .WillByDefault(InvokeWithoutArgs(MockNssUtil::CreateShortKey));
  EXPECT_CALL(nss, GenerateKeyPairForUser(_)).Times(1);

  const FilePath key_file_path(tmpdir_.path().AppendASCII("foo.pub"));
  ASSERT_EQ(keygen::GenerateKey(key_file_path, tmpdir_.path(), &nss), 0);
  ASSERT_TRUE(file_util::PathExists(key_file_path));

  SystemUtilsImpl utils;
  int32 file_size = 0;
  ASSERT_TRUE(utils.EnsureAndReturnSafeFileSize(key_file_path, &file_size));
  ASSERT_GT(file_size, 0);
}

}  // namespace login_manager
