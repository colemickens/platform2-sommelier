// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager.h"

#include <errno.h>
#include <gtest/gtest.h>
#include <signal.h>
#include <unistd.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/scoped_ptr.h>

#include "login_manager/child_job.h"
#include "login_manager/file_checker.h"
#include "login_manager/ipc_channel.h"

namespace login_manager {

class SessionManagerTest : public ::testing::Test { };

// These would be better done with gmock.
class TrueChecker : public FileChecker {
 public:
  virtual bool exists(const char* filename) {
    return true;
  }
};

class FalseChecker : public FileChecker {
 public:
  virtual bool exists(const char* filename) {
    return false;
  }
};

class ThrowingJob : public ChildJob {
  void Run() { EXPECT_TRUE(false) << "Job should never run!"; }
  void Toggle() {}
};

TEST(SessionManagerTest, NoLoopTest) {
  ChildJob* job = new ThrowingJob;
  login_manager::SessionManager manager(new TrueChecker,
                                        NULL,
                                        job,
                                        false);
  manager.LoopChrome("");
  EXPECT_EQ(0, manager.num_loops());
}

class CleanExitJob : public ChildJob {
  void Run() { exit(0); }
  void Toggle() {}
};

TEST(SessionManagerTest, CleanExitTest) {
  ChildJob* job = new CleanExitJob;
  login_manager::SessionManager manager(new FalseChecker,
                                        NULL,
                                        job,
                                        false);
  manager.LoopChrome("");
  EXPECT_EQ(1, manager.num_loops());
}

class LoopOnceJob : public ChildJob {
 public:
  LoopOnceJob(const char pipe_name[]) : looped_(false), writer_(pipe_name) {}
  void Run() {
    LOG(INFO) << "Running, looped_ is " << looped_;
    if (looped_) {
      exit(0);
    } else {
      writer_.init();
      writer_.send(START_SESSION);
      exit(1);
    }
  }
  void Toggle() { looped_ = !looped_; }
  bool looped_;
  IpcWriteChannel writer_;
};

TEST(SessionManagerTest, LoopOnceTest) {
  const char pipe_name[] = "/tmp/TESTFIFO";
  ChildJob* job = new LoopOnceJob(pipe_name);
  login_manager::SessionManager manager(new FalseChecker,
                                        new IpcReadChannel(pipe_name),
                                        job,
                                        true);
  manager.LoopChrome("");
  unlink(pipe_name);
  EXPECT_EQ(2, manager.num_loops());
}

class FalseOnceChecker : public FileChecker {
 public:
  FalseOnceChecker() : to_return_(false) {}
  virtual bool exists(const char* filename) {
    if (!to_return_) {
      to_return_ = true;
      return false;
    } else {
      return to_return_;
    }
  }
 private:
  bool to_return_;
};

class BadExitJob : public ChildJob {
  void Run() { exit(1); }
  void Toggle() {}
};

TEST(SessionManagerTest, MagicFileTest) {
  ChildJob* job = new BadExitJob;
  login_manager::SessionManager manager(new FalseOnceChecker,
                                        NULL,
                                        job,
                                        false);
  manager.LoopChrome("");
  EXPECT_EQ(1, manager.num_loops());
}

IpcMessage IpcTestHelper(const std::string& pipe_name, IpcMessage expected) {
  pid_t pID = fork();
  if (pID == 0) {
    IpcWriteChannel writer(pipe_name.c_str());
    writer.init();
    writer.send(expected);
    exit(0);
  }
  IpcReadChannel reader(pipe_name.c_str());
  reader.init();
  int status;
  while (waitpid(pID, &status, 0) == -1 && errno == EINTR)
    ;
  IpcMessage incoming = reader.recv();
  return incoming;
}

TEST(SessionManagerTest, IpcTest) {
  std::string pipe_name("/tmp/TESTFIFO");
  EXPECT_EQ(EMIT_LOGIN, IpcTestHelper(pipe_name, EMIT_LOGIN));
  EXPECT_EQ(START_SESSION, IpcTestHelper(pipe_name, START_SESSION));
  EXPECT_EQ(STOP_SESSION, IpcTestHelper(pipe_name, STOP_SESSION));
  unlink(pipe_name.c_str());
}

void usr2(int sig) {
  LOG(INFO) << "got sig";
}

TEST(SessionManagerTest, IpcEofTest) {
  const char pipe_name[] = "/tmp/TESTFIFO";

  pid_t pID = fork();
  if (pID == 0) {
    IpcWriteChannel writer(pipe_name);
    writer.init();
    exit(0);
  } else {
    IpcReadChannel reader(pipe_name);
    reader.init();
    int status;
    while (waitpid(pID, &status, 0) == -1 && errno == EINTR)
      ;

    EXPECT_EQ(FAILED, reader.recv());
    EXPECT_TRUE(reader.channel_eof());

    unlink(pipe_name);
  }
}

}  // namespace login_manager
