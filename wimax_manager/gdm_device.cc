// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/gdm_device.h"

#include <set>
#include <vector>

#include <base/logging.h>
#include <base/memory/scoped_vector.h>
#include <base/stl_util.h>
#include <chromeos/dbus/service_constants.h>

#include "wimax_manager/device_dbus_adaptor.h"
#include "wimax_manager/gdm_driver.h"
#include "wimax_manager/network.h"
#include "wimax_manager/network_dbus_adaptor.h"
#include "wimax_manager/utility.h"

using base::DictionaryValue;
using std::set;
using std::string;
using std::vector;

namespace wimax_manager {

namespace {

const int kStatusUpdateIntervalInSeconds = 3;

template <size_t N>
bool CopyEAPParameterToUInt8Array(const DictionaryValue &parameters,
                                  const string &key, UINT8 (&uint8_array)[N]) {
  if (!parameters.HasKey(key)) {
    uint8_array[0] = '\0';
    return true;
  }

  string value;
  if (!parameters.GetString(key, &value))
    return false;

  size_t value_length = value.length();
  if (value_length >= N)
    return false;

  char *char_array = reinterpret_cast<char *>(uint8_array);
  value.copy(char_array, value_length);
  char_array[value_length] = '\0';
  return true;
}

gboolean OnNetworkScan(gpointer data) {
  CHECK(data);

  reinterpret_cast<GdmDevice *>(data)->ScanNetworks();

  // Return TRUE to keep calling this function repeatedly.
  return TRUE;
}

gboolean OnStatusUpdate(gpointer data) {
  CHECK(data);

  reinterpret_cast<GdmDevice *>(data)->UpdateStatus();

  // Return TRUE to keep calling this function repeatedly.
  return TRUE;
}

}  // namespace

GdmDevice::GdmDevice(uint8 index, const string &name,
                     const base::WeakPtr<GdmDriver> &driver)
    : Device(index, name),
      driver_(driver),
      open_(false),
      connection_progress_(WIMAX_API_DEVICE_CONNECTION_PROGRESS_Ranging),
      scan_timeout_id_(0),
      status_update_timeout_id_(0) {
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

  // Set OnNetworkScan() to be called repeatedly at |scan_interval_| intervals
  // to scan and update the list of networks via ScanNetworks().
  //
  // TODO(benchan): Refactor common functionalities like periodic network scan
  // to the Device base class.
  if (scan_timeout_id_ == 0) {
    scan_timeout_id_ =
        g_timeout_add_seconds(scan_interval(), OnNetworkScan, this);
  }

  if (status_update_timeout_id_ == 0) {
    status_update_timeout_id_ = g_timeout_add_seconds(
        kStatusUpdateIntervalInSeconds, OnStatusUpdate, this);
  }

  if (!driver_->GetDeviceStatus(this)) {
    LOG(ERROR) << "Failed to get status of device '" << name() << "'";
    return false;
  }
  return true;
}

bool GdmDevice::Disable() {
  if (!driver_ || !open_)
    return false;

  // Cancel the periodic calls to OnNetworkScan().
  if (scan_timeout_id_ != 0) {
    g_source_remove(scan_timeout_id_);
    scan_timeout_id_ = 0;
  }

  // Cancel the periodic calls to OnStatusUpdate().
  if (status_update_timeout_id_ != 0) {
    g_source_remove(status_update_timeout_id_);
    status_update_timeout_id_ = 0;
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

bool GdmDevice::UpdateStatus() {
  if (!driver_->GetDeviceStatus(this)) {
    LOG(ERROR) << "Failed to get status of device '" << name() << "'";
    return false;
  }
  if (!driver_->GetDeviceRFInfo(this)) {
    LOG(ERROR) << "Failed to get RF information of device '" << name() << "'";
    return false;
  }
  return true;
}

bool GdmDevice::Connect(const Network &network,
                        const DictionaryValue &parameters) {
  if (!Open())
    return false;

  if (networks().empty())
    return false;

  GCT_API_EAP_PARAM eap_parameters;
  if (!ConstructEAPParameters(parameters, &eap_parameters))
    return false;

  if (!driver_->SetDeviceEAPParameters(this, &eap_parameters)) {
    LOG(ERROR) << "Failed to set EAP parameters on device '" << name() << "'";
    return false;
  }

  if (!driver_->ConnectDeviceToNetwork(this, network)) {
    LOG(ERROR) << "Failed to connect device '" << name()
               << "' to network '" << network.name() << "' ("
               << network.identifier() << ")";
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

  if (!driver_->GetDeviceStatus(this)) {
    LOG(ERROR) << "Failed to get status of device '" << name() << "'";
    return false;
  }
  return true;
}

// static
bool GdmDevice::ConstructEAPParameters(
    const DictionaryValue &connect_parameters,
    GCT_API_EAP_PARAM *eap_parameters) {
  CHECK(eap_parameters);

  memset(eap_parameters, 0, sizeof(GCT_API_EAP_PARAM));
  // TODO(benchan): Allow selection between EAP-TLS and EAP-TTLS;
  eap_parameters->type = GCT_WIMAX_EAP_TTLS_MSCHAPV2;
  eap_parameters->fragSize = 1300;
  eap_parameters->logEnable = 1;

  if (!CopyEAPParameterToUInt8Array(connect_parameters, kEAPUserIdentity,
                                    eap_parameters->userId)) {
    LOG(ERROR) << "Invalid EAP user identity";
    return false;
  }

  if (!CopyEAPParameterToUInt8Array(connect_parameters, kEAPUserPassword,
                                    eap_parameters->userIdPwd)) {
    LOG(ERROR) << "Invalid EAP user password";
    return false;
  }

  const DictionaryValue *connect_parameters_ptr = &connect_parameters;
  DictionaryValue updated_connect_parameters;
  string user_id;
  // If no anonymous identity is given, extract <realm> from the user identity
  // and use RANDOM@<realm> as the anonymous identity for EAP-TTLS.
  //
  // TODO(benchan): Not sure if this should be pushed via ONC as it seems to be
  // GDM specific.
  if (!connect_parameters.HasKey(kEAPAnonymousIdentity) &&
      connect_parameters.GetString(kEAPUserIdentity, &user_id)) {
    size_t realm_pos = user_id.find('@');
    if (realm_pos != string::npos) {
      string anonymous_id = "RANDOM" + user_id.substr(realm_pos);
      updated_connect_parameters.SetString(kEAPAnonymousIdentity, anonymous_id);
      connect_parameters_ptr = &updated_connect_parameters;
    }
  }

  if (!CopyEAPParameterToUInt8Array(*connect_parameters_ptr,
                                    kEAPAnonymousIdentity,
                                    eap_parameters->anonymousId)) {
    LOG(ERROR) << "Invalid EAP anonymous identity";
    return false;
  }

  return true;
}

}  // namespace wimax_manager
