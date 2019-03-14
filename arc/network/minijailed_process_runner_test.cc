// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/minijailed_process_runner.h"

#include <linux/capability.h>

#include <brillo/minijail/mock_minijail.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::Return;
using testing::SetArgPointee;
using testing::StrEq;

namespace arc_networkd {

class MinijailProcessRunnerTest : public testing::Test {
 protected:
  MinijailProcessRunnerTest() : runner_(&mj_) {}

  void SetUp() override {
    ON_CALL(mj_, DropRoot(_, _, _)).WillByDefault(Return(true));
    ON_CALL(mj_, RunSyncAndDestroy(_, _, _))
        .WillByDefault(DoAll(SetArgPointee<2>(0), Return(true)));
  }

  brillo::MockMinijail mj_;
  MinijailedProcessRunner runner_;
};

// Special matcher needed for vector<char*> type.
// Lifted from shill/process_manager_test.cc
MATCHER_P2(IsProcessArgs, program, args, "") {
  if (std::string(arg[0]) != program) {
    return false;
  }
  int index = 1;
  for (const auto& option : args) {
    if (std::string(arg[index++]) != option) {
      return false;
    }
  }
  return arg[index] == nullptr;
}

TEST_F(MinijailProcessRunnerTest, Run) {
  uint64_t caps = CAP_TO_MASK(CAP_NET_ADMIN) | CAP_TO_MASK(CAP_NET_RAW);
  const std::vector<std::string> args = {"arg1", "arg2"};

  EXPECT_CALL(mj_, New());
  EXPECT_CALL(mj_, DropRoot(_, StrEq("nobody"), StrEq("nobody")));
  EXPECT_CALL(mj_, UseCapabilities(_, Eq(caps)));
  EXPECT_CALL(mj_, RunSyncAndDestroy(_, IsProcessArgs("cmd", args), _));
  runner_.Run({"cmd", "arg1", "arg2"});
}

TEST_F(MinijailProcessRunnerTest, AddInterfaceToContainer) {
  const std::vector<std::string> args_ip = {
      "-t", "12345", "-n", "--", "/bin/ip", "link", "set", "foo", "name", "bar",
  };
  const std::vector<std::string> args_ifconfig = {
      "-t",  "12345",   "-n",      "--",           "/bin/ifconfig",
      "bar", "1.1.1.1", "netmask", "255.255.255.0"};
  EXPECT_CALL(mj_, New()).Times(2);
  EXPECT_CALL(mj_, DropRoot(_, _, _)).Times(0);
  EXPECT_CALL(
      mj_, RunSyncAndDestroy(_, IsProcessArgs("/usr/bin/nsenter", args_ip), _));
  EXPECT_CALL(mj_, RunSyncAndDestroy(
                       _, IsProcessArgs("/usr/bin/nsenter", args_ifconfig), _));
  runner_.AddInterfaceToContainer("foo", "bar", "1.1.1.1", "255.255.255.0",
                                  true, "12345");
}

TEST_F(MinijailProcessRunnerTest, AddInterfaceToContainerNoMulticast) {
  const std::vector<std::string> args_ip = {
      "-t", "12345", "-n", "--", "/bin/ip", "link", "set", "foo", "name", "bar",
  };
  const std::vector<std::string> args_ifconfig = {
      "-t",        "12345",         "-n",
      "--",        "/bin/ifconfig", "bar",
      "1.1.1.1",   "netmask",       "255.255.255.0",
      "-multicast"};
  EXPECT_CALL(mj_, New()).Times(2);
  EXPECT_CALL(mj_, DropRoot(_, _, _)).Times(0);
  EXPECT_CALL(
      mj_, RunSyncAndDestroy(_, IsProcessArgs("/usr/bin/nsenter", args_ip), _));
  EXPECT_CALL(mj_, RunSyncAndDestroy(
                       _, IsProcessArgs("/usr/bin/nsenter", args_ifconfig), _));
  runner_.AddInterfaceToContainer("foo", "bar", "1.1.1.1", "255.255.255.0",
                                  false, "12345");
}

TEST_F(MinijailProcessRunnerTest, WriteSentinelToContainer) {
  const std::vector<std::string> args = {
      "-t",
      "12345",
      "--mount",
      "--pid",
      "--",
      "/system/bin/touch",
      "/dev/.arc_network_ready",
  };
  EXPECT_CALL(mj_, New());
  EXPECT_CALL(mj_, DropRoot(_, _, _)).Times(0);
  EXPECT_CALL(mj_,
              RunSyncAndDestroy(_, IsProcessArgs("/usr/bin/nsenter", args), _));
  runner_.WriteSentinelToContainer("12345");
}

TEST_F(MinijailProcessRunnerTest, ModprobeAll) {
  uint64_t caps = CAP_TO_MASK(CAP_SYS_MODULE);

  const std::vector<std::string> args = {"-a", "module1", "module2"};
  EXPECT_CALL(mj_, New());
  EXPECT_CALL(mj_, DropRoot(_, StrEq("nobody"), StrEq("nobody")));
  EXPECT_CALL(mj_, UseCapabilities(_, Eq(caps)));
  EXPECT_CALL(mj_,
              RunSyncAndDestroy(_, IsProcessArgs("/sbin/modprobe", args), _));
  runner_.ModprobeAll({"module1", "module2"});
}

}  // namespace arc_networkd
