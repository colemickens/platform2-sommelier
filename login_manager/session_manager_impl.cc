// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_impl.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <iterator>
#include <locale>
#include <memory>
#include <string>
#include <utility>

#include <base/base64.h>
#include <base/bind.h>
#include <base/callback_helpers.h>
#include <base/files/file_util.h>
#include <base/message_loop/message_loop.h>
#include <base/rand_util.h>
#include <base/run_loop.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_tokenizer.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>
#include <brillo/cryptohome.h>
#include <brillo/dbus/dbus_object.h>
#include <chromeos/dbus/service_constants.h>
#include <crypto/scoped_nss_types.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>
#include <install_attributes/libinstallattributes.h>

#include "bindings/chrome_device_policy.pb.h"
#include "bindings/device_management_backend.pb.h"
#include "login_manager/blob_util.h"
#include "login_manager/crossystem.h"
#include "login_manager/dbus_util.h"
#include "login_manager/device_local_account_manager.h"
#include "login_manager/device_policy_service.h"
#include "login_manager/init_daemon_controller.h"
#include "login_manager/key_generator.h"
#include "login_manager/login_metrics.h"
#include "login_manager/nss_util.h"
#include "login_manager/policy_key.h"
#include "login_manager/policy_service.h"
#include "login_manager/process_manager_service_interface.h"
#include "login_manager/proto_bindings/arc.pb.h"
#include "login_manager/proto_bindings/policy_descriptor.pb.h"
#include "login_manager/regen_mitigator.h"
#include "login_manager/system_utils.h"
#include "login_manager/termina_manager_interface.h"
#include "login_manager/user_policy_service_factory.h"
#include "login_manager/validator_utils.h"
#include "login_manager/vpd_process.h"

using base::FilePath;
using brillo::cryptohome::home::GetHashedUserPath;
using brillo::cryptohome::home::GetRootPath;
using brillo::cryptohome::home::GetUserPath;
using brillo::cryptohome::home::kGuestUserName;
using brillo::cryptohome::home::SanitizeUserName;

namespace login_manager {  // NOLINT

constexpr char SessionManagerImpl::kStarted[] = "started";
constexpr char SessionManagerImpl::kStopping[] = "stopping";
constexpr char SessionManagerImpl::kStopped[] = "stopped";

constexpr char SessionManagerImpl::kLoggedInFlag[] =
    "/run/session_manager/logged_in";
constexpr char SessionManagerImpl::kResetFile[] =
    "/mnt/stateful_partition/factory_install_reset";
constexpr char SessionManagerImpl::kTPMFirmwareUpdateAvailableFile[] =
    "/run/tpm_firmware_update_available";

constexpr char SessionManagerImpl::kTPMFirmwareUpdateParamsVPDKey[] =
    "tpm_firmware_update_params";

constexpr char SessionManagerImpl::kStartUserSessionImpulse[] =
    "start-user-session";

constexpr char SessionManagerImpl::kArcContainerName[] = "android";
constexpr char SessionManagerImpl::kArcBridgeSocketPath[] =
    "/run/chrome/arc_bridge.sock";
constexpr char SessionManagerImpl::kArcBridgeSocketGroup[] = "arc-bridge";

// ARC related impulse (systemd unit start or Upstart signal).
constexpr char SessionManagerImpl::kStartArcInstanceForLoginScreenImpulse[] =
    "start-arc-instance-for-login-screen";
constexpr char SessionManagerImpl::kStartArcInstanceImpulse[] =
    "start-arc-instance";
constexpr char SessionManagerImpl::kStopArcInstanceImpulse[] =
    "stop-arc-instance";
constexpr char SessionManagerImpl::kContinueArcBootImpulse[] =
    "continue-arc-boot";
constexpr char SessionManagerImpl::kStartArcNetworkImpulse[] =
    "start-arc-network";
constexpr char SessionManagerImpl::kStopArcNetworkImpulse[] =
    "stop-arc-network";
constexpr char SessionManagerImpl::kArcBootedImpulse[] = "arc-booted";
constexpr char SessionManagerImpl::kRemoveOldArcDataImpulse[] =
    "remove-old-arc-data";

// TODO(b:66919195): Optimize Android master container shutdown time. It
// needs as long as 3s on kevin to perform graceful shutdown.
constexpr base::TimeDelta SessionManagerImpl::kContainerTimeout =
    base::TimeDelta::FromSeconds(USE_ANDROID_MASTER_CONTAINER ? 3 : 1);

namespace {

// Error message emitted when parsing a PolicyDescriptor proto fails.
constexpr char kDescriptorParsingFailed[] = "PolicyDescriptor parsing failed.";

// Error message emitted when encountering an invalid PolicyDescriptor.
constexpr char kDescriptorInvalid[] = "PolicyDescriptor invalid.";

// Characters allowed in a container name.
constexpr char kContainerNameAllowedChars[] =
    "0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "+-_.";
// Characters allowed in a container path.
constexpr char kContainerPathAllowedChars[] =
    "0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "+-_./";

// The flag to pass to chrome to open a named socket for testing.
const char kTestingChannelFlag[] = "--testing-channel=NamedTestingInterface:";

// Device-local account state directory.
const base::FilePath::CharType kDeviceLocalAccountStateDir[] =
    FILE_PATH_LITERAL("/var/lib/device_local_accounts");

#if USE_CHEETS
// To launch ARC, certain amount of free disk space is needed.
// Path and the amount for the check.
constexpr base::FilePath::CharType kArcDiskCheckPath[] = "/home";
constexpr int64_t kArcCriticalDiskFreeBytes = 64 << 20;  // 64MB
constexpr size_t kArcContainerInstanceIdLength = 16;

// Name of android-data directory.
const base::FilePath::CharType kAndroidDataDirName[] =
    FILE_PATH_LITERAL("android-data");

// Name of android-data-old directory which RemoveArcDataInternal uses.
const base::FilePath::CharType kAndroidDataOldDirName[] =
    FILE_PATH_LITERAL("android-data-old");

// To set the CPU limits of the Android container.
const char kCpuSharesFile[] =
    "/sys/fs/cgroup/cpu/session_manager_containers/cpu.shares";
const unsigned int kCpuSharesForeground = 1024;
const unsigned int kCpuSharesBackground = 64;
#endif

// The interval used to periodically check if time sync was done by tlsdated.
constexpr base::TimeDelta kSystemClockLastSyncInfoRetryDelay =
    base::TimeDelta::FromMilliseconds(1000);

// TPM firmware update modes.
constexpr char kTPMFirmwareUpdateModeRecovery[] = "recovery";
constexpr char kTPMFirmwareUpdateModeFirstBoot[] = "first_boot";

// Policy storage constants.
constexpr char kEmptyAccountId[] = "";
constexpr char kCannotGetPolicyServiceFormat[] =
    "Cannot get policy service for account type %i";
constexpr char kSigEncodeFailMessage[] = "Failed to retrieve policy data.";

const char* ToSuccessSignal(bool success) {
  return success ? "success" : "failure";
}

#if USE_CHEETS
bool IsDevMode(SystemUtils* system) {
  // When GetDevModeState() returns UNKNOWN, return true.
  return system->GetDevModeState() != DevModeState::DEV_MODE_OFF;
}

bool IsInsideVm(SystemUtils* system) {
  // When GetVmState() returns UNKNOWN, return false.
  return system->GetVmState() == VmState::INSIDE_VM;
}
#endif

// TODO(crbug.com/765644): This and all users of this method will be removed
// when Chrome has been switched to the new 'Ex' interface.
std::vector<uint8_t> MakePolicyDescriptor(PolicyAccountType account_type,
                                          const std::string& account_id) {
  PolicyDescriptor descriptor;
  descriptor.set_account_type(account_type);
  descriptor.set_account_id(account_id);
  descriptor.set_domain(POLICY_DOMAIN_CHROME);
  return StringToBlob(descriptor.SerializeAsString());
}

}  // namespace

// Tracks D-Bus service running.
// Create*Callback functions return a callback adaptor from given
// DBusMethodResponse. These cancel in-progress operations when the instance is
// deleted.
class SessionManagerImpl::DBusService {
 public:
  explicit DBusService(org::chromium::SessionManagerInterfaceAdaptor* adaptor)
      : adaptor_(adaptor), weak_ptr_factory_(this) {}
  ~DBusService() = default;

  bool Start(const scoped_refptr<dbus::Bus>& bus) {
    DCHECK(!dbus_object_);

    // Registers the SessionManagerInterface D-Bus methods and signals.
    dbus_object_ = std::make_unique<brillo::dbus_utils::DBusObject>(
        nullptr, bus,
        org::chromium::SessionManagerInterfaceAdaptor::GetObjectPath());
    adaptor_->RegisterWithDBusObject(dbus_object_.get());
    dbus_object_->RegisterAndBlock();

    // Note that this needs to happen *after* all methods are exported
    // (http://crbug.com/331431).
    // This should pass dbus::Bus::REQUIRE_PRIMARY once on the new libchrome.
    return bus->RequestOwnershipAndBlock(kSessionManagerServiceName,
                                         dbus::Bus::REQUIRE_PRIMARY);
  }

  // Adaptor from DBusMethodResponse to PolicyService::Completion callback.
  PolicyService::Completion CreatePolicyServiceCompletionCallback(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response) {
    return base::Bind(&DBusService::HandlePolicyServiceCompletion,
                      weak_ptr_factory_.GetWeakPtr(), base::Passed(&response));
  }

  // Adaptor from DBusMethodResponse to
  // ServerBackedStateKeyGenerator::StateKeyCallback callback.
  ServerBackedStateKeyGenerator::StateKeyCallback CreateStateKeyCallback(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          std::vector<std::vector<uint8_t>>>> response) {
    return base::Bind(&DBusService::HandleStateKeyCallback,
                      weak_ptr_factory_.GetWeakPtr(), base::Passed(&response));
  }

 private:
  void HandlePolicyServiceCompletion(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
      brillo::ErrorPtr error) {
    if (error) {
      response->ReplyWithError(error.get());
      return;
    }

    response->Return();
  }

  void HandleStateKeyCallback(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          std::vector<std::vector<uint8_t>>>> response,
      const std::vector<std::vector<uint8_t>>& state_key) {
    response->Return(std::move(state_key));
  }

  org::chromium::SessionManagerInterfaceAdaptor* const adaptor_;
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;

  base::WeakPtrFactory<DBusService> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DBusService);
};

struct SessionManagerImpl::UserSession {
 public:
  UserSession(const std::string& username,
              const std::string& userhash,
              bool is_incognito,
              crypto::ScopedPK11Slot slot,
              std::unique_ptr<PolicyService> policy_service)
      : username(username),
        userhash(userhash),
        is_incognito(is_incognito),
        slot(std::move(slot)),
        policy_service(std::move(policy_service)) {}
  ~UserSession() {}

  const std::string username;
  const std::string userhash;
  const bool is_incognito;
  crypto::ScopedPK11Slot slot;
  std::unique_ptr<PolicyService> policy_service;
};

SessionManagerImpl::SessionManagerImpl(
    Delegate* delegate,
    std::unique_ptr<InitDaemonController> init_controller,
    const scoped_refptr<dbus::Bus>& bus,
    KeyGenerator* key_gen,
    ServerBackedStateKeyGenerator* state_key_generator,
    ProcessManagerServiceInterface* manager,
    LoginMetrics* metrics,
    NssUtil* nss,
    SystemUtils* utils,
    Crossystem* crossystem,
    VpdProcess* vpd_process,
    PolicyKey* owner_key,
    ContainerManagerInterface* android_container,
    TerminaManagerInterface* termina_manager,
    InstallAttributesReader* install_attributes_reader,
    dbus::ObjectProxy* component_updater_proxy,
    dbus::ObjectProxy* system_clock_proxy)
    : session_started_(false),
      session_stopping_(false),
      screen_locked_(false),
      supervised_user_creation_ongoing_(false),
      system_clock_synchronized_(false),
      init_controller_(std::move(init_controller)),
      system_clock_last_sync_info_retry_delay_(
          kSystemClockLastSyncInfoRetryDelay),
      bus_(bus),
      adaptor_(this),
      delegate_(delegate),
      key_gen_(key_gen),
      state_key_generator_(state_key_generator),
      manager_(manager),
      login_metrics_(metrics),
      nss_(nss),
      system_(utils),
      crossystem_(crossystem),
      vpd_process_(vpd_process),
      owner_key_(owner_key),
      android_container_(android_container),
      termina_manager_(termina_manager),
      install_attributes_reader_(install_attributes_reader),
      component_updater_proxy_(component_updater_proxy),
      system_clock_proxy_(system_clock_proxy),
      mitigator_(key_gen),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
}

SessionManagerImpl::~SessionManagerImpl() {
  device_policy_->set_delegate(NULL);  // Could use WeakPtr instead?
}

#if USE_CHEETS
// static
bool SessionManagerImpl::ValidateStartArcInstanceRequest(
    const StartArcInstanceRequest& request, brillo::ErrorPtr* error) {
  if (request.for_login_screen()) {
    // If this request is for login screen, following params are just
    // irrelevant so no value should be passed.
    if (request.has_account_id() ||
        request.has_skip_boot_completed_broadcast() ||
        request.has_scan_vendor_priv_app()) {
      *error = CreateError(DBUS_ERROR_INVALID_ARGS,
                           "StartArcInstanceRquest has invalid argument(s).");
      return false;
    }
  } else {
    // If this request is after user sign in, following params are required.
    if (!request.has_account_id() ||
        !request.has_skip_boot_completed_broadcast() ||
        !request.has_scan_vendor_priv_app()) {
      *error = CreateError(
          DBUS_ERROR_INVALID_ARGS,
          "StartArcInstanceRequest has required argument(s) missing.");
      return false;
    }
  }

  // All checks passed.
  return true;
}

// static
base::FilePath SessionManagerImpl::GetAndroidDataDirForUser(
    const std::string& normalized_account_id) {
  return GetRootPath(normalized_account_id).Append(kAndroidDataDirName);
}

// static
base::FilePath SessionManagerImpl::GetAndroidDataOldDirForUser(
    const std::string& normalized_account_id) {
  return GetRootPath(normalized_account_id).Append(kAndroidDataOldDirName);
}
#endif  // USE_CHEETS

void SessionManagerImpl::SetPolicyServicesForTesting(
    std::unique_ptr<DevicePolicyService> device_policy,
    std::unique_ptr<UserPolicyServiceFactory> user_policy_factory,
    std::unique_ptr<DeviceLocalAccountManager> device_local_account_manager) {
  device_policy_ = std::move(device_policy);
  user_policy_factory_ = std::move(user_policy_factory);
  device_local_account_manager_ = std::move(device_local_account_manager);
}

void SessionManagerImpl::AnnounceSessionStoppingIfNeeded() {
  if (session_started_) {
    session_stopping_ = true;
    DLOG(INFO) << "emitting D-Bus signal SessionStateChanged:" << kStopping;
    adaptor_.SendSessionStateChangedSignal(kStopping);
  }
}

void SessionManagerImpl::AnnounceSessionStopped() {
  session_stopping_ = session_started_ = false;
  DLOG(INFO) << "emitting D-Bus signal SessionStateChanged:" << kStopped;
  adaptor_.SendSessionStateChangedSignal(kStopped);
}

bool SessionManagerImpl::ShouldEndSession() {
  return screen_locked_ || supervised_user_creation_ongoing_;
}

bool SessionManagerImpl::Initialize() {
  key_gen_->set_delegate(this);

  system_clock_proxy_->WaitForServiceToBeAvailable(
      base::Bind(&SessionManagerImpl::OnSystemClockServiceAvailable,
                 weak_ptr_factory_.GetWeakPtr()));

  // Note: If SetPolicyServicesForTesting has been called, all services have
  // already been set and initialized.
  if (!device_policy_) {
    device_policy_ =
        DevicePolicyService::Create(owner_key_, login_metrics_, &mitigator_,
                                    nss_, crossystem_, vpd_process_);
    // Thinking about combining set_delegate() with the 'else' block below and
    // moving it down? Note that device_policy_->Initialize() might call
    // OnKeyPersisted() on the delegate, so be sure it's safe.
    device_policy_->set_delegate(this);
    if (!device_policy_->Initialize())
      return false;

    DCHECK(!user_policy_factory_);
    user_policy_factory_ =
        std::make_unique<UserPolicyServiceFactory>(getuid(), nss_, system_);

    device_local_account_manager_ = std::make_unique<DeviceLocalAccountManager>(
        base::FilePath(kDeviceLocalAccountStateDir), owner_key_),
    device_local_account_manager_->UpdateDeviceSettings(
        device_policy_->GetSettings());
    if (device_policy_->MayUpdateSystemSettings())
      device_policy_->UpdateSystemSettings(PolicyService::Completion());
  } else {
    device_policy_->set_delegate(this);
  }

  return true;
}

void SessionManagerImpl::Finalize() {
  // Reset the SessionManagerDBusAdaptor first to ensure that it'll permit
  // any outstanding DBusMethodCompletion objects to be abandoned without
  // having been run (http://crbug.com/638774, http://crbug.com/725734).
  dbus_service_.reset();

  device_policy_->PersistAllPolicy();
  for (const auto& kv : user_sessions_) {
    if (kv.second)
      kv.second->policy_service->PersistAllPolicy();
  }
  device_local_account_manager_->PersistAllPolicy();

  // We want to stop all running containers and VMs.  Containers and VMs are
  // per-session and cannot persist across sessions.
  android_container_->RequestJobExit();
  android_container_->EnsureJobExit(kContainerTimeout);
  termina_manager_->RequestJobExit();
  termina_manager_->EnsureJobExit(kContainerTimeout);
}

bool SessionManagerImpl::StartDBusService() {
  DCHECK(!dbus_service_);
  auto dbus_service = std::make_unique<DBusService>(&adaptor_);
  if (!dbus_service->Start(bus_))
    return false;

  dbus_service_ = std::move(dbus_service);
  return true;
}

void SessionManagerImpl::EmitLoginPromptVisible() {
  login_metrics_->RecordStats("login-prompt-visible");
  adaptor_.SendLoginPromptVisibleSignal();
  init_controller_->TriggerImpulse("login-prompt-visible", {},
                                   InitDaemonController::TriggerMode::ASYNC);
}

bool SessionManagerImpl::EnableChromeTesting(
    brillo::ErrorPtr* error,
    bool in_force_relaunch,
    const std::vector<std::string>& in_extra_arguments,
    std::string* out_filepath) {
  // Check to see if we already have Chrome testing enabled.
  bool already_enabled = !chrome_testing_path_.empty();

  if (!already_enabled) {
    base::FilePath temp_file_path;  // So we don't clobber chrome_testing_path_;
    if (!system_->GetUniqueFilenameInWriteOnlyTempDir(&temp_file_path)) {
      *error = CreateError(dbus_error::kTestingChannelError,
                           "Could not create testing channel filename.");
      return false;
    }
    chrome_testing_path_ = temp_file_path;
  }

  if (!already_enabled || in_force_relaunch) {
    // Delete testing channel file if it already exists.
    system_->RemoveFile(chrome_testing_path_);

    // Add testing channel argument to extra arguments.
    std::string testing_argument = kTestingChannelFlag;
    testing_argument.append(chrome_testing_path_.value());
    std::vector<std::string> extra_args = in_extra_arguments;
    extra_args.push_back(testing_argument);
    manager_->RestartBrowserWithArgs(extra_args, true);
  }
  *out_filepath = chrome_testing_path_.value();
  return true;
}

bool SessionManagerImpl::StartSession(brillo::ErrorPtr* error,
                                      const std::string& in_account_id,
                                      const std::string& in_unique_identifier) {
  std::string actual_account_id;
  if (!NormalizeAccountId(in_account_id, &actual_account_id, error)) {
    DCHECK(*error);
    return false;
  }

  // Check if this user already started a session.
  if (user_sessions_.count(actual_account_id) > 0) {
    constexpr char kMessage[] = "Provided user id already started a session.";
    LOG(ERROR) << kMessage;
    *error = CreateError(dbus_error::kSessionExists, kMessage);
    return false;
  }

  // Create a UserSession object for this user.
  const bool is_incognito = IsIncognitoAccountId(actual_account_id);
  auto user_session = CreateUserSession(actual_account_id, is_incognito, error);
  if (!user_session) {
    DCHECK(*error);
    return false;
  }

  // Check whether the current user is the owner, and if so make sure they are
  // whitelisted and have an owner key.
  bool user_is_owner = false;
  if (!device_policy_->CheckAndHandleOwnerLogin(user_session->username,
                                                user_session->slot.get(),
                                                &user_is_owner, error)) {
    DCHECK(*error);
    return false;
  }

  // If all previous sessions were incognito (or no previous sessions exist).
  bool is_first_real_user = AllSessionsAreIncognito() && !is_incognito;

  // Send each user login event to UMA (right before we start session
  // since the metrics library does not log events in guest mode).
  const DevModeState dev_mode_state = system_->GetDevModeState();
  if (dev_mode_state != DevModeState::DEV_MODE_UNKNOWN) {
    login_metrics_->SendLoginUserType(
        dev_mode_state != DevModeState::DEV_MODE_OFF, is_incognito,
        user_is_owner);
  }

  init_controller_->TriggerImpulse(kStartUserSessionImpulse,
                                   {"CHROMEOS_USER=" + actual_account_id},
                                   InitDaemonController::TriggerMode::ASYNC);
  LOG(INFO) << "Starting user session";
  manager_->SetBrowserSessionForUser(actual_account_id, user_session->userhash);
  session_started_ = true;
  user_sessions_[actual_account_id] = std::move(user_session);
  DLOG(INFO) << "emitting D-Bus signal SessionStateChanged:" << kStarted;
  adaptor_.SendSessionStateChangedSignal(kStarted);

  // Active Directory managed devices are not expected to have a policy key.
  // Don't create one for them.
  const bool is_active_directory =
      install_attributes_reader_->GetAttribute(
          InstallAttributesReader::kAttrMode) ==
      InstallAttributesReader::kDeviceModeEnterpriseAD;
  if (device_policy_->KeyMissing() && !is_active_directory &&
      !device_policy_->Mitigating() && is_first_real_user) {
    // This is the first sign-in on this unmanaged device.  Take ownership.
    key_gen_->Start(actual_account_id);
  }

  // Record that a login has successfully completed on this boot.
  system_->AtomicFileWrite(base::FilePath(kLoggedInFlag), "1");
  return true;
}

void SessionManagerImpl::StopSession(const std::string& in_unique_identifier) {
  LOG(INFO) << "Stopping all sessions";
  // Most calls to StopSession() will log the reason for the call.
  // If you don't see a log message saying the reason for the call, it is
  // likely a D-Bus message.
  manager_->ScheduleShutdown();
  // TODO(cmasone): re-enable these when we try to enable logout without exiting
  //                the session manager
  // browser_.job->StopSession();
  // user_policy_.reset();
  // session_started_ = false;
}

void SessionManagerImpl::StorePolicy(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    const std::vector<uint8_t>& in_policy_blob) {
  StorePolicyEx(std::move(response),
                MakePolicyDescriptor(ACCOUNT_TYPE_DEVICE, kEmptyAccountId),
                in_policy_blob);
}

void SessionManagerImpl::StoreUnsignedPolicy(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    const std::vector<uint8_t>& in_policy_blob) {
  StoreUnsignedPolicyEx(
      std::move(response),
      MakePolicyDescriptor(ACCOUNT_TYPE_DEVICE, kEmptyAccountId),
      in_policy_blob);
}

bool SessionManagerImpl::RetrievePolicy(brillo::ErrorPtr* error,
                                        std::vector<uint8_t>* out_policy_blob) {
  return RetrievePolicyEx(
      error, MakePolicyDescriptor(ACCOUNT_TYPE_DEVICE, kEmptyAccountId),
      out_policy_blob);
}

void SessionManagerImpl::StorePolicyForUser(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    const std::string& in_account_id,
    const std::vector<uint8_t>& in_policy_blob) {
  StorePolicyEx(std::move(response),
                MakePolicyDescriptor(ACCOUNT_TYPE_USER, in_account_id),
                in_policy_blob);
}

void SessionManagerImpl::StoreUnsignedPolicyForUser(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    const std::string& in_account_id,
    const std::vector<uint8_t>& in_policy_blob) {
  StoreUnsignedPolicyEx(std::move(response),
                        MakePolicyDescriptor(ACCOUNT_TYPE_USER, in_account_id),
                        in_policy_blob);
}

bool SessionManagerImpl::RetrievePolicyForUser(
    brillo::ErrorPtr* error,
    const std::string& in_account_id,
    std::vector<uint8_t>* out_policy_blob) {
  return RetrievePolicyEx(
      error, MakePolicyDescriptor(ACCOUNT_TYPE_USER, in_account_id),
      out_policy_blob);
}

bool SessionManagerImpl::RetrievePolicyForUserWithoutSession(
    brillo::ErrorPtr* error,
    const std::string& in_account_id,
    std::vector<uint8_t>* out_policy_blob) {
  return RetrievePolicyEx(
      error, MakePolicyDescriptor(ACCOUNT_TYPE_SESSIONLESS_USER, in_account_id),
      out_policy_blob);
}

void SessionManagerImpl::StoreDeviceLocalAccountPolicy(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    const std::string& in_account_id,
    const std::vector<uint8_t>& in_policy_blob) {
  StorePolicyEx(
      std::move(response),
      MakePolicyDescriptor(ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT, in_account_id),
      in_policy_blob);
}

bool SessionManagerImpl::RetrieveDeviceLocalAccountPolicy(
    brillo::ErrorPtr* error,
    const std::string& in_account_id,
    std::vector<uint8_t>* out_policy_blob) {
  return RetrievePolicyEx(
      error,
      MakePolicyDescriptor(ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT, in_account_id),
      out_policy_blob);
}

void SessionManagerImpl::StorePolicyEx(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    const std::vector<uint8_t>& in_descriptor_blob,
    const std::vector<uint8_t>& in_policy_blob) {
  StorePolicyInternalEx(in_descriptor_blob, in_policy_blob,
                        SignatureCheck::kEnabled, std::move(response));
}

void SessionManagerImpl::StoreUnsignedPolicyEx(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    const std::vector<uint8_t>& in_descriptor_blob,
    const std::vector<uint8_t>& in_policy_blob) {
  brillo::ErrorPtr error = VerifyUnsignedPolicyStore();
  if (error) {
    response->ReplyWithError(error.get());
    return;
  }
  StorePolicyInternalEx(in_descriptor_blob, in_policy_blob,
                        SignatureCheck::kDisabled, std::move(response));
}

bool SessionManagerImpl::RetrievePolicyEx(
    brillo::ErrorPtr* error,
    const std::vector<uint8_t>& in_descriptor_blob,
    std::vector<uint8_t>* out_policy_blob) {
  PolicyDescriptor descriptor;
  if (!descriptor.ParseFromArray(in_descriptor_blob.data(),
                                 in_descriptor_blob.size())) {
    *error = CreateError(DBUS_ERROR_INVALID_ARGS, kDescriptorParsingFailed);
    return false;
  }
  if (!ValidatePolicyDescriptor(descriptor, PolicyDescriptorUsage::kRetrieve)) {
    *error = CreateError(DBUS_ERROR_INVALID_ARGS, kDescriptorInvalid);
    return false;
  }

  // Special case for SESSIONLESS_USER_POLICY, which has a different lifetime
  // management than all other cases (unique_ptr vs plain ptr).
  // TODO(crbug.com/771638): Clean this up when the bug is fixed and sessionless
  // users are handled differently.
  std::unique_ptr<PolicyService> policy_service_ptr;
  PolicyService* policy_service;
  if (descriptor.account_type() == ACCOUNT_TYPE_SESSIONLESS_USER) {
    policy_service_ptr =
        user_policy_factory_->CreateForHiddenUserHome(descriptor.account_id());
    policy_service = policy_service_ptr.get();
  } else {
    policy_service = GetPolicyService(descriptor);
  }
  if (!policy_service) {
    const std::string message =
        base::StringPrintf(kCannotGetPolicyServiceFormat,
                           static_cast<int>(descriptor.account_type()));
    LOG(ERROR) << message;
    *error = CreateError(dbus_error::kGetServiceFail, message);
    return false;
  }

  PolicyNamespace ns(descriptor.domain(), descriptor.component_id());

  if (!policy_service->Retrieve(ns, out_policy_blob)) {
    LOG(ERROR) << kSigEncodeFailMessage;
    *error = CreateError(dbus_error::kSigEncodeFail, kSigEncodeFailMessage);
    return false;
  }
  return true;
}

std::string SessionManagerImpl::RetrieveSessionState() {
  if (!session_started_)
    return kStopped;
  if (session_stopping_)
    return kStopping;
  return kStarted;
}

std::map<std::string, std::string>
SessionManagerImpl::RetrieveActiveSessions() {
  std::map<std::string, std::string> result;
  for (const auto& entry : user_sessions_) {
    if (!entry.second)
      continue;
    result[entry.second->username] = entry.second->userhash;
  }
  return result;
}

bool SessionManagerImpl::IsGuestSessionActive() {
  return !user_sessions_.empty() && AllSessionsAreIncognito();
}

void SessionManagerImpl::HandleSupervisedUserCreationStarting() {
  supervised_user_creation_ongoing_ = true;
}

void SessionManagerImpl::HandleSupervisedUserCreationFinished() {
  supervised_user_creation_ongoing_ = false;
}

bool SessionManagerImpl::LockScreen(brillo::ErrorPtr* error) {
  if (!session_started_) {
    constexpr char kMessage[] =
        "Attempt to lock screen outside of user session.";
    LOG(WARNING) << kMessage;
    *error = CreateError(dbus_error::kSessionDoesNotExist, kMessage);
    return false;
  }
  // If all sessions are incognito, then locking is not allowed.
  if (AllSessionsAreIncognito()) {
    constexpr char kMessage[] = "Attempt to lock screen during Guest session.";
    LOG(WARNING) << kMessage;
    *error = CreateError(dbus_error::kSessionExists, kMessage);
    return false;
  }
  if (!screen_locked_) {
    screen_locked_ = true;
    delegate_->LockScreen();
  }
  LOG(INFO) << "LockScreen() method called.";
  return true;
}

void SessionManagerImpl::HandleLockScreenShown() {
  LOG(INFO) << "HandleLockScreenShown() method called.";
  adaptor_.SendScreenIsLockedSignal();
}

void SessionManagerImpl::HandleLockScreenDismissed() {
  screen_locked_ = false;
  LOG(INFO) << "HandleLockScreenDismissed() method called.";
  adaptor_.SendScreenIsUnlockedSignal();
}

bool SessionManagerImpl::RestartJob(brillo::ErrorPtr* error,
                                    const dbus::FileDescriptor& in_cred_fd,
                                    const std::vector<std::string>& in_argv) {
  struct ucred ucred = {0};
  socklen_t len = sizeof(struct ucred);
  if (!in_cred_fd.is_valid() || getsockopt(in_cred_fd.value(), SOL_SOCKET,
                                           SO_PEERCRED, &ucred, &len) == -1) {
    PLOG(ERROR) << "Can't get peer creds";
    *error = CreateError("GetPeerCredsFailed", strerror(errno));
    return false;
  }

  if (!manager_->IsBrowser(ucred.pid)) {
    constexpr char kMessage[] = "Provided pid is unknown.";
    LOG(ERROR) << kMessage;
    *error = CreateError(dbus_error::kUnknownPid, kMessage);
    return false;
  }

  // To set "logged-in" state for BWSI mode.
  if (!StartSession(error, kGuestUserName, "")) {
    DCHECK(*error);
    return false;
  }

  manager_->RestartBrowserWithArgs(in_argv, false);
  return true;
}

bool SessionManagerImpl::StartDeviceWipe(brillo::ErrorPtr* error) {
  if (system_->Exists(base::FilePath(kLoggedInFlag))) {
    constexpr char kMessage[] = "A user has already logged in this boot.";
    LOG(ERROR) << kMessage;
    *error = CreateError(dbus_error::kSessionExists, kMessage);
    return false;
  }
  InitiateDeviceWipe("session_manager_dbus_request");
  return true;
}

void SessionManagerImpl::StartTPMFirmwareUpdate(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    const std::string& update_mode) {
  // Make sure |update_mode| is supported.
  if (update_mode != kTPMFirmwareUpdateModeRecovery &&
      update_mode != kTPMFirmwareUpdateModeFirstBoot) {
    constexpr char kMessage[] = "Bad update mode.";
    LOG(ERROR) << kMessage;
    auto error = CreateError(dbus_error::kInvalidParameter, kMessage);
    response->ReplyWithError(error.get());
    return;
  }

  // Verify that we haven't seen a user log in since boot.
  if (system_->Exists(base::FilePath(kLoggedInFlag))) {
    constexpr char kMessage[] = "A user has already logged since boot.";
    LOG(ERROR) << kMessage;
    auto error = CreateError(dbus_error::kSessionExists, kMessage);
    response->ReplyWithError(error.get());
    return;
  }

  // For remotely managed devices, make sure the requested update mode matches
  // the admin-configured one in device policy.
  if (device_policy_->InstallAttributesEnterpriseMode()) {
    const enterprise_management::TPMFirmwareUpdateSettingsProto& settings =
        device_policy_->GetSettings().tpm_firmware_update_settings();
    if (!settings.allow_user_initiated_powerwash()) {
      auto error = CreateError(dbus_error::kNotAvailable,
                               "Policy doesn't allow TPM firmware update.");
      response->ReplyWithError(error.get());
      return;
    }
  }

  // Check whether a firmware update is present.
  if (!system_->Exists(base::FilePath(kTPMFirmwareUpdateAvailableFile))) {
    constexpr char kMessage[] = "No TPM firmware update available.";
    LOG(ERROR) << kMessage;
    auto error = CreateError(dbus_error::kNotAvailable, kMessage);
    response->ReplyWithError(error.get());
    return;
  }

  // Get the current TPM firmware update params from VPD.
  std::string vpd_params;
  base::StringPairs vpd_pairs;
  if (!system_->GetAppOutput(
          {"/usr/sbin/vpd_get_value", kTPMFirmwareUpdateParamsVPDKey},
          &vpd_params) ||
      !base::SplitStringIntoKeyValuePairs(vpd_params, ':', ',', &vpd_pairs)) {
    constexpr char kMessage[] = "Failed to get current VPD value.";
    LOG(ERROR) << kMessage;
    auto error = CreateError(dbus_error::kVpdUpdateFailed, kMessage);
    response->ReplyWithError(error.get());
    return;
  }

  // Construct the update parameters.
  vpd_params = "mode:" + update_mode;
  for (const auto& pair : vpd_pairs) {
    if (pair.first == "dryrun") {
      vpd_params += ",dryrun:1";
      break;
    }
  }

  // Trigger VPD key update.
  auto completion = base::Bind(
      &SessionManagerImpl::OnTPMFirmwareUpdateModeUpdated,
      weak_ptr_factory_.GetWeakPtr(), update_mode, base::Passed(&response));
  if (!vpd_process_->RunInBackground(
          {{kTPMFirmwareUpdateParamsVPDKey, vpd_params}}, true, completion)) {
    // Make sure to send a response.
    completion.Run(false);
  }
}

void SessionManagerImpl::SetFlagsForUser(
    const std::string& in_account_id,
    const std::vector<std::string>& in_flags) {
  manager_->SetFlagsForUser(in_account_id, in_flags);
}

void SessionManagerImpl::GetServerBackedStateKeys(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        std::vector<std::vector<uint8_t>>>> response) {
  DCHECK(dbus_service_);
  ServerBackedStateKeyGenerator::StateKeyCallback callback =
      dbus_service_->CreateStateKeyCallback(std::move(response));
  if (system_clock_synchronized_) {
    state_key_generator_->RequestStateKeys(callback);
  } else {
    pending_state_key_callbacks_.push_back(callback);
  }
}

void SessionManagerImpl::OnSystemClockServiceAvailable(bool service_available) {
  if (!service_available) {
    LOG(ERROR) << "Failed to listen for tlsdated service start";
    return;
  }

  GetSystemClockLastSyncInfo();
}

void SessionManagerImpl::GetSystemClockLastSyncInfo() {
  dbus::MethodCall method_call(system_clock::kSystemClockInterface,
                               system_clock::kSystemLastSyncInfo);
  system_clock_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&SessionManagerImpl::OnGotSystemClockLastSyncInfo,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SessionManagerImpl::OnGotSystemClockLastSyncInfo(
    dbus::Response* response) {
  if (!response) {
    LOG(ERROR) << system_clock::kSystemClockInterface << "."
               << system_clock::kSystemLastSyncInfo << " request failed.";
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&SessionManagerImpl::GetSystemClockLastSyncInfo,
                   weak_ptr_factory_.GetWeakPtr()),
        system_clock_last_sync_info_retry_delay_);
    return;
  }

  dbus::MessageReader reader(response);
  bool network_synchronized = false;
  if (!reader.PopBool(&network_synchronized)) {
    LOG(ERROR) << system_clock::kSystemClockInterface << "."
               << system_clock::kSystemLastSyncInfo
               << " response lacks network-synchronized argument";
    return;
  }

  if (network_synchronized) {
    system_clock_synchronized_ = true;
    for (const auto& callback : pending_state_key_callbacks_)
      state_key_generator_->RequestStateKeys(callback);
    pending_state_key_callbacks_.clear();
  } else {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&SessionManagerImpl::GetSystemClockLastSyncInfo,
                   weak_ptr_factory_.GetWeakPtr()),
        system_clock_last_sync_info_retry_delay_);
  }
}

bool SessionManagerImpl::InitMachineInfo(brillo::ErrorPtr* error,
                                         const std::string& in_data) {
  std::map<std::string, std::string> params;
  if (!ServerBackedStateKeyGenerator::ParseMachineInfo(in_data, &params)) {
    *error = CreateError(dbus_error::kInitMachineInfoFail, "Parse failure.");
    return false;
  }

  if (!state_key_generator_->InitMachineInfo(params)) {
    *error =
        CreateError(dbus_error::kInitMachineInfoFail, "Missing parameters.");
    return false;
  }
  return true;
}

bool SessionManagerImpl::StartArcInstance(
    brillo::ErrorPtr* error,
    const std::vector<uint8_t>& in_request,
    std::string* out_container_instance_id,
    dbus::FileDescriptor* out_fd) {
#if USE_CHEETS
  pid_t pid = 0;
  android_container_->GetContainerPID(&pid);

  base::ScopedClosureRunner scoped_runner;
  if (pid > 0) {
    LOG(INFO) << "Container is running with PID " << pid;
    // Stop the existing instance if it fails to continue to boot an exsiting
    // container. Using Unretained() is okay because the closure will be called
    // when exiting this function.
    scoped_runner.Reset(base::Bind(&SessionManagerImpl::OnContinueArcBootFailed,
                                   base::Unretained(this)));
  }

  StartArcInstanceRequest request;
  if (!request.ParseFromArray(in_request.data(), in_request.size())) {
    *error = CreateError(DBUS_ERROR_INVALID_ARGS,
                         "StartArcInstanceRequest parsing failed.");
    return false;
  }
  if (!ValidateStartArcInstanceRequest(request, error)) {
    DCHECK(*error);
    return false;
  }

  dbus::FileDescriptor server_socket;
  if (!request.for_login_screen()) {
    // Create a server socket unless the request is for starting an instance
    // for login screen.
    if (!CreateArcServerSocket(&server_socket, error)) {
      DCHECK(*error);
      return false;
    }
  } else {
    // There is nothing to do here, but since passing an invalid handle is not
    // allowed by the dbus binding, open /dev/null and return a handle to the
    // file.
    server_socket.PutValue(HANDLE_EINTR(open("/dev/null", O_RDONLY)));
  }
  server_socket.CheckValidity();

  if (!StartArcInstanceInternal(error, request, pid,
                                out_container_instance_id)) {
    DCHECK(*error);
    return false;
  }
  auto closure_unused = scoped_runner.Release();

  *out_fd = std::move(server_socket);
  return true;
#else
  *error = CreateError(dbus_error::kNotAvailable, "ARC not supported.");
  return false;
#endif  // !USE_CHEETS
}

bool SessionManagerImpl::StopArcInstance(brillo::ErrorPtr* error) {
#if USE_CHEETS
  pid_t pid;
  if (!android_container_->GetContainerPID(&pid)) {
    constexpr char kMessage[] = "Error getting Android container pid.";
    LOG(ERROR) << kMessage;
    *error = CreateError(dbus_error::kContainerShutdownFail, kMessage);
    return false;
  }

  android_container_->RequestJobExit();
  android_container_->EnsureJobExit(kContainerTimeout);
  return true;
#else
  *error = CreateError(dbus_error::kNotAvailable, "ARC not supported.");
  return false;
#endif  // USE_CHEETS
}

bool SessionManagerImpl::SetArcCpuRestriction(brillo::ErrorPtr* error,
                                              uint32_t in_restriction_state) {
#if USE_CHEETS
  std::string shares_out;
  switch (static_cast<ContainerCpuRestrictionState>(in_restriction_state)) {
    case CONTAINER_CPU_RESTRICTION_FOREGROUND:
      shares_out = std::to_string(kCpuSharesForeground);
      break;
    case CONTAINER_CPU_RESTRICTION_BACKGROUND:
      shares_out = std::to_string(kCpuSharesBackground);
      break;
    default:
      constexpr char kMessage[] = "Invalid CPU restriction state specified.";
      LOG(ERROR) << kMessage;
      *error = CreateError(dbus_error::kArcCpuCgroupFail, kMessage);
      return false;
  }
  if (base::WriteFile(base::FilePath(kCpuSharesFile), shares_out.c_str(),
                      shares_out.length()) != shares_out.length()) {
    constexpr char kMessage[] = "Error updating Android container's cgroups.";
    LOG(ERROR) << kMessage;
    *error = CreateError(dbus_error::kArcCpuCgroupFail, kMessage);
    return false;
  }
  return true;
#else
  *error = CreateError(dbus_error::kNotAvailable, "ARC not supported.");
  return false;
#endif
}

bool SessionManagerImpl::EmitArcBooted(brillo::ErrorPtr* error,
                                       const std::string& in_account_id) {
#if USE_CHEETS
  std::vector<std::string> keyvals;
  if (!in_account_id.empty()) {
    std::string actual_account_id;
    if (!NormalizeAccountId(in_account_id, &actual_account_id, error)) {
      DCHECK(*error);
      return false;
    }
    const base::FilePath android_data_old_dir =
        GetAndroidDataOldDirForUser(actual_account_id);
    keyvals.emplace_back("ANDROID_DATA_OLD_DIR=" +
                         android_data_old_dir.value());
  }

  init_controller_->TriggerImpulse(kArcBootedImpulse, keyvals,
                                   InitDaemonController::TriggerMode::ASYNC);
  return true;
#else
  *error = CreateError(dbus_error::kNotAvailable, "ARC not supported.");
  return false;
#endif
}

bool SessionManagerImpl::GetArcStartTimeTicks(brillo::ErrorPtr* error,
                                              int64_t* out_start_time) {
#if USE_CHEETS
  if (arc_start_time_.is_null()) {
    *error = CreateError(dbus_error::kNotStarted, "ARC is not started yet.");
    return false;
  }
  *out_start_time = arc_start_time_.ToInternalValue();
  return true;
#else
  *error = CreateError(dbus_error::kNotAvailable, "ARC not supported.");
  return false;
#endif  // !USE_CHEETS
}

bool SessionManagerImpl::StartContainer(brillo::ErrorPtr* error,
                                        const std::string& in_path,
                                        const std::string& in_name,
                                        const std::string& in_hashed_username,
                                        bool in_writable) {
  // Ensure that the vm component is installed.
  dbus::MethodCall method_call(
      chromeos::kComponentUpdaterServiceInterface,
      chromeos::kComponentUpdaterServiceLoadComponentMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(imageloader::kTerminaComponentName);
  component_updater_proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);

  if (!base::ContainsOnlyChars(in_name, kContainerNameAllowedChars)) {
    LOG(ERROR) << "Invalid character in container name " << in_name;
    return false;
  }
  if (!base::ContainsOnlyChars(in_path,
                               std::string(kContainerPathAllowedChars) + "/")) {
    LOG(ERROR) << "Invalid character in container path" << in_path;
    return false;
  }
  // TODO(dgreid) - bug 770766 - Make hashed_username mandatory, drop default.
  static const base::FilePath kDefaultUserPath("/home/chronos/user");
  base::FilePath user_path;
  if (!in_hashed_username.empty()) {
    user_path = GetHashedUserPath(in_hashed_username);
    if (user_path.empty())
      return false;
  } else {
    user_path = kDefaultUserPath;
  }
  // TODO(dgreid) - Allow paths outside of downloads.
  base::FilePath container_path = user_path.Append("Downloads").Append(in_path);
  // Checking that the provided path doesn't contain '..' is important as it
  // limits the caller to reading files that they own.
  if (container_path.empty() || container_path.ReferencesParent())
    return false;
  if (!termina_manager_->StartVmContainer(container_path, in_name,
                                          in_writable)) {
    constexpr char kMessage[] = "Container start failed.";
    LOG(ERROR) << kMessage;
    *error = CreateError(dbus_error::kContainerStartupFail, kMessage);
    return false;
  }
  return true;
}

bool SessionManagerImpl::StopContainer(brillo::ErrorPtr* error,
                                       const std::string& in_name) {
  if (!termina_manager_->StopVmContainer(in_name)) {
    constexpr char kMessage[] = "Container stop failed.";
    LOG(ERROR) << kMessage;
    *error = CreateError(dbus_error::kContainerStartupFail, kMessage);
    return false;
  }
  return true;
}

bool SessionManagerImpl::RemoveArcData(brillo::ErrorPtr* error,
                                       const std::string& in_account_id) {
#if USE_CHEETS
  pid_t pid = 0;
  if (android_container_->GetContainerPID(&pid)) {
    *error = CreateError(dbus_error::kArcInstanceRunning,
                         "ARC is currently running.");
    return false;
  }

  std::string actual_account_id;
  if (!NormalizeAccountId(in_account_id, &actual_account_id, error)) {
    DCHECK(*error);
    return false;
  }
  const base::FilePath android_data_dir =
      GetAndroidDataDirForUser(actual_account_id);
  const base::FilePath android_data_old_dir =
      GetAndroidDataOldDirForUser(actual_account_id);

  if (RemoveArcDataInternal(android_data_dir, android_data_old_dir))
    return true;  // all done.

  PLOG(WARNING) << "Failed to rename " << android_data_dir.value()
                << "; directly deleting it instead";
  // As a last resort, directly delete the directory although it's not always
  // safe to do. If session_manager is killed or the device is shut down while
  // doing the removal, the directory will have an unusual set of files which
  // may confuse ARC and prevent it from booting.
  system_->RemoveDirTree(android_data_dir);
  LOG(INFO) << "Finished removing " << android_data_dir.value();
  return true;
#else
  *error = CreateError(dbus_error::kNotAvailable, "ARC not supported.");
  return false;
#endif  // USE_CHEETS
}

#if USE_CHEETS
bool SessionManagerImpl::RemoveArcDataInternal(
    const base::FilePath& android_data_dir,
    const base::FilePath& android_data_old_dir) {
  // It should never happen, but in case |android_data_old_dir| is a file,
  // remove it. RemoveFile() immediately returns false (i.e. no-op) when
  // |android_data_old_dir| is a directory.
  system_->RemoveFile(android_data_old_dir);

  // Create |android_data_old_dir| if it doesn't exist.
  if (!system_->DirectoryExists(android_data_old_dir)) {
    if (!system_->CreateDir(android_data_old_dir)) {
      PLOG(ERROR) << "Failed to create " << android_data_old_dir.value();
      return false;
    }
  }

  if (!system_->DirectoryExists(android_data_dir) &&
      system_->IsDirectoryEmpty(android_data_old_dir)) {
    return true;  // nothing to do.
  }

  // Create a random temporary directory in |android_data_old_dir|.
  // Note: Renaming a directory to an existing empty directory works.
  base::FilePath target_dir_name;
  if (!system_->CreateTemporaryDirIn(android_data_old_dir, &target_dir_name)) {
    LOG(WARNING) << "Failed to create a temporary directory in "
                 << android_data_old_dir.value();
    return false;
  }
  LOG(INFO) << "Renaming " << android_data_dir.value() << " to "
            << target_dir_name.value();

  // Does the actual renaming here with rename(2). Note that if the process
  // (or the device itself) is killed / turned off right before the rename(2)
  // operation, both |android_data_dir| and |android_data_old_dir| will remain
  // while ARC is disabled in the browser side. In that case, the browser will
  // call RemoveArcData() later as needed, and both directories will disappear.
  if (system_->DirectoryExists(android_data_dir)) {
    if (!system_->RenameDir(android_data_dir, target_dir_name)) {
      LOG(WARNING) << "Failed to rename " << android_data_dir.value() << " to "
                   << target_dir_name.value();
      return false;
    }
  }

  // Ask init to remove all files and directories in |android_data_old_dir|.
  // Note that the init job never deletes |android_data_old_dir| itself so the
  // rename() operation above never fails.
  LOG(INFO) << "Removing contents in " << android_data_old_dir.value();
  init_controller_->TriggerImpulse(
      kRemoveOldArcDataImpulse,
      {"ANDROID_DATA_OLD_DIR=" + android_data_old_dir.value()},
      InitDaemonController::TriggerMode::ASYNC);
  return true;
}
#endif  // USE_CHEETS

void SessionManagerImpl::OnPolicyPersisted(bool success) {
  device_local_account_manager_->UpdateDeviceSettings(
      device_policy_->GetSettings());
  adaptor_.SendPropertyChangeCompleteSignal(ToSuccessSignal(success));
}

void SessionManagerImpl::OnKeyPersisted(bool success) {
  adaptor_.SendSetOwnerKeyCompleteSignal(ToSuccessSignal(success));
}

void SessionManagerImpl::OnKeyGenerated(const std::string& username,
                                        const base::FilePath& temp_key_file) {
  ImportValidateAndStoreGeneratedKey(username, temp_key_file);
}

void SessionManagerImpl::ImportValidateAndStoreGeneratedKey(
    const std::string& username, const base::FilePath& temp_key_file) {
  DLOG(INFO) << "Processing generated key at " << temp_key_file.value();
  std::string key;
  base::ReadFileToString(temp_key_file, &key);
  PLOG_IF(WARNING, !base::DeleteFile(temp_key_file, false))
      << "Can't delete " << temp_key_file.value();
  device_policy_->ValidateAndStoreOwnerKey(
      username, StringToBlob(key), user_sessions_[username]->slot.get());
}

void SessionManagerImpl::InitiateDeviceWipe(const std::string& reason) {
  // The log string must not be confused with other clobbers-state parameters.
  // Sanitize by replacing all non-alphanumeric characters with underscores and
  // clamping size to 50 characters.
  std::string sanitized_reason(reason.substr(0, 50));
  std::locale locale("C");
  std::replace_if(sanitized_reason.begin(), sanitized_reason.end(),
                  [&locale](const std::string::value_type character) {
                    return !std::isalnum(character, locale);
                  },
                  '_');
  const base::FilePath reset_path(kResetFile);
  system_->AtomicFileWrite(reset_path,
                           "fast safe keepimg reason=" + sanitized_reason);
  delegate_->RestartDevice("session_manager (" + reason + ")");
}

// static
bool SessionManagerImpl::NormalizeAccountId(const std::string& account_id,
                                            std::string* actual_account_id_out,
                                            brillo::ErrorPtr* error_out) {
  if (ValidateAccountId(account_id, actual_account_id_out)) {
    DCHECK(!actual_account_id_out->empty());
    return true;
  }

  // TODO(alemate): adjust this error message after ChromeOS will stop using
  // email as cryptohome identifier.
  constexpr char kMessage[] =
      "Provided email address is not valid.  ASCII only.";
  LOG(ERROR) << kMessage;
  *error_out = CreateError(dbus_error::kInvalidAccount, kMessage);
  DCHECK(actual_account_id_out->empty());
  return false;
}

bool SessionManagerImpl::AllSessionsAreIncognito() {
  size_t incognito_count = 0;
  for (UserSessionMap::const_iterator it = user_sessions_.begin();
       it != user_sessions_.end(); ++it) {
    if (it->second)
      incognito_count += it->second->is_incognito;
  }
  return incognito_count == user_sessions_.size();
}

std::unique_ptr<SessionManagerImpl::UserSession>
SessionManagerImpl::CreateUserSession(const std::string& username,
                                      bool is_incognito,
                                      brillo::ErrorPtr* error) {
  std::unique_ptr<PolicyService> user_policy =
      user_policy_factory_->Create(username);
  if (!user_policy) {
    LOG(ERROR) << "User policy failed to initialize.";
    *error = CreateError(dbus_error::kPolicyInitFail, "Can't create session.");
    return nullptr;
  }

  crypto::ScopedPK11Slot slot(nss_->OpenUserDB(GetUserPath(username)));
  if (!slot) {
    LOG(ERROR) << "Could not open the current user's NSS database.";
    *error = CreateError(dbus_error::kNoUserNssDb, "Can't create session.");
    return nullptr;
  }

  return std::make_unique<UserSession>(username, SanitizeUserName(username),
                                       is_incognito, std::move(slot),
                                       std::move(user_policy));
}

brillo::ErrorPtr SessionManagerImpl::VerifyUnsignedPolicyStore() {
  // Unsigned policy store D-Bus call is allowed only in enterprise_ad mode.
  const std::string& mode = install_attributes_reader_->GetAttribute(
      InstallAttributesReader::kAttrMode);
  if (mode != InstallAttributesReader::kDeviceModeEnterpriseAD) {
    constexpr char kMessage[] = "Device mode doesn't permit unsigned policy.";
    LOG(ERROR) << kMessage;
    return CreateError(dbus_error::kPolicySignatureRequired, kMessage);
  }

  return nullptr;
}

PolicyService* SessionManagerImpl::GetPolicyService(
    const PolicyDescriptor& descriptor) {
  switch (descriptor.account_type()) {
    case ACCOUNT_TYPE_DEVICE:
      return device_policy_.get();
    case ACCOUNT_TYPE_USER: {
      UserSessionMap::const_iterator it =
          user_sessions_.find(descriptor.account_id());
      return it != user_sessions_.end() ? it->second->policy_service.get()
                                        : nullptr;
    }
    case ACCOUNT_TYPE_SESSIONLESS_USER:
      // Descriptor validation should prevent this case.
      NOTREACHED();
      return nullptr;
    case ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT:
      return device_local_account_manager_->GetPolicyService(
          descriptor.account_id());
  }
}

int SessionManagerImpl::GetKeyInstallFlags(const PolicyDescriptor& descriptor) {
  switch (descriptor.account_type()) {
    case ACCOUNT_TYPE_DEVICE: {
      int flags = PolicyService::KEY_ROTATE;
      if (!session_started_)
        flags |= PolicyService::KEY_INSTALL_NEW | PolicyService::KEY_CLOBBER;
      return flags;
    }
    case ACCOUNT_TYPE_USER:
      return PolicyService::KEY_INSTALL_NEW | PolicyService::KEY_ROTATE;
    case ACCOUNT_TYPE_SESSIONLESS_USER: {
      // Only supports retrieval, not storage.
      NOTREACHED();
      return PolicyService::KEY_NONE;
    }
    case ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT:
      return PolicyService::KEY_NONE;
  }
}

void SessionManagerImpl::StorePolicyInternalEx(
    const std::vector<uint8_t>& descriptor_blob,
    const std::vector<uint8_t>& policy_blob,
    SignatureCheck signature_check,
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response) {
  PolicyDescriptor descriptor;
  if (!descriptor.ParseFromArray(descriptor_blob.data(),
                                 descriptor_blob.size())) {
    auto error = CreateError(DBUS_ERROR_INVALID_ARGS, kDescriptorParsingFailed);
    response->ReplyWithError(error.get());
    return;
  }
  if (!ValidatePolicyDescriptor(descriptor, PolicyDescriptorUsage::kStore)) {
    auto error = CreateError(DBUS_ERROR_INVALID_ARGS, kDescriptorInvalid);
    response->ReplyWithError(error.get());
    return;
  }

  PolicyService* policy_service = GetPolicyService(descriptor);
  if (!policy_service) {
    const std::string message =
        base::StringPrintf(kCannotGetPolicyServiceFormat,
                           static_cast<int>(descriptor.account_type()));
    LOG(ERROR) << message;
    auto error = CreateError(dbus_error::kGetServiceFail, message);
    response->ReplyWithError(error.get());
    return;
  }

  int key_flags = GetKeyInstallFlags(descriptor);
  PolicyNamespace ns(descriptor.domain(), descriptor.component_id());

  DCHECK(dbus_service_);
  policy_service->Store(ns, policy_blob, key_flags, signature_check,
                        dbus_service_->CreatePolicyServiceCompletionCallback(
                            std::move(response)));
}

void SessionManagerImpl::OnTPMFirmwareUpdateModeUpdated(
    const std::string& update_mode,
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    bool success) {
  if (!success) {
    constexpr char kMessage[] = "Failed set update mode in VPD.";
    LOG(ERROR) << kMessage;
    auto error = CreateError(dbus_error::kVpdUpdateFailed, kMessage);
    response->ReplyWithError(error.get());
    return;
  }

  // At this point, we have set things up so the firmware updater will actually
  // run instead of bailing out. How the device is supposed to run the firmware
  // update depends on the requested update mode.
  if (update_mode == kTPMFirmwareUpdateModeRecovery) {
    // We're done. The user needs to trigger recovery manually.
    response->Return();
    return;
  } else if (update_mode == kTPMFirmwareUpdateModeFirstBoot) {
    // Trigger a wipe. Note that this also implicitly clears TPM ownership,
    // which is required for the TPM firmware updater to run. Furthermore, we'll
    // reboot immediately.
    InitiateDeviceWipe("session_manager_tpm_firmware_update");
    response->Return();
    return;
  }

  // We shouldn't be here because we have checked |update_mode| to match one of
  // the recognized modes before.
  constexpr char kMessage[] = "Bad update mode.";
  NOTREACHED() << kMessage;
  auto error = CreateError(dbus_error::kInvalidParameter, kMessage);
  response->ReplyWithError(error.get());
}

#if USE_CHEETS
bool SessionManagerImpl::CreateArcServerSocket(dbus::FileDescriptor* out_fd,
                                               brillo::ErrorPtr* error) {
  ScopedPlatformHandle socket_fd(
      system_->CreateServerHandle(NamedPlatformHandle(kArcBridgeSocketPath)));
  if (!socket_fd.is_valid()) {
    constexpr char kMessage[] = "Failed to create a server socket";
    LOG(ERROR) << kMessage;
    *error = CreateError(dbus_error::kContainerStartupFail, kMessage);
    return false;
  }

  // Change permissions on the socket.
  gid_t arc_bridge_gid = -1;
  if (!system_->GetGroupInfo(kArcBridgeSocketGroup, &arc_bridge_gid)) {
    constexpr char kMessage[] = "Failed to get arc-bridge gid";
    LOG(ERROR) << kMessage;
    *error = CreateError(dbus_error::kContainerStartupFail, kMessage);
    return false;
  }

  if (!system_->ChangeOwner(base::FilePath(kArcBridgeSocketPath), -1,
                            arc_bridge_gid)) {
    constexpr char kMessage[] = "Failed to change group of the socket";
    PLOG(ERROR) << kMessage;
    *error = CreateError(dbus_error::kContainerStartupFail, kMessage);
    return false;
  }

  if (!system_->SetPosixFilePermissions(base::FilePath(kArcBridgeSocketPath),
                                        0660)) {
    constexpr char kMessage[] = "Failed to change permissions of the socket";
    PLOG(ERROR) << kMessage;
    *error = CreateError(dbus_error::kContainerStartupFail, kMessage);
    return false;
  }

  out_fd->PutValue(socket_fd.release());
  return true;
}

bool SessionManagerImpl::StartArcInstanceInternal(
    brillo::ErrorPtr* error,
    const StartArcInstanceRequest& in_request,
    pid_t container_pid,
    std::string* out_container_instance_id) {
  // Set up impulse params.
  std::vector<std::string> keyvals = {
      "CHROMEOS_DEV_MODE=" + std::to_string(IsDevMode(system_)),
      "CHROMEOS_INSIDE_VM=" + std::to_string(IsInsideVm(system_)),
      "NATIVE_BRIDGE_EXPERIMENT=" +
          std::to_string(in_request.native_bridge_experiment()),
  };

  const bool continue_boot = container_pid > 0;
  android_container_->SetStatefulMode(in_request.for_login_screen()
                                          ? StatefulMode::STATELESS
                                          : StatefulMode::STATEFUL);
  if (!in_request.for_login_screen()) {
    arc_start_time_ = base::TimeTicks::Now();

    // To boot or continue booting ARC instance, certain amount of disk space is
    // needed under the home. We first check it.
    // Note that this check is unnecessary for login screen case, because it
    // runs on tmpfs.
    if (system_->AmountOfFreeDiskSpace(base::FilePath(kArcDiskCheckPath)) <
        kArcCriticalDiskFreeBytes) {
      constexpr char kMessage[] = "Low free disk under /home";
      LOG(ERROR) << kMessage;
      *error = CreateError(dbus_error::kLowFreeDisk, kMessage);
      return false;
    }

    std::string account_id;
    if (!NormalizeAccountId(in_request.account_id(), &account_id, error)) {
      DCHECK(*error);
      return false;
    }
    if (user_sessions_.count(account_id) == 0) {
      // This path can be taken if a forged D-Bus message for starting a full
      // (stateful) container is sent to session_manager before the actual
      // user's session has started. Do not remove the |account_id| check to
      // prevent such a container from starting on login screen.
      constexpr char kMessage[] = "Provided user ID does not have a session.";
      LOG(ERROR) << kMessage;
      *error = CreateError(dbus_error::kSessionDoesNotExist, kMessage);
      return false;
    }

    std::vector<std::string> extra_keyvals = {
        "ANDROID_DATA_DIR=" + GetAndroidDataDirForUser(account_id).value(),
        "ANDROID_DATA_OLD_DIR=" +
            GetAndroidDataOldDirForUser(account_id).value(),
        "CHROMEOS_USER=" + account_id,
        "DISABLE_BOOT_COMPLETED_BROADCAST=" +
            std::to_string(in_request.skip_boot_completed_broadcast()),
        "ENABLE_VENDOR_PRIVILEGED=" +
            std::to_string(in_request.scan_vendor_priv_app()),
    };
    keyvals.insert(keyvals.end(),
                   std::make_move_iterator(extra_keyvals.begin()),
                   std::make_move_iterator(extra_keyvals.end()));
    if (continue_boot)
      keyvals.emplace_back("CONTAINER_PID=" + std::to_string(container_pid));
  }

  std::string container_instance_id;
  if (!continue_boot) {
    // Start the container.
    const char* init_signal = in_request.for_login_screen()
                                  ? kStartArcInstanceForLoginScreenImpulse
                                  : kStartArcInstanceImpulse;
    container_instance_id = StartArcContainer(init_signal, keyvals, error);
    if (container_instance_id.empty()) {
      DCHECK(*error);
      return false;
    }
  } else {
    // Continue booting the existing container.
    if (!ContinueArcBoot(keyvals, error)) {
      DCHECK(*error);
      return false;
    }
  }

  if (!in_request.for_login_screen()) {
    // In addition, start ARC network service, if this is not for login screen.
    if (!StartArcNetwork(error)) {
      DCHECK(*error);
      if (continue_boot)
        return false;  // the caller shuts down the container.
      // Asking the container to exit will result in
      // OnAndroidContainerStopped() being called, which will handle any
      // necessary cleanup.
      android_container_->RequestJobExit();
      android_container_->EnsureJobExit(kContainerTimeout);
      return false;
    }
    login_metrics_->StartTrackingArcUseTime();
  }

  *out_container_instance_id = std::move(container_instance_id);
  return true;
}

std::string SessionManagerImpl::StartArcContainer(
    const std::string& init_signal,
    const std::vector<std::string>& init_keyvals,
    brillo::ErrorPtr* error_out) {
  // TODO(cmtm): remove this for android P with b/67480541
  if (!init_controller_->TriggerImpulse(
          init_signal, init_keyvals, InitDaemonController::TriggerMode::SYNC)) {
    // Failed to start container. Thus, trigger stop-arc-instance impulse
    // manually for cleanup.
    init_controller_->TriggerImpulse(kStopArcInstanceImpulse, {},
                                     InitDaemonController::TriggerMode::SYNC);
    const std::string message = "Emitting " + init_signal + " impulse failed.";
    LOG(ERROR) << message;
    *error_out = CreateError(dbus_error::kEmitFailed, message);
    return std::string();
  }

  // Container instance id needs to be valid ASCII/UTF-8, so encode as base64.
  std::string container_instance_id =
      base::RandBytesAsString(kArcContainerInstanceIdLength);
  base::Base64Encode(container_instance_id, &container_instance_id);

  // Pass in the same key/value pairs that were passed to arc-setup (through
  // init, above) into the container invocation as environment values. When the
  // container is started with run_oci, this allows for it to correctly
  // propagate some information (such as the ANDROID_DATA_DIR) to the hooks so
  // it can set itself up.
  if (!android_container_->StartContainer(
          init_keyvals,
          base::Bind(&SessionManagerImpl::OnAndroidContainerStopped,
                     weak_ptr_factory_.GetWeakPtr(), container_instance_id))) {
    // Failed to start container. Thus, trigger stop-arc-instance impulse
    // manually for cleanup.
    init_controller_->TriggerImpulse(kStopArcInstanceImpulse, {},
                                     InitDaemonController::TriggerMode::SYNC);
    constexpr char kMessage[] = "Starting Android container failed.";
    LOG(ERROR) << kMessage;
    *error_out = CreateError(dbus_error::kContainerStartupFail, kMessage);
    return std::string();
  }

  pid_t pid = 0;
  android_container_->GetContainerPID(&pid);
  LOG(INFO) << "Started Android container with PID " << pid;
  return container_instance_id;
}

bool SessionManagerImpl::StartArcNetwork(brillo::ErrorPtr* error_out) {
  pid_t pid = 0;
  if (!android_container_->GetContainerPID(&pid)) {
    constexpr char kMessage[] = "Getting Android container info failed.";
    LOG(ERROR) << kMessage;
    *error_out = CreateError(dbus_error::kContainerStartupFail, kMessage);
    return false;
  }

  // Tell init to configure the network.
  init_controller_->TriggerImpulse(
      kStartArcNetworkImpulse,
      {"CONTAINER_NAME=" + std::string(kArcContainerName),
       "CONTAINER_PID=" + std::to_string(pid)},
      InitDaemonController::TriggerMode::ASYNC);
  return true;
}

bool SessionManagerImpl::ContinueArcBoot(
    const std::vector<std::string>& init_keyvals, brillo::ErrorPtr* error_out) {
  if (!init_controller_->TriggerImpulse(
          kContinueArcBootImpulse, init_keyvals,
          InitDaemonController::TriggerMode::SYNC)) {
    constexpr char kMessage[] = "Emitting continue-arc-boot impulse failed.";
    LOG(ERROR) << kMessage;
    *error_out = CreateError(dbus_error::kEmitFailed, kMessage);
    return false;
  }
  return true;
}

void SessionManagerImpl::OnContinueArcBootFailed() {
  LOG(ERROR) << "Failed to continue ARC boot. Stopping the container.";
  brillo::ErrorPtr error_ptr;
  StopArcInstance(&error_ptr);
}

void SessionManagerImpl::OnAndroidContainerStopped(
    const std::string& container_instance_id, pid_t pid, bool clean) {
  if (clean) {
    LOG(INFO) << "Android Container with pid " << pid << " stopped";
  } else {
    LOG(ERROR) << "Android Container with pid " << pid << " crashed";
  }

  login_metrics_->StopTrackingArcUseTime();
  if (!init_controller_->TriggerImpulse(
          kStopArcInstanceImpulse, {},
          InitDaemonController::TriggerMode::SYNC)) {
    LOG(ERROR) << "Emitting stop-arc-instance impulse failed.";
  }

  if (!init_controller_->TriggerImpulse(
          kStopArcNetworkImpulse, {},
          InitDaemonController::TriggerMode::SYNC)) {
    LOG(ERROR) << "Emitting stop-arc-network impulse failed.";
  }

  adaptor_.SendArcInstanceStoppedSignal(clean, container_instance_id);
}
#endif  // USE_CHEETS

}  // namespace login_manager
