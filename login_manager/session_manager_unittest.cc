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
#include <base/message_loop.h>
#include <base/time.h>

#include "login_manager/child_job.h"
#include "login_manager/file_checker.h"
#include "login_manager/matchers.h"
#include "login_manager/mock_child_job.h"
#include "login_manager/mock_file_checker.h"
#include "login_manager/mock_liveness_checker.h"
#include "login_manager/mock_metrics.h"
#include "login_manager/mock_mitigator.h"
#include "login_manager/mock_policy_service.h"
#include "login_manager/mock_session_manager.h"
#include "login_manager/mock_system_utils.h"
#include "login_manager/mock_upstart_signal_emitter.h"
#include "login_manager/mock_user_policy_service_factory.h"

using ::testing::AtMost;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrEq;
using ::testing::WithArgs;
using ::testing::_;

namespace login_manager {

// static
char SessionManagerTest::kFakeEmail[] = "cmasone@whaaat.org";
const char SessionManagerTest::kCheckedFile[] = "/tmp/checked_file";
const pid_t SessionManagerTest::kDummyPid = 4;

SessionManagerTest::SessionManagerTest()
    : manager_(NULL),
      file_checker_(new MockFileChecker(kCheckedFile)),
      liveness_checker_(new MockLivenessChecker),
      metrics_(new MockMetrics),
      mitigator_(new MockMitigator),
      session_manager_impl_(new MockSessionManager),
      must_destroy_mocks_(true) {
}

SessionManagerTest::~SessionManagerTest() {
  if (must_destroy_mocks_) {
    delete file_checker_;
    delete liveness_checker_;
    delete metrics_;
    delete mitigator_;
  }
}

void SessionManagerTest::SetUp() {
  ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
}

void SessionManagerTest::TearDown() {
  must_destroy_mocks_ = !manager_.get();
  manager_ = NULL;
}

void SessionManagerTest::InitManager(MockChildJob* job) {
  EXPECT_CALL(*job, GetName())
      .WillRepeatedly(Return(std::string("job")));
  EXPECT_CALL(*job, IsDesiredUidSet())
      .WillRepeatedly(Return(false));
  scoped_ptr<ChildJobInterface> job_ptr(job);

  ASSERT_TRUE(MessageLoop::current() == NULL);
  manager_ = new SessionManagerService(job_ptr.Pass(),
                                       3,
                                       false,
                                       base::TimeDelta(),
                                       &real_utils_);
  manager_->Reset();
  manager_->set_file_checker(file_checker_);
  manager_->set_mitigator(mitigator_);
  manager_->test_api().set_liveness_checker(liveness_checker_);
  manager_->test_api().set_login_metrics(metrics_);
  manager_->test_api().set_session_manager(session_manager_impl_);
}

void SessionManagerTest::SimpleRunManager() {
  manager_->test_api().set_exit_on_child_done(true);

  ExpectSuccessfulInitialization();
  ExpectShutdown();

  // Expect and mimic successful cleanup of children.
  EXPECT_CALL(utils_, kill(_, _, _))
      .Times(AtMost(1))
      .WillRepeatedly(WithArgs<0,2>(Invoke(::kill)));
  EXPECT_CALL(utils_, ChildIsGone(_, _))
      .Times(AtMost(1))
      .WillRepeatedly(Return(true));

  MockUtils();
  manager_->Run();
}

void SessionManagerTest::MockUtils() {
  manager_->test_api().set_systemutils(&utils_);
}

void SessionManagerTest::ExpectSuccessfulInitialization() {
  // Happy signal needed during Run().
  EXPECT_CALL(*session_manager_impl_, Initialize())
      .WillOnce(Return(true));
}

void SessionManagerTest::ExpectShutdown() {
  EXPECT_CALL(*session_manager_impl_, Finalize())
      .Times(1);
  EXPECT_CALL(*session_manager_impl_, AnnounceSessionStoppingIfNeeded())
      .Times(1);
  EXPECT_CALL(*session_manager_impl_, AnnounceSessionStopped())
      .Times(1);
}

}  // namespace login_manager
