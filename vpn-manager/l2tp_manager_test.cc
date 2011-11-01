// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/process_mock.h"
#include "chromeos/syslog_logging.h"
#include "chromeos/test_helpers.h"
#include "gflags/gflags.h"
#include "gtest/gtest.h"

#include "vpn-manager/l2tp_manager.h"

DECLARE_bool(defaultroute);
DECLARE_bool(systemconfig);
DECLARE_bool(usepeerdns);
DECLARE_string(password);
DECLARE_int32(ppp_setup_timeout);
DECLARE_string(pppd_plugin);
DECLARE_string(user);

using ::chromeos::FindLog;
using ::chromeos::ProcessMock;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

class L2tpManagerTest : public ::testing::Test {
 public:
  void SetUp() {
    test_path_ = FilePath("test");
    ServiceManager::temp_path_ = new FilePath(test_path_);
    file_util::Delete(test_path_, true);
    file_util::CreateDirectory(test_path_);
    remote_address_text_ = "1.2.3.4";
    ServiceManager::ConvertIPStringToSockAddr(remote_address_text_,
                                              &remote_address_);
    control_path_ = test_path_.Append("control");
    pppd_config_path_ = test_path_.Append("pppd.config");
    ppp_interface_path_ = test_path_.Append("ppp0");
    l2tpd_ = new ProcessMock;
    l2tp_.l2tpd_.reset(l2tpd_);
    l2tp_.l2tpd_control_path_ = control_path_;
    l2tp_.ppp_interface_path_ = ppp_interface_path_;
    FLAGS_pppd_plugin = "";
    FLAGS_user = "me";
    EXPECT_TRUE(l2tp_.Initialize(remote_address_));
  }

 protected:
  std::string GetExpectedConfig(std::string remote_address_text,
                                bool debug);

  std::string remote_address_text_;
  struct sockaddr remote_address_;
  FilePath test_path_;
  FilePath control_path_;
  FilePath pppd_config_path_;
  FilePath ppp_interface_path_;
  L2tpManager l2tp_;
  ProcessMock* l2tpd_;
};

std::string L2tpManagerTest::GetExpectedConfig(std::string remote_address_text,
                                               bool debug) {
  return StringPrintf(
      "[lac managed]\n"
      "lns = %s\n"
      "require chap = yes\n"
      "refuse pap = no\n"
      "require authentication = yes\n"
      "name = me\n"
      "%s"
      "pppoptfile = test/pppd.config\n"
      "length bit = yes\n", remote_address_text.c_str(),
      debug ? "ppp debug = yes\n" : "");
}

TEST_F(L2tpManagerTest, FormatL2tpdConfiguration) {
  EXPECT_EQ(GetExpectedConfig(remote_address_text_, false),
            l2tp_.FormatL2tpdConfiguration(pppd_config_path_.value()));
  l2tp_.set_debug(true);
  EXPECT_EQ(GetExpectedConfig(remote_address_text_, true),
            l2tp_.FormatL2tpdConfiguration(pppd_config_path_.value()));
}

TEST_F(L2tpManagerTest, FormatPppdConfiguration) {
  const char kBaseExpected[] =
      "ipcp-accept-local\n"
      "ipcp-accept-remote\n"
      "refuse-eap\n"
      "noccp\n"
      "noauth\n"
      "crtscts\n"
      "idle 1800\n"
      "mtu 1410\n"
      "mru 1410\n"
      "lock\n"
      "connect-delay 5000\n";
  FLAGS_defaultroute = false;
  FLAGS_usepeerdns = false;
  std::string expected(kBaseExpected);
  expected.append("nodefaultroute\n");
  EXPECT_EQ(expected, l2tp_.FormatPppdConfiguration());
  expected = kBaseExpected;
  FLAGS_defaultroute = true;
  expected.append("defaultroute\n");
  l2tp_.ppp_output_fd_ = 4;
  l2tp_.ppp_output_path_ = FilePath("/tmp/pppd.log");
  expected.append("logfile /tmp/pppd.log\n");
  EXPECT_EQ(expected, l2tp_.FormatPppdConfiguration());
  FLAGS_usepeerdns = true;
  expected.append("usepeerdns\n");
  EXPECT_EQ(expected, l2tp_.FormatPppdConfiguration());
  FLAGS_systemconfig = false;
  expected.append("nosystemconfig\n");
  EXPECT_EQ(expected, l2tp_.FormatPppdConfiguration());
  FLAGS_pppd_plugin = "myplugin";
  expected.append("plugin myplugin\n");
  EXPECT_EQ(expected, l2tp_.FormatPppdConfiguration());
  l2tp_.set_debug(true);
  expected.append("debug\n");
  EXPECT_EQ(expected, l2tp_.FormatPppdConfiguration());
}

TEST_F(L2tpManagerTest, Initiate) {
  FLAGS_user = "me";
  FLAGS_password = "password";
  EXPECT_TRUE(l2tp_.Initiate());
  ExpectFileEquals("c managed me password\n", control_path_.value().c_str());
  FLAGS_pppd_plugin = "set";
  EXPECT_TRUE(l2tp_.Initiate());
  ExpectFileEquals("c managed\n", control_path_.value().c_str());
}

TEST_F(L2tpManagerTest, Terminate) {
  EXPECT_TRUE(l2tp_.Terminate());
  ExpectFileEquals("d managed\n", control_path_.value().c_str());
}

TEST_F(L2tpManagerTest, Start) {
  InSequence unused;
  const int kMockFd = 567;

  EXPECT_CALL(*l2tpd_, Reset(0));
  EXPECT_CALL(*l2tpd_, AddArg(L2TPD));
  EXPECT_CALL(*l2tpd_, AddArg("-c"));
  EXPECT_CALL(*l2tpd_, AddArg("test/l2tpd.conf"));
  EXPECT_CALL(*l2tpd_, AddArg("-C"));
  EXPECT_CALL(*l2tpd_, AddArg("test/l2tpd.control"));
  EXPECT_CALL(*l2tpd_, AddArg("-D"));
  EXPECT_CALL(*l2tpd_, RedirectUsingPipe(STDERR_FILENO, false));
  EXPECT_CALL(*l2tpd_, Start());
  EXPECT_CALL(*l2tpd_, GetPipe(STDERR_FILENO)).WillOnce(Return(kMockFd));

  EXPECT_TRUE(l2tp_.Start());
  EXPECT_EQ(kMockFd, l2tp_.output_fd());
  EXPECT_FALSE(l2tp_.start_ticks_.is_null());
  EXPECT_FALSE(l2tp_.was_initiated_);
}

TEST_F(L2tpManagerTest, PollWaitIfNotUpYet) {
  l2tp_.start_ticks_ = base::TimeTicks::Now();
  EXPECT_EQ(1000, l2tp_.Poll());
}

TEST_F(L2tpManagerTest, PollTimeoutWaitingForControl) {
  l2tp_.start_ticks_ = base::TimeTicks::Now() -
      base::TimeDelta::FromSeconds(FLAGS_ppp_setup_timeout + 1);
  EXPECT_EQ(1000, l2tp_.Poll());
  EXPECT_TRUE(FindLog("PPP setup timed out"));
  EXPECT_TRUE(l2tp_.was_stopped());
  EXPECT_FALSE(file_util::PathExists(control_path_));
}

TEST_F(L2tpManagerTest, PollTimeoutWaitingForUp) {
  l2tp_.start_ticks_ = base::TimeTicks::Now() -
      base::TimeDelta::FromSeconds(FLAGS_ppp_setup_timeout + 1);
  l2tp_.was_initiated_ = true;
  EXPECT_EQ(1000, l2tp_.Poll());
  EXPECT_TRUE(FindLog("PPP setup timed out"));
  EXPECT_TRUE(l2tp_.was_stopped());
  ExpectFileEquals("d managed\n", control_path_.value().c_str());
}

TEST_F(L2tpManagerTest, PollInitiateConnection) {
  l2tp_.start_ticks_ = base::TimeTicks::Now();
  file_util::WriteFile(control_path_, "", 0);
  EXPECT_FALSE(l2tp_.was_initiated_);
  FLAGS_pppd_plugin = "set";
  EXPECT_EQ(1000, l2tp_.Poll());
  EXPECT_TRUE(l2tp_.was_initiated_);
  ExpectFileEquals("c managed\n", control_path_.value().c_str());
}

TEST_F(L2tpManagerTest, PollTransitionToUp) {
  l2tp_.start_ticks_ = base::TimeTicks::Now();
  l2tp_.was_initiated_ = true;
  EXPECT_FALSE(l2tp_.is_running());
  file_util::WriteFile(ppp_interface_path_, "", 0);
  EXPECT_EQ(-1, l2tp_.Poll());
  EXPECT_TRUE(FindLog("L2TP connection now up"));
  EXPECT_TRUE(l2tp_.is_running());
}

TEST_F(L2tpManagerTest, PollNothingIfRunning) {
  l2tp_.is_running_ = true;
  EXPECT_EQ(-1, l2tp_.Poll());
}

int main(int argc, char** argv) {
  SetUpTests(&argc, argv, true);
  return RUN_ALL_TESTS();
}
