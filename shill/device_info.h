// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_INFO_
#define SHILL_DEVICE_INFO_

#include <map>
#include <set>
#include <string>

#include <base/callback_old.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/byte_string.h"
#include "shill/device.h"
#include "shill/rtnl_listener.h"

namespace shill {

class Manager;
class RTNLMessage;

class DeviceInfo {
 public:
  DeviceInfo(ControlInterface *control_interface,
             EventDispatcher *dispatcher,
             Manager *manager);
  ~DeviceInfo();

  void AddDeviceToBlackList(const std::string &device_name);
  void Start();
  void Stop();

  // Adds |device| to this DeviceInfo instance so that we can handle its link
  // messages, and registers it with the manager.
  void RegisterDevice(const DeviceRefPtr &device);

  DeviceRefPtr GetDevice(int interface_index) const;
  virtual bool GetAddress(int interface_index, ByteString *address) const;
  virtual bool GetFlags(int interface_index, unsigned int *flags) const;

 private:
  friend class DeviceInfoTest;
  FRIEND_TEST(CellularTest, StartLinked);

  struct Info {
    Info() : flags(0) {}

    DeviceRefPtr device;
    ByteString address;
    unsigned int flags;
  };

  static const char *kInterfaceUevent;
  static const char *kInterfaceDriver;
  static const char *kModemDrivers[];

  static Device::Technology GetDeviceTechnology(const std::string &face_name);

  void AddLinkMsgHandler(const RTNLMessage &msg);
  void DelLinkMsgHandler(const RTNLMessage &msg);
  void LinkMsgHandler(const RTNLMessage &msg);

  const Info *GetInfo(int interface_index) const;
  void RemoveInfo(int interface_index);

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Manager *manager_;
  std::map<int, Info> infos_;
  scoped_ptr<Callback1<const RTNLMessage &>::Type> link_callback_;
  scoped_ptr<RTNLListener> link_listener_;
  std::set<std::string> black_list_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfo);
};

}  // namespace shill

#endif  // SHILL_DEVICE_INFO_
