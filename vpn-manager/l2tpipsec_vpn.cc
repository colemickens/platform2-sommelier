// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <sys/wait.h>

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_temp_dir.h"
#include "chromeos/process.h"
#include "chromeos/syslog_logging.h"
#include "gflags/gflags.h"
#include "vpn-manager/ipsec_manager.h"
#include "vpn-manager/l2tp_manager.h"

#pragma GCC diagnostic ignored "-Wstrict-aliasing"
DEFINE_string(client_cert_id, "", "PKCS#11 slot with client certificate");
DEFINE_string(client_cert_slot, "", "PKCS#11 key ID for client certificate");
DEFINE_string(psk_file, "", "File with IPsec pre-shared key");
DEFINE_string(remote_host, "", "VPN server hostname");
DEFINE_string(server_ca_file, "", "File with IPsec server CA in DER format");
DEFINE_string(server_id, "", "ID expected from server");
DEFINE_string(user_pin, "", "PKCS#11 User PIN");
#pragma GCC diagnostic error "-Wstrict-aliasing"

// True if a signal has requested termination.
static bool s_terminate_request = false;

void HandleSignal(int sig_num) {
  LOG(INFO) << "Caught signal " << sig_num;
  switch(sig_num) {
    case SIGTERM:
    case SIGINT:
      s_terminate_request = true;
      break;
    case SIGALRM:
      break;
  }
}

static void InstallSignalHandlers() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = HandleSignal;
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGALRM, &sa, NULL);
}

static void LockDownUmask() {
  // Only user and group may access configuration files we create.
  umask(S_IWGRP | S_IROTH | S_IWOTH);
}

// Run the main event loop.  The events to handle are:
// 1) timeout from poll
// 2) caught signal
// 3) stdout/err of child process ready
// 4) child process dies
static void RunEventLoop(IpsecManager* ipsec, L2tpManager* l2tp) {
  do {
    int status;
    int ipsec_poll_timeout = ipsec->Poll();
    int l2tp_poll_timeout = l2tp->Poll();
    int poll_timeout = (ipsec_poll_timeout > l2tp_poll_timeout) ?
        ipsec_poll_timeout : l2tp_poll_timeout;

    const int poll_input_count = 2;
    struct pollfd poll_inputs[poll_input_count] = {
      { ipsec->output_fd(), POLLIN },  // ipsec output
      { l2tp->output_fd(), POLLIN }  // l2tp output
    };
    int poll_result = poll(poll_inputs, poll_input_count, poll_timeout);
    if (poll_result < 0 && errno != EINTR) {
      int saved_errno = errno;
      LOG(ERROR) << "Unexpected poll error: " << saved_errno;
      return;
    }

    // Check if there are any child processes to be reaped without
    // blocking.
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (pid > 0 && (ipsec->IsChild(pid) || l2tp->IsChild(pid))) {
      LOG(WARNING) << "Child process " << pid << " stopped early";
      s_terminate_request = true;
    }

    if (poll_inputs[0].revents & POLLIN)
      ipsec->ProcessOutput();
    if (poll_inputs[1].revents & POLLIN)
      l2tp->ProcessOutput();
  } while(!ipsec->was_stopped() && !s_terminate_request);
}

int main(int argc, char* argv[]) {
  ScopedTempDir temp_dir;
  CommandLine::Init(argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  int log_flags = chromeos::kLogToSyslog;
  if (isatty(STDOUT_FILENO)) log_flags |= chromeos::kLogToStderr;
  chromeos::InitLog(log_flags);
  chromeos::OpenLog("l2tpipsec_vpn", true);
  IpsecManager ipsec;
  L2tpManager l2tp;

  LockDownUmask();

  ServiceManager::InitializeDirectories(&temp_dir);

  struct sockaddr remote_address;
  if (!ServiceManager::ResolveNameToSockAddr(FLAGS_remote_host,
                                             &remote_address)) {
    LOG(ERROR) << "Unable to resolve hostname " << FLAGS_remote_host;
    return 1;
  }

  if (!ipsec.Initialize(1,
                        remote_address,
                        FLAGS_psk_file,
                        FLAGS_server_ca_file,
                        FLAGS_server_id,
                        FLAGS_client_cert_slot,
                        FLAGS_client_cert_id,
                        FLAGS_user_pin)) {
    return 1;
  }
  if (!l2tp.Initialize(remote_address)) {
    return 1;
  }
  ServiceManager::SetLayerOrder(&ipsec, &l2tp);

  InstallSignalHandlers();
  CHECK(ipsec.Start()) << "Unable to start IPsec layer";

  RunEventLoop(&ipsec, &l2tp);

  LOG(INFO) << "Shutting down...";
  l2tp.Stop();
  return 0;
}
