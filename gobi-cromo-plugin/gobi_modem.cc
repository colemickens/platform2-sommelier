// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi_modem.h"
#include "gobi_modem_handler.h"
#include <sstream>

#include <dbus/dbus.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>

extern "C" {
#include <libudev.h>
#include <time.h>
};

#include <base/stringprintf.h>
#include <cromo/carrier.h>
#include <cromo/cromo_server.h>
#include <cromo/utilities.h>

#include <glog/logging.h>
#include <mm/mm-modem.h>

// This ought to be in a header file somewhere, but if it is, I can't find it.
#ifndef NDEBUG
static const int DEBUG = 1;
#else
static const int DEBUG = 0;
#endif

#define DEFINE_ERROR(name) \
  const char* k##name##Error = "org.chromium.ModemManager.Error." #name;
#define DEFINE_MM_ERROR(name) \
  const char* k##name##Error = \
    "org.freedesktop.ModemManager.Modem.Gsm." #name;

#include "gobi_modem_errors.h"
#undef DEFINE_ERROR
#undef DEFINE_MM_ERROR

static const char *k2kNetworkDriver = "QCUSBNet2k";
static const char *k3kNetworkDriver = "GobiNet";
static const char *kUnifiedNetworkDriver = "gobi";

using utilities::DBusPropertyMap;

// The following constants define the granularity with which signal
// strength is reported, i.e., the number of bars.
//
// The values given here are used to compute an array of thresholds
// consisting of the values [-113, -98, -83, -68, -53], which results
// in the following mapping of signal strength as reported in dBm to
// bars displayed:
//
// <  -113             0 bars
// >= -113 and < -98   1 bar
// >=  -98 and < -83   2 bars
// >=  -83 and < -68   3 bars
// >=  -68 and < -53   4 bars
// >=  -53             5 bars

// Any reported signal strength larger than or equal to kMaxSignalStrengthDbm
// is considered full strength.
static const int kMaxSignalStrengthDbm = -53;
// Any reported signal strength smaller than kMinSignalStrengthDbm is
// considered zero strength.
static const int kMinSignalStrengthDbm = -113;
// The number of signal strength levels we want to report, ranging from
// 0 bars to kSignalStrengthNumLevels-1 bars.
static const int kSignalStrengthNumLevels = 6;

// Returns the current time, in milliseconds, from an unspecified epoch.
unsigned long long GobiModem::GetTimeMs(void) {
  struct timespec ts;
  unsigned long long rv;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  rv = ts.tv_sec;
  rv *= 1000;
  rv += ts.tv_nsec / 1000000;
  return rv;
}

unsigned long GobiModem::MapDbmToPercent(INT8 signal_strength_dbm) {
  unsigned long signal_strength_percent;

  if (signal_strength_dbm < kMinSignalStrengthDbm)
    signal_strength_percent = 0;
  else if (signal_strength_dbm >= kMaxSignalStrengthDbm)
    signal_strength_percent = 100;
  else
    signal_strength_percent =
        ((signal_strength_dbm - kMinSignalStrengthDbm) * 100 /
         (kMaxSignalStrengthDbm - kMinSignalStrengthDbm));
  return signal_strength_percent;
}

ULONG GobiModem::MapDataBearerToRfi(ULONG data_bearer_technology) {
  switch (data_bearer_technology) {
    case gobi::kDataBearerCdma1xRtt:
      return gobi::kRfiCdma1xRtt;
    case gobi::kDataBearerCdmaEvdo:
    case gobi::kDataBearerCdmaEvdoRevA:
      return gobi::kRfiCdmaEvdo;
    case gobi::kDataBearerGprs:
      return gobi::kRfiGsm;
    case gobi::kDataBearerWcdma:
    case gobi::kDataBearerEdge:
    case gobi::kDataBearerHsdpaDlWcdmaUl:
    case gobi::kDataBearerWcdmaDlUsupaUl:
    case gobi::kDataBearerHsdpaDlHsupaUl:
      return gobi::kRfiUmts;
    default:
      return gobi::kRfiCdmaEvdo;
  }
}

static struct udev_enumerate *enumerate_net_devices(struct udev *udev)
{
  int rc;
  struct udev_enumerate *udev_enumerate = udev_enumerate_new(udev);

  if (udev_enumerate == NULL) {
    return NULL;
  }

  rc = udev_enumerate_add_match_subsystem(udev_enumerate, "net");
  if (rc != 0) {
    udev_enumerate_unref(udev_enumerate);
    return NULL;
  }

  rc = udev_enumerate_scan_devices(udev_enumerate);
  if (rc != 0) {
    udev_enumerate_unref(udev_enumerate);
    return NULL;
  }
  return udev_enumerate;
}

GobiModem* GobiModem::connected_modem_(NULL);
GobiModemHandler* GobiModem::handler_;
GobiModem::mutex_wrapper_ GobiModem::modem_mutex_;

bool StartExitTrampoline(void *arg) {
  GobiModem *modem = static_cast<GobiModem*>(arg);
  return modem->StartExit();
}

bool ExitOkTrampoline(void *arg) {
  GobiModem *modem = static_cast<GobiModem*>(arg);
  return modem->is_disconnected();
}

bool SuspendOkTrampoline(void *arg) {
  GobiModem *modem = static_cast<GobiModem*>(arg);
  return modem->is_disconnected();
}

GobiModem::GobiModem(DBus::Connection& connection,
                     const DBus::Path& path,
                     const gobi::DeviceElement& device,
                     gobi::Sdk* sdk)
    : DBus::ObjectAdaptor(connection, path),
      sdk_(sdk),
      last_seen_(-1),
      session_state_(gobi::kDisconnected),
      session_id_(0),
      signal_available_(false),
      suspending_(false),
      exiting_(false),
      connect_start_time_(0),
      disconnect_start_time_(0),
      registration_start_time_(0) {
  memcpy(&device_, &device, sizeof(device_));
}

void GobiModem::Init() {
  sdk_->set_current_modem_path(path());
  metrics_lib_.reset(new MetricsLibrary());
  metrics_lib_->Init();

  // Initialize DBus object properties
  Enabled = false;
  IpMethod = MM_MODEM_IP_METHOD_DHCP;
  UnlockRequired = "";
  UnlockRetries = 999;
  SetDeviceProperties();
  SetModemProperties();

  carrier_ = handler_->server().FindCarrierNoOp();

  for (int i = 0; i < GOBI_EVENT_MAX; i++) {
    event_enabled[i] = false;
  }

  char name[64];
  void *thisp = static_cast<void*>(this);
  snprintf(name, sizeof(name), "gobi-%p", thisp);
  hooks_name_ = name;

  handler_->server().start_exit_hooks().Add(hooks_name_, StartExitTrampoline,
                                            this);
  handler_->server().exit_ok_hooks().Add(hooks_name_, ExitOkTrampoline, this);
  RegisterStartSuspend(hooks_name_);
  handler_->server().suspend_ok_hooks().Add(hooks_name_, SuspendOkTrampoline,
                                            this);
}

GobiModem::~GobiModem() {
  handler_->server().start_exit_hooks().Del(hooks_name_);
  handler_->server().exit_ok_hooks().Del(hooks_name_);
  handler_->server().UnregisterStartSuspend(hooks_name_);
  handler_->server().suspend_ok_hooks().Del(hooks_name_);

  ApiDisconnect();
}

enum {
  METRIC_INDEX_NONE = 0,
  METRIC_INDEX_QMI_HARDWARE_RESTRICTED,
  METRIC_INDEX_MAX
};

static unsigned int QCErrorToMetricIndex(unsigned int error) {
  if (error == gobi::kQMIHardwareRestricted)
    return METRIC_INDEX_QMI_HARDWARE_RESTRICTED;
  return METRIC_INDEX_NONE;
}

// DBUS Methods: Modem
void GobiModem::Enable(const bool& enable, DBus::Error& error) {
  LOG(INFO) << "Enable: " << Enabled() << " => " << enable;
  if (enable && !Enabled()) {
    ApiConnect(error);
    if (error.is_set())
      return;
    LogGobiInformation();
    ULONG firmware_id;
    ULONG technology;
    ULONG carrier_id;
    ULONG region;
    ULONG gps_capability;

    ULONG rc = sdk_->SetPower(gobi::kOnline);
    metrics_lib_->SendEnumToUMA(METRIC_BASE_NAME "SetPower",
                                QCErrorToMetricIndex(rc),
                                METRIC_INDEX_MAX);
    if (rc != 0) {
      error.set(kSdkError, "SetPower");
      LOG(WARNING) << "SetPower failed : " << rc;
      ApiDisconnect();
      return;
    }
    rc = sdk_->GetFirmwareInfo(&firmware_id,
                               &technology,
                               &carrier_id,
                               &region,
                               &gps_capability);
    if (rc != 0) {
      error.set(kSdkError, "GetFirmwareInfo");
      LOG(WARNING) << "GetFirmwareInfo failed : " << rc;
      ApiDisconnect();
      return;
    }

    const Carrier *carrier =
        handler_->server().FindCarrierByCarrierId(carrier_id);

    if (carrier != NULL) {
      LOG(INFO) << "Current carrier is " << carrier->name()
                << " (" << carrier->carrier_id() << ")";
      carrier_ = carrier;
    } else {
      LOG(WARNING) << "Unknown carrier, id=" << carrier_id;
    }
    Enabled = true;
    registration_start_time_ = GetTimeMs();
  } else if (!enable && Enabled()) {
    ULONG rc = sdk_->SetPower(gobi::kPersistentLowPower);
    if (rc != 0) {
      error.set(kSdkError, "SetPower");
      LOG(WARNING) << "SetPower failed : " << rc;
      return;
    }
    ApiDisconnect();
    Enabled = false;
  }
}

void GobiModem::Connect(const std::string& unused_number, DBus::Error& error) {
  DBusPropertyMap properties;
  Connect(properties, error);
}

void GobiModem::Disconnect(DBus::Error& error) {
  LOG(INFO) << "Disconnect(" << session_id_ << ")";
  if (session_state_ != gobi::kConnected) {
    LOG(WARNING) << "Disconnect called when not connected";
    error.set(kDisconnectError, "Not connected");
    return;
  }

  disconnect_start_time_ = GetTimeMs();
  ULONG rc = sdk_->StopDataSession(session_id_);
  if (rc != 0)
    disconnect_start_time_ = 0;
  ENSURE_SDK_SUCCESS(StopDataSession, rc, kDisconnectError);
  error.set(kOperationInitiatedError, "");
  // session_state_ will be set in SessionStateCallback
}

std::string GobiModem::GetUSBAddress() {
  return sysfs_path_.substr(sysfs_path_.find_last_of('/') + 1);
}

DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> GobiModem::GetIP4Config(
    DBus::Error& error) {
  DBus::Struct<uint32_t, uint32_t, uint32_t, uint32_t> result;

  LOG(INFO) << "GetIP4Config (unimplemented)";

  return result;
}

void GobiModem::FactoryReset(const std::string& code, DBus::Error& error) {
  LOG(INFO) << "ResetToFactoryDefaults";
  ULONG rc = sdk_->ResetToFactoryDefaults(const_cast<CHAR *>(code.c_str()));
  ENSURE_SDK_SUCCESS(ResetToFactoryDefaults, rc, kSdkError);
  LOG(INFO) << "FactoryReset succeeded. Device should disappear and reappear.";
}

DBus::Struct<std::string, std::string, std::string> GobiModem::GetInfo(
    DBus::Error& error) {
  // (manufacturer, modem, version).
  DBus::Struct<std::string, std::string, std::string> result;

  char buffer[kDefaultBufferSize];

  ULONG rc = sdk_->GetManufacturer(sizeof(buffer), buffer);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetManufacturer, rc, kSdkError, result);
  result._1 = buffer;
  rc = sdk_->GetModelID(sizeof(buffer), buffer);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetModelID, rc, kSdkError, result);
  result._2 = buffer;
  rc = sdk_->GetHardwareRevision(sizeof(buffer), buffer);
  ENSURE_SDK_SUCCESS_WITH_RESULT(GetHardwareRevision, rc, kSdkError, result);
  result._3 = buffer;

  LOG(INFO) << "Manufacturer: " << result._1;
  LOG(INFO) << "Modem: " << result._2;
  LOG(INFO) << "Version: " << result._3;
  return result;
}

//======================================================================
// DBUS Methods: Modem.Simple

void GobiModem::Connect(const DBusPropertyMap& properties, DBus::Error& error) {
  if (!Enabled()) {
    LOG(WARNING) << "Connect on disabled modem";
    error.set(kConnectError, "Modem is disabled");
    return;
  }
  if (exiting_) {
    LOG(WARNING) << "Connect when exiting";
    error.set(kConnectError, "Cromo is exiting");
    return;
  }
  const char* apn = utilities::ExtractString(properties, "apn", NULL, error);
  const char* username =
      utilities::ExtractString(properties, "username", NULL, error);
  const char* password =
      utilities::ExtractString(properties, "password", NULL, error);
  ULONG failure_reason;
  ULONG rc;

  connect_start_time_ = GetTimeMs();
  if (apn !=  NULL)
    LOG(INFO) << "Starting data session for APN " << apn;
  rc = sdk_->StartDataSession(NULL,  // technology
                              apn,
                              NULL,  // Authentication
                              username,
                              password,
                              &session_id_,  // OUT: session ID
                              &failure_reason  // OUT: failure reason
                              );
  if (rc == 0) {
    LOG(INFO) << "Session ID " <<  session_id_;
    return;
  }

  LOG(WARNING) << "StartDataSession failed: " << rc;
  const char* msg;
  if (rc == gobi::kCallFailed) {
    LOG(WARNING) << "Failure Reason " <<  failure_reason;
    msg = QMICallFailureToMMError(failure_reason);
  } else {
    msg = "StartDataSession";
  }
  error.set(kConnectError, msg);
}

void GobiModem::GetSerialNumbers(SerialNumbers *out, DBus::Error &error) {
  char esn[kDefaultBufferSize];
  char imei[kDefaultBufferSize];
  char meid[kDefaultBufferSize];

  ULONG rc = sdk_->GetSerialNumbers(kDefaultBufferSize, esn,
                              kDefaultBufferSize, imei,
                              kDefaultBufferSize, meid);
  ENSURE_SDK_SUCCESS(GetSerialNumbers, rc, kSdkError);
  out->esn = esn;
  out->imei = imei;
  out->meid = meid;
}

unsigned int GobiModem::QCStateToMMState(ULONG qcstate) {
  unsigned int mmstate;

  // TODO(ers) if not registered, should return enabled state
  switch (qcstate) {
    case gobi::kConnected:
      mmstate = MM_MODEM_STATE_CONNECTED;
      break;
    case gobi::kAuthenticating:
      mmstate = MM_MODEM_STATE_CONNECTING;
      break;
    case gobi::kDisconnected:
    default:
      ULONG rc;
      ULONG reg_state;
      ULONG l1, l2;      // don't care
      WORD w1, w2;
      BYTE b3[10];
      BYTE b2 = sizeof(b3)/sizeof(BYTE);
      char buf[255];
      rc = sdk_->GetServingNetwork(&reg_state, &l1, &b2, b3, &l2, &w1, &w2,
                                   sizeof(buf), buf);
      if (rc == 0) {
        if (reg_state == gobi::kRegistered) {
          mmstate = MM_MODEM_STATE_REGISTERED;
          break;
        } else if (reg_state == gobi::kSearching) {
          mmstate = MM_MODEM_STATE_SEARCHING;
          break;
        }
      }
      mmstate = MM_MODEM_STATE_UNKNOWN;
  }

  return mmstate;
}

// if we get SDK errors while trying to retrieve information,
// we ignore them, and just don't set the corresponding properties
DBusPropertyMap GobiModem::GetStatus(DBus::Error& error_ignored) {
  DBusPropertyMap result;

  int32_t rssi;
  DBus::Error signal_strength_error;

  StrengthMap interface_to_dbm;
  GetSignalStrengthDbm(rssi, &interface_to_dbm, signal_strength_error);
  if (signal_strength_error.is_set() || rssi <= kMinSignalStrengthDbm) {
    // Activate() looks for this key and does not even try if present
    result["no_signal"].writer().append_bool(true);
  } else {
    result["signal_strength_dbm"].writer().append_int32(rssi);
    for (StrengthMap::iterator i = interface_to_dbm.begin();
         i != interface_to_dbm.end();
         ++i) {
      char buf[30];
      snprintf(buf,
               sizeof(buf),
               "interface_%u_dbm",
               static_cast<unsigned int>(i->first));
      result[buf].writer().append_int32(i->second);
    }
  }

  SerialNumbers serials;
  DBus::Error serial_numbers_error;
  this->GetSerialNumbers(&serials, serial_numbers_error);
  if (!serial_numbers_error.is_set()) {
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
  char imsi[kDefaultBufferSize];
  ULONG rc = sdk_->GetIMSI(kDefaultBufferSize, imsi);
  if (rc == 0 && strlen(imsi) != 0)
    result["imsi"].writer().append_string(imsi);

  ULONG firmware_id;
  ULONG technology_id;
  ULONG carrier_id;
  ULONG region;
  ULONG gps_capability;
  rc = sdk_->GetFirmwareInfo(&firmware_id,
                             &technology_id,
                             &carrier_id,
                             &region,
                             &gps_capability);
  if (rc == 0) {
    if (carrier_id != carrier_->carrier_id()) {
      LOG(ERROR) << "Inconsistent carrier: id = " << carrier_id;
      std::ostringstream s;
      s << "inconsistent: " << carrier_id << " vs. " << carrier_->carrier_id();
      result["carrier"].writer().append_string(s.str().c_str());
    } else {
      result["carrier"].writer().append_string(carrier_->name());
    }

    // TODO(ers) we'd like to return "operator_name", but the
    // SDK provides no apparent means of determining it.

    const char* technology;
    if (technology_id == 0)
      technology = "CDMA";
    else if (technology_id == 1)
      technology = "UMTS";
    else
      technology = "unknown";
    result["technology"].writer().append_string(technology);
  }


  ULONG session_state;
  rc = sdk_->GetSessionState(&session_state);
  if (rc == 0) {
    result["state"].writer().append_uint32(QCStateToMMState(session_state));
  }

  char mdn[kDefaultBufferSize], min[kDefaultBufferSize];
  rc = sdk_->GetVoiceNumber(kDefaultBufferSize, mdn,
                            kDefaultBufferSize, min);
  if (rc == 0) {
    result["mdn"].writer().append_string(mdn);
    result["min"].writer().append_string(min);
  } else if (rc != gobi::kNotProvisioned) {
    LOG(WARNING) << "GetVoiceNumber failed: " << rc;
  }

  char firmware_revision[kDefaultBufferSize];
  rc = sdk_->GetFirmwareRevision(sizeof(firmware_revision), firmware_revision);
  if (rc == 0 && strlen(firmware_revision) != 0) {
    result["firmware_revision"].writer().append_string(firmware_revision);
  }

  GetTechnologySpecificStatus(&result);
  carrier_-> ModifyModemStatusReturn(&result);

  return result;
}

// This is only in debug builds; if you add actual code here, see
// RegisterCallbacks().
static void ByteTotalsCallback(ULONGLONG tx, ULONGLONG rx) {
  LOG(INFO) << "ByteTotalsCallback: tx " << tx << " rx " << rx;
}

// This is only in debug builds; if you add actual code here, see
// RegisterCallbacks().
gboolean GobiModem::DormancyStatusCallback(gpointer data) {
  DormancyStatusArgs *args = reinterpret_cast<DormancyStatusArgs*>(data);
  GobiModem *modem = handler_->LookupByDbusPath(*args->path);
  LOG(INFO) << "DormancyStatusCallback: " << args->status;
  if (modem->event_enabled[GOBI_EVENT_DORMANCY]) {
    modem->DormancyStatus(args->status == gobi::kDormant);
  }
  return FALSE;
}

static void MobileIPStatusCallback(ULONG status) {
  LOG(INFO) << "MobileIPStatusCallback: " << status;
}

static void PowerCallback(ULONG mode) {
  LOG(INFO) << "PowerCallback: " << mode;
}

static void LURejectCallback(ULONG domain, ULONG cause) {
  LOG(INFO) << "LURejectCallback: domain " << domain << " cause " << cause;
}

static void PDSStateCallback(ULONG enabled, ULONG tracking) {
  LOG(INFO) << "PDSStateCallback: enabled " << enabled
            << " tracking " << tracking;
}

void GobiModem::RegisterCallbacks() {
  sdk_->SetMobileIPStatusCallback(MobileIPStatusCallback);
  sdk_->SetPowerCallback(PowerCallback);
  sdk_->SetRFInfoCallback(RFInfoCallbackTrampoline);
  sdk_->SetLURejectCallback(LURejectCallback);
  sdk_->SetPDSStateCallback(PDSStateCallback);

  sdk_->SetSessionStateCallback(SessionStateCallbackTrampoline);
  sdk_->SetDataBearerCallback(DataBearerCallbackTrampoline);
  sdk_->SetRoamingIndicatorCallback(RoamingIndicatorCallbackTrampoline);
  sdk_->SetDataCapabilitiesCallback(DataCapabilitiesCallbackTrampoline);

  if (DEBUG) {
    // These are only used for logging. If you make one of these a non-debug
    // callback, see EventKeyToIndex() below, which will need to be updated.
    sdk_->SetByteTotalsCallback(ByteTotalsCallback, 60);
    sdk_->SetDormancyStatusCallback(DormancyStatusCallbackTrampoline);
  }

  static int num_thresholds = kSignalStrengthNumLevels - 1;
  int interval =
      (kMaxSignalStrengthDbm - kMinSignalStrengthDbm) /
      (kSignalStrengthNumLevels - 1);
  INT8 thresholds[num_thresholds];
  for (int i = 0; i < num_thresholds; i++) {
    thresholds[i] = kMinSignalStrengthDbm + interval * i;
  }
  sdk_->SetSignalStrengthCallback(SignalStrengthCallbackTrampoline,
                                  num_thresholds,
                                  thresholds);
}

bool GobiModem::CanMakeMethodCalls(void) {
  // The Gobi has been observed (at least once - see chromium-os:7172) to get
  // into a state where we can set up a QMI channel to it (so QCWWANConnect()
  // succeeds) but not actually invoke any functions. We'll force the issue here
  // by calling GetSerialNumbers so we can detect this failure mode early.
  ULONG rc;
  char esn[kDefaultBufferSize];
  char imei[kDefaultBufferSize];
  char meid[kDefaultBufferSize];
  rc = sdk_->GetSerialNumbers(sizeof(esn), esn, sizeof(imei), imei,
                              sizeof(meid), meid);
  if (rc != 0) {
    LOG(ERROR) << "GetSerialNumbers() failed: " << rc;
  }

  return rc == 0;
}

void GobiModem::ApiConnect(DBus::Error& error) {
  // It is safe to test for NULL outside of a lock because ApiConnect
  // is only called by the main thread, and only the main thread can
  // modify connected_modem_.
  if (connected_modem_ != NULL) {
    LOG(INFO) << "ApiAlready connected: connected_modem_=0x" << connected_modem_
              << "this=0x" << this;
    error.set(kOperationNotAllowedError,
              "Only one modem can be connected via Api");
    return;
  }

  pthread_mutex_lock(&modem_mutex_.mutex_);
  connected_modem_ = this;
  pthread_mutex_unlock(&modem_mutex_.mutex_);

  ULONG rc = sdk_->QCWWANConnect(device_.deviceNode, device_.deviceKey);
  if (rc != 0 || !CanMakeMethodCalls()) {
    LOG(ERROR) << "QCWWANConnect() failed.  Exiting so modem can reset." << rc;
    ExitAndResetDevice(rc);
  }
  RegisterCallbacks();
}


ULONG GobiModem::ApiDisconnect(void) {
  ULONG rc;
  pthread_mutex_lock(&modem_mutex_.mutex_);
  if (connected_modem_ == this) {
    LOG(INFO) << "Disconnecting from QCWWAN.  this_=0x" << this;
    connected_modem_ = NULL;
    pthread_mutex_unlock(&modem_mutex_.mutex_);
    rc = sdk_->QCWWANDisconnect();
  } else {
    LOG(INFO) << "Not connected.  this_=0x" << this;
    pthread_mutex_unlock(&modem_mutex_.mutex_);
  }
  return rc;
}

void GobiModem::LogGobiInformation() {
  ULONG rc;

  char buffer[kDefaultBufferSize];
  rc = sdk_->GetManufacturer(sizeof(buffer), buffer);
  if (rc == 0) {
    LOG(INFO) << "Manufacturer: " << buffer;
  }

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
  if (rc == 0) {
    LOG(INFO) << "Firmware info: "
              << "firmware_id: " << firmware_id
              << " technology: " << technology
              << " carrier: " << carrier
              << " region: " << region
              << " gps_capability: " << gps_capability;
  } else {
    LOG(WARNING) << "Cannot get firmware info: " << rc;
  }

  char amss[kDefaultBufferSize], boot[kDefaultBufferSize];
  char pri[kDefaultBufferSize];

  rc = sdk_->GetFirmwareRevisions(kDefaultBufferSize, amss,
                                  kDefaultBufferSize, boot,
                                  kDefaultBufferSize, pri);
  if (rc == 0) {
    LOG(INFO) << "Firmware Revisions: AMSS: " << amss
              << " boot: " << boot
              << " pri: " << pri;
  } else {
    LOG(WARNING) << "Cannot get firmware revision info: " << rc;
  }

  SerialNumbers serials;
  DBus::Error error;
  GetSerialNumbers(&serials, error);
  if (!error.is_set()) {
    DLOG(INFO) << "ESN " << serials.esn
               << " IMEI " << serials.imei
               << " MEID " << serials.meid;
  } else {
    LOG(WARNING) << "Cannot get serial numbers: " << error;
  }

  char number[kDefaultBufferSize], min_array[kDefaultBufferSize];
  rc = sdk_->GetVoiceNumber(kDefaultBufferSize, number,
                            kDefaultBufferSize, min_array);
  if (rc == 0) {
    char masked_min[kDefaultBufferSize + 1];
    strncpy(masked_min, min_array, sizeof(masked_min));
    if (strlen(masked_min) >= 4)
      strcpy(masked_min + strlen(masked_min) - 4, "XXXX");
    char masked_voice[kDefaultBufferSize + 1];
    strncpy(masked_voice, number, sizeof(masked_voice));
    if (strlen(masked_voice) >= 4)
      strcpy(masked_voice + strlen(masked_voice) - 4, "XXXX");
    LOG(INFO) << "Voice: " << masked_voice
              << " MIN: " << masked_min;
  } else if (rc != gobi::kNotProvisioned) {
    LOG(WARNING) << "GetVoiceNumber failed: " << rc;
  }

  BYTE index;
  rc = sdk_->GetActiveMobileIPProfile(&index);
  if (rc != 0 && rc != gobi::kNotSupportedByDevice) {
    LOG(WARNING) << "GetAMIPP: " << rc;
  } else {
    LOG(INFO) << "Mobile IP profile: " << (int)index;
  }
}

void GobiModem::SoftReset(DBus::Error& error) {
  ResetModem(error);
}

void GobiModem::PowerCycle(DBus::Error &error) {
  LOG(INFO) << "Initiating modem powercycle";
  ULONG rc = sdk_->SetPower(gobi::kPowerOff);
  ENSURE_SDK_SUCCESS(SetPower, rc, kSdkError);
}

void GobiModem::ResetModem(DBus::Error& error) {
  Enabled = false;
  LOG(INFO) << "Offline";

  // Resetting the modem will cause it to disappear and reappear.
  ULONG rc = sdk_->SetPower(gobi::kOffline);
  ENSURE_SDK_SUCCESS(SetPower, rc, kSdkError);

  LOG(INFO) << "Reset";
  rc = sdk_->SetPower(gobi::kReset);
  ENSURE_SDK_SUCCESS(SetPower, rc, kSdkError);

  rc = ApiDisconnect();
  ENSURE_SDK_SUCCESS(QCWWanDisconnect, rc, kSdkError);

  // WARNING: This modem became invalid when we called SetPower(gobi::kReset)
  // above; any further attempts to make method calls might result in us trying
  // to open a /dev/qcqmi which doesn't exist yet (the modem being in the middle
  // of a reset operation).
  //
  // Note that after the unregister_obj(), methods on us can't be called any
  // more; it would be rude to have signals originating from this object after
  // Remove() returns, since Remove() declares us to no longer exist.
  unregister_obj();
  handler_->Remove(this);
}

void GobiModem::SetCarrier(const std::string& carrier_name,
                           DBus::Error& error) {
  const Carrier *carrier = handler_->server().FindCarrierByName(carrier_name);
  if (carrier == NULL) {
    // TODO(rochberg):  Do we need to sanitize this string?
    LOG(WARNING) << "Could not parse carrier: " << carrier_name;
    error.set(kFirmwareLoadError, "Unknown carrier name");
    return;
  }

  LOG(INFO) << "Carrier image selection starting: " << carrier_name;
  ULONG firmware_id;
  ULONG technology;
  ULONG modem_carrier_id;
  ULONG region;
  ULONG gps_capability;
  ULONG rc = sdk_->GetFirmwareInfo(&firmware_id,
                                   &technology,
                                   &modem_carrier_id,
                                   &region,
                                   &gps_capability);
  ENSURE_SDK_SUCCESS(GetFirmwareInfo, rc, kFirmwareLoadError);

  if (modem_carrier_id != carrier->carrier_id()) {

    // UpgradeFirmware doesn't pay attention to anything before the
    // last /, so we don't supply it
    std::string image_path = std::string("/") + carrier->firmware_directory();

    LOG(INFO) << "Current Gobi carrier: " << modem_carrier_id
              << ".  Carrier image selection "
              << image_path;
    rc = sdk_->UpgradeFirmware(const_cast<CHAR *>(image_path.c_str()));
    if (rc != 0) {
      LOG(WARNING) << "Carrier image selection from: "
                   << image_path << " failed: " << rc;
      error.set(kFirmwareLoadError, "UpgradeFirmware");
    } else {
      ApiDisconnect();
    }
  }
}

uint32_t GobiModem::CommonGetSignalQuality(DBus::Error& error) {
  if (!Enabled()) {
    LOG(WARNING) << "GetSignalQuality on disabled modem";
    error.set(kModeError, "Modem is disabled");
  } else {
    int32_t signal_strength_dbm;
    GetSignalStrengthDbm(signal_strength_dbm, NULL, error);
    if (!error.is_set()) {
      uint32_t result = MapDbmToPercent(signal_strength_dbm);
      LOG(INFO) << "GetSignalQuality => " << result << "%";
      return result;
    }
  }
  // for the error cases, return an impossible value
  return 999;
}

void GobiModem::GetSignalStrengthDbm(int& output,
                                     StrengthMap *interface_to_dbm,
                                     DBus::Error& error) {
  ULONG kSignals = 10;
  ULONG signals = kSignals;
  INT8 strengths[kSignals];
  ULONG interfaces[kSignals];

  signal_available_ = false;
  ULONG rc = sdk_->GetSignalStrengths(&signals, strengths, interfaces);
  if (rc == gobi::kNoAvailableSignal) {
    output = kMinSignalStrengthDbm-1;
    return;
  }
  ENSURE_SDK_SUCCESS(GetSignalStrengths, rc, kSdkError);
  signal_available_ = true;

  signals = std::min(kSignals, signals);

  if (interface_to_dbm) {
    for (ULONG i = 0; i < signals; ++i) {
      (*interface_to_dbm)[interfaces[i]] = strengths[i];
    }
  }

  INT8 max_strength = kint8min;
  for (ULONG i = 0; i < signals; ++i) {
    DLOG(INFO) << "Interface " << i << ": " << static_cast<int>(strengths[i])
               << " dBM technology: " << interfaces[i];
    // TODO(ers) mark radio interface technology as registered?
    if (strengths[i] > max_strength) {
      max_strength = strengths[i];
    }
  }

  // If we're in the connected state, pick the signal strength for the radio
  // interface that's being used. Otherwise, pick the strongest signal.
  ULONG session_state;
  rc = sdk_->GetSessionState(&session_state);
  ENSURE_SDK_SUCCESS(GetSessionState, rc, kSdkError);

  if (session_state == gobi::kConnected) {
    ULONG db_technology;
    rc =  sdk_->GetDataBearerTechnology(&db_technology);
    if (rc != 0) {
      LOG(WARNING) << "GetDataBearerTechnology failed: " << rc;
      error.set(kSdkError, "GetDataBearerTechnology");
      return;
    }
    DLOG(INFO) << "data bearer technology " << db_technology;
    ULONG rfi_technology = MapDataBearerToRfi(db_technology);
    for (ULONG i = 0; i < signals; ++i) {
      if (interfaces[i] == rfi_technology) {
        output = strengths[i];
        return;
      }
    }
  }
  output = max_strength;
}

// Set properties for which a connection to the SDK is required
// to obtain the needed information. Since this is called before
// the modem is enabled, we connect to the SDK, get the properties
// we need, and then disconnect from the SDK.
// pre-condition: Enabled == false
void GobiModem::SetModemProperties() {
  DBus::Error connect_error;

  ApiConnect(connect_error);
  if (connect_error.is_set()) {
    Type = MM_MODEM_TYPE_CDMA;
    return;
  }

  ULONG u1, u2, u3, u4;
  BYTE radioInterfaces[10];
  ULONG numRadioInterfaces = sizeof(radioInterfaces)/sizeof(BYTE);
  ULONG rc = sdk_->GetDeviceCapabilities(&u1, &u2, &u3, &u4,
                                         &numRadioInterfaces,
                                         radioInterfaces);
  if (rc == 0) {
    if (numRadioInterfaces != 0) {
      if (radioInterfaces[0] == gobi::kRfiGsm ||
          radioInterfaces[0] == gobi::kRfiUmts) {
        Type = MM_MODEM_TYPE_GSM;
      } else {
        Type = MM_MODEM_TYPE_CDMA;
      }
    }
  }
  SetTechnologySpecificProperties();
  ApiDisconnect();
}

// DBUS message handler.
void GobiModem::SetAutomaticTracking(const bool& service_enable,
                                     const bool& port_enable,
                                     DBus::Error& error) {
  ULONG rc;
  rc = sdk_->SetServiceAutomaticTracking(service_enable);
  ENSURE_SDK_SUCCESS(SetServiceAutomaticTracking, rc, kSdkError);
  LOG(INFO) << "Service automatic tracking " << (service_enable ?
          "enabled" : "disabled");

  rc = sdk_->SetPortAutomaticTracking(port_enable);
  ENSURE_SDK_SUCCESS(SetPortAutomaticTracking, rc, kSdkError);
  LOG(INFO) << "Port automatic tracking " << (port_enable ?
          "enabled" : "disabled");
}

void GobiModem::InjectFault(const std::string& name,
                            const int32_t &value,
                            DBus::Error& error) {
  if (name == "SdkError") {
    sdk_->InjectFaultSdkError(value);
  } else {
    error.set(kInvalidArgumentError, "Unknown fault requested.");
  }
}


void GobiModem::SinkSdkError(const std::string& modem_path,
                             const std::string& sdk_function,
                             ULONG error) {
  LOG(ERROR) << sdk_function << ": unrecoverable error " << error
             << " on modem " << modem_path;
  PostCallbackRequest(GobiModem::SdkErrorHandler, new SdkErrorArgs(error));
}

// Callbacks:  Run in the context of the main thread
gboolean GobiModem::SdkErrorHandler(gpointer data) {
  SdkErrorArgs *args = static_cast<SdkErrorArgs *>(data);
  GobiModem* modem = handler_->LookupByDbusPath(*args->path);
  if (modem != NULL) {
    modem->ExitAndResetDevice(args->error);
  } else {
    LOG(INFO) << "Reset received for obsolete path "
              << args->path;
  }
  return FALSE;
}

gboolean GobiModem::SignalStrengthCallback(gpointer data) {
  SignalStrengthArgs* args = static_cast<SignalStrengthArgs*>(data);
  GobiModem* modem = handler_->LookupByDbusPath(*args->path);
  if (modem != NULL)
    modem->SignalStrengthHandler(args->signal_strength, args->radio_interface);
  delete args;
  return FALSE;
}

gboolean GobiModem::SessionStateCallback(gpointer data) {
  SessionStateArgs* args = static_cast<SessionStateArgs*>(data);
  GobiModem* modem = handler_->LookupByDbusPath(*args->path);
  if (modem != NULL)
    modem->SessionStateHandler(args->state, args->session_end_reason);
  delete args;
  return FALSE;
}

gboolean GobiModem::RegistrationStateCallback(gpointer data) {
  CallbackArgs* args = static_cast<CallbackArgs*>(data);
  GobiModem* modem = handler_->LookupByDbusPath(*args->path);
  delete args;
  if (modem != NULL)
    modem->RegistrationStateHandler();
  return FALSE;
}

gboolean GobiModem::DataCapabilitiesCallback(gpointer data) {
  DataCapabilitiesArgs* args = static_cast<DataCapabilitiesArgs*>(data);
  GobiModem* modem = handler_->LookupByDbusPath(*args->path);
  if (modem != NULL)
    modem->DataCapabilitiesHandler(args->num_data_caps, args->data_caps);
  delete args;
  return FALSE;
}

gboolean GobiModem::DataBearerTechnologyCallback(gpointer data) {
  DataBearerTechnologyArgs* args = static_cast<DataBearerTechnologyArgs*>(data);
  GobiModem* modem = handler_->LookupByDbusPath(*args->path);
  if (modem != NULL)
    modem->DataBearerTechnologyHandler(args->technology);
  delete args;
  return FALSE;
}

void GobiModem::SessionStateHandler(ULONG state, ULONG session_end_reason) {
  LOG(INFO) << "SessionStateHandler state: " << state
            << " reason: "
            << (state == gobi::kConnected?0:session_end_reason);
  if (state == gobi::kConnected) {
    ULONG data_bearer_technology;
    sdk_->GetDataBearerTechnology(&data_bearer_technology);
    // TODO(ers) send a signal or change a property to notify
    // listeners about the change in data bearer technology
  }

  session_state_ = state;
  if (state == gobi::kDisconnected) {
    if (disconnect_start_time_) {
      ULONG duration = GetTimeMs() - disconnect_start_time_;
      metrics_lib_->SendToUMA(METRIC_BASE_NAME "Disconnect",
                              duration, 0, 150000, 20);
      disconnect_start_time_ = 0;
    }
    session_id_ = 0;
    unsigned int reason = QMIReasonToMMReason(session_end_reason);
    if (reason == MM_MODEM_STATE_CHANGED_REASON_USER_REQUESTED && suspending_)
      reason = MM_MODEM_STATE_CHANGED_REASON_SUSPEND;
    // TODO(ellyjones): stop guessing about MM_MODEM_STATE_REGISTERED here.
    StateChanged(MM_MODEM_STATE_CONNECTED, MM_MODEM_STATE_REGISTERED, reason);
  } else if (state == gobi::kConnected) {
    ULONG duration = GetTimeMs() - connect_start_time_;
    metrics_lib_->SendToUMA(METRIC_BASE_NAME "Connect",
                            duration, 0, 150000, 20);
    suspending_ = false;
    // TODO(ellyjones): stop guessing about MM_MODEM_STATE_REGISTERED here.
    StateChanged(MM_MODEM_STATE_REGISTERED, MM_MODEM_STATE_CONNECTED, 0);
  }
}

void GobiModem::DataBearerTechnologyHandler(ULONG technology) {
  // Default is to ignore the argument and treat this as a
  // registration state change. This behavior can be overridden.
  RegistrationStateHandler();
}

// Set DBus properties that pertain to the modem hardware device.
// The properties set here are Device, MasterDevice, and Driver.
void GobiModem::SetDeviceProperties()
{
  struct udev *udev = udev_new();

  if (udev == NULL) {
    LOG(WARNING) << "udev == NULL";
    return;
  }

  struct udev_list_entry *entry;
  struct udev_enumerate *udev_enumerate = enumerate_net_devices(udev);
  if (udev_enumerate == NULL) {
    LOG(WARNING) << "udev_enumerate == NULL";
    udev_unref(udev);
    return;
  }

  for (entry = udev_enumerate_get_list_entry(udev_enumerate);
      entry != NULL;
      entry = udev_list_entry_get_next(entry)) {

    std::string syspath(udev_list_entry_get_name(entry));

    struct udev_device *udev_device =
        udev_device_new_from_syspath(udev, syspath.c_str());
    if (udev_device == NULL)
      continue;

    std::string driver;
    struct udev_device *parent = udev_device_get_parent(udev_device);
    if (parent != NULL)
      driver = udev_device_get_driver(parent);

    if (driver.compare(k2kNetworkDriver) == 0 ||
        driver.compare(k3kNetworkDriver) == 0 ||
        driver.compare(kUnifiedNetworkDriver) == 0) {
      // Extract last portion of syspath...
      size_t found = syspath.find_last_of('/');
      if (found != std::string::npos) {
        Device = syspath.substr(found + 1);
        struct udev_device *grandparent;
        if (parent != NULL) {
          grandparent = udev_device_get_parent(parent);
          if (grandparent != NULL) {
            sysfs_path_ = udev_device_get_syspath(grandparent);
            LOG(INFO) << "sysfs path: " << sysfs_path_;
            MasterDevice = sysfs_path_;
          }
        }
        Driver = driver;
        udev_device_unref(udev_device);

        // TODO(jglasgow): Support multiple devices.
        // This functions returns the first network device whose
        // driver is a qualcomm network device driver.  This will not
        // work properly if a machine has multiple devices that use the
        // Qualcomm network device driver.
        break;
      }
    }
    udev_device_unref(udev_device);
  }
  udev_enumerate_unref(udev_enumerate);
  udev_unref(udev);
}

bool GobiModem::StartExit() {
  exiting_ = true;
  if (session_id_)
    sdk_->StopDataSession(session_id_);

  LOG(INFO) << "StartExit: session id " << session_id_;
  return true;
}

const char* GobiModem::QMIReturnCodeToMMError(unsigned int qmicode) {
  switch (qmicode) {
    case gobi::kIncorrectPinId:
      return MM_ERROR_MODEM_GSM_INCORRECTPASSWORD;
    case gobi::kInvalidPinId:
    case gobi::kAccessToRequiredEntityNotAvailable:
      return MM_ERROR_MODEM_GSM_SIMPINREQUIRED;
    case gobi::kPinBlocked:
    case gobi::kPinPermanentlyBlocked:
      // blocked vs. permanently block is distinguished by
      // looking at the value of UnlockRetries. If it is
      // zero, then the SIM is permanently blocked.
      return MM_ERROR_MODEM_GSM_SIMPUKREQUIRED;
    default:
      return NULL;
  }
}

// Map call failure reasons into ModemManager errors
const char* GobiModem::QMICallFailureToMMError(unsigned int qmireason) {
  switch (qmireason) {
    case gobi::kReasonBadApn:
    case gobi::kReasonNotSubscribed:
      return MM_ERROR_MODEM_GSM_GPRSNOTSUBSCRIBED;
    default:
      return MM_ERROR_MODEM_GSM_UNKNOWN;
  }
}

unsigned int GobiModem::QMIReasonToMMReason(unsigned int qmireason) {
  switch (qmireason) {
    case gobi::kReasonClientEndedCall:
      return MM_MODEM_STATE_CHANGED_REASON_USER_REQUESTED;
    default:
      return MM_MODEM_STATE_CHANGED_REASON_UNKNOWN;
  }
}

bool GobiModem::StartSuspend() {
  LOG(INFO) << "StartSuspend";
  suspending_ = true;
  if (session_id_)
    sdk_->StopDataSession(session_id_);
  return true;
}

bool StartSuspendTrampoline(void *arg) {
  GobiModem *modem = static_cast<GobiModem*>(arg);
  return modem->StartSuspend();
}

void GobiModem::RegisterStartSuspend(const std::string &name) {
  // TODO(ellyjones): Get maxdelay_ms from the carrier plugin
  static const int maxdelay_ms = 10000;
  handler_->server().RegisterStartSuspend(name, StartSuspendTrampoline, this,
                                          maxdelay_ms);
}

// Tokenizes a string of the form (<[+-]ident>)* into a list of strings of the
// form [+-]ident.
static std::vector<std::string> TokenizeRequest(const std::string& req) {
  std::vector<std::string> tokens;
  std::string token;
  size_t start, end;

  start = req.find_first_of("+-");
  while (start != req.npos) {
    end = req.find_first_of("+-", start + 1);
    if (end == req.npos)
      token = req.substr(start);
    else
      token = req.substr(start, end - start);
    tokens.push_back(token);
    start = end;
  }

  return tokens;
}

int GobiModem::EventKeyToIndex(const char *key) {
  if (DEBUG && !strcmp(key, "dormancy"))
    return GOBI_EVENT_DORMANCY;
  return -1;
}

void GobiModem::RequestEvent(const std::string request, DBus::Error& error) {
  const char *req = request.c_str();
  const char *key = req + 1;

  if (!strcmp(key, "*")) {
    for (int i = 0; i < GOBI_EVENT_MAX; i++) {
      event_enabled[i] = (req[0] == '+');
    }
    return;
  }

  int idx = EventKeyToIndex(key);
  if (idx < 0) {
    error.set(kInvalidArgumentError, "Unknown event requested.");
    return;
  }

  event_enabled[idx] = (req[0] == '+');
}

void GobiModem::RequestEvents(const std::string& events, DBus::Error& error) {
  std::vector<std::string> requests = TokenizeRequest(events);
  std::vector<std::string>::iterator it;
  for (it = requests.begin(); it != requests.end(); it++) {
    RequestEvent(*it, error);
  }
}

void GobiModem::RecordResetReason(ULONG reason) {
  static const ULONG distinguished_errors[] = {
    gobi::kErrorSendingQmiRequest,    // 0
    gobi::kErrorReceivingQmiRequest,  // 1
    gobi::kErrorNeedsReset,           // 2
  };
  // Leave some room for other errors
  const int kMaxError = 10;
  int bucket = kMaxError;
  for (size_t i = 0; i < arraysize(distinguished_errors); ++i) {
    if (reason == distinguished_errors[i]) {
      bucket = i;
    }
  }
  metrics_lib_->SendEnumToUMA(
      METRIC_BASE_NAME "ResetReason", bucket, kMaxError + 1);
}

void GobiModem::ExitAndResetDevice(ULONG reason) {
  ApiDisconnect();
  if (suspending_) {
    LOG(ERROR) << "Already suspending " << static_cast<std::string>(path());
    return;
  }
  if (reason)
    RecordResetReason(reason);

  handler_->ExitLeavingModemsForCleanup();
}

// DBus-exported
void GobiModem::Reset(DBus::Error& error) {
  // NB: If we have multiple modems, this is going to disconnect those
  // other modems.  If this becomes a problem, previous versions of
  // the code forked off a suid-root process to kick the device off
  // the bus; that code is still in the git repo.

  ExitAndResetDevice(0);
}
