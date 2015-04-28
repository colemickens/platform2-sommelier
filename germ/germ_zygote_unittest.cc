// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/germ_zygote.h"

#include <signal.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>

#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <gtest/gtest.h>

#include "germ/proto_bindings/soma_container_spec.pb.h"
#include "germ/test_util.h"

namespace germ {
namespace {

const int kTestTimeoutSeconds = 10;

class TestGermZygote : public GermZygote {
 public:
  TestGermZygote() {}
  ~TestGermZygote() {}

 private:
  // Use a plain fork so that the test can run unprivileged (since PID
  // namespaces require CAP_SYS_ADMIN, and unprivileged user namespaces are not
  // allowed from inside a chroot).
  pid_t ForkContainer(const soma::ContainerSpec& spec) override {
    return fork();
  }
};

soma::ContainerSpec ContainerSpecForTest() {
  soma::ContainerSpec spec;
  spec.set_name("test_container");
  spec.set_service_bundle_path("/path/to/bundle");
  spec.add_namespaces(soma::ContainerSpec::NEWPID);
  soma::ContainerSpec::Executable* executable;
  for (int i = 0; i < 3; ++i) {
    executable = spec.add_executables();
    executable->add_command_line("/bin/true");
    executable->set_uid(getuid());
    executable->set_gid(getgid());
  }
  return spec;
}

// TODO(rickyz): This test does not catch bugs in init process launching.
TEST(GermZygote, BasicUsage) {
  ScopedAlarm time_out(kTestTimeoutSeconds);
  PCHECK(prctl(PR_SET_CHILD_SUBREAPER, 1) == 0);

  TestGermZygote zygote;
  zygote.Start();

  const soma::ContainerSpec spec = ContainerSpecForTest();
  pid_t init_pid;

  ASSERT_TRUE(zygote.StartContainer(spec, &init_pid));

  // Use signalfd because a plain waitpid may return ECHILD if the container
  // init has not been reparented to us yet.
  sigset_t mask;
  PCHECK(sigemptyset(&mask) == 0);
  PCHECK(sigaddset(&mask, SIGCHLD) == 0);
  PCHECK(sigprocmask(SIG_BLOCK, &mask, nullptr) == 0);

  int sigfd = signalfd(-1, &mask, SFD_CLOEXEC);
  PCHECK(sigfd != -1);

  struct signalfd_siginfo siginfo;
  PCHECK(HANDLE_EINTR(read(sigfd, &siginfo, sizeof(siginfo))) ==
         sizeof(siginfo));

  // Check that we got notified for the right process.
  EXPECT_EQ(SIGCHLD, siginfo.ssi_signo);
  EXPECT_EQ(init_pid, siginfo.ssi_pid);
  EXPECT_EQ(CLD_EXITED, siginfo.ssi_code);
  EXPECT_EQ(EX_OK, siginfo.ssi_status);

  // Reap the init process.
  int status;
  PCHECK(HANDLE_EINTR(waitpid(init_pid, &status, 0)) == init_pid);
  EXPECT_TRUE(WIFEXITED(status));
  EXPECT_EQ(EX_OK, WEXITSTATUS(status));

  // Normally, the zygote would run forever - kill it now.
  ASSERT_NE(-1, zygote.pid());
  PCHECK(kill(zygote.pid(), SIGTERM) == 0);
  PCHECK(HANDLE_EINTR(waitpid(zygote.pid(), &status, 0)) == zygote.pid());

  EXPECT_TRUE(WIFSIGNALED(status));
  EXPECT_EQ(SIGTERM, WTERMSIG(status));
}

}  // namespace
}  // namespace germ
