// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_RTNL_HANDLER_
#define SHILL_RTNL_HANDLER_

#include <string>
#include <vector>

#include <base/callback_old.h>
#include <base/hash_tables.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/singleton.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/device.h"

#include "shill/io_handler.h"
#include "shill/rtnl_listener.h"
#include "shill/shill_event.h"

struct nlmsghdr;

namespace shill {

class IPConfig;
class Sockets;

// This singleton class is responsible for interacting with the RTNL subsystem.
// RTNL provides (among other things) access to interface discovery (add/remove
// events), interface state monitoring and the ability to change interace flags.
// Similar functionality also exists for IP address configuration for interfaces
// and IP routing tables.
//
// RTNLHandler provides access to these events through a callback system and
// provides utility functions to make changes to interface, address and routing
// state.
class RTNLHandler {
 public:
  static const int kRequestLink = 1;
  static const int kRequestAddr = 2;
  static const int kRequestRoute = 4;

  // Since this is a singleton, use RTNHandler::GetInstance()->Foo()
  static RTNLHandler *GetInstance();

  // This starts the event-monitoring function of the RTNL handler. This
  // function requires an EventDispatcher pointer so it can add itself to the
  // event loop.
  void Start(EventDispatcher *dispatcher, Sockets *sockets);

  // This stops the event-monitoring function of the RTNL handler
  void Stop();

  // Add an RTNL event listener to the list of entities that will
  // be notified of RTNL events.
  void AddListener(RTNLListener *to_add);

  // Remove a previously added RTNL event listener
  void RemoveListener(RTNLListener *to_remove);

  // Set flags on a network interface that has a kernel index of
  // 'interface_index'.  Only the flags bits set in 'change' will
  // be set, and they will be set to the corresponding bit in 'flags'.
  void SetInterfaceFlags(int interface_index, unsigned int flags,
                         unsigned int change);

  // Set address of a network interface that has a kernel index of
  // 'interface_index'.
  bool AddInterfaceAddress(int interface_index, const IPConfig &config);

  // Remove address from a network interface that has a kernel index of
  // 'interface_index'.
  bool RemoveInterfaceAddress(int interface_index, const IPConfig &config);

  // Request that various tables (link, address, routing) tables be
  // exhaustively dumped via RTNL.  As results arrive from the kernel
  // they will be broadcast to all listeners.  The possible values
  // (multiple can be ORred together) are below.
  void RequestDump(int request_flags);

  // Returns the index of interface |interface_name|, or -1 if unable to
  // determine the index.
  int GetInterfaceIndex(const std::string &interface_name);

 private:
  friend class DeviceInfoTest;
  friend class ModemTest;
  friend class RTNLHandlerTest;
  friend struct DefaultSingletonTraits<RTNLHandler>;
  FRIEND_TEST(RTNLListenerTest, NoRun);
  FRIEND_TEST(RTNLListenerTest, Run);

  // Private to ensure that this behaves as a singleton.
  RTNLHandler();
  virtual ~RTNLHandler();

  // Dispatches an rtnl message to all listeners
  void DispatchEvent(int type, struct nlmsghdr *hdr);
  // Send the next table-dump request to the kernel
  void NextRequest(uint32_t seq);
  // Parse an incoming rtnl message from the kernel
  void ParseRTNL(InputData *data);
  bool AddressRequest(int interface_index, int cmd, int flags,
                      const IPConfig &config);

  Sockets *sockets_;
  bool in_request_;

  int rtnl_socket_;
  uint32_t request_flags_;
  uint32_t request_sequence_;

  std::vector<RTNLListener *> listeners_;
  scoped_ptr<Callback1<InputData *>::Type> rtnl_callback_;
  scoped_ptr<IOInputHandler> rtnl_handler_;

  DISALLOW_COPY_AND_ASSIGN(RTNLHandler);
};

}  // namespace shill

#endif  // SHILL_RTNL_HANDLER_
