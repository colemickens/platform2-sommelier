// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/container_spec_wrapper.h"

#include <sys/types.h>

#include <algorithm>
#include <limits>
#include <set>
#include <string>

#include <base/files/file_path.h>
#include <base/memory/scoped_vector.h>

#include "soma/device_filter.h"
#include "soma/namespace.h"
#include "soma/port.h"
#include "soma/proto_bindings/soma_container_spec.pb.h"
#include "soma/sysfs_filter.h"
#include "soma/usb_device_filter.h"

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;

namespace soma {
namespace {
void SetListenPorts(ContainerSpec::PortSpec* port_spec,
                    const std::set<parser::port::Number>& ports) {
  // If the wildcard port is in the set, just allow all and bail early.
  if (ports.find(parser::port::kWildcard) != ports.end()) {
    port_spec->set_allow_all(true);
    return;
  }
  for (const parser::port::Number& port : ports) {
    // The parsing code should have ensured this, so just DCHECK()
    DCHECK(port <= std::numeric_limits<uint16_t>::max());
    port_spec->add_ports(static_cast<uint16_t>(port));
  }
}

bool ListenPortIsAllowed(const ContainerSpec::PortSpec& port_spec,
                         parser::port::Number port) {
  if (port_spec.allow_all())
    return true;
  const RepeatedField<uint32>& ports = port_spec.ports();
  return std::find(ports.begin(), ports.end(), port) != ports.end();
}
}  // namespace

ContainerSpecWrapper::ContainerSpecWrapper(
    const std::string& name,
    const base::FilePath& service_bundle_path,
    uid_t uid,
    gid_t gid) {
  internal_.set_name(name);
  internal_.set_service_bundle_path(service_bundle_path.value());
  internal_.set_uid(uid);
  internal_.set_gid(gid);
}

ContainerSpecWrapper::~ContainerSpecWrapper() = default;

void ContainerSpecWrapper::SetServiceNames(
    const std::vector<std::string>& service_names) {
  internal_.clear_service_names();
  for (const std::string& name : service_names)
    internal_.add_service_names(name);
}

void ContainerSpecWrapper::SetCommandLine(
    const std::vector<std::string>& command_line) {
  internal_.clear_command_line();
  for (const std::string& arg : command_line)
    internal_.add_command_line(arg);
}

void ContainerSpecWrapper::SetNamespaces(
    const std::set<parser::ns::Kind>& namespaces) {
  internal_.clear_namespaces();
  for (const parser::ns::Kind& ns : namespaces)
    internal_.add_namespaces(ns);
}

void ContainerSpecWrapper::SetTcpListenPorts(
    const std::set<parser::port::Number>& ports) {
  internal_.clear_tcp_listen_ports();
  SetListenPorts(internal_.mutable_tcp_listen_ports(), ports);
}

void ContainerSpecWrapper::SetUdpListenPorts(
    const std::set<parser::port::Number>& ports) {
  internal_.clear_udp_listen_ports();
  SetListenPorts(internal_.mutable_udp_listen_ports(), ports);
}

void ContainerSpecWrapper::SetDevicePathFilters(
    const parser::DevicePathFilter::Set& filters) {
  internal_.clear_device_path_filters();
  for (const parser::DevicePathFilter& filter : filters)
    internal_.add_device_path_filters()->set_filter(filter.filter().value());
}

void ContainerSpecWrapper::SetDeviceNodeFilters(
    const parser::DeviceNodeFilter::Set& filters) {
  internal_.clear_device_node_filters();
  for (const parser::DeviceNodeFilter& parser_filter : filters) {
    ContainerSpec::DeviceNodeFilter* filter =
        internal_.add_device_node_filters();
    filter->set_major(parser_filter.major());
    filter->set_minor(parser_filter.minor());
  }
}

void ContainerSpecWrapper::AddSysfsFilter(const std::string& filter) {
  sysfs_filters_.push_back(new SysfsFilter(base::FilePath(filter)));
}

void ContainerSpecWrapper::AddUSBDeviceFilter(int vid, int pid) {
  usb_device_filters_.push_back(new USBDeviceFilter(vid, pid));
}

bool ContainerSpecWrapper::ProvidesServiceNamed(const std::string& name) const {
  const RepeatedPtrField<std::string>& names = internal_.service_names();
  return std::find(names.begin(), names.end(), name) != names.end();
}

bool ContainerSpecWrapper::ShouldApplyNamespace(
    parser::ns::Kind candidate) const {
  const RepeatedField<int>& namespaces = internal_.namespaces();
  return std::find(namespaces.begin(), namespaces.end(), candidate) !=
      namespaces.end();
}

bool ContainerSpecWrapper::TcpListenPortIsAllowed(
    parser::port::Number port) const {
  return ListenPortIsAllowed(internal_.tcp_listen_ports(), port);
}

bool ContainerSpecWrapper::UdpListenPortIsAllowed(
    parser::port::Number port) const {
  return ListenPortIsAllowed(internal_.udp_listen_ports(), port);
}

bool ContainerSpecWrapper::DevicePathIsAllowed(
    const base::FilePath& query) const {
  const RepeatedPtrField<ContainerSpec::DevicePathFilter>& filters =
      internal_.device_path_filters();
  return
      filters.end() !=
      std::find_if(filters.begin(), filters.end(),
                   [query](const ContainerSpec::DevicePathFilter& to_check) {
                     parser::DevicePathFilter parser_filter(
                         base::FilePath(to_check.filter()));
                     return parser_filter.Allows(query);
                   });
}

bool ContainerSpecWrapper::DeviceNodeIsAllowed(int major, int minor) const {
  const RepeatedPtrField<ContainerSpec::DeviceNodeFilter>& filters =
      internal_.device_node_filters();
  return
      filters.end() !=
      std::find_if(
          filters.begin(), filters.end(),
          [major, minor](const ContainerSpec::DeviceNodeFilter& to_check) {
            parser::DeviceNodeFilter parser_filter(to_check.major(),
                                                   to_check.minor());
            return parser_filter.Allows(major, minor);
          });
}

}  // namespace soma
