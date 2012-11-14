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
    loop_proxy_ = base::MessageLoopProxy::current();
    checker_.reset(new LivenessCheckerImpl(manager_.get(),
                                           &system_,
                                           loop_proxy_,
                                           true));
  }

  void ScheduleQuit() {
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  void ExpectLivenessPing() {
    EXPECT_CALL(system_,
                SendSignalToChromium(StrEq(chromium::kLivenessRequestedSignal),
                                     NULL))
        .Times(1);
  }

  // Expect two pings, the first with a response.
  void ExpectLivenessPingResponsePing() {
    EXPECT_CALL(system_,
                SendSignalToChromium(StrEq(chromium::kLivenessRequestedSignal),
                                     NULL))
        .WillOnce(
            InvokeWithoutArgs(checker_.get(),
                              &LivenessCheckerImpl::HandleLivenessConfirmed))
        .WillOnce(Return());
  }

  // Expect three runs through CheckAndSendLivenessPing():
  // 1) No ping has been sent before, so expect initial ping and ACK it.
  // 2) Last ping was ACK'd, so expect a no-op during this run.
  // 3) Caller should expect action during this run; Quit after it.
  void ExpectPingResponsePingCheckPingAndQuit() {
    EXPECT_CALL(system_,
                SendSignalToChromium(StrEq(chromium::kLivenessRequestedSignal),
                                     NULL))
        .WillOnce(
            InvokeWithoutArgs(checker_.get(),
                              &LivenessCheckerImpl::HandleLivenessConfirmed))
        .WillOnce(Return())
        .WillOnce(InvokeWithoutArgs(MessageLoop::current(),
                                    &MessageLoop::QuitNow));
  }

  MessageLoop loop_;
  scoped_ptr<LivenessCheckerImpl> checker_;
  scoped_refptr<base::MessageLoopProxy> loop_proxy_;
  scoped_refptr<StrictMock<MockSessionManagerService> > manager_;
  StrictMock<MockSystemUtils> system_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LivenessCheckerImplTest);
};

TEST_F(LivenessCheckerImplTest, CheckAndSendOutstandingPing) {
  ExpectLivenessPing();
  EXPECT_CALL(*manager_.get(), AbortBrowser()).Times(1);
  checker_->CheckAndSendLivenessPing(0);
  MessageLoop::current()->RunAllPending();
}

TEST_F(LivenessCheckerImplTest, CheckAndSendAckedThenOutstandingPing) {
  ExpectLivenessPingResponsePing();
  EXPECT_CALL(*manager_.get(), AbortBrowser()).Times(1);
  checker_->CheckAndSendLivenessPing(0);
  MessageLoop::current()->RunAllPending();
}

TEST_F(LivenessCheckerImplTest, CheckAndSendAckedThenOutstandingPingNeutered) {
  checker_.reset(new LivenessCheckerImpl(manager_.get(),
                                         &system_,
                                         loop_proxy_,
                                         false));
  ExpectPingResponsePingCheckPingAndQuit();
  // Expect _no_ browser abort!
  checker_->CheckAndSendLivenessPing(0);
  MessageLoop::current()->RunAllPending();
}

TEST_F(LivenessCheckerImplTest, StartStop) {
  checker_->Start();
  EXPECT_TRUE(checker_->IsRunning());
  checker_->Stop();  // Should cancel ping, so...
  EXPECT_FALSE(checker_->IsRunning());
}

}  // namespace login_manager
