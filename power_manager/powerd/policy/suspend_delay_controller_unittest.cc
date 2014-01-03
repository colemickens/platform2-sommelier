// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/suspend_delay_controller.h"

#include <gtest/gtest.h>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_sender_stub.h"
#include "power_manager/common/test_main_loop_runner.h"
#include "power_manager/powerd/policy/suspend_delay_observer.h"
#include "power_manager/proto_bindings/suspend.pb.h"

namespace power_manager {
namespace policy {

namespace {

// Maximum amount of time to wait for OnReadyForSuspend() to be called.
const int kSuspendTimeoutMs = 5000;

class TestObserver : public SuspendDelayObserver {
 public:
  TestObserver() {}
  virtual ~TestObserver() {}

  // Runs |loop_| until OnReadyForSuspend() is called.
  bool RunUntilReadyForSuspend() {
    return loop_runner_.StartLoop(
        base::TimeDelta::FromMilliseconds(kSuspendTimeoutMs));
  }

  // SuspendDelayObserver implementation:
  virtual void OnReadyForSuspend(int suspend_id) OVERRIDE {
    loop_runner_.StopLoop();
  }

 private:
  TestMainLoopRunner loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class SuspendDelayControllerTest : public ::testing::Test {
 public:
  SuspendDelayControllerTest()
      : controller_(&dbus_sender_) {
    controller_.AddObserver(&observer_);
  }

  virtual ~SuspendDelayControllerTest() {
    controller_.RemoveObserver(&observer_);
  }

 protected:
  // Calls |controller_|'s RegisterSuspendDelay() method and returns the
  // newly-created delay's ID.
  int RegisterSuspendDelay(base::TimeDelta timeout, const std::string& client) {
    RegisterSuspendDelayRequest request;
    request.set_timeout(timeout.ToInternalValue());
    request.set_description(client + "-desc");
    RegisterSuspendDelayReply reply;
    controller_.RegisterSuspendDelay(request, client, &reply);
    return reply.delay_id();
  }

  // Calls |controller_|'s UnregisterSuspendDelay() method.
  void UnregisterSuspendDelay(int delay_id, const std::string& client) {
    UnregisterSuspendDelayRequest request;
    request.set_delay_id(delay_id);
    controller_.UnregisterSuspendDelay(request, client);
  }

  // Calls |controller_|'s HandleSuspendReadiness() method.
  void HandleSuspendReadiness(int delay_id,
                              int suspend_id,
                              const std::string& client) {
    SuspendReadinessInfo info;
    info.set_delay_id(delay_id);
    info.set_suspend_id(suspend_id);
    controller_.HandleSuspendReadiness(info, client);
  }

  // Tests that exactly one SuspendImminent signal has been emitted via
  // |dbus_sender_| and returns the suspend ID from it.  Clears sent signals
  // before returning.
  int GetSuspendId() {
    EXPECT_EQ(1, dbus_sender_.num_sent_signals());
    SuspendImminent proto;
    EXPECT_TRUE(dbus_sender_.GetSentSignal(0, kSuspendImminentSignal, &proto));
    dbus_sender_.ClearSentSignals();
    return proto.suspend_id();
  }

  TestObserver observer_;
  DBusSenderStub dbus_sender_;
  SuspendDelayController controller_;

  DISALLOW_COPY_AND_ASSIGN(SuspendDelayControllerTest);
};

}  // namespace

TEST_F(SuspendDelayControllerTest, NoDelays) {
  // The controller should say that it's initially ready to suspend when no
  // delays have been registered.
  EXPECT_TRUE(controller_.ready_for_suspend());

  // The controller should still say that it's ready to suspend after we request
  // suspending -- there are no delays to wait for.
  const int kSuspendId = 5;
  controller_.PrepareForSuspend(kSuspendId);
  EXPECT_EQ(kSuspendId, GetSuspendId());
  EXPECT_TRUE(controller_.ready_for_suspend());

  // The observer should be notified that it's safe to suspend.
  EXPECT_TRUE(observer_.RunUntilReadyForSuspend());
  EXPECT_TRUE(controller_.ready_for_suspend());
}

TEST_F(SuspendDelayControllerTest, SingleDelay) {
  // Register a delay.
  const std::string kClient = "client";
  int delay_id = RegisterSuspendDelay(base::TimeDelta::FromSeconds(8), kClient);
  EXPECT_TRUE(controller_.ready_for_suspend());

  // A SuspendImminent signal should be emitted after suspending is requested.
  // The controller shouldn't report readiness now; it's waiting on the delay.
  const int kSuspendId = 5;
  controller_.PrepareForSuspend(kSuspendId);
  EXPECT_EQ(kSuspendId, GetSuspendId());
  EXPECT_FALSE(controller_.ready_for_suspend());

  // Tell the controller that the delay is ready and check that the controller
  // reports readiness now.
  HandleSuspendReadiness(delay_id, kSuspendId, kClient);
  EXPECT_TRUE(controller_.ready_for_suspend());
  EXPECT_TRUE(observer_.RunUntilReadyForSuspend());
  EXPECT_TRUE(controller_.ready_for_suspend());
}

TEST_F(SuspendDelayControllerTest, UnregisterDelayBeforeRequestingSuspend) {
  // Register a delay, but unregister it immediately.
  const std::string kClient = "client";
  int delay_id = RegisterSuspendDelay(base::TimeDelta::FromSeconds(8), kClient);
  EXPECT_TRUE(controller_.ready_for_suspend());
  UnregisterSuspendDelay(delay_id, kClient);
  EXPECT_TRUE(controller_.ready_for_suspend());

  // The controller should immediately report readiness.
  const int kSuspendId = 5;
  controller_.PrepareForSuspend(kSuspendId);
  EXPECT_EQ(kSuspendId, GetSuspendId());
  EXPECT_TRUE(controller_.ready_for_suspend());
  EXPECT_TRUE(observer_.RunUntilReadyForSuspend());
  EXPECT_TRUE(controller_.ready_for_suspend());
}

TEST_F(SuspendDelayControllerTest, UnregisterDelayAfterRequestingSuspend) {
  // Register a delay.
  const std::string kClient = "client";
  int delay_id = RegisterSuspendDelay(base::TimeDelta::FromSeconds(8), kClient);
  EXPECT_TRUE(controller_.ready_for_suspend());

  // Request suspending.
  const int kSuspendId = 5;
  controller_.PrepareForSuspend(kSuspendId);
  EXPECT_EQ(kSuspendId, GetSuspendId());
  EXPECT_FALSE(controller_.ready_for_suspend());

  // If the delay is unregistered while the controller is waiting for it, the
  // controller should start reporting readiness.
  UnregisterSuspendDelay(delay_id, kClient);
  EXPECT_TRUE(controller_.ready_for_suspend());
  EXPECT_TRUE(observer_.RunUntilReadyForSuspend());
  EXPECT_TRUE(controller_.ready_for_suspend());
}

TEST_F(SuspendDelayControllerTest, RegisterDelayAfterRequestingSuspend) {
  // Request suspending before any delays have been registered.
  const int kSuspendId = 5;
  controller_.PrepareForSuspend(kSuspendId);
  EXPECT_EQ(kSuspendId, GetSuspendId());
  EXPECT_TRUE(controller_.ready_for_suspend());

  // Register a delay now.  The controller should still report readiness.
  const std::string kClient = "client";
  int delay_id = RegisterSuspendDelay(base::TimeDelta::FromSeconds(8), kClient);
  EXPECT_TRUE(controller_.ready_for_suspend());
  EXPECT_TRUE(observer_.RunUntilReadyForSuspend());
  EXPECT_TRUE(controller_.ready_for_suspend());

  // Request suspending again.  The controller should say it isn't ready now.
  const int kNextSuspendId = 6;
  controller_.PrepareForSuspend(kNextSuspendId);
  EXPECT_EQ(kNextSuspendId, GetSuspendId());
  EXPECT_FALSE(controller_.ready_for_suspend());

  HandleSuspendReadiness(delay_id, kNextSuspendId, kClient);
  EXPECT_TRUE(controller_.ready_for_suspend());
  EXPECT_TRUE(observer_.RunUntilReadyForSuspend());
  EXPECT_TRUE(controller_.ready_for_suspend());
}

TEST_F(SuspendDelayControllerTest, Timeout) {
  // Register a delay with a short timeout.
  const std::string kClient = "client";
  RegisterSuspendDelay(base::TimeDelta::FromMilliseconds(8), kClient);
  EXPECT_TRUE(controller_.ready_for_suspend());

  // The controller should report readiness due to the timeout being hit.
  const int kSuspendId = 5;
  controller_.PrepareForSuspend(kSuspendId);
  EXPECT_EQ(kSuspendId, GetSuspendId());
  EXPECT_FALSE(controller_.ready_for_suspend());
  EXPECT_TRUE(observer_.RunUntilReadyForSuspend());
  EXPECT_TRUE(controller_.ready_for_suspend());
}

TEST_F(SuspendDelayControllerTest, DisconnectClientBeforeRequestingSuspend) {
  // Register a delay, but immediately tell the controller that the D-Bus client
  // that registered the delay has disconnected.
  const std::string kClient = "client";
  RegisterSuspendDelay(base::TimeDelta::FromSeconds(8), kClient);
  EXPECT_TRUE(controller_.ready_for_suspend());
  controller_.HandleDBusClientDisconnected(kClient);
  EXPECT_TRUE(controller_.ready_for_suspend());

  // The delay should have been removed.
  const int kSuspendId = 5;
  controller_.PrepareForSuspend(kSuspendId);
  EXPECT_EQ(kSuspendId, GetSuspendId());
  EXPECT_TRUE(controller_.ready_for_suspend());
  EXPECT_TRUE(observer_.RunUntilReadyForSuspend());
  EXPECT_TRUE(controller_.ready_for_suspend());
}

TEST_F(SuspendDelayControllerTest, DisconnectClientAfterRequestingSuspend) {
  const std::string kClient = "client";
  RegisterSuspendDelay(base::TimeDelta::FromSeconds(8), kClient);
  EXPECT_TRUE(controller_.ready_for_suspend());

  const int kSuspendId = 5;
  controller_.PrepareForSuspend(kSuspendId);
  EXPECT_EQ(kSuspendId, GetSuspendId());
  EXPECT_FALSE(controller_.ready_for_suspend());

  // If the client is disconnected while the controller is waiting, it should
  // report readiness.
  controller_.HandleDBusClientDisconnected(kClient);
  EXPECT_TRUE(controller_.ready_for_suspend());
  EXPECT_TRUE(observer_.RunUntilReadyForSuspend());
  EXPECT_TRUE(controller_.ready_for_suspend());

}

TEST_F(SuspendDelayControllerTest, MultipleSuspendRequests) {
  const std::string kClient = "client";
  int delay_id = RegisterSuspendDelay(base::TimeDelta::FromSeconds(8), kClient);
  EXPECT_TRUE(controller_.ready_for_suspend());

  // Request suspending.
  const int kSuspendId = 5;
  controller_.PrepareForSuspend(kSuspendId);
  EXPECT_EQ(kSuspendId, GetSuspendId());
  EXPECT_FALSE(controller_.ready_for_suspend());

  // Before confirming that the delay is ready, request suspending again.
  const int kNextSuspendId = 6;
  controller_.PrepareForSuspend(kNextSuspendId);
  EXPECT_EQ(kNextSuspendId, GetSuspendId());
  EXPECT_FALSE(controller_.ready_for_suspend());

  // Report readiness, but do it on behalf of the original suspend attempt.  The
  // controller shouldn't say it's ready yet.
  HandleSuspendReadiness(delay_id, kSuspendId, kClient);
  EXPECT_FALSE(controller_.ready_for_suspend());

  // Now report readiness on behalf of the second suspend attempt.
  HandleSuspendReadiness(delay_id, kNextSuspendId, kClient);
  EXPECT_TRUE(controller_.ready_for_suspend());
  EXPECT_TRUE(observer_.RunUntilReadyForSuspend());
  EXPECT_TRUE(controller_.ready_for_suspend());
}

TEST_F(SuspendDelayControllerTest, MultipleDelays) {
  // Register two delays.
  const std::string kClient1 = "client1";
  int delay_id1 =
      RegisterSuspendDelay(base::TimeDelta::FromSeconds(8), kClient1);
  EXPECT_TRUE(controller_.ready_for_suspend());

  const std::string kClient2 = "client2";
  int delay_id2 =
      RegisterSuspendDelay(base::TimeDelta::FromSeconds(8), kClient2);
  EXPECT_TRUE(controller_.ready_for_suspend());

  // After getting a suspend request, the controller shouldn't report readiness
  // until both delays have confirmed their readiness.
  const int kSuspendId = 5;
  controller_.PrepareForSuspend(kSuspendId);
  EXPECT_EQ(kSuspendId, GetSuspendId());
  EXPECT_FALSE(controller_.ready_for_suspend());
  HandleSuspendReadiness(delay_id2, kSuspendId, kClient2);
  EXPECT_FALSE(controller_.ready_for_suspend());
  HandleSuspendReadiness(delay_id1, kSuspendId, kClient1);
  EXPECT_TRUE(controller_.ready_for_suspend());
  EXPECT_TRUE(observer_.RunUntilReadyForSuspend());
  EXPECT_TRUE(controller_.ready_for_suspend());
}

}  // namespace policy
}  // namespace power_manager
