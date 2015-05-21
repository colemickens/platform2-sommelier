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
#include <base/time/time.h>
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

// When killing a container, a delayed task is queued up which sends SIGKILL to
// the container in case it does not die in response to SIGTERM. This test
// ensures that SIGKILL is never sent to a newer instance of the container.
TEST(ContainerTest, OnlyKillCurrentGeneration) {
  base::AtExitManager exit_manager;
  base::MessageLoopForIO message_loop;
  base::RunLoop run_loop;

  MockGermZygote mock_zygote;

  const char kContainerName[] = "test_container";
  const pid_t kContainerPid = 1234;
  const pid_t kRestartedContainerPid = 5678;

  soma::SandboxSpec spec = MakeSpecForTest(kContainerName);

  {
    InSequence seq;
    EXPECT_CALL(mock_zygote, StartContainer(EqualsSpec(spec), _))
        .WillOnce(DoAll(SetArgPointee<1>(kContainerPid), Return(true)));
    EXPECT_CALL(mock_zygote, Kill(kContainerPid, SIGTERM))
        .WillOnce(Return(true));

    EXPECT_CALL(mock_zygote, StartContainer(EqualsSpec(spec), _))
        .WillOnce(
            DoAll(SetArgPointee<1>(kRestartedContainerPid), Return(true)));
    EXPECT_CALL(mock_zygote, Kill(kRestartedContainerPid, SIGTERM))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_zygote, Kill(kRestartedContainerPid, SIGKILL))
        .WillOnce(DoAll(PostTask(run_loop.QuitClosure()), Return(true)));
  }

  // Start a container, and terminate it by sending it a SIGTERM (and queuing up
  // a SIGKILL for later).
  scoped_refptr<Container> container = new Container(spec);
  EXPECT_TRUE(container->Launch(&mock_zygote));
  EXPECT_EQ(kContainerPid, container->init_pid());
  EXPECT_EQ(Container::State::RUNNING, container->state());

  EXPECT_TRUE(container->Terminate(&mock_zygote));
  EXPECT_EQ(Container::State::DYING, container->state());

  // Pretend that the container was killed by the SIGTERM.
  container->OnReap();
  EXPECT_EQ(Container::State::STOPPED, container->state());

  // Start the container again then terminate it.
  EXPECT_TRUE(container->Launch(&mock_zygote));
  EXPECT_EQ(kRestartedContainerPid, container->init_pid());
  EXPECT_EQ(Container::State::RUNNING, container->state());

  EXPECT_TRUE(container->Terminate(&mock_zygote));
  EXPECT_EQ(Container::State::DYING, container->state());

  // Now, the message loop should have the SIGKILL tasks for both instantiations
  // of the container, but only the one for the second instantiation should
  // do anything.
  run_loop.Run();
}

// When killing a container, a delayed task is queued up which sends SIGKILL to
// the container in case it does not die in response to SIGTERM. This test
// ensures that SIGKILL is not sent if the process dies in response to SIGTERM.
TEST(ContainerTest, DoesNotSendSIGKILLToReapedContainer) {
  base::AtExitManager exit_manager;
  base::MessageLoopForIO message_loop;
  base::RunLoop run_loop;

  MockGermZygote mock_zygote;

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
  }

  // Start a container, and terminate it by sending it a SIGTERM (and queuing up
  // a SIGKILL for later).
  scoped_refptr<Container> container = new Container(spec);
  EXPECT_TRUE(container->Launch(&mock_zygote));
  EXPECT_EQ(Container::State::RUNNING, container->state());
  EXPECT_EQ(kContainerPid, container->init_pid());

  EXPECT_TRUE(container->Terminate(&mock_zygote));
  EXPECT_EQ(Container::State::DYING, container->state());

  // Pretend that the container was killed by the SIGTERM.
  container->OnReap();
  EXPECT_EQ(Container::State::STOPPED, container->state());

  // Should be scheduled after the SIGKILL task.
  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(spec.shutdown_timeout_ms()));

  // Now, the message loop should have the SIGKILL task, but it should do
  // nothing because the container is already in state STOPPED.
  run_loop.Run();
}

}  // namespace
}  // namespace germ
