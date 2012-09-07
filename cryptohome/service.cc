// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "service.h"

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <base/bind.h>
#include <base/callback.h>
#include <base/file_util.h>
#include <base/json/json_writer.h>
#include <base/logging.h>
#include <base/platform_file.h>
#include <base/memory/scoped_ptr.h>
#include <base/string_util.h>
#include <base/time.h>
#include <base/values.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/secure_blob.h>
#include <metrics/metrics_library.h>
#include <metrics/timer.h>
#include <string>
#include <vector>

#include "cryptohome_event_source.h"
#include "crypto.h"
#include "install_attributes.h"
#include "interface.h"
#include "marshal.glibmarshal.h"
#include "mount.h"
#include "platform.h"
#include "stateful_recovery.h"
#include "tpm.h"
#include "username_passkey.h"
#include "vault_keyset.pb.h"

using chromeos::SecureBlob;

// Forcibly namespace the dbus-bindings generated server bindings instead of
// modifying the files afterward.
namespace cryptohome {
namespace gobject {
#include "bindings/server.h"
}  // namespace gobject
}  // namespace cryptohome

namespace cryptohome {

const char *kSaltFilePath = "/home/.shadow/salt";
const char *kDefaultMountName = "";

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
    kNumTimerTypes  // For the number of timer types.
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
  {"Cryptohome.TimeToMountAsync", 0, 4000, 50},
  {"Cryptohome.TimeToMountSync", 0, 4000, 50},
  {"Cryptohome.TimeToMountGuestAsync", 0, 4000, 50},
  {"Cryptohome.TimeToMountGuestSync", 0, 4000, 50},
  {"Cryptohome.TimeToTakeTpmOwnership", 0, 100000, 50},
  {"Cryptohome.TimeToInitPkcs11", 1000, 100000, 50}
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

  virtual const char* GetEventName() const {
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

// Bridges between the MountTaskObserver callback model and the
// CryptohomeEventSource callback model. This class forwards MountTaskObserver
// events to a CryptohomeEventSource. An instance of this class is single-use
// (i.e., will be freed after it has observed one event).
class MountTaskObserverBridge : public MountTaskObserver {
 public:
  explicit MountTaskObserverBridge(cryptohome::Mount* mount,
                                   CryptohomeEventSource* source)
    : mount_(mount), source_(source) { }
  virtual ~MountTaskObserverBridge() { }
  virtual bool MountTaskObserve(const MountTaskResult& result) {
    MountTaskResult *r = new MountTaskResult(result);
    r->set_mount(mount_);
    source_->AddEvent(r);
    return true;
  }

 protected:
  cryptohome::Mount* mount_;
  CryptohomeEventSource* source_;
};

Service::Service()
    : use_tpm_(true),
      loop_(NULL),
      cryptohome_(NULL),
      system_salt_(),
      crypto_(new Crypto()),
      default_platform_(new Platform()),
      platform_(default_platform_.get()),
      tpm_(Tpm::GetSingleton()),
      default_tpm_init_(new TpmInit(platform_)),
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
      timer_collection_(new TimerCollection()),
      reported_pkcs11_init_fail_(false),
      default_homedirs_(new cryptohome::HomeDirs()),
      homedirs_(default_homedirs_.get()) {
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

  crypto_->set_use_tpm(use_tpm_);
  if (!crypto_->Init(platform_))
    return false;

  homedirs_->set_crypto(crypto_);
  if (!homedirs_->Init())
    return false;

  mount_ = GetMountForUser(kDefaultMountName);
  if (!mount_)
    mount_ = CreateMountForUser(kDefaultMountName);
  mount_->Init();
  // If the TPM is unowned or doesn't exist, it's safe for
  // this function to be called again. However, it shouldn't
  // be called across multiple threads in parallel.
  InitializeInstallAttributes(false);

  // TODO(wad) Determine if this should only be called if
  //           tpm->IsEnabled() is true.
  if (tpm_ && initialize_tpm_) {
    tpm_init_->set_tpm(tpm_);
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

  // Start scheduling periodic cleanup events. Subsequent events are scheduled
  // by the callback itself.
  mount_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&Service::AutoCleanupCallback, base::Unretained(this)));

  StatefulRecovery recovery(platform_);
  if (recovery.Requested()) {
    if (recovery.Recover())
      LOG(INFO) << "A stateful recovery was performed successfully.";
    recovery.PerformReboot();
  }

  return result;
}

void Service::InitializeInstallAttributes(bool first_time) {
  // Wait for ownership if there is a working TPM.
  if (tpm_ && tpm_->IsEnabled() && !tpm_->IsOwned())
    return;

  // The TPM owning instance may have changed since initialization.
  // InstallAttributes can handle a NULL or !IsEnabled Tpm object.
  install_attrs_->SetTpm(tpm_);

  if (first_time)
    // TODO(wad) Go nuclear if PrepareSystem fails!
    install_attrs_->PrepareSystem();

  // Init can fail without making the interface inconsistent so we're okay here.
  install_attrs_->Init();

  // Check if the machine is enterprise owned and report to mount_ then.
  DetectEnterpriseOwnership();
}

void Service::InitializePkcs11(cryptohome::Mount* mount) {
  // Wait for ownership if there is a working TPM.
  if (tpm_ && tpm_->IsEnabled() && !tpm_->IsOwned()) {
    LOG(WARNING) << "TPM was not owned. TPM initialization call back will"
                 << " handle PKCS#11 initialization.";
    mount->set_pkcs11_state(cryptohome::Mount::kIsWaitingOnTPM);
    return;
  }

  // Ok, so the TPM is owned. Time to request asynchronous initialization of
  // PKCS#11.
  // Make sure cryptohome is mounted, otherwise all of this is for naught.
  CHECK(mount);
  if (!mount->IsCryptohomeMounted()) {
    LOG(WARNING) << "PKCS#11 initialization requested but cryptohome is"
                 << " not mounted.";
    return;
  }

  // Reset PKCS#11 initialization status. A successful completion of
  // MountTaskPkcs11_Init would set it in the service thread via NotifyEvent().
  timer_collection_->UpdateTimer(TimerCollection::kPkcs11InitTimer, true);
  mount->set_pkcs11_state(cryptohome::Mount::kIsBeingInitialized);
  MountTaskObserverBridge* bridge =
      new MountTaskObserverBridge(mount, &event_source_);
  scoped_refptr<MountTaskPkcs11Init> pkcs11_init_task =
      new MountTaskPkcs11Init(bridge, mount);
  LOG(INFO) << "Putting a Pkcs11_Initialize on the mount thread.";
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskPkcs11Init::Run, pkcs11_init_task.get()));
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

void Service::NotifyEvent(CryptohomeEventBase* event) {
  if (!strcmp(event->GetEventName(), kMountTaskResultEventType)) {
    MountTaskResult* result = static_cast<MountTaskResult*>(event);
    g_signal_emit(cryptohome_, async_complete_signal_, 0, result->sequence_id(),
                  result->return_status(), result->return_code());
    if (result->pkcs11_init()) {
      LOG(INFO) << "An asynchronous mount request with sequence id: "
                << result->sequence_id()
                << " finished; doing PKCS11 init...";
      // We only report successful mounts.
      if (result->return_status() && !result->return_code()) {
        timer_collection_->UpdateTimer(TimerCollection::kAsyncMountTimer,
                                       false);
      }
      // Time to push the task for PKCS#11 initialization.
      InitializePkcs11(result->mount());
    } else if (result->guest()) {
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
      result->mount()->set_pkcs11_state(cryptohome::Mount::kIsInitialized);
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
    result->mount()->set_pkcs11_state(cryptohome::Mount::kIsFailed);
  }
}

void Service::InitializeTpmComplete(bool status, bool took_ownership) {
  if (took_ownership) {
    gboolean mounted = FALSE;
    timer_collection_->UpdateTimer(TimerCollection::kTpmTakeOwnershipTimer,
                                   false);
    // When TPM initialization finishes, we need to tell every Mount to
    // reinitialize its TPM context, since the TPM is now useable, and we might
    // need to kick off their PKCS11 initialization if they were blocked before.
    for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); ++it) {
      cryptohome::Mount* mount = it->second;
      MountTaskResult ignored_result;
      base::WaitableEvent event(true, false);
      scoped_refptr<MountTaskResetTpmContext> mount_task =
          new MountTaskResetTpmContext(NULL, mount);
      mount_task->set_result(&ignored_result);
      mount_task->set_complete_event(&event);
      mount_thread_.message_loop()->PostTask(FROM_HERE,
          base::Bind(&MountTaskResetTpmContext::Run, mount_task.get()));
      event.Wait();
      // Check if we have a pending pkcs11 init task due to tpm ownership
      // not being done earlier. Trigger initialization if so.
      if (mount->pkcs11_state() == cryptohome::Mount::kIsWaitingOnTPM)
        InitializePkcs11(mount);
    }
    // Initialize the install-time locked attributes since we
    // can't do it prior to ownership.
    InitializeInstallAttributes(true);
    // If we mounted before the TPM finished initialization, we must
    // finalize the install attributes now too, otherwise it takes a
    // full re-login cycle to finalize.
    if (IsMounted(&mounted, NULL) && mounted &&
        install_attrs_->is_first_install())
      install_attrs_->Finalize();
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
  for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); ++it) {
    if (it->second->AreSameUser(credentials)) {
      *OUT_result = it->second->AreValid(credentials);
      return TRUE;
    }
  }

  MountTaskResult result;
  base::WaitableEvent event(true, false);
  scoped_refptr<MountTaskTestCredentials> mount_task =
      new MountTaskTestCredentials(NULL, NULL, homedirs_, credentials);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskTestCredentials::Run, mount_task.get()));
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
  MountTaskObserverBridge* bridge =
      new MountTaskObserverBridge(NULL, &event_source_);
  for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); ++it) {
    // Fast path - because we can check credentials on a Mount very fast, we can
    // afford to check them synchronously here and post the result
    // asynchronously.
    if (it->second->AreSameUser(credentials)) {
      bool ok = it->second->AreValid(credentials);
      scoped_refptr<MountTaskNop> mount_task = new MountTaskNop(bridge);
      mount_task->result()->set_return_code(MOUNT_ERROR_NONE);
      mount_task->result()->set_return_status(ok);
      *OUT_async_id = mount_task->sequence_id();
      mount_thread_.message_loop()->PostTask(FROM_HERE,
          base::Bind(&MountTaskNop::Run, mount_task.get()));
      return TRUE;
    }
  }

  // Slow path - ask the HomeDirs to check credentials.
  scoped_refptr<MountTaskTestCredentials> mount_task
      = new MountTaskTestCredentials(bridge, NULL, homedirs_, credentials);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskTestCredentials::Run, mount_task.get()));
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
  scoped_refptr<MountTaskMigratePasskey> mount_task =
      new MountTaskMigratePasskey(NULL, homedirs_, credentials, from_key);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskMigratePasskey::Run, mount_task.get()));
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

  MountTaskObserverBridge* bridge =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<MountTaskMigratePasskey> mount_task =
      new MountTaskMigratePasskey(bridge, homedirs_, credentials, from_key);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskMigratePasskey::Run, mount_task.get()));
  return TRUE;
}

gboolean Service::Remove(gchar *userid,
                         gboolean *OUT_result,
                         GError **error) {
  UsernamePasskey credentials(userid, chromeos::Blob());
  if (mount_->IsCryptohomeMountedForUser(credentials)) {
    *OUT_result = FALSE;
    return TRUE;
  }

  MountTaskResult result;
  base::WaitableEvent event(true, false);
  MountTaskObserverBridge* bridge =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<MountTaskRemove> mount_task =
      new MountTaskRemove(bridge, NULL, credentials, homedirs_);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskRemove::Run, mount_task.get()));
  event.Wait();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncRemove(gchar *userid,
                              gint *OUT_async_id,
                              GError **error) {
  UsernamePasskey credentials(userid, chromeos::Blob());
  MountTaskObserverBridge* bridge =
      new MountTaskObserverBridge(NULL, &event_source_);
  if (mount_->IsCryptohomeMountedForUser(credentials)) {
    scoped_refptr<MountTaskNop> mount_task = new MountTaskNop(bridge);
    mount_task->result()->set_return_status(false);
    *OUT_async_id = mount_task->sequence_id();
    mount_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(&MountTaskNop::Run, mount_task.get()));
  } else {
    scoped_refptr<MountTaskRemove> mount_task =
        new MountTaskRemove(bridge, NULL, credentials, homedirs_);
    *OUT_async_id = mount_task->sequence_id();
    mount_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(&MountTaskRemove::Run, mount_task.get()));
  }
  return TRUE;
}

gboolean Service::GetSystemSalt(GArray **OUT_salt, GError **error) {
  if (!CreateSystemSaltIfNeeded())
    return FALSE;
  *OUT_salt = g_array_new(false, false, 1);
  g_array_append_vals(*OUT_salt, &system_salt_.front(), system_salt_.size());
  return TRUE;
}

gboolean Service::IsMounted(gboolean *OUT_is_mounted, GError **error) {
  // We consider "the cryptohome" to be mounted if any existing cryptohome is
  // mounted.
  *OUT_is_mounted = FALSE;
  for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); ++it)
    *OUT_is_mounted = *OUT_is_mounted || it->second->IsCryptohomeMounted();
  return TRUE;
}

gboolean Service::IsMountedForUser(gchar *userid,
                                   gboolean *OUT_is_mounted,
                                   gboolean *OUT_is_ephemeral_mount,
                                   GError **error) {
  UsernamePasskey credentials(userid, SecureBlob("", 0));
  if (mount_->IsVaultMountedForUser(credentials)) {
    *OUT_is_mounted = true;
    *OUT_is_ephemeral_mount = false;
  } else if (mount_->IsCryptohomeMountedForUser(credentials)) {
    *OUT_is_mounted = true;
    *OUT_is_ephemeral_mount = true;
  } else {
    *OUT_is_mounted = false;
    *OUT_is_ephemeral_mount = false;
  }
  return TRUE;
}

gboolean Service::Mount(const gchar *userid,
                        const gchar *key,
                        gboolean create_if_missing,
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
      InitializePkcs11(mount_);
      *OUT_error_code = MOUNT_ERROR_NONE;
      *OUT_result = TRUE;
      return TRUE;
    } else {
      if (!mount_->UnmountCryptohome()) {
        LOG(ERROR) << "Could not unmount cryptohome from previous user";
        *OUT_error_code = MOUNT_ERROR_MOUNT_POINT_BUSY;
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
  scoped_refptr<MountTaskMount> mount_task = new MountTaskMount(NULL,
                                                                mount_,
                                                                credentials,
                                                                mount_args);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskMount::Run, mount_task.get()));
  event.Wait();
  // We only report successful mounts.
  if (result.return_status() && !result.return_code())
    timer_collection_->UpdateTimer(TimerCollection::kSyncMountTimer, false);

  mount_->set_pkcs11_state(cryptohome::Mount::kUninitialized);
  InitializePkcs11(mount_);

  *OUT_error_code = result.return_code();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncMount(const gchar *userid,
                             const gchar *key,
                             gboolean create_if_missing,
                             gint *OUT_async_id,
                             GError **error) {
  UsernamePasskey credentials(userid, SecureBlob(key, strlen(key)));

  if (mount_->IsCryptohomeMounted()) {
    if (mount_->IsCryptohomeMountedForUser(credentials)) {
      LOG(INFO) << "Cryptohome already mounted for this user";
      MountTaskObserverBridge* bridge =
          new MountTaskObserverBridge(mount_, &event_source_);
      scoped_refptr<MountTaskNop> mount_task = new MountTaskNop(bridge);
      mount_task->result()->set_return_code(MOUNT_ERROR_NONE);
      mount_task->result()->set_return_status(true);
      *OUT_async_id = mount_task->sequence_id();
      mount_thread_.message_loop()->PostTask(FROM_HERE,
          base::Bind(&MountTaskNop::Run, mount_task.get()));
      // See comment in Service::Mount() above on why this is needed here.
      InitializePkcs11(mount_);
      return TRUE;
    } else {
      if (!mount_->UnmountCryptohome()) {
        LOG(ERROR) << "Could not unmount cryptohome from previous user";
        MountTaskObserverBridge* bridge =
            new MountTaskObserverBridge(mount_, &event_source_);
        scoped_refptr<MountTaskNop> mount_task = new MountTaskNop(bridge);
        mount_task->result()->set_return_code(
            MOUNT_ERROR_MOUNT_POINT_BUSY);
        mount_task->result()->set_return_status(false);
        *OUT_async_id = mount_task->sequence_id();
        mount_thread_.message_loop()->PostTask(FROM_HERE,
            base::Bind(&MountTaskNop::Run, mount_task.get()));
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
  MountTaskObserverBridge *bridge =
      new MountTaskObserverBridge(mount_, &event_source_);
  scoped_refptr<MountTaskMount> mount_task = new MountTaskMount(bridge,
                                                                mount_,
                                                                credentials,
                                                                mount_args);
  mount_task->result()->set_pkcs11_init(true);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskMount::Run, mount_task.get()));

  LOG(INFO) << "Asynced Mount() requested. Tracking request sequence id"
            << " for later PKCS#11 initialization.";
  mount_->set_pkcs11_state(cryptohome::Mount::kUninitialized);
  return TRUE;
}

gboolean Service::MountGuest(gint *OUT_error_code,
                             gboolean *OUT_result,
                             GError **error) {
  if (mount_->IsCryptohomeMounted()) {
    if (!mount_->UnmountCryptohome()) {
      LOG(ERROR) << "Could not unmount cryptohome from previous user";
      *OUT_error_code = MOUNT_ERROR_MOUNT_POINT_BUSY;
      *OUT_result = FALSE;
      return TRUE;
    }
  }

  timer_collection_->UpdateTimer(TimerCollection::kSyncGuestMountTimer, true);
  MountTaskResult result;
  base::WaitableEvent event(true, false);
  scoped_refptr<MountTaskMountGuest> mount_task
      = new MountTaskMountGuest(NULL, mount_);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskMountGuest::Run, mount_task.get()));
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
      MountTaskObserverBridge* bridge =
          new MountTaskObserverBridge(mount_, &event_source_);
      scoped_refptr<MountTaskNop> mount_task = new MountTaskNop(bridge);
      mount_task->result()->set_return_code(
          MOUNT_ERROR_MOUNT_POINT_BUSY);
      mount_task->result()->set_return_status(false);
      *OUT_async_id = mount_task->sequence_id();
      mount_thread_.message_loop()->PostTask(FROM_HERE,
          base::Bind(&MountTaskNop::Run, mount_task.get()));
      return TRUE;
    }
  }

  timer_collection_->UpdateTimer(TimerCollection::kAsyncGuestMountTimer, true);
  MountTaskObserverBridge* bridge =
      new MountTaskObserverBridge(mount_, &event_source_);
  scoped_refptr<MountTaskMountGuest> mount_task
      = new MountTaskMountGuest(bridge, mount_);
  mount_task->result()->set_guest(true);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskMountGuest::Run, mount_task.get()));
  return TRUE;
}

// Unmount all mounted cryptohomes.
gboolean Service::Unmount(gboolean *OUT_result, GError **error) {
  *OUT_result = TRUE;
  for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); ++it) {
    cryptohome::Mount* mount = it->second;
    bool ok = !mount->IsCryptohomeMounted() || mount->UnmountCryptohome();
    // Consider it a failure if any mount fails to unmount.
    *OUT_result = *OUT_result && ok;
    if (mount->pkcs11_state() == cryptohome::Mount::kIsBeingInitialized) {
      // TODO(gauravsh): Need a better strategy on how to deal with an ongoing
      // initialization on the mount thread. Can we kill it?
      LOG(WARNING) << "Unmount request received while PKCS#11 init in progress";
    }
    // Reset PKCS#11 initialization state.
    mount->set_pkcs11_state(cryptohome::Mount::kUninitialized);
    // And also reset its 'failure reported' state.
    reported_pkcs11_init_fail_ = false;
  }
  return TRUE;
}

gboolean Service::UnmountForUser(gchar *userid, gboolean *OUT_result,
                                 GError **error) {
  // TODO(ellyjones): per-user unmount support.
  return Unmount(OUT_result, error);
}

gboolean Service::DoAutomaticFreeDiskSpaceControl(gboolean *OUT_result,
                                                  GError **error) {
  MountTaskResult result;
  base::WaitableEvent event(true, false);
  MountTaskObserverBridge* bridge =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<MountTaskAutomaticFreeDiskSpace> mount_task =
      new MountTaskAutomaticFreeDiskSpace(bridge, mount_);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskAutomaticFreeDiskSpace::Run, mount_task.get()));
  event.Wait();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncDoAutomaticFreeDiskSpaceControl(gint *OUT_async_id,
                                                       GError **error) {
  MountTaskObserverBridge* bridge =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<MountTaskAutomaticFreeDiskSpace> mount_task =
      new MountTaskAutomaticFreeDiskSpace(bridge, mount_);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskAutomaticFreeDiskSpace::Run, mount_task.get()));
  return TRUE;
}

gboolean Service::UpdateCurrentUserActivityTimestamp(gint time_shift_sec,
                                                     GError **error) {
  for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); ++it)
    it->second->UpdateCurrentUserActivityTimestamp(time_shift_sec);
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

gboolean Service::TpmIsAttestationPrepared(gboolean* OUT_prepared,
                                           GError** error) {
  *OUT_prepared = tpm_init_->IsAttestationPrepared();
  return TRUE;
}

gboolean Service::TpmVerifyAttestationData(gboolean* OUT_verified,
                                           GError** error) {
  *OUT_verified = tpm_init_->VerifyAttestationData();
  return TRUE;
}

gboolean Service::TpmVerifyEK(gboolean* OUT_verified, GError** error) {
  *OUT_verified = tpm_init_->VerifyEK();
  return TRUE;
}

// Returns true if all Pkcs11 tokens are ready.
gboolean Service::Pkcs11IsTpmTokenReady(gboolean* OUT_ready, GError** error) {
  // TODO(gauravsh): Give out more information here. The state of PKCS#11
  // initialization, and it it failed - the reason for that.
  *OUT_ready = TRUE;
  for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); ++it) {
    cryptohome::Mount* mount = it->second;
    bool ok =  (mount->pkcs11_state() == cryptohome::Mount::kIsInitialized);
    *OUT_ready = *OUT_ready && ok;
  }
  return TRUE;
}

gboolean Service::Pkcs11IsTpmTokenReadyForUser(gchar* username,
                                               gboolean* OUT_ready,
                                               GError** error) {
  // TODO(ellyjones): make this really check per user. crosbug.com/22127
  return Pkcs11IsTpmTokenReady(OUT_ready, error);
}

gboolean Service::Pkcs11GetTpmTokenInfo(gchar** OUT_label,
                                        gchar** OUT_user_pin,
                                        GError** error) {
  pkcs11_init_->GetTpmTokenInfo(OUT_label,
                                OUT_user_pin);
  return TRUE;
}

gboolean Service::Pkcs11GetTpmTokenInfoForUser(gchar* username,
                                               gchar** OUT_label,
                                               gchar** OUT_user_pin,
                                               GError** error) {
  pkcs11_init_->GetTpmTokenInfoForUser(username, OUT_label, OUT_user_pin);
  return TRUE;
}

gboolean Service::Pkcs11Terminate(gchar* username, GError **error) {
  for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); ++it) {
    // TODO(dkrahn): Make this user-specific.
    it->second->RemovePkcs11Token();
  }
  return TRUE;
}

gboolean Service::InstallAttributesGet(gchar* name,
                                       GArray** OUT_value,
                                       gboolean* OUT_successful,
                                       GError** error) {
  chromeos::Blob value;
  *OUT_successful = install_attrs_->Get(name, &value);
  // TODO(wad) can g_array_new return NULL.
  *OUT_value = g_array_new(false, false, sizeof(value.front()));
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

  // Check if the machine is enterprise owned and report this to mount_.
  DetectEnterpriseOwnership();

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
  DictionaryValue dv;
  ListValue* mounts = new ListValue();
  for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); it++)
    mounts->Append(it->second->GetStatus());
  Value* attrs = install_attrs_->GetStatus();
  Value* tpm = tpm_->GetStatusValue(tpm_init_);
  dv.Set("mounts", mounts);
  dv.Set("installattrs", attrs);
  dv.Set("tpm", tpm);
  std::string json;
  base::JSONWriter::Write(&dv, true, &json);
  *OUT_status = g_strdup(json.c_str());
  return TRUE;
}

// Called on Mount thread.
void Service::AutoCleanupCallback() {
  static int ticks;

  // Update current user's activity timestamp every day.
  if (++ticks > update_user_activity_period_) {
    for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); ++it)
      it->second->UpdateCurrentUserActivityTimestamp(0);
    ticks = 0;
  }

  homedirs_->FreeDiskSpace();

  // Schedule our next call. If the thread is terminating, we would
  // not be called. We use base::Unretained here because the Service object is
  // never destroyed.
  mount_thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&Service::AutoCleanupCallback, base::Unretained(this)),
      auto_cleanup_period_);
}

void Service::DetectEnterpriseOwnership() const {
  static const char true_str[] = "true";
  const chromeos::Blob true_value(true_str, true_str + arraysize(true_str));
  chromeos::Blob value;
  if (install_attrs_->Get("enterprise.owned", &value) && value == true_value)
    // Tell every mount that it is enterprise-owned.
    for (MountMap::const_iterator it = mounts_.begin();
         it != mounts_.end(); ++it)
      it->second->set_enterprise_owned(true);
}

cryptohome::Mount* Service::CreateMountForUser(const std::string& username) {
  CHECK_EQ(mounts_.count(username), 0U);
  cryptohome::Mount* m = new cryptohome::Mount();
  mounts_[username] = m;
  return m;
}

cryptohome::Mount* Service::GetMountForUser(const std::string& username) {
  if (mounts_.count(username) == 1)
    return mounts_[username];
  return NULL;
}

bool Service::CreateSystemSaltIfNeeded() {
  if (!system_salt_.empty())
    return true;
  FilePath saltfile(kSaltFilePath);
  return crypto_->GetOrCreateSalt(saltfile, CRYPTOHOME_DEFAULT_SALT_LENGTH,
                                  false, &system_salt_);
}

void Service::DispatchEvents() {
  event_source_.HandleDispatch();
}

}  // namespace cryptohome
