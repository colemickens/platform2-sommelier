// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/lib/soma/read_only_container_spec.h"

#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
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

ReadOnlyContainerSpec::ReadOnlyContainerSpec(const ContainerSpec* spec)
    : internal_(spec),
      service_bundle_path_(internal_->service_bundle_path()),
      command_line_(internal_->command_line().begin(),
                    internal_->command_line().end()),
      service_names_(internal_->service_names().begin(),
                     internal_->service_names().end()),
      working_dir_(internal_->working_directory()),
      tcp_listen_ports_(internal_->tcp_listen_ports().ports().begin(),
                        internal_->tcp_listen_ports().ports().end()),
      udp_listen_ports_(internal_->udp_listen_ports().ports().begin(),
                        internal_->udp_listen_ports().ports().end()) {
  // Sadly, the proto2 generated code for a repeated Enum field provides
  // only iterators over <int> not over the proper type :-/
  // Accessing the elements by index gives me back a properly typed enum.
  for (int i = 0; i < internal_->namespaces_size(); ++i)
    namespaces_.push_back(Translate(internal_->namespaces(i)));
  for (const auto& filter : internal_->device_path_filters())
    device_path_filters_.push_back(base::FilePath(filter.filter()));
  for (const auto& filter : internal_->device_node_filters()) {
    device_node_filters_.push_back(std::make_pair(filter.major(),
                                                  filter.minor()));
  }
}

ReadOnlyContainerSpec::~ReadOnlyContainerSpec() {}

const std::string& ReadOnlyContainerSpec::name() const {
  return internal_->name();
}

uid_t ReadOnlyContainerSpec::uid() const { return internal_->uid(); }

gid_t ReadOnlyContainerSpec::gid() const { return internal_->gid(); }

bool ReadOnlyContainerSpec::all_tcp_ports_allowed() const {
  return internal_->tcp_listen_ports().allow_all();
}

bool ReadOnlyContainerSpec::all_udp_ports_allowed() const {
  return internal_->udp_listen_ports().allow_all();
}

}  // namespace soma
