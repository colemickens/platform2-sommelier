// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SERVICE_
#define SHILL_SERVICE_

#include <string>
#include <map>
#include <vector>

#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>

#include "shill/device_config_interface.h"
#include "shill/property_store_interface.h"

namespace shill {

class Connection;
class Configuration;
class ControlInterface;
class Endpoint;
class Error;
class EventDispatcher;
class Service;
class ServiceAdaptorInterface;

typedef scoped_refptr<const Service> ServiceConstRefPtr;
typedef scoped_refptr<Service> ServiceRefPtr;

class Service : public base::RefCounted<Service>,
                public PropertyStoreInterface {
 public:
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
  enum ConnectState {
    kServiceStateUnknown,
    kServiceStateIdle,
    kServiceStateAssociating,
    kServiceStateConfiguring,
    kServiceStateConnected,
    kServiceStateDisconnected,
    kServiceStateFailure
  };

  // A constructor for the Service object
  Service(ControlInterface *control_interface,
          EventDispatcher *dispatcher,
          DeviceConfigInterfaceRefPtr config_interface,
          const std::string& name);
  virtual ~Service();
  virtual void Connect() = 0;
  virtual void Disconnect() = 0;

  // Implementation of PropertyStoreInterface
  bool SetBoolProperty(const std::string& name, bool value, Error *error);
  bool SetInt32Property(const std::string& name, int32 value, Error *error);
  bool SetStringProperty(const std::string& name,
                         const std::string& value,
                         Error *error);
  bool SetStringmapProperty(const std::string& name,
                            const std::map<std::string, std::string>& values,
                            Error *error);
  bool SetUint8Property(const std::string& name, uint8 value, Error *error);

  // Returns a string that is guaranteed to uniquely identify this
  // Service instance.
  virtual const std::string& UniqueName() { return name_; }

 private:
  const std::string name_;
  bool available_;
  bool configured_;
  bool auto_connect_;
  Configuration *configuration_;
  Connection *connection_;
  DeviceConfigInterfaceRefPtr config_interface_;
  Endpoint *endpoint_;
  scoped_ptr<ServiceAdaptorInterface> adaptor_;
  friend class ServiceAdaptorInterface;
};

}  // namespace shill

#endif  // SHILL_SERVICE_
