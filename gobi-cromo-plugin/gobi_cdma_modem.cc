// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi_cdma_modem.h"
#include "gobi_modem_handler.h"
#include <cromo/carrier.h>
#include <mm/mm-modem.h>

extern "C" {
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
};

using utilities::DBusPropertyMap;


//======================================================================
// Construct and destruct

GobiCdmaModem::GobiCdmaModem(DBus::Connection& connection,
                             const DBus::Path& path,
                             const gobi::DeviceElement& device,
                             gobi::Sdk* sdk)
    : GobiModem(connection, path, device, sdk),
      activation_time_(METRIC_BASE_NAME "Activation", 0, 150000, 20) {
}

GobiCdmaModem::~GobiCdmaModem() {
}

void GobiCdmaModem::GetCdmaRegistrationState(ULONG* cdma_1x_state,
                                             ULONG* cdma_evdo_state,
                                             ULONG* roaming_state,
                                             DBus::Error& error) {
  ULONG reg_state;
  ULONG l1;
  WORD w1, w2;
  BYTE radio_interfaces[10];
  BYTE num_radio_interfaces = sizeof(radio_interfaces)/sizeof(BYTE);
  CHAR netname[32];

  ULONG rc = sdk_->GetServingNetwork(&reg_state, &l1, &num_radio_interfaces,
                                     radio_interfaces, roaming_state,
                                     &w1, &w2, sizeof(netname), netname);
  if (rc != 0) {
    // All errors are treated as if the modem is not yet registered.
    *cdma_1x_state = 0;
    *cdma_evdo_state = 0;
    *roaming_state = 0;
    return;
  }

  for (int i = 0; i < num_radio_interfaces; i++) {
    if (radio_interfaces[i] == gobi::kRfiCdma1xRtt)
      *cdma_1x_state = reg_state;
    else if (radio_interfaces[i] == gobi::kRfiCdmaEvdo)
      *cdma_evdo_state = reg_state;
  }
}

int GobiCdmaModem::GetMmActivationState() {
  ULONG device_activation_state;
  ULONG rc;
  rc = sdk_->GetActivationState(&device_activation_state);
  if (rc != 0) {
    LOG(ERROR) << "GetActivationState: " << rc;
    return -1;
  }
  LOG(INFO) << "device activation state: " << device_activation_state;
  if (device_activation_state == 1) {
    return MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED;
  }

  // Is the modem de-activated, or is there an activation in flight?
  switch (carrier_->activation_method()) {
    case Carrier::kOmadm: {
        ULONG session_state;
        ULONG session_type;
        ULONG failure_reason;
        BYTE retry_count;
        WORD session_pause;
        WORD time_remaining;  // For session pause
        rc = sdk_->OMADMGetSessionInfo(
            &session_state, &session_type, &failure_reason, &retry_count,
            &session_pause, & time_remaining);
        if (rc != 0) {
          // kNoTrackingSessionHasBeenStarted -> modem has never tried
          // to run OMADM; this is not an error condition.
          if (rc != gobi::kNoTrackingSessionHasBeenStarted) {
            LOG(ERROR) << "Could not get omadm state: " << rc;
          }
          return MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED;
        }
        return (session_state <= gobi::kOmadmMaxFinal) ?
            MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED :
            MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING;
      }
      break;
    case Carrier::kOtasp:
      return (device_activation_state == gobi::kNotActivated) ?
          MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED :
          MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING;
      break;
    default:  // This is a UMTS carrier; we count it as activated
      return MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED;
  }
}

//======================================================================
// Callbacks and callback utilities

static GobiCdmaModem* LookupCdmaModem(GobiModemHandler *handler,
                                       const DBus::Path &path) {
  return static_cast<GobiCdmaModem *>(handler->LookupByDbusPath(path));
}

static BYTE* GetFileContents(const char* filename, ULONG* num_bytes) {
  int bytes_read;
  int fd;
  struct stat st;

  fd = open(filename, O_RDONLY);
  if (fd == -1) {
    LOG(WARNING) << "Can't open '" << filename << "': " << strerror(errno);
    return NULL;
  }

  if (fstat(fd, &st) == -1) {
    LOG(WARNING) << "Can't fstat '" << filename << "': " << strerror(errno);
    close(fd);
    return NULL;
  }

  *num_bytes = st.st_size;
  BYTE* buffer = new BYTE[*num_bytes];
  bytes_read = read(fd, reinterpret_cast<char*>(buffer), *num_bytes);

  if (bytes_read < 0) {
    LOG(WARNING) << "Cannot read contents of PRL file \"" << filename
                 << "\": " << strerror(errno);
    delete [] buffer;
    close(fd);
    return NULL;
  }

  LOG(INFO) << "Read " << bytes_read << " bytes from file \""
            << filename << "\"";
  *num_bytes = bytes_read;
  close(fd);
  return buffer;
}

gboolean GobiCdmaModem::ActivationStatusCallback(gpointer data) {
  ActivationStatusArgs* args = static_cast<ActivationStatusArgs*>(data);
  LOG(INFO) << "OTASP status callback: " << args->device_activation_state;
  GobiCdmaModem* modem = LookupCdmaModem(handler_, *args->path);

  if (modem != NULL) {
    if (modem->activation_callback_id_) {
      g_source_remove(modem->activation_callback_id_);
      modem->activation_callback_id_ = 0;
    }
    if (args->device_activation_state == gobi::kActivated ||
        args->device_activation_state == gobi::kNotActivated) {
      modem->activation_time_.StopIfStarted();
    }
    if (args->device_activation_state == gobi::kActivated) {
      DBus::Error error;
      // Reset modem as per SDK documentation. This has the side-effect of
      // causing the modem to disappear from the DBus bus, which will cause the
      // connection manager to lose track of its state, but when we come back,
      // we'll be in the right state.

      // Do not send the ActivationStateChanged signal here as it will
      // only serve to encourage flimflam to start issuing new
      // commands and the modem is about to disappear anyway.
      modem->ResetModem(error);
    } else if (args->device_activation_state == gobi::kNotActivated) {
      modem->SendActivationStateChanged(
          MM_MODEM_CDMA_ACTIVATION_ERROR_PROVISIONING_FAILED);
    }
  }
  delete args;
  return FALSE;
}

gboolean GobiCdmaModem::ActivationTimeoutCallback(gpointer data) {
  CallbackArgs* args = static_cast<CallbackArgs*>(data);

  GobiCdmaModem* modem = LookupCdmaModem(handler_, *args->path);
  if (modem == NULL)
    return FALSE;

  LOG(ERROR) << "ActivationTimeout";
  modem->SendActivationStateChanged(MM_MODEM_CDMA_ACTIVATION_ERROR_TIMED_OUT);
  return FALSE;
}

void GobiCdmaModem::CleanupActivationTimeoutCallback(gpointer data) {
  CallbackArgs *args = static_cast<CallbackArgs*>(data);
  delete args;
}

static void OMADMAlertCallback(ULONG type, USHORT id) {
  LOG(INFO) << "OMDADMAlertCallback type " << type << " id " << id;
}

gboolean GobiCdmaModem::OmadmStateCallback(gpointer data) {
  OmadmStateArgs* args = static_cast<OmadmStateArgs*>(data);
  LOG(INFO) << "OMA-DM State Callback: " << args->session_state;
  GobiCdmaModem* modem = LookupCdmaModem(handler_, *args->path);
  bool activation_done = true;
  if (modem != NULL) {
    switch (args->session_state) {
      case gobi::kOmadmComplete:
        modem->SendActivationStateChanged(
            MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR);
        break;
      case gobi::kOmadmFailed:
        LOG(INFO) << "OMA-DM failure reason: " << args->failure_reason;
        // fall through
      case gobi::kOmadmUpdateInformationUnavailable:
        modem->SendActivationStateChanged(
            MM_MODEM_CDMA_ACTIVATION_ERROR_PROVISIONING_FAILED);
        break;
      default:
        activation_done = false;
    }
  }

  if (activation_done) {
    modem->activation_time_.Stop();
  }

  delete args;
  return FALSE;
}

void GobiCdmaModem::SignalStrengthHandler(INT8 signal_strength,
                                          ULONG radio_interface) {
  unsigned long ss_percent = MapDbmToPercent(signal_strength);

  ULONG cdma_evdo_state;
  ULONG cdma_1x_state;
  ULONG roaming_state;
  DBus::Error error;
  GetCdmaRegistrationState(&cdma_1x_state, &cdma_evdo_state,
                           &roaming_state, error);
  if ((radio_interface == gobi::kRfiCdma1xRtt &&
       cdma_1x_state == gobi::kRegistered) ||
      (radio_interface == gobi::kRfiCdmaEvdo &&
       cdma_evdo_state == gobi::kRegistered)) {
    SignalQuality(ss_percent);  // NB:  org.freedesktop...Modem.Cdma
  }
}


void GobiCdmaModem::RegisterCallbacks() {
  GobiModem::RegisterCallbacks();
  sdk_->SetOMADMAlertCallback(OMADMAlertCallback);
  sdk_->SetActivationStatusCallback(ActivationStatusCallbackTrampoline);
  sdk_->SetOMADMStateCallback(OmadmStateCallbackTrampoline);
}

//======================================================================
// DBUS Methods: overridden Modem.Simple
void GobiCdmaModem::GetTechnologySpecificStatus(DBusPropertyMap* properties) {
  WORD prl_version;
  ULONG rc = sdk_->GetPRLVersion(&prl_version);
  if (rc == 0) {
    (*properties)["prl_version"].writer().append_uint16(prl_version);
  }

  int activation_state = GetMmActivationState();
  if (activation_state >= 0) {
    (*properties)["activation_state"].writer().append_uint32(activation_state);
  }
}

//======================================================================
// DBUS Methods: ModemCDMA

// NB: This function only uses the DBus::Error field to return
// kOperationInitiatedError.  Other errors are returned as uint32_t
// values from MM_MODEM_CDMA_ACTIVATION_ERROR
uint32_t GobiCdmaModem::Activate(const std::string& carrier_name,
                             DBus::Error& activation_started_error) {
  // TODO(ellyjones): This is a guess based on empirical observations; someone
  // must know a real reasonable value for it. Find out what such a value is and
  // insert it here.
  static const int kActivationTimeoutSec = 5;
  LOG(INFO) << "Activate(" << carrier_name << ")";

  // Check current firmware to see whether it's for the requested carrier
  ULONG firmware_id;
  ULONG technology;
  ULONG carrier_id;
  ULONG region;
  ULONG gps_capability;

  ULONG rc = sdk_->GetFirmwareInfo(&firmware_id,
                                   &technology,
                                   &carrier_id,
                                   &region,
                                   &gps_capability);

  if (0 != rc) {
    LOG(ERROR) << "GetFirmwareInfo: " << rc;
    return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
  }
  const Carrier *carrier;
  if (carrier_name.empty()) {
    carrier = handler_->server().FindCarrierByCarrierId(carrier_id);
    if (carrier == NULL) {
      LOG(ERROR) << "Unknown carrier id: " << carrier_id;
      return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
    }
  } else {
    carrier = handler_->server().FindCarrierByName(carrier_name);
    if (carrier == NULL) {
      LOG(WARNING) << "Unknown carrier: " << carrier_name;
      return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
    }
    if (carrier_id != carrier->carrier_id()) {
      LOG(WARNING) << "Current device firmware does not match the requested"
          "carrier.";
      LOG(WARNING) << "SetCarrier operation must be done before activating.";
      return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
    }
  }

  DBus::Error internal_error;
  DBusPropertyMap status = GetStatus(internal_error);
  if (internal_error.is_set()) {
    LOG(ERROR) << internal_error;
    return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
  }

  DBusPropertyMap::const_iterator p;
  p = status.find("no_signal");
  if (p != status.end()) {
    LOG(ERROR) << "no_signal";
    return MM_MODEM_CDMA_ACTIVATION_ERROR_NO_SIGNAL;
  }

  p = status.find("activation_state");
  if (p != status.end()) {
    try {  // Style guide violation forced by dbus-c++
      LOG(INFO) << "Current activation state: "
                << p->second.reader().get_uint32();
    } catch (const DBus::Error &e) {
      LOG(ERROR) << e;
      return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
    }
  }

  activation_time_.Start();
  switch (carrier->activation_method()) {
    case Carrier::kOmadm:
      return ActivateOmadm();
      break;

    case Carrier::kOtasp: {
        uint32_t immediate_return;
        if (carrier->CdmaCarrierSpecificActivate(
                status, this, &immediate_return)) {
          return immediate_return;
        }
      }
      if (carrier->activation_code() == NULL) {
        LOG(ERROR) << "Number was not supplied for OTASP activation";
        return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
      }
      return ActivateOtasp(carrier->activation_code());
      break;

    default:
      LOG(ERROR) << "Unknown activation method: "
                   << carrier->activation_method();
      return MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN;
      break;
  }

  if (activation_callback_id_) {
    g_source_remove(activation_callback_id_);
    activation_callback_id_ = 0;
  }

  activation_callback_id_ = g_timeout_add_seconds_full(
      G_PRIORITY_DEFAULT,
      kActivationTimeoutSec,
      ActivationTimeoutCallback,
      new CallbackArgs(
          new DBus::Path(connected_modem_->path())),
      CleanupActivationTimeoutCallback);
  // We've successfully fired off an activation attempt, so we return
  // the "error" of saying so.
  activation_started_error.set(kOperationInitiatedError, "");
  // Return will be ignored; DBus Error sent insteaad
  return MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR;
}

void GobiCdmaModem::ActivateManual(const DBusPropertyMap& const_properties,
                               DBus::Error &error) {
  using utilities::ExtractString;
  DBusPropertyMap properties(const_properties);

  // TODO(rochberg): Does it make sense to set defaults from the
  // modem's current state?
  const char* spc = NULL;
  const char* prl_file = NULL;
  uint16_t system_id = 65535;
  const char* mdn = NULL;
  const char* min = NULL;
  const char* mnha = NULL;
  const char* mnaaa = NULL;

  DBusPropertyMap::const_iterator p;
  // try/catch required to cope with dbus-c++'s handling of type
  // mismatches
  try {  // Style guide violation forced by dbus-c++
    spc = ExtractString(properties, "spc", "000000", error);
    prl_file = ExtractString(properties, "prlfile", NULL, error);
    p = properties.find("system_id");
    if (p != properties.end()) {
      system_id = p->second.reader().get_uint16();
    }
    mdn = ExtractString(properties, "mdn", "",  error);
    min = ExtractString(properties, "min", "", error);
    mnha = ExtractString(properties, "mnha", NULL, error);
    mnaaa = ExtractString(properties, "mnhaaa", NULL, error);
  } catch (DBus::Error e) {
    error = e;
    return;
  }
  BYTE* prl = NULL;
  ULONG prl_size = 0;
  if (prl_file != NULL) {
    prl = GetFileContents(prl_file, &prl_size);
    if (prl == NULL) {
      error.set(kActivationError, "PRL file cannot be read");
      return;
    }
  }
  ULONG rc = sdk_->ActivateManual(spc,
                                  system_id,
                                  mdn,
                                  min,
                                  prl_size,
                                  prl,
                                  mnha,
                                  mnaaa);
  delete [] prl;
  ENSURE_SDK_SUCCESS(ActivateManual, rc, kActivationError);
}

void GobiCdmaModem::ActivateManualDebug(
    const std::map<std::string, std::string>& properties,
    DBus::Error &error) {
  DBusPropertyMap output;

  for (std::map<std::string, std::string>::const_iterator i =
           properties.begin();
       i != properties.end();
       ++i) {
    if (i->first != "system_id") {
      output[i->first].writer().append_string(i->second.c_str());
    } else {
      const std::string& value = i->second;
      char *end;
      errno = 0;
      uint16_t system_id = static_cast<uint16_t>(
          strtoul(value.c_str(), &end, 10));
      if (errno != 0 || *end != '\0') {
        LOG(ERROR) << "Bad system_id: " << value;
        error.set(kSdkError, "Bad system_id");
        return;
      }
      output[i->first].writer().append_uint16(system_id);
    }
  }
  ActivateManual(output, error);
}

uint32_t GobiCdmaModem::ActivateOmadm() {
  ULONG rc;
  LOG(INFO) << "Activating OMA-DM";

  rc = sdk_->OMADMStartSession(gobi::kConfigure);
  if (rc != 0) {
    LOG(ERROR) << "OMA-DM activation failed: " << rc;
    return MM_MODEM_CDMA_ACTIVATION_ERROR_START_FAILED;
  }
  return MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR;
}

uint32_t GobiCdmaModem::ActivateOtasp(const std::string& number) {
  ULONG rc;
  LOG(INFO) << "Activating OTASP";

  rc = sdk_->ActivateAutomatic(number.c_str());
  if (rc != 0) {
    LOG(ERROR) << "OTASP activation failed: " << rc;
    return MM_MODEM_CDMA_ACTIVATION_ERROR_START_FAILED;
   }
  return MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR;
}

std::string GobiCdmaModem::GetEsn(DBus::Error& error) {
  LOG(INFO) << "GetEsn";

  SerialNumbers serials;
  GetSerialNumbers(&serials, error);
  return serials.esn;
}

void GobiCdmaModem::GetRegistrationState(uint32_t& cdma_1x_state,
                                     uint32_t& cdma_evdo_state,
                                     DBus::Error& error) {
  ULONG reg_state_evdo;
  ULONG reg_state_1x;
  ULONG roaming_state;

  cdma_1x_state = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
  cdma_evdo_state = MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;

  GetCdmaRegistrationState(&reg_state_1x, &reg_state_evdo,
                           &roaming_state, error);
  if (error.is_set())
    return;

  uint32_t mm_reg_state;

  if (roaming_state == gobi::kHome)
    mm_reg_state = MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
  else
    mm_reg_state = MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING;

  if (reg_state_1x == gobi::kRegistered)
    cdma_1x_state = mm_reg_state;
  if (reg_state_evdo == gobi::kRegistered)
    cdma_evdo_state = mm_reg_state;
}

// returns <band class, band, system id>
DBus::Struct<uint32_t, std::string, uint32_t> GobiCdmaModem::GetServingSystem(
    DBus::Error& error) {
  DBus::Struct<uint32_t, std::string, uint32_t> result;
  WORD mcc, mnc, sid, nid;
  CHAR netname[32];
  ULONG reg_state;
  ULONG roaming_state;
  ULONG l1;
  BYTE radio_interfaces[10];
  BYTE num_radio_interfaces = sizeof(radio_interfaces)/sizeof(BYTE);
  LOG(INFO) << "GetServingSystem";

  ULONG rc = sdk_->GetServingNetwork(&reg_state, &l1, &num_radio_interfaces,
                                     radio_interfaces, &roaming_state,
                                     &mcc, &mnc, sizeof(netname), netname);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetServingNetwork, rc, kSdkError, result);
  LOG(INFO) << "Serving MCC/MNC: " << mcc << "/" << mnc;
  if (reg_state != gobi::kRegistered) {
    error.set(kErrorNoNetwork, "No network service is available");
    return result;
  }

  rc = sdk_->GetHomeNetwork(&mcc, &mnc,
                            sizeof(netname), netname, &sid, &nid);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetHomeNetwork, rc, kSdkError, result);

  LOG(INFO) << "Home MCC/MNC: " << mcc << "/" << mnc << " SID/NID: " << sid
            << "/" << nid << " name: " << netname;
  gobi::RfInfoInstance rf_info[10];
  BYTE rf_info_size = sizeof(rf_info) / sizeof(rf_info[0]);

  rc = sdk_->GetRFInfo(&rf_info_size, static_cast<BYTE*>((void *)&rf_info[0]));
  if (rc == gobi::kInformationElementUnavailable) {
    error.set(kErrorNoNetwork, "No network service is available");
    return result;
  } else if (rc != 0) {
    error.set(kSdkError, "GetRFInfo");
    return result;
  }

  if (rf_info_size != 0) {
    LOG(INFO) << "RF info for " << rf_info[0].radioInterface
              << " band class " << rf_info[0].activeBandClass
              << " channel "    << rf_info[0].activeChannel;
    switch (rf_info[0].activeBandClass) {
      case 0:
      case 85:          // WCDMA 800
        result._1 = 1;  // 800 Mhz band class
        break;
      case 1:
      case 81:          // WCDMA PCS 1900
        result._1 = 2;  // 1900 Mhz band class
        break;
      default:
        result._1 = 0;  // unknown band class
        break;
    }
    result._2 = "F";    // XXX bogus
  }
  result._3 = sid;

  return result;
}

uint32_t GobiCdmaModem::GetSignalQuality(DBus::Error& error) {
  return GobiModem::CommonGetSignalQuality(error);
}

void GobiCdmaModem::RegistrationStateHandler() {
  uint32_t cdma_1x_state;
  uint32_t evdo_state;
  DBus::Error error;

  GetRegistrationState(cdma_1x_state, evdo_state, error);
  if (error.is_set())
    return;
  if (cdma_1x_state != MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN ||
      evdo_state != MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN) {
    registration_time_.Stop();
  }
  RegistrationStateChanged(cdma_1x_state, evdo_state);

  // TODO(ers) check data bearer technology and notify if appropriate.

  LOG(INFO) << "  => 1xRTT: " << cdma_1x_state << " EVDO: " << evdo_state;
}

void GobiCdmaModem::DataCapabilitiesHandler(BYTE num_data_caps,
                                            ULONG* data_caps) {
  // TODO(ers) explore whether we should be doing anything
  // with this event.
}

void GobiCdmaModem::SetTechnologySpecificProperties() {
  SerialNumbers serials;
  DBus::Error error;
  GetSerialNumbers(&serials, error);
  if (!error.is_set())
    Meid = serials.meid;
}

bool GobiCdmaModem::CheckEnableOk(DBus::Error &error) {
  return true;
}

void GobiCdmaModem::SendActivationStateFailed() {
  DBusPropertyMap empty;
  ActivationStateChanged(MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED,
                         MM_MODEM_CDMA_ACTIVATION_ERROR_UNKNOWN,
                         empty);
}

void GobiCdmaModem::SendActivationStateChanged(uint32_t mm_activation_error) {
  DBusPropertyMap status;
  DBusPropertyMap to_send;
  DBus::Error internal_error;
  status = GetStatus(internal_error);
  if (internal_error.is_set()) {
    // GetStatus should never fail;  we are punting here.
    SendActivationStateFailed();
    return;
  }

  DBusPropertyMap::const_iterator p;
  uint32_t mm_activation_state;

  if ((p = status.find("activation_state")) == status.end()) {
    LOG(ERROR);
    SendActivationStateFailed();
    return;
  }
  try {  // Style guide violation forced by dbus-c++
    mm_activation_state = p->second.reader().get_uint32();
  } catch (const DBus::Error &e) {
    LOG(ERROR);
    SendActivationStateFailed();
  }

  if (mm_activation_error == MM_MODEM_CDMA_ACTIVATION_ERROR_TIMED_OUT &&
      mm_activation_state == MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED) {
    mm_activation_error = MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR;
  }

  // TODO(rochberg):  Table drive
  if ((p = status.find("mdn")) != status.end()) {
    to_send["mdn"] = p->second;
  }
  if ((p = status.find("min")) != status.end()) {
    to_send["min"] = p->second;
  }
  if ((p = status.find("payment_url")) != status.end()) {
    to_send["payment_url"] = p->second;
  }

  ActivationStateChanged(mm_activation_state,
                         mm_activation_error,
                         to_send);
}
