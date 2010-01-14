// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_service.h"

#include <errno.h>
#include <gtest/gtest.h>
#include <signal.h>
#include <unistd.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/scoped_ptr.h>
#include <chromeos/dbus/dbus.h>

#include "login_manager/bindings/client.h"
#include "login_manager/child_job.h"
#include "login_manager/constants.h"
#include "login_manager/file_checker.h"

namespace login_manager {

class SessionManagerTest : public ::testing::Test { };

// These would be better done with gmock.
class TrueChecker : public FileChecker {
 public:
  TrueChecker() : FileChecker("") {}
  virtual bool exists() {
    return true;
  }
};

class ThrowingJob : public ChildJob {
  bool ShouldRun() { return false; }
  void Run() { EXPECT_TRUE(false) << "Job should never run!"; }
  void Toggle() {}
};

/*
TEST(SessionManagerTest, NoLoopTest) {
  ChildJob* job = new ThrowingJob;
  login_manager::SessionManager manager(NULL,
                                        job,  // manager takes ownership
                                        false);
  manager.LoopChrome("");
}
*/

class RunEnJob : public ChildJob {
 public:
  RunEnJob(int desired_runs)
      : desired_runs_(desired_runs),
        num_loops_(0) {
  }
  bool ShouldRun() {
    ++num_loops_;
    if (num_loops_ >= desired_runs_) {
      return false;
    } else {
      return true;
    }
  }
  void Toggle() {}
  int num_loops() { return num_loops_; }
 private:
  const int desired_runs_;
  int num_loops_;
};

class CleanExitJob : public RunEnJob {
 public:
  CleanExitJob(int runs) : RunEnJob(runs) {}
  void Run() { exit(0); }
};

class BadExitJob : public RunEnJob {
 public:
  BadExitJob(int runs) : RunEnJob(runs) {}
  void Run() { exit(1); }
};

TEST(SessionManagerTest, CleanExitTest) {
  CleanExitJob* job = new CleanExitJob(1);
  login_manager::SessionManagerService manager(job,  // manager takes ownership
                                               true);
  manager.Run();
  EXPECT_EQ(1, job->num_loops());
}

TEST(SessionManagerTest, BadExitTest) {
  BadExitJob* job = new BadExitJob(1);
  login_manager::SessionManagerService manager(job,  // manager takes ownership
                                               true);
  manager.Run();
  EXPECT_EQ(1, job->num_loops());
}

TEST(SessionManagerTest, MultiRunTest) {
  int runs = 3;
  CleanExitJob* job = new CleanExitJob(runs);
  login_manager::SessionManagerService manager(job,  // manager takes ownership
                                               true);
  manager.Run();
  EXPECT_EQ(3, job->num_loops());
}

class EnforceToggleJob : public ChildJob {
 public:
  EnforceToggleJob() : was_toggled_(false) {}
  ~EnforceToggleJob() { EXPECT_TRUE(was_toggled_); }
  bool ShouldRun() { return true; }
  void Run() { }
  void Toggle() { EXPECT_FALSE(was_toggled_); was_toggled_ = true; }
 private:
  bool was_toggled_;
};

TEST(SessionManagerTest, StartSessionTest) {
  ChildJob* job = new EnforceToggleJob;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  gboolean out;
  manager.StartSession(NULL, NULL, &out, NULL);
}

TEST(SessionManagerTest, StopSessionTest) {
  ChildJob* job = new EnforceToggleJob;
  login_manager::SessionManagerService manager(job);  // manager takes ownership
  gboolean out;
  manager.StopSession(NULL, &out, NULL);
}

}  // namespace login_manager
