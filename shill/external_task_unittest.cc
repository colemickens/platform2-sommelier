// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/external_task.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/memory/weak_ptr.h>
#include <base/strings/string_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/event_dispatcher.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_glib.h"
#include "shill/mock_process_killer.h"
#include "shill/nice_mock_control.h"

using std::map;
using std::set;
using std::string;
using std::vector;
using testing::_;
using testing::Matcher;
using testing::MatchesRegex;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;

namespace shill {

class ExternalTaskTest : public testing::Test,
                         public RPCTaskDelegate {
 public:
  ExternalTaskTest()
      : weak_ptr_factory_(this),
        death_callback_(
          base::Bind(&ExternalTaskTest::TaskDiedCallback,
                     weak_ptr_factory_.GetWeakPtr())),
        external_task_(
            new ExternalTask(&control_, &glib_, weak_ptr_factory_.GetWeakPtr(),
                             death_callback_)),
        test_rpc_task_destroyed_(false) {
    external_task_->process_killer_ = &process_killer_;
  }

  virtual ~ExternalTaskTest() {}

  virtual void TearDown() {
    if (!external_task_) {
      return;
    }

    if (external_task_->child_watch_tag_) {
      EXPECT_CALL(glib_, SourceRemove(external_task_->child_watch_tag_));
    }

    if (external_task_->pid_) {
      EXPECT_CALL(process_killer_, Kill(external_task_->pid_, _));
    }
  }

  void set_test_rpc_task_destroyed(bool destroyed) {
    test_rpc_task_destroyed_ = destroyed;
  }

  // Defined out-of-line, due to dependency on TestRPCTask.
  void FakeUpRunningProcess(unsigned int tag, int pid);

  void ExpectStop(unsigned int tag, int pid) {
    EXPECT_CALL(glib_, SourceRemove(tag));
    EXPECT_CALL(process_killer_, Kill(pid, _));
  }

  void VerifyStop() {
    if (external_task_) {
      EXPECT_EQ(0, external_task_->child_watch_tag_);
      EXPECT_EQ(0, external_task_->pid_);
      EXPECT_FALSE(external_task_->rpc_task_);
    }
    EXPECT_TRUE(test_rpc_task_destroyed_);
    // Make sure EXPECTations were met before the fixture's dtor.
    Mock::VerifyAndClearExpectations(&glib_);
    Mock::VerifyAndClearExpectations(&process_killer_);
  }

 protected:
  // Implements RPCTaskDelegate interface.
  MOCK_METHOD2(GetLogin, void(string* user, string* password));
  MOCK_METHOD2(Notify, void(const string& reason,
                            const map<string, string>& dict));

  MOCK_METHOD2(TaskDiedCallback, void(pid_t dead_process, int exit_status));

  NiceMockControl control_;
  EventDispatcher dispatcher_;
  MockGLib glib_;
  MockProcessKiller process_killer_;
  base::WeakPtrFactory<ExternalTaskTest> weak_ptr_factory_;
  base::Callback<void(pid_t, int)> death_callback_;
  std::unique_ptr<ExternalTask> external_task_;
  bool test_rpc_task_destroyed_;
};

namespace {

class TestRPCTask : public RPCTask {
 public:
  TestRPCTask(ControlInterface* control, ExternalTaskTest* test);
  virtual ~TestRPCTask();

 private:
  ExternalTaskTest* test_;
};

TestRPCTask::TestRPCTask(ControlInterface* control, ExternalTaskTest* test)
    : RPCTask(control, test),
      test_(test) {
  test_->set_test_rpc_task_destroyed(false);
}

TestRPCTask::~TestRPCTask() {
  test_->set_test_rpc_task_destroyed(true);
  test_ = nullptr;
}

}  // namespace

void ExternalTaskTest::FakeUpRunningProcess(unsigned int tag, int pid) {
  external_task_->child_watch_tag_ = tag;
  external_task_->pid_ = pid;
  external_task_->rpc_task_.reset(new TestRPCTask(&control_, this));
}

TEST_F(ExternalTaskTest, Destructor) {
  const unsigned int kTag = 123;
  const int kPID = 123456;
  FakeUpRunningProcess(kTag, kPID);
  ExpectStop(kTag, kPID);
  external_task_.reset();
  VerifyStop();
}

TEST_F(ExternalTaskTest, DestroyLater) {
  const unsigned int kTag = 123;
  const int kPID = 123456;
  FakeUpRunningProcess(kTag, kPID);
  ExpectStop(kTag, kPID);
  external_task_.release()->DestroyLater(&dispatcher_);
  dispatcher_.DispatchPendingEvents();
  VerifyStop();
}

namespace {

// Returns true iff. there is at least one anchored match in |arg|,
// for each item in |expected_values|. Order of items does not matter.
//
// |arg| is a NULL-terminated array of C-strings.
// |expected_values| is a container of regular expressions (as strings).
MATCHER_P(HasElementsMatching, expected_values, "") {
  for (const auto& expected_value : expected_values) {
    auto regex_matcher(MatchesRegex(expected_value).impl());
    char** arg_local = arg;
    while (*arg_local) {
      if (regex_matcher.MatchAndExplain(*arg_local, result_listener)) {
        break;
      }
      ++arg_local;
    }
    if (*arg_local == nullptr) {
      *result_listener << "missing value " << expected_value << "\n";
      arg_local = arg;
      while (*arg_local) {
        *result_listener << "received: " << *arg_local << "\n";
        ++arg_local;
      }
      return false;
    }
  }
  return true;
}

}  // namespace

TEST_F(ExternalTaskTest, Start) {
  const string kCommand = "/run/me";
  const vector<string> kCommandOptions{"arg1", "arg2"};
  const map<string, string> kCommandEnv{{"env1", "val1"}, {"env2", "val2"}};
  const vector<string> kExpectedEnv{"SHILL_TASK_SERVICE=.+",
                                    "SHILL_TASK_PATH=.+", "env1=val1",
                                    "env2=val2"};
  const int kPID = 234678;
  EXPECT_CALL(glib_, SpawnAsync(_,
                                HasElementsMatching(kCommandOptions),
                                HasElementsMatching(kExpectedEnv),
                                _, _, _, _, _))
      .WillOnce(Return(false))
      .WillOnce(DoAll(SetArgumentPointee<6>(kPID), Return(true)));
  const int kTag = 6;
  Error error;
  EXPECT_CALL(glib_,
              ChildWatchAdd(
                  kPID, &external_task_->OnTaskDied, external_task_.get()))
      .WillOnce(Return(kTag));
  EXPECT_FALSE(external_task_->Start(
      base::FilePath(kCommand), kCommandOptions, kCommandEnv, false, &error));
  EXPECT_EQ(Error::kInternalError, error.type());
  EXPECT_FALSE(external_task_->rpc_task_);

  error.Reset();
  EXPECT_TRUE(external_task_->Start(
      base::FilePath(kCommand), kCommandOptions, kCommandEnv, false, &error));
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(kPID, external_task_->pid_);
  EXPECT_EQ(kTag, external_task_->child_watch_tag_);
  EXPECT_NE(nullptr, external_task_->rpc_task_);
}

TEST_F(ExternalTaskTest, Stop) {
  const unsigned int kTag = 123;
  const int kPID = 123456;
  FakeUpRunningProcess(kTag, kPID);
  ExpectStop(kTag, kPID);
  external_task_->Stop();
  ASSERT_NE(nullptr, external_task_);
  VerifyStop();
}

TEST_F(ExternalTaskTest, StopNotStarted) {
  EXPECT_CALL(glib_, SourceRemove(_)).Times(0);
  EXPECT_CALL(process_killer_, Kill(_, _)).Times(0);
  external_task_->Stop();
  EXPECT_FALSE(test_rpc_task_destroyed_);
}

TEST_F(ExternalTaskTest, GetLogin) {
  string username;
  string password;
  EXPECT_CALL(*this, GetLogin(&username, &password));
  EXPECT_CALL(*this, Notify(_, _)).Times(0);
  external_task_->GetLogin(&username, &password);
}

TEST_F(ExternalTaskTest, Notify) {
  const string kReason("you may already have won!");
  const map<string, string>& kArgs{
    {"arg1", "val1"},
    {"arg2", "val2"}};
  EXPECT_CALL(*this, GetLogin(_, _)).Times(0);
  EXPECT_CALL(*this, Notify(kReason, kArgs));
  external_task_->Notify(kReason, kArgs);
}

TEST_F(ExternalTaskTest, OnTaskDied) {
  const int kPID = 99999;
  const int kExitStatus = 1;
  external_task_->child_watch_tag_ = 333;
  external_task_->pid_ = kPID;
  EXPECT_CALL(process_killer_, Kill(_, _)).Times(0);
  EXPECT_CALL(*this, TaskDiedCallback(kPID, kExitStatus));
  ExternalTask::OnTaskDied(kPID, kExitStatus, external_task_.get());
  EXPECT_EQ(0, external_task_->child_watch_tag_);
  EXPECT_EQ(0, external_task_->pid_);
}

}  // namespace shill
