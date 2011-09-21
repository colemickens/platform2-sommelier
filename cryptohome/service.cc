// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "service.h"

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/platform_file.h>
#include <base/scoped_ptr.h>
#include <base/string_util.h>
#include <base/time.h>
#include <chromeos/dbus/dbus.h>
#include <metrics/metrics_library.h>
#include <metrics/timer.h>
#include <string>
#include <vector>

#include "cryptohome/marshal.glibmarshal.h"
#include "cryptohome_event_source.h"
#include "crypto.h"
#include "install_attributes.h"
#include "interface.h"
#include "mount.h"
#include "secure_blob.h"
#include "tpm.h"
#include "username_passkey.h"
#include "vault_keyset.pb.h"

// Forcibly namespace the dbus-bindings generated server bindings instead of
// modifying the files afterward.
namespace cryptohome {  // NOLINT
namespace gobject {  // NOLINT
#include "cryptohome/bindings/server.h"
}  // namespace gobject
}  // namespace cryptohome

namespace cryptohome {

// Encapsulates histogram parameters, for UMA reporting.
struct HistogramParams {
  const char* metric_name;
  int min_sample;
  int max_sample;
  int num_buckets;
};

// Wrapper for all timers used by Service.
class TimerCollection {
 public:
  enum TimerType {
    kAsyncMountTimer,
    kSyncMountTimer,
    kAsyncGuestMountTimer,
    kSyncGuestMountTimer,
    kTpmTakeOwnershipTimer,
    kPkcs11InitTimer,
    kNumTimerTypes // For the number of timer types.
  };

  TimerCollection()
      : timer_array_() {}
  virtual ~TimerCollection() {}

  // |is_start| represents whether the timer is supposed to start (true), or
  // stop (false).
  bool UpdateTimer(TimerType timer_type, bool is_start) {
    chromeos_metrics::TimerReporter* timer = timer_array_[timer_type].get();
    if (is_start) {
      // Starts timer related to |timer_type|. Creates it if necessary.
      if (!timer) {
        timer = new chromeos_metrics::TimerReporter(
            kHistogramParams[timer_type].metric_name,
            kHistogramParams[timer_type].min_sample,
            kHistogramParams[timer_type].max_sample,
            kHistogramParams[timer_type].num_buckets);
        timer_array_[timer_type].reset(timer);
      }
      return timer->Start();
    }
    // Stops the timer.
    bool success = (timer && timer->HasStarted() &&
                    timer->Stop() && timer->ReportMilliseconds());
    if (!success) {
      LOG(WARNING) << "Timer " << kHistogramParams[timer_type].metric_name
                   << " failed to report";
    }
    return success;
  }

 private:
  // Histogram parameters. This should match the order of
  // 'TimerCollection::TimerType'. Min and max samples are in milliseconds.
  static const HistogramParams kHistogramParams[kNumTimerTypes];

  // The array of timers. TimerReporter's need to be pointers, since each of
  // them will be constructed with different arguments.
  scoped_ptr<chromeos_metrics::TimerReporter> timer_array_[kNumTimerTypes];

  DISALLOW_COPY_AND_ASSIGN(TimerCollection);
};

// static
const HistogramParams TimerCollection::kHistogramParams[kNumTimerTypes] = {
  {"Cryptohome.TimeToMountAsync", 0, 2000, 50},
  {"Cryptohome.TimeToMountSync", 0, 2000, 50},
  {"Cryptohome.TimeToMountGuestAsync", 0, 2000, 50},
  {"Cryptohome.TimeToMountGuestSync", 0, 2000, 50},
  {"Cryptohome.TimeToTakeTpmOwnership", 0, 10000, 50},
  {"Cryptohome.TimeToInitPkcs11", 0, 100000, 50}
};
// A note on the PKCS#11 initialization time:
// Max sample for PKCS#11 initialization time is 100s, since we are interested
// in recording the very first PKCS#11 initialization time, which is the
// lengthy one. Subsequent initializations are fast (under 1s) because they
// just check if PKCS#11 was previously initialized, returning immediately.
// These will all fall into the first histogram bucket. We are currently not
// filtering these since this initialization is done via a separated process,
// called via command line, and it is difficult to distinguish the first
// initialization from the others.

const int kAutoCleanupPeriodMS = 1000 * 60 * 60;  // 1 hour
const int kUpdateUserActivityPeriod = 24;  // divider of the former
const int kDefaultRandomSeedLength = 64;
const char* kMountThreadName = "MountThread";
const char* kTpmInitStatusEventType = "TpmInitStatus";

// The default entropy source to seed with random data from the TPM on startup.
const char* kDefaultEntropySource = "/dev/urandom";

// The name of the UMA user action for reporting a failure to initialize the
// PKCS#11.
const char* kMetricNamePkcs11InitFail = "Cryptohome.PKCS11InitFail";

class TpmInitStatus : public CryptohomeEventBase {
 public:
  TpmInitStatus()
      : took_ownership_(false),
        status_(false) { }
  virtual ~TpmInitStatus() { }

  virtual const char* GetEventName() {
    return kTpmInitStatusEventType;
  }

  void set_took_ownership(bool value) {
    took_ownership_ = value;
  }

  bool get_took_ownership() {
    return took_ownership_;
  }

  void set_status(bool value) {
    status_ = value;
  }

  bool get_status() {
    return status_;
  }

 private:
  bool took_ownership_;
  bool status_;
};

Service::Service()
    : loop_(NULL),
      cryptohome_(NULL),
      system_salt_(),
      default_mount_(new cryptohome::Mount()),
      mount_(default_mount_.get()),
      default_tpm_init_(new TpmInit()),
      tpm_init_(default_tpm_init_.get()),
      default_pkcs11_init_(new Pkcs11Init()),
      pkcs11_init_(default_pkcs11_init_.get()),
      initialize_tpm_(true),
      mount_thread_(kMountThreadName),
      async_complete_signal_(-1),
      tpm_init_signal_(-1),
      event_source_(),
      auto_cleanup_period_(kAutoCleanupPeriodMS),
      default_install_attrs_(new cryptohome::InstallAttributes(NULL)),
      install_attrs_(default_install_attrs_.get()),
      update_user_activity_period_(kUpdateUserActivityPeriod - 1),
      pkcs11_state_(kUninitialized),
      async_mount_pkcs11_init_sequence_id_(-1),
      async_guest_mount_sequence_id_(-1),
      timer_collection_(new TimerCollection()),
      reported_pkcs11_init_fail_(false) {
}

Service::~Service() {
  if (loop_)
    g_main_loop_unref(loop_);
  if (cryptohome_)
    g_object_unref(cryptohome_);
  if (mount_thread_.IsRunning()) {
    mount_thread_.Stop();
  }
}

bool Service::Initialize() {
  bool result = true;

  // Initialize the metrics library for stat reporting.
  metrics_lib_.Init();
  chromeos_metrics::TimerReporter::set_metrics_lib(&metrics_lib_);

  mount_->Init();
  // If the TPM is unowned or doesn't exist, it's safe for
  // this function to be called again. However, it shouldn't
  // be called across multiple threads in parallel.
  InitializeInstallAttributes(false);

  Tpm* tpm = const_cast<Tpm*>(mount_->get_crypto()->get_tpm());
  // TODO(wad) Determine if this should only be called if
  //           tpm->IsEnabled() is true.
  if (tpm && initialize_tpm_) {
    tpm_init_->set_tpm(tpm);
    tpm_init_->Init(this);
    if (!SeedUrandom()) {
      LOG(ERROR) << "FAILED TO SEED /dev/urandom AT START";
    }
  }
  // Install the type-info for the service with dbus.
  dbus_g_object_type_install_info(gobject::cryptohome_get_type(),
                                  &gobject::dbus_glib_cryptohome_object_info);
  if (!Reset()) {
    result = false;
  }

  async_complete_signal_ = g_signal_new("async_call_status",
                                        gobject::cryptohome_get_type(),
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL,
                                        NULL,
                                        cryptohome_VOID__INT_BOOLEAN_INT,
                                        G_TYPE_NONE,
                                        3,
                                        G_TYPE_INT,
                                        G_TYPE_BOOLEAN,
                                        G_TYPE_INT);

  tpm_init_signal_ = g_signal_new("tpm_init_status",
                                  gobject::cryptohome_get_type(),
                                  G_SIGNAL_RUN_LAST,
                                  0,
                                  NULL,
                                  NULL,
                                  cryptohome_VOID__BOOLEAN_BOOLEAN_BOOLEAN,
                                  G_TYPE_NONE,
                                  3,
                                  G_TYPE_BOOLEAN,
                                  G_TYPE_BOOLEAN,
                                  G_TYPE_BOOLEAN);

  mount_thread_.Start();

  // Start scheduling periodic cleanup events.  Note, that the first
  // event will be called by Chrome explicitly from the login screen.
  mount_thread_.message_loop()->PostDelayedTask(
      FROM_HERE, NewRunnableMethod(this, &Service::AutoCleanupCallback),
      auto_cleanup_period_);

  return result;
}

void Service::InitializeInstallAttributes(bool first_time) {
  Tpm* tpm = const_cast<Tpm*>(mount_->get_crypto()->get_tpm());
  // Wait for ownership if there is a working TPM.
  if (tpm && tpm->IsEnabled() && !tpm->IsOwned())
    return;

  // The TPM owning instance may have changed since initialization.
  // InstallAttributes can handle a NULL or !IsEnabled Tpm object.
  install_attrs_->SetTpm(tpm);

  if (first_time)
    // TODO(wad) Go nuclear if PrepareSystem fails!
    install_attrs_->PrepareSystem();

  // Init can fail without making the interface inconsistent so we're okay here.
  install_attrs_->Init();

  // Check if the machine is enterprise owned and report to mount_ then.
  DetectEnterpriseOwnership();
}

void Service::InitializePkcs11() {
  Tpm* tpm = const_cast<Tpm*>(mount_->get_crypto()->get_tpm());
  // Wait for ownership if there is a working TPM.
  if (tpm && tpm->IsEnabled() && !tpm->IsOwned()) {
    LOG(WARNING) << "TPM was not owned. TPM initialization call back will"
                 << " handle PKCS#11 initialization.";
    pkcs11_state_ = kIsWaitingOnTPM;
    return;
  }

  // Ok, so the TPM is owned. Time to request asynchronous initialization of
  // PKCS#11.
  // Make sure cryptohome is mounted, otherwise all of this is for naught.
  if (!mount_->IsCryptohomeMounted()) {
    LOG(WARNING) << "PKCS#11 initialization requested but cryptohome is"
                 << " not mounted.";
    return;
  }

  // Reset PKCS#11 initialization status. A successful completion of
  // MountTaskPkcs11_Init would set it in the service thread via NotifyEvent().
  timer_collection_->UpdateTimer(TimerCollection::kPkcs11InitTimer, true);
  pkcs11_state_ = kIsBeingInitialized;
  MountTaskPkcs11Init* pkcs11_init_task = new MountTaskPkcs11Init(this, mount_);
  LOG(INFO) << "Putting a Pkcs11_Initialize on the mount thread.";
  mount_thread_.message_loop()->PostTask(FROM_HERE, pkcs11_init_task);
}

bool Service::SeedUrandom() {
  SecureBlob random;
  if (!tpm_init_->GetRandomData(kDefaultRandomSeedLength, &random)) {
    LOG(ERROR) << "Could not get random data from the TPM";
    return false;
  }
  size_t written = file_util::WriteFile(FilePath(kDefaultEntropySource),
      static_cast<const char*>(random.const_data()), random.size());
  if (written != random.size()) {
    LOG(ERROR) << "Error writing data to /dev/urandom";
    return false;
  }
  return true;
}

bool Service::Reset() {
  if (cryptohome_)
    g_object_unref(cryptohome_);
  cryptohome_ = reinterpret_cast<gobject::Cryptohome*>(
      g_object_new(gobject::cryptohome_get_type(), NULL));
  // Allow references to this instance.
  cryptohome_->service = this;

  if (loop_) {
    ::g_main_loop_unref(loop_);
  }
  loop_ = g_main_loop_new(NULL, false);
  if (!loop_) {
    LOG(ERROR) << "Failed to create main loop";
    return false;
  }
  // Install the local event source for handling async results
  event_source_.Reset(this, g_main_loop_get_context(loop_));
  return true;
}

void Service::MountTaskObserve(const MountTaskResult& result) {
  // The event source will free this object
  event_source_.AddEvent(new MountTaskResult(result));
}

void Service::NotifyEvent(CryptohomeEventBase* event) {
  if (!strcmp(event->GetEventName(), kMountTaskResultEventType)) {
    MountTaskResult* result = static_cast<MountTaskResult*>(event);
    g_signal_emit(cryptohome_, async_complete_signal_, 0, result->sequence_id(),
                  result->return_status(), result->return_code());
    if (result->sequence_id() == async_mount_pkcs11_init_sequence_id_) {
      LOG(INFO) << "An asynchronous mount request with sequence id: "
                << async_mount_pkcs11_init_sequence_id_
                << " finished.";
      // We only report successful mounts.
      if (result->return_status() && !result->return_code()) {
        timer_collection_->UpdateTimer(TimerCollection::kAsyncMountTimer,
                                       false);
      }
      // Time to push the task for PKCS#11 initialization.
      InitializePkcs11();
    } else if (result->sequence_id() == async_guest_mount_sequence_id_) {
      if (result->return_status() && !result->return_code()) {
        timer_collection_->UpdateTimer(TimerCollection::kAsyncGuestMountTimer,
                                       false);
      }
    }
  } else if (!strcmp(event->GetEventName(), kTpmInitStatusEventType)) {
    TpmInitStatus* result = static_cast<TpmInitStatus*>(event);
    g_signal_emit(cryptohome_, tpm_init_signal_, 0, tpm_init_->IsTpmReady(),
                  tpm_init_->IsTpmEnabled(), result->get_took_ownership());
    // TODO(wad) should we package up a InstallAttributes status here too?
  } else if (!strcmp(event->GetEventName(), kPkcs11InitResultEventType)) {
    LOG(INFO) << "A Pkcs11_Init event got finished.";
    MountTaskResult* result = static_cast<MountTaskResult*>(event);
    if (result->return_status()) {
      timer_collection_->UpdateTimer(TimerCollection::kPkcs11InitTimer, false);
      LOG(INFO) << "PKCS#11 initialization succeeded.";
      pkcs11_state_ = kIsInitialized;
      return;
    }
    // We only report failures on the PKCS#11 initialization once per
    // initialization attempt, which is currently done once per login.
    if (!reported_pkcs11_init_fail_) {
      reported_pkcs11_init_fail_ =
          metrics_lib_.SendUserActionToUMA(kMetricNamePkcs11InitFail);
      LOG_IF(WARNING, !reported_pkcs11_init_fail_)
          << "Failed to report a failure on PKCS#11 initialization.";
    }
    LOG(ERROR) << "PKCS#11 initialization failed.";
    pkcs11_state_ = kIsFailed;
  }
}

void Service::InitializeTpmComplete(bool status, bool took_ownership) {
  if (took_ownership) {
    timer_collection_->UpdateTimer(TimerCollection::kTpmTakeOwnershipTimer,
                                   false);
    MountTaskResult ignored_result;
    base::WaitableEvent event(true, false);
    MountTaskResetTpmContext* mount_task =
        new MountTaskResetTpmContext(NULL, mount_);
    mount_task->set_result(&ignored_result);
    mount_task->set_complete_event(&event);
    mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
    event.Wait();
    // Check if we have a pending pkcs11 init task due to tpm ownership
    // not being done earlier. Trigger initialization if so.
    if (pkcs11_state_ == kIsWaitingOnTPM) {
      InitializePkcs11();
    }
    // Initialize the install-time locked attributes since we
    // can't do it prior to ownership.
    InitializeInstallAttributes(true);
  }
  // The event source will free this object
  TpmInitStatus* tpm_init_status = new TpmInitStatus();
  tpm_init_status->set_status(status);
  tpm_init_status->set_took_ownership(took_ownership);
  event_source_.AddEvent(tpm_init_status);
}

gboolean Service::CheckKey(gchar *userid,
                           gchar *key,
                           gboolean *OUT_result,
                           GError **error) {
  UsernamePasskey credentials(userid, SecureBlob(key, strlen(key)));

  MountTaskResult result;
  base::WaitableEvent event(true, false);
  MountTaskTestCredentials* mount_task =
      new MountTaskTestCredentials(NULL, mount_, credentials);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  event.Wait();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncCheckKey(gchar *userid,
                                gchar *key,
                                gint *OUT_async_id,
                                GError **error) {
  UsernamePasskey credentials(userid, SecureBlob(key, strlen(key)));

  // Freed by the message loop
  MountTaskTestCredentials* mount_task = new MountTaskTestCredentials(
      this,
      mount_,
      credentials);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  return TRUE;
}

gboolean Service::MigrateKey(gchar *userid,
                             gchar *from_key,
                             gchar *to_key,
                             gboolean *OUT_result,
                             GError **error) {
  UsernamePasskey credentials(userid, SecureBlob(to_key, strlen(to_key)));

  MountTaskResult result;
  base::WaitableEvent event(true, false);
  MountTaskMigratePasskey* mount_task =
      new MountTaskMigratePasskey(NULL, mount_, credentials, from_key);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  event.Wait();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncMigrateKey(gchar *userid,
                                  gchar *from_key,
                                  gchar *to_key,
                                  gint *OUT_async_id,
                                  GError **error) {
  UsernamePasskey credentials(userid, SecureBlob(to_key, strlen(to_key)));

  MountTaskMigratePasskey* mount_task =
      new MountTaskMigratePasskey(this, mount_, credentials, from_key);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  return TRUE;
}

gboolean Service::Remove(gchar *userid,
                         gboolean *OUT_result,
                         GError **error) {
  UsernamePasskey credentials(userid, chromeos::Blob());

  MountTaskResult result;
  base::WaitableEvent event(true, false);
  MountTaskRemove* mount_task =
      new MountTaskRemove(this, mount_, credentials);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  event.Wait();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncRemove(gchar *userid,
                              gint *OUT_async_id,
                              GError **error) {
  UsernamePasskey credentials(userid, chromeos::Blob());

  MountTaskRemove* mount_task =
      new MountTaskRemove(this, mount_, credentials);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  return TRUE;
}

gboolean Service::GetSystemSalt(GArray **OUT_salt, GError **error) {
  if (system_salt_.size() == 0) {
    mount_->GetSystemSalt(&system_salt_);
  }
  *OUT_salt = g_array_new(false, false, 1);
  g_array_append_vals(*OUT_salt, &system_salt_.front(), system_salt_.size());

  return TRUE;
}

gboolean Service::IsMounted(gboolean *OUT_is_mounted, GError **error) {
  *OUT_is_mounted = mount_->IsCryptohomeMounted();
  return TRUE;
}

gboolean Service::Mount(gchar *userid,
                        gchar *key,
                        gboolean create_if_missing,
                        gboolean deprecated_replace_tracked_subdirectories,
                        gchar** deprecated_tracked_subdirectories,
                        gint *OUT_error_code,
                        gboolean *OUT_result,
                        GError **error) {
  UsernamePasskey credentials(userid, SecureBlob(key, strlen(key)));

  if (mount_->IsCryptohomeMounted()) {
    if (mount_->IsCryptohomeMountedForUser(credentials)) {
      LOG(INFO) << "Cryptohome already mounted for this user";
      // This is the case where there were 2 mount requests for a given user
      // without any intervening unmount requests. This can happen, for example,
      // if cryptohomed was killed and restarted before an unmount request could
      // be received or processed.
      // As far as PKCS#11 initialization goes, we treat this as a brand new
      // mount request. InitializePkcs11() will detect and re-initialize if
      // necessary.
      InitializePkcs11();
      *OUT_error_code = Mount::MOUNT_ERROR_NONE;
      *OUT_result = TRUE;
      return TRUE;
    } else {
      if (!mount_->UnmountCryptohome()) {
        LOG(ERROR) << "Could not unmount cryptohome from previous user";
        *OUT_error_code = Mount::MOUNT_ERROR_MOUNT_POINT_BUSY;
        *OUT_result = FALSE;
        return TRUE;
      }
    }
  }

  // Any non-guest mount attempt triggers InstallAttributes finalization.
  // The return value is ignored as it is possible we're pre-ownership.
  // The next login will assure finalization if possible.
  if (install_attrs_->is_first_install())
    install_attrs_->Finalize();

  timer_collection_->UpdateTimer(TimerCollection::kSyncMountTimer, true);
  MountTaskResult result;
  base::WaitableEvent event(true, false);
  Mount::MountArgs mount_args;
  mount_args.create_if_missing = create_if_missing;
  MountTaskMount* mount_task = new MountTaskMount(NULL,
                                                  mount_,
                                                  credentials,
                                                  mount_args);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  event.Wait();
  // We only report successful mounts.
  if (result.return_status() && !result.return_code())
    timer_collection_->UpdateTimer(TimerCollection::kSyncMountTimer, false);

  pkcs11_state_ = kUninitialized;
  InitializePkcs11();

  *OUT_error_code = result.return_code();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncMount(gchar *userid,
                             gchar *key,
                             gboolean create_if_missing,
                             gboolean deprecated_replace_tracked_subdirectories,
                             gchar** deprecated_tracked_subdirectories,
                             gint *OUT_async_id,
                             GError **error) {
  UsernamePasskey credentials(userid, SecureBlob(key, strlen(key)));

  if (mount_->IsCryptohomeMounted()) {
    if (mount_->IsCryptohomeMountedForUser(credentials)) {
      LOG(INFO) << "Cryptohome already mounted for this user";
      MountTaskNop* mount_task = new MountTaskNop(this);
      mount_task->result()->set_return_code(Mount::MOUNT_ERROR_NONE);
      mount_task->result()->set_return_status(true);
      *OUT_async_id = mount_task->sequence_id();
      mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
      // See comment in Service::Mount() above on why this is needed here.
      InitializePkcs11();
      return TRUE;
    } else {
      if (!mount_->UnmountCryptohome()) {
        LOG(ERROR) << "Could not unmount cryptohome from previous user";
        MountTaskNop* mount_task = new MountTaskNop(this);
        mount_task->result()->set_return_code(
            Mount::MOUNT_ERROR_MOUNT_POINT_BUSY);
        mount_task->result()->set_return_status(false);
        *OUT_async_id = mount_task->sequence_id();
        mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
        return TRUE;
      }
    }
  }

  // See Mount for a relevant comment.
  if (install_attrs_->is_first_install())
    install_attrs_->Finalize();

  timer_collection_->UpdateTimer(TimerCollection::kAsyncMountTimer, true);
  Mount::MountArgs mount_args;
  mount_args.create_if_missing = create_if_missing;
  MountTaskMount* mount_task = new MountTaskMount(this,
                                                  mount_,
                                                  credentials,
                                                  mount_args);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);

  LOG(INFO) << "Asynced Mount() requested. Tracking request sequence id"
            << " for later PKCS#11 initialization.";;
  pkcs11_state_ = kUninitialized;
  async_mount_pkcs11_init_sequence_id_ = mount_task->sequence_id();
  return TRUE;
}

gboolean Service::MountGuest(gint *OUT_error_code,
                             gboolean *OUT_result,
                             GError **error) {
  if (mount_->IsCryptohomeMounted()) {
    if (!mount_->UnmountCryptohome()) {
      LOG(ERROR) << "Could not unmount cryptohome from previous user";
      *OUT_error_code = Mount::MOUNT_ERROR_MOUNT_POINT_BUSY;
      *OUT_result = FALSE;
      return TRUE;
    }
  }

  timer_collection_->UpdateTimer(TimerCollection::kSyncGuestMountTimer, true);
  MountTaskResult result;
  base::WaitableEvent event(true, false);
  MountTaskMountGuest* mount_task = new MountTaskMountGuest(NULL, mount_);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  event.Wait();
  // We only report successful mounts.
  if (result.return_status() && !result.return_code())
    timer_collection_->UpdateTimer(TimerCollection::kSyncGuestMountTimer,
                                   false);
  *OUT_error_code = result.return_code();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncMountGuest(gint *OUT_async_id,
                                  GError **error) {
  if (mount_->IsCryptohomeMounted()) {
    if (!mount_->UnmountCryptohome()) {
      LOG(ERROR) << "Could not unmount cryptohome from previous user";
      MountTaskNop* mount_task = new MountTaskNop(this);
      mount_task->result()->set_return_code(
          Mount::MOUNT_ERROR_MOUNT_POINT_BUSY);
      mount_task->result()->set_return_status(false);
      *OUT_async_id = mount_task->sequence_id();
      mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
      return TRUE;
    }
  }

  timer_collection_->UpdateTimer(TimerCollection::kAsyncGuestMountTimer, true);
  MountTaskMountGuest* mount_task = new MountTaskMountGuest(this, mount_);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  async_guest_mount_sequence_id_ = mount_task->sequence_id();
  return TRUE;
}

gboolean Service::Unmount(gboolean *OUT_result, GError **error) {
  if (mount_->IsCryptohomeMounted()) {
    *OUT_result = mount_->UnmountCryptohome();
  } else {
    *OUT_result = true;
  }
  if (pkcs11_state_ == kIsBeingInitialized) {
    // TODO(gauravsh): Need a better strategy on how to deal with an ongoing
    // initialization on the mount thread. Can we kill it?
    LOG(WARNING) << "Unmount request received while PKCS#11 init in progress";
  }
  // Reset PKCS#11 initialization state.
  pkcs11_state_ = kUninitialized;
  // And also reset its 'failure reported' state.
  reported_pkcs11_init_fail_ = false;
  return TRUE;
}

gboolean Service::RemoveTrackedSubdirectories(gboolean *OUT_result,
                                              GError **error) {
  MountTaskResult result;
  base::WaitableEvent event(true, false);
  MountTaskRemoveTrackedSubdirectories* mount_task =
      new MountTaskRemoveTrackedSubdirectories(this, mount_);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  event.Wait();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncRemoveTrackedSubdirectories(gint *OUT_async_id,
                                                   GError **error) {
  MountTaskRemoveTrackedSubdirectories* mount_task =
      new MountTaskRemoveTrackedSubdirectories(this, mount_);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  return TRUE;
}

gboolean Service::DoAutomaticFreeDiskSpaceControl(gboolean *OUT_result,
                                                  GError **error) {
  MountTaskResult result;
  base::WaitableEvent event(true, false);
  MountTaskAutomaticFreeDiskSpace* mount_task =
      new MountTaskAutomaticFreeDiskSpace(this, mount_);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  event.Wait();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncDoAutomaticFreeDiskSpaceControl(gint *OUT_async_id,
                                                       GError **error) {
  MountTaskAutomaticFreeDiskSpace* mount_task =
      new MountTaskAutomaticFreeDiskSpace(this, mount_);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  return TRUE;
}

gboolean Service::AsyncSetOwnerUser(gchar *user,
                                    gint *OUT_async_id,
                                    GError **error) {
  MountTaskSetOwnerUser* mount_task =
      new MountTaskSetOwnerUser(this, mount_, user);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  return TRUE;
}

gboolean Service::AsyncUpdateCurrentUserActivityTimestamp(gint time_shift_sec,
                                                          gint *OUT_async_id,
                                                          GError **error) {
  MountTaskUpdateCurrentUserActivityTimestamp* mount_task =
      new MountTaskUpdateCurrentUserActivityTimestamp(
          this, mount_, time_shift_sec);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE, mount_task);
  return TRUE;
}

gboolean Service::TpmIsReady(gboolean* OUT_ready, GError** error) {
  *OUT_ready = tpm_init_->IsTpmReady();
  return TRUE;
}

gboolean Service::TpmIsEnabled(gboolean* OUT_enabled, GError** error) {
  *OUT_enabled = tpm_init_->IsTpmEnabled();
  return TRUE;
}

gboolean Service::TpmGetPassword(gchar** OUT_password, GError** error) {
  SecureBlob password;
  if (!tpm_init_->GetTpmPassword(&password)) {
    *OUT_password = NULL;
    return TRUE;
  }
  *OUT_password = g_strndup(static_cast<char*>(password.data()),
                            password.size());
  return TRUE;
}

gboolean Service::TpmIsOwned(gboolean* OUT_owned, GError** error) {
  *OUT_owned = tpm_init_->IsTpmOwned();
  return TRUE;
}

gboolean Service::TpmIsBeingOwned(gboolean* OUT_owning, GError** error) {
  *OUT_owning = tpm_init_->IsTpmBeingOwned();
  return TRUE;
}

gboolean Service::TpmCanAttemptOwnership(GError** error) {
  if (!tpm_init_->HasInitializeBeenCalled()) {
    timer_collection_->UpdateTimer(TimerCollection::kTpmTakeOwnershipTimer,
                                   true);
    tpm_init_->StartInitializeTpm();
  }
  return TRUE;
}

gboolean Service::TpmClearStoredPassword(GError** error) {
  tpm_init_->ClearStoredTpmPassword();
  return TRUE;
}

gboolean Service::Pkcs11IsTpmTokenReady(gboolean* OUT_ready, GError** error) {
  // TODO(gauravsh): Give out more information here. The state of PKCS#11
  // initialization, and it it failed - the reason for that.
  *OUT_ready = (pkcs11_state_ == kIsInitialized);
  return TRUE;
}

gboolean Service::Pkcs11GetTpmTokenInfo(gchar** OUT_label,
                                        gchar** OUT_user_pin,
                                        GError** error) {
  pkcs11_init_->GetTpmTokenInfo(OUT_label,
                                OUT_user_pin);
  return TRUE;
}

gboolean Service::InstallAttributesGet(gchar* name,
                                       GArray** OUT_value,
                                       gboolean* OUT_successful,
                                       GError** error) {
  chromeos::Blob value;
  *OUT_successful = install_attrs_->Get(name, &value);
  // TODO(wad) can g_array_new return NULL.
  *OUT_value = g_array_new(false, false, sizeof(char));
  if (*OUT_successful)
    g_array_append_vals(*OUT_value, &value.front(), value.size());
  return TRUE;
}

gboolean Service::InstallAttributesSet(gchar* name,
                                       GArray* value,
                                       gboolean* OUT_successful,
                                       GError** error) {
  // Convert from GArray to vector
  chromeos::Blob value_blob;
  value_blob.assign(value->data, value->data + value->len);
  *OUT_successful = install_attrs_->Set(name, value_blob);
  return TRUE;
}

gboolean Service::InstallAttributesFinalize(gboolean* OUT_finalized,
                                            GError** error) {
  *OUT_finalized = install_attrs_->Finalize();
  return TRUE;
}

gboolean Service::InstallAttributesCount(gint* OUT_count, GError** error) {
  // TODO(wad) for all of these functions return error on uninit.
  // Follow the CHROMEOS_LOGIN_ERROR quark example in chromeos/dbus/
  *OUT_count = install_attrs_->Count();
  return TRUE;
}

gboolean Service::InstallAttributesIsReady(gboolean* OUT_ready,
                                           GError** error) {
  *OUT_ready = (install_attrs_->IsReady() == true);
  return TRUE;
}

gboolean Service::InstallAttributesIsSecure(gboolean* OUT_is_secure,
                                            GError** error) {
  *OUT_is_secure = (install_attrs_->is_secure() == true);
  return TRUE;
}

gboolean Service::InstallAttributesIsInvalid(gboolean* OUT_is_invalid,
                                             GError** error) {
  // Is true after a failed init or prior to Init().
  *OUT_is_invalid = (install_attrs_->is_invalid() == true);
  return TRUE;
}

gboolean Service::InstallAttributesIsFirstInstall(
    gboolean* OUT_is_first_install,
    GError** error) {
  *OUT_is_first_install = (install_attrs_->is_first_install() == true);
  return TRUE;
}

gboolean Service::GetStatusString(gchar** OUT_status, GError** error) {
  Tpm::TpmStatusInfo tpm_status;
  mount_->get_crypto()->EnsureTpm(false);
  Tpm* tpm = const_cast<Tpm*>(mount_->get_crypto()->get_tpm());

  if (tpm) {
    tpm->GetStatus(true, &tpm_status);
  } else {
    Tpm::GetSingleton()->GetStatus(true, &tpm_status);
  }

  if (tpm_init_) {
    tpm_status.Enabled = tpm_init_->IsTpmEnabled();
    tpm_status.BeingOwned = tpm_init_->IsTpmBeingOwned();
    tpm_status.Owned = tpm_init_->IsTpmOwned();
  }

  std::string user_data;
  UserSession* session = mount_->get_current_user();
  if (session) {
    do {
      std::string obfuscated_user;
      session->GetObfuscatedUsername(&obfuscated_user);
      if (!obfuscated_user.length()) {
        break;
      }
      std::string vault_path = StringPrintf("%s/%s/master.0",
                                            mount_->get_shadow_root().c_str(),
                                            obfuscated_user.c_str());
      FilePath vault_file(vault_path);
      base::PlatformFileInfo file_info;
      if(!file_util::GetFileInfo(vault_file, &file_info)) {
        break;
      }
      SecureBlob contents;
      if (!Mount::LoadFileBytes(vault_file, &contents)) {
        break;
      }
      cryptohome::SerializedVaultKeyset serialized;
      if (!serialized.ParseFromArray(
          static_cast<const unsigned char*>(&contents[0]), contents.size())) {
        break;
      }
      base::Time::Exploded exploded;
      file_info.last_modified.UTCExplode(&exploded);
      user_data = StringPrintf(
          "User Session:\n"
          "  Keyset Was TPM Wrapped..........: %s\n"
          "  Keyset Was Scrypt Wrapped.......: %s\n"
          "  Keyset Last Modified............: %02d-%02d-%04d %02d:%02d:%02d"
          " (UTC)\n",
          ((serialized.flags() &
            cryptohome::SerializedVaultKeyset::TPM_WRAPPED) ? "1" : "0"),
          ((serialized.flags() &
            cryptohome::SerializedVaultKeyset::SCRYPT_WRAPPED) ? "1" : "0"),
          exploded.month, exploded.day_of_month, exploded.year,
          exploded.hour, exploded.minute, exploded.second);

    } while(false);
  }
  int install_attrs_size = install_attrs_->Count();
  std::string install_attrs_data("InstallAttributes Contents:\n");
  if (install_attrs_->Count()) {
    std::string name;
    chromeos::Blob value;
    for (int pair = 0; pair < install_attrs_size; ++pair) {
      install_attrs_data.append(StringPrintf(
        "  Index...........................: %d\n", pair));
      if (install_attrs_->GetByIndex(pair, &name, &value)) {
        std::string value_str(reinterpret_cast<const char*>(&value[0]),
                              value.size());
        install_attrs_data.append(StringPrintf(
          "  Name............................: %s\n"
          "  Value...........................: %s\n",
          name.c_str(),
          value_str.c_str()));
      }
    }
  }

  *OUT_status = g_strdup_printf(
      "TPM Status:\n"
      "  Enabled.........................: %s\n"
      "  Owned...........................: %s\n"
      "  Being Owned.....................: %s\n"
      "  Can Connect.....................: %s\n"
      "  Can Load SRK....................: %s\n"
      "  Can Load SRK Public.............: %s\n"
      "  Has Cryptohome Key..............: %s\n"
      "  Can Encrypt.....................: %s\n"
      "  Can Decrypt.....................: %s\n"
      "  Instance Context................: %s\n"
      "  Instance Key Handle.............: %s\n"
      "  Last Error......................: %08x\n"
      "%s"
      "Mount Status:\n"
      "  Vault Is Mounted................: %s\n"
      "  Owner User......................: %s\n"
      "  Enterprise Owned................: %s\n"
      "InstallAttributes Status:\n"
      "  Initialized.....................: %s\n"
      "  Version.........................: %"PRIx64"\n"
      "  Lockbox Index...................: 0x%x\n"
      "  Secure..........................: %s\n"
      "  Invalid.........................: %s\n"
      "  First Install / Unlocked........: %s\n"
      "  Entries.........................: %d\n"
      "%s"
      "PKCS#11 Init State................: %d\n",
      (tpm_status.Enabled ? "1" : "0"),
      (tpm_status.Owned ? "1" : "0"),
      (tpm_status.BeingOwned ? "1" : "0"),
      (tpm_status.CanConnect ? "1" : "0"),
      (tpm_status.CanLoadSrk ? "1" : "0"),
      (tpm_status.CanLoadSrkPublicKey ? "1" : "0"),
      (tpm_status.HasCryptohomeKey ? "1" : "0"),
      (tpm_status.CanEncrypt ? "1" : "0"),
      (tpm_status.CanDecrypt ? "1" : "0"),
      (tpm_status.ThisInstanceHasContext ? "1" : "0"),
      (tpm_status.ThisInstanceHasKeyHandle ? "1" : "0"),
      tpm_status.LastTpmError,
      user_data.c_str(),
      (mount_->IsCryptohomeMounted() ? "1" : "0"),
      mount_->owner_obfuscated_username().c_str(),
      (mount_->enterprise_owned() ? "1" : "0"),
      (install_attrs_->is_initialized() ? "1" : "0"),
      install_attrs_->version(),
      InstallAttributes::kLockboxIndex,
      (install_attrs_->is_secure() ? "1" : "0"),
      (install_attrs_->is_invalid() ? "1" : "0"),
      (install_attrs_->is_first_install() ? "1" : "0"),
      install_attrs_size,
      install_attrs_data.c_str(),
      pkcs11_state_);
  return TRUE;
}

// Called on Mount thread.
void Service::AutoCleanupCallback() {
  static int ticks;

  // Update current user's activity timestamp every day.
  if (++ticks > update_user_activity_period_) {
    mount_->UpdateCurrentUserActivityTimestamp(0);
    ticks = 0;
  }

  mount_->DoAutomaticFreeDiskSpaceControl();

  // Schedule our next call. If the thread is terminating, we would
  // not be called.
  mount_thread_.message_loop()->PostDelayedTask(
      FROM_HERE, NewRunnableMethod(this, &Service::AutoCleanupCallback),
      auto_cleanup_period_);
}

void Service::DetectEnterpriseOwnership() const {
  static const char true_str[] = "true";
  const chromeos::Blob true_value(true_str, true_str + arraysize(true_str));
  chromeos::Blob value;
  if (install_attrs_->Get("enterprise.owned", &value) &&
      value == true_value) {
    mount_->set_enterprise_owned(true);
  }
}

}  // namespace cryptohome

// We do not want AutoCleanupCallback() to refer the class and make it
// wait for its execution.  If Mount thread terminates, it will delete
// our pending task or wait for it to finish.
DISABLE_RUNNABLE_METHOD_REFCOUNT(cryptohome::Service);
