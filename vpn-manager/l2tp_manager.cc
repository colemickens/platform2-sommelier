// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vpn-manager/l2tp_manager.h"

#include <fcntl.h>
#include <stdlib.h>  // for setenv()
#include <sys/stat.h>
#include <sys/types.h>

#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chromeos/process.h"
#include "gflags/gflags.h"

#pragma GCC diagnostic ignored "-Wstrict-aliasing"
DEFINE_bool(defaultroute, true, "defaultroute");
DEFINE_bool(length_bit, true, "length bit");
DEFINE_bool(require_chap, true, "require chap");
DEFINE_bool(refuse_pap, false, "refuse chap");
DEFINE_bool(require_authentication, true, "require authentication");
DEFINE_string(password, "", "password (insecure - use pppd plugin instead)");
DEFINE_bool(ppp_debug, true, "ppp debug");
DEFINE_int32(ppp_setup_timeout, 10, "timeout to setup ppp (seconds)");
DEFINE_string(pppd_plugin, "", "pppd plugin");
DEFINE_bool(usepeerdns, true, "usepeerdns - ask peer for DNS");
DEFINE_string(user, "", "user name");
DEFINE_bool(systemconfig, true, "enable ppp to configure IPs/routes/DNS");
#pragma GCC diagnostic error "-Wstrict-aliasing"

const char kL2tpConnectionName[] = "managed";
// Environment variable available to ppp plugin to know the resolved address
// of the L2TP server.
const char kLnsAddress[] = "LNS_ADDRESS";
const char kPppInterfacePath[] = "/sys/class/net/ppp0";
const char kPppLogPrefix[] = "pppd: ";
const char kPppAuthenticationFailurePattern[] = "*authentication failed*";

using ::chromeos::ProcessImpl;

L2tpManager::L2tpManager()
    : ServiceManager("l2tp"),
      was_initiated_(false),
      output_fd_(-1),
      ppp_output_fd_(-1),
      ppp_interface_path_(kPppInterfacePath),
      l2tpd_(new ProcessImpl) {
}

bool L2tpManager::Initialize(const struct sockaddr& remote_address) {
  if (!ConvertSockAddrToIPString(remote_address, &remote_address_text_)) {
    LOG(ERROR) << "Unable to convert sockaddr to name for remote host";
    RegisterError(kServiceErrorInternal);
    return false;
  }
  remote_address_ = remote_address;

  if (FLAGS_user.empty()) {
    LOG(ERROR) << "l2tp layer requires user name";
    RegisterError(kServiceErrorInvalidArgument);
    return false;
  }
  if (!FLAGS_pppd_plugin.empty() &&
      !file_util::PathExists(FilePath(FLAGS_pppd_plugin))) {
    LOG(WARNING) << "pppd_plugin (" << FLAGS_pppd_plugin << ") does not exist";
  }
  if (!FLAGS_password.empty()) {
    LOG(WARNING) << "Passing a password on the command-line is insecure";
  }
  return true;
}

static void AddString(std::string* config, const char* key,
                      const std::string& value) {
  config->append(StringPrintf("%s = %s\n", key, value.c_str()));
}

static void AddBool(std::string* config, const char* key, bool value) {
  config->append(StringPrintf("%s = %s\n", key, value ? "yes" : "no"));
}

bool L2tpManager::CreatePppLogFifo() {
  ppp_output_path_ = temp_path()->Append("pppd.log");
  const char* fifo_path = ppp_output_path_.value().c_str();
  if (HANDLE_EINTR(mkfifo(fifo_path, S_IRUSR | S_IWUSR)) == 0) {
    ppp_output_fd_ = HANDLE_EINTR(open(fifo_path, O_RDONLY | O_NONBLOCK));
    if (ppp_output_fd_ != -1)
      return true;
  }
  return false;
}

std::string L2tpManager::FormatL2tpdConfiguration(
    const std::string& ppp_config_path) {
  std::string l2tpd_config;
  l2tpd_config.append(StringPrintf("[lac %s]\n", kL2tpConnectionName));
  AddString(&l2tpd_config, "lns", remote_address_text_);
  AddBool(&l2tpd_config, "require chap", FLAGS_require_chap);
  AddBool(&l2tpd_config, "refuse pap", FLAGS_refuse_pap);
  AddBool(&l2tpd_config, "require authentication",
          FLAGS_require_authentication);
  AddString(&l2tpd_config, "name", FLAGS_user);
  if (debug()) {
    AddBool(&l2tpd_config, "ppp debug", FLAGS_ppp_debug);
  }
  AddString(&l2tpd_config, "pppoptfile", ppp_config_path);
  AddBool(&l2tpd_config, "length bit", FLAGS_length_bit);
  return l2tpd_config;
}

std::string L2tpManager::FormatPppdConfiguration() {
  std::string pppd_config(
      "ipcp-accept-local\n"
      "ipcp-accept-remote\n"
      "refuse-eap\n"
      "noccp\n"
      "noauth\n"
      "crtscts\n"
      "idle 1800\n"
      "mtu 1410\n"
      "mru 1410\n"
      "lock\n"
      "connect-delay 5000\n");
  pppd_config.append(StringPrintf("%sdefaultroute\n",
                                  FLAGS_defaultroute ? "" : "no"));
  if (ppp_output_fd_ != -1) {
    pppd_config.append(StringPrintf("logfile %s\n",
                                    ppp_output_path_.value().c_str()));
  }
  if (FLAGS_usepeerdns) {
    pppd_config.append("usepeerdns\n");
  }
  if (!FLAGS_systemconfig) {
    // nosystemconfig is only supported by the chromiumos patched
    // version of pppd.
    pppd_config.append("nosystemconfig\n");
  }
  if (!FLAGS_pppd_plugin.empty()) {
    DLOG(INFO) << "Using pppd plugin " << FLAGS_pppd_plugin;
    pppd_config.append(StringPrintf("plugin %s\n", FLAGS_pppd_plugin.c_str()));
  }
  if (debug()) {
    pppd_config.append("debug\n");
  }
  return pppd_config;
}

bool L2tpManager::Initiate() {
  std::string control_string;
  control_string = StringPrintf("c %s", kL2tpConnectionName);
  if (FLAGS_pppd_plugin.empty()) {
    control_string.append(StringPrintf(" %s %s\n",
                                       FLAGS_user.c_str(),
                                       FLAGS_password.c_str()));
  } else {
    // otherwise the plugin must specify username and password.
    control_string.append("\n");
  }
  if (!file_util::WriteFile(l2tpd_control_path_, control_string.c_str(),
                            control_string.size())) {
    return false;
  }
  was_initiated_ = true;
  return true;
}

bool L2tpManager::Terminate() {
  std::string control_string = StringPrintf("d %s\n",
                                            kL2tpConnectionName);
  if (!file_util::WriteFile(l2tpd_control_path_, control_string.c_str(),
                            control_string.size())) {
    return false;
  }
  return true;
}

bool L2tpManager::Start() {
  FilePath pppd_config_path = temp_path()->Append("pppd.conf");
  std::string l2tpd_config = FormatL2tpdConfiguration(pppd_config_path.value());
  FilePath l2tpd_config_path = temp_path()->Append("l2tpd.conf");
  if (!file_util::WriteFile(l2tpd_config_path, l2tpd_config.c_str(),
                            l2tpd_config.size())) {
    LOG(ERROR) << "Unable to write l2tpd config to "
               << l2tpd_config_path.value();
    RegisterError(kServiceErrorInternal);
    return false;
  }

  if (!CreatePppLogFifo()) {
    PLOG(ERROR) << "Unable to create ppp log fifo";
    RegisterError(kServiceErrorInternal);
    return false;
  }
  std::string pppd_config = FormatPppdConfiguration();
  if (!file_util::WriteFile(pppd_config_path, pppd_config.c_str(),
                            pppd_config.size())) {
    LOG(ERROR) << "Unable to write pppd config to " << pppd_config_path.value();
    RegisterError(kServiceErrorInternal);
    return false;
  }
  l2tpd_control_path_ = temp_path()->Append("l2tpd.control");
  file_util::Delete(l2tpd_control_path_, false);

  if (!FLAGS_pppd_plugin.empty()) {
    // Pass the resolved LNS address to the plugin.
    setenv(kLnsAddress, remote_address_text_.c_str(), 1);
  }

  l2tpd_->Reset(0);
  l2tpd_->AddArg(L2TPD);
  l2tpd_->AddStringOption("-c", l2tpd_config_path.value());
  l2tpd_->AddStringOption("-C", l2tpd_control_path_.value());
  l2tpd_->AddArg("-D");
  l2tpd_->RedirectUsingPipe(STDERR_FILENO, false);
  l2tpd_->Start();
  output_fd_ = l2tpd_->GetPipe(STDERR_FILENO);
  start_ticks_ = base::TimeTicks::Now();
  return true;
}

int L2tpManager::Poll() {
  if (is_running()) return -1;
  if (start_ticks_.is_null()) return -1;
  if (!was_initiated_ && file_util::PathExists(l2tpd_control_path_)) {
    if (!Initiate()) {
      LOG(ERROR) << "Unable to initiate connection";
      RegisterError(kServiceErrorL2tpConnectionFailed);
      Terminate();
      OnStopped(false);
      return -1;
    }
    // With the connection initated, check if it's up in 1s.
    return 1000;
  }
  if (was_initiated_ && file_util::PathExists(FilePath(ppp_interface_path_))) {
    LOG(INFO) << "L2TP connection now up";
    OnStarted();
    return -1;
  }
  // Check for the ppp setup timeout.  This includes the time
  // to start pppd, it to set up its control file, l2tp connection
  // setup, ppp connection setup.  Authentication happens after
  // the ppp device is created.
  if (base::TimeTicks::Now() - start_ticks_ >
      base::TimeDelta::FromSeconds(FLAGS_ppp_setup_timeout)) {
    RegisterError(kServiceErrorPppConnectionFailed);
    LOG(ERROR) << "PPP setup timed out";
    // Cleanly terminate if the control file exists.
    if (was_initiated_) Terminate();
    OnStopped(false);
    // Poll in 1 second in order to check if clean shutdown worked.
  }
  return 1000;
}

void L2tpManager::ProcessOutput() {
  WriteFdToSyslog(output_fd_, "", &partial_output_line_);
}

void L2tpManager::ProcessPppOutput() {
  WriteFdToSyslog(ppp_output_fd_, kPppLogPrefix, &partial_ppp_output_line_);
}

bool L2tpManager::IsChild(pid_t pid) {
  return pid == l2tpd_->pid();
}

void L2tpManager::Stop() {
  if (l2tpd_->pid()) {
    LOG(INFO) << "Shutting down L2TP";
    Terminate();
  }
  OnStopped(false);
}

void L2tpManager::OnSyslogOutput(const std::string& prefix,
                                 const std::string& line) {
  if (prefix == kPppLogPrefix &&
      MatchPattern(line, kPppAuthenticationFailurePattern)) {
    LOG(ERROR) << "PPP authentication failed";
    RegisterError(kServiceErrorPppAuthenticationFailed);
    Stop();
  }
}
