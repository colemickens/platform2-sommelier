// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MANAGER_
#define SHILL_MANAGER_

#include "shill/resource.h"
#include "shill/shill_event.h"

namespace shill {

class Connection;
class Configuration;
class Device;
class Endpoint;

class Service : public Resource {
 public:
  // A constructor for the Service object
  explicit Service(ControlInterface *control_interface,
		   EventDispatcher *dispatcher);
  ~Service();
  void Connnect();
  void Disconnect();
  enum ConnectState {
    kServiceStateUnknown,
    kServiceStateIdle,
    kServiceStateAssociating,
    kServiceStateConfiguring,
    kServiceStateConnected,
    kServiceStateDisconnected,
    kServiceStateFailure
  };
  enum ConnectFailure {
    kServiceFailureUnknown,
    kServiceFailureActivationFailure,
    kServiceFailureOutOfRange,
    kServiceFailurePinMissing,
    kServiceFailureConfigurationFailed,
    kServiceFailureBadCredentials,
    kServiceFailureNeedEVDO,
    kServiceFailureNeedHomeNetwork,
    kServiceFailureOTASPFailure,
    kServiceFailureAAAFailure
  };

 private:
  string name_;
  bool available_;
  bool configured_;
  bool auto_connect_;
  Configuration *configuration_;
  Connection *connection_;
  Device *interface_;
  Endpoint *endpoint_;
  ServiceAdaptorInterface *adaptor_;
  friend class ServiceAdaptorInterface;
};

}  // namespace shill

#endif  // SHILL_SERVICE_
