// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/container_spec_helpers.h"

#include <sys/types.h>

#include <algorithm>
#include <limits>

#include <base/logging.h>

#include "soma/proto_bindings/soma_container_spec.pb.h"

namespace soma {
namespace parser {
namespace container_spec_helpers {
namespace {

void SetListenPorts(ContainerSpec::PortSpec* port_spec,
                    const std::set<port::Number>& listen_ports) {
  // If the wildcard port is in the set, just allow all and bail early.
  if (listen_ports.find(port::kWildcard) != listen_ports.end()) {
    port_spec->set_allow_all(true);
    return;
  }
  for (const port::Number& port : listen_ports) {
    // The parsing code should have ensured this, so just DCHECK()
    DCHECK(port <= std::numeric_limits<uint16_t>::max());
    port_spec->add_ports(static_cast<uint16_t>(port));
  }
}

}  // namespace

std::unique_ptr<ContainerSpec> CreateContainerSpec(const std::string& name) {
  std::unique_ptr<ContainerSpec> spec(new ContainerSpec);
  spec->set_name(name);
  return std::move(spec);
}

void SetServiceBundlePath(const base::FilePath& service_bundle_path,
                          ContainerSpec* to_modify) {
  to_modify->set_service_bundle_path(service_bundle_path.value());
}

void SetServiceNames(const std::vector<std::string>& service_names,
                     ContainerSpec* to_modify) {
  to_modify->clear_service_names();
  for (const std::string& name : service_names)
    to_modify->add_service_names(name);
}

void SetNamespaces(const std::set<ns::Kind>& namespaces,
                   ContainerSpec* to_modify) {
  to_modify->clear_namespaces();
  for (const ns::Kind& ns : namespaces)
    to_modify->add_namespaces(ns);
}

void SetDevicePathFilters(const DevicePathFilter::Set& filters,
                          ContainerSpec* to_modify) {
  to_modify->clear_device_path_filters();
  for (const DevicePathFilter& filter : filters)
    to_modify->add_device_path_filters()->set_filter(filter.filter().value());
}

void SetDeviceNodeFilters(const DeviceNodeFilter::Set& filters,
                          ContainerSpec* to_modify) {
  to_modify->clear_device_node_filters();
  for (const DeviceNodeFilter& parser_filter : filters) {
    ContainerSpec::DeviceNodeFilter* filter =
        to_modify->add_device_node_filters();
    filter->set_major(parser_filter.major());
    filter->set_minor(parser_filter.minor());
  }
}

void SetUserACL(const std::string& service_name,
                const std::set<uid_t>& whitelist,
                ContainerSpec* to_modify) {
  if (service_name.empty() || whitelist.empty())
    return;
  ContainerSpec::UserACL* acl = to_modify->add_user_acls();
  acl->set_service_name(service_name);
  for (uid_t uid : whitelist)
    acl->add_uids(uid);
}

void SetGroupACL(const std::string& service_name,
                 const std::set<gid_t>& whitelist,
                 ContainerSpec* to_modify) {
  if (service_name.empty() || whitelist.empty())
    return;
  ContainerSpec::GroupACL* acl = to_modify->add_group_acls();
  acl->set_service_name(service_name);
  for (gid_t gid : whitelist)
    acl->add_gids(gid);
}

void SetUidAndGid(uid_t uid, gid_t gid, ContainerSpec::Executable* to_modify) {
  to_modify->set_uid(uid);
  to_modify->set_gid(gid);
}

void SetCommandLine(const std::vector<std::string>& command_line,
                    ContainerSpec::Executable* to_modify) {
  to_modify->clear_command_line();
  for (const std::string& arg : command_line)
    to_modify->add_command_line(arg);
}

void SetTcpListenPorts(const std::set<port::Number>& ports,
                       ContainerSpec::Executable* to_modify) {
  to_modify->clear_tcp_listen_ports();
  SetListenPorts(to_modify->mutable_tcp_listen_ports(), ports);
}

void SetUdpListenPorts(const std::set<port::Number>& ports,
                       ContainerSpec::Executable* to_modify) {
  to_modify->clear_udp_listen_ports();
  SetListenPorts(to_modify->mutable_udp_listen_ports(), ports);
}

}  // namespace container_spec_helpers
}  // namespace parser
}  // namespace soma
