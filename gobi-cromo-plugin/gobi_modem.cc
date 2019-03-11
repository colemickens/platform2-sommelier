// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi-cromo-plugin/gobi_modem.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <vector>

#include <dbus/dbus.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>

extern "C" {
#include <libudev.h>
#include <time.h>
}

#include <base/logging.h>
#include <base/macros.h>
#include <base/strings/stringprintf.h>
#include <cromo/carrier.h>
#include <cromo/cromo_server.h>
#include <cromo/utilities.h>

#include "gobi-cromo-plugin/gobi_modem_handler.h"

// This ought to be in a header file somewhere, but if it is, I can't find it.
#ifndef NDEBUG
static const int DEBUG = 1;
#else
static const int DEBUG = 0;
#endif

#define DEFINE_ERROR(name) \
    const char* k##name##Error = "org.chromium.ModemManager.Error." #name;
#define DEFINE_MM_ERROR(name, str) \
    const char* kError##name = "org.freedesktop.ModemManager.Modem.Gsm." str;

#include "gobi-cromo-plugin/gobi_modem_errors.h"
#undef DEFINE_ERROR
#undef DEFINE_MM_ERROR

static const char k2kNetworkDriver[] = "QCUSBNet2k";
static const char k3kNetworkDriver[] = "GobiNet";
static const char kUnifiedNetworkDriver[] = "gobi";

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

// static
// Number of times to retry StartDataSession() when it fails due to lack of
// service.
const int GobiModem::kNumStartDataSessionRetries = 10;
// Error message returned by SetCarrier() when an unknown carrier is specified.
const char GobiModemHelper::kErrorUnknownCarrier[] = "Unknown carrier name";

// Encapsulates a separate thread that calls sdk->StartDataSession.
// We need to run this on a separate thread so that we can continue to
// process DBus messages (including requests to cancel the call to
// StartDataSession).  The main thread creates an anonynmous
// SessionStarter, then deletes it when the session starter passes a
// pointer to itself as an argument to SessionStarterDoneCallback.
class SessionStarter {
 public:
  SessionStarter(gobi::Sdk *sdk,
                 GobiModem *modem,
                 const char *apn,
                 const char *username,
                 const char *password)
      : sdk_(sdk),
        modem_dbus_path_(modem->path()),
        apn_(apn),
        username_(username),
        password_(password),
        return_value_(ULONG_MAX),
        failure_reason_(ULONG_MAX),
        connect_time_(METRIC_BASE_NAME "Connect", 0, 150000, 20),
        fault_inject_sleep_time_ms_(static_cast <useconds_t> (
            modem->injected_faults_["AsyncConnectSleepMs"])) {
    // Do not save modem; it might be deleted from under us
  }

  static void *StarterThreadTrampoline(void *data) {
    SessionStarter *s = static_cast<SessionStarter *>(data);
    return s->Run();
  }

  void *Run(void) {
    connect_time_.Start();
    // A warning: there is a race here, if the modem handler is
    // deallocated while this is in flight, we'll lose sdk_.  If this
    // is a problem, we'll need to refcount sdk_.
    LOG(INFO) << "Starter thread running";
    for (int count = 0; count < GobiModem::kNumStartDataSessionRetries;
         ++count) {
      return_value_ = sdk_->StartDataSession(
          nullptr,
          apn_.get(),
          nullptr,  // Authentication
          username_.get(),
          password_.get(),
          &session_id_,  // OUT: session ID
          &failure_reason_);  // OUT: failure reason
      if (return_value_ != gobi::kCallFailed &&
          failure_reason_ != gobi::kReasonNoService)
        break;
      gobi::RegistrationState reg_state =
        GobiModem::GetRegistrationState(sdk_);
      if (reg_state == gobi::kRegistrationDenied)
        break;
      sleep(1);
    }
    if (return_value_ == gobi::kOperationHasNoEffect) {
      return_value_ = 0;
      failure_reason_ = 0;
    }
    if (return_value_)
      LOG(ERROR) << "StartDataSession failed with "
                 << "(" << return_value_ << ", " << failure_reason_ << ")";
    connect_time_.Stop();
    if (fault_inject_sleep_time_ms_ > 0) {
      LOG(WARNING) << "Fault injection:  connect sleeping for "
                   << fault_inject_sleep_time_ms_ << "ms";
      usleep(1000 * fault_inject_sleep_time_ms_);
    }
    LOG(INFO) << "Starter completing";
    g_idle_add(CompletionCallback, this);
    return nullptr;
  }

  static gboolean CompletionCallback(void *data) {
    // Runs on main thread;
    std::unique_ptr<SessionStarter> s(static_cast<SessionStarter *>(data));

    int join_rc = pthread_join(s->thread_, nullptr);
    if (join_rc != 0) {
      errno = join_rc;
      PLOG(ERROR) << "Join failed";
    }

    // GobiModem::handler_ is a static
    GobiModem *modem = GobiModem::handler_->LookupByDbusPath(
        s->modem_dbus_path_);
    if (!modem) {
      // The DBUs call is never completed; the object was unregistered
      // from the bus.
      LOG(ERROR) << "SessionStarter complete with no modem";
      return false;
    }
    modem->SessionStarterDoneCallback(s.get());
    return false;
  }

  void StartDataSession(DBus::Error& error) {
    // Runs on main thread
    int rc = pthread_create(
        &thread_, nullptr /* attributes */, StarterThreadTrampoline, this);
    if (rc != 0) {
      errno = rc;
      PLOG(ERROR) << "Thread creation failed: " << rc;
      error.set(kConnectError, "Could not start thread");
    }
  }

  static ULONG CancelDataSession(gobi::Sdk *sdk) {
    // Runs on main thread
    LOG(WARNING) << "Cancelling in-flight data session start";

    // Horrendous hack alert; If we issue CancelStartDataSession
    // before the StartDataSession request has made much progress,
    // then the StartDataSession call ends up hanging for the entire 5
    // minute timeout.  Empirically, 100ms seems to be a long enough
    // pause, so we go for 3x that.  This solution is totally
    // unacceptable, but debugging the Gobi API isn't an option right
    // now.
    usleep(300000);

    ULONG rc;
    rc = sdk->CancelStartDataSession();
    if (rc != 0) {
      LOG(ERROR) << "CancelStartDataSession failed: " << rc;
    }
    return rc;
  }

 protected:
  gobi::Sdk *sdk_;
  DBus::Path modem_dbus_path_;

  // CharStarCopier preserves NULL-ness
  gobi::CharStarCopier apn_;
  gobi::CharStarCopier username_;
  gobi::CharStarCopier password_;

  ULONG session_id_;
  ULONG return_value_;
  ULONG failure_reason_;

  MetricsStopwatch connect_time_;
  int fault_inject_sleep_time_ms_;
  pthread_t thread_;
  DBus::Tag continuation_tag_;

 private:
  friend class GobiModem;

  DISALLOW_COPY_AND_ASSIGN(SessionStarter);
};

// Tracks a call to Enable (enable or disable) because the Enable
// operation waits for the PowerCallback, or because we cannot respond
// immmediately (because a long-running connect is already in
// progress)
class PendingEnable {
 public:
  explicit PendingEnable(bool enable) : enable_(enable) {}
  ~PendingEnable() {}

  // NB: contrary to our style guide, this throws an exception that
  // dbus catches
  void RecordEnableAndReturnLater(GobiModem *modem) {
    modem->return_later(&tag_);
  }

  DBus::Tag tag_;
  const bool enable_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PendingEnable);
};

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

static struct udev_enumerate *enumerate_net_devices(struct udev *udev) {
  int rc;
  struct udev_enumerate *udev_enumerate = udev_enumerate_new(udev);

  if (!udev_enumerate) {
    return nullptr;
  }

  rc = udev_enumerate_add_match_subsystem(udev_enumerate, "net");
  if (rc != 0) {
    udev_enumerate_unref(udev_enumerate);
    return nullptr;
  }

  rc = udev_enumerate_scan_devices(udev_enumerate);
  if (rc != 0) {
    udev_enumerate_unref(udev_enumerate);
    return nullptr;
  }
  return udev_enumerate;
}

GobiModem* GobiModem::connected_modem_(nullptr);
GobiModemHandler* GobiModem::handler_;
GobiModem::mutex_wrapper_ GobiModem::modem_mutex_;

bool StartExitTrampoline(void *arg) {
  GobiModem *modem = static_cast<GobiModem*>(arg);
  return modem->StartExit();
}

bool ExitOkTrampoline(void *arg) {
  GobiModem *modem = static_cast<GobiModem*>(arg);
  return !modem->is_connecting_or_connected();
}

GobiModem::GobiModem(DBus::Connection& connection,
                     const DBus::Path& path,
                     const gobi::DeviceElement& device,
                     gobi::Sdk* sdk,
                     GobiModemHelper *modem_helper)
    : DBus::ObjectAdaptor(connection, path),
      sdk_(sdk),
      modem_helper_(modem_helper),
      last_seen_(-1),
      mm_state_(MM_MODEM_STATE_UNKNOWN),
      session_id_(0),
      signal_available_(false),
      exiting_(false),
      device_resetting_(false),
      getting_deallocated_(false),
      session_starter_in_flight_(false),
      disconnect_time_(METRIC_BASE_NAME "Disconnect", 0, 150000, 20),
      registration_time_(METRIC_BASE_NAME "Registration", 0, 150000, 20) {
  memcpy(&device_, &device, sizeof(device_));
}

void GobiModem::Init() {
  sdk_->set_current_modem_path(path());
  metrics_lib_.reset(new MetricsLibrary());

  // Initialize DBus object properties
  IpMethod = MM_MODEM_IP_METHOD_DHCP;
  UnlockRequired = "";
  UnlockRetries = 999;
  SetDeviceProperties();
  SetModemProperties();

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
}

GobiModem::~GobiModem() {
  pthread_mutex_lock(&modem_mutex_.mutex_);
  getting_deallocated_ = true;
  pthread_mutex_unlock(&modem_mutex_.mutex_);

  if (pending_enable_) {
    // Despite the imminent destruction of the modem, pretend that the
    // pending_enable succeeded.  It is a race anyway.
    FinishEnable(DBus::Error());
  }
  handler_->server().start_exit_hooks().Del(hooks_name_);
  handler_->server().exit_ok_hooks().Del(hooks_name_);

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


void GobiModem::RescheduleDisable(void) {
  static const int kRetryDisableTimeoutSec = 1;

  retry_disable_callback_source_.TimeoutAddFull(
      G_PRIORITY_DEFAULT,
      kRetryDisableTimeoutSec,
      RetryDisableCallback,
      new CallbackArgs(new DBus::Path(path())),
      CleanupRetryDisableCallback);
}

void GobiModem::CleanupRetryDisableCallback(gpointer data) {
  delete static_cast<CallbackArgs*>(data);
}

gboolean GobiModem::RetryDisableCallback(gpointer data) {
  CallbackArgs *args = static_cast<CallbackArgs*>(data);

  // GobiModem::handler_ is a static
  GobiModem *modem = GobiModem::handler_->LookupByDbusPath(*args->path);
  if (!modem) {
    LOG(ERROR) << "DisableRetryCallback with no modem";
    return FALSE;
  }
  modem->PerformDeferredDisable();
  return FALSE;
}


// EnableHelper
//
// Enable or Disable (poweroff) the modem based on the enable flag.
// Calling the GOBI SetPower API does not immediately change the power
// level of the modem, it merely starts the process.  Once the power
// level has changed, the PowerModeHandler is invoked, and
// PowerModeHandler finishes updating the state of the modem and
// disconnecting from the Qualcomm API when the modem is disabled.
//
// There are several failure modes which include failing to
// communicate with the gobi API and failing to communicate with the
// module.
//
// If the user_initiated argument is false, force the modem to be disabled even
// if an error is returned when trying stop the current data session.
//
// Returns:
//   true -  to indicate either that SetPower operation has started and
//           PowerModeHandler will be called later, or that
//           CancelDataSession has been called and
//           SessionStarterDoneCallback will be called later.
//   false - the dbus error has been set because the operation could
//           not be completed, or the operation would have no effect.
//
bool GobiModem::EnableHelper(const bool& enable, DBus::Error& error,
                             const bool &user_initiated) {
  if (enable && Enabled()) {
    ScopedApiConnection connection(*this);
    connection.ApiConnect(error);
    CheckEnableOk(error);
  } else if (enable && !Enabled()) {
    ApiConnect(error);
    if (error.is_set())
      return false;
    if (!CheckEnableOk(error)) {
      ApiDisconnect();
      return false;
    }
    LogGobiInformation();

    /* Check the Enabled state again after API connect.  The cached
     * value may be stale.  This can happen if the RF Enable pin was
     * pulled low and a SetPower call was made, but failed with 1083.
     * When the pin voltage changes the modem will be enabled, but
     * because this plugin does not maintain a connection to the GOBI
     * API, there will be no callback to inform it of the change in
     * power state.
     */
    GetPowerState();
    if (Enabled()) {
      return false;
    }

    ULONG rc = sdk_->SetPower(gobi::kOnline);
    metrics_lib_->SendEnumToUMA(METRIC_BASE_NAME "SetPower",
                                QCErrorToMetricIndex(rc),
                                METRIC_INDEX_MAX);
    if (rc != 0) {
      error.set(kSdkError, "SetPower");
      LOG(WARNING) << "SetPower failed : " << rc;
      ApiDisconnect();
      return false;
    }
    SetMmState(MM_MODEM_STATE_ENABLING, MM_MODEM_STATE_CHANGED_REASON_UNKNOWN);
    return true;
  } else if (!enable && Enabled()) {
    if (is_connecting_or_connected()) {
      LOG(INFO) << "Disable while connected or connect in flight";

      int fault_inject_sleep_time_ms = static_cast <useconds_t> (
          injected_faults_["DisableSleepMs"]);
      if (fault_inject_sleep_time_ms) {
        LOG(WARNING) << "Fault injection:  disable sleeping for "
                     << fault_inject_sleep_time_ms << "ms";
        usleep(1000 * fault_inject_sleep_time_ms);
      }

      if (ForceDisconnect() == 0)
        return true;
      if (user_initiated) {
        // The error on force disconnect is probably a race.  If this
        // was a user initiated request, allow
        // SessionStarterDoneCallback to run, and try to disable later.
        RescheduleDisable();
        return true;
      }
    }
    ULONG rc = sdk_->SetPower(gobi::kPersistentLowPower);
    if (rc == gobi::kErrorReceivingQmiRequest ||
        rc == gobi::kTimeoutReceivingQmiRequest)
      // SetPower() sometimes fail with one of these errors and retrying
      // the call appears to succeed.
      rc = sdk_->SetPower(gobi::kPersistentLowPower);
    if (rc != 0) {
      error.set(kSdkError, "SetPower");
      LOG(WARNING) << "SetPower failed : " << rc;
      return false;
    }
    SetMmState(MM_MODEM_STATE_DISABLING, MM_MODEM_STATE_CHANGED_REASON_UNKNOWN);
    return true;
  }
  // The operation is already done!
  if (!error)
    LOG(WARNING) << "Operation Enable(" << enable
                 << ") has no effect on modem in state: " << Enabled();
  else
    LOG(WARNING) << "Operation Enable(" << enable
                 << ") on modem in state: " << Enabled()
                 << " returning error \"" << error.name() << " - "
                 << error.message() << "\"";
  return false;
}

// DBUS Methods: Modem
void GobiModem::Enable(const bool& enable, DBus::Error& error) {
  bool operation_pending;

  LOG(INFO) << "Enable: " << Enabled() << " => " << enable;

  if (pending_enable_) {
    LOG(INFO) << "Already have a pending Enable operation";
    error.set(kModeError, "Already have a pending Enable operation");
    return;
  }
  operation_pending = EnableHelper(enable, error, true);
  if (operation_pending) {
    CHECK(!pending_enable_);
    CHECK(!error);
    // Cleaned up in PowerModeHandler or SessionStarterDoneCallback
    pending_enable_.reset(new PendingEnable(enable));
    pending_enable_->RecordEnableAndReturnLater(this);
    CHECK(false) << "Not reached";
  }
}

void GobiModem::Connect(const std::string& unused_number, DBus::Error& error) {
  DBusPropertyMap properties;
  Connect(properties, error);
}

ULONG GobiModem::StopDataSession(ULONG session_id) {
  disconnect_time_.Start();
  ULONG rc = sdk_->StopDataSession(session_id);
  if (rc != 0)
    disconnect_time_.Reset();
  return rc;
}

void GobiModem::Disconnect(DBus::Error& error) {
  LOG(INFO) << "Disconnect(" << session_id_ << ")";
  if (session_id_ == 0) {
    LOG(WARNING) << "Disconnect called when not connected";
    error.set(kDisconnectError, "Not connected");
    return;
  }
  ULONG rc = StopDataSession(session_id_);
  ENSURE_SDK_SUCCESS(StopDataSession, rc, kDisconnectError);
  SetMmState(MM_MODEM_STATE_DISCONNECTING,
             MM_MODEM_STATE_CHANGED_REASON_USER_REQUESTED);
  error.set(kOperationInitiatedError, "");
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
  ScopedApiConnection connection(*this);
  connection.ApiConnect(error);
  if (error.is_set())
    return;

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
  if (pending_enable_) {
    LOG(WARNING) << "Connect while modem "
                 << (pending_enable_->enable_ ? "enabling" : "disabling")
                 << ".";
    error.set(kConnectError, "Enable operation in progress");
    return;
  }
  if (exiting_) {
    LOG(WARNING) << "Connect when exiting";
    error.set(kConnectError, "Cromo is exiting");
    return;
  }
  if (session_starter_in_flight_) {
    LOG(WARNING) << "Session start already in flight";
    error.set(kConnectError, "Connect already in progress");
    return;
  }
  const char* apn = utilities::ExtractString(properties, "apn", nullptr, error);
  const char* username =
      utilities::ExtractString(properties, "username", nullptr, error);
  const char* password =
      utilities::ExtractString(properties, "password", nullptr, error);

  if (apn !=  nullptr)
    LOG(INFO) << "Starting data session for APN " << apn;

  SessionStarter *starter = new SessionStarter(sdk_,
                                               this,
                                               apn,
                                               username,
                                               password);
  session_starter_in_flight_ = true;
  if (mm_state_ == MM_MODEM_STATE_REGISTERED)
    SetMmState(MM_MODEM_STATE_CONNECTING,
               MM_MODEM_STATE_CHANGED_REASON_UNKNOWN);
  starter->StartDataSession(error);

  return_later(&starter->continuation_tag_);
}

void GobiModem::FinishDeferredCall(DBus::Tag *tag, const DBus::Error &error) {
  // find_continuation, return_error, and return_now are defined in
  // <dbus-c++/object.h>
  DBus::ObjectAdaptor::Continuation *c = find_continuation(tag);
  if (!c) {
    LOG(ERROR) << "Could not find continuation: tag = " << std::ios::hex << tag;
    return;
  }
  if (error.is_set()) {
    return_error(c, error);
  } else {
    return_now(c);
  }
}

void GobiModem::FinishEnable(const DBus::Error &error) {
  std::unique_ptr<PendingEnable> scoped_enable = std::move(pending_enable_);
  retry_disable_callback_source_.Remove();
  FinishDeferredCall(&scoped_enable->tag_, error);
}

gobi::RegistrationState GobiModem::GetRegistrationState(gobi::Sdk *sdk) {
  ULONG rc = 0;
  ULONG gobi_reg_state = gobi::kRegistrationStateUnknown;
  ULONG roaming_state;
  ULONG ran;
  WORD mcc, mnc;
  CHAR netname[32];
  BYTE radio_interfaces[10];
  BYTE num_radio_interfaces = arraysize(radio_interfaces);
  rc = sdk->GetServingNetwork(&gobi_reg_state, &ran,
                              &num_radio_interfaces,
                              radio_interfaces, &roaming_state,
                              &mcc, &mnc,
                              sizeof(netname), netname);
  if (rc) {
    LOG(ERROR) << "Failed to get current registration state: " << rc;
    return gobi::kRegistrationStateUnknown;
  }
  return static_cast<gobi::RegistrationState>(gobi_reg_state);
}

void GobiModem::PerformDeferredDisable() {
  if (pending_enable_) {
    DBus::Error disable_error;
    bool operation_pending;
    CHECK(pending_enable_->enable_ == false);
    operation_pending = EnableHelper(false, disable_error, false);
    CHECK(operation_pending || disable_error);
    if (disable_error) {
      FinishEnable(disable_error);
    }
    // NOTE: if !disable_error, there is nothing to do, because
    // PowerModeHandler will eventually return a value.
  }
}

void GobiModem::SessionStarterDoneCallback(SessionStarter *starter) {
  session_starter_in_flight_ = false;
  if (injected_faults_["ConnectFailsWithErrorSendingQmiRequest"]) {
    LOG(ERROR) << "Fault injection: Making StartDataSession return QMI error";
    starter->return_value_ = gobi::kErrorSendingQmiRequest;
  }

  DBus::Error error;
  if (starter->return_value_ == 0) {
    session_id_ = starter->session_id_;

    if (pending_enable_) {
      error.set(kConnectError, "StartDataSession Cancelled");
      LOG(INFO) << "Cancellation arrived after connect succeeded";
      ULONG rc = StopDataSession(session_id_);
      if (rc != 0) {
        LOG(ERROR) << "Could not disconnect: " << rc;
      } else {
        SetMmState(MM_MODEM_STATE_DISCONNECTING,
                   MM_MODEM_STATE_CHANGED_REASON_USER_REQUESTED);
      }
      // SessionStateHandler will clear session_id_
    }
  } else {
    LOG(WARNING) << "StartDataSession failed: " << starter->return_value_;
    const char* err_name;
    const char* err_msg;
    switch (starter->return_value_) {
      case gobi::kCallFailed:
        LOG(WARNING) << "Failure Reason " <<  starter->failure_reason_;
        err_name = QMICallFailureToMMError(starter->failure_reason_);
        err_msg = "Connect failed";
        break;
      case gobi::kErrorSendingQmiRequest:
      case gobi::kErrorReceivingQmiRequest:
        if (!pending_enable_) {
          // Normally the SDK enqueues an SdkErrorHandler event when
          // it sees these errors.  But, since these errors occur
          // benignly on StartDataSession cancellation, the SDK
          // doesn't do that automatically for StartDataSession. If we
          // have no cancellation, then this is a for-real problem,
          // and we reboot cromo
          SinkSdkError(path(), "StartDataSession", starter->return_value_);
        }
        // fall through
      default:
        err_name = kConnectError;
        err_msg = "StartDataSession";
        break;
    }
    error.set(err_name, err_msg);
  }
  LOG(INFO) << "Returning deferred connect call";
  FinishDeferredCall(&starter->continuation_tag_, error);

  if (mm_state_ == MM_MODEM_STATE_CONNECTING) {
    if (!error.is_set())
      SetMmState(MM_MODEM_STATE_CONNECTED,
                 MM_MODEM_STATE_CHANGED_REASON_UNKNOWN);
    else
      SetMmState(MM_MODEM_STATE_REGISTERED,
                 MM_MODEM_STATE_CHANGED_REASON_UNKNOWN);
  }

  if (pending_enable_)
    // The pending_enable_ operation (which is most certainly a
    // "DISABLE") should not be run until the SessionStateHandler has
    // run (assuming it gets run).  By defering this call for a
    // second, we give a chance to the SessionStateHandler to run, but
    // also protect ourselves in case the SessionStateHandler never runs.
    RescheduleDisable();
}

void GobiModem::SetMmState(uint32_t new_state, uint32_t reason) {
  if (new_state != mm_state_) {
    LOG(INFO) << "MM state change: " << mm_state_ << " => " << new_state;
    StateChanged(mm_state_, new_state, reason);
    State = new_state;
    mm_state_ = new_state;
  }
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
      ULONG radio_interfaces[10];
      BYTE num_radio_interfaces = arraysize(radio_interfaces);
      char buf[255];
      rc = sdk_->GetServingNetwork(&reg_state, &l1, &num_radio_interfaces,
                                   reinterpret_cast<BYTE*>(radio_interfaces),
                                   &l2, &w1, &w2, sizeof(buf), buf);
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

  DBus::Error api_connect_error;
  ScopedApiConnection connection(*this);
  // Errors are ignored so data not requiring a connection can be returned
  connection.ApiConnect(api_connect_error);

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
  const Carrier *carrier = nullptr;
  rc = sdk_->GetFirmwareInfo(&firmware_id,
                             &technology_id,
                             &carrier_id,
                             &region,
                             &gps_capability);
  if (rc == 0) {
    carrier = handler_->server().FindCarrierByCarrierId(carrier_id);
    if (carrier)
      result["carrier"].writer().append_string(carrier->name());
    else
      LOG(WARNING) << "Carrier lookup failed for ID " << carrier_id;

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
  } else {
    LOG(WARNING) << "GetFirmwareInfo failed: " << rc;
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
  if (carrier)
    carrier-> ModifyModemStatusReturn(&result);

  return result;
}

// This is only in debug builds; if you add actual code here, see
// RegisterCallbacks().
static void ByteTotalsCallback(ULONGLONG tx, ULONGLONG rx) {}

// This is only in debug builds; if you add actual code here, see
// RegisterCallbacks().
gboolean GobiModem::DormancyStatusCallback(gpointer data) {
  DormancyStatusArgs *args = reinterpret_cast<DormancyStatusArgs*>(data);
  GobiModem *modem = handler_->LookupByDbusPath(*args->path);
  if (modem->event_enabled[GOBI_EVENT_DORMANCY]) {
    modem->DormancyStatus(args->status == gobi::kDormant);
  }
  return FALSE;
}

static void MobileIPStatusCallback(ULONG status) {
  LOG(INFO) << "MobileIPStatusCallback: " << status;
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
  sdk_->SetRFInfoCallback(RFInfoCallbackTrampoline);
  sdk_->SetLURejectCallback(LURejectCallback);
  sdk_->SetPDSStateCallback(PDSStateCallback);

  sdk_->SetPowerCallback(PowerCallbackTrampoline);
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
  int interval = (kMaxSignalStrengthDbm - kMinSignalStrengthDbm) /
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
  // It is safe to test for nullptr outside of a lock because ApiConnect
  // is only called by the main thread, and only the main thread can
  // modify connected_modem_.
  if (connected_modem_) {
    LOG(INFO) << "ApiAlready connected: connected_modem_=0x" << connected_modem_
              << "this=0x" << this;
    error.set(kErrorOperationNotAllowed,
              "Only one modem can be connected via Api");
    return;
  }
  LOG(INFO) << "Connecting to QCWWAN";
  pthread_mutex_lock(&modem_mutex_.mutex_);
  connected_modem_ = this;
  if (getting_deallocated_) {
    LOG(INFO) << "Modem getting deallocated, not connecting";
    pthread_mutex_unlock(&modem_mutex_.mutex_);
    error.set(kErrorOperationNotAllowed, "Modem is getting deallocated");
    return;
  }
  pthread_mutex_unlock(&modem_mutex_.mutex_);

  ULONG rc = sdk_->QCWWANConnect(device_.deviceNode, device_.deviceKey);
  if (rc != 0 || !CanMakeMethodCalls()) {
    LOG(ERROR) << "QCWWANConnect() failed.  Exiting so modem can reset." << rc;
    ExitAndResetDevice(rc);
  }
  RegisterCallbacks();
}


ULONG GobiModem::ApiDisconnect(void) {
  ULONG rc = 0;
  pthread_mutex_lock(&modem_mutex_.mutex_);
  if (connected_modem_ == this) {
    LOG(INFO) << "Disconnecting from QCWWAN.  this_=0x" << this;
    if (session_starter_in_flight_) {
      SessionStarter::CancelDataSession(sdk_);
    }
    connected_modem_ = nullptr;
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
    LOG(INFO) << "Mobile IP profile: " << static_cast<int>(index);
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
  SetEnabled(false);
  SetMmState(MM_MODEM_STATE_DISABLED, MM_MODEM_STATE_CHANGED_REASON_UNKNOWN);
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
  modem_helper_->SetCarrier(this, handler_, carrier_name, error);
}

uint32_t GobiModem::CommonGetSignalQuality(DBus::Error& error) {
  if (!Enabled()) {
    LOG(WARNING) << "GetSignalQuality on disabled modem";
    error.set(kModeError, "Modem is disabled");
  } else {
    int32_t signal_strength_dbm;
    GetSignalStrengthDbm(signal_strength_dbm, nullptr, error);
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

  INT8 max_strength = std::numeric_limits<INT8>::min();
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
  if (session_id_) {
    ULONG db_technology;
    rc =  sdk_->GetDataBearerTechnology(&db_technology);
    if (rc != 0) {
      LOG(WARNING) << "GetDataBearerTechnology failed: " << rc;
      error.set(kSdkError, "GetDataBearerTechnology");
      return;
    }
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

void GobiModem::GetPowerState() {
  ULONG power_mode;
  ULONG rc = sdk_->GetPower(&power_mode);
  if (rc != 0) {
    LOG(INFO) << "Cannot determine initial power mode: Error " << rc;
  } else {
    LOG(INFO) << "Initial power mode: " << power_mode;
    if (power_mode == gobi::kOnline) {
      SetEnabled(true);
      State = mm_state_ = MM_MODEM_STATE_ENABLED;
      RegistrationStateHandler();
    } else {
      SetEnabled(false);
      State = mm_state_ = MM_MODEM_STATE_DISABLED;
    }
  }
}

// Set properties for which a connection to the SDK is required
// to obtain the needed information. If this is called before
// the modem is enabled, we connect to the SDK, get the properties
// we need, and then disconnect from the SDK. Otherwise, we
// stay connected.
void GobiModem::SetModemProperties() {
  DBus::Error error;

  ApiConnect(error);
  if (error.is_set()) {
    Type = MM_MODEM_TYPE_CDMA;
    return;
  }

  GetPowerState();

  ULONG rc;
  ULONG u1, u2, u3, u4;
  ULONG radio_interfaces[10];
  ULONG num_radio_interfaces = arraysize(radio_interfaces);
  rc = sdk_->GetDeviceCapabilities(&u1, &u2, &u3, &u4, &num_radio_interfaces,
                                   reinterpret_cast<BYTE*>(radio_interfaces));
  if (rc == 0) {
    if (num_radio_interfaces != 0) {
      if (radio_interfaces[0] == gobi::kRfiGsm ||
          radio_interfaces[0] == gobi::kRfiUmts) {
        Type = MM_MODEM_TYPE_GSM;
      } else {
        Type = MM_MODEM_TYPE_CDMA;
      }
    }
  }
  SetTechnologySpecificProperties();
  if (mm_state_ == MM_MODEM_STATE_UNKNOWN ||
      mm_state_ == MM_MODEM_STATE_DISABLED)
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
  if (name == "ClearFaults") {
    LOG(ERROR) << "Clearing injected faults";
    sdk_->InjectFaultSdkError(0);
    injected_faults_.clear();
  } else if (name == "SdkError") {
    LOG(ERROR) << "Injecting fault:  All Sdk calls will return " << value;
    sdk_->InjectFaultSdkError(value);
  } else {
    LOG(ERROR) << "Injecting fault " << name << ": " << value;
    injected_faults_[name] = value;
  }
}

void GobiModem::SetNetworkPreference(const int32_t &value,
                                     DBus::Error& error) {
  LOG(INFO) << __func__ << ": " << value;

  DBus::Error api_connect_error;
  ScopedApiConnection connection(*this);
  connection.ApiConnect(api_connect_error);

  ULONG preference;
  switch (value) {
    case kNetworkPreferenceAutomatic:
      preference = gobi::kRegistrationTechnologyAutomatic;
      break;
    case kNetworkPreferenceCdma1xRtt:
      preference = gobi::kRegistrationTechnologyCdma |
                   (gobi::kRegistrationTechnologyPreferenceCdma1xRtt << 2);
      break;
    case kNetworkPreferenceCdmaEvdo:
      preference = gobi::kRegistrationTechnologyCdma |
                   (gobi::kRegistrationTechnologyPreferenceCdmaEvdo << 2);
      break;
    case kNetworkPreferenceGsm:
      preference = gobi::kRegistrationTechnologyUmts |
                   (gobi::kRegistrationTechnologyPreferenceUmtsGsm << 2);
      break;
    case kNetworkPreferenceWcdma:
      preference = gobi::kRegistrationTechnologyUmts |
                   (gobi::kRegistrationTechnologyPreferenceUmtsWcdma << 2);
      break;
    default:
      LOG(ERROR) << __func__ << ": Invalid technology " << value;
      error.set(kSdkError, "SetNetworkPreference");
      return;
  }
  ULONG rc = sdk_->SetNetworkPreference(
                 preference, gobi::kRegistrationPreferencePersistent);
  if (rc != 0 && rc != gobi::kOperationHasNoEffect) {
    LOG(ERROR) << "Failed to set network registration preference: " << rc;
    error.set(kSdkError, "SetNetworkPreference");
  }
}

void GobiModem::ForceModemActivatedStatus(DBus::Error& error) {
}

void GobiModem::ClearIdleCallbacks() {
  for (std::set<guint>::iterator it = idle_callback_ids_.begin();
       it != idle_callback_ids_.end();
       ++it) {
    g_source_remove(*it);
  }
  idle_callback_ids_.clear();
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
  if (modem) {
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
  if (modem)
    modem->SignalStrengthHandler(args->signal_strength, args->radio_interface);
  return FALSE;
}

gboolean GobiModem::PowerCallback(gpointer data) {
  CallbackArgs* args = static_cast<CallbackArgs*>(data);
  GobiModem* modem = handler_->LookupByDbusPath(*args->path);
  if (modem)
    modem->PowerModeHandler();
  return FALSE;
}

gboolean GobiModem::SessionStateCallback(gpointer data) {
  SessionStateArgs* args = static_cast<SessionStateArgs*>(data);
  GobiModem* modem = handler_->LookupByDbusPath(*args->path);
  if (modem)
    modem->SessionStateHandler(args->state, args->session_end_reason);
  return FALSE;
}

gboolean GobiModem::RegistrationStateCallback(gpointer data) {
  CallbackArgs* args = static_cast<CallbackArgs*>(data);
  GobiModem* modem = handler_->LookupByDbusPath(*args->path);
  if (modem)
    modem->RegistrationStateHandler();
  return FALSE;
}

gboolean GobiModem::DataCapabilitiesCallback(gpointer data) {
  DataCapabilitiesArgs* args = static_cast<DataCapabilitiesArgs*>(data);
  GobiModem* modem = handler_->LookupByDbusPath(*args->path);
  if (modem)
    modem->DataCapabilitiesHandler(args->num_data_caps, args->data_caps);
  return FALSE;
}

gboolean GobiModem::DataBearerTechnologyCallback(gpointer data) {
  DataBearerTechnologyArgs* args = static_cast<DataBearerTechnologyArgs*>(data);
  GobiModem* modem = handler_->LookupByDbusPath(*args->path);
  if (modem)
    modem->DataBearerTechnologyHandler(args->technology);
  return FALSE;
}

void GobiModem::PowerModeHandler() {
  ULONG power_mode;
  DBus::Error error;
  ULONG rc = sdk_->GetPower(&power_mode);
  if (rc != 0) {
    LOG(INFO) << "Cannot determine power mode: Error " << rc;
    error.set(kSdkError, "GetPowerMode");
  } else {
    LOG(INFO) << "PowerModeHandler: " << power_mode;
    if (power_mode == gobi::kOnline) {
      SetEnabled(true);
      SetMmState(MM_MODEM_STATE_ENABLED, MM_MODEM_STATE_CHANGED_REASON_UNKNOWN);
      registration_time_.Start();    // Stopped in
      // GobiCdmaModem::RegistrationStateHandler,
      // if appropriate
    } else {
      ApiDisconnect();
      SetEnabled(false);
      SetMmState(MM_MODEM_STATE_DISABLED,
                 MM_MODEM_STATE_CHANGED_REASON_UNKNOWN);
    }
  }
  if (pending_enable_) {
    FinishEnable(error);
    LOG(INFO) << "PowerModeHandler: finishing deferred call";
  }
}

void GobiModem::SessionStateHandler(ULONG state, ULONG session_end_reason) {
  LOG(INFO) << "SessionStateHandler state: " << state
            << " reason: "
            << (state == gobi::kConnected ? 0 : session_end_reason);
  if (state == gobi::kConnected) {
    ULONG data_bearer_technology;
    sdk_->GetDataBearerTechnology(&data_bearer_technology);
    // TODO(ers) send a signal or change a property to notify
    // listeners about the change in data bearer technology
  }

  if (state == gobi::kDisconnected) {
    disconnect_time_.Stop();
    session_id_ = 0;
    unsigned int reason = QMIReasonToMMReason(session_end_reason);
    if (pending_enable_)
      PerformDeferredDisable();
    else
      SetMmState(QCStateToMMState(state), reason);
  } else if (state == gobi::kConnected) {
    // Nothing to do here; this is handled in SessionStarterDoneCallback
  }
}

void GobiModem::DataBearerTechnologyHandler(ULONG technology) {
  // Default is to ignore the argument and treat this as a
  // registration state change. This behavior can be overridden.
  RegistrationStateHandler();
}

// Set DBus properties that pertain to the modem hardware device.
// The properties set here are Device, MasterDevice, and Driver.
void GobiModem::SetDeviceProperties() {
  struct udev *udev = udev_new();
  if (!udev) {
    LOG(WARNING) << "udev == nullptr";
    return;
  }

  struct udev_enumerate *udev_enumerate = enumerate_net_devices(udev);
  if (!udev_enumerate) {
    LOG(WARNING) << "udev_enumerate == nullptr";
    udev_unref(udev);
    return;
  }

  struct udev_list_entry *entry;
  for (entry = udev_enumerate_get_list_entry(udev_enumerate);
       entry != nullptr;
       entry = udev_list_entry_get_next(entry)) {
    std::string syspath(udev_list_entry_get_name(entry));

    struct udev_device *udev_device =
        udev_device_new_from_syspath(udev, syspath.c_str());
    if (!udev_device)
      continue;

    std::string driver;
    struct udev_device *parent = udev_device_get_parent(udev_device);
    if (parent) {
      const char *udev_driver = udev_device_get_driver(parent);
      if (udev_driver) {
        driver = udev_driver;
      }
    }

    if (driver == k2kNetworkDriver ||
        driver == k3kNetworkDriver ||
        driver == kUnifiedNetworkDriver) {
      // Extract last portion of syspath...
      size_t found = syspath.find_last_of('/');
      if (found != std::string::npos) {
        Device = syspath.substr(found + 1);
        struct udev_device *grandparent;
        if (parent) {
          grandparent = udev_device_get_parent(parent);
          if (grandparent) {
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
  return (ForceDisconnect() == 0);
}

const char* QMIReturnCodeToMMError(unsigned int qmicode) {
  switch (qmicode) {
    case gobi::kIncorrectPinId:
      return kErrorIncorrectPassword;
    case gobi::kInvalidPinId:
    case gobi::kAccessToRequiredEntityNotAvailable:
      return kErrorSimPinRequired;
    case gobi::kPinBlocked:
    case gobi::kPinPermanentlyBlocked:
      // blocked vs. permanently block is distinguished by
      // looking at the value of UnlockRetries. If it is
      // zero, then the SIM is permanently blocked.
      return kErrorSimPukRequired;
    default:
      return nullptr;
  }
}

// Map call failure reasons into ModemManager errors
const char* QMICallFailureToMMError(unsigned int qmireason) {
  switch (qmireason) {
    case gobi::kReasonBadApn:
    case gobi::kReasonNotSubscribed:
      return kErrorGprsNotSubscribed;
    default:
      return kErrorGsmUnknown;
  }
}

unsigned int QMIReasonToMMReason(unsigned int qmireason) {
  switch (qmireason) {
    case gobi::kReasonClientEndedCall:
      return MM_MODEM_STATE_CHANGED_REASON_USER_REQUESTED;
    default:
      return MM_MODEM_STATE_CHANGED_REASON_UNKNOWN;
  }
}

// Return true if a data session has started, or is in the process of starting.
bool GobiModem::is_connecting_or_connected() {
  return session_starter_in_flight_ || session_id_;
}

// Force a disconnect of a data session, or stop the process of
// starting a datasession
//
// Return 0 on success, gobi error code otherwise
ULONG GobiModem::ForceDisconnect() {
  ULONG rc = 0;
  if (session_id_) {
    LOG(INFO) << "ForceDisconnect: Stopping data session";
    rc = StopDataSession(session_id_);
    SetMmState(MM_MODEM_STATE_DISCONNECTING,
               MM_MODEM_STATE_CHANGED_REASON_USER_REQUESTED);
    if (rc != 0)
      LOG(WARNING) << "ForceDisconnect: StopDataSessionFailed: " << rc;
  } else if (session_starter_in_flight_) {
    LOG(INFO) << "ForceDisconnect: Canceling StartDataSession";
    rc = SessionStarter::CancelDataSession(sdk_);
    if (rc != 0)
      LOG(WARNING) << "ForceDisconnect: CancelDataSessionFailed: " << rc;
    else
      SetMmState(QCStateToMMState(gobi::kDisconnected),
                 MM_MODEM_STATE_CHANGED_REASON_USER_REQUESTED);
  }
  return rc;
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

void GobiModem::SetEnabled(bool enabled) {
  Enabled = enabled;
  LOG(INFO) << "MM sending Enabled property changed signal: " << enabled;
  utilities::DBusPropertyMap props;
  props["Enabled"].writer().append_bool(enabled);
  MmPropertiesChanged(
      org::freedesktop::ModemManager::Modem_adaptor::introspect()->name,
      props);
}
