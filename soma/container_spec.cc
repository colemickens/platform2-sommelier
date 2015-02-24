// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/container_spec.h"

#include <sys/types.h>

#include <algorithm>
#include <set>
#include <string>

#include <base/files/file_path.h>
#include <base/memory/scoped_vector.h>

#include "soma/device_filter.h"
#include "soma/sysfs_filter.h"
#include "soma/usb_device_filter.h"

namespace soma {

ContainerSpec::ContainerSpec(const base::FilePath& service_bundle_path,
                             uid_t uid,
                             gid_t gid)
    : service_bundle_path_(service_bundle_path),
      uid_(uid),
      gid_(gid) {
}

ContainerSpec::~ContainerSpec() {}

void ContainerSpec::AddListenPort(int port) {
  listen_ports_.insert(port);
}

void ContainerSpec::AddDevicePathFilter(const std::string& filter) {
  device_path_filters_.push_back(new DevicePathFilter(base::FilePath(filter)));
}

void ContainerSpec::AddDeviceNodeFilter(int major, int minor) {
  device_node_filters_.push_back(new DeviceNodeFilter(major, minor));
}

void ContainerSpec::AddSysfsFilter(const std::string& filter) {
  sysfs_filters_.push_back(new SysfsFilter(base::FilePath(filter)));
}

void ContainerSpec::AddUSBDeviceFilter(int vid, int pid) {
  usb_device_filters_.push_back(new USBDeviceFilter(vid, pid));
}

bool ContainerSpec::DevicePathIsAllowed(const base::FilePath& query) {
  return
      device_path_filters_.end() !=
      std::find_if(device_path_filters_.begin(), device_path_filters_.end(),
                   [query](DevicePathFilter* to_check) {
                     return to_check->Allows(query);
                   });
}

bool ContainerSpec::DeviceNodeIsAllowed(int major, int minor) {
  return
      device_node_filters_.end() !=
      std::find_if(device_node_filters_.begin(), device_node_filters_.end(),
                   [major, minor](DeviceNodeFilter* to_check) {
                     return to_check->Allows(major, minor);
                   });
}

}  // namespace soma
