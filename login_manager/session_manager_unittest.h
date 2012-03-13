// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SESSION_MANAGER_UNITTEST_H_
#define LOGIN_MANAGER_SESSION_MANAGER_UNITTEST_H_

#include "login_manager/session_manager_service.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/types.h>

#include <base/basictypes.h>
#include <base/memory/ref_counted.h>
#include <base/scoped_temp_dir.h>

#include "login_manager/mock_system_utils.h"

namespace login_manager {

class MockChildJob;
class MockDevicePolicyService;
class MockFileChecker;
class MockMetrics;
class MockMitigator;
class MockUpstartSignalEmitter;

// Used as a base class for other SessionManagerService unittests.
class SessionManagerTest : public ::testing::Test {
 public:
  SessionManagerTest();
  virtual ~SessionManagerTest();

  virtual void SetUp();
  virtual void TearDown();

 protected:
  // kFakeEmail is NOT const so that it can be passed to methods that
  // implement dbus calls, which (of necessity) take bare gchar*.
  static char kFakeEmail[];
  static const char kCheckedFile[];
  static const pid_t kDummyPid;

  // Instantiates |manager_| with the provided jobs. Mocks the file
  // checker, the key loss mitigator, the upstart signal emitter and
  // the device policy service.
  // |job2| can be NULL.
  void InitManager(MockChildJob* job1, MockChildJob* job2);

  // Sets up expectations for common things that happen during startup and
  // shutdown of SessionManagerService and calls manager_->Run().
  void SimpleRunManager();

  // Injects a mock SystemUtils into |manager_|.
  void MockUtils();

  // Sets up expectations for policy stuff we do at startup.
  void ExpectPolicySetup();

  scoped_refptr<SessionManagerService> manager_;
  SystemUtils real_utils_;
  MockSystemUtils utils_;
  MockFileChecker* file_checker_;
  MockMetrics* metrics_;
  MockMitigator* mitigator_;
  MockUpstartSignalEmitter* upstart_;
  MockDevicePolicyService* device_policy_service_;
  bool must_destroy_mocks_;
  ScopedTempDir tmpdir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManagerTest);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_SESSION_MANAGER_UNITTEST_H_
