// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_
#define SHILL_DEVICE_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/ipconfig.h"
#include "shill/property_store.h"
#include "shill/refptr_types.h"
#include "shill/shill_event.h"

namespace shill {

class ControlInterface;
class DeviceAdaptorInterface;
class DeviceInfo;
class Endpoint;
class Error;
class EventDispatcher;
class Manager;

// Device superclass.  Individual network interfaces types will inherit from
// this class.
class Device : public base::RefCounted<Device>, public PropertyStore {
 public:
  enum Technology {
    kEthernet,
    kWifi,
    kCellular,
    kBlackListed,
    kUnknown,
    kNumTechnologies
  };

  // A constructor for the Device object
  Device(ControlInterface *control_interface,
         EventDispatcher *dispatcher,
         Manager *manager,
         const std::string &link_name,
         int interface_index);
  virtual ~Device();

  virtual void Start();
  virtual void Stop();

  // Base method always returns false.
  virtual bool TechnologyIs(const Technology type);

  virtual void LinkEvent(unsigned flags, unsigned change);
  virtual void Scan();

  virtual void ConfigIP() {}

  // Implementation of PropertyStore
  virtual bool SetBoolProperty(const std::string& name,
                               bool value,
                               Error *error);
  virtual bool SetInt32Property(const std::string& name,
                                int32 value,
                                Error *error);
  virtual bool SetUint16Property(const std::string& name,
                                 uint16 value,
                                 Error *error);
  virtual bool SetStringProperty(const std::string &name,
                                 const std::string &value,
                                 Error *error);

  const std::string &link_name() const { return link_name_; }

  // Returns a string that is guaranteed to uniquely identify this Device
  // instance.
  const std::string &UniqueName() const;

  std::string GetRpcIdentifier();

 protected:
  FRIEND_TEST(DeviceTest, AcquireDHCPConfig);
  FRIEND_TEST(DeviceTest, DestroyIPConfig);
  FRIEND_TEST(DeviceTest, DestroyIPConfigNULL);
  FRIEND_TEST(DeviceTest, GetProperties);

  // If there's an IP configuration in |ipconfig_|, releases the IP address and
  // destroys the configuration instance.
  void DestroyIPConfig();

  // Creates a new DHCP IP configuration instance, stores it in |ipconfig_| and
  // requests a new IP configuration. Registers a callback to
  // IPConfigUpdatedCallback on IP configuration changes. Returns true if the IP
  // request was successfully sent.
  bool AcquireDHCPConfig();

  void RegisterDerivedString(const std::string &name,
                             std::string(Device::*get)(void),
                             bool(Device::*set)(const std::string&));
  void RegisterDerivedStrings(const std::string &name,
                              Strings(Device::*get)(void),
                              bool(Device::*set)(const Strings&));

  // Properties
  std::string hardware_address_;
  bool powered_;  // TODO(pstew): Is this what |running_| is for?
  bool reconnect_;

  std::vector<ServiceRefPtr> services_;
  int interface_index_;
  bool running_;
  Manager *manager_;
  IPConfigRefPtr ipconfig_;

 private:
  friend class DeviceAdaptorInterface;

  // Callback invoked on every IP configuration update.
  void IPConfigUpdatedCallback(const IPConfigRefPtr &ipconfig, bool success);

  std::vector<std::string> AvailableIPConfigs();
  std::string GetRpcConnectionIdentifier();

  const std::string link_name_;
  scoped_ptr<DeviceAdaptorInterface> adaptor_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace shill

#endif  // SHILL_DEVICE_
