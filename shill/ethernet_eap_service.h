// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ETHERNET_EAP_SERVICE_H_
#define SHILL_ETHERNET_EAP_SERVICE_H_

#include <string>

#include "shill/service.h"

namespace shill {

// The EthernetEapService contains configuraton for any Ethernet interface
// while authenticating or authenticated to a wired 802.1x endpoint.  This
// includes EAP credentials and Static IP configuration.  This service in
// itself is not connectable, but can be used by any Ethernet device during
// authentication.
class EthernetEapService : public Service {
 public:
  EthernetEapService(ControlInterface *control_interface,
                     EventDispatcher *dispatcher,
                     Metrics *metrics,
                     Manager *manager);
  virtual ~EthernetEapService();

  // Inherited from Service.
  virtual std::string GetDeviceRpcId(Error *error) const;
  virtual std::string GetStorageIdentifier() const;
  virtual bool Is8021x() const { return true; }
  virtual bool IsVisible() const { return false; }
  virtual void OnEapCredentialsChanged();
  virtual bool Unload();
};

}  // namespace shill

#endif  // SHILL_ETHERNET_EAP_SERVICE_H_
