// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "chromeos/process_mock.h"
#include "chromeos/syslog_logging.h"
#include "chromeos/test_helpers.h"
#include "gflags/gflags.h"
#include "gtest/gtest.h"
#include "vpn-manager/ipsec_manager.h"

using ::chromeos::FindLog;
using ::chromeos::ProcessMock;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

const int kMockFd = 123;

const int kMockStarterPid = 10001;

DECLARE_int32(ipsec_timeout);

class IpsecManagerTest : public ::testing::Test {
 public:
  void SetUp() {
    FilePath cwd;
    file_util::GetCurrentDirectory(&cwd);
    test_path_ = cwd.Append("test");
    file_util::Delete(test_path_, true);
    file_util::CreateDirectory(test_path_);
    stateful_container_ = test_path_.Append("etc");
    file_util::CreateDirectory(stateful_container_);
    remote_address_text_ = "1.2.3.4";
    ASSERT_TRUE(ServiceManager::ConvertIPStringToSockAddr(remote_address_text_,
                                                          &remote_address_));
    ServiceManager::temp_path_ = new FilePath(test_path_);
    psk_file_ = test_path_.Append("psk").value();
    server_ca_file_ = test_path_.Append("server.ca").value();
    WriteFile(server_ca_file_, "contents not used for testing");
    server_id_ = "CN=vpnserver";
    client_cert_tpm_slot_ = "0";
    client_cert_tpm_id_ = "0a";
    tpm_user_pin_ = "123456";
    ipsec_run_path_ = test_path_.Append("run").value();
    ipsec_up_file_ = FilePath(ipsec_run_path_).Append("up").value();
    WriteFile(psk_file_, "secret");
    file_util::CopyFile(cwd.Append("testdata/cacert.der"),
                        FilePath(server_ca_file_));
    chromeos::ClearLog();
    starter_ = new ProcessMock;
    ipsec_.starter_.reset(starter_);
    ipsec_.stateful_container_ = stateful_container_.value();
    ipsec_.ipsec_group_ = getgid();
    ipsec_.ipsec_run_path_ = ipsec_run_path_;
    ipsec_.ipsec_up_file_ = ipsec_up_file_;
    ipsec_.force_local_address_ = "5.6.7.8";
  }

  void SetStartStarterExpectations(bool already_running);

 protected:
  void WriteFile(const std::string& file_path, const char* contents) {
    if (file_util::WriteFile(FilePath(file_path), contents,
                             strlen(contents)) < 0) {
      LOG(ERROR) << "Unable to create " << file_path;
    }
  }

  void DoInitialize(int ike_version, bool use_psk);

  IpsecManager ipsec_;
  FilePath stateful_container_;
  FilePath test_path_;
  std::string remote_address_text_;
  struct sockaddr remote_address_;
  std::string psk_file_;
  std::string server_ca_file_;
  std::string server_id_;
  std::string client_cert_tpm_slot_;
  std::string client_cert_tpm_id_;
  std::string tpm_user_pin_;
  std::string ipsec_run_path_;
  std::string ipsec_up_file_;
  ProcessMock* starter_;
};

void IpsecManagerTest::DoInitialize(int ike_version, bool use_psk) {
  if (use_psk) {
    ASSERT_TRUE(ipsec_.Initialize(ike_version, remote_address_, psk_file_,
                                  "", "", "", "", ""));
  } else {
    ASSERT_TRUE(ipsec_.Initialize(ike_version, remote_address_, "",
                                  server_ca_file_, server_id_,
                                  client_cert_tpm_slot_, client_cert_tpm_id_,
                                  tpm_user_pin_));
  }
}

void IpsecManagerTest::SetStartStarterExpectations(bool already_running) {
  if (already_running) {
    ipsec_.starter_pid_file_ = test_path_.Append("starter_pid").value();
    // File must exist.
    file_util::WriteFile(FilePath(ipsec_.starter_pid_file_), "", 0);
    // Test that it attempts to kill the running starter.
    EXPECT_CALL(*starter_, ResetPidByFile(ipsec_.starter_pid_file_)).
        WillOnce(Return(true));
    EXPECT_CALL(*starter_, pid()).WillOnce(Return(1));
    EXPECT_CALL(*starter_, Reset(0));
  }

  EXPECT_CALL(*starter_, AddArg(IPSEC_STARTER));
  EXPECT_CALL(*starter_, AddArg("--nofork"));
  EXPECT_CALL(*starter_, RedirectUsingPipe(STDERR_FILENO, false));
  EXPECT_CALL(*starter_, Start()).WillOnce(Return(true));
  EXPECT_CALL(*starter_, GetPipe(STDERR_FILENO)).WillOnce(Return(kMockFd));
  EXPECT_CALL(*starter_, pid()).WillOnce(Return(kMockStarterPid));
}

TEST_F(IpsecManagerTest, InitializeNoAuth) {
  EXPECT_FALSE(ipsec_.Initialize(1, remote_address_, "", "", "", "", "", ""));
  EXPECT_TRUE(FindLog("Must specify either PSK or certificates"));
}

TEST_F(IpsecManagerTest, InitializeNotBoth) {
  EXPECT_FALSE(ipsec_.Initialize(1, remote_address_,
                                 psk_file_,
                                 server_ca_file_,
                                 server_id_,
                                 client_cert_tpm_slot_,
                                 client_cert_tpm_id_,
                                 tpm_user_pin_));
  EXPECT_TRUE(FindLog("Specified both PSK and certificates"));
}

TEST_F(IpsecManagerTest, InitializeUnsupportedVersion) {
  EXPECT_FALSE(ipsec_.Initialize(3, remote_address_, psk_file_, "", "", "",
                                 "", ""));
  EXPECT_TRUE(FindLog("Unsupported IKE version"));
}

TEST_F(IpsecManagerTest, InitializeIkev2WithCertificates) {
  EXPECT_FALSE(ipsec_.Initialize(2, remote_address_, "",
                                 server_ca_file_,
                                 server_id_,
                                 client_cert_tpm_slot_,
                                 client_cert_tpm_id_,
                                 tpm_user_pin_));
  EXPECT_TRUE(FindLog("Only IKE version 1 is supported with certificates"));
}

TEST_F(IpsecManagerTest, CreateIpsecRunDirectory) {
  EXPECT_TRUE(ipsec_.CreateIpsecRunDirectory());
  struct stat stat_buffer;
  ASSERT_EQ(0, stat(ipsec_run_path_.c_str(), &stat_buffer));
  EXPECT_EQ(0, stat_buffer.st_mode & (S_IWOTH | S_IXOTH | S_IROTH));
}

TEST_F(IpsecManagerTest, PollWaitIfNotUpYet) {
  ipsec_.start_ticks_ = base::TimeTicks::Now();
  EXPECT_EQ(1000, ipsec_.Poll());
}

TEST_F(IpsecManagerTest, PollTimeoutWaiting) {
  ipsec_.start_ticks_ = base::TimeTicks::Now() -
      base::TimeDelta::FromSeconds(FLAGS_ipsec_timeout + 1);
  EXPECT_EQ(1000, ipsec_.Poll());
  EXPECT_TRUE(FindLog("IPsec connection timed out"));
  EXPECT_TRUE(ipsec_.was_stopped());
}

TEST_F(IpsecManagerTest, PollTransitionToUp) {
  ipsec_.start_ticks_ = base::TimeTicks::Now();
  EXPECT_TRUE(ipsec_.CreateIpsecRunDirectory());
  EXPECT_TRUE(file_util::PathExists(FilePath(ipsec_run_path_)));
  WriteFile(ipsec_up_file_, "");
  EXPECT_FALSE(ipsec_.is_running());
  EXPECT_EQ(-1, ipsec_.Poll());
  EXPECT_TRUE(FindLog("IPsec connection now up"));
  EXPECT_TRUE(ipsec_.is_running());
}

TEST_F(IpsecManagerTest, PollNothingIfRunning) {
  ipsec_.is_running_ = true;
  EXPECT_EQ(-1, ipsec_.Poll());
}

TEST_F(IpsecManagerTest, FormatSecretsNoSlot) {
  client_cert_tpm_slot_ = "";
  DoInitialize(1, false);
  std::string formatted;
  EXPECT_TRUE(ipsec_.FormatSecrets(&formatted));
  EXPECT_EQ("5.6.7.8 1.2.3.4 : PIN \%smartcard0:0a \"123456\"\n", formatted);
}

TEST_F(IpsecManagerTest, FormatSecretsNonZeroSlot) {
  client_cert_tpm_slot_ = "1";
  DoInitialize(1, false);
  std::string formatted;
  EXPECT_TRUE(ipsec_.FormatSecrets(&formatted));
  EXPECT_EQ("5.6.7.8 1.2.3.4 : PIN \%smartcard1:0a \"123456\"\n", formatted);
}

class IpsecManagerTestIkeV1Psk : public IpsecManagerTest {
 public:
  void SetUp() {
    IpsecManagerTest::SetUp();
    DoInitialize(1, true);
  }

 protected:
  std::string GetExpectedStarter(bool debug);
};

TEST_F(IpsecManagerTestIkeV1Psk, Initialize) {
}

TEST_F(IpsecManagerTestIkeV1Psk, FormatSecrets) {
  FilePath input(psk_file_);
  const char psk[] = "pAssword\n";
  file_util::WriteFile(input, psk, strlen(psk));
  std::string formatted;
  EXPECT_TRUE(ipsec_.FormatSecrets(&formatted));
  EXPECT_EQ("5.6.7.8 1.2.3.4 : PSK \"pAssword\"\n", formatted);
}

TEST_F(IpsecManagerTestIkeV1Psk, StartStarterNotYetRunning) {
  InSequence unused;
  SetStartStarterExpectations(false);
  EXPECT_TRUE(ipsec_.StartStarter());
  EXPECT_EQ(kMockFd, ipsec_.output_fd());
  EXPECT_EQ("ipsec[10001]: ", ipsec_.ipsec_prefix_);
}

TEST_F(IpsecManagerTestIkeV1Psk, StartStarterAlreadyRunning) {
  InSequence unused;
  SetStartStarterExpectations(true);
  EXPECT_TRUE(ipsec_.StartStarter());
  EXPECT_EQ(kMockFd, ipsec_.output_fd());
  EXPECT_EQ("ipsec[10001]: ", ipsec_.ipsec_prefix_);
}

std::string IpsecManagerTestIkeV1Psk::GetExpectedStarter(bool debug) {
  std::string expected(
      "config setup\n"
      "\tcharonstart=no\n");
  if (debug) {
    expected.append("\tplutodebug=\"all\"\n");
  }
  expected.append(
      "\tnat_traversal=yes\n"
      "\tpkcs11module=\"/usr/lib/opencryptoki/libopencryptoki.so.0\"\n"
      "conn managed\n"
      "\tike=\"3des-sha1-modp1024\"\n"
      "\tkeyexchange=\"ikev1\"\n"
      "\tauthby=\"psk\"\n"
      "\tpfs=no\n"
      "\trekey=no\n"
      "\tleft=\"\%defaultroute\"\n"
      "\tleftprotoport=\"17/1701\"\n"
      "\tleftupdown=\"/usr/libexec/l2tpipsec_vpn/pluto_updown\"\n"
      "\tright=\"1.2.3.4\"\n"
      "\trightprotoport=\"17/1701\"\n"
      "\ttype=\"transport\"\n"
      "\tauto=\"start\"\n");
  return expected;
}

TEST_F(IpsecManagerTestIkeV1Psk, FormatStarterConfigFile) {
  EXPECT_EQ(GetExpectedStarter(false), ipsec_.FormatStarterConfigFile());
  ipsec_.set_debug(true);
  EXPECT_EQ(GetExpectedStarter(true), ipsec_.FormatStarterConfigFile());
}

TEST_F(IpsecManagerTestIkeV1Psk, Start) {
  InSequence unused;
  SetStartStarterExpectations(false);
  EXPECT_TRUE(ipsec_.Start());
  EXPECT_FALSE(ipsec_.start_ticks_.is_null());
}

TEST_F(IpsecManagerTestIkeV1Psk, WriteConfigFiles) {
  EXPECT_TRUE(ipsec_.WriteConfigFiles());
  std::string conf_contents;
  ASSERT_TRUE(file_util::ReadFileToString(
      stateful_container_.Append("ipsec.conf"), &conf_contents));
  EXPECT_EQ(GetExpectedStarter(false), conf_contents);
  ASSERT_TRUE(file_util::PathExists(stateful_container_.Append(
      "ipsec.secrets")));
}

class IpsecManagerTestIkeV1Certs : public IpsecManagerTest {
 public:
  void SetUp() {
    IpsecManagerTest::SetUp();
    DoInitialize(1, false);
  }

 protected:
  std::string GetExpectedStarter(bool debug);
};

TEST_F(IpsecManagerTestIkeV1Certs, Initialize) {
  DoInitialize(1, false);
}

TEST_F(IpsecManagerTestIkeV1Certs, FormatSecrets) {
  std::string formatted;
  EXPECT_TRUE(ipsec_.FormatSecrets(&formatted));
  EXPECT_EQ("5.6.7.8 1.2.3.4 : PIN \%smartcard0:0a \"123456\"\n", formatted);
}

std::string IpsecManagerTestIkeV1Certs::GetExpectedStarter(bool debug) {
  std::string expected(
      "config setup\n"
      "\tcharonstart=no\n");
  if (debug) {
    expected.append("\tplutodebug=\"all\"\n");
  }
  expected.append(
      "\tnat_traversal=yes\n"
      "\tpkcs11module=\"/usr/lib/opencryptoki/libopencryptoki.so.0\"\n"
      "conn managed\n"
      "\tike=\"3des-sha1-modp1024\"\n"
      "\tkeyexchange=\"ikev1\"\n"
      "\tpfs=no\n"
      "\trekey=no\n"
      "\tleft=\"\%defaultroute\"\n"
      "\tleftcert=\"\%smartcard0:0a\"\n"
      "\tleftprotoport=\"17/1701\"\n"
      "\tleftupdown=\"/usr/libexec/l2tpipsec_vpn/pluto_updown\"\n"
      "\tright=\"1.2.3.4\"\n"
      "\trightca=\"C=US, O=simonjam, CN=rootca\"\n"
      "\trightid=\"CN=vpnserver\"\n"
      "\trightprotoport=\"17/1701\"\n"
      "\ttype=\"transport\"\n"
      "\tauto=\"start\"\n");
  return expected;
}

TEST_F(IpsecManagerTestIkeV1Certs, FormatStarterConfigFile) {
  EXPECT_EQ(GetExpectedStarter(false), ipsec_.FormatStarterConfigFile());
  ipsec_.set_debug(true);
  EXPECT_EQ(GetExpectedStarter(true), ipsec_.FormatStarterConfigFile());
}

TEST_F(IpsecManagerTestIkeV1Certs, WriteConfigFiles) {
  EXPECT_TRUE(ipsec_.WriteConfigFiles());
  std::string conf_contents;
  ASSERT_TRUE(file_util::ReadFileToString(
      stateful_container_.Append("ipsec.conf"), &conf_contents));
  EXPECT_EQ(GetExpectedStarter(false), conf_contents);
  ASSERT_TRUE(file_util::PathExists(stateful_container_.Append(
      "ipsec.secrets")));
  ASSERT_TRUE(file_util::PathExists(stateful_container_.Append("cacert.der")));
}

int main(int argc, char** argv) {
  SetUpTests(&argc, argv, true);
  return RUN_ALL_TESTS();
}
