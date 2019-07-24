// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_SCAN_MANAGER_H_
#define BLUETOOTH_NEWBLUED_SCAN_MANAGER_H_

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <base/memory/weak_ptr.h>

#include "bluetooth/newblued/device_interface_handler.h"
#include "bluetooth/newblued/newblue.h"

namespace bluetooth {

// Core implementation of scan management.
class ScanManager final : public DeviceInterfaceHandler::DeviceObserver {
 public:
  // |newblue| and |device_interface_handler| not owned, caller must make sure
  // it outlives this object.
  ScanManager(Newblue* newblue,
              DeviceInterfaceHandler* device_interface_handler);
  ~ScanManager() override;

  // Overrides of DeviceInterfaceHandler::DeviceObserver.
  void OnGattConnected(const std::string& device_address,
                       gatt_client_conn_t conn_id) override;
  void OnGattDisconnected(const std::string& device_address,
                          gatt_client_conn_t conn_id,
                          bool is_disconnected_by_newblue) override;
  void OnDevicePaired(const std::string& device_address) override;
  void OnDeviceUnpaired(const std::string& device_address) override;

  // Functions for external clients to request scan activities. |client_id|
  // is used to uniquely identify the clients who requested a scan session.
  bool StartScan(std::string client_id);
  bool StopScan(std::string client_id);
  bool UpdateScanSuspensionState(bool is_in_suspension);

 private:
  // Struct to hold scan settings for different scan behaviors.
  struct ScanSettings {
    bool active;
    uint16_t scan_interval;
    uint16_t scan_window;
    bool use_randomAddr;
    bool only_whitelist;
    bool filter_duplicates;
  };

  // struct to hold paired device information.
  struct PairedDevice {
    bool is_connected = false;
    bool is_disconnected_by_newblue = false;
  };

  // Scan manager state machine names.
  enum class ScanState : uint8_t {
    IDLE,
    ACTIVE_SCAN,
    PASSIVE_SCAN,
  };

  // Update the scan behavior based on all inputs.
  bool UpdateScan(void);
  // Evaluate if background scan is needed.
  void UpdateBackgroundScan(void);
  // Called when an update of a device info is received.
  void DeviceDiscoveryCallback(const std::string& address,
                               uint8_t address_type,
                               const std::string& resolved_address,
                               int8_t rssi,
                               uint8_t reply_type,
                               const std::vector<uint8_t>& eir);

  bool needs_background_scan_;
  bool is_in_suspension_;
  int number_of_clients_;
  // Initialized with IDLE state.
  ScanState scan_state_;
  // An unordered map to stored scan profiles, which consists sets of scan
  // parameters.
  std::unordered_map<std::string, ScanSettings> profiles_;

  Newblue* newblue_;

  // TODO(mcchou): Once the refactoring of internal API layer is done, the
  // constructor should take the pointer to the object holding the device
  // connection instead of DeviceInterfaceHandler.
  DeviceInterfaceHandler* device_interface_handler_;

  // Contains pairs of <device address, PairedDevice Objects> to store the
  // paired devices information.
  std::map<std::string, PairedDevice> paired_devices_;

  // Vector to store the clients that requested a scan session.
  std::vector<std::string> clients_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<ScanManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScanManager);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_SCAN_MANAGER_H_