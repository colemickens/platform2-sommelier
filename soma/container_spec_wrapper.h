// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_CONTAINER_SPEC_WRAPPER_H_
#define SOMA_CONTAINER_SPEC_WRAPPER_H_

#include <sys/types.h>

#include <set>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/memory/scoped_vector.h>

#include "soma/device_filter.h"
#include "soma/namespace.h"
#include "soma/port.h"
#include "soma/proto_bindings/soma_container_spec.pb.h"
#include "soma/sysfs_filter.h"
#include "soma/usb_device_filter.h"

namespace soma {

// More friendly wrapper around the ContainerSpec protobuf.
// This class owns an instance of the protobuf and will hand out read-only
// references of it on request. This means a new protobuf is allocated
// and populated every time we go to read a spec, and then it needs to
// be merged into an RPC response. If this is too slow, requiring too
// many copies of the same data, we could change this class to take a
// pointer to a ContainerSpec instead.
class ContainerSpecWrapper {
 public:
  ContainerSpecWrapper(const base::FilePath& service_bundle_path,
                       uid_t uid,
                       gid_t gid);
  virtual ~ContainerSpecWrapper();

  void SetCommandLine(const std::vector<std::string> command_line);
  void SetNamespaces(const std::set<parser::ns::Kind> namespaces);
  void SetTcpListenPorts(const std::set<parser::port::Number>& ports);
  void SetUdpListenPorts(const std::set<parser::port::Number>& ports);
  void SetDevicePathFilters(const parser::DevicePathFilterSet& filters);
  void SetDeviceNodeFilters(const parser::DeviceNodeFilterSet& filters);

  // These don't really do anything yet.
  void AddSysfsFilter(const std::string& filter);
  void AddUSBDeviceFilter(int vid, int pid);

  base::FilePath service_bundle_path() {
    return base::FilePath(internal_.service_bundle_path());
  }
  uid_t uid() { return internal_.uid(); }
  gid_t gid() { return internal_.gid(); }

  // Returns true if candidate is explicitly allowed.
  bool ShouldApplyNamespace(parser::ns::Kind candidate) const;

  // Returns true if port is explicitly or implicitly allowed (by wildcarding).
  bool TcpListenPortIsAllowed(parser::port::Number port) const;
  bool UdpListenPortIsAllowed(parser::port::Number port) const;

  // Returns true if there's a DevicePathFilter that matches query.
  bool DevicePathIsAllowed(const base::FilePath& query) const;

  // Returns true if there's a DeviceNodeFilter that matches major and minor.
  bool DeviceNodeIsAllowed(int major, int minor) const;

  const ContainerSpec& AsProto() const { return internal_; }

 private:
  // TODO(cmasone): As we gain more experience with these, investigate whether
  // they should also be sets, or at least have set semantics.
  ScopedVector<SysfsFilter> sysfs_filters_;
  ScopedVector<USBDeviceFilter> usb_device_filters_;

  ContainerSpec internal_;

  DISALLOW_COPY_AND_ASSIGN(ContainerSpecWrapper);
};

}  // namespace soma
#endif  // SOMA_CONTAINER_SPEC_WRAPPER_H_
