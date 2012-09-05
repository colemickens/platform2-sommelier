// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_unittest.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <base/basictypes.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>

#include "login_manager/child_job.h"
#include "login_manager/file_checker.h"
#include "login_manager/mock_child_job.h"
#include "login_manager/mock_device_policy_service.h"
#include "login_manager/mock_file_checker.h"
#include "login_manager/mock_metrics.h"
#include "login_manager/mock_mitigator.h"
#include "login_manager/mock_policy_service.h"
#include "login_manager/mock_system_utils.h"
#include "login_manager/mock_upstart_signal_emitter.h"
#include "login_manager/mock_user_policy_service_factory.h"

using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::WithArgs;
using ::testing::_;

namespace {

MATCHER_P(CastEq, str, "") {
  return std::equal(str.begin(), str.end(), reinterpret_cast<const char*>(arg));
}

}  // namespace

namespace login_manager {

// static
char SessionManagerTest::kFakeEmail[] = "cmasone@whaaat.org";
const char SessionManagerTest::kCheckedFile[] = "/tmp/checked_file";
const pid_t SessionManagerTest::kDummyPid = 4;

SessionManagerTest::SessionManagerTest()
    : manager_(NULL),
      file_checker_(new MockFileChecker(kCheckedFile)),
      metrics_(new MockMetrics),
      mitigator_(new MockMitigator),
      upstart_(new MockUpstartSignalEmitter),
      device_policy_service_(new MockDevicePolicyService),
      user_policy_service_(NULL),
      must_destroy_mocks_(true) {
}

SessionManagerTest::~SessionManagerTest() {
  if (must_destroy_mocks_) {
    delete file_checker_;
    delete metrics_;
    delete mitigator_;
    delete upstart_;
  }
}

void SessionManagerTest::SetUp() {
  ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
}

void SessionManagerTest::TearDown() {
  must_destroy_mocks_ = !manager_.get();
  manager_ = NULL;
}

void SessionManagerTest::InitManager(MockChildJob* job1, MockChildJob* job2) {
  std::vector<ChildJobInterface*> jobs;
  EXPECT_CALL(*job1, GetName())
      .WillRepeatedly(Return(std::string("job1")));
  EXPECT_CALL(*job1, IsDesiredUidSet())
      .WillRepeatedly(Return(false));
  jobs.push_back(job1);
  if (job2) {
    EXPECT_CALL(*job2, GetName())
        .WillRepeatedly(Return(std::string("job2")));
    EXPECT_CALL(*job2, IsDesiredUidSet())
        .WillRepeatedly(Return(false));
    jobs.push_back(job2);
  }
  ASSERT_TRUE(MessageLoop::current() == NULL);
  manager_ = new SessionManagerService(jobs, 3, &real_utils_);
  manager_->set_file_checker(file_checker_);
  manager_->set_mitigator(mitigator_);
  manager_->test_api().set_exit_on_child_done(true);
  manager_->test_api().set_upstart_signal_emitter(upstart_);
  manager_->test_api().set_device_policy_service(device_policy_service_);
  manager_->test_api().set_login_metrics(metrics_);
}

PolicyService* SessionManagerTest::CreateUserPolicyService() {
  user_policy_service_ = new MockPolicyService();
  EXPECT_CALL(*user_policy_service_, Initialize())
      .WillOnce(Return(true));
  return user_policy_service_;
}

void SessionManagerTest::SimpleRunManager() {
  ExpectPolicySetup();
  EXPECT_CALL(utils_,
              BroadcastSignal(_, _, SessionManagerService::kStopped, _))
      .Times(1);
  EXPECT_CALL(utils_, kill(_, _, _))
      .Times(AtMost(1))
      .WillRepeatedly(WithArgs<0,2>(Invoke(::kill)));
  EXPECT_CALL(utils_, ChildIsGone(_, _))
      .Times(AtMost(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(utils_, IsDevMode())
      .Times(AtMost(1))
      .WillRepeatedly(Return(false));
  MockUtils();

  manager_->Run();
}

void SessionManagerTest::MockUtils() {
  manager_->test_api().set_systemutils(&utils_);
}

void SessionManagerTest::ExpectPolicySetup() {
  EXPECT_CALL(*device_policy_service_, Initialize())
      .WillOnce(Return(true));
  EXPECT_CALL(*device_policy_service_, PersistPolicySync())
      .WillOnce(Return(true));
  if (user_policy_service_) {
    EXPECT_CALL(*user_policy_service_, PersistPolicySync())
        .WillOnce(Return(true));
  }
}

void SessionManagerTest::ExpectUserPolicySetup() {
  // Pretend user policy initializes successfully.
  MockUserPolicyServiceFactory* factory = new MockUserPolicyServiceFactory;
  EXPECT_CALL(*factory, Create(_))
      .Times(AtMost(1))
      .WillRepeatedly(
          InvokeWithoutArgs(this,
                            &SessionManagerTest::CreateUserPolicyService));
  manager_->test_api().set_user_policy_service_factory(factory);
}

}  // namespace login_manager
