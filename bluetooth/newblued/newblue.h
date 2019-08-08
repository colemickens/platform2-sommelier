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
#include <base/location.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <base/single_thread_task_runner.h>
#include <newblue/att.h>

#include "bluetooth/newblued/libnewblue.h"
#include "bluetooth/newblued/property.h"
#include "bluetooth/newblued/util.h"
#include "bluetooth/newblued/uuid.h"

namespace bluetooth {

// The value(s) is/are based on libnewblue API.
constexpr gatt_client_conn_t kInvalidGattConnectionId = 0;

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

// These are based on the pairing errors defined in newblue/sm.h.
enum class PairError : uint8_t {
  NONE,
  ALREADY_PAIRED,
  IN_PROGRESS,
  INVALID_PAIR_REQ,
  PASSKEY_FAILED,
  OOB_NOT_AVAILABLE,
  AUTH_REQ_INFEASIBLE,
  CONF_VALUE_MISMATCHED,
  PAIRING_NOT_SUPPORTED,
  ENCR_KEY_SIZE,
  REPEATED_ATTEMPT,
  INVALID_PARAM,
  MEMORY,
  L2C_CONN,
  NO_SUCH_DEVICE,
  UNEXPECTED_SM_CMD,
  SEND_SM_CMD,
  ENCR_CONN,
  UNEXPECTED_L2C_EVT,
  STALLED,
  UNKNOWN
};

// These are based on GATT_CLI_STATUS_* in newblue/gatt.h
enum class GattClientOperationStatus : uint8_t {
  OK,
  OTHER_SIDE_DISC,
  ERR,
  WE_DISC
};

// These are based on ATT_ERROR_* in newblue/att.h.
enum class AttError : uint8_t {
  NONE = ATT_ERROR_NONE,
  INVALID_HANDLE = ATT_ERROR_INVALID_HANDLE,
  READ_NOT_ALLOWED = ATT_ERROR_READ_NOT_ALLOWED,
  WRITE_NOT_ALLOWED = ATT_ERROR_WRITE_NOT_ALLOWED,
  INVALID_PDU = ATT_ERROR_INVALID_PDU,
  INSUFF_AUTHN = ATT_ERROR_INSUFFICIENT_AUTH,
  REQ_NOT_SUPPORTED = ATT_ERROR_REQ_NOT_SUPPORTED,
  INVALID_OFFSET = ATT_ERROR_INVALID_OFFSET,
  INSUFF_AUTHZ = ATT_ERROR_INSUFFICIENT_ATHOR,
  PREPARE_QUEUE_FULL = ATT_ERROR_PREPARE_QUEUE_FULL,
  ATTR_NOT_FOUND = ATT_ERROR_ATTRIBUTE_NOT_FOUND,
  ATTR_NOT_LONG = ATT_ERROR_ATTRIBUTE_NOT_LONG,
  INSUFF_ENCR_KEY_SIZE = ATT_ERROR_INSUFF_ENCR_KEY_SZ,
  INVALID_ATTR_VALUE_LENGTH = ATT_ERROR_INVAL_ATTR_VAL_LEN,
  UNLIKELY_ERROR = ATT_ERROR_UNLIKELY_ERROR,
  INSUFF_ENCR = ATT_ERROR_INSUFFICIENT_ENCR,
  UNSUPPORTED_GROUP_TYPE = ATT_ERROR_UNSUPP_GROUP_TYPE,
  INSUFF_RESOURCES = ATT_ERROR_INSUFFICIENT_RSRCS,
  APP_ERROR_FIRST = ATT_ERROR_APP_ERR_FIRST,
  APP_ERROR_LAST = ATT_ERROR_APP_ERR_LAST,
  COMMON_FIRST = ATT_ERROR_COMMON_FIRST,
  COMMON_LAST = ATT_ERROR_COMMON_LAST,
};

// TODO(mcchou): Add more entries for GATT client operations.
// The is based on gattCli*Cbk defined in newblue/gatt.h.
enum class GattClientOperationType {
  SERVICES_ENUM,
  PRIMARY_SERVICE_TRAV,
  READ_LONG_VALUE,
};

// These are based on GATT_CLI_AUTH_REQ_* defined in newblue/gatt.h.
enum class GattClientOperationAuthentication : uint8_t {
  NONE,
  AUTHN_NO_MITM,
  AUTHN_MITM,
  AUTHN_SIGNED_NO_MITM,
  AUTHN_SIGNED_MITM,
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

// Structure representing a known device.
struct KnownDevice {
  // The known device name.
  std::string name;
  // MAC address (in format XX:XX:XX:XX:XX:XX).
  std::string address;
  // libnewblue's BT_ADDR_TYPE_*
  uint8_t address_type;
  // Whether the device was previously paired.
  bool is_paired;
  // Identity address (in format XX:XX:XX:XX:XX:XX).
  std::string identity_address;
};

// A higher-level API wrapper of the low-level libnewblue C interface.
// This class abstracts away the C interface details of libnewblue and provides
// event handling model that is compatible with libchrome's main loop.
class Newblue {
 public:
  using DeviceDiscoveredCallback =
      base::Callback<void(const std::string& adv_address,
                          uint8_t address_type,
                          const std::string& resolved_address,
                          int8_t rssi,
                          uint8_t reply_type,
                          const std::vector<uint8_t>& eir)>;

  using PairStateChangedCallback =
      base::Callback<void(const std::string& address,
                          PairState pair_state,
                          PairError pair_error,
                          const std::string& identity_address)>;

  using GattClientConnectCallback =
      base::Callback<void(gatt_client_conn_t conn_id, uint8_t status)>;

  using GattClientServicesEnumCallback =
      base::Callback<void(bool finished,
                          gatt_client_conn_t conn_id,
                          UniqueId transaction_id,
                          Uuid uuid,
                          bool primary,
                          uint16_t first_handle,
                          uint16_t num_handles,
                          GattClientOperationStatus status)>;

  using GattClientPrimaryServiceTravCallback =
      base::Callback<void(gatt_client_conn_t conn_id,
                          UniqueId transaction_id,
                          std::unique_ptr<GattService> service)>;

  // Empty |value| indicates error. If |value| is empty, |error| should be
  // any AttError code other than AttError::NONE.
  using GattClientReadLongValueCallback =
      base::Callback<void(gatt_client_conn_t conn,
                          UniqueId transaction_id,
                          uint16_t value_handle,
                          AttError error,
                          const std::vector<uint8_t>& value)>;

  // Represents a GATT client operation.
  struct GattClientOperation {
    GattClientOperationType type;
    GattClientServicesEnumCallback services_enum_callback;
    GattClientPrimaryServiceTravCallback service_trav_callback;
    GattClientReadLongValueCallback read_long_value_callback;
  };

  explicit Newblue(std::unique_ptr<LibNewblue> libnewblue);

  ~Newblue() = default;

  LibNewblue* libnewblue() { return libnewblue_.get(); }

  base::WeakPtr<Newblue> GetWeakPtr();

  // Initializes the LE stack (blocking call).
  // Returns true if initialization succeeds, false otherwise.
  bool Init();

  // Registers/Unregisters pairing agent which handles user interactions during
  // pairing process.
  // |pairing_agent| not owned, must outlive this object.
  void RegisterPairingAgent(PairingAgent* pairing_agent);
  void UnregisterPairingAgent();

  // Listens to reset complete event from the chip. This is useful to detect
  // when NewBlue is ready to bring up the stack.
  // Returns true on success and false otherwise.
  bool ListenReadyForUp(base::Closure callback);

  // Brings up the NewBlue stack. This should be called when the adapter has
  // just been turned on, detected when there is reset complete event from the
  // chip. ListenReadyForUp() should be used to detect this event.
  // Returns true if success, false otherwise.
  bool BringUp();

  // Starts LE scanning, |callback| will be called every time inquiry response
  // is received.
  bool StartDiscovery(DeviceDiscoveredCallback callback);
  // Stops LE scanning.
  bool StopDiscovery();

  // Registers as an observer of pairing states of devices.
  UniqueId RegisterAsPairObserver(PairStateChangedCallback callback);
  // Unregisters as an observer of pairing states.
  void UnregisterAsPairObserver(UniqueId observer_id);

  // Performs LE pairing.
  bool Pair(const std::string& device_address,
            bool is_random_address,
            smPairSecurityRequirements security_requirement);
  // Cancels LE pairing.
  bool CancelPair(const std::string& device_address, bool is_random_address);

  // Registers/Unregisters a callback for GATT connection state change
  // notifications. We allow only one observer for connection callback.
  bool RegisterGattClientConnectCallback(GattClientConnectCallback callback);
  void UnregisterGattClientConnectCallback();
  // Connects as a GATT client to a peer device.
  gatt_client_conn_t GattClientConnect(const std::string& device_address,
                                       bool is_random_address);
  // Disconnects from a peer device.
  GattClientOperationStatus GattClientDisconnect(gatt_client_conn_t conn_id);

  // Retrieves the known devices from NewBlue's persist.
  std::vector<KnownDevice> GetKnownDevices();

  // Enumerates GATT services provided by a connected device.
  GattClientOperationStatus GattClientEnumServices(
      gatt_client_conn_t conn_id,
      bool primary,
      UniqueId transaction_id,
      GattClientServicesEnumCallback callback);

  // Traverses the attributes included in a primary service.
  GattClientOperationStatus GattClientTravPrimaryService(
      gatt_client_conn_t conn_id,
      const Uuid& uuid,
      UniqueId transaction_id,
      GattClientPrimaryServiceTravCallback callback);

  // Reads the complete value of a GATT characteristic or a GATT descriptor.
  GattClientOperationStatus GattClientReadLongValue(
      gatt_client_conn_t conn_id,
      uint16_t value_handle,
      GattClientOperationAuthentication authentication,
      UniqueId transaction_id,
      GattClientReadLongValueCallback callback);

 private:
  // Posts task to the thread which created this Newblue object.
  // libnewblue callbacks should always post task using this method rather
  // than doing any processing in the callback's thread.
  bool PostTask(const base::Location& from_here, const base::Closure& task);

  // Called by NewBlue when it's ready to bring up the stack. This is called
  // on one of NewBlue's threads, so we shouldn't do anything on this thread
  // other than posting the task to our mainloop thread.
  static void OnStackReadyForUpThunk(void* data);
  // Triggers the callback registered via ListenReadyForUp().
  void OnStackReadyForUp();

  static void DiscoveryCallbackThunk(void* data,
                                     const struct bt_addr* adv_address,
                                     const struct bt_addr* resolved_address,
                                     int8_t rssi,
                                     uint8_t reply_type,
                                     const void* eir,
                                     uint8_t eir_len);
  // Called when inquiry response is received as a result of StartDiscovery().
  void DiscoveryCallback(const std::string& adv_address,
                         uint8_t address_type,
                         const std::string& resolved_address,
                         int8_t rssi,
                         uint8_t reply_type,
                         const std::vector<uint8_t>& eir);

  static void PairStateCallbackThunk(void* data,
                                     const void* pair_state_change,
                                     uniq_t observer_id);
  // Called when pairing state changed events are received.
  void PairStateCallback(const smPairStateChange& change, uniq_t observer_id);

  static void GattConnectCallbackThunk(void* user_data,
                                       gatt_client_conn_t conn,
                                       uint8_t status);

  static void GattClientEnumServicesCallbackThunk(void* user_data,
                                                  gatt_client_conn_t conn_id,
                                                  uniq_t transaction_id,
                                                  const struct uuid* uuid,
                                                  bool primary,
                                                  uint16_t first_handle,
                                                  uint16_t num_handles,
                                                  uint8_t status);
  // Called when the service enumeration results are received.
  void GattClientEnumServicesCallback(gatt_client_conn_t conn_id,
                                      uniq_t transaction_id,
                                      Uuid uuid,
                                      bool primary,
                                      uint16_t first_handle,
                                      uint16_t num_handles,
                                      uint8_t status);

  static void GattClientTravPrimaryServiceCallbackThunk(
      void* user_data,
      gatt_client_conn_t conn,
      uniq_t transaction_id,
      const struct GattTraversedService* service);
  // Called when the primary service traversal result is received.
  void GattClientTravPrimaryServiceCallback(
      gatt_client_conn_t conn_id,
      uniq_t transaction_id,
      std::unique_ptr<GattService> service);

  static void GattClientReadLongCallbackThunk(void* user_data,
                                              gatt_client_conn_t conn,
                                              uniq_t transaction_id,
                                              uint16_t handle,
                                              uint8_t error,
                                              sg data);
  // Called when the result of reading long value is received.
  void GattClientReadLongCallback(gatt_client_conn_t conn,
                                  uniq_t transaction_id,
                                  uint16_t handle,
                                  AttError error,
                                  std::vector<uint8_t> value);

  static void PasskeyDisplayObserverCallbackThunk(
      void* data,
      const struct smPasskeyDisplay* passkey_display,
      uniq_t observer_id);
  void PasskeyDisplayObserverCallback(struct smPasskeyDisplay passkey_display,
                                      uniq_t observer_id);

  std::unique_ptr<LibNewblue> libnewblue_;

  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;

  base::Closure ready_for_up_callback_;

  DeviceDiscoveredCallback device_discovered_callback_;
  uniq_t discovery_handle_ = 0;

  uniq_t passkey_display_observer_id_ = 0;

  PairingAgent* pairing_agent_ = nullptr;

  // Handle from security manager of being a central pairing observer. We are
  // responsible of receiving pairing state changed events and informing
  // clients whoever registered as pairing observers.
  uniq_t pair_state_handle_;
  // Contains pairs of <observer ID, callback>. Clients who registered for the
  // pairing update can expect to be notified via callback provided in
  // RegisterAsPairObserver(). For now, Newblued is the only client.
  std::map<UniqueId, PairStateChangedCallback> pair_observers_;

  GattClientConnectCallback gatt_client_connect_callback_;

  // Contains pairs of <transaction ID, GATT operation> which track the ongoing
  // GATT client operations.
  std::map<UniqueId, GattClientOperation> gatt_client_ops_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<Newblue> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Newblue);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_NEWBLUE_H_
