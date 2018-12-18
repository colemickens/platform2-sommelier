// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_NEWBLUE_H_
#define BLUETOOTH_NEWBLUED_NEWBLUE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <base/single_thread_task_runner.h>

#include "bluetooth/common/util.h"
#include "bluetooth/newblued/libnewblue.h"
#include "bluetooth/newblued/property.h"
#include "bluetooth/newblued/uuid.h"

namespace bluetooth {

// These are based on the numbers assigned by Bluetooth SIG, see
// www.bluetooth.com/specifications/assigned-numbers/generic-access-profile.
// Note that only the subset of all EIR data types is needed for now.
enum class EirType : uint8_t {
  FLAGS = 0x01,
  UUID16_INCOMPLETE = 0x02,
  UUID16_COMPLETE = 0x03,
  UUID32_INCOMPLETE = 0x04,
  UUID32_COMPLETE = 0x05,
  UUID128_INCOMPLETE = 0x06,
  UUID128_COMPLETE = 0x07,
  NAME_SHORT = 0x08,
  NAME_COMPLETE = 0x09,
  TX_POWER = 0x0a,
  CLASS_OF_DEV = 0x0d,
  SVC_DATA16 = 0x16,
  GAP_APPEARANCE = 0x19,
  SVC_DATA32 = 0x20,
  SVC_DATA128 = 0x21,
  MANUFACTURER_DATA = 0xff,
};

// These are based on the pairing state defined in newblue/sm.h.
enum class PairState : uint8_t {
  NOT_PAIRED,
  STARTED,
  PAIRED,
  CANCELED,
  FAILED,
};

// Structure representing a discovered device.
struct Device {
  Device();
  explicit Device(const std::string& address);

  // MAC address (in format XX:XX:XX:XX:XX:XX).
  std::string address;
  // Whether the MAC address is a random address.
  bool is_random_address;

  // [mandatory] Whether the device is paired.
  Property<bool> paired;
  // [mandatory] Whether the device is connected.
  Property<bool> connected;
  // [mandatory] Whether the device is in the white list.
  Property<bool> trusted;
  // [mandatory] Whether the device is in the black list.
  Property<bool> blocked;
  // [mandatory] Whether the services provided by the device has been resolved.
  Property<bool> services_resolved;

  // [mandatory] A readable and writable alias given to the device.
  Property<std::string> alias;
  // Actual alias provided by the user.
  std::string internal_alias;
  // [optional] A readable name of the device.
  Property<std::string> name;

  // [optional] Transmission power level of the advertisement packet
  Property<int16_t> tx_power;
  // [optional] RSSI of last received inquiry response.
  Property<int16_t> rssi;

  // [optional] Class of the device.
  Property<uint32_t> eir_class;
  // [optional] External appearance of the device.
  Property<uint16_t> appearance;
  // [optional] Icon type of the device based on the value of |appearance|.
  Property<std::string> icon;

  // [optional] Advertising flags.
  Property<std::vector<uint8_t>> flags;
  // [optional] Service UUIDs of 16-bit 32-bit and 128-bit.
  Property<std::set<Uuid>> service_uuids;
  // [optional] Service data associated with UUIDs.
  Property<std::map<Uuid, std::vector<uint8_t>>> service_data;

  // [optional] Manufacturer identifier with the extra manufacturer data
  Property<std::map<uint16_t, std::vector<uint8_t>>> manufacturer;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

// Agent to receive pairing user interaction events.
class PairingAgent {
 public:
  virtual void DisplayPasskey(const std::string& device_address,
                              uint32_t passkey) = 0;
  // TODO(sonnysasaka): Add other methods:
  // RequestPinCode, DisplayPinCode, RequestPasskey, RequestConfirmation,
  // RequestAuthorization, AuthorizeService.
};

// A higher-level API wrapper of the low-level libnewblue C interface.
// This class abstracts away the C interface details of libnewblue and provides
// event handling model that is compatible with libchrome's main loop.
class Newblue {
 public:
  using DeviceDiscoveredCallback = base::Callback<void(const Device&)>;
  // dbus_error_code refers to namespace bluetooth_device
  // chromeos/dbus/service_constants.h.
  using PairStateChangedCallback =
      base::Callback<void(const Device& device,
                          PairState pair_state,
                          const std::string& dbus_error)>;

  explicit Newblue(std::unique_ptr<LibNewblue> libnewblue);

  virtual ~Newblue() = default;

  base::WeakPtr<Newblue> GetWeakPtr();

  // Initializes the LE stack (blocking call).
  // Returns true if initialization succeeds, false otherwise.
  virtual bool Init();

  // Registers/Unregisters pairing agent which handles user interactions during
  // pairing process.
  // |pairing_agent| not owned, must outlive this object.
  virtual void RegisterPairingAgent(PairingAgent* pairing_agent);
  virtual void UnregisterPairingAgent();

  // Listens to reset complete event from the chip. This is useful to detect
  // when NewBlue is ready to bring up the stack.
  // Returns true on success and false otherwise.
  virtual bool ListenReadyForUp(base::Closure callback);

  // Brings up the NewBlue stack. This should be called when the adapter has
  // just been turned on, detected when there is reset complete event from the
  // chip. ListenReadyForUp() should be used to detect this event.
  // Returns true if success, false otherwise.
  virtual bool BringUp();

  // Starts LE scanning, |callback| will be called every time inquiry response
  // is received.
  virtual bool StartDiscovery(DeviceDiscoveredCallback callback);
  // Stops LE scanning.
  virtual bool StopDiscovery();

  // Updates EIR data of |device|.
  static void UpdateEir(Device* device, const std::vector<uint8_t>& eir);

  // Registers as an observer of pairing states of devices.
  virtual UniqueId RegisterAsPairObserver(PairStateChangedCallback callback);
  // Unregisters as an observer of pairing states.
  virtual void UnregisterAsPairObserver(UniqueId observer_id);

  // Performs LE pairing.
  virtual bool Pair(const std::string& device_address);
  // Cancels LE pairing.
  virtual bool CancelPair(const std::string& device_address);

 private:
  // Posts task to the thread which created this Newblue object.
  // libnewblue callbacks should always post task using this method rather than
  // doing any processing in the callback's thread.
  bool PostTask(const tracked_objects::Location& from_here,
                const base::Closure& task);

  // Called by NewBlue when it's ready to bring up the stack. This is called on
  // one of NewBlue's threads, so we shouldn't do anything on this thread other
  // than posting the task to our mainloop thread.
  static void OnStackReadyForUpThunk(void* data);
  // Triggers the callback registered via ListenReadyForUp().
  void OnStackReadyForUp();

  static void DiscoveryCallbackThunk(void* data,
                                     const struct bt_addr* addr,
                                     int8_t rssi,
                                     uint8_t reply_type,
                                     const void* eir,
                                     uint8_t eir_len);
  // Called when inquiry response is received as a result of StartDiscovery().
  void DiscoveryCallback(const std::string& address,
                         uint8_t address_type,
                         int8_t rssi,
                         uint8_t reply_type,
                         const std::vector<uint8_t>& eir);

  // Updates the service UUIDs based on data of EirTypes including
  // UUID16_INCOMPLETE, UUID16_COMPLETE, UUID32_INCOMPLETE, UUID32_COMPLETE,
  // UUID128_INCOMPLETE and UUID128_COMPLETE.
  static void UpdateServiceUuids(std::set<Uuid>* service_uuids,
                                 uint8_t uuid_size,
                                 const uint8_t* data,
                                 uint8_t data_len);
  // Updates the service data based on data of EirTypes including SVC_DATA16,
  // SVC_DATA32 and SVC_DATA128.
  static void UpdateServiceData(
      std::map<Uuid, std::vector<uint8_t>>* service_data,
      uint8_t uuid_size,
      const uint8_t* data,
      uint8_t data_len);

  // Resets the update status of device properties.
  void ClearPropertiesUpdated(Device* device);

  static void PairStateCallbackThunk(void* data,
                                     const void* pair_state_change,
                                     uniq_t observer_id);
  // Called when pairing state changed events are received.
  void PairStateCallback(const smPairStateChange& change, uniq_t observer_id);

  static void PasskeyDisplayObserverCallbackThunk(
      void* data,
      const struct smPasskeyDisplay* passkey_display,
      uniq_t observer_id);
  void PasskeyDisplayObserverCallback(struct smPasskeyDisplay passkey_display,
                                      uniq_t observer_id);

  // Determines the security requirements based on the appearance of a device.
  // Returns true if determined. The default security requirements
  // (bond:true MITM:false) are used.
  struct smPairSecurityRequirements DetermineSecurityRequirements(
      const Device& device);

  // Finds a device from |discovered_devices_| with the given |device_address|.
  // Returns nullptr if no such device is found.
  Device* FindDevice(const std::string& device_address);

  std::unique_ptr<LibNewblue> libnewblue_;

  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;

  base::Closure ready_for_up_callback_;

  DeviceDiscoveredCallback device_discovered_callback_;
  uniq_t discovery_handle_ = 0;
  std::map<std::string, std::unique_ptr<Device>> discovered_devices_;

  uniq_t passkey_display_observer_id_ = 0;

  PairingAgent* pairing_agent_ = nullptr;

  // Handle from security manager of being a central pairing observer. We are
  // responsible of receiving pairing state changed events and informing clients
  // whoever registered as pairing observers.
  uniq_t pair_state_handle_;
  // Contains pairs of <observer ID, callback>. Clients who registered for the
  // pairing update can expect to be notified via callback provided in
  // RegisterAsPairObserver(). For now, Newblued is the only client.
  std::map<UniqueId, PairStateChangedCallback> pair_observers_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<Newblue> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Newblue);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_NEWBLUE_H_
