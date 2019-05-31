// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "smbprovider/fake_kerberos_artifact_client.h"
#include "smbprovider/kerberos_artifact_client.h"
#include "smbprovider/kerberos_artifact_synchronizer.h"
#include "smbprovider/smbprovider_test_helper.h"

namespace smbprovider {

namespace {

void ExpectSetupSuccess(bool success) {
  EXPECT_TRUE(success);
}

void ExpectSetupFailure(bool success) {
  EXPECT_FALSE(success);
}

void IncrementInt(int* count, bool expected_success, bool success) {
  EXPECT_EQ(expected_success, success);
  (*count)++;
}

}  // namespace

class KerberosArtifactSynchronizerTest : public testing::Test {
 public:
  KerberosArtifactSynchronizerTest() {
    auto fake_ptr = std::make_unique<FakeKerberosArtifactClient>();
    fake_artifact_client_ = fake_ptr.get();

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    krb5_conf_path_ = CreateKrb5ConfPath(temp_dir_.GetPath());
    krb5_ccache_path_ = CreateKrb5CCachePath(temp_dir_.GetPath());

    synchronizer_ = std::make_unique<KerberosArtifactSynchronizer>(
        krb5_conf_path_, krb5_ccache_path_, std::move(fake_ptr));
  }

  ~KerberosArtifactSynchronizerTest() override = default;

 protected:
  base::ScopedTempDir temp_dir_;
  std::string krb5_conf_path_;
  std::string krb5_ccache_path_;
  FakeKerberosArtifactClient* fake_artifact_client_;
  std::unique_ptr<KerberosArtifactSynchronizer> synchronizer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KerberosArtifactSynchronizerTest);
};

// SetupKerberos makes a call to GetUserKerberosFiles.
TEST_F(KerberosArtifactSynchronizerTest, SetupKerberosCallsGetFiles) {
  const std::string user = "test user";
  const std::string krb5cc = "test creds";
  const std::string krb5conf = "test conf";

  authpolicy::KerberosFiles kerberos_files =
      CreateKerberosFilesProto(krb5cc, krb5conf);
  fake_artifact_client_->AddKerberosFiles(user, kerberos_files);

  synchronizer_->SetupKerberos(user, base::Bind(&ExpectSetupSuccess));
  EXPECT_EQ(1, fake_artifact_client_->GetFilesMethodCallCount());
}

// SetupKerberos writes the files to the correct location.
TEST_F(KerberosArtifactSynchronizerTest, KerberosFilesWriteToCorrectLocation) {
  const std::string user = "test user";
  const std::string krb5cc = "test creds";
  const std::string krb5conf = "test conf";

  authpolicy::KerberosFiles kerberos_files =
      CreateKerberosFilesProto(krb5cc, krb5conf);
  fake_artifact_client_->AddKerberosFiles(user, kerberos_files);
  synchronizer_->SetupKerberos(user, base::Bind(&ExpectSetupSuccess));

  ExpectFileEqual(krb5_conf_path_, krb5conf);
  ExpectFileEqual(krb5_ccache_path_, krb5cc);
}

// SetupKerberos connects to a signal.
TEST_F(KerberosArtifactSynchronizerTest, SetupKerberosConnectsToSignal) {
  const std::string user = "test user";
  const std::string krb5cc = "test creds";
  const std::string krb5conf = "test conf";

  authpolicy::KerberosFiles kerberos_files =
      CreateKerberosFilesProto(krb5cc, krb5conf);
  fake_artifact_client_->AddKerberosFiles(user, kerberos_files);

  synchronizer_->SetupKerberos(user, base::Bind(&ExpectSetupSuccess));
  EXPECT_TRUE(fake_artifact_client_->IsConnected());
}

// Synchronizer calls GetFiles an additional time when the signal fires.
TEST_F(KerberosArtifactSynchronizerTest, GetFilesRunsOnSignalFire) {
  const std::string user = "test user";
  const std::string krb5cc = "test creds";
  const std::string krb5conf = "test conf";

  authpolicy::KerberosFiles kerberos_files =
      CreateKerberosFilesProto(krb5cc, krb5conf);
  fake_artifact_client_->AddKerberosFiles(user, kerberos_files);
  synchronizer_->SetupKerberos(user, base::Bind(&ExpectSetupSuccess));
  int setup_callback_count = 0;
  synchronizer_->SetupKerberos(
      user, base::Bind(&IncrementInt, &setup_callback_count, true));

  EXPECT_EQ(1, fake_artifact_client_->GetFilesMethodCallCount());

  fake_artifact_client_->FireSignal();

  EXPECT_EQ(2, fake_artifact_client_->GetFilesMethodCallCount());
  EXPECT_EQ(1, setup_callback_count);
}

// Synchronizer calls GetFiles an additional time when the signal fires, but
// GetUserKerberosFiles() fails.
TEST_F(KerberosArtifactSynchronizerTest,
       GetFilesRunsOnSignalFireWithGetFilesFailure) {
  const std::string user = "test user";
  const std::string krb5cc = "test creds";
  const std::string krb5conf = "test conf";

  authpolicy::KerberosFiles kerberos_files =
      CreateKerberosFilesProto(krb5cc, krb5conf);
  fake_artifact_client_->AddKerberosFiles(user, kerberos_files);
  synchronizer_->SetupKerberos(user, base::Bind(&ExpectSetupSuccess));
  int setup_callback_count = 0;
  synchronizer_->SetupKerberos(
      user, base::Bind(&IncrementInt, &setup_callback_count, true));

  EXPECT_EQ(1, fake_artifact_client_->GetFilesMethodCallCount());

  fake_artifact_client_->ResetKerberosFiles();
  fake_artifact_client_->FireSignal();

  EXPECT_EQ(2, fake_artifact_client_->GetFilesMethodCallCount());
  EXPECT_EQ(1, setup_callback_count);
}

// Synchronizer overwrites the Kerberos files when the signal fires.
TEST_F(KerberosArtifactSynchronizerTest, GetFilesOverwritesOldFiles) {
  const std::string user = "test user";
  const std::string krb5cc = "test creds";
  const std::string krb5conf = "test conf";

  authpolicy::KerberosFiles kerberos_files =
      CreateKerberosFilesProto(krb5cc, krb5conf);
  fake_artifact_client_->AddKerberosFiles(user, kerberos_files);
  synchronizer_->SetupKerberos(user, base::Bind(&ExpectSetupSuccess));

  ExpectFileEqual(krb5_conf_path_, krb5conf);
  ExpectFileEqual(krb5_ccache_path_, krb5cc);

  const std::string new_krb5cc = "new test creds";
  const std::string new_krb5conf = "new test conf";

  authpolicy::KerberosFiles new_kerberos_files =
      CreateKerberosFilesProto(new_krb5cc, new_krb5conf);
  fake_artifact_client_->AddKerberosFiles(user, new_kerberos_files);
  fake_artifact_client_->FireSignal();

  ExpectFileNotEqual(krb5_conf_path_, krb5conf);
  ExpectFileNotEqual(krb5_ccache_path_, krb5cc);

  ExpectFileEqual(krb5_conf_path_, new_krb5conf);
  ExpectFileEqual(krb5_ccache_path_, new_krb5cc);
}

// SetupKerberos fails when the getting the user's kerberos files fails.
TEST_F(KerberosArtifactSynchronizerTest, SetupKerberosFailsKerberosFilesEmpty) {
  const std::string user = "test user";

  authpolicy::KerberosFiles kerberos_files;
  fake_artifact_client_->AddKerberosFiles(user, kerberos_files);

  synchronizer_->SetupKerberos(user, base::Bind(&ExpectSetupFailure));
}

// SetupKerberos is called twice for the same user.
TEST_F(KerberosArtifactSynchronizerTest, SetupKerberosCalledTwice) {
  const std::string user = "test user";
  const std::string krb5cc = "test creds";
  const std::string krb5conf = "test conf";

  authpolicy::KerberosFiles kerberos_files =
      CreateKerberosFilesProto(krb5cc, krb5conf);
  fake_artifact_client_->AddKerberosFiles(user, kerberos_files);

  synchronizer_->SetupKerberos(user, base::Bind(&ExpectSetupSuccess));
  synchronizer_->SetupKerberos(user, base::Bind(&ExpectSetupSuccess));
  EXPECT_EQ(1, fake_artifact_client_->GetFilesMethodCallCount());
}

// SetupKerberos is called twice for different users.
TEST_F(KerberosArtifactSynchronizerTest,
       SetupKerberosCalledTwiceDifferentUsers) {
  const std::string user = "test user";
  const std::string user2 = "test user 2";
  const std::string krb5cc = "test creds";
  const std::string krb5conf = "test conf";

  authpolicy::KerberosFiles kerberos_files =
      CreateKerberosFilesProto(krb5cc, krb5conf);
  fake_artifact_client_->AddKerberosFiles(user, kerberos_files);

  synchronizer_->SetupKerberos(user, base::Bind(&ExpectSetupSuccess));
  synchronizer_->SetupKerberos(user2, base::Bind(&ExpectSetupFailure));
  EXPECT_EQ(1, fake_artifact_client_->GetFilesMethodCallCount());
}

}  // namespace smbprovider
