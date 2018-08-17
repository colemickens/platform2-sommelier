// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/at_exit.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/string_util.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "portier/portier.h"

using std::string;

using portier::Portier;
using portier::Status;

namespace {

int DoManageInterface(int argc, char** argv) {
  DEFINE_string(if_name, "", "Interface name");
  brillo::FlagHelper::Init(argc, argv, "Bind Portier to interface");

  const string if_name = FLAGS_if_name;
  if (if_name.empty()) {
    LOG(ERROR) << "Missing interface name: --if_name";
    return 1;
  }

  auto client = Portier::Create();
  if (!client) {
    return 1;
  }

  const Status status = client->BindInterface(if_name);
  if (!status) {
    LOG(ERROR) << status;
    return 1;
  }
  LOG(INFO) << "OK";
  return 0;
}

int DoReleaseInterface(int argc, char** argv) {
  DEFINE_string(if_name, "", "Interface name");
  brillo::FlagHelper::Init(argc, argv, "Release proxy interface");

  const string if_name = FLAGS_if_name;
  if (if_name.empty()) {
    LOG(ERROR) << "Missing interface name: --if_name";
    return 1;
  }

  auto client = Portier::Create();
  if (!client) {
    return 1;
  }

  const Status status = client->ReleaseInterface(if_name);
  if (!status) {
    LOG(ERROR) << status;
    return 1;
  }
  LOG(INFO) << "OK";
  return 0;
}

int DoCreateProxyGroup(int argc, char** argv) {
  DEFINE_string(group_name, "", "Proxy group name");
  brillo::FlagHelper::Init(argc, argv, "Create proxy group");

  const string pg_name = FLAGS_group_name;
  if (pg_name.empty()) {
    LOG(ERROR) << "Missing interface name: --group_name";
    return 1;
  }

  auto client = Portier::Create();
  if (!client) {
    return 1;
  }

  const Status status = client->CreateProxyGroup(pg_name);
  if (!status) {
    LOG(ERROR) << status;
    return 1;
  }
  LOG(INFO) << "OK";
  return 0;
}

int DoReleaseProxyGroup(int argc, char** argv) {
  DEFINE_string(group_name, "", "Proxy group name");
  brillo::FlagHelper::Init(argc, argv, "Release proxy group");

  const string pg_name = FLAGS_group_name;
  if (pg_name.empty()) {
    LOG(ERROR) << "Missing interface name: --group_name";
    return 1;
  }

  auto client = Portier::Create();
  if (!client) {
    return 1;
  }

  const Status status = client->ReleaseProxyGroup(pg_name);
  if (!status) {
    LOG(ERROR) << status;
    return 1;
  }
  LOG(INFO) << "OK";
  return 0;
}

int DoAddToGroup(int argc, char** argv) {
  DEFINE_string(if_name, "", "Interface name");
  DEFINE_string(group_name, "", "Proxy group name");
  DEFINE_bool(as_upstream, false, "Enabled interface as upstream");
  brillo::FlagHelper::Init(argc, argv, "Add interface to proxy group");

  const string if_name = FLAGS_if_name;
  if (if_name.empty()) {
    LOG(ERROR) << "Missing interface name: --if_name";
    return 1;
  }

  const string pg_name = FLAGS_group_name;
  if (pg_name.empty()) {
    LOG(ERROR) << "Missing interface name: --group_name";
    return 1;
  }

  const bool as_upstream = FLAGS_as_upstream;

  auto client = Portier::Create();
  if (!client) {
    return 1;
  }

  const Status status = client->AddToGroup(if_name, pg_name, as_upstream);
  if (!status) {
    LOG(ERROR) << status;
    return 1;
  }
  LOG(INFO) << "OK";
  return 0;
}

int DoRemoveFromGroup(int argc, char** argv) {
  DEFINE_string(if_name, "", "Interface name");
  brillo::FlagHelper::Init(argc, argv, "Remove interface from its group");

  const string if_name = FLAGS_if_name;
  if (if_name.empty()) {
    LOG(ERROR) << "Missing interface name: --if_name";
    return 1;
  }

  auto client = Portier::Create();
  if (!client) {
    return 1;
  }

  const Status status = client->RemoveFromGroup(if_name);
  if (!status) {
    LOG(ERROR) << status;
    return 1;
  }
  LOG(INFO) << "OK";
  return 0;
}

int DoSetUpstream(int argc, char** argv) {
  DEFINE_string(if_name, "", "Interface name");
  brillo::FlagHelper::Init(argc, argv, "Set interface as upstream");

  const string if_name = FLAGS_if_name;
  if (if_name.empty()) {
    LOG(ERROR) << "Missing interface name: --if_name";
    return 1;
  }

  auto client = Portier::Create();
  if (!client) {
    return 1;
  }

  const Status status = client->SetUpstream(if_name);
  if (!status) {
    LOG(ERROR) << status;
    return 1;
  }
  LOG(INFO) << "OK";
  return 0;
}

int DoUnsetUpstream(int argc, char** argv) {
  DEFINE_string(group_name, "", "Proxy group name");
  brillo::FlagHelper::Init(argc, argv, "Unset proxy group's upstream");

  const string pg_name = FLAGS_group_name;
  if (pg_name.empty()) {
    LOG(ERROR) << "Missing interface name: --group_name";
    return 1;
  }

  auto client = Portier::Create();
  if (!client) {
    return 1;
  }

  const Status status = client->UnsetUpstream(pg_name);
  if (!status) {
    LOG(ERROR) << status;
    return 1;
  }
  LOG(INFO) << "OK";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  brillo::InitLog(brillo::kLogToStderrIfTty);

  if (argc == 1) {
    LOG(ERROR) << "Missing comment";
    LOG(INFO) << "usage: " << argv[0]
              << " { bind-if | release-if | create-group | release-group | "
                 "add-if | remove-if | set-upstream | unset-upstream }";
    return 1;
  }

  base::MessageLoopForIO message_loop;
  const string command = argv[1];

  if (base::LowerCaseEqualsASCII(command, "bind-if")) {
    return DoManageInterface(argc - 1, argv + 1);
  }
  if (base::LowerCaseEqualsASCII(command, "release-if")) {
    return DoReleaseInterface(argc - 1, argv + 1);
  }
  if (base::LowerCaseEqualsASCII(command, "create-group")) {
    return DoCreateProxyGroup(argc - 1, argv + 1);
  }
  if (base::LowerCaseEqualsASCII(command, "release-group")) {
    return DoReleaseProxyGroup(argc - 1, argv + 1);
  }
  if (base::LowerCaseEqualsASCII(command, "add-if")) {
    return DoAddToGroup(argc - 1, argv + 1);
  }
  if (base::LowerCaseEqualsASCII(command, "remove-if")) {
    return DoRemoveFromGroup(argc - 1, argv + 1);
  }
  if (base::LowerCaseEqualsASCII(command, "set-upstream")) {
    return DoSetUpstream(argc - 1, argv + 1);
  }
  if (base::LowerCaseEqualsASCII(command, "unset-upstream")) {
    return DoUnsetUpstream(argc - 1, argv + 1);
  }

  LOG(ERROR) << "Unknown command " << command;
  return 1;
}
