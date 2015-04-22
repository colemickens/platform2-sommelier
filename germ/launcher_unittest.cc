// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/launcher.h"

#include <sys/types.h>

#include <memory>

#include <base/at_exit.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chromeos/process_mock.h>
#include <soma/read_only_container_spec.h>

#include "germ/proto_bindings/soma_container_spec.pb.h"

using ::testing::_;
using ::testing::Return;

namespace {
const std::string kInitctlOutput =
    "germ_template (test) start/running, process 8117";
const pid_t kServicePid = 8117;
}

namespace germ {

class MockLauncher : public Launcher {
 public:
  MockLauncher() = default;
  ~MockLauncher() override = default;

  // This takes ownership of the pointer.
  void SetProcessInstance(chromeos::Process* instance) {
    instance_.reset(instance);
  }

 protected:
  std::string ReadFromStdout(chromeos::Process* process) override {
    return kInitctlOutput;
  }

  std::unique_ptr<chromeos::Process> GetProcessInstance() override {
    return std::move(instance_);
  }

 private:
  std::unique_ptr<chromeos::Process> instance_;
};

class LauncherTest : public ::testing::Test {
 public:
  LauncherTest() = default;
  ~LauncherTest() override = default;

  void SetUp() override {
    const char name[] = "yes";
    const char path[] = "/path/to/bundle";
    const char* command_line[2] = {"/usr/bin/yes", "no"};
    const uid_t uid = 1;
    const gid_t gid = 8;

    spec_.set_name(name);
    spec_.set_service_bundle_path(path);

    soma::ContainerSpec::Executable* executable = spec_.add_executables();
    executable->set_uid(uid);
    executable->set_gid(gid);
    for (const char* arg : command_line) {
      executable->add_command_line(arg);
    }

    ASSERT_TRUE(ro_spec_.Init(spec_));
  }

 protected:
  base::AtExitManager exit_manager_;
  soma::ContainerSpec spec_;
  soma::ReadOnlyContainerSpec ro_spec_;
  MockLauncher launcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherTest);
};

TEST_F(LauncherTest, LaunchDaemonizedSuccess) {
  chromeos::ProcessMock* process = new chromeos::ProcessMock();

  // This is here to prevent useless logging.
  EXPECT_CALL(*process, AddArg(_)).WillRepeatedly(Return());
  EXPECT_CALL(*process, RedirectUsingPipe(_, _)).WillRepeatedly(Return());

  EXPECT_CALL(*process, Start()).WillOnce(Return(true));
  EXPECT_CALL(*process, Wait()).WillOnce(Return(0 /* success */));
  launcher_.SetProcessInstance(process);
  pid_t pid;
  ASSERT_TRUE(launcher_.RunDaemonized(ro_spec_, &pid));
  // This pid is parsed from |kInitctlOutput|.
  ASSERT_EQ(kServicePid, pid);
}

TEST_F(LauncherTest, LaunchDaemonizedInitctlFailure) {
  chromeos::ProcessMock* process = new chromeos::ProcessMock();

  // This is here to prevent useless logging.
  EXPECT_CALL(*process, AddArg(_)).WillRepeatedly(Return());
  EXPECT_CALL(*process, RedirectUsingPipe(_, _)).WillRepeatedly(Return());

  EXPECT_CALL(*process, Start()).WillOnce(Return(true));
  EXPECT_CALL(*process, Wait()).WillOnce(Return(1 /* failure */));
  launcher_.SetProcessInstance(process);
  pid_t pid;
  ASSERT_FALSE(launcher_.RunDaemonized(ro_spec_, &pid));
  ASSERT_EQ(-1, pid);
}

TEST_F(LauncherTest, TerminateSuccess) {
  chromeos::ProcessMock* start_process = new chromeos::ProcessMock();

  // This is here to prevent useless logging.
  EXPECT_CALL(*start_process, AddArg(_)).WillRepeatedly(Return());
  EXPECT_CALL(*start_process, RedirectUsingPipe(_, _)).WillRepeatedly(Return());

  EXPECT_CALL(*start_process, Start()).WillOnce(Return(true));
  EXPECT_CALL(*start_process, Wait()).WillOnce(Return(0 /* success */));
  launcher_.SetProcessInstance(start_process);
  pid_t pid;
  ASSERT_TRUE(launcher_.RunDaemonized(ro_spec_, &pid));
  // This pid is parsed from the string in ReadFromStdout() above.
  ASSERT_EQ(kServicePid, pid);

  chromeos::ProcessMock* stop_process = new chromeos::ProcessMock();
  EXPECT_CALL(*stop_process, AddArg(_)).WillRepeatedly(Return());
  EXPECT_CALL(*stop_process, RedirectUsingPipe(_, _)).WillRepeatedly(Return());
  EXPECT_CALL(*stop_process, Run()).WillOnce(Return(0 /* success */));
  launcher_.SetProcessInstance(stop_process);
  ASSERT_TRUE(launcher_.Terminate(kServicePid));
}

TEST_F(LauncherTest, TerminateInitctlFailure) {
  chromeos::ProcessMock* start_process = new chromeos::ProcessMock();

  // This is here to prevent useless logging.
  EXPECT_CALL(*start_process, AddArg(_)).WillRepeatedly(Return());
  EXPECT_CALL(*start_process, RedirectUsingPipe(_, _)).WillRepeatedly(Return());

  EXPECT_CALL(*start_process, Start()).WillOnce(Return(true));
  EXPECT_CALL(*start_process, Wait()).WillOnce(Return(0 /* success */));
  launcher_.SetProcessInstance(start_process);
  pid_t pid;
  ASSERT_TRUE(launcher_.RunDaemonized(ro_spec_, &pid));
  // This pid is parsed from the string in ReadFromStdout() above.
  ASSERT_EQ(kServicePid, pid);

  chromeos::ProcessMock* stop_process = new chromeos::ProcessMock();
  EXPECT_CALL(*stop_process, AddArg(_)).WillRepeatedly(Return());
  EXPECT_CALL(*stop_process, RedirectUsingPipe(_, _)).WillRepeatedly(Return());
  EXPECT_CALL(*stop_process, Run()).WillOnce(Return(1 /* success */));
  launcher_.SetProcessInstance(stop_process);
  ASSERT_FALSE(launcher_.Terminate(kServicePid));
}

TEST_F(LauncherTest, TerminateInvalidPidFailure) {
  chromeos::ProcessMock* process = new chromeos::ProcessMock();
  // This is here to prevent useless logging.
  EXPECT_CALL(*process, AddArg(_)).WillRepeatedly(Return());
  launcher_.SetProcessInstance(process);
  // Invalid pid.
  ASSERT_FALSE(launcher_.Terminate(-1));
  // Unknown pid.
  ASSERT_FALSE(launcher_.Terminate(kServicePid + 1));
}

}  // namespace germ
