// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/gdm_device.h"

#include <set>
#include <vector>

#include <base/logging.h>
#include <base/memory/scoped_vector.h>
#include <base/stl_util.h>
#include <base/strings/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "wimax_manager/device_dbus_adaptor.h"
#include "wimax_manager/gdm_driver.h"
#include "wimax_manager/manager.h"
#include "wimax_manager/network.h"
#include "wimax_manager/network_dbus_adaptor.h"
#include "wimax_manager/proto_bindings/eap_parameters.pb.h"
#include "wimax_manager/proto_bindings/network_operator.pb.h"
#include "wimax_manager/utility.h"

using base::DictionaryValue;
using std::set;
using std::string;
using std::vector;

namespace wimax_manager {

namespace {

// Timeout for connecting to a network.
const int kConnectTimeoutInSeconds = 60;
// Initial network scan interval, in seconds, after the device is enabled.
const int kInitialNetworkScanIntervalInSeconds = 1;
// Default time interval, in seconds, between status updates when the device
// is connecting to a network.
const int kStatusUpdateIntervalDuringConnectInSeconds = 1;
const int kShortDelayInSeconds = 1;

const char kRealmTag[] = "@${realm}";

bool ExtractStringParameter(const DictionaryValue &parameters,
                            const string &key,
                            const string &default_value,
                            string *value) {
  if (!parameters.HasKey(key)) {
    *value = default_value;
    return true;
  }

  if (parameters.GetString(key, value))
    return true;

  return false;
}

template <size_t N>
bool CopyStringToUInt8Array(const string &value, UINT8 (&uint8_array)[N]) {
  size_t value_length = value.length();
  if (value_length >= N)
    return false;

  char *char_array = reinterpret_cast<char *>(uint8_array);
  value.copy(char_array, value_length);
  char_array[value_length] = '\0';
  return true;
}

const char *GetEAPTypeString(GCT_API_EAP_TYPE eap_type) {
  switch (eap_type) {
    case GCT_WIMAX_NO_EAP:
      return "No EAP";
    case GCT_WIMAX_EAP_TLS:
      return "TLS";
    case GCT_WIMAX_EAP_TTLS_MD5:
      return "TTLS/MD5";
    case GCT_WIMAX_EAP_TTLS_MSCHAPV2:
      return "TTLS/MS-CHAP v2";
    case GCT_WIMAX_EAP_TTLS_CHAP:
      return "TTLS/CHAP";
    case GCT_WIMAX_EAP_AKA:
      return "AKA";
    default:
      return "Unknown";
  }
}

const char *MaskString(const char *value) {
  return (value && value[0]) ? "<***>" : "";
}

}  // namespace

GdmDevice::GdmDevice(Manager *manager, uint8_t index, const string &name,
                     const base::WeakPtr<GdmDriver> &driver)
    : Device(manager, index, name),
      driver_(driver),
      open_(false),
      connection_progress_(WIMAX_API_DEVICE_CONNECTION_PROGRESS_Ranging),
      restore_status_update_interval_(false),
      current_network_identifier_(Network::kInvalidIdentifier) {
}

GdmDevice::~GdmDevice() {
  Disable();
  Close();
}

bool GdmDevice::Open() {
  if (!driver_)
    return false;

  if (open_)
    return true;

  if (!driver_->OpenDevice(this)) {
    LOG(ERROR) << "Failed to open device '" << name() << "'";
    return false;
  }

  open_ = true;
  return true;
}

bool GdmDevice::Close() {
  if (!driver_)
    return false;

  if (!open_)
    return true;

  if (!driver_->CloseDevice(this)) {
    LOG(ERROR) << "Failed to close device '" << name() << "'";
    return false;
  }

  ClearCurrentConnectionProfile();

  open_ = false;
  return true;
}

bool GdmDevice::Enable() {
  if (!Open())
    return false;

  if (!driver_->GetDeviceStatus(this)) {
    LOG(ERROR) << "Failed to get status of device '" << name() << "'";
    return false;
  }

  if (!driver_->AutoSelectProfileForDevice(this)) {
    LOG(ERROR) << "Failed to auto select profile for device '" << name() << "'";
    return false;
  }

  if (!driver_->PowerOnDeviceRF(this)) {
    LOG(ERROR) << "Failed to power on RF of device '" << name() << "'";
    return false;
  }

  if (!driver_->SetScanInterval(this, network_scan_interval())) {
    LOG(WARNING) << "Failed to set internal network scan by SDK.";
  }

  // Schedule an initial network scan shortly after the device is enabled.
  initial_network_scan_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kInitialNetworkScanIntervalInSeconds),
      this,
      &GdmDevice::OnNetworkScan);

  // Set OnNetworkScan() to be called repeatedly at |network_scan_interval_|
  // intervals to scan and update the list of networks via ScanNetworks().
  //
  // TODO(benchan): Refactor common functionalities like periodic network scan
  // to the Device base class.
  network_scan_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(network_scan_interval()),
      this,
      &GdmDevice::OnNetworkScan);

  status_update_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(status_update_interval()),
      this,
      &GdmDevice::OnStatusUpdate);

  if (!driver_->GetDeviceStatus(this)) {
    LOG(ERROR) << "Failed to get status of device '" << name() << "'";
    return false;
  }
  return true;
}

bool GdmDevice::Disable() {
  if (!driver_ || !open_)
    return false;

  ClearCurrentConnectionProfile();

  restore_status_update_interval_timer_.Stop();
  RestoreStatusUpdateInterval();

  // Cancel any pending connect timeout.
  connect_timeout_timer_.Stop();

  // Cancel any scheduled network scan.
  initial_network_scan_timer_.Stop();
  network_scan_timer_.Stop();

  // Cancel any scheduled status update.
  dbus_adaptor_status_update_timer_.Stop();
  status_update_timer_.Stop();

  NetworkMap *networks = mutable_networks();
  if (!networks->empty()) {
    networks->clear();
    UpdateNetworks();
  }

  if (!driver_->PowerOffDeviceRF(this)) {
    LOG(ERROR) << "Failed to power off RF of device '" << name() << "'";
    return false;
  }

  if (!driver_->GetDeviceStatus(this)) {
    LOG(ERROR) << "Failed to get status of device '" << name() << "'";
    return false;
  }
  return true;
}

bool GdmDevice::ScanNetworks() {
  if (!Open())
    return false;

  vector<NetworkRefPtr> scanned_networks;
  if (!driver_->GetNetworksForDevice(this, &scanned_networks)) {
    LOG(WARNING) << "Failed to get list of networks for device '"
                 << name() << "'";
    // Ignore error and wait for next scan.
    return true;
  }

  bool networks_added = false;
  NetworkMap *networks = mutable_networks();
  set<Network::Identifier> networks_to_remove = GetKeysOfMap(*networks);

  for (size_t i = 0; i < scanned_networks.size(); ++i) {
    Network::Identifier identifier = scanned_networks[i]->identifier();
    NetworkMap::iterator network_iterator = networks->find(identifier);
    if (network_iterator == networks->end()) {
      // Add a newly found network.
      scanned_networks[i]->CreateDBusAdaptor();
      (*networks)[identifier] = scanned_networks[i];
      networks_added = true;
    } else {
      // Update an existing network.
      network_iterator->second->UpdateFrom(*scanned_networks[i]);
    }
    networks_to_remove.erase(identifier);
  }

  // Remove networks that disappeared.
  RemoveKeysFromMap(networks, networks_to_remove);

  // Only call UpdateNetworks(), which emits NetworksChanged signal, when
  // a network is added or removed.
  if (networks_added || !networks_to_remove.empty())
    UpdateNetworks();

  return true;
}

void GdmDevice::OnNetworkScan() {
  ScanNetworks();
}

bool GdmDevice::UpdateStatus() {
  if (!driver_->GetDeviceStatus(this)) {
    LOG(ERROR) << "Failed to get status of device '" << name() << "'";
    return false;
  }

  // Cancel the timeout for connect and restore status update interval
  // if the device is no longer in the 'connecting' state.
  if (connect_timeout_timer_.IsRunning() &&
      status() != kDeviceStatusConnecting) {
    LOG(INFO) << "Disable connect timeout.";
    connect_timeout_timer_.Stop();

    restore_status_update_interval_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kShortDelayInSeconds),
        this,
        &GdmDevice::RestoreStatusUpdateInterval);
  }

  if (!driver_->GetDeviceRFInfo(this)) {
    LOG(ERROR) << "Failed to get RF information of device '" << name() << "'";
    return false;
  }
  return true;
}

void GdmDevice::OnStatusUpdate() {
  UpdateStatus();
}

void GdmDevice::OnDBusAdaptorStatusUpdate() {
  dbus_adaptor()->UpdateStatus();
}

void GdmDevice::UpdateNetworkScanInterval(uint32_t network_scan_interval) {
  if (network_scan_timer_.IsRunning()) {
    LOG(INFO) << "Update network scan interval to " << network_scan_interval
              << "s.";
    network_scan_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(network_scan_interval),
        this,
        &GdmDevice::OnNetworkScan);

    if (!driver_->SetScanInterval(this, network_scan_interval)) {
      LOG(WARNING) << "Failed to set internal network scan by SDK.";
    }
  }
}

void GdmDevice::UpdateStatusUpdateInterval(uint32_t status_update_interval) {
  if (status_update_timer_.IsRunning()) {
    LOG(INFO) << "Update status update interval to " << status_update_interval
              << "s.";
    status_update_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(status_update_interval),
        this,
        &GdmDevice::OnStatusUpdate);
  }
}

void GdmDevice::RestoreStatusUpdateInterval() {
  if (!restore_status_update_interval_)
    return;

  UpdateStatusUpdateInterval(status_update_interval());
  restore_status_update_interval_ = false;

  // Restarts the network scan timeout source to be aligned with the status
  // update timeout source, which helps increase the idle time of the device
  // when both sources are fired and served by the device around the same time.
  UpdateNetworkScanInterval(network_scan_interval());
}

bool GdmDevice::Connect(const Network &network,
                        const DictionaryValue &parameters) {
  if (!Open())
    return false;

  if (networks().empty())
    return false;

  if (!driver_->GetDeviceStatus(this)) {
    LOG(ERROR) << "Failed to get status of device '" << name() << "'";
    return false;
  }

  EAPParameters operator_eap_parameters =
      GetNetworkOperatorEAPParameters(network);

  // TODO(benchan): Refactor this code into Device base class.
  string user_identity;
  ExtractStringParameter(parameters,
                         kEAPUserIdentity,
                         operator_eap_parameters.user_identity(),
                         &user_identity);
  if (status() == kDeviceStatusConnecting ||
      status() == kDeviceStatusConnected) {
    if (current_network_identifier_ == network.identifier() &&
        current_user_identity_ == user_identity) {
      // The device status may remain unchanged, schedule a deferred call to
      // DeviceDBusAdaptor::UpdateStatus() to explicitly notify the connection
      // manager about the current device status.
      dbus_adaptor_status_update_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromSeconds(kShortDelayInSeconds),
          this,
          &GdmDevice::OnDBusAdaptorStatusUpdate);
      return true;
    }

    if (!driver_->DisconnectDeviceFromNetwork(this)) {
      LOG(ERROR) << "Failed to disconnect device '" << name()
                 << "' from network";
      return false;
    }
  }

  GCT_API_EAP_PARAM eap_parameters;
  if (!ConstructEAPParameters(parameters, operator_eap_parameters,
                              &eap_parameters))
    return false;

  VLOG(1) << "Connect to " << network.GetNameWithIdentifier()
          << " via EAP (Type: " << GetEAPTypeString(eap_parameters.type)
          << ", Anonymous identity: '"
          << MaskString(reinterpret_cast<const char *>(
              eap_parameters.anonymousId))
          << "', User identity: '"
          << MaskString(reinterpret_cast<const char *>(eap_parameters.userId))
          << "', User password: '"
          << MaskString(reinterpret_cast<const char *>(
              eap_parameters.userIdPwd))
          << "', Bypass device certificate: "
          << (eap_parameters.devCertNULL == 0 ? false : true)
          << ", Bypass CA certificate: "
          << (eap_parameters.caCertNULL == 0 ? false : true)
          << ")";

  if (!driver_->SetDeviceEAPParameters(this, &eap_parameters)) {
    LOG(ERROR) << "Failed to set EAP parameters on device '" << name() << "'";
    return false;
  }

  if (!driver_->ConnectDeviceToNetwork(this, network)) {
    LOG(ERROR) << "Failed to connect device '" << name()
               << "' to " << network.GetNameWithIdentifier();
    return false;
  }

  restore_status_update_interval_timer_.Stop();
  UpdateStatusUpdateInterval(kStatusUpdateIntervalDuringConnectInSeconds);
  restore_status_update_interval_ = true;

  current_network_identifier_ = network.identifier();
  current_user_identity_ = user_identity;

  // Schedule a timeout to abort the connection attempt in case the device
  // is stuck at the 'connecting' state.
  connect_timeout_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kConnectTimeoutInSeconds),
      this,
      &GdmDevice::CancelConnectOnTimeout);

  if (!driver_->GetDeviceStatus(this)) {
    LOG(ERROR) << "Failed to get status of device '" << name() << "'";
    return false;
  }
  return true;
}

bool GdmDevice::Disconnect() {
  if (!driver_ || !open_)
    return false;

  if (!driver_->DisconnectDeviceFromNetwork(this)) {
    LOG(ERROR) << "Failed to disconnect device '" << name() << "' from network";
    return false;
  }

  ClearCurrentConnectionProfile();

  restore_status_update_interval_timer_.Stop();
  RestoreStatusUpdateInterval();

  if (!driver_->GetDeviceStatus(this)) {
    LOG(ERROR) << "Failed to get status of device '" << name() << "'";
    return false;
  }
  return true;
}

void GdmDevice::CancelConnectOnTimeout() {
  LOG(WARNING) << "Timed out connecting to the network.";
  Disconnect();
}

void GdmDevice::ClearCurrentConnectionProfile() {
  current_network_identifier_ = Network::kInvalidIdentifier;
  current_user_identity_.clear();
}

// static
bool GdmDevice::ConstructEAPParameters(
    const DictionaryValue &connect_parameters,
    const EAPParameters &operator_eap_parameters,
    GCT_API_EAP_PARAM *eap_parameters) {
  CHECK(eap_parameters);

  memset(eap_parameters, 0, sizeof(GCT_API_EAP_PARAM));
  eap_parameters->fragSize = 1300;
  eap_parameters->logEnable = 1;

  switch (operator_eap_parameters.type()) {
    case EAP_TYPE_TLS:
      eap_parameters->type = GCT_WIMAX_EAP_TLS;
      break;
    case EAP_TYPE_TTLS_MD5:
      eap_parameters->type = GCT_WIMAX_EAP_TTLS_MD5;
      break;
    case EAP_TYPE_TTLS_MSCHAPV2:
      eap_parameters->type = GCT_WIMAX_EAP_TTLS_MSCHAPV2;
      break;
    case EAP_TYPE_TTLS_CHAP:
      eap_parameters->type = GCT_WIMAX_EAP_TTLS_CHAP;
      break;
    case EAP_TYPE_AKA:
      eap_parameters->type = GCT_WIMAX_EAP_AKA;
      break;
    default:
      eap_parameters->type = GCT_WIMAX_NO_EAP;
      break;
  }

  if (operator_eap_parameters.bypass_device_certificate())
    eap_parameters->devCertNULL = 1;

  if (operator_eap_parameters.bypass_ca_certificate())
    eap_parameters->caCertNULL = 1;

  string user_identity;
  if (!ExtractStringParameter(connect_parameters,
                              kEAPUserIdentity,
                              operator_eap_parameters.user_identity(),
                              &user_identity) ||
      !CopyStringToUInt8Array(user_identity, eap_parameters->userId)) {
    LOG(ERROR) << "Invalid EAP user identity";
    return false;
  }

  string user_password;
  if (!ExtractStringParameter(connect_parameters,
                              kEAPUserPassword,
                              operator_eap_parameters.user_password(),
                              &user_password) ||
      !CopyStringToUInt8Array(user_password, eap_parameters->userIdPwd)) {
    LOG(ERROR) << "Invalid EAP user password";
    return false;
  }

  string anonymous_identity;
  if (!ExtractStringParameter(connect_parameters,
                              kEAPAnonymousIdentity,
                              operator_eap_parameters.anonymous_identity(),
                              &anonymous_identity)) {
    LOG(ERROR) << "Invalid EAP anonymous identity";
    return false;
  }

  // If the anonymous identity contains a realm tag ('@${realm}'),
  // replace it with the realm extracted from the user identity.
  string realm;
  size_t realm_pos = user_identity.find('@');
  if (realm_pos != string::npos)
    realm = user_identity.substr(realm_pos);

  ReplaceSubstringsAfterOffset(&anonymous_identity, 0, kRealmTag, realm);

  if (!CopyStringToUInt8Array(anonymous_identity,
                              eap_parameters->anonymousId)) {
    LOG(ERROR) << "Invalid EAP anonymous identity";
    return false;
  }

  return true;
}

EAPParameters GdmDevice::GetNetworkOperatorEAPParameters(
    const Network &network) const {
  const NetworkOperator *network_operator =
      manager()->GetNetworkOperator(network.identifier());
  if (network_operator)
    return network_operator->eap_parameters();

  LOG(INFO) << "No network operator information specified for "
            << network.GetNameWithIdentifier();
  return EAPParameters();
}

}  // namespace wimax_manager
