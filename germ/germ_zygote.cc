// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/germ_zygote.h"

#include <sched.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/scoped_vector.h>
#include <base/posix/eintr_wrapper.h>
#include <base/posix/unix_domain_socket_linux.h>
#include <base/pickle.h>
#include <base/process/launch.h>
#include <base/strings/string_piece.h>

#include "germ/germ_init.h"
#include "germ/proto_bindings/soma_sandbox_spec.pb.h"

namespace germ {

namespace {
static const char kZygoteChildPingMessage[] = "CHILD_PING";
// TODO(rickyz) Is it reasonable to have a hard limit for the SandboxSpecs +
// pickle size?
static const size_t kZygoteMaxMessageLength = 8192;
static const char kSysFsCgroupDir[] = "/sys/fs/cgroup";
}  // namespace

GermZygote::GermZygote() : zygote_pid_(-1) {}
GermZygote::~GermZygote() {}

void GermZygote::Start() {
  int fds[2];
  PCHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);

  client_fd_.reset(fds[0]);
  server_fd_.reset(fds[1]);

  zygote_pid_ = fork();
  PCHECK(zygote_pid_ != -1);

  if (zygote_pid_ == 0) {
    client_fd_.reset();
    if (!SetUpCgroups()) {
      // TODO(jorgelo): Make this fatal.
      LOG(ERROR) << "Failed to set up cgroups";
    }
    HandleRequests();
    NOTREACHED();
  }

  server_fd_.reset();
}

void GermZygote::HandleRequests() {
  while (true) {
    // Yes, this is yet another hand-rolled protobuf RPC protocol :-( We're
    // doing this because we do not want to fork in the presence of binder.
    char buf[kZygoteMaxMessageLength];
    ScopedVector<base::ScopedFD> ipc_fds;
    ssize_t len = base::UnixDomainSocket::RecvMsg(server_fd_.get(), buf,
                                                  sizeof(buf), &ipc_fds);

    if (len == 0 || (len == -1 && errno == ECONNRESET)) {
      _exit(0);
    }

    if (len == -1) {
      PLOG(ERROR) << "RecvMsg failed.";
      continue;
    }

    if (ipc_fds.size() != 1) {
      LOG(ERROR) << "Expected one IPC fd, received: " << ipc_fds.size();
      continue;
    }

    base::Pickle pickle(buf, len);
    base::PickleIterator iter(pickle);

    base::StringPiece serialized_spec;
    if (!iter.ReadStringPiece(&serialized_spec)) {
      LOG(ERROR) << "Failed to parse serialized spec from pickle.";
    }

    soma::SandboxSpec spec;
    if (!spec.ParseFromArray(serialized_spec.data(), serialized_spec.size())) {
      LOG(ERROR) << "Failed to parse spec.";
      continue;
    }

    const int ipc_fd = ipc_fds[0]->get();
    SpawnContainer(spec, ipc_fd);
  }

  NOTREACHED();
  _exit(1);
}

bool GermZygote::StartContainer(const soma::SandboxSpec& spec, pid_t* pid) {
  std::string serialized_spec;
  if (!spec.SerializeToString(&serialized_spec)) {
    LOG(ERROR) << "Failed to serialize spec.";
    return false;
  }

  base::Pickle pickle;
  if (!pickle.WriteString(serialized_spec)) {
    LOG(ERROR) << "Failed to pickle serialized spec.";
    return false;
  }

  int ipc_fds[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, ipc_fds) != 0) {
    PLOG(ERROR) << "socketpair failed";
    return false;
  }

  // TODO(rickyz): These can CHECK fail.
  base::ScopedFD client_fd(ipc_fds[0]);
  base::ScopedFD server_fd(ipc_fds[1]);
  PCHECK(base::UnixDomainSocket::EnableReceiveProcessId(client_fd.get()));

  std::vector<int> fds = {server_fd.get()};
  if (!base::UnixDomainSocket::SendMsg(client_fd_.get(), pickle.data(),
                                       pickle.size(), fds)) {
    PLOG(ERROR) << "SendMsg failed (spec)";
    return false;
  }

  server_fd.reset();

  ScopedVector<base::ScopedFD> dummy_fds;
  char ping_message_buf[sizeof(kZygoteChildPingMessage)];
  if (!base::UnixDomainSocket::RecvMsgWithPid(client_fd.get(), ping_message_buf,
                                              sizeof(ping_message_buf),
                                              &dummy_fds, pid)) {
    PLOG(ERROR) << "RecvMsgWithPid failed (ping message)";
    return false;
  }

  if (memcmp(ping_message_buf, kZygoteChildPingMessage,
             sizeof(ping_message_buf)) != 0) {
    LOG(ERROR) << "Received invalid ping message: " << ping_message_buf;
    return false;
  }

  return true;
}

bool GermZygote::Kill(pid_t pid, int signal) {
  if (kill(pid, signal) != 0) {
    PLOG(ERROR) << "kill(" << pid << ", " << signal << ") failed";
    return false;
  }

  return true;
}

pid_t GermZygote::ForkContainer(const soma::SandboxSpec& spec) {
  unsigned long flags = SIGCHLD;  // NOLINT(runtime/int)
  for (const int ns_int : spec.namespaces()) {
    const soma::SandboxSpec::Namespace ns =
        static_cast<soma::SandboxSpec::Namespace>(ns_int);
    switch (ns) {
      case soma::SandboxSpec::NEWIPC:
        flags |= CLONE_NEWIPC;
        break;
      case soma::SandboxSpec::NEWNET:
        flags |= CLONE_NEWNET;
        break;
      case soma::SandboxSpec::NEWNS:
        flags |= CLONE_NEWNS;
        break;
      case soma::SandboxSpec::NEWPID:
        flags |= CLONE_NEWPID;
        break;
      case soma::SandboxSpec::NEWUSER:
        flags |= CLONE_NEWUSER;
        break;
      case soma::SandboxSpec::NEWUTS:
        flags |= CLONE_NEWUTS;
        break;
      default:
        LOG(FATAL) << "Invalid namespace type for: " << spec.name();
        break;
    }
  }

  if (!(flags & CLONE_NEWPID)) {
    LOG(WARNING) << "PID namespace missing from SandboxSpec, enabling anyway: "
                 << spec.name();
    flags |= CLONE_NEWPID;
  }
  if (!(flags & CLONE_NEWNS)) {
    LOG(WARNING)
        << "Mount namespace missing from SandboxSpec, enabling anyway: "
        << spec.name();
    flags |= CLONE_NEWNS;
  }

  PCHECK(setsid() != -1);
  return base::ForkWithFlags(flags, nullptr, nullptr);
}

bool GermZygote::SetUpCgroups() {
  base::FilePath cgroup(kSysFsCgroupDir);
  if (!base::PathExists(cgroup)) {
    // The system should be creating this path.
    LOG(ERROR) << kSysFsCgroupDir << " does not exist";
    return false;
  }

  base::FilePath cpuacct = cgroup.Append("cpuacct");
  std::string cpuacct_path = cpuacct.MaybeAsASCII();
  if (mkdir(cpuacct_path.c_str(), 0700) == -1) {
    // Don't fail if the path already exists.
    if (errno != EEXIST) {
      PLOG(ERROR) << "mkdir(" << cpuacct_path << ")";
      return false;
    }
  }

  if (mount("cgroup", cpuacct_path.c_str(), "cgroup",
            MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RELATIME, "cpuacct") == -1) {
    if (errno != EBUSY) {
      PLOG(ERROR) << "mount(" << cpuacct_path << ")";
      return false;
    }
    PLOG(INFO) << "mount(" << cpuacct_path << ")";
  }
  return true;
}

void GermZygote::SpawnContainer(const soma::SandboxSpec& spec, int client_fd) {
  const pid_t pid = fork();
  PCHECK(pid != -1);

  // Double-fork and exit in the parent. This gives up ownership of the
  // container init process.
  if (pid == 0) {
    server_fd_.reset();

    const pid_t init_pid = ForkContainer(spec);
    PCHECK(init_pid != -1);

    if (init_pid == 0) {
      // Send a ping back so that the requester can obtain our PID.
      CHECK(base::UnixDomainSocket::SendMsg(client_fd, kZygoteChildPingMessage,
                                            sizeof(kZygoteChildPingMessage),
                                            std::vector<int>()));
      PCHECK(IGNORE_EINTR(close(client_fd)) == 0);

      GermInit init(spec);
      _exit(init.Run());
    }

    _exit(0);
  }

  // TODO(rickyz): We may want to be more paranoid about not CHECK failing in
  // the Germ zygote, since it is a critical system process.
  int status;
  PCHECK(waitpid(pid, &status, 0) == pid);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    LOG(ERROR) << "Received unexpected exit status from first fork: " << status;
  }
}

}  // namespace germ
