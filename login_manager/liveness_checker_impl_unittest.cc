// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/liveness_checker_impl.h"

#include <base/basictypes.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop.h>
#include <base/message_loop_proxy.h>
#include <base/run_loop.h>
#include <base/time.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/dbus.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "login_manager/mock_process_manager_service.h"
#include "login_manager/mock_system_utils.h"
#include "login_manager/scoped_dbus_pending_call.h"

using ::base::TimeDelta;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace login_manager {

class LivenessCheckerImplTest : public ::testing::Test {
 public:
  LivenessCheckerImplTest() {}
  virtual ~LivenessCheckerImplTest() {
    // NB: checker_ must be destroyed before system_.
  }

  virtual void SetUp() {
    manager_.reset(new StrictMock<MockProcessManagerService>);
    loop_proxy_ = base::MessageLoopProxy::current();
    checker_.reset(new LivenessCheckerImpl(manager_.get(),
                                           &system_,
                                           loop_proxy_,
                                           true,
                                           TimeDelta::FromSeconds(0)));
  }

  void ExpectUnAckedLivenessPing() {
    scoped_ptr<ScopedDBusPendingCall> call =
        ScopedDBusPendingCall::CreateForTesting();
    EXPECT_CALL(system_, CheckAsyncMethodSuccess(call->Get()))
        .WillOnce(Return(false));
    EXPECT_CALL(system_, CancelAsyncMethodCall(call->Get()))
        .Times(1);
    system_.EnqueueFakePendingCall(call.Pass());
  }

  // Expect two pings, the first with a response.
  void ExpectLivenessPingResponsePing() {
    scoped_ptr<ScopedDBusPendingCall> call1 =
        ScopedDBusPendingCall::CreateForTesting();
    scoped_ptr<ScopedDBusPendingCall> call2 =
        ScopedDBusPendingCall::CreateForTesting();
    EXPECT_CALL(system_, CheckAsyncMethodSuccess(call1->Get()))
        .WillOnce(Return(true));
    EXPECT_CALL(system_, CheckAsyncMethodSuccess(call2->Get()))
        .WillOnce(Return(false));
    EXPECT_CALL(system_, CancelAsyncMethodCall(call2->Get()))
        .Times(1);

    system_.EnqueueFakePendingCall(call1.Pass());
    system_.EnqueueFakePendingCall(call2.Pass());
  }

  // Expect three runs through CheckAndSendLivenessPing():
  // 1) No ping has been sent before, so expect initial ping and ACK it.
  // 2) Last ping was ACK'd, so expect a no-op during this run.
  // 3) Caller should expect action during this run; Quit after it.
  void ExpectPingResponsePingCheckPingAndQuit() {
    scoped_ptr<ScopedDBusPendingCall> call1 =
        ScopedDBusPendingCall::CreateForTesting();
    scoped_ptr<ScopedDBusPendingCall> call2 =
        ScopedDBusPendingCall::CreateForTesting();
    scoped_ptr<ScopedDBusPendingCall> call3 =
        ScopedDBusPendingCall::CreateForTesting();

    EXPECT_CALL(system_, CheckAsyncMethodSuccess(call1->Get()))
        .WillOnce(Return(true));
    EXPECT_CALL(system_, CheckAsyncMethodSuccess(call2->Get()))
        .WillOnce(DoAll(InvokeWithoutArgs(MessageLoop::current(),
                                          &MessageLoop::QuitNow),
                        Return(false)));
    EXPECT_CALL(system_, CancelAsyncMethodCall(call3->Get()))
        .Times(1);

    system_.EnqueueFakePendingCall(call1.Pass());
    system_.EnqueueFakePendingCall(call2.Pass());
    system_.EnqueueFakePendingCall(call3.Pass());
  }

  MessageLoop loop_;
  scoped_refptr<base::MessageLoopProxy> loop_proxy_;
  StrictMock<MockSystemUtils> system_;
  scoped_ptr<StrictMock<MockProcessManagerService> > manager_;

  scoped_ptr<LivenessCheckerImpl> checker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LivenessCheckerImplTest);
};

TEST_F(LivenessCheckerImplTest, CheckAndSendOutstandingPing) {
  ExpectUnAckedLivenessPing();
  EXPECT_CALL(*manager_.get(), AbortBrowser(SIGFPE)).Times(1);
  checker_->CheckAndSendLivenessPing(TimeDelta());
  base::RunLoop().RunUntilIdle();
}

TEST_F(LivenessCheckerImplTest, CheckAndSendAckedThenOutstandingPing) {
  ExpectLivenessPingResponsePing();
  EXPECT_CALL(*manager_.get(), AbortBrowser(SIGFPE)).Times(1);
  checker_->CheckAndSendLivenessPing(TimeDelta());
  base::RunLoop().RunUntilIdle();
}

TEST_F(LivenessCheckerImplTest, CheckAndSendAckedThenOutstandingPingNeutered) {
  checker_.reset(new LivenessCheckerImpl(manager_.get(),
                                         &system_,
                                         loop_proxy_,
                                         false  /* Disable aborting */,
                                         TimeDelta()));
  ExpectPingResponsePingCheckPingAndQuit();
  // Expect _no_ browser abort!
  checker_->CheckAndSendLivenessPing(TimeDelta());
  base::RunLoop().RunUntilIdle();
}

TEST_F(LivenessCheckerImplTest, StartStop) {
  checker_->Start();
  EXPECT_TRUE(checker_->IsRunning());
  checker_->Stop();  // Should cancel ping, so...
  EXPECT_FALSE(checker_->IsRunning());
}

}  // namespace login_manager
