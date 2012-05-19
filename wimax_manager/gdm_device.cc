// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/gdm_device.h"

#include <base/logging.h>
#include <base/memory/scoped_vector.h>
#include <base/stl_util.h>
#include <chromeos/dbus/service_constants.h>

#include "wimax_manager/gdm_driver.h"
#include "wimax_manager/network.h"
#include "wimax_manager/network_dbus_adaptor.h"

using std::string;
using std::vector;

namespace wimax_manager {

namespace {

const int kMaxNumberOfTrials = 10;

template <size_t N>
bool CopyEAPParameterToUInt8Array(const base::DictionaryValue &parameters,
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

}  // namespace

GdmDevice::GdmDevice(uint8 index, const string &name,
                     const base::WeakPtr<GdmDriver> &driver)
    : Device(index, name),
      driver_(driver),
      open_(false),
      status_(WIMAX_API_DEVICE_STATUS_UnInitialized),
      connection_progress_(WIMAX_API_DEVICE_CONNECTION_PROGRESS_Ranging) {
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

  return true;
}

bool GdmDevice::Disable() {
  if (!driver_ || !open_)
    return false;

  if (!driver_->PowerOffDeviceRF(this)) {
    LOG(ERROR) << "Failed to power off RF of device '" << name() << "'";
    return false;
  }

  return true;
}

bool GdmDevice::ScanNetworks() {
  if (!Open())
    return false;

  ScopedVector<Network> *networks = mutable_networks();
  networks->reset();

  for (int num_trials = 0; num_trials < kMaxNumberOfTrials; ++num_trials) {
    // TODO(benchan): Fix this code.
    sleep(2);
    if (!driver_->GetNetworksForDevice(this, &networks->get())) {
      LOG(ERROR) << "Failed to get list of networks for device '"
                 << name() << "'";
    }
    if (!networks->empty())
      break;
  }

  for (size_t i = 0; i < networks->size(); ++i)
    (*networks)[i]->CreateDBusAdaptor();

  UpdateNetworks();
  return true;
}

bool GdmDevice::Connect(const Network &network,
                        const base::DictionaryValue &parameters) {
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

  return true;
}

bool GdmDevice::ConstructEAPParameters(
    const base::DictionaryValue &connect_parameters,
    GCT_API_EAP_PARAM *eap_parameters) {
  CHECK(eap_parameters);

  memset(eap_parameters, 0, sizeof(GCT_API_EAP_PARAM));
  // TODO(benchan): Allow selection between EAP-TLS and EAP-TTLS;
  eap_parameters->type = GCT_WIMAX_EAP_TLS;
  eap_parameters->fragSize = 1300;
  eap_parameters->useNvramParam = 1;
  eap_parameters->logEnable = 1;

  if (!CopyEAPParameterToUInt8Array(connect_parameters, kEAPAnonymousIdentity,
                                    eap_parameters->anonymousId)) {
    LOG(ERROR) << "Invalid EAP anonymous identity";
    return false;
  }

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

  return true;
}

void GdmDevice::set_mac_address(const uint8 mac_address[6]) {
  memcpy(mac_address_, mac_address, sizeof(mac_address_));
}

}  // namespace wimax_manager
