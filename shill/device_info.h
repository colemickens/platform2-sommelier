// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_INFO_
#define SHILL_DEVICE_INFO_

#include <base/callback_old.h>
#include <base/hash_tables.h>
#include <base/memory/scoped_ptr.h>

#include "shill/device.h"
#include "shill/shill_event.h"

namespace shill {

class DeviceInfo {
 public:
  DeviceInfo(EventDispatcher *dispatcher);
  ~DeviceInfo();
  void Start();
  void Stop();

 private:
  void ParseRTNL(InputData *data);
  void NextRequest(uint32_t seq);
  void LinkMsg(unsigned char *buf, bool del);
  void AddrMsg(unsigned char *buf, bool del);
  void RouteMsg(unsigned char *buf, bool del);

  bool running_;
  EventDispatcher *dispatcher_;
  scoped_ptr<Callback1<InputData *>::Type> rtnl_callback_;
  int rtnl_socket_;
  base::hash_map<int, Device *> devices_;
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
