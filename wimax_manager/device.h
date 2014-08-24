// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_DEVICE_H_
#define WIMAX_MANAGER_DEVICE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/scoped_vector.h>
#include <base/values.h>
#include <chromeos/dbus/service_constants.h>

#include "wimax_manager/byte_identifier.h"
#include "wimax_manager/dbus_adaptable.h"
#include "wimax_manager/network.h"

namespace wimax_manager {

class DeviceDBusAdaptor;
class Manager;

class Device : public DBusAdaptable<Device, DeviceDBusAdaptor> {
 public:
  Device(Manager *manager, uint8_t index, const std::string &name);
  virtual ~Device() = default;

  virtual bool Enable() = 0;
  virtual bool Disable() = 0;
  virtual bool ScanNetworks() = 0;
  virtual bool Connect(const Network &network,
                       const base::DictionaryValue &parameters) = 0;
  virtual bool Disconnect() = 0;

  uint8_t index() const { return index_; }
  const std::string &name() const { return name_; }
  const ByteIdentifier &mac_address() const { return mac_address_; }
  const ByteIdentifier &base_station_id() const { return base_station_id_; }
  const int frequency() const { return frequency_; }
  const std::vector<int> &cinr() const { return cinr_; }
  const std::vector<int> &rssi() const { return rssi_; }
  const NetworkMap &networks() const { return networks_; }
  const DeviceStatus status() const { return status_; }

  uint32_t network_scan_interval() const { return network_scan_interval_; }
  void SetNetworkScanInterval(uint32_t network_scan_interval);

  int status_update_interval() const { return status_update_interval_; }
  void SetStatusUpdateInterval(uint32_t status_update_interval);

 protected:
  virtual void UpdateNetworkScanInterval(uint32_t network_scan_interval) = 0;
  virtual void UpdateStatusUpdateInterval(uint32_t status_update_interval) = 0;

  Manager *manager() const { return manager_; }

  void UpdateNetworks();
  void UpdateRFInfo();

  void SetMACAddress(const ByteIdentifier &mac_address);
  void SetBaseStationId(const ByteIdentifier &base_station_id);
  void set_frequency(int frequency) { frequency_ = frequency; }
  void set_cinr(const std::vector<int> &cinr) { cinr_ = cinr; }
  void set_rssi(const std::vector<int> &rssi) { rssi_ = rssi; }
  NetworkMap *mutable_networks() { return &networks_; }
  void SetStatus(DeviceStatus status);

 private:
  Manager *manager_;
  uint8_t index_;
  std::string name_;
  ByteIdentifier mac_address_;
  ByteIdentifier base_station_id_;
  int frequency_;
  std::vector<int> cinr_;
  std::vector<int> rssi_;
  NetworkMap networks_;
  uint32_t network_scan_interval_;
  uint32_t status_update_interval_;
  DeviceStatus status_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_DEVICE_H_
