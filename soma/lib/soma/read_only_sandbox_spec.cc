// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/lib/soma/read_only_sandbox_spec.h"

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/time/time.h>

#include "soma/proto_bindings/soma_sandbox_spec.pb.h"

namespace soma {

namespace {

ReadOnlySandboxSpec::Namespace Translate(SandboxSpec::Namespace ns) {
  switch (ns) {
    case SandboxSpec::NEWIPC:
      return ReadOnlySandboxSpec::Namespace::NEWIPC;
    case SandboxSpec::NEWNET:
      return ReadOnlySandboxSpec::Namespace::NEWNET;
    case SandboxSpec::NEWNS:
      return ReadOnlySandboxSpec::Namespace::NEWNS;
    case SandboxSpec::NEWPID:
      return ReadOnlySandboxSpec::Namespace::NEWPID;
    case SandboxSpec::NEWUSER:
      return ReadOnlySandboxSpec::Namespace::NEWUSER;
    case SandboxSpec::NEWUTS:
      return ReadOnlySandboxSpec::Namespace::NEWUTS;
    default:
      NOTREACHED();
  }
  NOTREACHED();
  return ReadOnlySandboxSpec::Namespace::INVALID;
}

bool AbsolutePathExistsAndIsExecutable(const std::string& exe_name) {
  base::FilePath exe_path(exe_name);
  int permissions = 00;
  return (exe_path.IsAbsolute() && base::PathExists(exe_path) &&
          base::GetPosixFilePermissions(exe_path, &permissions) &&
          (permissions & base::FILE_PERMISSION_EXECUTE_BY_USER));
}

}  // namespace

ReadOnlySandboxSpec::Executable::Executable(
    std::vector<std::string> command_line_in,
    uid_t uid_in,
    gid_t gid_in,
    base::FilePath working_directory_in,
    bool all_tcp_ports_allowed_in,
    bool all_udp_ports_allowed_in,
    std::vector<uint32_t> tcp_listen_ports_in,
    std::vector<uint32_t> udp_listen_ports_in)
    : command_line(std::move(command_line_in)),
      uid(uid_in),
      gid(gid_in),
      working_directory(std::move(working_directory_in)),
      all_tcp_ports_allowed(all_tcp_ports_allowed_in),
      all_udp_ports_allowed(all_udp_ports_allowed_in),
      tcp_listen_ports(std::move(tcp_listen_ports_in)),
      udp_listen_ports(std::move(udp_listen_ports_in)) {
}

ReadOnlySandboxSpec::Executable::~Executable() = default;

ReadOnlySandboxSpec::ReadOnlySandboxSpec() = default;

ReadOnlySandboxSpec::~ReadOnlySandboxSpec() = default;

bool ReadOnlySandboxSpec::Init(const SandboxSpec& spec) {
  Clear();

  name_ = spec.name();
  overlay_path_ = base::FilePath(spec.overlay_path());
  if (name_.empty() || overlay_path_.empty()) {
    LOG(ERROR) << "Neither overlay_path nor name can be empty!";
    return false;
  }
  endpoint_names_.assign(std::begin(spec.endpoint_names()),
                         std::end(spec.endpoint_names()));

  // Sadly, the proto2 generated code for a repeated Enum field provides
  // only iterators over <int> not over the proper type :-/
  // Accessing the elements by index gives me back a properly typed enum.
  for (int i = 0; i < spec.namespaces_size(); ++i)
    namespaces_.push_back(Translate(spec.namespaces(i)));
  for (const auto& filter : spec.device_path_filters())
    device_path_filters_.push_back(base::FilePath(filter.filter()));
  for (const auto& filter : spec.device_node_filters()) {
    device_node_filters_.push_back(
        std::make_pair(filter.major(), filter.minor()));
  }
  for (const auto& user_acl : spec.user_acls()) {
    user_acls_[user_acl.endpoint_name()] =
        std::vector<uid_t>(user_acl.uids().begin(), user_acl.uids().end());
  }
  for (const auto& group_acl : spec.group_acls()) {
    group_acls_[group_acl.endpoint_name()] =
        std::vector<gid_t>(group_acl.gids().begin(), group_acl.gids().end());
  }

  executables_.reserve(spec.executables_size());
  for (const auto& executable : spec.executables()) {
    if (!executable.has_uid() || !executable.has_gid()) {
      LOG(ERROR) << "All executables must define a uid and gid!";
      return false;
    }
    if (executable.command_line_size() == 0) {
      LOG(ERROR) << "All executables must define a non-empty command line!";
      return false;
    }
    if (!AbsolutePathExistsAndIsExecutable(executable.command_line(0))) {
      LOG(ERROR) << "Command line must reference an existing executable "
                 << "by absolute path: " << executable.command_line(0);
      return false;
    }

    executables_.push_back(std::unique_ptr<Executable>(new Executable(
        std::vector<std::string>(std::begin(executable.command_line()),
                                 std::end(executable.command_line())),
        executable.uid(), executable.gid(),
        base::FilePath(executable.working_directory()),
        executable.tcp_listen_ports().allow_all(),
        executable.udp_listen_ports().allow_all(),
        std::vector<uint32_t>(std::begin(executable.tcp_listen_ports().ports()),
                              std::end(executable.tcp_listen_ports().ports())),
        std::vector<uint32_t>(
            std::begin(executable.udp_listen_ports().ports()),
            std::end(executable.udp_listen_ports().ports())))));
  }
  if (executables_.size() < 1) {
    LOG(ERROR) << "All SandboxSpecs must define at least one executable!";
    return false;
  }

  shutdown_timeout_ =
      base::TimeDelta::FromMilliseconds(spec.shutdown_timeout_ms());
  if (shutdown_timeout_ < base::TimeDelta()) {
    return false;
  }

  return true;
}

void ReadOnlySandboxSpec::Clear() {
  name_.clear();
  overlay_path_.clear();
  endpoint_names_.clear();
  namespaces_.clear();
  device_path_filters_.clear();
  device_node_filters_.clear();
  executables_.clear();
}

// static
const std::vector<uid_t> ReadOnlySandboxSpec::empty_user_acl_;
// static
const std::vector<gid_t> ReadOnlySandboxSpec::empty_group_acl_;

}  // namespace soma
