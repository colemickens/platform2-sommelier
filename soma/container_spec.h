// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_CONTAINER_SPEC_H_
#define SOMA_CONTAINER_SPEC_H_

#include <sys/types.h>

#include <set>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/memory/scoped_vector.h>

#include "soma/device_filter.h"
#include "soma/sysfs_filter.h"
#include "soma/usb_device_filter.h"

namespace soma {

// Holds intermediate representation of container specification.
// TODO(cmasone): Serialization of this will need to be a thing.
class ContainerSpec {
 public:
  ContainerSpec(const base::FilePath& service_bundle_path,
                uid_t uid,
                gid_t gid);
  virtual ~ContainerSpec();

  void AddListenPort(int port);

  void AddDevicePathFilter(const std::string& filter);
  void AddDeviceNodeFilter(int major, int minor);
  void AddSysfsFilter(const std::string& filter);
  void AddUSBDeviceFilter(int vid, int pid);

  const base::FilePath& service_bundle_path() { return service_bundle_path_; }
  uid_t uid() { return uid_; }
  gid_t gid() { return gid_; }

  // Returns true if there's a DevicePathFilter that matches query.
  bool DevicePathIsAllowed(const base::FilePath& query);

  // Returns true if there's a DeviceNodeFilter that matches major and minor.
  bool DeviceNodeIsAllowed(int major, int minor);

 private:
  const base::FilePath service_bundle_path_;
  const uid_t uid_;
  const gid_t gid_;

  std::set<int> listen_ports_;

  // TODO(cmasone): As we gain more experience with these, investigate whether
  // they should also be sets, or at leat have set semantics.
  ScopedVector<DevicePathFilter> device_path_filters_;
  ScopedVector<DeviceNodeFilter> device_node_filters_;
  ScopedVector<SysfsFilter> sysfs_filters_;
  ScopedVector<USBDeviceFilter> usb_device_filters_;

  DISALLOW_COPY_AND_ASSIGN(ContainerSpec);
};

}  // namespace soma
#endif  // SOMA_CONTAINER_SPEC_H_
