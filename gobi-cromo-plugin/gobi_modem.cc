// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi_modem.h"

#include <algorithm>

#include <base/time.h>
#include <glog/logging.h>
#include <mm/mm-modem.h>

static const size_t DEFAULT_SIZE=128;

GobiModem* GobiModem::connected_modem_(NULL);

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
  LogGobiInformation();
}

// DBUS Methods: Modem
void GobiModem::Enable(const bool& enable) {
  if (enable && !Enabled()) {
    if (!ApiConnect()) {
      // TODO(rochberg): ERROR
      CHECK(0);
    }

    ULONG firmware_id;
    ULONG technology;
    ULONG carrier;
    ULONG region;
    ULONG gps_capability;
    ULONG rc;
    rc = sdk_->GetFirmwareInfo(&firmware_id,
                               &technology,
                               &carrier,
                               &region,
                               &gps_capability);
    if (rc != 0) {
      LOG(WARNING) << "GetFirmwareInfo: " << rc;
      // TODO(rochberg):  ERROR
      return;
    }

    // TODO(rochberg):  Make this more general
    if (carrier != 1  && // Generic
        carrier < 101) {  // Defined carriers start at 101
      SetCarrier("Sprint");
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
    LOG(INFO) << "Failure Reason " <<  failure_reason;
  }
  LOG(INFO) << "Session ID " <<  session_id;
}


void GobiModem::Disconnect() {
  LOG(INFO) << "Disconnect";
}

DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> GobiModem::GetIP4Config() {
  DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> result;

  LOG(INFO) << "GetIP4Config";

  return result;
}

void GobiModem::FactoryReset(const std::string& code) {
  ULONG rc;
  LOG(INFO) << "ResetToFactoryDefaults";
  rc = sdk_->ResetToFactoryDefaults(const_cast<CHAR *>(code.c_str()));
  if (rc != 0) {
    // TODO(rochberg): ERROR
    LOG(WARNING) << "Factory reset failed: " << rc;
    return;
  }
  // TODO(rochberg): check ResetModem and ERROR
  ResetModem();

  return;
}

DBus::Struct<std::string, std::string, std::string> GobiModem::GetInfo() {
  // (manufacturer, modem, version).
  DBus::Struct<std::string, std::string, std::string> result;

  char buffer[DEFAULT_SIZE];
  ULONG rc;
  rc = sdk_->GetManufacturer(sizeof(buffer), buffer);
  if (rc != 0) {
    // TODO(rochberg): ERROR
    LOG(WARNING) << "GetManufacturer: " << rc;
    return result;
  }

  result._1 = buffer;

  rc = sdk_->GetModelID(sizeof(buffer), buffer);
  if (rc != 0) {
    // TODO(rochberg): ERROR
    LOG(WARNING) << "GetModelID: " << rc;
    return result;
  }
  result._2 = buffer;

  rc = sdk_->GetFirmwareRevision(sizeof(buffer), buffer);
  if (rc != 0) {
    // TODO(rochberg): ERROR
    LOG(WARNING) << "GetFirmwareRevision: " << rc;
    return result;
  }
  result._3 = buffer;

  LOG(INFO) << "Manufacturer: " << result._1;
  LOG(INFO) << "Modem: " << result._2;
  LOG(INFO) << "Version: " << result._3;
  return result;
}

// DBUS Methods: ModemSimple
void GobiModem::Connect(const DBusPropertyMap& properties) {
  LOG(INFO) << "Simple.Connect";
}

bool GobiModem::GetSerialNumbers(SerialNumbers *out) {
  char esn[DEFAULT_SIZE], imei[DEFAULT_SIZE], meid[DEFAULT_SIZE];
  ULONG rc = sdk_-> GetSerialNumbers(DEFAULT_SIZE, esn,
                                     DEFAULT_SIZE, imei,
                                     DEFAULT_SIZE, meid);
  if (rc != 0) {
    LOG(WARNING) << "GSN: " << rc;
    return false;
  }
  out->esn = esn;
  out->imei = imei;
  out->meid = meid;
  return true;
}

DBusPropertyMap GobiModem::GetStatus() {
  DBusPropertyMap result;

  int32_t rssi;

  // TODO(rochberg):  More mandatory properties expected
  if (GetSignalStrengthDbm(&rssi)) {
    result["signal_strength_dbm"].writer().append_int32(rssi);
  }

  SerialNumbers serials;

  if (this->GetSerialNumbers(&serials)) {
    if (serials.esn.length()) {
      result["esn"].writer().append_string(serials.esn.c_str());
    }
    if (serials.meid.length()) {
      result["meid"].writer().append_string(serials.meid.c_str());
    }
    if (serials.imei.length()) {
      result["imei"].writer().append_string(serials.imei.c_str());
    }
  }

  return result;
}

  // DBUS Methods: ModemCDMA

std::string GobiModem::GetEsn() {
  LOG(INFO) << "GetEsn";

  SerialNumbers serials;
  if (!GetSerialNumbers(&serials)) {
    // TODO(rochberg): ERROR
    CHECK(0);
  }
  return serials.esn;
}


uint32_t GobiModem::GetSignalQuality() {
  if (!Enabled()) {
    // TODO(rochberg): ERROR
    LOG(WARNING) << "GetSignalQuality on disabled modem";
    return 0;
  }
  // TODO(rochberg): Map dBM to quality
  LOG(WARNING) << "Returning bogus signal quality";
  return 61;
}


DBus::Struct<uint32_t, std::string, uint32_t> GobiModem::GetServingSystem() {
  DBus::Struct<uint32_t, std::string, uint32_t> result;
  LOG(INFO) << "GetServingSystem";

  return result;
}

void GobiModem::GetRegistrationState(uint32_t& cdma_1x_state,
                                     uint32_t& evdo_state) {
  LOG(WARNING) << "Returning bogus values for GetRegistrationState";
  cdma_1x_state = 1;
  evdo_state = 1;
}

bool GobiModem::ApiConnect() {
  ULONG rc;
  rc = sdk_->QCWWANConnect(device_.deviceNode, device_.deviceKey);
  if (rc != 0) {
    return false;
  }
  connected_modem_ = this;

  SetActivationStatusCallback(ActivationStatusTrampoline);
  SetNMEAPlusCallback(NmeaPlusCallbackTrampoline);
  SetOMADMStateCallback(OmadmStateCallbackTrampoline);

  return true;
}

void GobiModem::LogGobiInformation() {
  ULONG rc;

  char buffer[DEFAULT_SIZE];
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

  char amss[DEFAULT_SIZE], boot[DEFAULT_SIZE], pri[DEFAULT_SIZE];
  rc = sdk_->GetFirmwareRevisions(DEFAULT_SIZE, amss,
                                  DEFAULT_SIZE, boot,
                                  DEFAULT_SIZE, pri);
  if (rc == 0) {
    LOG(INFO) << "Firmware Revisions: AMSS: " << amss
              << " boot: " << boot
              << " pri: " << pri;
  } else {
    LOG(WARNING) << "GFR: " << rc;
  }

  rc = GetImageStore(sizeof(buffer), buffer);
  if (rc == 0) {
    LOG(INFO) << "ImageStore: " << buffer;
  } else {
    LOG(WARNING) << "GIS: " << rc;
  }

  SerialNumbers serials;
  CHECK(GetSerialNumbers(&serials));
  LOG(INFO) << "ESN: " << serials.esn;
  LOG(INFO) << "IMEI: " << serials.imei;
  LOG(INFO) << "MEID: " << serials.meid;

  char number[DEFAULT_SIZE], min_array[DEFAULT_SIZE];
  rc = GetVoiceNumber(DEFAULT_SIZE, number, DEFAULT_SIZE, min_array);
  if (rc == 0) {
    LOG(INFO) << "Voice: " << number
              << " MIN: " << min_array;
  } else if (rc != gobi::kNotProvisioned) {
    LOG(INFO) << "GetVoiceNumber failed: " << rc;
  }

  BYTE index;
  rc = GetActiveMobileIPProfile(&index);
  if (rc != 0 && rc != gobi::kNotSupportedByDevice) {
    LOG(WARNING) << "GetAMIPP: " << rc;
  } else {
    LOG(INFO) << "Mobile IP profile: " << (int)index;
  }
}

void GobiModem::SoftReset() {
  if (!ResetModem()) {
    // TODO(rochberg):  ERROR
  }
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
    return false;
  }

  const int kPollIntervalMs = 500;
  base::Time::Time deadline = base::Time::NowFromSystemTime() +
      base::TimeDelta::FromSeconds(60);

  bool connected = false;
  // Wait for modem to become unavailable, then available again.
  while (base::Time::NowFromSystemTime() < deadline) {
    sdk_->QCWWANDisconnect();
    if (!ApiConnect()) {
      break;
    }
    usleep(kPollIntervalMs * 1000);
  }

  LOG(INFO) << "Modem reset:  Modem now unavailable";

  while (base::Time::NowFromSystemTime() < deadline) {
    sdk_->QCWWANDisconnect();
    if ((connected = ApiConnect())) {
      break;
    };
    usleep(kPollIntervalMs * 1000);
  }

  if (!connected) {
    // TODO(rochberg):  Send DeviceRemoved
    LOG(WARNING) << "Modem did not come back after reset";
    return false;
  } else {
    LOG(INFO) << "Modem returned from reset";
  }
  Enabled = true;
  return true;
}


bool GobiModem::ActivateOmadm() {
  ULONG rc;
  LOG(INFO) << "Activating OMA-DM";

  activation_state_ = -1;

  rc = sdk_->OMADMStartSession(gobi::kConfigure);
  if (rc != 0) {
    LOG(WARNING) << "OMADMStartSession failed: " << rc;
    return false;
  }

  // TODO(rochberg):  Factor out this pattern + add timeouts
  pthread_mutex_lock(&activation_mutex_);
  while (activation_state_ != gobi::kActivated) {
    LOG(WARNING) << "Waiting...";
    pthread_cond_wait(&activation_cond_, &activation_mutex_);
    if (activation_state_ <= gobi::kOmadmMaxFinal) {
      break;
    }
  }
  pthread_mutex_unlock(&activation_mutex_);
  return true;
}

bool GobiModem::ActivateOtasp() {
  ULONG rc;

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

  return ResetModem();
}

// TODO(rochberg):  Refactor this and build a Carrier class
bool GobiModem::EnsureActivated() {
  ULONG rc;

  rc = sdk_->GetActivationState(&activation_state_);
  LOG(INFO) << "Activation state: " << activation_state_;

  if (activation_state_ == gobi::kActivated) {
    return true;
  }

  if (activation_state_ != gobi::kNotActivated) {
    LOG(WARNING) << "Unexpected activation state: " << activation_state_;
    return false;
  }

  // TODO(rochberg): Switch based on carrer
  return ActivateOmadm();
}


// TODO(rochberg):  Make this into a class, add dBM -> % maps,
// activation routines
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
  LogGobiInformation();
  return true;
}

void GobiModem::SetCarrier(const std::string& carrier) {
  if (!EnsureFirmwareLoaded(carrier.c_str())) {
    // TODO(rochberg): ERROR
  }
}

void GobiModem::on_get_property(DBus::InterfaceAdaptor& interface,
                                const std::string& property,
                                DBus::Variant& vale) {
  LOG(INFO) << "GetProperty called for " << property;
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

void GobiModem::OmadmStateCallback(ULONG sessionState, ULONG failureReason) {
  LOG(INFO) << "OMA-DM State Callback: "
            << sessionState << " " << failureReason;
  pthread_mutex_lock(&activation_mutex_);
  activation_state_ = sessionState;
  pthread_cond_broadcast(&activation_cond_);
  pthread_mutex_unlock(&activation_mutex_);
}
