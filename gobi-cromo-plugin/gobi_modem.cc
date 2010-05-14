// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi_modem.h"

#include <algorithm>

#include <glog/logging.h>
#include <mm/mm-modem.h>

#include "gobi_sdk_wrapper.h"

GobiModem* GobiModem::connected_modem_(NULL);

static void QueryGobi(gobi::Sdk *sdk_);

GobiModem::GobiModem(DBus::Connection& connection,
                     const DBus::Path& path,
                     GobiModemHandler *handler,
                     const DEVICE_ELEMENT &device,
                     gobi::Sdk *sdk)
    : DBus::ObjectAdaptor(connection, path),
      handler_(handler),
      sdk_(sdk),
      last_seen_(-1),
      activation_state_(0) {
  memcpy(&device_, &device, sizeof(device_));

  pthread_mutex_init(&activation_mutex_, NULL);
  pthread_cond_init(&activation_cond_, NULL);

  // Initialize DBus object properties
  Device = device_.deviceNode;
  Driver = "";
  Enabled = false;
  IpMethod = MM_MODEM_IP_METHOD_DHCP;
  MasterDevice = "TODO(rochberg)";
  // TODO(rochberg):  Gobi can be either
  Type = MM_MODEM_TYPE_CDMA;
  UnlockRequired = "";

  // Reviewer note:  These are just temporary calls
  Enable(true);
  LOG(INFO) << "Enabled: ";

  QueryGobi(sdk_);
}

// DBUS Methods: Modem
void GobiModem::Enable(const bool& enable) {
  LOG(INFO) << "Enable: " << enable;
  LOG(INFO) << "Enabled: " << Enabled();
  if (enable && !Enabled()) {
    if (!ApiConnect()) {
      // TODO(rochberg): ERROR
      CHECK(0);
    }
    if (!EnsureFirmwareLoaded("Sprint")) {
      // TODO(rochberg): ERROR
      CHECK(0);
    }
    Enabled = true;
  }

}

void GobiModem::Connect(const std::string& unused_number) {
  if (!Enabled()) {
    // TODO(rochberg): ERROR
    LOG(WARNING) << "Connect on disabled modem";
    return;
  }

  EnsureActivated();
  ULONG rc;
  ULONG session_id;
  ULONG failure_reason;
  LOG(INFO) << "Starting data session: ";
  rc = StartDataSession(NULL,  // technology
                        NULL,  // Primary DNS
                        NULL,  // Secondary DNS
                        NULL,  // Primary NetBIOS NS
                        NULL,  // Secondary NetBIOS NS
                        NULL,  // APN Name
                        NULL,  // Preferred IP address
                        NULL,  // Authentication
                        NULL,  // Username
                        NULL,  // Password
                        &session_id,  // OUT: session ID
                        &failure_reason  // OUT: failure reason
                        );
  if (rc != 0) {
    LOG(WARNING) << "StartDataSession failed: " << rc;
  }
  LOG(INFO) << "Session ID " <<  session_id;
  LOG(INFO) << "Failure Reason " <<  failure_reason;
}

void GobiModem::Disconnect() {
  LOG(INFO) << "Disconnect";
}

DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> GobiModem::GetIP4Config() {
  DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> result;

  LOG(INFO) << "GetIP4Config";

  return result;
}

DBus::Struct<std::string, std::string, std::string> GobiModem::GetInfo() {
  DBus::Struct<std::string, std::string, std::string> result;
  return result;
}

// DBUS Methods: ModemSimple
void GobiModem::Connect(const DBusPropertyMap& properties) {
  LOG(INFO) << "Simple.Connect";
}

DBusPropertyMap GobiModem::GetStatus() {
  DBusPropertyMap result;

  int32_t rssi;

  // TODO(rochberg):  Much more than just rssi
  if (GetSignalStrengthDbm(&rssi)) {
    result["signal_strength_dbm"].writer().append_int32(rssi);
  }
  return result;
}

  // DBUS Methods: ModemCDMA

std::string GobiModem::GetEsn() {
  LOG(INFO) << "GetEsn";

  return "12345";
}


uint32_t GobiModem::GetSignalQuality() {
  if (!Enabled()) {
    // TODO(rochberg): ERROR
    LOG(WARNING) << "GetSignalQuality on disabled modem";
    return 0;
  }
  // TODO(rochberg): Map dBM to quality
  return 11;
}


DBus::Struct<uint32_t, std::string, uint32_t> GobiModem::GetServingSystem() {
  DBus::Struct<uint32_t, std::string, uint32_t> result;
  LOG(INFO) << "GetServingSystem";

  return result;
}

void GobiModem::GetRegistrationState(uint32_t& cdma_1x_state,
                                     uint32_t& evdo_state) {
  LOG(INFO) << "GetRegistrationState";
}

bool GobiModem::ApiConnect() {
  ULONG rc;
  rc = sdk_->QCWWANConnect(device_.deviceNode, device_.deviceKey);
  if (rc != 0) {
    LOG(WARNING) << "Could not connect to Gobi modem: " << rc;
    return false;
  }
  connected_modem_ = this;

  SetActivationStatusCallback(ActivationStatusTrampoline);
  SetNMEAPlusCallback(NmeaPlusCallbackTrampoline);

  return true;
}

static void QueryGobi(gobi::Sdk *sdk_) {
  const int SIZE=128;
  ULONG rc;

  char buffer[SIZE];
  rc = sdk_->GetManufacturer(sizeof(buffer), buffer);
  LOG(INFO) << "Manufacturer: " << buffer;

  ULONG firmware_id;
  ULONG technology;
  ULONG carrier;
  ULONG region;
  ULONG gps_capability;
  rc = sdk_->GetFirmwareInfo(&firmware_id,
                             &technology,
                             &carrier,
                             &region,
                             &gps_capability);
  CHECK(rc == 0) << rc ;
  LOG(INFO) << "Firmware info: "
            << "firmware_id: " << firmware_id
            << " technology: " << technology
            << " carrier: " << carrier
            << " region: " << region
            << " gps_capability: " << gps_capability;

  char amss[SIZE], boot[SIZE], pri[SIZE];
  rc = sdk_->GetFirmwareRevisions(SIZE, amss, SIZE, boot, SIZE, pri);
  CHECK(rc == 0) << rc;

  LOG(INFO) << "Firmware Revisions: AMSS: " << amss
            << " boot: " << boot
            << " pri: " << pri;

  rc = GetImageStore(sizeof(buffer), buffer);
  CHECK(rc == 0) << rc;
  LOG(INFO) << "ImageStore: " << buffer;

  char esn[SIZE], imei[SIZE], meid[SIZE];
  rc = GetSerialNumbers(SIZE, esn, SIZE, imei, SIZE, meid);
  LOG(INFO) << "ESN: " << esn;
  LOG(INFO) << "IMEI: " << imei;
  LOG(INFO) << "MEID: " << meid;

  char number[SIZE], min_array[SIZE];
  rc = GetVoiceNumber(SIZE, number, SIZE, min_array);
  LOG(INFO) << "Voice: " << number
            << " MIN: " << min_array;
}

bool GobiModem::ResetModem() {
  ULONG rc;

  // TODO(rochberg):  Is this going to confuse connman?
  Enabled = false;

  LOG(INFO) << "Offline";
  rc = sdk_->SetPower(gobi::kOffline);
  if (rc != 0) {
    LOG(WARNING) << "Offline: " << rc;
    // TODO(rochberg):  Disconnect?
    return false;
  }

  LOG(INFO) << "Reset";
  rc = sdk_->SetPower(gobi::kReset);
  if (rc != 0) {
    LOG(WARNING) << "Offline: " << rc;
    // TODO(rochberg):  Disconnect?
    return false;
  }

  rc = QCWWANDisconnect();
  if (rc != 0) {
    LOG(WARNING) << "QCWWANDisconnect: " << rc;
    return false;
  }


  // TODO(rochberg): Timeout
  // TODO(rochberg): This reconnects right away before the modem is
  // happy.  I wonder if we have to further stop the library somehow
  while (true) {
    rc = sdk_->QCWWANConnect(device_.deviceNode, device_.deviceKey);
    if (rc == 0) {
      break;
    }
    usleep(100 * 1000);
  }

  if (rc != 0) {
    // TODO(rochberg):  Send DeviceRemoved
    LOG(WARNING) << "Modem did not come back after reset";
    return false;
  }

  Enabled = true;
  return true;
}

bool GobiModem::EnsureActivated() {
  ULONG rc;

  // TODO(rochberg):  This flow is VZW-specific
  rc = sdk_->GetActivationState(&activation_state_);
  LOG(INFO) << "Activation state: " << activation_state_;
  if (activation_state_ == gobi::kActivated) {
    return true;
  }

  if (activation_state_ != gobi::kNotActivated) {
    LOG(WARNING) << "Unexpected activation state: " << activation_state_;
    return false;
  }

  LOG(INFO) << "Activating";
  rc = sdk_->ActivateAutomatic("*22899");

  LOG(INFO) << "Waiting for activation to complete";
  // TODO(rochberg):  Timeout
  pthread_mutex_lock(&activation_mutex_);
  while (activation_state_ != gobi::kActivated) {
    pthread_cond_wait(&activation_cond_, &activation_mutex_);
  }
  pthread_mutex_unlock(&activation_mutex_);

  rc = sdk_->QCWWANDisconnect();
  Enabled = false;
  if (rc != 0) {
    LOG(WARNING) << "QCWWANDisconnect(): " << rc;
    return false;
  }
  return ResetModem();
}

// TODO(rochberg):  Make this into a class, add dBM -> % maps
struct Carrier {
  const char *name;
  const char *firmware_directory;
  ULONG carrier_id;
  int   carrier_type;
};

static const Carrier kCarrierList[] = {
  // This is only a subset of the available carriers
  {"Vodafone", "0", 202, MM_MODEM_TYPE_GSM},
  {"Verizon Wireless", "1", 101, MM_MODEM_TYPE_CDMA},
  {"AT&T", "2", 201, MM_MODEM_TYPE_GSM},
  {"Sprint", "3", 102, MM_MODEM_TYPE_CDMA},
  {"T-Mobile", "4", 203, MM_MODEM_TYPE_GSM},
  {NULL, NULL, -1, -1},
};

bool GobiModem::EnsureFirmwareLoaded(const char* carrier_name) {
  const Carrier *carrier = NULL;
  for (size_t i = 0; kCarrierList[i].name; ++i) {
    if (strcmp(carrier_name, kCarrierList[i].name) == 0) {
      carrier = &kCarrierList[i];
      break;
    }
  }
  if (!carrier) {
    // TODO(rochberg):  Do we need to sanitize this string?
    LOG(WARNING) << "Could not parse carrier: " << carrier_name;
    return false;
  }

  ULONG rc;
  ULONG firmware_id;
  ULONG technology;
  ULONG modem_carrier_id;
  ULONG region;
  ULONG gps_capability;
  rc = sdk_->GetFirmwareInfo(&firmware_id,
                             &technology,
                             &modem_carrier_id,
                             &region,
                             &gps_capability);

  if (rc != 0) {
    LOG(WARNING) << "Could not get firmware info: " << rc;
    return false;
  }

  if (modem_carrier_id != carrier->carrier_id) {
    std::string image_path = "/opt/Qualcomm/Images2k/HP/";
    image_path += carrier->firmware_directory;

    LOG(INFO) << "Current Gobi carrier: " << modem_carrier_id
              << ".  Upgrading Gobi firmware with "
              << image_path;
    rc = UpgradeFirmware(const_cast<CHAR *>(image_path.c_str()));
    if (rc != 0) {
      LOG(WARNING) << "Firmware load from " << image_path << " failed: " << rc;
      return false;
    }

    if (!ResetModem()) {
      return false;
    }

    rc = sdk_->GetFirmwareInfo(&firmware_id,
                               &technology,
                               &modem_carrier_id,
                               &region,
                               &gps_capability);
    if (rc != 0) {
      LOG(WARNING) << "Could not get confirmation firmware info: " << rc;
      return false;
    }

    if (modem_carrier_id != carrier->carrier_id) {
      LOG(WARNING) << "After upgrade, expected carrier: " << carrier->carrier_id
                   << ".  Instead got: " << modem_carrier_id;
      return false;
    }
  }

  LOG(INFO) << "Carrier image selection complete: " << carrier_name;
  return true;
}

bool GobiModem::GetSignalStrengthDbm(int *output) {
  if (!Enabled()) {
    // TODO(rochberg): ERROR
    LOG(WARNING) << "GetSignalQuality on disabled modem";
    return false;
  }
  ULONG rc;
  ULONG kSignals = 10;
  ULONG signals = kSignals;
  INT8 strengths[kSignals];
  ULONG interfaces[kSignals];

  rc = sdk_->GetSignalStrengths(&signals, strengths, interfaces);
  if (rc != 0) {
    // TODO(rochberg):  ERROR
    LOG(WARNING) << "GetSignalStrengths failed: " << rc;
    return false;
  }
  signals = std::min(kSignals, signals);

  for (ULONG i = 0; i < signals; ++i) {
    LOG(INFO) << "Interface " << i << ": " << static_cast<int>(strengths[i])
              << " dBM technology: " << interfaces[i];
  }
  if (!output) {
    LOG(WARNING) << "signal";
    return false;
  }
  // TODO(rochberg): Use GetDataBearerTechnology() to determine which
  // bearer we're using and return that signal.  In the mean time,
  // return the best of what's out there and assume the modem is using
  // that.
  *output = *(std::max_element(strengths, &strengths[signals]));

  return true;
}


void GobiModem::ActivationStatusCallback(ULONG activation_state) {
  LOG(INFO) << "Activation status callback: " << activation_state;
  pthread_mutex_lock(&activation_mutex_);
  activation_state_ = activation_state;
  pthread_cond_broadcast(&activation_cond_);
  pthread_mutex_unlock(&activation_mutex_);
}

void GobiModem::NmeaPlusCallback(const char *nmea, ULONG mode) {
  LOG(INFO) << "NMEA mode: " << mode << " " << nmea;
}
