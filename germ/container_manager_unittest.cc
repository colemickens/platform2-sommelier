// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/container_manager.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <string>

#include <base/at_exit.h>
#include <base/run_loop.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "germ/container.h"
#include "germ/mock_germ_zygote.h"
#include "germ/test_util.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace germ {
namespace {

// Basic test of starting/terminating a container.
TEST(ContainerManagerTest, StartAndTerminate) {
  base::AtExitManager exit_manager;
  base::MessageLoopForIO message_loop;
  base::RunLoop run_loop;

  MockGermZygote mock_zygote;
  ContainerManager container_manager(&mock_zygote);

  const char kContainerName[] = "test_container";
  const pid_t kContainerPid = 1234;

  soma::SandboxSpec spec = MakeSpecForTest(kContainerName);
  spec.set_shutdown_timeout_ms(50);

  {
    InSequence seq;
    EXPECT_CALL(mock_zygote, StartContainer(EqualsSpec(spec), _))
        .WillOnce(DoAll(SetArgPointee<1>(kContainerPid), Return(true)));
    EXPECT_CALL(mock_zygote, Kill(kContainerPid, SIGTERM))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_zygote, Kill(kContainerPid, SIGKILL))
        .WillOnce(DoAll(PostTask(run_loop.QuitClosure()), Return(true)));
  }

  // Start the container.
  EXPECT_TRUE(container_manager.StartContainer(spec));
  scoped_refptr<Container> container = container_manager.Lookup(kContainerName);
  ASSERT_TRUE(container != nullptr);
  EXPECT_EQ(kContainerName, container->name());
  EXPECT_EQ(Container::State::RUNNING, container->state());
  EXPECT_EQ(Container::State::RUNNING, container->desired_state());
  EXPECT_EQ(kContainerPid, container->init_pid());

  // Terminate the container.
  EXPECT_TRUE(container_manager.TerminateContainer(kContainerName));
  EXPECT_EQ(Container::State::STOPPED, container->desired_state());

  // TerminateContainer should have queued a task on the message loop to send
  // SIGKILL to the container, so run it.
  run_loop.Run();

  EXPECT_EQ(Container::State::DYING, container->state());

  // Notify container_manager that the container process has been reaped.
  siginfo_t info = {};
  info.si_pid = kContainerPid;
  info.si_signo = SIGCHLD;
  info.si_status = SIGKILL;
  info.si_code = CLD_KILLED;

  container_manager.OnReap(info);

  // The container should no longer be managed by ContainerManager.
  EXPECT_TRUE(container_manager.Lookup(kContainerName) == nullptr);
}

// Test that containers are restarted if they die on their own.
TEST(ContainerManagerTest, RestartContainer) {
  base::AtExitManager exit_manager;
  base::MessageLoopForIO message_loop;

  MockGermZygote mock_zygote;
  ContainerManager container_manager(&mock_zygote);

  const char kContainerName[] = "test_container";
  const pid_t kContainerPid = 1234;
  const pid_t kRestartedContainerPid = 5678;

  soma::SandboxSpec spec = MakeSpecForTest(kContainerName);
  spec.set_shutdown_timeout_ms(50);

  EXPECT_CALL(mock_zygote, StartContainer(EqualsSpec(spec), _))
      .Times(2)
      .WillOnce(DoAll(SetArgPointee<1>(kContainerPid), Return(true)))
      .WillOnce(DoAll(SetArgPointee<1>(kRestartedContainerPid), Return(true)));

  // Start the container.
  EXPECT_TRUE(container_manager.StartContainer(spec));
  scoped_refptr<Container> container = container_manager.Lookup(kContainerName);
  ASSERT_TRUE(container != nullptr);
  EXPECT_EQ(kContainerName, container->name());
  EXPECT_EQ(Container::State::RUNNING, container->state());
  EXPECT_EQ(Container::State::RUNNING, container->desired_state());
  EXPECT_EQ(kContainerPid, container->init_pid());

  // Notify container_manager that the container exited.
  siginfo_t info = {};
  info.si_pid = kContainerPid;
  info.si_signo = SIGCHLD;
  info.si_status = 0;
  info.si_code = CLD_EXITED;

  container_manager.OnReap(info);

  // ContainerManager should have restarted the container.
  container = container_manager.Lookup(kContainerName);
  ASSERT_TRUE(container != nullptr);
  EXPECT_EQ(kContainerName, container->name());
  EXPECT_EQ(Container::State::RUNNING, container->state());
  EXPECT_EQ(Container::State::RUNNING, container->desired_state());
  EXPECT_EQ(kRestartedContainerPid, container->init_pid());
}

// StartContainer on an already running container should restart the container.
TEST(ContainerManagerTest, StartContainerAlreadyRunning) {
  base::AtExitManager exit_manager;
  base::MessageLoopForIO message_loop;
  base::RunLoop run_loop;

  MockGermZygote mock_zygote;
  ContainerManager container_manager(&mock_zygote);

  const char kContainerName[] = "test_container";
  const pid_t kContainerPid = 1234;
  const pid_t kRestartedContainerPid = 5678;

  soma::SandboxSpec spec = MakeSpecForTest(kContainerName);
  spec.set_shutdown_timeout_ms(50);

  {
    InSequence seq;
    EXPECT_CALL(mock_zygote, StartContainer(EqualsSpec(spec), _))
        .WillOnce(DoAll(SetArgPointee<1>(kContainerPid), Return(true)));
    EXPECT_CALL(mock_zygote, Kill(kContainerPid, SIGTERM))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_zygote, Kill(kContainerPid, SIGKILL))
        .WillOnce(DoAll(PostTask(run_loop.QuitClosure()), Return(true)));
    EXPECT_CALL(mock_zygote, StartContainer(EqualsSpec(spec), _))
        .WillOnce(
            DoAll(SetArgPointee<1>(kRestartedContainerPid), Return(true)));
  }
  // Start the container.
  EXPECT_TRUE(container_manager.StartContainer(spec));
  scoped_refptr<Container> container = container_manager.Lookup(kContainerName);
  ASSERT_TRUE(container != nullptr);
  EXPECT_EQ(kContainerName, container->name());
  EXPECT_EQ(Container::State::RUNNING, container->state());
  EXPECT_EQ(Container::State::RUNNING, container->desired_state());
  EXPECT_EQ(kContainerPid, container->init_pid());

  // Call StartContainer on the already running container.
  EXPECT_TRUE(container_manager.StartContainer(spec));

  // StartContainer should attempt to kill the running container first.
  EXPECT_EQ(Container::State::DYING, container->state());
  EXPECT_EQ(Container::State::RUNNING, container->desired_state());

  // When killing the running container, we should have queued a task on the
  // message loop to send SIGKILL to the container, so run it.
  run_loop.Run();

  // Notify container_manager that the original container process has been
  // reaped.
  siginfo_t info = {};
  info.si_pid = kContainerPid;
  info.si_signo = SIGCHLD;
  info.si_status = SIGKILL;
  info.si_code = CLD_KILLED;

  container_manager.OnReap(info);

  // ContainerManager should have restarted the container.
  container = container_manager.Lookup(kContainerName);
  ASSERT_TRUE(container != nullptr);
  EXPECT_EQ(kContainerName, container->name());
  EXPECT_EQ(Container::State::RUNNING, container->state());
  EXPECT_EQ(Container::State::RUNNING, container->desired_state());
  EXPECT_EQ(kRestartedContainerPid, container->init_pid());
}

}  // namespace
}  // namespace germ
