// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/liveness_checker_impl.h"

#include <base/basictypes.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop.h>
#include <base/message_loop_proxy.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "login_manager/mock_session_manager_service.h"
#include "login_manager/mock_system_utils.h"

using ::testing::AtLeast;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace login_manager {

class LivenessCheckerImplTest : public ::testing::Test {
 public:
  LivenessCheckerImplTest() {}
  virtual ~LivenessCheckerImplTest() {}

  virtual void SetUp() {
    manager_ = new StrictMock<MockSessionManagerService>();
    scoped_refptr<base::MessageLoopProxy> message_loop(
        base::MessageLoopProxy::current());
    checker_.reset(new LivenessCheckerImpl(manager_.get(),
                                           &system_,
                                           message_loop,
                                           true));
  }

  void ScheduleQuit() {
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  void ExpectLivenessPingAndQuit() {
  EXPECT_CALL(system_,
              SendSignalToChromium(StrEq(chromium::kLivenessRequestedSignal),
                                   NULL))
      .WillOnce(InvokeWithoutArgs(this,
                                  &LivenessCheckerImplTest::ScheduleQuit));
  }

  void ExpectAndRespondToLivenessPingAndQuit() {
    EXPECT_CALL(system_,
                SendSignalToChromium(StrEq(chromium::kLivenessRequestedSignal),
                                     NULL))
        .WillOnce(
            InvokeWithoutArgs(checker_.get(),
                              &LivenessCheckerImpl::HandleLivenessConfirmed))
        .WillOnce(InvokeWithoutArgs(this,
                                    &LivenessCheckerImplTest::ScheduleQuit));
  }

  scoped_ptr<LivenessCheckerImpl> checker_;

  scoped_refptr<StrictMock<MockSessionManagerService> > manager_;
  StrictMock<MockSystemUtils> system_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LivenessCheckerImplTest);
};

TEST_F(LivenessCheckerImplTest, CheckAndSendOutstandingPing) {
  ExpectLivenessPingAndQuit();
  EXPECT_CALL(*manager_.get(), AbortBrowser()).Times(1);
  checker_->CheckAndSendLivenessPing(0);
  MessageLoop::current()->Run();
}

TEST_F(LivenessCheckerImplTest, CheckAndSendOutstandingPingNeutered) {
  scoped_refptr<base::MessageLoopProxy> message_loop(
      base::MessageLoopProxy::current());
  checker_.reset(new LivenessCheckerImpl(manager_.get(),
                                         &system_,
                                         message_loop,
                                         false));
  ExpectAndRespondToLivenessPingAndQuit();
  // Expect _no_ browser abort!
  checker_->CheckAndSendLivenessPing(0);
  MessageLoop::current()->Run();
}

TEST_F(LivenessCheckerImplTest, CheckAndSendAckedThenOutstandingPing) {
  ExpectAndRespondToLivenessPingAndQuit();
  EXPECT_CALL(*manager_.get(), AbortBrowser()).Times(1);
  checker_->CheckAndSendLivenessPing(0);
  MessageLoop::current()->Run();
}

TEST_F(LivenessCheckerImplTest, StartStop) {
  ExpectLivenessPingAndQuit();
  checker_->Start();
  checker_->Stop();  // Should cancel ping, so...
  MessageLoop::current()->RunAllPending();  // ...there should be no tasks!
}

}  // namespace login_manager
