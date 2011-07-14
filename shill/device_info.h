// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_INFO_
#define SHILL_DEVICE_INFO_

#include <base/callback_old.h>
#include <base/hash_tables.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>

#include "shill/device.h"

#include "shill/rtnl_listener.h"

struct nlmsghdr;

namespace shill {

class Manager;

class DeviceInfo {
 public:
  DeviceInfo(ControlInterface *control_interface,
             EventDispatcher *dispatcher,
             Manager *manager);
  ~DeviceInfo();

  void Start();
  void Stop();

  static Device::Technology GetDeviceTechnology(const char *interface_name);

 private:
  friend class DeviceInfoTest;

  static const char *kInterfaceUevent;
  static const char *kInterfaceDriver;
  static const char *kModemDrivers[];

  void AddLinkMsgHandler(struct nlmsghdr *hdr);
  void DelLinkMsgHandler(struct nlmsghdr *hdr);
  void LinkMsgHandler(struct nlmsghdr *hdr);

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Manager *manager_;
  base::hash_map<int, DeviceRefPtr> devices_;
  scoped_ptr<Callback1<struct nlmsghdr *>::Type> link_callback_;
  scoped_ptr<RTNLListener> link_listener_;
};

}  // namespace shill

#endif  // SHILL_DEVICE_INFO_
