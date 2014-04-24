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
#include <base/strings/string_util.h>
#include <base/time/time.h>
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
#include "dbus_transition.h"
#include "install_attributes.h"
#include "interface.h"
#include "key.pb.h"
#include "marshal.glibmarshal.h"
#include "mount.h"
#include "platform.h"
#include "rpc.pb.h"
#include "stateful_recovery.h"
#include "tpm.h"
#include "username_passkey.h"
#include "vault_keyset.pb.h"

using base::FilePath;
using chromeos::SecureBlob;

// Forcibly namespace the dbus-bindings generated server bindings instead of
// modifying the files afterward.
namespace cryptohome {
namespace gobject {
#include "bindings/server.h"
}  // namespace gobject
}  // namespace cryptohome

namespace cryptohome {

const char kSaltFilePath[] = "/home/.shadow/salt";
const char kPublicMountSaltFilePath[] = "/var/lib/public_mount_salt";
const char kChapsSystemToken[] = "/var/lib/chaps";

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
    kMountExTimer,
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
  {"Cryptohome.TimeToMountEx", 0, 4000, 50},
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
const char kMountThreadName[] = "MountThread";
const char kTpmInitStatusEventType[] = "TpmInitStatus";

// The default entropy source to seed with random data from the TPM on startup.
const char kDefaultEntropySource[] = "/dev/urandom";

// The name of the UMA user action for reporting a failure to initialize the
// PKCS#11.
const char kMetricNamePkcs11InitFail[] = "Cryptohome.PKCS11InitFail";

// Location of the path to store basic device enrollment information that
// will persist across powerwashes.
const char kPreservedEnrollmentStatePath[] =
    "/mnt/stateful_partition/unencrypted/preserve/enrollment_state.epb";

// A helper function which maps an integer to a valid CertificateProfile.
CertificateProfile GetProfile(int profile_value) {
  // The protobuf compiler generates the _IsValid function.
  if (!CertificateProfile_IsValid(profile_value))
    return ENTERPRISE_USER_CERTIFICATE;
  return static_cast<CertificateProfile>(profile_value);
};

// A helper function which maps an integer to a valid Attestation::PCAType.
Attestation::PCAType GetPCAType(int value) {
  if (value < 0 || value > Attestation::kMaxPCAType)
    return Attestation::kDefaultPCA;
  return static_cast<Attestation::PCAType>(value);
}

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
      default_reply_factory_(new cryptohome::DBusReplyFactory),
      reply_factory_(default_reply_factory_.get()),
      default_homedirs_(new cryptohome::HomeDirs()),
      homedirs_(default_homedirs_.get()),
      guest_user_(chromeos::cryptohome::home::kGuestUserName),
      legacy_mount_(true),
      public_mount_salt_(),
      default_chaps_client_(new chaps::TokenManagerClient()),
      chaps_client_(default_chaps_client_.get()),
      default_attestation_(new Attestation()),
      attestation_(default_attestation_.get()) {
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
  std::vector<std::string> tokens;
  if (!chaps_client_->GetTokenList(isolate, &tokens))
    return false;
  for (size_t i = 0; i < tokens.size(); ++i) {
    if (tokens[i] != kChapsSystemToken && !PrefixPresent(exclude, tokens[i])) {
      LOG(INFO) << "Cleaning up PKCS #11 token: " << tokens[i];
      chaps_client_->UnloadToken(isolate, FilePath(tokens[i]));
    }
  }
  return true;
}

CryptohomeErrorCode Service::MountErrorToCryptohomeError(
   const MountError code) const {
  switch (code) {
  case MOUNT_ERROR_FATAL:
    return CRYPTOHOME_ERROR_MOUNT_FATAL;
  case MOUNT_ERROR_KEY_FAILURE:
   return CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED;
  case MOUNT_ERROR_MOUNT_POINT_BUSY:
    return CRYPTOHOME_ERROR_MOUNT_MOUNT_POINT_BUSY;
  case MOUNT_ERROR_TPM_COMM_ERROR:
    return CRYPTOHOME_ERROR_TPM_COMM_ERROR;
  case MOUNT_ERROR_TPM_DEFEND_LOCK:
    return CRYPTOHOME_ERROR_TPM_DEFEND_LOCK;
  case MOUNT_ERROR_USER_DOES_NOT_EXIST:
    return CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND;
  case MOUNT_ERROR_TPM_NEEDS_REBOOT:
    return CRYPTOHOME_ERROR_TPM_NEEDS_REBOOT;
  case MOUNT_ERROR_RECREATED:
  default:
    return CRYPTOHOME_ERROR_NOT_SET;
  }
}

void Service::SendReply(DBusGMethodInvocation* context,
                        const BaseReply& reply) {
  // DBusReply will take ownership of the |reply_str|.
  scoped_ptr<std::string> reply_str(new std::string);
  reply.SerializeToString(reply_str.get());
  event_source_.AddEvent(reply_factory_->NewReply(context,
                                                  reply_str.release()));
}

void Service::SendInvalidArgsReply(DBusGMethodInvocation* context,
                                   const char* message) {
    GError* error = g_error_new_literal(DBUS_GERROR,
                                        DBUS_GERROR_INVALID_ARGS,
                                        message);
    DBusErrorReply* reply_cb = reply_factory_->NewErrorReply(context, error);
    event_source_.AddEvent(reply_cb);
}

bool Service::CleanUpStaleMounts(bool force) {
  // This function is meant to aid in a clean recovery from a crashed or
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
    for (; match != matches.end() && match->first == curr->first; ++match) {
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
    }

    // Delete anything that shouldn't be unmounted.
    if (keep) {
      std::multimap<const std::string, const std::string>::iterator it;
      for (it = curr; it != match; ++it)
        exclude.push_back(it->second);
      matches.erase(curr, match);
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

  // Pass in all the shared dependencies here rather than
  // needing to always get the Attestation object to set them
  // during testing.
  attestation_->Initialize(tpm_, platform_, crypto_, install_attrs_);

  // TODO(wad) Determine if this should only be called if
  //           tpm->IsEnabled() is true.
  if (tpm_ && initialize_tpm_) {
    tpm_init_->set_tpm(tpm_);
    tpm_init_->Init(this);
    if (!SeedUrandom()) {
      LOG(ERROR) << "FAILED TO SEED /dev/urandom AT START";
    }
    chromeos::SecureBlob password;
    if (tpm_init_->IsTpmReady() && tpm_init_->GetTpmPassword(&password))
      attestation_->PrepareForEnrollmentAsync();
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
      // We only report and init PKCS#11 for successful mounts.
      if (result->return_status()) {
        if (!result->return_code()) {
          timer_collection_->UpdateTimer(TimerCollection::kAsyncMountTimer,
                                         false);
        }
        // A return code of MOUNT_RECREATED will still need PKCS#11 init.
        InitializePkcs11(result->mount());
      }
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
  } if (!strcmp(event->GetEventName(), kDBusErrorReplyEventType)) {
    DBusErrorReply* result = static_cast<DBusErrorReply*>(event);
    result->Run();
  } if (!strcmp(event->GetEventName(), kDBusReplyEventType)) {
    DBusReply* result = static_cast<DBusReply*>(event);
    result->Run();
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

  // Do attestation work after AddEvent because it may take long.
  attestation_->PrepareForEnrollment();
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

void Service::DoCheckKeyEx(AccountIdentifier* identifier,
                           AuthorizationRequest* authorization,
                           CheckKeyRequest* check_key_request,
                           DBusGMethodInvocation* context) {
  if (!identifier || !authorization || !check_key_request) {
    SendInvalidArgsReply(context, "Failed to parse parameters.");
    return;
  }

  if (identifier->email().empty()) {
    SendInvalidArgsReply(context, "No email supplied");
    return;
  }

  // An AuthorizationRequest key without a label will test against
  // all VaultKeysets of a compatible key().data().type().
  if (authorization->key().secret().empty()) {
    SendInvalidArgsReply(context, "No key secret supplied");
    return;
  }

  UsernamePasskey credentials(identifier->email().c_str(),
                            SecureBlob(authorization->key().secret().c_str(),
                                       authorization->key().secret().length()));
  credentials.set_key_data(authorization->key().data());

  BaseReply reply;
  for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); ++it) {
    if (it->second->AreSameUser(credentials)) {
      if (!it->second->AreValid(credentials)) {
        // Fallthrough to HomeDirs to cover different keys for the same user.
        break;
      }
      SendReply(context, reply);
      return;
    }
  }

  if (!homedirs_->Exists(credentials)) {
    reply.set_error(CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
  } else if (!homedirs_->AreCredentialsValid(credentials)) {
    // TODO(wad) Should this pass along KEY_NOT_FOUND too?
    reply.set_error(CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
  }
  SendReply(context, reply);
}

gboolean Service::CheckKeyEx(GArray* account_id,
                             GArray* authorization_request,
                             GArray* check_key_request,
                             DBusGMethodInvocation *context) {
  scoped_ptr<AccountIdentifier> identifier(new AccountIdentifier);
  scoped_ptr<AuthorizationRequest> authorization(new AuthorizationRequest);
  scoped_ptr<CheckKeyRequest> request(new CheckKeyRequest);

  // On parsing failure, pass along a NULL.
  if (!identifier->ParseFromArray(account_id->data, account_id->len))
    identifier.reset(NULL);
  if (!authorization->ParseFromArray(authorization_request->data,
                                     authorization_request->len))
    authorization.reset(NULL);
  if (!request->ParseFromArray(check_key_request->data, check_key_request->len))
    request.reset(NULL);

  // If PBs don't parse, the validation in the handler will catch it.
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&Service::DoCheckKeyEx, base::Unretained(this),
                 base::Owned(identifier.release()),
                 base::Owned(authorization.release()),
                 base::Owned(request.release()),
                 base::Unretained(context)));
  return TRUE;
}

void Service::DoRemoveKeyEx(AccountIdentifier* identifier,
                            AuthorizationRequest* authorization,
                            RemoveKeyRequest* remove_key_request,
                            DBusGMethodInvocation* context) {
  if (!identifier || !authorization || !remove_key_request) {
    SendInvalidArgsReply(context, "Failed to parse parameters.");
    return;
  }

  if (identifier->email().empty()) {
    SendInvalidArgsReply(context, "No email supplied");
    return;
  }

  // An AuthorizationRequest key without a label will test against
  // all VaultKeysets of a compatible key().data().type().
  if (authorization->key().secret().empty()) {
    SendInvalidArgsReply(context, "No key secret supplied");
    return;
  }

  if (remove_key_request->key().data().label().empty()) {
    SendInvalidArgsReply(context, "No label provided for target key");
    return;
  }

  BaseReply reply;
  UsernamePasskey credentials(
      identifier->email().c_str(),
      SecureBlob(authorization->key().secret().c_str(),
                 authorization->key().secret().length()));
  credentials.set_key_data(authorization->key().data());

  if (!homedirs_->Exists(credentials)) {
    reply.set_error(CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    SendReply(context, reply);
    return;
  }

  reply.set_error(homedirs_->RemoveKeyset(credentials,
                                          remove_key_request->key().data()));
  if (reply.error() == CRYPTOHOME_ERROR_NOT_SET) {
    // Don't set the error if there wasn't one.
    reply.clear_error();
  }
  SendReply(context, reply);
}

gboolean Service::RemoveKeyEx(GArray* account_id,
                             GArray* authorization_request,
                             GArray* remove_key_request,
                             DBusGMethodInvocation *context) {
  scoped_ptr<AccountIdentifier> identifier(new AccountIdentifier);
  scoped_ptr<AuthorizationRequest> authorization(new AuthorizationRequest);
  scoped_ptr<RemoveKeyRequest> request(new RemoveKeyRequest);

  // On parsing failure, pass along a NULL.
  if (!identifier->ParseFromArray(account_id->data, account_id->len))
    identifier.reset(NULL);
  if (!authorization->ParseFromArray(authorization_request->data,
                                     authorization_request->len))
    authorization.reset(NULL);
  if (!request->ParseFromArray(remove_key_request->data,
                               remove_key_request->len))
    request.reset(NULL);

  // If PBs don't parse, the validation in the handler will catch it.
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&Service::DoRemoveKeyEx, base::Unretained(this),
                 base::Owned(identifier.release()),
                 base::Owned(authorization.release()),
                 base::Owned(request.release()),
                 base::Unretained(context)));
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

void Service::DoAddKeyEx(AccountIdentifier* identifier,
                         AuthorizationRequest* authorization,
                         AddKeyRequest* add_key_request,
                         DBusGMethodInvocation* context) {
  if (!identifier || !authorization || !add_key_request) {
    SendInvalidArgsReply(context, "Failed to parse parameters.");
    return;
  }

  // Setup a reply for use during error handling.
  BaseReply reply;

  if (identifier->email().empty()) {
    SendInvalidArgsReply(context, "No email supplied");
    return;
  }

  // An AuthorizationRequest key without a label will test against
  // all VaultKeysets of a compatible key().data().type().
  if (authorization->key().secret().empty()) {
    SendInvalidArgsReply(context, "No key secret supplied");
    return;
  }

  if (!add_key_request->has_key() || add_key_request->key().secret().empty()) {
    SendInvalidArgsReply(context, "No new key supplied");
    return;
  }

  if (add_key_request->key().data().label().empty()) {
    SendInvalidArgsReply(context, "No new key label supplied");
    return;
  }

  // Ensure any new keys do not contain a wrapped authorization key.
  for (int ad = 0;
       ad < add_key_request->key().data().authorization_data_size();
       ++ad) {
    const KeyAuthorizationData auth_data =
      add_key_request->key().data().authorization_data(ad);
    for (int s = 0; s < auth_data.secrets_size(); ++s) {
      if (auth_data.secrets(s).wrapped()) {
        // If wrapping becomes richer in the future, this may change.
        SendInvalidArgsReply(context,
                             "KeyAuthorizationSecrets may not be wrapped");
        return;
      }
    }
  }

  UsernamePasskey credentials(
      identifier->email().c_str(),
      SecureBlob(authorization->key().secret().c_str(),
                 authorization->key().secret().length()));
  credentials.set_key_data(authorization->key().data());

  if (!homedirs_->Exists(credentials)) {
    reply.set_error(CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    SendReply(context, reply);
    return;
  }

  int index = -1;
  SecureBlob new_secret(add_key_request->key().secret().c_str(),
                        add_key_request->key().secret().length());
  reply.set_error(homedirs_->AddKeyset(credentials,
                                       new_secret,
                                       &add_key_request->key().data(),
                                       add_key_request->clobber_if_exists(),
                                       &index));
  if (reply.error() == CRYPTOHOME_ERROR_NOT_SET) {
    // Don't set the error if there wasn't one.
    reply.clear_error();
  }
  SendReply(context, reply);
}

gboolean Service::AddKeyEx(GArray* account_id,
                           GArray* authorization_request,
                           GArray* add_key_request,
                           DBusGMethodInvocation *context) {
  scoped_ptr<AccountIdentifier> identifier(new AccountIdentifier);
  scoped_ptr<AuthorizationRequest> authorization(new AuthorizationRequest);
  scoped_ptr<AddKeyRequest> request(new AddKeyRequest);

  // On parsing failure, pass along a NULL.
  if (!identifier->ParseFromArray(account_id->data, account_id->len))
    identifier.reset(NULL);
  if (!authorization->ParseFromArray(authorization_request->data,
                                     authorization_request->len))
    authorization.reset(NULL);
  if (!request->ParseFromArray(add_key_request->data, add_key_request->len))
    request.reset(NULL);

  // If PBs don't parse, the validation in the handler will catch it.
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&Service::DoAddKeyEx, base::Unretained(this),
                 base::Owned(identifier.release()),
                 base::Owned(authorization.release()),
                 base::Owned(request.release()),
                 base::Unretained(context)));
  return TRUE;
}

void Service::DoUpdateKeyEx(AccountIdentifier* identifier,
                            AuthorizationRequest* authorization,
                            UpdateKeyRequest* update_key_request,
                            DBusGMethodInvocation* context) {
  if (!identifier || !authorization || !update_key_request) {
    SendInvalidArgsReply(context, "Failed to parse parameters.");
    return;
  }

  // Setup a reply for use during error handling.
  BaseReply reply;

  if (identifier->email().empty()) {
    SendInvalidArgsReply(context, "No email supplied");
    return;
  }

  // An AuthorizationRequest key without a label will test against
  // all VaultKeysets of a compatible key().data().type().
  if (authorization->key().secret().empty()) {
    SendInvalidArgsReply(context, "No key secret supplied");
    return;
  }

  // Any undefined field in changes() will be left as it is.
  if (!update_key_request->has_changes()) {
    SendInvalidArgsReply(context, "No updates requested");
    return;
  }

  for (int ad = 0;
       ad < update_key_request->changes().data().authorization_data_size();
       ++ad) {
    const KeyAuthorizationData auth_data =
      update_key_request->changes().data().authorization_data(ad);
    for (int s = 0; s < auth_data.secrets_size(); ++s) {
      if (auth_data.secrets(s).wrapped()) {
        // If wrapping becomes richer in the future, this may change.
        SendInvalidArgsReply(context,
                             "KeyAuthorizationSecrets may not be wrapped");
        return;
      }
    }
  }

  UsernamePasskey credentials(identifier->email().c_str(),
                            SecureBlob(authorization->key().secret().c_str(),
                                       authorization->key().secret().length()));
  credentials.set_key_data(authorization->key().data());

  if (!homedirs_->Exists(credentials)) {
    reply.set_error(CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    SendReply(context, reply);
    return;
  }

  reply.set_error(homedirs_->UpdateKeyset(
                      credentials,
                      &update_key_request->changes(),
                      update_key_request->authorization_signature()));
  if (reply.error() == CRYPTOHOME_ERROR_NOT_SET) {
    // Don't set the error if there wasn't one.
    reply.clear_error();
  }
  SendReply(context, reply);
}

gboolean Service::UpdateKeyEx(GArray* account_id,
                           GArray* authorization_request,
                           GArray* update_key_request,
                           DBusGMethodInvocation *context) {
  scoped_ptr<AccountIdentifier> identifier(new AccountIdentifier);
  scoped_ptr<AuthorizationRequest> authorization(new AuthorizationRequest);
  scoped_ptr<UpdateKeyRequest> request(new UpdateKeyRequest);

  // On parsing failure, pass along a NULL.
  if (!identifier->ParseFromArray(account_id->data, account_id->len))
    identifier.reset(NULL);
  if (!authorization->ParseFromArray(authorization_request->data,
                                     authorization_request->len))
    authorization.reset(NULL);
  if (!request->ParseFromArray(update_key_request->data,
                               update_key_request->len))
    request.reset(NULL);

  // If PBs don't parse, the validation in the handler will catch it.
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&Service::DoUpdateKeyEx, base::Unretained(this),
                 base::Owned(identifier.release()),
                 base::Owned(authorization.release()),
                 base::Owned(request.release()),
                 base::Unretained(context)));
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
  if (result.return_status()) {
    InitializePkcs11(result.mount());
  } else {
    RemoveMount(result.mount());
  }

  *OUT_error_code = result.return_code();
  *OUT_result = result.return_status();
  return TRUE;
}

void Service::DoMountEx(AccountIdentifier* identifier,
                        AuthorizationRequest* authorization,
                        MountRequest* request,
                        DBusGMethodInvocation *context) {
  if (!identifier || !authorization || !request) {
    SendInvalidArgsReply(context, "Failed to parse parameters.");
    return;
  }

  // Setup a reply for use during error handling.
  BaseReply reply;

  // Needed to pass along |recreated|
  MountReply* mount_reply = reply.MutableExtension(MountReply::reply);
  mount_reply->set_recreated(false);

  // See ::Mount for detailed commentary.
  if (mounts_.size() == 0)
    CleanUpStaleMounts(false);

  // At present, we only enforce non-empty email addresses.
  // In the future, we may wish to canonicalize if we don't move
  // to requiring a IdP-unique identifier.
  if (identifier->email().empty()) {
    SendInvalidArgsReply(context, "No email supplied");
    return;
  }

  // An AuthorizationRequest key without a label will test against
  // all VaultKeysets of a compatible key().data().type().
  if (authorization->key().secret().empty()) {
    SendInvalidArgsReply(context, "No key secret supplied");
    return;
  }

  if (request->has_create()) {
    if (request->create().copy_authorization_key()) {
      Key *auth_key = request->mutable_create()->add_keys();
      *auth_key = authorization->key();
      // Don't allow a key creation and mount if the key lacks
      // the privileges.
      if (!auth_key->data().privileges().mount()) {
        reply.set_error(CRYPTOHOME_ERROR_AUTHORIZATION_KEY_DENIED);
        SendReply(context, reply);
        return;
      }
    }
    int keys_size = request->create().keys_size();
    if (keys_size == 0) {
      SendInvalidArgsReply(context, "CreateRequest supplied with no keys");
      return;
    } else if (keys_size > 1) {
      LOG(INFO) << "MountEx: unimplemented CreateRequest with multiple keys";
      reply.set_error(CRYPTOHOME_ERROR_NOT_IMPLEMENTED);
      SendReply(context, reply);
      return;
    } else {
      const Key key = request->create().keys(0);
      // TODO(wad) Ensure the labels are all unique.
      if (key.secret().empty() || !key.has_data() ||
          key.data().label().empty()) {
        SendInvalidArgsReply(context,
                             "CreateRequest Keys are not fully specified");
        return;
      }
      // TODO(wad): Refactor out this check and other incoming Key validations
      //            in a helper.  crbug.com/353644
      for (int ad = 0; ad < key.data().authorization_data_size(); ++ad) {
        const KeyAuthorizationData auth_data =
          key.data().authorization_data(ad);
        for (int s = 0; s < auth_data.secrets_size(); ++s) {
          if (auth_data.secrets(s).wrapped()) {
          // If wrapping becomes richer in the future, this may change.
          SendInvalidArgsReply(context,
                               "KeyAuthorizationSecrets may not be wrapped");
          return;
          }
        }
      }
    }
  }

  UsernamePasskey credentials(identifier->email().c_str(),
                            SecureBlob(authorization->key().secret().c_str(),
                                       authorization->key().secret().length()));
  // Everything else can be the default.
  credentials.set_key_data(authorization->key().data());

  if (!request->has_create() && !homedirs_->Exists(credentials)) {
    reply.set_error(CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    SendReply(context, reply);
    return;
  }

  // While it would be cleaner to implement the privilege enforcement
  // here, that can only be done if a label was supplied.  If a wildcard
  // was supplied, then we can only perform the enforcement after the
  // matching key is identified.
  //
  // See Mount::MountCryptohome for privilege checking.

  scoped_refptr<cryptohome::Mount> guest_mount = GetMountForUser(guest_user_);
  bool guest_mounted = guest_mount.get() && guest_mount->IsMounted();
  // TODO(wad,ellyjones) Change this behavior to return failure even
  // on a succesful unmount to tell chrome MOUNT_ERROR_NEEDS_RESTART.
  if (guest_mounted && !guest_mount->UnmountCryptohome()) {
    LOG(ERROR) << "Could not unmount cryptohome from Guest session";
    reply.set_error(CRYPTOHOME_ERROR_MOUNT_MOUNT_POINT_BUSY);
    SendReply(context, reply);
    return;
  }

  // Don't overlay an ephemeral mount over a file-backed one.
  scoped_refptr<cryptohome::Mount> user_mount =
      GetOrCreateMountForUser(identifier->email());
  if (request->require_ephemeral() && user_mount->IsVaultMounted()) {
    // TODO(wad,ellyjones) Change this behavior to return failure even
    // on a succesful unmount to tell chrome MOUNT_ERROR_NEEDS_RESTART.
    if (!user_mount->UnmountCryptohome()) {
      LOG(ERROR) << "Could not unmount vault before an ephemeral mount.";
      reply.set_error(CRYPTOHOME_ERROR_MOUNT_MOUNT_POINT_BUSY);
      SendReply(context, reply);
      return;
    }
  }

  if (user_mount->IsMounted()) {
    LOG(INFO) << "Mount exists. Rechecking credentials.";
    // Attempt a short-circuited credential test.
    if (user_mount->AreSameUser(credentials) &&
        user_mount->AreValid(credentials)) {
      SendReply(context, reply);
      return;
    }
    // If the Mount has invalid credentials (repopulated from system state)
    // this will ensure a user can still sign-in with the right ones.
    // TODO(wad) Should we unmount on a failed re-mount attempt?
    if (!user_mount->AreValid(credentials) &&
        !homedirs_->AreCredentialsValid(credentials)) {
      reply.set_error(CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
    }
    SendReply(context, reply);
    return;
  }

  // See Mount for a relevant comment.
  if (install_attrs_->is_first_install()) {
    install_attrs_->Finalize();
  }

  // As per the other timers, this really only tracks time spent in
  // MountCryptohome() not in the other areas prior.
  timer_collection_->UpdateTimer(TimerCollection::kMountExTimer, true);
  MountError code = MOUNT_ERROR_NONE;
  Mount::MountArgs mount_args;
  mount_args.create_if_missing = request->has_create();
  mount_args.ensure_ephemeral = request->require_ephemeral();
  bool status = user_mount->MountCryptohome(credentials,
                                            mount_args,
                                            &code);
  user_mount->set_pkcs11_state(cryptohome::Mount::kUninitialized);

  // Provide an authoritative filesystem-sanitized username.
  mount_reply->set_sanitized_username(
      chromeos::cryptohome::home::SanitizeUserName(identifier->email()));

  // Mark the timer as done.
  timer_collection_->UpdateTimer(TimerCollection::kMountExTimer, false);
  if (!status) {
    reply.set_error(MountErrorToCryptohomeError(code));
  }
  if (code == MOUNT_ERROR_RECREATED) {
    mount_reply->set_recreated(true);
  }

  SendReply(context, reply);

  // Update user activity timestamp to be able to detect old users.
  // This action is not mandatory, so we perform it after
  // CryptohomeMount() returns, in background.
  user_mount->UpdateCurrentUserActivityTimestamp(0);
  // Time to push the task for PKCS#11 initialization.
  // TODO(wad) This call will PostTask back to the same thread. It is safe, but
  //           it seems pointless.
  InitializePkcs11(user_mount);
}

gboolean Service::MountEx(const GArray *account_id,
                          const GArray *authorization_request,
                          const GArray *mount_request,
                          DBusGMethodInvocation *context) {
  scoped_ptr<AccountIdentifier> identifier(new AccountIdentifier);
  scoped_ptr<AuthorizationRequest> authorization(new AuthorizationRequest);
  scoped_ptr<MountRequest> request(new MountRequest);

  // On parsing failure, pass along a NULL.
  if (!identifier->ParseFromArray(account_id->data, account_id->len))
    identifier.reset(NULL);
  if (!authorization->ParseFromArray(authorization_request->data,
                                     authorization_request->len))
    authorization.reset(NULL);
  if (!request->ParseFromArray(mount_request->data, mount_request->len))
    request.reset(NULL);

  // If PBs don't parse, the validation in the handler will catch it.
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&Service::DoMountEx, base::Unretained(this),
                 base::Owned(identifier.release()),
                 base::Owned(authorization.release()),
                 base::Owned(request.release()),
                 base::Unretained(context)));
  return TRUE;
}

void Service::SendLegacyAsyncReply(MountTaskMount* mount_task,
                                   MountError return_code,
                                   bool return_status) {
    MountTaskResult *result = new MountTaskResult(*mount_task->result());
    result->set_mount(mount_task->mount());
    result->set_return_code(return_code);
    result->set_return_status(return_status);
    event_source_.AddEvent(result);
    return;
}

// This function implements the _old_ style Mounts.  It should be removed
// once MountEx is used everywhere.
// Pass in the MountTaskMount so the async_id stays consistent.
void Service::DoAsyncMount(const std::string& userid,
                           SecureBlob *key,
                           bool public_mount,
                           MountTaskMount* mount_task) {
  // Clean up stale mounts if this is the only mount.
  if (mounts_.size() != 0 || CleanUpStaleMounts(false))  {
    // Don't proceed if there is any existing mount or stale mount.
    if (public_mount) {
      LOG(ERROR) << "Public mount requested with other mounts active.";
      PostAsyncCallResultForUser(userid, mount_task,
                                 MOUNT_ERROR_MOUNT_POINT_BUSY, false);
      return;
    }
  }

  if (public_mount) {
    std::string public_mount_passkey;
    if (!GetPublicMountPassKey(userid, &public_mount_passkey)) {
      LOG(ERROR) << "Could not get public mount passkey.";
      PostAsyncCallResultForUser(userid, mount_task,
                                 MOUNT_ERROR_KEY_FAILURE, false);
      return;
    }
    SecureBlob public_key(public_mount_passkey);
    key->swap(public_key);
    // Override the mount_task credentials with the public key.
    UsernamePasskey credentials(userid.c_str(), *key);
    mount_task->set_credentials(credentials);
  }

  scoped_refptr<cryptohome::Mount> guest_mount = GetMountForUser(guest_user_);
  mount_task->set_mount(guest_mount);
  bool guest_mounted = guest_mount.get() && guest_mount->IsMounted();
  // TODO(wad,ellyjones) Change this behavior to return failure even
  // on a succesful unmount to tell chrome MOUNT_ERROR_NEEDS_RESTART.
  if (guest_mounted && !guest_mount->UnmountCryptohome()) {
    LOG(ERROR) << "Could not unmount cryptohome from Guest session";
    SendLegacyAsyncReply(mount_task, MOUNT_ERROR_MOUNT_POINT_BUSY, false);
    return;
  }

  scoped_refptr<cryptohome::Mount> user_mount =
      GetOrCreateMountForUser(userid.c_str());
  // Any work from here will use the user_mount.
  mount_task->set_mount(user_mount);

  // Don't overlay an ephemeral mount over a file-backed one.
  const Mount::MountArgs mount_args = mount_task->mount_args();
  if (mount_args.ensure_ephemeral && user_mount->IsVaultMounted()) {
    // TODO(wad,ellyjones) Change this behavior to return failure even
    // on a succesful unmount to tell chrome MOUNT_ERROR_NEEDS_RESTART.
    if (!user_mount->UnmountCryptohome()) {
      LOG(ERROR) << "Could not unmount vault before an ephemeral mount.";
      SendLegacyAsyncReply(mount_task, MOUNT_ERROR_MOUNT_POINT_BUSY, false);
      return;
    }
  }

  UsernamePasskey credentials(userid.c_str(), *key);
  if (user_mount->IsMounted()) {
    LOG(INFO) << "Mount exists. Rechecking credentials.";
    // Attempt a short-circuited credential test.
    if (user_mount->AreSameUser(credentials) &&
        user_mount->AreValid(credentials)) {
      SendLegacyAsyncReply(mount_task, MOUNT_ERROR_NONE, true);
      return;
    }

    // If the Mount has invalid credentials (repopulated from system state)
    // this will ensure a user can still sign-in with the right ones.
    // TODO(wad) Should we unmount on a failed re-mount attempt?
    bool return_status = homedirs_->AreCredentialsValid(credentials);
    SendLegacyAsyncReply(mount_task, MOUNT_ERROR_NONE, return_status);

    // See comment in Service::Mount() above on why this is needed here.
    InitializePkcs11(user_mount.get());
    return;
  }

  // See Mount for a relevant comment.
  if (install_attrs_->is_first_install()) {
    MountTaskInstallAttrsFinalize *finalize =
        new MountTaskInstallAttrsFinalize(NULL, install_attrs_);
    mount_thread_.message_loop()->PostTask(FROM_HERE,
        base::Bind(&MountTaskInstallAttrsFinalize::Run, finalize));
  }

  timer_collection_->UpdateTimer(TimerCollection::kAsyncMountTimer, true);
  mount_task->result()->set_pkcs11_init(true);
  user_mount->set_pkcs11_state(cryptohome::Mount::kUninitialized);
  mount_task->Run();
  MountTaskResult *result = new MountTaskResult(*mount_task->result());
  event_source_.AddEvent(result);
  return;
}

gboolean Service::AsyncMount(const gchar *userid,
                             const gchar *key,
                             gboolean create_if_missing,
                             gboolean ensure_ephemeral,
                             DBusGMethodInvocation *context) {

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = create_if_missing;
  mount_args.ensure_ephemeral = ensure_ephemeral;
  scoped_ptr<SecureBlob> key_blob(new SecureBlob(key, strlen(key)));
  UsernamePasskey credentials(userid, *key_blob);
  scoped_refptr<MountTaskMount> mount_task = new MountTaskMount(
                                                            NULL,
                                                            NULL,
                                                            credentials,
                                                            mount_args);

  // Send the async_id before we do any real work.
  dbus_g_method_return(context, mount_task->sequence_id());

  LOG(INFO) << "Asynced Mount() requested. Tracking request sequence id "
            << mount_task->sequence_id()
            << " for later PKCS#11 initialization.";

  // Just pass the task and the args.
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&Service::DoAsyncMount, base::Unretained(this),
                 std::string(userid),
                 base::Owned(key_blob.release()),
                 false,
                 mount_task));


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
                                   DBusGMethodInvocation *context) {

  Mount::MountArgs mount_args;
  mount_args.create_if_missing = create_if_missing;
  mount_args.ensure_ephemeral = ensure_ephemeral;
  scoped_ptr<SecureBlob> key_blob(new SecureBlob());
  UsernamePasskey credentials(public_mount_id, *key_blob);
  scoped_refptr<MountTaskMount> mount_task = new MountTaskMount(
                                                            NULL,
                                                            NULL,
                                                            credentials,
                                                            mount_args);

  // Send the async_id before we do any real work.
  dbus_g_method_return(context, mount_task->sequence_id());


  // This should really call DoAsyncMount
  mount_thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&Service::DoAsyncMount, base::Unretained(this),
                 std::string(public_mount_id),
                 base::Owned(key_blob.release()),
                 true,
                 mount_task));
  return TRUE;
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
  *OUT_prepared = attestation_->IsPreparedForEnrollment();
  return TRUE;
}

gboolean Service::TpmVerifyAttestationData(gboolean* OUT_verified,
                                           GError** error) {
  *OUT_verified = attestation_->Verify();
  return TRUE;
}

gboolean Service::TpmVerifyEK(gboolean* OUT_verified, GError** error) {
  *OUT_verified = attestation_->VerifyEK();
  return TRUE;
}

gboolean Service::TpmAttestationCreateEnrollRequest(gint pca_type,
                                                    GArray** OUT_pca_request,
                                                    GError** error) {
  *OUT_pca_request = g_array_new(false, false, sizeof(SecureBlob::value_type));
  chromeos::SecureBlob blob;
  if (attestation_->CreateEnrollRequest(GetPCAType(pca_type), &blob))
    g_array_append_vals(*OUT_pca_request, &blob.front(), blob.size());
  return TRUE;
}

gboolean Service::AsyncTpmAttestationCreateEnrollRequest(gint pca_type,
                                                         gint* OUT_async_id,
                                                         GError** error) {
  AttestationTaskObserver* observer =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<CreateEnrollRequestTask> task =
      new CreateEnrollRequestTask(observer, attestation_,
                                  GetPCAType(pca_type));
  *OUT_async_id = task->sequence_id();
  mount_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CreateEnrollRequestTask::Run, task.get()));
  return TRUE;
}

gboolean Service::TpmAttestationEnroll(gint pca_type,
                                       GArray* pca_response,
                                       gboolean* OUT_success,
                                       GError** error) {
  chromeos::SecureBlob blob(pca_response->data, pca_response->len);
  *OUT_success = attestation_->Enroll(GetPCAType(pca_type), blob);
  return TRUE;
}

gboolean Service::AsyncTpmAttestationEnroll(gint pca_type,
                                            GArray* pca_response,
                                            gint* OUT_async_id,
                                            GError** error) {
  chromeos::SecureBlob blob(pca_response->data, pca_response->len);
  AttestationTaskObserver* observer =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<EnrollTask> task =
      new EnrollTask(observer, attestation_, GetPCAType(pca_type), blob);
  *OUT_async_id = task->sequence_id();
  mount_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&EnrollTask::Run, task.get()));
  return TRUE;
}

gboolean Service::TpmAttestationCreateCertRequest(gint pca_type,
                                                  gint certificate_profile,
                                                  gchar* username,
                                                  gchar* request_origin,
                                                  GArray** OUT_pca_request,
                                                  GError** error) {
  *OUT_pca_request = g_array_new(false, false, sizeof(SecureBlob::value_type));
  chromeos::SecureBlob blob;
  if (attestation_->CreateCertRequest(GetPCAType(pca_type),
                                      GetProfile(certificate_profile),
                                      username,
                                      request_origin,
                                      &blob))
    g_array_append_vals(*OUT_pca_request, &blob.front(), blob.size());
  return TRUE;
}

gboolean Service::AsyncTpmAttestationCreateCertRequest(
    gint pca_type,
    gint certificate_profile,
    gchar* username,
    gchar* request_origin,
    gint* OUT_async_id,
    GError** error) {
  AttestationTaskObserver* observer =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<CreateCertRequestTask> task =
      new CreateCertRequestTask(observer,
                                attestation_,
                                GetPCAType(pca_type),
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
                                                  gchar* username,
                                                  gchar* key_name,
                                                  GArray** OUT_cert,
                                                  gboolean* OUT_success,
                                                  GError** error) {
  *OUT_cert = g_array_new(false, false, sizeof(SecureBlob::value_type));
  chromeos::SecureBlob response_blob(pca_response->data, pca_response->len);
  chromeos::SecureBlob cert_blob;
  *OUT_success = attestation_->FinishCertRequest(response_blob,
                                                 is_user_specific,
                                                 username,
                                                 key_name,
                                                 &cert_blob);
  if (*OUT_success)
    g_array_append_vals(*OUT_cert, &cert_blob.front(), cert_blob.size());
  return TRUE;
}

gboolean Service::AsyncTpmAttestationFinishCertRequest(
    GArray* pca_response,
    gboolean is_user_specific,
    gchar* username,
    gchar* key_name,
    gint* OUT_async_id,
    GError** error) {
  chromeos::SecureBlob blob(pca_response->data, pca_response->len);
  AttestationTaskObserver* observer =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<FinishCertRequestTask> task =
      new FinishCertRequestTask(observer,
                                attestation_,
                                blob,
                                is_user_specific,
                                username,
                                key_name);
  *OUT_async_id = task->sequence_id();
  mount_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&FinishCertRequestTask::Run, task.get()));
  return TRUE;
}

gboolean Service::TpmIsAttestationEnrolled(gboolean* OUT_is_enrolled,
                                           GError** error) {
  *OUT_is_enrolled = attestation_->IsEnrolled();
  return TRUE;
}

gboolean Service::TpmAttestationDoesKeyExist(gboolean is_user_specific,
                                             gchar* username,
                                             gchar* key_name,
                                             gboolean *OUT_exists,
                                             GError** error) {
  *OUT_exists = attestation_->DoesKeyExist(is_user_specific,
                                           username,
                                           key_name);
  return TRUE;
}

gboolean Service::TpmAttestationGetCertificate(gboolean is_user_specific,
                                               gchar* username,
                                               gchar* key_name,
                                               GArray **OUT_certificate,
                                               gboolean* OUT_success,
                                               GError** error) {
  *OUT_certificate = g_array_new(false, false, sizeof(SecureBlob::value_type));
  chromeos::SecureBlob blob;
  *OUT_success = attestation_->GetCertificateChain(is_user_specific,
                                                   username,
                                                   key_name,
                                                   &blob);
  if (*OUT_success)
    g_array_append_vals(*OUT_certificate, &blob.front(), blob.size());
  return TRUE;
}

gboolean Service::TpmAttestationGetPublicKey(gboolean is_user_specific,
                                             gchar* username,
                                             gchar* key_name,
                                             GArray **OUT_public_key,
                                             gboolean* OUT_success,
                                             GError** error) {
  *OUT_public_key = g_array_new(false, false, sizeof(SecureBlob::value_type));
  chromeos::SecureBlob blob;
  *OUT_success = attestation_->GetPublicKey(is_user_specific,
                                            username,
                                            key_name,
                                            &blob);
  if (*OUT_success)
    g_array_append_vals(*OUT_public_key, &blob.front(), blob.size());
  return TRUE;
}

gboolean Service::TpmAttestationRegisterKey(gboolean is_user_specific,
                                            gchar* username,
                                            gchar* key_name,
                                            gint *OUT_async_id,
                                            GError** error) {
  AttestationTaskObserver* observer =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<RegisterKeyTask> task =
      new RegisterKeyTask(observer,
                          attestation_,
                          is_user_specific,
                          username,
                          key_name);
  *OUT_async_id = task->sequence_id();
  mount_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&RegisterKeyTask::Run, task.get()));
  return TRUE;
}

gboolean Service::TpmAttestationSignEnterpriseChallenge(
      gboolean is_user_specific,
      gchar* username,
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
                            attestation_,
                            is_user_specific,
                            username,
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
      gchar* username,
      gchar* key_name,
      GArray* challenge,
      gint *OUT_async_id,
      GError** error) {
  chromeos::SecureBlob challenge_blob(challenge->data, challenge->len);
  AttestationTaskObserver* observer =
      new MountTaskObserverBridge(NULL, &event_source_);
  scoped_refptr<SignChallengeTask> task =
      new SignChallengeTask(observer,
                            attestation_,
                            is_user_specific,
                            username,
                            key_name,
                            challenge_blob);
  *OUT_async_id = task->sequence_id();
  mount_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SignChallengeTask::Run, task.get()));
  return TRUE;
}

gboolean Service::TpmAttestationGetKeyPayload(gboolean is_user_specific,
                                              gchar* username,
                                              gchar* key_name,
                                              GArray** OUT_payload,
                                              gboolean* OUT_success,
                                              GError** error) {
  *OUT_payload = g_array_new(false, false, sizeof(SecureBlob::value_type));
  chromeos::SecureBlob blob;
  *OUT_success = attestation_->GetKeyPayload(is_user_specific,
                                             username,
                                             key_name,
                                             &blob);
  if (*OUT_success)
    g_array_append_vals(*OUT_payload, &blob.front(), blob.size());
  return TRUE;
}

gboolean Service::TpmAttestationSetKeyPayload(gboolean is_user_specific,
                                              gchar* username,
                                              gchar* key_name,
                                              GArray* payload,
                                              gboolean* OUT_success,
                                              GError** error) {
  chromeos::SecureBlob blob(payload->data, payload->len);
  *OUT_success = attestation_->SetKeyPayload(is_user_specific,
                                             username,
                                             key_name,
                                             blob);
  return TRUE;
}

gboolean Service::TpmAttestationDeleteKeys(gboolean is_user_specific,
                                           gchar* username,
                                           gchar* key_prefix,
                                           gboolean* OUT_success,
                                           GError** error) {
  *OUT_success = attestation_->DeleteKeysByPrefix(is_user_specific,
                                                  username,
                                                  key_prefix);
  return TRUE;
}

gboolean Service::TpmAttestationGetEK(gchar** OUT_ek_info,
                                      gboolean* OUT_success,
                                      GError** error) {
  std::string ek_info;
  *OUT_success = attestation_->GetEKInfo(&ek_info);
  *OUT_ek_info = g_strndup(ek_info.data(), ek_info.size());
  return TRUE;
}

gboolean Service::TpmAttestationResetIdentity(gchar* reset_token,
                                              GArray** OUT_reset_request,
                                              gboolean* OUT_success,
                                              GError** error) {
  *OUT_reset_request = g_array_new(false, false,
                                   sizeof(SecureBlob::value_type));
  chromeos::SecureBlob reset_request;
  *OUT_success = attestation_->GetIdentityResetRequest(
      std::string(reinterpret_cast<char*>(reset_token)),
      &reset_request);
  if (*OUT_success)
    g_array_append_vals(*OUT_reset_request,
                        &reset_request.front(),
                        reset_request.size());
  return TRUE;
}

// Returns true if all Pkcs11 tokens are ready.
gboolean Service::Pkcs11IsTpmTokenReady(gboolean* OUT_ready, GError** error) {
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
                                        gint* OUT_slot,
                                        GError** error) {
  pkcs11_init_->GetTpmTokenInfo(OUT_label, OUT_user_pin);
  *OUT_slot = -1;
  CK_SLOT_ID slot;
  if (pkcs11_init_->GetTpmTokenSlotForPath(FilePath(kChapsSystemToken), &slot))
    *OUT_slot = slot;
  return TRUE;
}

gboolean Service::Pkcs11GetTpmTokenInfoForUser(gchar* username,
                                               gchar** OUT_label,
                                               gchar** OUT_user_pin,
                                               gint* OUT_slot,
                                               GError** error) {
  pkcs11_init_->GetTpmTokenInfoForUser(username, OUT_label, OUT_user_pin);
  *OUT_slot = -1;
  CK_SLOT_ID slot;
  FilePath token_path = homedirs_->GetChapsTokenDir(username);
  if (pkcs11_init_->GetTpmTokenSlotForPath(token_path, &slot))
    *OUT_slot = slot;
  return TRUE;
}

gboolean Service::Pkcs11Terminate(gchar* username, GError **error) {
  for (MountMap::iterator it = mounts_.begin(); it != mounts_.end(); ++it) {
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

gboolean Service::StoreEnrollmentState(GArray* enrollment_state,
                                       gboolean* OUT_success,
                                       GError** error) {
  *OUT_success = false;
  if (!enterprise_owned_) {
    LOG(ERROR) << "Not preserving enrollment state as we are not enrolled.";
    return TRUE;
  }
  SecureBlob data_blob(enrollment_state->data, enrollment_state->len);
  std::string encrypted_data;
  if (!crypto_->EncryptWithTpm(data_blob, &encrypted_data)) {
    return TRUE;
  }
  if (!platform_->WriteStringToFile(kPreservedEnrollmentStatePath,
                                    encrypted_data)) {
    LOG(ERROR) << "Failed to write out enrollment state to "
               << kPreservedEnrollmentStatePath;
    return TRUE;
  }
  *OUT_success = true;
  return TRUE;
}

gboolean Service::LoadEnrollmentState(GArray** OUT_enrollment_state,
                                      gboolean* OUT_success,
                                      GError** error) {
  *OUT_success = false;
  chromeos::Blob enrollment_blob;
  if (!platform_->ReadFile(kPreservedEnrollmentStatePath,
                           &enrollment_blob)) {
    LOG(ERROR) << "Failed to read out enrollment state from "
               << kPreservedEnrollmentStatePath;
    return TRUE;
  }
  std::string enrollment_string(
      reinterpret_cast<const char*>(&enrollment_blob.front()),
      enrollment_blob.size());
  SecureBlob secure_data;
  if (!crypto_->DecryptWithTpm(enrollment_string,
                               &secure_data)) {
    return TRUE;
  }
  *OUT_enrollment_state = g_array_new(false, false, 1);
  g_array_append_vals(*OUT_enrollment_state,
                      reinterpret_cast<const char*>(&secure_data[0]),
                      secure_data.size());
  *OUT_success = true;
  return TRUE;
}

gboolean Service::GetStatusString(gchar** OUT_status, GError** error) {
  base::DictionaryValue dv;
  base::ListValue* mounts = new base::ListValue();
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
    homedirs_->set_enterprise_owned(true);
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

void Service::PostAsyncCallResultForUser(const std::string& user_id,
                                         MountTaskMount* mount_task,
                                         MountError return_code,
                                         bool return_status) {
  // Create a ref-counted mount for async use and then throw it away.
  scoped_refptr<cryptohome::Mount> mount = GetOrCreateMountForUser(user_id);
  mount_task->set_mount(GetOrCreateMountForUser(user_id));
  // Drop it from the map now that the MountTask has a ref.
  if (!RemoveMountForUser(user_id))
    LOG(ERROR) << "Unexpectedly cannot drop unused mount from map.";

  SendLegacyAsyncReply(mount_task, return_code, return_status);
}

void Service::DispatchEvents() {
  event_source_.HandleDispatch();
}

}  // namespace cryptohome
