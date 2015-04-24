// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/lib/soma/read_only_container_spec.h"

#include <base/logging.h>

#include "soma/proto_bindings/soma_container_spec.pb.h"

namespace soma {

namespace {

ReadOnlyContainerSpec::Namespace Translate(ContainerSpec::Namespace ns) {
  switch (ns) {
    case ContainerSpec::NEWIPC:
      return ReadOnlyContainerSpec::Namespace::NEWIPC;
    case ContainerSpec::NEWNET:
      return ReadOnlyContainerSpec::Namespace::NEWNET;
    case ContainerSpec::NEWNS:
      return ReadOnlyContainerSpec::Namespace::NEWNS;
    case ContainerSpec::NEWPID:
      return ReadOnlyContainerSpec::Namespace::NEWPID;
    case ContainerSpec::NEWUSER:
      return ReadOnlyContainerSpec::Namespace::NEWUSER;
    case ContainerSpec::NEWUTS:
      return ReadOnlyContainerSpec::Namespace::NEWUTS;
    default:
      NOTREACHED();
  }
  NOTREACHED();
  return ReadOnlyContainerSpec::Namespace::INVALID;
}

}  // namespace

ReadOnlyContainerSpec::Executable::Executable(
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

ReadOnlyContainerSpec::Executable::~Executable() = default;

ReadOnlyContainerSpec::ReadOnlyContainerSpec() =  default;

ReadOnlyContainerSpec::~ReadOnlyContainerSpec() = default;

bool ReadOnlyContainerSpec::Init(const ContainerSpec& spec) {
  Clear();

  name_ = spec.name();
  service_bundle_path_ = base::FilePath(spec.service_bundle_path());
  if (name_.empty() || service_bundle_path_.empty()) {
    LOG(ERROR) << "Neither service_bundle_path nor name can be empty!";
    return false;
  }
  service_names_.assign(std::begin(spec.service_names()),
                        std::end(spec.service_names()));

  // Sadly, the proto2 generated code for a repeated Enum field provides
  // only iterators over <int> not over the proper type :-/
  // Accessing the elements by index gives me back a properly typed enum.
  for (int i = 0; i < spec.namespaces_size(); ++i)
    namespaces_.push_back(Translate(spec.namespaces(i)));
  for (const auto& filter : spec.device_path_filters())
    device_path_filters_.push_back(base::FilePath(filter.filter()));
  for (const auto& filter : spec.device_node_filters()) {
    device_node_filters_.push_back(std::make_pair(filter.major(),
                                                  filter.minor()));
  }
  for (const auto& user_acl : spec.user_acls()) {
    user_acls_[user_acl.service_name()] =
        std::vector<uid_t>(user_acl.uids().begin(), user_acl.uids().end());
  }
  for (const auto& group_acl : spec.group_acls()) {
    group_acls_[group_acl.service_name()] =
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

    executables_.push_back(
        std::unique_ptr<Executable>(new Executable(
            std::vector<std::string>(std::begin(executable.command_line()),
                                     std::end(executable.command_line())),
            executable.uid(),
            executable.gid(),
            base::FilePath(executable.working_directory()),
            executable.tcp_listen_ports().allow_all(),
            executable.udp_listen_ports().allow_all(),
            std::vector<uint32_t>(
                std::begin(executable.tcp_listen_ports().ports()),
                std::end(executable.tcp_listen_ports().ports())),
            std::vector<uint32_t>(
                std::begin(executable.udp_listen_ports().ports()),
                std::end(executable.udp_listen_ports().ports())))));
  }
  if (executables_.size() < 1) {
    LOG(ERROR) << "All ContainerSpecs must define at least one executable!";
    return false;
  }
  return true;
}

void ReadOnlyContainerSpec::Clear() {
  name_.clear();
  service_bundle_path_.clear();
  service_names_.clear();
  namespaces_.clear();
  device_path_filters_.clear();
  device_node_filters_.clear();
  executables_.clear();
}

// static
const std::vector<uid_t> ReadOnlyContainerSpec::empty_user_acl_;
// static
const std::vector<gid_t> ReadOnlyContainerSpec::empty_group_acl_;

}  // namespace soma
