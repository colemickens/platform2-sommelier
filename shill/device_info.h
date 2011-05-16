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

#include "shill/shill_event.h"

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
  static const char *kInterfaceUevent;
  static const char *kInterfaceDriver;
  static const char *kModemDrivers[];

  void ParseRTNL(InputData *data);
  void NextRequest(uint32_t seq);
  void AddLinkMsg(struct nlmsghdr *hdr);
  void DelLinkMsg(struct nlmsghdr *hdr);
  void AddAddrMsg(struct nlmsghdr *hdr);
  void DelAddrMsg(struct nlmsghdr *hdr);
  void AddRouteMsg(struct nlmsghdr *hdr);
  void DelRouteMsg(struct nlmsghdr *hdr);

  bool running_;
  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Manager *manager_;
  scoped_ptr<Callback1<InputData *>::Type> rtnl_callback_;
  int rtnl_socket_;
  base::hash_map<int, scoped_refptr<Device> > devices_;
  scoped_ptr<IOInputHandler> rtnl_handler_;
  int request_flags_;
  uint32_t request_sequence_;
  friend class DeviceInfoTest;
};

enum {
  REQUEST_LINK = 1,
  REQUEST_ADDR = 2,
  REQUEST_ROUTE = 4
};

}  // namespace shill

#endif  // SHILL_DEVICE_INFO_
