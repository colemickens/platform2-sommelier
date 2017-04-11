// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_dbus_adaptor.h"

#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/files/file_util.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/exported_object.h>
#include <dbus/file_descriptor.h>
#include <dbus/message.h>

#include "login_manager/policy_service.h"

namespace login_manager {
namespace {

const char kBindingsPath[] =
    "/usr/share/dbus-1/interfaces/org.chromium.SessionManagerInterface.xml";
const char kDBusIntrospectableInterface[] =
    "org.freedesktop.DBus.Introspectable";
const char kDBusIntrospectMethod[] = "Introspect";

// Passes |method_call| to |handler| and passes the response to
// |response_sender|. If |handler| returns NULL, an empty response is created
// and sent.
void HandleSynchronousDBusMethodCall(
    base::Callback<std::unique_ptr<dbus::Response>(dbus::MethodCall*)> handler,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response = handler.Run(method_call);
  if (!response)
    response = dbus::Response::FromMethodCall(method_call);
  response_sender.Run(std::move(response));
}

std::unique_ptr<dbus::Response> CreateError(dbus::MethodCall* call,
                                            const std::string& name,
                                            const std::string& message) {
  return dbus::ErrorResponse::FromMethodCall(call, name, message);
}

// Creates a new "invalid args" reply to call.
std::unique_ptr<dbus::Response> CreateInvalidArgsError(dbus::MethodCall* call,
                                                       std::string message) {
  return CreateError(call, DBUS_ERROR_INVALID_ARGS, "Signature is: " + message);
}

// Craft a Response to call that is appropriate, given the contents of error.
// If error is set, this will be an ErrorResponse. Otherwise, it will be a
// Response containing payload.
std::unique_ptr<dbus::Response> CraftAppropriateResponseWithBool(
    dbus::MethodCall* call,
    const SessionManagerImpl::Error& error,
    bool payload) {
  std::unique_ptr<dbus::Response> response;
  if (error.is_set()) {
    response = CreateError(call, error.name(), error.message());
  } else {
    response = dbus::Response::FromMethodCall(call);
    dbus::MessageWriter writer(response.get());
    writer.AppendBool(payload);
  }
  return response;
}

std::unique_ptr<dbus::Response> CraftAppropriateResponseWithString(
    dbus::MethodCall* call,
    const SessionManagerImpl::Error& error,
    const std::string& payload) {
  std::unique_ptr<dbus::Response> response;
  if (error.is_set()) {
    response = CreateError(call, error.name(), error.message());
  } else {
    response = dbus::Response::FromMethodCall(call);
    dbus::MessageWriter writer(response.get());
    writer.AppendString(payload);
  }
  return response;
}

std::unique_ptr<dbus::Response> CraftAppropriateResponseWithBytes(
    dbus::MethodCall* call,
    const SessionManagerImpl::Error& error,
    const std::vector<uint8_t>& payload) {
  std::unique_ptr<dbus::Response> response;
  if (error.is_set()) {
    response = CreateError(call, error.name(), error.message());
  } else {
    response = dbus::Response::FromMethodCall(call);
    dbus::MessageWriter writer(response.get());
    writer.AppendArrayOfBytes(payload.data(), payload.size());
  }
  return response;
}

// Handles completion of a server-backed state key retrieval operation and
// passes the response back to the waiting DBus invocation context.
void HandleGetServerBackedStateKeysCompletion(
    dbus::MethodCall* call,
    const dbus::ExportedObject::ResponseSender& sender,
    const std::vector<std::vector<uint8_t>>& state_keys) {
  std::unique_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(call));
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("ay", &array_writer);
  for (std::vector<std::vector<uint8_t>>::const_iterator state_key(
           state_keys.begin());
       state_key != state_keys.end();
       ++state_key) {
    array_writer.AppendArrayOfBytes(state_key->data(), state_key->size());
  }
  writer.CloseContainer(&array_writer);
  sender.Run(std::move(response));
}

}  // namespace

// Callback that forwards a result to a DBus invocation context.
class DBusMethodCompletion {
 public:
  static PolicyService::Completion CreateCallback(
      dbus::MethodCall* call,
      const dbus::ExportedObject::ResponseSender& sender);

  // Permits objects to be destroyed before their calls have been completed. Can
  // be called during shutdown to abandon in-progress calls.
  static void AllowAbandonment();

  virtual ~DBusMethodCompletion();

 private:
  DBusMethodCompletion() : call_(nullptr) {}
  DBusMethodCompletion(dbus::MethodCall* call,
                       const dbus::ExportedObject::ResponseSender& sender);
  void HandleResult(const PolicyService::Error& error);

  // Should we allow destroying objects before their calls have been completed?
  static bool s_allow_abandonment_;

  dbus::MethodCall* call_ = nullptr;  // Weak, owned by caller.
  dbus::ExportedObject::ResponseSender sender_;

  DISALLOW_COPY_AND_ASSIGN(DBusMethodCompletion);
};

bool DBusMethodCompletion::s_allow_abandonment_ = false;

// static
PolicyService::Completion DBusMethodCompletion::CreateCallback(
      dbus::MethodCall* call,
      const dbus::ExportedObject::ResponseSender& sender) {
  return base::Bind(&DBusMethodCompletion::HandleResult,
                    base::Owned(new DBusMethodCompletion(call, sender)));
}

// static
void DBusMethodCompletion::AllowAbandonment() {
  s_allow_abandonment_ = true;
}

DBusMethodCompletion::~DBusMethodCompletion() {
  if (call_ && !s_allow_abandonment_) {
    NOTREACHED() << "Unfinished D-Bus call!";
    sender_.Run(dbus::Response::FromMethodCall(call_));
  }
}

DBusMethodCompletion::DBusMethodCompletion(
    dbus::MethodCall* call,
    const dbus::ExportedObject::ResponseSender& sender)
    : call_(call), sender_(sender) {
}

void DBusMethodCompletion::HandleResult(const PolicyService::Error& error) {
  if (error.code() == dbus_error::kNone) {
    std::unique_ptr<dbus::Response> response(
        dbus::Response::FromMethodCall(call_));
    dbus::MessageWriter writer(response.get());
    writer.AppendBool(true);
    sender_.Run(std::move(response));
    call_ = nullptr;
  } else {
    sender_.Run(
        dbus::ErrorResponse::FromMethodCall(call_,
                                            error.code(), error.message()));
    call_ = nullptr;
  }
}

SessionManagerDBusAdaptor::SessionManagerDBusAdaptor(SessionManagerImpl* impl)
    : impl_(impl) {
  CHECK(impl_);
}

SessionManagerDBusAdaptor::~SessionManagerDBusAdaptor() {
  // Abandon in-progress incoming D-Bus method calls.
  DBusMethodCompletion::AllowAbandonment();
}

void SessionManagerDBusAdaptor::ExportDBusMethods(
    dbus::ExportedObject* object) {
  ExportSyncDBusMethod(object,
                       kSessionManagerEmitLoginPromptVisible,
                       &SessionManagerDBusAdaptor::EmitLoginPromptVisible);
  ExportSyncDBusMethod(object,
                       "EnableChromeTesting",
                       &SessionManagerDBusAdaptor::EnableChromeTesting);
  ExportSyncDBusMethod(object,
                       kSessionManagerStartSession,
                       &SessionManagerDBusAdaptor::StartSession);
  ExportSyncDBusMethod(object,
                       kSessionManagerStopSession,
                       &SessionManagerDBusAdaptor::StopSession);

  ExportAsyncDBusMethod(object,
                        kSessionManagerStorePolicy,
                        &SessionManagerDBusAdaptor::StorePolicy);
  ExportAsyncDBusMethod(object,
                        kSessionManagerStoreUnsignedPolicy,
                        &SessionManagerDBusAdaptor::StoreUnsignedPolicy);
  ExportSyncDBusMethod(object,
                       kSessionManagerRetrievePolicy,
                       &SessionManagerDBusAdaptor::RetrievePolicy);

  ExportAsyncDBusMethod(object,
                        kSessionManagerStorePolicyForUser,
                        &SessionManagerDBusAdaptor::StorePolicyForUser);
  ExportAsyncDBusMethod(object,
                        kSessionManagerStoreUnsignedPolicyForUser,
                        &SessionManagerDBusAdaptor::StoreUnsignedPolicyForUser);
  ExportSyncDBusMethod(object,
                       kSessionManagerRetrievePolicyForUser,
                       &SessionManagerDBusAdaptor::RetrievePolicyForUser);

  ExportAsyncDBusMethod(
      object,
      kSessionManagerStoreDeviceLocalAccountPolicy,
      &SessionManagerDBusAdaptor::StoreDeviceLocalAccountPolicy);
  ExportSyncDBusMethod(
      object,
      kSessionManagerRetrieveDeviceLocalAccountPolicy,
      &SessionManagerDBusAdaptor::RetrieveDeviceLocalAccountPolicy);

  ExportSyncDBusMethod(object,
                       kSessionManagerRetrieveSessionState,
                       &SessionManagerDBusAdaptor::RetrieveSessionState);
  ExportSyncDBusMethod(object,
                       kSessionManagerRetrieveActiveSessions,
                       &SessionManagerDBusAdaptor::RetrieveActiveSessions);

  ExportSyncDBusMethod(
      object,
      kSessionManagerHandleSupervisedUserCreationStarting,
      &SessionManagerDBusAdaptor::HandleSupervisedUserCreationStarting);
  ExportSyncDBusMethod(
      object,
      kSessionManagerHandleSupervisedUserCreationFinished,
      &SessionManagerDBusAdaptor::HandleSupervisedUserCreationFinished);
  ExportSyncDBusMethod(object,
                       kSessionManagerLockScreen,
                       &SessionManagerDBusAdaptor::LockScreen);
  ExportSyncDBusMethod(object,
                       kSessionManagerHandleLockScreenShown,
                       &SessionManagerDBusAdaptor::HandleLockScreenShown);
  ExportSyncDBusMethod(object,
                       kSessionManagerHandleLockScreenDismissed,
                       &SessionManagerDBusAdaptor::HandleLockScreenDismissed);

  ExportSyncDBusMethod(object,
                       kSessionManagerRestartJob,
                       &SessionManagerDBusAdaptor::RestartJob);
  ExportSyncDBusMethod(object,
                       kSessionManagerStartDeviceWipe,
                       &SessionManagerDBusAdaptor::StartDeviceWipe);
  ExportSyncDBusMethod(object,
                       kSessionManagerSetFlagsForUser,
                       &SessionManagerDBusAdaptor::SetFlagsForUser);

  ExportAsyncDBusMethod(object,
                        kSessionManagerGetServerBackedStateKeys,
                        &SessionManagerDBusAdaptor::GetServerBackedStateKeys);
  ExportSyncDBusMethod(object,
                       kSessionManagerInitMachineInfo,
                       &SessionManagerDBusAdaptor::InitMachineInfo);

  ExportSyncDBusMethod(object,
                       kSessionManagerStartContainer,
                       &SessionManagerDBusAdaptor::StartContainer);
  ExportSyncDBusMethod(object,
                       kSessionManagerStopContainer,
                       &SessionManagerDBusAdaptor::StopContainer);
  ExportSyncDBusMethod(object,
                       kSessionManagerStartArcInstance,
                       &SessionManagerDBusAdaptor::StartArcInstance);
  ExportSyncDBusMethod(object,
                       kSessionManagerStopArcInstance,
                       &SessionManagerDBusAdaptor::StopArcInstance);
  ExportSyncDBusMethod(object,
                       kSessionManagerSetArcCpuRestriction,
                       &SessionManagerDBusAdaptor::SetArcCpuRestriction);
  ExportSyncDBusMethod(object,
                       kSessionManagerEmitArcBooted,
                       &SessionManagerDBusAdaptor::EmitArcBooted);
  ExportSyncDBusMethod(object,
                       kSessionManagerGetArcStartTimeTicks,
                       &SessionManagerDBusAdaptor::GetArcStartTimeTicks);
  ExportSyncDBusMethod(object,
                       kSessionManagerRemoveArcData,
                       &SessionManagerDBusAdaptor::RemoveArcData);

  CHECK(object->ExportMethodAndBlock(
      kDBusIntrospectableInterface,
      kDBusIntrospectMethod,
      base::Bind(&HandleSynchronousDBusMethodCall,
                 base::Bind(&SessionManagerDBusAdaptor::Introspect,
                            base::Unretained(this)))));
}

std::unique_ptr<dbus::Response>
SessionManagerDBusAdaptor::EmitLoginPromptVisible(dbus::MethodCall* call) {
  impl_->EmitLoginPromptVisible();
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::EnableChromeTesting(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  bool relaunch;
  std::vector<std::string> extra_args;
  if (!reader.PopBool(&relaunch) || !reader.PopArrayOfStrings(&extra_args))
    return CreateInvalidArgsError(call, call->GetSignature());

  SessionManagerImpl::Error error;
  std::string testing_path =
      impl_->EnableChromeTesting(relaunch, extra_args, &error);
  return CraftAppropriateResponseWithString(call, error, testing_path);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::StartSession(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  std::string account_id, unique_id;
  if (!reader.PopString(&account_id) || !reader.PopString(&unique_id))
    return CreateInvalidArgsError(call, call->GetSignature());

  SessionManagerImpl::Error error;
  bool success = impl_->StartSession(account_id, unique_id, &error);
  return CraftAppropriateResponseWithBool(call, error, success);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::StopSession(
    dbus::MethodCall* call) {
  // Though this takes a string (unique_id), it is ignored.
  bool success = impl_->StopSession();
  std::unique_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(call));
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(success);
  return response;
}

void SessionManagerDBusAdaptor::StorePolicy(
    dbus::MethodCall* call,
    dbus::ExportedObject::ResponseSender sender) {
  DoStorePolicy(call, sender, SignatureCheck::kEnabled);
}

void SessionManagerDBusAdaptor::StoreUnsignedPolicy(
    dbus::MethodCall* call,
    dbus::ExportedObject::ResponseSender sender) {
  DoStorePolicy(call, sender, SignatureCheck::kDisabled);
}

void SessionManagerDBusAdaptor::DoStorePolicy(
    dbus::MethodCall* call,
    dbus::ExportedObject::ResponseSender sender,
    SignatureCheck signature_check) {
  const uint8_t* policy_blob = NULL;
  size_t policy_blob_len = 0;
  dbus::MessageReader reader(call);
  // policy_blob points into reader after pop.
  if (!reader.PopArrayOfBytes(&policy_blob, &policy_blob_len)) {
    sender.Run(CreateInvalidArgsError(call, call->GetSignature()));
  } else {
    impl_->StorePolicy(policy_blob, policy_blob_len, signature_check,
                       DBusMethodCompletion::CreateCallback(call, sender));
    // Response will be sent asynchronously.
  }
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::RetrievePolicy(
    dbus::MethodCall* call) {
  std::vector<uint8_t> policy_data;
  SessionManagerImpl::Error error;
  impl_->RetrievePolicy(&policy_data, &error);
  return CraftAppropriateResponseWithBytes(call, error, policy_data);
}

void SessionManagerDBusAdaptor::StorePolicyForUser(
    dbus::MethodCall* call,
    dbus::ExportedObject::ResponseSender sender) {
  DoStorePolicyForUser(call, sender, SignatureCheck::kEnabled);
}

void SessionManagerDBusAdaptor::StoreUnsignedPolicyForUser(
    dbus::MethodCall* call,
    dbus::ExportedObject::ResponseSender sender) {
  DoStorePolicyForUser(call, sender, SignatureCheck::kDisabled);
}

void SessionManagerDBusAdaptor::DoStorePolicyForUser(
    dbus::MethodCall* call,
    dbus::ExportedObject::ResponseSender sender,
    SignatureCheck signature_check) {
  std::string account_id;
  const uint8_t* policy_blob = NULL;
  size_t policy_blob_len = 0;
  dbus::MessageReader reader(call);
  // policy_blob points into reader after pop.
  if (!reader.PopString(&account_id) ||
      !reader.PopArrayOfBytes(&policy_blob, &policy_blob_len)) {
    sender.Run(CreateInvalidArgsError(call, call->GetSignature()));
  } else {
    impl_->StorePolicyForUser(
        account_id, policy_blob, policy_blob_len, signature_check,
        DBusMethodCompletion::CreateCallback(call, sender));
    // Response will normally be sent asynchronously.
  }
}

std::unique_ptr<dbus::Response>
SessionManagerDBusAdaptor::RetrievePolicyForUser(dbus::MethodCall* call) {
  std::string account_id;
  dbus::MessageReader reader(call);

  if (!reader.PopString(&account_id))
    return CreateInvalidArgsError(call, call->GetSignature());

  std::vector<uint8_t> policy_data;
  SessionManagerImpl::Error error;
  impl_->RetrievePolicyForUser(account_id, &policy_data, &error);
  return CraftAppropriateResponseWithBytes(call, error, policy_data);
}

void SessionManagerDBusAdaptor::StoreDeviceLocalAccountPolicy(
    dbus::MethodCall* call,
    dbus::ExportedObject::ResponseSender sender) {
  std::string account_id;
  const uint8_t* policy_blob = NULL;
  size_t policy_blob_len = 0;
  dbus::MessageReader reader(call);
  // policy_blob points into reader after pop.
  if (!reader.PopString(&account_id) ||
      !reader.PopArrayOfBytes(&policy_blob, &policy_blob_len)) {
    sender.Run(CreateInvalidArgsError(call, call->GetSignature()));
  } else {
    impl_->StoreDeviceLocalAccountPolicy(
        account_id,
        policy_blob,
        policy_blob_len,
        DBusMethodCompletion::CreateCallback(call, sender));
    // Response will be sent asynchronously.
  }
}

std::unique_ptr<dbus::Response>
SessionManagerDBusAdaptor::RetrieveDeviceLocalAccountPolicy(
    dbus::MethodCall* call) {
  std::string account_id;
  dbus::MessageReader reader(call);

  if (!reader.PopString(&account_id))
    return CreateInvalidArgsError(call, call->GetSignature());

  std::vector<uint8_t> policy_data;
  SessionManagerImpl::Error error;
  impl_->RetrieveDeviceLocalAccountPolicy(account_id, &policy_data, &error);
  return CraftAppropriateResponseWithBytes(call, error, policy_data);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::RetrieveSessionState(
    dbus::MethodCall* call) {
  std::unique_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(call));
  dbus::MessageWriter writer(response.get());
  writer.AppendString(impl_->RetrieveSessionState());
  return response;
}

std::unique_ptr<dbus::Response>
SessionManagerDBusAdaptor::RetrieveActiveSessions(dbus::MethodCall* call) {
  std::map<std::string, std::string> sessions;
  impl_->RetrieveActiveSessions(&sessions);

  std::unique_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(call));
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{ss}", &array_writer);
  for (std::map<std::string, std::string>::const_iterator it = sessions.begin();
       it != sessions.end();
       ++it) {
    dbus::MessageWriter entry_writer(NULL);
    array_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendString(it->first);
    entry_writer.AppendString(it->second);
    array_writer.CloseContainer(&entry_writer);
  }
  writer.CloseContainer(&array_writer);
  return response;
}

std::unique_ptr<dbus::Response>
SessionManagerDBusAdaptor::HandleSupervisedUserCreationStarting(
    dbus::MethodCall* call) {
  impl_->HandleSupervisedUserCreationStarting();
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response>
SessionManagerDBusAdaptor::HandleSupervisedUserCreationFinished(
    dbus::MethodCall* call) {
  impl_->HandleSupervisedUserCreationFinished();
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::LockScreen(
    dbus::MethodCall* call) {
  SessionManagerImpl::Error error;
  impl_->LockScreen(&error);

  if (error.is_set())
    return CreateError(call, error.name(), error.message());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response>
SessionManagerDBusAdaptor::HandleLockScreenShown(dbus::MethodCall* call) {
  impl_->HandleLockScreenShown();
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response>
SessionManagerDBusAdaptor::HandleLockScreenDismissed(dbus::MethodCall* call) {
  impl_->HandleLockScreenDismissed();
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::RestartJob(
    dbus::MethodCall* call) {
  dbus::FileDescriptor fd;
  std::vector<std::string> argv;
  dbus::MessageReader reader(call);
  if (!reader.PopFileDescriptor(&fd) || !reader.PopArrayOfStrings(&argv))
    return CreateInvalidArgsError(call, call->GetSignature());

  fd.CheckValidity();
  CHECK(fd.is_valid());

  SessionManagerImpl::Error error;
  if (impl_->RestartJob(fd.value(), argv, &error))
    return dbus::Response::FromMethodCall(call);
  return CreateError(call, error.name(), error.message());
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::StartDeviceWipe(
    dbus::MethodCall* call) {
  SessionManagerImpl::Error error;
  impl_->StartDeviceWipe("session_manager_dbus_request", &error);
  return CraftAppropriateResponseWithBool(call, error, true);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::SetFlagsForUser(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  std::string account_id;
  std::vector<std::string> session_user_flags;
  if (!reader.PopString(&account_id) ||
      !reader.PopArrayOfStrings(&session_user_flags)) {
    return CreateInvalidArgsError(call, call->GetSignature());
  }
  impl_->SetFlagsForUser(account_id, session_user_flags);
  return dbus::Response::FromMethodCall(call);
}

void SessionManagerDBusAdaptor::GetServerBackedStateKeys(
    dbus::MethodCall* call,
    dbus::ExportedObject::ResponseSender sender) {
  std::vector<std::vector<uint8_t>> state_keys;
  impl_->RequestServerBackedStateKeys(
      base::Bind(&HandleGetServerBackedStateKeysCompletion, call, sender));
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::InitMachineInfo(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  std::string data;
  if (!reader.PopString(&data))
    return CreateInvalidArgsError(call, call->GetSignature());

  SessionManagerImpl::Error error;
  impl_->InitMachineInfo(data, &error);
  if (error.is_set())
    return CreateError(call, error.name(), error.message());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::StartContainer(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  std::string name;
  if (!reader.PopString(&name))
    return CreateInvalidArgsError(call, call->GetSignature());

  SessionManagerImpl::Error error;
  impl_->StartContainer(name, &error);
  if (error.is_set())
    return CreateError(call, error.name(), error.message());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::StopContainer(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  std::string name;
  if (!reader.PopString(&name))
    return CreateInvalidArgsError(call, call->GetSignature());

  SessionManagerImpl::Error error;
  impl_->StopContainer(name, &error);
  if (error.is_set())
    return CreateError(call, error.name(), error.message());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::StartArcInstance(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  std::string account_id;
  if (!reader.PopString(&account_id))
    return CreateInvalidArgsError(call, call->GetSignature());

  bool disable_boot_completed_broadcast = false;
  if (!reader.PopBool(&disable_boot_completed_broadcast))
    return CreateInvalidArgsError(call, call->GetSignature());

  SessionManagerImpl::Error error;
  impl_->StartArcInstance(account_id, disable_boot_completed_broadcast, &error);
  if (error.is_set())
    return CreateError(call, error.name(), error.message());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::StopArcInstance(
    dbus::MethodCall* call) {
  SessionManagerImpl::Error error;
  impl_->StopArcInstance(&error);
  if (error.is_set())
    return CreateError(call, error.name(), error.message());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response>
SessionManagerDBusAdaptor::SetArcCpuRestriction(dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  uint32_t state_int;
  if (!reader.PopUint32(&state_int))
    return CreateInvalidArgsError(call, call->GetSignature());
  ContainerCpuRestrictionState state =
      static_cast<ContainerCpuRestrictionState>(state_int);

  SessionManagerImpl::Error error;
  impl_->SetArcCpuRestriction(state, &error);
  if (error.is_set())
    return CreateError(call, error.name(), error.message());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::EmitArcBooted(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  std::string account_id;
  if (!reader.PopString(&account_id)) {
    // TODO(xzhou): Return error here once Chrome is updated.
    LOG(WARNING) << "Failed to pop account_id in EmitArcBooted";
  }
  SessionManagerImpl::Error error;
  impl_->EmitArcBooted(account_id, &error);
  if (error.is_set())
    return CreateError(call, error.name(), error.message());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::GetArcStartTimeTicks(
    dbus::MethodCall* call) {
  SessionManagerImpl::Error error;
  base::TimeTicks start_time = impl_->GetArcStartTime(&error);
  if (error.is_set())
    return CreateError(call, error.name(), error.message());

  std::unique_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(call));
  dbus::MessageWriter writer(response.get());
  writer.AppendInt64(start_time.ToInternalValue());
  return response;
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::RemoveArcData(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  std::string account_id;
  if (!reader.PopString(&account_id))
    return CreateInvalidArgsError(call, call->GetSignature());

  SessionManagerImpl::Error error;
  impl_->RemoveArcData(account_id, &error);
  if (error.is_set())
    return CreateError(call, error.name(), error.message());

  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::Introspect(
    dbus::MethodCall* call) {
  std::string output;
  if (!base::ReadFileToString(base::FilePath(kBindingsPath), &output)) {
    PLOG(ERROR) << "Can't read XML bindings from disk:";
    return CreateError(call, "Can't read XML bindings from disk.", "");
  }
  std::unique_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(call));
  dbus::MessageWriter writer(response.get());
  writer.AppendString(output);
  return response;
}

void SessionManagerDBusAdaptor::ExportSyncDBusMethod(
    dbus::ExportedObject* object,
    const std::string& method_name,
    SyncDBusMethodCallMemberFunction member) {
  DCHECK(object);
  CHECK(object->ExportMethodAndBlock(
      kSessionManagerInterface,
      method_name,
      base::Bind(&HandleSynchronousDBusMethodCall,
                 base::Bind(member, base::Unretained(this)))));
}

void SessionManagerDBusAdaptor::ExportAsyncDBusMethod(
    dbus::ExportedObject* object,
    const std::string& method_name,
    AsyncDBusMethodCallMemberFunction member) {
  DCHECK(object);
  CHECK(
      object->ExportMethodAndBlock(kSessionManagerInterface,
                                   method_name,
                                   base::Bind(member, base::Unretained(this))));
}

}  // namespace login_manager
