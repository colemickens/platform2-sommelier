// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_impl.h"

#include <stdint.h>

#include <string>

#include <base/bind.h>
#include <base/callback.h>
#include <base/command_line.h>
#include <base/files/file_util.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/message_loop/message_loop_proxy.h>
#include <base/stl_util.h>
#include <base/strings/string_util.h>
#include <chromeos/cryptohome.h>
#include <crypto/scoped_nss_types.h>
#include <dbus/message.h>
#include <vboot/crossystem.h>

#include "bindings/chrome_device_policy.pb.h"
#include "bindings/device_management_backend.pb.h"
#include "login_manager/dbus_error_types.h"
#include "login_manager/dbus_signal_emitter.h"
#include "login_manager/device_local_account_policy_service.h"
#include "login_manager/device_policy_service.h"
#include "login_manager/key_generator.h"
#include "login_manager/login_metrics.h"
#include "login_manager/nss_util.h"
#include "login_manager/policy_key.h"
#include "login_manager/policy_service.h"
#include "login_manager/process_manager_service_interface.h"
#include "login_manager/regen_mitigator.h"
#include "login_manager/system_utils.h"
#include "login_manager/upstart_signal_emitter.h"
#include "login_manager/user_policy_service_factory.h"

using base::FilePath;
using chromeos::cryptohome::home::GetUserPath;
using chromeos::cryptohome::home::SanitizeUserName;
using chromeos::cryptohome::home::kGuestUserName;

namespace login_manager {  // NOLINT

const char SessionManagerImpl::kDemoUser[] = "demouser@";

const char SessionManagerImpl::kStarted[] = "started";
const char SessionManagerImpl::kStopping[] = "stopping";
const char SessionManagerImpl::kStopped[] = "stopped";

const char SessionManagerImpl::kLoggedInFlag[] =
    "/var/run/session_manager/logged_in";
const char SessionManagerImpl::kResetFile[] =
    "/mnt/stateful_partition/factory_install_reset";

namespace {

// Constants used in email validation.
const char kEmailSeparator = '@';
const char kLegalCharacters[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    ".@1234567890!#$%&'*+-/=?^_`{|}~";

// The flag to pass to chrome to open a named socket for testing.
const char kTestingChannelFlag[] = "--testing-channel=NamedTestingInterface:";

// Device-local account state directory.
const base::FilePath::CharType kDeviceLocalAccountStateDir[] =
    FILE_PATH_LITERAL("/var/lib/device_local_accounts");

// The name of the flag that indicates whether dev mode should be blocked.
const char kCrossystemBlockDevmode[] = "block_devmode";

}  // namespace

SessionManagerImpl::Error::Error() : set_(false) {
}

SessionManagerImpl::Error::Error(const std::string& name,
                                 const std::string& message)
    : name_(name), message_(message), set_(true) {
}

SessionManagerImpl::Error::~Error() {
}

void SessionManagerImpl::Error::Set(const std::string& name,
                                    const std::string& message) {
  name_ = name;
  message_ = message;
  set_ = true;
}

struct SessionManagerImpl::UserSession {
 public:
  UserSession(const std::string& username,
              const std::string& userhash,
              bool is_incognito,
              crypto::ScopedPK11Slot slot,
              scoped_ptr<PolicyService> policy_service)
      : username(username),
        userhash(userhash),
        is_incognito(is_incognito),
        slot(slot.Pass()),
        policy_service(policy_service.Pass()) {}
  ~UserSession() {}

  const std::string username;
  const std::string userhash;
  const bool is_incognito;
  crypto::ScopedPK11Slot slot;
  scoped_ptr<PolicyService> policy_service;
};

SessionManagerImpl::SessionManagerImpl(
    scoped_ptr<UpstartSignalEmitter> emitter,
    DBusSignalEmitterInterface* dbus_emitter,
    base::Closure lock_screen_closure,
    base::Closure restart_device_closure,
    KeyGenerator* key_gen,
    ServerBackedStateKeyGenerator* state_key_generator,
    ProcessManagerServiceInterface* manager,
    LoginMetrics* metrics,
    NssUtil* nss,
    SystemUtils* utils)
    : session_started_(false),
      session_stopping_(false),
      screen_locked_(false),
      upstart_signal_emitter_(emitter.Pass()),
      lock_screen_closure_(lock_screen_closure),
      restart_device_closure_(restart_device_closure),
      dbus_emitter_(dbus_emitter),
      key_gen_(key_gen),
      state_key_generator_(state_key_generator),
      manager_(manager),
      login_metrics_(metrics),
      nss_(nss),
      system_(utils),
      owner_key_(nss->GetOwnerKeyFilePath(), nss),
      mitigator_(key_gen) {
}

SessionManagerImpl::~SessionManagerImpl() {
  STLDeleteValues(&user_sessions_);
  device_policy_->set_delegate(NULL);  // Could use WeakPtr instead?
}

void SessionManagerImpl::InjectPolicyServices(
    scoped_ptr<DevicePolicyService> device_policy,
    scoped_ptr<UserPolicyServiceFactory> user_policy_factory,
    scoped_ptr<DeviceLocalAccountPolicyService> device_local_account_policy) {
  device_policy_ = device_policy.Pass();
  user_policy_factory_ = user_policy_factory.Pass();
  device_local_account_policy_ = device_local_account_policy.Pass();
}

void SessionManagerImpl::AnnounceSessionStoppingIfNeeded() {
  if (session_started_) {
    session_stopping_ = true;
    DLOG(INFO) << "emitting D-Bus signal SessionStateChanged:" << kStopping;
    dbus_emitter_->EmitSignalWithString(kSessionStateChangedSignal, kStopping);
  }
}

void SessionManagerImpl::AnnounceSessionStopped() {
  session_stopping_ = session_started_ = false;
  DLOG(INFO) << "emitting D-Bus signal SessionStateChanged:" << kStopped;
  dbus_emitter_->EmitSignalWithString(kSessionStateChangedSignal, kStopped);
}

bool SessionManagerImpl::Initialize() {
  scoped_refptr<base::MessageLoopProxy> loop_proxy =
      base::MessageLoop::current()->message_loop_proxy();

  key_gen_->set_delegate(this);

  device_policy_.reset(DevicePolicyService::Create(
      login_metrics_, &owner_key_, &mitigator_, nss_, loop_proxy));
  device_policy_->set_delegate(this);

  user_policy_factory_.reset(
      new UserPolicyServiceFactory(getuid(), loop_proxy, nss_, system_));
  device_local_account_policy_.reset(new DeviceLocalAccountPolicyService(
      base::FilePath(kDeviceLocalAccountStateDir), &owner_key_, loop_proxy));

  if (device_policy_->Initialize()) {
    device_local_account_policy_->UpdateDeviceSettings(
        device_policy_->GetSettings());
    UpdateSystemSettings();
    return true;
  }
  return false;
}

void SessionManagerImpl::Finalize() {
  device_policy_->PersistPolicySync();
  for (UserSessionMap::const_iterator it = user_sessions_.begin();
       it != user_sessions_.end();
       ++it) {
    if (it->second)
      it->second->policy_service->PersistPolicySync();
  }
}

void SessionManagerImpl::EmitLoginPromptVisible(Error* error) {
  login_metrics_->RecordStats("login-prompt-visible");
  dbus_emitter_->EmitSignal(kLoginPromptVisibleSignal);
  scoped_ptr<dbus::Response> emit_response =
      upstart_signal_emitter_->EmitSignal("login-prompt-visible",
                                          std::vector<std::string>());
  if (!emit_response) {
    const char msg[] = "Emitting login-prompt-visible upstart signal failed.";
    LOG(ERROR) << msg;
    error->Set(dbus_error::kEmitFailed, msg);
  }
}

std::string SessionManagerImpl::EnableChromeTesting(
    bool relaunch,
    std::vector<std::string> extra_args,
    Error* error) {
  // Check to see if we already have Chrome testing enabled.
  bool already_enabled = !chrome_testing_path_.empty();

  if (!already_enabled) {
    base::FilePath temp_file_path;  // So we don't clobber chrome_testing_path_;
    if (!system_->GetUniqueFilenameInWriteOnlyTempDir(&temp_file_path)) {
      error->Set(dbus_error::kTestingChannelError,
                 "Could not create testing channel filename.");
      return std::string();
    }
    chrome_testing_path_ = temp_file_path;
  }

  if (!already_enabled || relaunch) {
    // Delete testing channel file if it already exists.
    system_->RemoveFile(chrome_testing_path_);

    // Add testing channel argument to extra arguments.
    std::string testing_argument = kTestingChannelFlag;
    testing_argument.append(chrome_testing_path_.value());
    extra_args.push_back(testing_argument);

    manager_->RestartBrowserWithArgs(extra_args, true);
  }
  return chrome_testing_path_.value();
}

bool SessionManagerImpl::StartSession(const std::string& email,
                                      const std::string& unique_id,
                                      Error* error) {
  // Validate the |email|.
  const std::string email_string(base::StringToLowerASCII(email));
  const bool is_incognito =
      ((email_string == kGuestUserName) || (email_string == kDemoUser));
  if (!is_incognito && !ValidateEmail(email_string)) {
    const char msg[] = "Provided email address is not valid.  ASCII only.";
    LOG(ERROR) << msg;
    error->Set(dbus_error::kInvalidAccount, msg);
    return false;
  }

  // Check if this user already started a session.
  if (user_sessions_.count(email_string) > 0) {
    const char msg[] = "Provided email address already started a session.";
    LOG(ERROR) << msg;
    error->Set(dbus_error::kSessionExists, msg);
    return false;
  }

  // Create a UserSession object for this user.
  std::string error_name;
  scoped_ptr<UserSession> user_session(
      CreateUserSession(email_string, is_incognito, &error_name));
  if (!user_session.get()) {
    error->Set(error_name, "Can't create session.");
    return false;
  }
  // Check whether the current user is the owner, and if so make sure she is
  // whitelisted and has an owner key.
  bool user_is_owner = false;
  PolicyService::Error policy_error;
  if (!device_policy_->CheckAndHandleOwnerLogin(user_session->username,
                                                user_session->slot.get(),
                                                &user_is_owner,
                                                &policy_error)) {
    error->Set(policy_error.code(), policy_error.message());
    return false;
  }

  // If all previous sessions were incognito (or no previous sessions exist).
  bool is_first_real_user = AllSessionsAreIncognito() && !is_incognito;

  // Send each user login event to UMA (right before we start session
  // since the metrics library does not log events in guest mode).
  int dev_mode = system_->IsDevMode();
  if (dev_mode > -1)
    login_metrics_->SendLoginUserType(dev_mode, is_incognito, user_is_owner);

  scoped_ptr<dbus::Response> emit_response =
      upstart_signal_emitter_->EmitSignal(
          "start-user-session",
          std::vector<std::string>(1, "CHROMEOS_USER=" + email_string));

  if (!emit_response) {
    const char msg[] = "Emitting start-user-session upstart signal failed.";
    LOG(ERROR) << msg;
    error->Set(dbus_error::kEmitFailed, msg);
    return false;
  }
  LOG(INFO) << "Starting user session";
  manager_->SetBrowserSessionForUser(email_string, user_session->userhash);
  session_started_ = true;
  user_sessions_[email_string] = user_session.release();
  DLOG(INFO) << "emitting D-Bus signal SessionStateChanged:" << kStarted;
  dbus_emitter_->EmitSignalWithString(kSessionStateChangedSignal, kStarted);

  if (device_policy_->KeyMissing() && !device_policy_->Mitigating() &&
      is_first_real_user) {
    // This is the first sign-in on this unmanaged device.  Take ownership.
    key_gen_->Start(email_string);
  }

  // Record that a login has successfully completed on this boot.
  system_->AtomicFileWrite(base::FilePath(kLoggedInFlag), "1");
  return true;
}

bool SessionManagerImpl::StopSession() {
  LOG(INFO) << "Stopping all sessions";
  // Most calls to StopSession() will log the reason for the call.
  // If you don't see a log message saying the reason for the call, it is
  // likely a DBUS message. See dbus_glib_shim.cc for that call.
  manager_->ScheduleShutdown();
  // TODO(cmasone): re-enable these when we try to enable logout without exiting
  //                the session manager
  // browser_.job->StopSession();
  // user_policy_.reset();
  // session_started_ = false;
  return true;
}

void SessionManagerImpl::StorePolicy(const uint8_t* policy_blob,
                                     size_t policy_blob_len,
                                     PolicyService::Completion* completion) {
  int flags = PolicyService::KEY_ROTATE;
  if (!session_started_)
    flags |= PolicyService::KEY_INSTALL_NEW | PolicyService::KEY_CLOBBER;
  device_policy_->Store(policy_blob, policy_blob_len, completion, flags);
}

void SessionManagerImpl::RetrievePolicy(std::vector<uint8_t>* policy_data,
                                        Error* error) {
  if (!device_policy_->Retrieve(policy_data)) {
    const char msg[] = "Failed to retrieve policy data.";
    LOG(ERROR) << msg;
    error->Set(dbus_error::kSigEncodeFail, msg);
    return;
  }
}

void SessionManagerImpl::StorePolicyForUser(
    const std::string& user_email,
    const uint8_t* policy_blob,
    size_t policy_blob_len,
    PolicyService::Completion* completion) {
  PolicyService* policy_service = GetPolicyService(user_email);
  if (!policy_service) {
    PolicyService::Error error(
        dbus_error::kSessionDoesNotExist,
        "Cannot store user policy before session is started.");
    LOG(ERROR) << error.message();
    completion->ReportFailure(error);
    return;
  }

  policy_service->Store(
      policy_blob,
      policy_blob_len,
      completion,
      PolicyService::KEY_INSTALL_NEW | PolicyService::KEY_ROTATE);
}

void SessionManagerImpl::RetrievePolicyForUser(
    const std::string& user_email,
    std::vector<uint8_t>* policy_data,
    Error* error) {
  PolicyService* policy_service = GetPolicyService(user_email);
  if (!policy_service) {
    const char msg[] = "Cannot retrieve user policy before session is started.";
    LOG(ERROR) << msg;
    error->Set(dbus_error::kSessionDoesNotExist, msg);
    return;
  }
  if (!policy_service->Retrieve(policy_data)) {
    const char msg[] = "Failed to retrieve policy data.";
    LOG(ERROR) << msg;
    error->Set(dbus_error::kSigEncodeFail, msg);
  }
}

void SessionManagerImpl::StoreDeviceLocalAccountPolicy(
    const std::string& account_id,
    const uint8_t* policy_blob,
    size_t policy_blob_len,
    PolicyService::Completion* completion) {
  device_local_account_policy_->Store(
      account_id, policy_blob, policy_blob_len, completion);
}

void SessionManagerImpl::RetrieveDeviceLocalAccountPolicy(
    const std::string& account_id,
    std::vector<uint8_t>* policy_data,
    Error* error) {
  if (!device_local_account_policy_->Retrieve(account_id, policy_data)) {
    const char msg[] = "Failed to retrieve policy data.";
    LOG(ERROR) << msg;
    error->Set(dbus_error::kSigEncodeFail, msg);
    return;
  }
}

const char* SessionManagerImpl::RetrieveSessionState() {
  if (!session_started_)
    return kStopped;
  else
    return (session_stopping_ ? kStopping : kStarted);
}

void SessionManagerImpl::RetrieveActiveSessions(
    std::map<std::string, std::string>* active_sessions) {
  for (UserSessionMap::const_iterator it = user_sessions_.begin();
       it != user_sessions_.end();
       ++it) {
    if (it->second) {
      (*active_sessions)[it->second->username] = it->second->userhash;
    }
  }
}

void SessionManagerImpl::LockScreen(Error* error) {
  if (!session_started_) {
    const char msg[] = "Attempt to lock screen outside of user session.";
    LOG(WARNING) << msg;
    error->Set(dbus_error::kSessionDoesNotExist, msg);
    return;
  }
  // If all sessions are incognito, then locking is not allowed.
  if (AllSessionsAreIncognito()) {
    const char msg[] = "Attempt to lock screen during Guest session.";
    LOG(WARNING) << msg;
    error->Set(dbus_error::kSessionExists, msg);
    return;
  }
  if (!screen_locked_) {
    screen_locked_ = true;
    lock_screen_closure_.Run();
  }
  LOG(INFO) << "LockScreen() method called.";
}

void SessionManagerImpl::HandleLockScreenShown() {
  LOG(INFO) << "HandleLockScreenShown() method called.";
  dbus_emitter_->EmitSignal(kScreenIsLockedSignal);
}

void SessionManagerImpl::HandleLockScreenDismissed() {
  screen_locked_ = false;
  LOG(INFO) << "HandleLockScreenDismissed() method called.";
  dbus_emitter_->EmitSignal(kScreenIsUnlockedSignal);
}

bool SessionManagerImpl::RestartJob(pid_t pid,
                                    const std::string& arguments,
                                    Error* error) {
  if (!manager_->IsBrowser(pid)) {
    const char msg[] = "Provided pid is unknown.";
    LOG(ERROR) << msg;
    error->Set(dbus_error::kUnknownPid, msg);
    return false;
  }

  gchar** argv = NULL;
  gint argc = 0;
  GError* parse_error = NULL;
  if (!g_shell_parse_argv(arguments.c_str(), &argc, &argv, &parse_error)) {
    LOG(ERROR) << "Could not parse command: " << parse_error->message;
    g_strfreev(argv);
    error->Set(DBUS_ERROR_INVALID_ARGS, parse_error->message);
    return false;
  }
  CommandLine new_command_line(argc, argv);
  g_strfreev(argv);

  // To set "logged-in" state for BWSI mode.
  if (!StartSession(kGuestUserName, "", error))
    return false;
  manager_->RestartBrowserWithArgs(new_command_line.argv(), false);
  return true;
}

void SessionManagerImpl::StartDeviceWipe(Error* error) {
  const base::FilePath session_path(kLoggedInFlag);
  if (system_->Exists(session_path)) {
    const char msg[] = "A user has already logged in this boot.";
    LOG(ERROR) << msg;
    error->Set(dbus_error::kSessionExists, msg);
    return;
  }
  InitiateDeviceWipe();
}

void SessionManagerImpl::SetFlagsForUser(
    const std::string& user_email,
    const std::vector<std::string>& session_user_flags) {
  manager_->SetFlagsForUser(user_email, session_user_flags);
}

void SessionManagerImpl::RequestServerBackedStateKeys(
    const ServerBackedStateKeyGenerator::StateKeyCallback& callback) {
  state_key_generator_->RequestStateKeys(callback);
}

void SessionManagerImpl::InitMachineInfo(const std::string& data,
                                         Error* error) {
  std::map<std::string, std::string> params;
  if (!ServerBackedStateKeyGenerator::ParseMachineInfo(data, &params))
    error->Set(dbus_error::kInitMachineInfoFail, "Parse failure.");

  if (!state_key_generator_->InitMachineInfo(params))
    error->Set(dbus_error::kInitMachineInfoFail, "Missing parameters.");
}

void SessionManagerImpl::OnPolicyPersisted(bool success) {
  dbus_emitter_->EmitSignalWithSuccessFailure(kPropertyChangeCompleteSignal,
                                              success);
  device_local_account_policy_->UpdateDeviceSettings(
      device_policy_->GetSettings());
  UpdateSystemSettings();
}

void SessionManagerImpl::OnKeyPersisted(bool success) {
  dbus_emitter_->EmitSignalWithSuccessFailure(kOwnerKeySetSignal, success);
}

void SessionManagerImpl::OnKeyGenerated(const std::string& username,
                                        const base::FilePath& temp_key_file) {
  ImportValidateAndStoreGeneratedKey(username, temp_key_file);
}

void SessionManagerImpl::ImportValidateAndStoreGeneratedKey(
    const std::string& username,
    const base::FilePath& temp_key_file) {
  DLOG(INFO) << "Processing generated key at " << temp_key_file.value();
  std::string key;
  base::ReadFileToString(temp_key_file, &key);
  PLOG_IF(WARNING, !base::DeleteFile(temp_key_file, false))
      << "Can't delete " << temp_key_file.value();
  device_policy_->ValidateAndStoreOwnerKey(
      username, key, user_sessions_[username]->slot.get());
}

void SessionManagerImpl::InitiateDeviceWipe() {
  const base::FilePath reset_path(kResetFile);
  system_->AtomicFileWrite(reset_path, "fast safe");
  restart_device_closure_.Run();
}

// static
bool SessionManagerImpl::ValidateEmail(const std::string& email_address) {
  if (email_address.find_first_not_of(kLegalCharacters) != std::string::npos)
    return false;

  size_t at = email_address.find(kEmailSeparator);
  // it has NO @.
  if (at == std::string::npos)
    return false;

  // it has more than one @.
  if (email_address.find(kEmailSeparator, at + 1) != std::string::npos)
    return false;

  return true;
}

bool SessionManagerImpl::AllSessionsAreIncognito() {
  size_t incognito_count = 0;
  for (UserSessionMap::const_iterator it = user_sessions_.begin();
       it != user_sessions_.end();
       ++it) {
    if (it->second)
      incognito_count += it->second->is_incognito;
  }
  return incognito_count == user_sessions_.size();
}

SessionManagerImpl::UserSession* SessionManagerImpl::CreateUserSession(
    const std::string& username,
    bool is_incognito,
    std::string* error_message) {
  scoped_ptr<PolicyService> user_policy(user_policy_factory_->Create(username));
  if (!user_policy) {
    LOG(ERROR) << "User policy failed to initialize.";
    if (error_message)
      *error_message = dbus_error::kPolicyInitFail;
    return NULL;
  }
  crypto::ScopedPK11Slot slot(nss_->OpenUserDB(GetUserPath(username)));
  if (!slot) {
    LOG(ERROR) << "Could not open the current user's NSS database.";
    if (error_message)
      *error_message = dbus_error::kNoUserNssDb;
    return NULL;
  }
  return new SessionManagerImpl::UserSession(username,
                                             SanitizeUserName(username),
                                             is_incognito,
                                             slot.Pass(),
                                             user_policy.Pass());
}

PolicyService* SessionManagerImpl::GetPolicyService(const std::string& user) {
  UserSessionMap::const_iterator it = user_sessions_.find(user);
  return it == user_sessions_.end() ? NULL : it->second->policy_service.get();
}

void SessionManagerImpl::UpdateSystemSettings() {
  // Only write settings when device ownership is established.
  if (!owner_key_.IsPopulated())
    return;

  int block_devmode_setting =
      device_policy_->GetSettings().system_settings().block_devmode() ? 1 : 0;
  int block_devmode_value = VbGetSystemPropertyInt(kCrossystemBlockDevmode);
  if (block_devmode_value == -1)
    LOG(ERROR) << "Failed to read block_devmode flag!";

  if (block_devmode_setting != block_devmode_value) {
    if (VbSetSystemPropertyInt(kCrossystemBlockDevmode, block_devmode_setting))
      LOG(ERROR) << "Failed to write block_devmode flag!";
  }
}

}  // namespace login_manager
