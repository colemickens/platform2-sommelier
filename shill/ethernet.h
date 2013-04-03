// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ETHERNET_
#define SHILL_ETHERNET_

#include <string>

#include <base/memory/scoped_ptr.h>

#include "shill/device.h"
#include "shill/event_dispatcher.h"
#include "shill/refptr_types.h"

namespace shill {

class EapListener;

class Ethernet : public Device {
 public:
  Ethernet(ControlInterface *control_interface,
           EventDispatcher *dispatcher,
           Metrics *metrics,
           Manager *manager,
           const std::string& link_name,
           const std::string &address,
           int interface_index);
  virtual ~Ethernet();

  virtual void Start(Error *error, const EnabledStateChangedCallback &callback);
  virtual void Stop(Error *error, const EnabledStateChangedCallback &callback);
  virtual void LinkEvent(unsigned int flags, unsigned int change);
  virtual void ConnectTo(EthernetService *service);
  virtual void DisconnectFrom(EthernetService *service);

 private:
  friend class EthernetTest;

  void OnEapDetected();

  ServiceRefPtr service_;
  bool link_up_;

  // Track whether an EAP authenticator has been detected on this link.
  bool is_eap_detected_;
  scoped_ptr<EapListener> eap_listener_;

  DISALLOW_COPY_AND_ASSIGN(Ethernet);
};

}  // namespace shill

#endif  // SHILL_ETHERNET_
