// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
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
#include <chaps/isolate.h>
#include <chaps/token_manager_client.h>
#include <chromeos/cryptohome.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/secure_blob.h>
#include <metrics/metrics_library.h>
#include <metrics/timer.h>
#include <map>
#include <string>
#include <vector>

#include "attestation_task.h"
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

const char* kSaltFilePath = "/home/.shadow/salt";
const char* kPublicMountSaltFilePath = "/var/lib/public_mount_salt";

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

// A helper function which maps an integer to a valid CertificateProfile.
CertificateProfile GetProfile(int profile_value) {
  const int kMaxProfileEnum = 2;
  if (profile_value < 0 || profile_value > kMaxProfileEnum)
    profile_value = kMaxProfileEnum;
  return static_cast<CertificateProfile>(profile_value);
};

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
  scoped_refptr<cryptohome::Mount> mount_;
  CryptohomeEventSource* source_;
};

Service::Service()
    : use_tpm_(true),
      loop_(NULL),
      cryptohome_(NULL),
      system_salt_(),
      default_platform_(new Platform()),
      platform_(default_platform_.get()),
      crypto_(new Crypto(platform_)),
      tpm_(Tpm::GetSingleton()),
      default_tpm_init_(new TpmInit(platform_)),
      tpm_init_(default_tpm_init_.get()),
      default_pkcs11_init_(new Pkcs11Init()),
      pkcs11_init_(default_pkcs11_init_.get()),
      initialize_tpm_(true),
      mount_thread_(kMountThreadName),
      async_complete_signal_(-1),
      async_data_complete_signal_(-1),
      tpm_init_signal_(-1),
      event_source_(),
      auto_cleanup_period_(kAutoCleanupPeriodMS),
      default_install_attrs_(new cryptohome::InstallAttributes(NULL)),
      install_attrs_(default_install_attrs_.get()),
      update_user_activity_period_(kUpdateUserActivityPeriod - 1),
      timer_collection_(new TimerCollection()),
      reported_pkcs11_init_fail_(false),
      enterprise_owned_(false),
      mounts_lock_(),
      default_mount_factory_(new cryptohome::MountFactory),
      mount_factory_(default_mount_factory_.get()),
      default_homedirs_(new cryptohome::HomeDirs()),
      homedirs_(default_homedirs_.get()),
      guest_user_(chromeos::cryptohome::home::kGuestUserName),
      legacy_mount_(true),
      public_mount_salt_() {
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

bool Service::GetExistingMounts(
    std::multimap<const std::string, const std::string>* mounts) {
  bool found = platform_->GetMountsBySourcePrefix("/home/.shadow/", mounts);
  found |= platform_->GetMountsBySourcePrefix(kEphemeralDir, mounts);
  found |= platform_->GetMountsBySourcePrefix(kGuestMountPath, mounts);
  return found;
}

static bool PrefixPresent(const std::vector<std::string>& prefixes,
                          const std::string& path) {
  std::vector<std::string>::const_iterator it;
  for (it = prefixes.begin(); it != prefixes.end(); ++it)
    if (StartsWithASCII(path, *it, false))
      return true;
  return false;
}

bool Service::UnloadPkcs11Tokens(const std::vector<std::string>& exclude) {
  SecureBlob isolate =
      chaps::IsolateCredentialManager::GetDefaultIsolateCredential();
  chaps::TokenManagerClient chaps_client_;
  std::vector<std::string> tokens;
  if (!chaps_client_.GetTokenList(isolate, &tokens))
    return false;
  for (size_t i = 0; i < tokens.size(); ++i) {
    if (!PrefixPresent(exclude, tokens[i])) {
      LOG(INFO) << "Cleaning up: " << tokens[i];
      chaps_client_.UnloadToken(isolate, FilePath(tokens[i]));
    }
  }
  return true;
}

bool Service::CleanUpStaleMounts(bool force) {
  // This function is meant to aid in a clean recover from a crashed or
  // manually restarted cryptohomed.  Cryptohomed may restart:
  // 1. Before any mounts occur
  // 2. While mounts are active
  // 3. During an unmount
  // In case #1, there should be no special work to be done.
  // The best way to disambiguate #2 and #3 is to determine if there are
  // any active open files on any stale mounts.  If there are open files,
  // then we've likely(*) resumed an active session. If there are not,
  // the last cryptohome should have been unmounted.
  // It's worth noting that a restart during active use doesn't impair
  // other user session behavior, like CheckKey, because it doesn't rely
  // exclusively on mount state.
  //
  // In the future, it may make sense to attempt to keep the MountMap
  // persisted to disk which would make resumption much easier.
  //
  // (*) Relies on the expectation that all processes have been killed off.
  bool skipped = false;
  std::multimap<const std::string, const std::string> matches;
  std::vector<std::string> exclude;
  if (!GetExistingMounts(&matches)) {
    // If there's no existing mounts, go ahead and unload all chaps tokens by
    // passing an empty exclude list.
    UnloadPkcs11Tokens(exclude);
    return skipped;
  }

  std::multimap<const std::string, const std::string>::iterator match;
  for (match = matches.begin(); match != matches.end(); ) {
    std::multimap<const std::string, const std::string>::iterator curr = match;
    bool keep = false;
    // Walk each set of sources as one group since multimaps are key ordered.
    do {
      // Ignore known mounts.
      for (MountMap::iterator mount = mounts_.begin();
           mount != mounts_.end(); ++mount) {
        if (mount->second->OwnsMountPoint(match->second)) {
          keep = true;
          break;
        }
      }
      // Optionally, ignore mounts with open files.
      if (!force) {
        std::vector<ProcessInformation> processes;
        platform_->GetProcessesWithOpenFiles(match->second, &processes);
        if (processes.size()) {
          LOG(WARNING) << "Stale mount " << match->second
                       << " from " << match->first
                       << " has active holders.";
          keep = true;
          skipped = true;
        }
      }
      ++match;
    } while (match != matches.end() && match->first == curr->first);

    // Delete anything that shouldn't be unmounted.
    if (keep) {
      --match;
      exclude.push_back(match->second);
      matches.erase(curr, ++match);
    }
  }
  UnloadPkcs11Tokens(exclude);
  // Unmount anything left.
  for (match = matches.begin(); match != matches.end(); ++match) {
    LOG(WARNING) << "Lazily unmounting stale mount: " << match->second
                 << " from " << match->first;
    platform_->Unmount(match->second, true, NULL);
  }
  return skipped;
}

bool Service::Initialize() {
  bool result = true;

  // Initialize the metrics library for stat reporting.
  metrics_lib_.Init();
  chromeos_metrics::TimerReporter::set_metrics_lib(&metrics_lib_);

  crypto_->set_use_tpm(use_tpm_);
  if (!crypto_->Init())
    return false;

  homedirs_->set_crypto(crypto_);
  if (!homedirs_->Init())
    return false;

  // If the TPM is unowned or doesn't exist, it's safe for
  // this function to be called again. However, it shouldn't
  // be called across multiple threads in parallel.
  InitializeInstallAttributes(false);

  // Clean up any unreferenced mountpoints at startup.
  CleanUpStaleMounts(false);

  // TODO(wad) Determine if this should only be called if
  //           tpm->IsEnabled() is true.
  if (tpm_ && initialize_tpm_) {
    tpm_init_->set_tpm(tpm_);
    tpm_init_->Init(this, crypto_);
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

  async_data_complete_signal_ = g_signal_new(
      "async_call_status_with_data",
      gobject::cryptohome_get_type(),
      G_SIGNAL_RUN_LAST,
      0,
      NULL,
      NULL,
      cryptohome_VOID__INT_BOOLEAN_POINTER,
      G_TYPE_NONE,
      3,
      G_TYPE_INT,
      G_TYPE_BOOLEAN,
      DBUS_TYPE_G_UCHAR_ARRAY);

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

  // TODO(keescook,ellyjones) Make this mock-able.
  StatefulRecovery recovery(platform_, this);
  if (recovery.Requested()) {
    if (recovery.Recover())
      LOG(INFO) << "A stateful recovery was performed successfully.";
    recovery.PerformReboot();
  }

  return result;
}

bool Service::IsOwner(const std::string &userid) {
  std::string owner;
  if (homedirs_->GetPlainOwner(&owner) && userid.length() && userid == owner)
    return true;
  return false;
}

void Service::InitializeInstallAttributes(bool first_time) {
  // Wait for ownership if there is a working TPM.
  if (tpm_ && tpm_->IsEnabled() && !tpm_->IsOwned())
    return;

  // The TPM owning instance may have changed since initialization.
  // InstallAttributes can handle a NULL or !IsEnabled Tpm object.
  install_attrs_->SetTpm(tpm_);

  if (first_time && !install_attrs_->PrepareSystem()) {
    // TODO(wad) persist this failure to allow recovery or force
    //           powerwash/reset.
    LOG(ERROR) << "Unable to prepare system for install attributes.";
  }

  // Init can fail without making the interface inconsistent so we're okay here.
  install_attrs_->Init();

  // Check if the machine is enterprise owned and report to mount_ then.
  DetectEnterpriseOwnership();
}

void Service::InitializePkcs11(cryptohome::Mount* mount) {
  if (!mount) {
    LOG(ERROR) << "InitializePkcs11 called with NULL mount!";
    return;
  }
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
  if (!mount->IsMounted()) {
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
  pkcs11_tasks_[pkcs11_init_task->sequence_id()] = pkcs11_init_task.get();
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskPkcs11Init::Run, pkcs11_init_task.get()));
}

bool Service::SeedUrandom() {
  SecureBlob random;
  if (!tpm_init_->GetRandomData(kDefaultRandomSeedLength, &random)) {
    LOG(ERROR) << "Could not get random data from the TPM";
    return false;
  }
  if (!platform_->WriteFile(kDefaultEntropySource, random)) {
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
    if (!result->return_data()) {
      g_signal_emit(cryptohome_,
                    async_complete_signal_,
                    0,
                    result->sequence_id(),
                    result->return_status(),
                    result->return_code());
      // TODO(wad) are there any non-mount uses of this type?
      if (!result->return_status()) {
        RemoveMount(result->mount());
      }
    } else {
      chromeos::glib::ScopedArray tmp_array(g_array_new(FALSE, FALSE, 1));
      g_array_append_vals(tmp_array.get(),
                          result->return_data()->const_data(),
                          result->return_data()->size());
      g_signal_emit(cryptohome_,
                    async_data_complete_signal_,
                    0,
                    result->sequence_id(),
                    result->return_status(),
                    tmp_array.get());
      chromeos::SecureMemset(tmp_array.get()->data, 0, tmp_array.get()->len);
    }
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
      if (!result->return_status()) {
        DLOG(INFO) << "Dropping MountMap entry for failed Guest mount.";
        RemoveMountForUser(guest_user_);
      }
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
    // Drop the reference since the work is done.
    pkcs11_tasks_.erase(result->sequence_id());
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
      *OUT_async_id = PostAsyncCallResult(bridge, MOUNT_ERROR_NONE, ok);
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

gboolean Service::AddKey(gchar *userid,
                         gchar *key,
                         gchar *new_key,
                         gint *OUT_key_id,
                         gboolean *OUT_result,
                         GError **error) {
  UsernamePasskey credentials(userid, SecureBlob(key, strlen(key)));

  MountTaskResult result;
  base::WaitableEvent event(true, false);
  scoped_refptr<MountTaskAddPasskey> mount_task =
      new MountTaskAddPasskey(NULL, homedirs_, credentials, new_key);
  mount_task->set_result(&result);
  mount_task->set_complete_event(&event);
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskAddPasskey::Run, mount_task.get()));
  event.Wait();
  *OUT_key_id = result.return_code();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncAddKey(gchar *userid,
                              gchar *key,
                              gchar *new_key,
                              gint *OUT_async_id,
                              GError **error) {
  UsernamePasskey credentials(userid, SecureBlob(key, strlen(key)));

  MountTaskObserverBridge* bridge =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<MountTaskAddPasskey> mount_task =
      new MountTaskAddPasskey(bridge, homedirs_, credentials, new_key);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskAddPasskey::Run, mount_task.get()));
  return TRUE;
}

gboolean Service::Remove(gchar *userid,
                         gboolean *OUT_result,
                         GError **error) {
  UsernamePasskey credentials(userid, chromeos::Blob());
  scoped_refptr<cryptohome::Mount> user_mount = GetMountForUser(userid);
  if (user_mount.get() && user_mount->IsMounted()) {
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
  MountTaskObserverBridge* bridge =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<cryptohome::Mount> user_mount = GetMountForUser(userid);
  if (user_mount.get() && user_mount->IsMounted()) {
    scoped_refptr<MountTaskNop> mount_task = new MountTaskNop(bridge);
    mount_task->result()->set_return_status(false);
    *OUT_async_id = mount_task->sequence_id();
    mount_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(&MountTaskNop::Run, mount_task.get()));
  } else {
    UsernamePasskey credentials(userid, chromeos::Blob());
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

gboolean Service::GetSanitizedUsername(gchar *username,
                                       gchar **OUT_sanitized,
                                       GError **error) {
  // UsernamePasskey::GetObfuscatedUsername() returns an uppercase hex encoding,
  // while SanitizeUserName() returns a lowercase hex encoding. They should
  // return the same value, but login_manager is already relying on
  // SanitizeUserName() and that's the value that chrome should see.
  std::string sanitized =
      chromeos::cryptohome::home::SanitizeUserName(username);
  if (sanitized.empty())
    return FALSE;
  *OUT_sanitized = g_strndup(sanitized.data(), sanitized.size());
  return TRUE;
}

gboolean Service::IsMounted(gboolean *OUT_is_mounted, GError **error) {
  // We consider "the cryptohome" to be mounted if any existing cryptohome is
  // mounted.
  *OUT_is_mounted = FALSE;
  for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); ++it) {
    if (it->second->IsMounted()) {
      *OUT_is_mounted = TRUE;
      break;
    }
  }
  return TRUE;
}

gboolean Service::IsMountedForUser(gchar *userid,
                                   gboolean *OUT_is_mounted,
                                   gboolean *OUT_is_ephemeral_mount,
                                   GError **error) {
  scoped_refptr<cryptohome::Mount> mount = GetMountForUser(userid);
  *OUT_is_mounted = false;
  *OUT_is_ephemeral_mount = false;
  if (!mount.get())
    return TRUE;
  if (mount->IsVaultMounted()) {
    *OUT_is_mounted = true;
    *OUT_is_ephemeral_mount = false;
  } else if (mount->IsMounted()) {
    *OUT_is_mounted = true;
    *OUT_is_ephemeral_mount = true;
  }
  return TRUE;
}

gboolean Service::Mount(const gchar *userid,
                        const gchar *key,
                        gboolean create_if_missing,
                        gboolean ensure_ephemeral,
                        gint *OUT_error_code,
                        gboolean *OUT_result,
                        GError **error) {
  // This is safe even if cryptohomed restarts during a multi-mount
  // session and a new mount is added because cleanup is not forced.
  // An existing process will keep the mount alive.  On the next
  // Unmount() it'll be forcibly cleaned up.  In the case that
  // cryptohomed crashes and misses the Unmount call, the stale
  // mountpoints should still be cleaned up on the next daemon
  // interaction.
  //
  // As we introduce multiple mounts, we can consider API changes to
  // make it clearer what the UI expectations are (AddMount, etc).
  if (mounts_.size() == 0)
    // This could run on every interaction to catch any unused mounts.
    CleanUpStaleMounts(false);

  UsernamePasskey credentials(userid, SecureBlob(key, strlen(key)));

  scoped_refptr<cryptohome::Mount> guest_mount = GetMountForUser(guest_user_);
  bool guest_mounted = guest_mount.get() && guest_mount->IsMounted();
  if (guest_mounted && !guest_mount->UnmountCryptohome()) {
    LOG(ERROR) << "Could not unmount cryptohome from Guest session";
    *OUT_error_code = MOUNT_ERROR_MOUNT_POINT_BUSY;
    *OUT_result = FALSE;
    return TRUE;
  }

  // If a cryptohome is mounted for the user already, reuse that mount unless
  // the |ensure_ephemeral| flag prevents it: When |ensure_ephemeral| is
  // |true|, a cryptohome backed by tmpfs is required. If the currently
  // mounted cryptohome is backed by a vault, it must be unmounted and
  // remounted with a tmpfs backend.
  scoped_refptr<cryptohome::Mount> user_mount = GetOrCreateMountForUser(userid);
  if (ensure_ephemeral && user_mount->IsVaultMounted()) {
    // TODO(wad,ellyjones) Change this behavior to return failure even
    // on a succesful unmount to tell chrome MOUNT_ERROR_NEEDS_RESTART.
    if (!user_mount->UnmountCryptohome()) {
      // The MountMap entry is kept since the Unmount failed.
      LOG(ERROR) << "Could not unmount vault before an ephemeral mount.";
      *OUT_error_code = MOUNT_ERROR_MOUNT_POINT_BUSY;
      *OUT_result = FALSE;
      return TRUE;
    }
  }

  // TODO(wad) A case we haven't handled is mount-over of a non-ephemeral user.

  // This is the case where there were 2 mount requests for a given user
  // without any intervening unmount requests. This should only be able to
  // happen if Chrome acts pathologically and re-requests a Mount.  If,
  // for instance, cryptohomed crashed, the MountMap would not contain the
  // entry.
  // TODO(wad) Can we get rid of this code path?

  if (user_mount->IsMounted()) {
    // TODO(wad) This tests against the stored credentials, not the TPM.
    // If mounts are "repopulated", then a trip through the TPM would be needed.
    LOG(INFO) << "Mount exists. Rechecking credentials.";
    if (!user_mount->AreSameUser(credentials) ||
        !user_mount->AreValid(credentials)) {
      // Need to take a trip through the TPM.
      if (!homedirs_->AreCredentialsValid(credentials)) {
        LOG(ERROR) << "Failed to reauthenticate against the existing mount!";
        // TODO(wad) Should we teardown all the mounts if this happens?
        // RemoveAllMounts();
        *OUT_error_code = MOUNT_ERROR_KEY_FAILURE;
        *OUT_result = FALSE;
        return TRUE;
      }
    }

    // As far as PKCS#11 initialization goes, we treat this as a brand new
    // mount request. InitializePkcs11() will detect and re-initialize if
    // necessary except if the mount point is ephemeral as there is no PKCS#11
    // data.
    InitializePkcs11(user_mount.get());
    *OUT_error_code = MOUNT_ERROR_NONE;
    *OUT_result = TRUE;
    return TRUE;
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
  mount_args.ensure_ephemeral = ensure_ephemeral;
  scoped_refptr<MountTaskMount> mount_task = new MountTaskMount(
                                                            NULL,
                                                            user_mount.get(),
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

  user_mount->set_pkcs11_state(cryptohome::Mount::kUninitialized);
  InitializePkcs11(user_mount.get());

  *OUT_error_code = result.return_code();
  *OUT_result = result.return_status();
  return TRUE;
}

gboolean Service::AsyncMount(const gchar *userid,
                             const gchar *key,
                             gboolean create_if_missing,
                             gboolean ensure_ephemeral,
                             gint *OUT_async_id,
                             GError **error) {
  // See ::Mount for detailed commentary.
  if (mounts_.size() == 0)
    CleanUpStaleMounts(false);

  UsernamePasskey credentials(userid, SecureBlob(key, strlen(key)));

  scoped_refptr<cryptohome::Mount> guest_mount = GetMountForUser(guest_user_);
  bool guest_mounted = guest_mount.get() && guest_mount->IsMounted();
  // TODO(wad,ellyjones) Change this behavior to return failure even
  // on a succesful unmount to tell chrome MOUNT_ERROR_NEEDS_RESTART.
  if (guest_mounted && !guest_mount->UnmountCryptohome()) {
    LOG(ERROR) << "Could not unmount cryptohome from Guest session";
    MountTaskObserverBridge* bridge =
        new MountTaskObserverBridge(guest_mount.get(), &event_source_);
    *OUT_async_id = PostAsyncCallResult(bridge,
                                        MOUNT_ERROR_MOUNT_POINT_BUSY,
                                        false);
    return TRUE;
  }

  // Don't overlay an ephemeral mount over a file-backed one.
  scoped_refptr<cryptohome::Mount> user_mount = GetOrCreateMountForUser(userid);
  if (ensure_ephemeral && user_mount->IsVaultMounted()) {
    // TODO(wad,ellyjones) Change this behavior to return failure even
    // on a succesful unmount to tell chrome MOUNT_ERROR_NEEDS_RESTART.
    if (!user_mount->UnmountCryptohome()) {
      LOG(ERROR) << "Could not unmount vault before an ephemeral mount.";
      MountTaskObserverBridge* bridge =
           new MountTaskObserverBridge(user_mount.get(), &event_source_);
      *OUT_async_id = PostAsyncCallResult(bridge,
                                          MOUNT_ERROR_MOUNT_POINT_BUSY,
                                          false);
      return TRUE;
    }
  }

  if (user_mount->IsMounted()) {
    LOG(INFO) << "Mount exists. Rechecking credentials.";
    MountTaskObserverBridge* bridge =
        new MountTaskObserverBridge(user_mount.get(), &event_source_);
    // Attempt a short-circuited credential test.
    if (user_mount->AreSameUser(credentials) &&
        user_mount->AreValid(credentials)) {
      *OUT_async_id = PostAsyncCallResult(bridge, MOUNT_ERROR_NONE, true);
      return TRUE;
    }
    // TODO(wad) This should really hang off of the MountTaskTestCredentials
    //           below I believe.
    // See comment in Service::Mount() above on why this is needed here.
    InitializePkcs11(user_mount.get());

    // If the Mount has invalid credentials (repopulated from system state)
    // this will ensure a user can still sign-in with the right ones.
    // TODO(wad) Should we unmount on a failed re-mount attempt?
    scoped_refptr<MountTaskTestCredentials> mount_task
      = new MountTaskTestCredentials(bridge, NULL, homedirs_, credentials);
    *OUT_async_id = mount_task->sequence_id();
    mount_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(&MountTaskTestCredentials::Run, mount_task.get()));
    return TRUE;
  }

  // See Mount for a relevant comment.
  if (install_attrs_->is_first_install()) {
    MountTaskInstallAttrsFinalize *finalize =
        new MountTaskInstallAttrsFinalize(NULL, install_attrs_);
    mount_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(&MountTaskInstallAttrsFinalize::Run, finalize));
  }

  timer_collection_->UpdateTimer(TimerCollection::kAsyncMountTimer, true);
  Mount::MountArgs mount_args;
  mount_args.create_if_missing = create_if_missing;
  mount_args.ensure_ephemeral = ensure_ephemeral;
  MountTaskObserverBridge *bridge =
      new MountTaskObserverBridge(user_mount, &event_source_);
  scoped_refptr<MountTaskMount> mount_task = new MountTaskMount(
                                                            bridge,
                                                            user_mount.get(),
                                                            credentials,
                                                            mount_args);
  mount_task->result()->set_pkcs11_init(true);
  user_mount->set_pkcs11_state(cryptohome::Mount::kUninitialized);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskMount::Run, mount_task.get()));

  LOG(INFO) << "Asynced Mount() requested. Tracking request sequence id"
            << " for later PKCS#11 initialization.";
  return TRUE;
}

gboolean Service::MountGuest(gint *OUT_error_code,
                             gboolean *OUT_result,
                             GError **error) {
  if (mounts_.size() != 0)
    LOG(WARNING) << "Guest mount requested with other mounts active.";
  // Rather than make it safe to check the size, then clean up, just always
  // clean up.
  if (!RemoveAllMounts(true)) {
    LOG(ERROR) << "Could not unmount cryptohomes for Guest use";
    *OUT_error_code = MOUNT_ERROR_MOUNT_POINT_BUSY;
    *OUT_result = FALSE;
    return TRUE;
  }

  scoped_refptr<cryptohome::Mount> guest_mount =
    GetOrCreateMountForUser(guest_user_);
  timer_collection_->UpdateTimer(TimerCollection::kSyncGuestMountTimer, true);
  MountTaskResult result;
  base::WaitableEvent event(true, false);
  scoped_refptr<MountTaskMountGuest> mount_task
      = new MountTaskMountGuest(NULL, guest_mount.get());
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
  if (mounts_.size() != 0)
    LOG(WARNING) << "Guest mount requested with other mounts active.";
  // Rather than make it safe to check the size, then clean up, just always
  // clean up.
  bool ok = RemoveAllMounts(true);
  // Create a ref-counted guest mount for async use and then throw it away.
  scoped_refptr<cryptohome::Mount> guest_mount =
    GetOrCreateMountForUser(guest_user_);
  if (!ok) {
    LOG(ERROR) << "Could not unmount cryptohomes for Guest use";
    MountTaskObserverBridge* bridge =
        new MountTaskObserverBridge(guest_mount.get(), &event_source_);
    // Drop it from the map now that the MountTask has a ref.
    if (!RemoveMountForUser(guest_user_)) {
      LOG(ERROR) << "Unexpectedly cannot drop unused Guest mount from map.";
    }
    *OUT_async_id = PostAsyncCallResult(bridge,
                                        MOUNT_ERROR_MOUNT_POINT_BUSY,
                                        false);
    return TRUE;
  }

  timer_collection_->UpdateTimer(TimerCollection::kAsyncGuestMountTimer, true);
  MountTaskObserverBridge* bridge =
      new MountTaskObserverBridge(guest_mount.get(), &event_source_);
  scoped_refptr<MountTaskMountGuest> mount_task
      = new MountTaskMountGuest(bridge, guest_mount.get());
  mount_task->result()->set_guest(true);
  *OUT_async_id = mount_task->sequence_id();
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskMountGuest::Run, mount_task.get()));
  return TRUE;
}

gboolean Service::MountPublic(const gchar* public_mount_id,
                              gboolean create_if_missing,
                              gboolean ensure_ephemeral,
                              gint* OUT_error_code,
                              gboolean* OUT_result,
                              GError** error) {
  // Don't proceed if there is any existing mount or stale mount.
  if (mounts_.size() != 0 || CleanUpStaleMounts(false))  {
    LOG(ERROR) << "Public mount requested with other mounts active.";
    *OUT_error_code = MOUNT_ERROR_MOUNT_POINT_BUSY;
    *OUT_result = FALSE;
    return TRUE;
  }

  std::string public_mount_passkey;
  if (!GetPublicMountPassKey(public_mount_id, &public_mount_passkey)) {
    LOG(ERROR) << "Could not get public mount passkey.";
    *OUT_error_code = MOUNT_ERROR_KEY_FAILURE;
    *OUT_result = FALSE;
    return FALSE;
  }

  return Mount(public_mount_id,
               public_mount_passkey.c_str(),
               create_if_missing,
               ensure_ephemeral,
               OUT_error_code,
               OUT_result,
               error);
}

gboolean Service::AsyncMountPublic(const gchar* public_mount_id,
                                   gboolean create_if_missing,
                                   gboolean ensure_ephemeral,
                                   gint* OUT_async_id,
                                   GError** error) {
  // Don't proceed if there is any existing mount or stale mount.
  if (mounts_.size() != 0 || CleanUpStaleMounts(false))  {
    LOG(ERROR) << "Public mount requested with other mounts active.";

    *OUT_async_id = PostAsyncCallResultForUser(
        public_mount_id, MOUNT_ERROR_MOUNT_POINT_BUSY, false);
    return TRUE;
  }

  std::string public_mount_passkey;
  if (!GetPublicMountPassKey(public_mount_id, &public_mount_passkey)) {
    LOG(ERROR) << "Could not get public mount passkey.";

    *OUT_async_id = PostAsyncCallResultForUser(
        public_mount_id, MOUNT_ERROR_KEY_FAILURE, false);
    return TRUE;
  }

  return AsyncMount(public_mount_id,
                    public_mount_passkey.c_str(),
                    create_if_missing,
                    ensure_ephemeral,
                    OUT_async_id,
                    error);
}

// Unmount all mounted cryptohomes.
gboolean Service::Unmount(gboolean *OUT_result, GError **error) {
  *OUT_result = RemoveAllMounts(true);
  // If there are any unexpected mounts lingering from a crash/restart,
  // clean them up now.
  CleanUpStaleMounts(true);
  return TRUE;
}

gboolean Service::UnmountForUser(const gchar *userid, gboolean *OUT_result,
                                 GError **error) {
  // NOTE: it's not clear we ever want to allow a per-user unmount.
  return Unmount(OUT_result, error);
}

gboolean Service::DoAutomaticFreeDiskSpaceControl(gboolean *OUT_result,
                                                  GError **error) {
  MountTaskResult result;
  base::WaitableEvent event(true, false);
  MountTaskObserverBridge* bridge =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<MountTaskAutomaticFreeDiskSpace> mount_task =
      new MountTaskAutomaticFreeDiskSpace(bridge, homedirs_);
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
      new MountTaskAutomaticFreeDiskSpace(bridge, homedirs_);
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

gboolean Service::TpmAttestationCreateEnrollRequest(GArray** OUT_pca_request,
                                                    GError** error) {
  *OUT_pca_request = g_array_new(false, false, sizeof(SecureBlob::value_type));
  Attestation* attestation = tpm_init_->get_attestation();
  if (!attestation) {
    LOG(ERROR) << "Attestation is not available.";
    return TRUE;
  }
  chromeos::SecureBlob blob;
  if (attestation->CreateEnrollRequest(&blob))
    g_array_append_vals(*OUT_pca_request, &blob.front(), blob.size());
  return TRUE;
}

gboolean Service::AsyncTpmAttestationCreateEnrollRequest(gint* OUT_async_id,
                                                         GError** error) {
  AttestationTaskObserver* observer =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<CreateEnrollRequestTask> task =
      new CreateEnrollRequestTask(observer, tpm_init_->get_attestation());
  *OUT_async_id = task->sequence_id();
  mount_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CreateEnrollRequestTask::Run, task.get()));
  return TRUE;
}

gboolean Service::TpmAttestationEnroll(GArray* pca_response,
                                       gboolean* OUT_success,
                                       GError** error) {
  Attestation* attestation = tpm_init_->get_attestation();
  if (!attestation) {
    LOG(ERROR) << "Attestation is not available.";
    *OUT_success = FALSE;
    return TRUE;
  }
  chromeos::SecureBlob blob(pca_response->data, pca_response->len);
  *OUT_success = attestation->Enroll(blob);
  return TRUE;
}

gboolean Service::AsyncTpmAttestationEnroll(GArray* pca_response,
                                            gint* OUT_async_id,
                                            GError** error) {
  chromeos::SecureBlob blob(pca_response->data, pca_response->len);
  AttestationTaskObserver* observer =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<EnrollTask> task =
      new EnrollTask(observer, tpm_init_->get_attestation(), blob);
  *OUT_async_id = task->sequence_id();
  mount_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&EnrollTask::Run, task.get()));
  return TRUE;
}

gboolean Service::TpmAttestationCreateCertRequest(gint certificate_profile,
                                                  gchar* username,
                                                  gchar* request_origin,
                                                  GArray** OUT_pca_request,
                                                  GError** error) {
  *OUT_pca_request = g_array_new(false, false, sizeof(SecureBlob::value_type));
  Attestation* attestation = tpm_init_->get_attestation();
  if (!attestation) {
    LOG(ERROR) << "Attestation is not available.";
    return TRUE;
  }
  chromeos::SecureBlob blob;
  if (attestation->CreateCertRequest(GetProfile(certificate_profile),
                                     "",
                                     request_origin,
                                     &blob))
    g_array_append_vals(*OUT_pca_request, &blob.front(), blob.size());
  return TRUE;
}

gboolean Service::AsyncTpmAttestationCreateCertRequest(
    gint certificate_profile,
    gchar* username,
    gchar* request_origin,
    gint* OUT_async_id,
    GError** error) {
  AttestationTaskObserver* observer =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<CreateCertRequestTask> task =
      new CreateCertRequestTask(observer,
                                tpm_init_->get_attestation(),
                                GetProfile(certificate_profile),
                                username,
                                request_origin);
  *OUT_async_id = task->sequence_id();
  mount_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CreateCertRequestTask::Run, task.get()));
  return TRUE;
}

gboolean Service::TpmAttestationFinishCertRequest(GArray* pca_response,
                                                  gboolean is_user_specific,
                                                  gchar* key_name,
                                                  GArray** OUT_cert,
                                                  gboolean* OUT_success,
                                                  GError** error) {
  *OUT_cert = g_array_new(false, false, sizeof(SecureBlob::value_type));
  Attestation* attestation = tpm_init_->get_attestation();
  if (!attestation) {
    LOG(ERROR) << "Attestation is not available.";
    *OUT_success = FALSE;
    return TRUE;
  }
  chromeos::SecureBlob response_blob(pca_response->data, pca_response->len);
  chromeos::SecureBlob cert_blob;
  *OUT_success = attestation->FinishCertRequest(response_blob,
                                                is_user_specific,
                                                key_name,
                                                &cert_blob);
  if (*OUT_success)
    g_array_append_vals(*OUT_cert, &cert_blob.front(), cert_blob.size());
  return TRUE;
}

gboolean Service::AsyncTpmAttestationFinishCertRequest(
    GArray* pca_response,
    gboolean is_user_specific,
    gchar* key_name,
    gint* OUT_async_id,
    GError** error) {
  chromeos::SecureBlob blob(pca_response->data, pca_response->len);
  AttestationTaskObserver* observer =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<FinishCertRequestTask> task =
      new FinishCertRequestTask(observer,
                                tpm_init_->get_attestation(),
                                blob,
                                is_user_specific,
                                key_name);
  *OUT_async_id = task->sequence_id();
  mount_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&FinishCertRequestTask::Run, task.get()));
  return TRUE;
}

gboolean Service::TpmIsAttestationEnrolled(gboolean* OUT_is_enrolled,
                                           GError** error) {
  *OUT_is_enrolled = FALSE;
  Attestation* attestation = tpm_init_->get_attestation();
  if (attestation)
    *OUT_is_enrolled = attestation->IsEnrolled();
  return TRUE;
}

gboolean Service::TpmAttestationDoesKeyExist(gboolean is_user_specific,
                                             gchar* key_name,
                                             gboolean *OUT_exists,
                                             GError** error) {
  Attestation* attestation = tpm_init_->get_attestation();
  if (!attestation) {
    LOG(ERROR) << "Attestation is not available.";
    *OUT_exists = FALSE;
    return TRUE;
  }
  *OUT_exists = attestation->DoesKeyExist(is_user_specific, key_name);
  return TRUE;
}

gboolean Service::TpmAttestationGetCertificate(gboolean is_user_specific,
                                               gchar* key_name,
                                               GArray **OUT_certificate,
                                               gboolean* OUT_success,
                                               GError** error) {
  *OUT_certificate = g_array_new(false, false, sizeof(SecureBlob::value_type));
  Attestation* attestation = tpm_init_->get_attestation();
  if (!attestation) {
    LOG(ERROR) << "Attestation is not available.";
    *OUT_success = FALSE;
    return TRUE;
  }
  chromeos::SecureBlob blob;
  *OUT_success = attestation->GetCertificateChain(is_user_specific,
                                                  key_name,
                                                  &blob);
  if (*OUT_success)
    g_array_append_vals(*OUT_certificate, &blob.front(), blob.size());
  return TRUE;
}

gboolean Service::TpmAttestationGetPublicKey(gboolean is_user_specific,
                                             gchar* key_name,
                                             GArray **OUT_public_key,
                                             gboolean* OUT_success,
                                             GError** error) {
  *OUT_public_key = g_array_new(false, false, sizeof(SecureBlob::value_type));
  Attestation* attestation = tpm_init_->get_attestation();
  if (!attestation) {
    LOG(ERROR) << "Attestation is not available.";
    *OUT_success = FALSE;
    return TRUE;
  }
  chromeos::SecureBlob blob;
  *OUT_success = attestation->GetPublicKey(is_user_specific,
                                           key_name,
                                           &blob);
  if (*OUT_success)
    g_array_append_vals(*OUT_public_key, &blob.front(), blob.size());
  return TRUE;
}

gboolean Service::TpmAttestationRegisterKey(gboolean is_user_specific,
                                            gchar* key_name,
                                            gint *OUT_async_id,
                                            GError** error) {
  AttestationTaskObserver* observer =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<RegisterKeyTask> task =
      new RegisterKeyTask(observer,
                          tpm_init_->get_attestation(),
                          is_user_specific,
                          key_name);
  *OUT_async_id = task->sequence_id();
  mount_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&RegisterKeyTask::Run, task.get()));
  return TRUE;
}

gboolean Service::TpmAttestationSignEnterpriseChallenge(
      gboolean is_user_specific,
      gchar* key_name,
      gchar* domain,
      GArray* device_id,
      gboolean include_signed_public_key,
      GArray* challenge,
      gint *OUT_async_id,
      GError** error) {
  chromeos::SecureBlob device_id_blob(device_id->data, device_id->len);
  chromeos::SecureBlob challenge_blob(challenge->data, challenge->len);
  AttestationTaskObserver* observer =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<SignChallengeTask> task =
      new SignChallengeTask(observer,
                            tpm_init_->get_attestation(),
                            is_user_specific,
                            key_name,
                            domain,
                            device_id_blob,
                            include_signed_public_key,
                            challenge_blob);
  *OUT_async_id = task->sequence_id();
  mount_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SignChallengeTask::Run, task.get()));
  return TRUE;
}

gboolean Service::TpmAttestationSignSimpleChallenge(
      gboolean is_user_specific,
      gchar* key_name,
      GArray* challenge,
      gint *OUT_async_id,
      GError** error) {
  chromeos::SecureBlob challenge_blob(challenge->data, challenge->len);
  AttestationTaskObserver* observer =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<SignChallengeTask> task =
      new SignChallengeTask(observer,
                            tpm_init_->get_attestation(),
                            is_user_specific,
                            key_name,
                            challenge_blob);
  *OUT_async_id = task->sequence_id();
  mount_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SignChallengeTask::Run, task.get()));
  return TRUE;
}

gboolean Service::TpmAttestationGetKeyPayload(gboolean is_user_specific,
                                              gchar* key_name,
                                              GArray** OUT_payload,
                                              gboolean* OUT_success,
                                              GError** error) {
  *OUT_payload = g_array_new(false, false, sizeof(SecureBlob::value_type));
  Attestation* attestation = tpm_init_->get_attestation();
  if (!attestation) {
    LOG(ERROR) << "Attestation is not available.";
    *OUT_success = FALSE;
    return TRUE;
  }
  chromeos::SecureBlob blob;
  *OUT_success = attestation->GetKeyPayload(is_user_specific,
                                            key_name,
                                            &blob);
  if (*OUT_success)
    g_array_append_vals(*OUT_payload, &blob.front(), blob.size());
  return TRUE;
}

gboolean Service::TpmAttestationSetKeyPayload(gboolean is_user_specific,
                                              gchar* key_name,
                                              GArray* payload,
                                              gboolean* OUT_success,
                                              GError** error) {
  Attestation* attestation = tpm_init_->get_attestation();
  if (!attestation) {
    LOG(ERROR) << "Attestation is not available.";
    *OUT_success = FALSE;
    return TRUE;
  }
  chromeos::SecureBlob blob(payload->data, payload->len);
  *OUT_success = attestation->SetKeyPayload(is_user_specific,
                                            key_name,
                                            blob);
  return TRUE;
}

// Returns true if all Pkcs11 tokens are ready.
gboolean Service::Pkcs11IsTpmTokenReady(gboolean* OUT_ready, GError** error) {
  // TODO(gauravsh): Give out more information here. The state of PKCS#11
  // initialization, and it it failed - the reason for that.
  *OUT_ready = TRUE;
  for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); ++it) {
    cryptohome::Mount* mount = it->second;
    bool ok = (mount->pkcs11_state() == cryptohome::Mount::kIsInitialized);
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
  // Don't return ready if the attestation blob isn't yet prepared.
  // http://crbug.com/189681
  if (*OUT_ready && tpm_ && !tpm_init_->IsAttestationPrepared()) {
    // Don't block on attestation prep if the TPM Owner password has been
    // cleared.  Without a password attestation preparation cannot be
    // performed.
    SecureBlob password;
    if (tpm_init_->GetTpmPassword(&password)) {
      LOG(WARNING) << "InstallAttributesIsReady: blocked on attestation";
      *OUT_ready = false;
    }
  }
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
  base::JSONWriter::WriteWithOptions(&dv,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json);
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
      base::TimeDelta::FromMilliseconds(auto_cleanup_period_));
}

void Service::DetectEnterpriseOwnership() {
  static const char true_str[] = "true";
  const chromeos::Blob true_value(true_str, true_str + arraysize(true_str));
  chromeos::Blob value;
  if (install_attrs_->Get("enterprise.owned", &value) && value == true_value) {
    enterprise_owned_ = true;
    // Update any active mounts with the state.
    for (MountMap::const_iterator it = mounts_.begin();
         it != mounts_.end(); ++it)
      it->second->set_enterprise_owned(true);
  }
}

scoped_refptr<cryptohome::Mount> Service::GetOrCreateMountForUser(
    const std::string& username) {
  scoped_refptr<cryptohome::Mount> m;
  mounts_lock_.Acquire();
  if (mounts_.count(username) == 0U) {
    m = mount_factory_->New();
    m->Init();
    m->set_enterprise_owned(enterprise_owned_);
    m->set_legacy_mount(legacy_mount_);
    mounts_[username] = m;
  } else {
    m = mounts_[username];
  }
  mounts_lock_.Release();
  return m;
}

bool Service::RemoveMountForUser(const std::string& username) {
  bool ok = true;
  mounts_lock_.Acquire();
  if (mounts_.count(username) != 0)
    ok = (1U == mounts_.erase(username));
  mounts_lock_.Release();
  return ok;
}

void Service::RemoveMount(cryptohome::Mount* mount) {
  mounts_lock_.Acquire();
  for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); ++it) {
     if (it->second.get() == mount) {
       mounts_.erase(it);
       break;
     }
  }
  mounts_lock_.Release();
}


bool Service::RemoveAllMounts(bool unmount) {
  bool ok = true;
  mounts_lock_.Acquire();
  for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); ) {
    scoped_refptr<cryptohome::Mount> mount = it->second;
    if (unmount && mount->IsMounted()) {
      if (mount->pkcs11_state() == cryptohome::Mount::kIsBeingInitialized) {
        // Walk the open tasks.
        for (Pkcs11TaskMap::iterator it = pkcs11_tasks_.begin();
             it != pkcs11_tasks_.end(); ++it) {
          scoped_refptr<MountTaskPkcs11Init> task = it->second;
          if (task->mount().get() == mount.get()) {
            task->Cancel();
            LOG(INFO) << "Cancelling PKCS#11 Init on unmount.";
            break;
          }
        }
        // Reset the state.
        mount->set_pkcs11_state(cryptohome::Mount::kUninitialized);
        // And also reset the global failure reported state.
        // TODO(wad,ellyjones,dkrahn) De-globalize this when Chaps support
        // multiple mounts.
        reported_pkcs11_init_fail_ = false;
      }
      ok = ok && mount->UnmountCryptohome();
    }
    mounts_.erase(it++);
  }
  mounts_lock_.Release();
  return ok;
}

bool Service::GetMountPointForUser(const std::string& username,
                                   std::string* path) {
  scoped_refptr<cryptohome::Mount> mount;

  mount = GetMountForUser(username);
  if (!mount.get() || !mount->IsMounted())
    return false;
  *path = mount->mount_point();
  return true;
}

scoped_refptr<cryptohome::Mount> Service::GetMountForUser(
    const std::string& username) {
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

bool Service::CreatePublicMountSaltIfNeeded() {
  if (!public_mount_salt_.empty())
    return true;
  FilePath saltfile(kPublicMountSaltFilePath);
  return crypto_->GetOrCreateSalt(saltfile, CRYPTOHOME_DEFAULT_SALT_LENGTH,
                                  false, &public_mount_salt_);
}

bool Service::GetPublicMountPassKey(const std::string& public_mount_id,
                                    std::string* public_mount_passkey) {
  if (!CreatePublicMountSaltIfNeeded())
    return false;

  SecureBlob passkey;
  Crypto::PasswordToPasskey(public_mount_id.c_str(),
                            public_mount_salt_,
                            &passkey);
  public_mount_passkey->assign(static_cast<char*>(passkey.data()),
                               passkey.size());
  return true;
}

int Service::PostAsyncCallResult(MountTaskObserver* bridge,
                                 MountError return_code,
                                 bool return_status) {
  scoped_refptr<MountTaskNop> mount_task = new MountTaskNop(bridge);
  mount_task->result()->set_return_code(return_code);
  mount_task->result()->set_return_status(return_status);
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&MountTaskNop::Run, mount_task.get()));

  return mount_task->sequence_id();
}

int Service::PostAsyncCallResultForUser(const std::string& user_id,
                                        MountError return_code,
                                        bool return_status) {
  // Create a ref-counted mount for async use and then throw it away.
  scoped_refptr<cryptohome::Mount> mount = GetOrCreateMountForUser(user_id);
  MountTaskObserverBridge* bridge =
      new MountTaskObserverBridge(mount.get(), &event_source_);
  // Drop it from the map now that the MountTask has a ref.
  if (!RemoveMountForUser(user_id))
    LOG(ERROR) << "Unexpectedly cannot drop unused mount from map.";

  return PostAsyncCallResult(bridge, return_code, return_status);
}

void Service::DispatchEvents() {
  event_source_.HandleDispatch();
}

}  // namespace cryptohome
