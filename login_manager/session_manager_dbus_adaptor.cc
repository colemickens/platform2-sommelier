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
#include <base/memory/ptr_util.h>
#include <brillo/dbus/data_serialization.h>
#include <brillo/dbus/utils.h>
#include <brillo/errors/error_codes.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/exported_object.h>
#include <dbus/file_descriptor.h>
#include <dbus/message.h>

#include "login_manager/blob_util.h"
#include "login_manager/policy_service.h"
#include "login_manager/proto_bindings/arc.pb.h"

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

std::unique_ptr<dbus::Response> CreateStringResponse(
    dbus::MethodCall* call,
    const std::string& payload) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(call);
  dbus::MessageWriter writer(response.get());
  writer.AppendString(payload);
  return response;
}

std::unique_ptr<dbus::Response> CreateBytesResponse(
    dbus::MethodCall* call,
    const std::vector<uint8_t>& payload) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(call);
  dbus::MessageWriter writer(response.get());
  writer.AppendArrayOfBytes(payload.data(), payload.size());
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
  DBusMethodCompletion() = default;
  DBusMethodCompletion(dbus::MethodCall* call,
                       const dbus::ExportedObject::ResponseSender& sender);
  void HandleResult(brillo::ErrorPtr error);

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

void DBusMethodCompletion::HandleResult(brillo::ErrorPtr error) {
  sender_.Run(error ?
              brillo::dbus_utils::GetDBusError(call_, error.get()) :
              dbus::Response::FromMethodCall(call_));
  call_ = nullptr;
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

  brillo::ErrorPtr error;
  std::string testing_path;
  if (!impl_->EnableChromeTesting(
          &error, relaunch, extra_args, &testing_path)) {
    return brillo::dbus_utils::GetDBusError(call, error.get());
  }
  return CreateStringResponse(call, testing_path);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::StartSession(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  std::string account_id, unique_id;
  if (!reader.PopString(&account_id) || !reader.PopString(&unique_id))
    return CreateInvalidArgsError(call, call->GetSignature());

  brillo::ErrorPtr error;
  if (!impl_->StartSession(&error, account_id, unique_id))
    return brillo::dbus_utils::GetDBusError(call, error.get());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::StopSession(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  std::string unique_id;
  if (!reader.PopString(&unique_id))
    return CreateInvalidArgsError(call, call->GetSignature());

  impl_->StopSession(unique_id);
  return dbus::Response::FromMethodCall(call);
}

void SessionManagerDBusAdaptor::StorePolicy(
    dbus::MethodCall* call,
    dbus::ExportedObject::ResponseSender sender) {
  std::vector<uint8_t> policy_blob;
  dbus::MessageReader reader(call);
  if (!brillo::dbus_utils::PopValueFromReader(&reader, &policy_blob)) {
    sender.Run(CreateInvalidArgsError(call, call->GetSignature()));
    return;
  }

  impl_->StorePolicy(
      base::MakeUnique<brillo::dbus_utils::DBusMethodResponse<>>(call, sender),
      policy_blob);
}

void SessionManagerDBusAdaptor::StoreUnsignedPolicy(
    dbus::MethodCall* call,
    dbus::ExportedObject::ResponseSender sender) {
  std::vector<uint8_t> policy_blob;
  dbus::MessageReader reader(call);
  if (!brillo::dbus_utils::PopValueFromReader(&reader, &policy_blob)) {
    sender.Run(CreateInvalidArgsError(call, call->GetSignature()));
    return;
  }

  impl_->StoreUnsignedPolicy(
      base::MakeUnique<brillo::dbus_utils::DBusMethodResponse<>>(call, sender),
      policy_blob);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::RetrievePolicy(
    dbus::MethodCall* call) {
  brillo::ErrorPtr error;
  std::vector<uint8_t> policy_blob;
  if (!impl_->RetrievePolicy(&error, &policy_blob))
    return brillo::dbus_utils::GetDBusError(call, error.get());
  return CreateBytesResponse(call, policy_blob);
}

void SessionManagerDBusAdaptor::StorePolicyForUser(
    dbus::MethodCall* call,
    dbus::ExportedObject::ResponseSender sender) {
  std::string account_id;
  std::vector<uint8_t> policy_blob;
  dbus::MessageReader reader(call);
  if (!reader.PopString(&account_id) ||
      !brillo::dbus_utils::PopValueFromReader(&reader, &policy_blob)) {
    sender.Run(CreateInvalidArgsError(call, call->GetSignature()));
    return;
  }

  impl_->StorePolicyForUser(
      base::MakeUnique<brillo::dbus_utils::DBusMethodResponse<>>(call, sender),
      account_id, policy_blob);
}

void SessionManagerDBusAdaptor::StoreUnsignedPolicyForUser(
    dbus::MethodCall* call,
    dbus::ExportedObject::ResponseSender sender) {
  std::string account_id;
  std::vector<uint8_t> policy_blob;
  dbus::MessageReader reader(call);
  if (!reader.PopString(&account_id) ||
      !brillo::dbus_utils::PopValueFromReader(&reader, &policy_blob)) {
    sender.Run(CreateInvalidArgsError(call, call->GetSignature()));
    return;
  }

  impl_->StoreUnsignedPolicyForUser(
      base::MakeUnique<brillo::dbus_utils::DBusMethodResponse<>>(call, sender),
      account_id, policy_blob);
}

std::unique_ptr<dbus::Response>
SessionManagerDBusAdaptor::RetrievePolicyForUser(dbus::MethodCall* call) {
  std::string account_id;
  dbus::MessageReader reader(call);

  if (!reader.PopString(&account_id))
    return CreateInvalidArgsError(call, call->GetSignature());

  brillo::ErrorPtr error;
  std::vector<uint8_t> policy_blob;
  if (!impl_->RetrievePolicyForUser(&error, account_id, &policy_blob))
    return brillo::dbus_utils::GetDBusError(call, error.get());
  return CreateBytesResponse(call, policy_blob);
}

void SessionManagerDBusAdaptor::StoreDeviceLocalAccountPolicy(
    dbus::MethodCall* call,
    dbus::ExportedObject::ResponseSender sender) {
  std::string account_id;
  std::vector<uint8_t> policy_blob;
  dbus::MessageReader reader(call);
  if (!reader.PopString(&account_id) ||
      !brillo::dbus_utils::PopValueFromReader(&reader, &policy_blob)) {
    sender.Run(CreateInvalidArgsError(call, call->GetSignature()));
    return;
  }

  impl_->StoreDeviceLocalAccountPolicy(
      base::MakeUnique<brillo::dbus_utils::DBusMethodResponse<>>(call, sender),
      account_id, policy_blob);
}

std::unique_ptr<dbus::Response>
SessionManagerDBusAdaptor::RetrieveDeviceLocalAccountPolicy(
    dbus::MethodCall* call) {
  std::string account_id;
  dbus::MessageReader reader(call);

  if (!reader.PopString(&account_id))
    return CreateInvalidArgsError(call, call->GetSignature());

  brillo::ErrorPtr error;
  std::vector<uint8_t> policy_blob;
  if (!impl_->RetrieveDeviceLocalAccountPolicy(
          &error, account_id, &policy_blob)) {
    return brillo::dbus_utils::GetDBusError(call, error.get());
  }
  return CreateBytesResponse(call, policy_blob);
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
  std::map<std::string, std::string> sessions =
      impl_->RetrieveActiveSessions();

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(call);
  dbus::MessageWriter writer(response.get());
  brillo::dbus_utils::AppendValueToWriter(&writer, sessions);
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
  brillo::ErrorPtr error;
  if (!impl_->LockScreen(&error))
    return brillo::dbus_utils::GetDBusError(call, error.get());
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

  brillo::ErrorPtr error;
  if (!impl_->RestartJob(&error, fd, argv))
    return brillo::dbus_utils::GetDBusError(call, error.get());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::StartDeviceWipe(
    dbus::MethodCall* call) {
  brillo::ErrorPtr error;
  if (!impl_->StartDeviceWipe(&error))
    return brillo::dbus_utils::GetDBusError(call, error.get());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::SetFlagsForUser(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  std::string account_id;
  std::vector<std::string> flags;
  if (!reader.PopString(&account_id) || !reader.PopArrayOfStrings(&flags))
    return CreateInvalidArgsError(call, call->GetSignature());
  impl_->SetFlagsForUser(account_id, flags);
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

  brillo::ErrorPtr error;
  if (!impl_->InitMachineInfo(&error, data))
    return brillo::dbus_utils::GetDBusError(call, error.get());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::StartContainer(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  std::string name;
  if (!reader.PopString(&name))
    return CreateInvalidArgsError(call, call->GetSignature());

  brillo::ErrorPtr error;
  if (!impl_->StartContainer(&error, name))
    return brillo::dbus_utils::GetDBusError(call, error.get());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::StopContainer(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  std::string name;
  if (!reader.PopString(&name))
    return CreateInvalidArgsError(call, call->GetSignature());

  brillo::ErrorPtr error;
  if (!impl_->StopContainer(&error, name))
    return brillo::dbus_utils::GetDBusError(call, error.get());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::StartArcInstance(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);

  std::vector<uint8_t> request;
  if (!brillo::dbus_utils::PopValueFromReader(&reader, &request)) {
    // TODO(xiaohuic): remove after Chromium side moved to proto.
    // http://b/37989086
    login_manager::StartArcInstanceRequest proto;
    bool skip_boot_completed_broadcast = false;
    bool scan_vendor_priv_app = false;
    if (!reader.PopString(proto.mutable_account_id()) ||
        !reader.PopBool(&skip_boot_completed_broadcast) ||
        !reader.PopBool(&scan_vendor_priv_app)) {
      return CreateInvalidArgsError(call, call->GetSignature());
    }
    proto.set_skip_boot_completed_broadcast(skip_boot_completed_broadcast);
    proto.set_scan_vendor_priv_app(scan_vendor_priv_app);
    request = SerializeAsBlob(proto);
  }

  brillo::ErrorPtr error;
  std::string container_instance_id;
  if (!impl_->StartArcInstance(&error, request, &container_instance_id))
    return brillo::dbus_utils::GetDBusError(call, error.get());
  return CreateStringResponse(call, container_instance_id);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::StopArcInstance(
    dbus::MethodCall* call) {
  brillo::ErrorPtr error;
  if (!impl_->StopArcInstance(&error))
    return brillo::dbus_utils::GetDBusError(call, error.get());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response>
SessionManagerDBusAdaptor::SetArcCpuRestriction(dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  uint32_t state;
  if (!reader.PopUint32(&state))
    return CreateInvalidArgsError(call, call->GetSignature());

  brillo::ErrorPtr error;
  if (!impl_->SetArcCpuRestriction(&error, state))
    return brillo::dbus_utils::GetDBusError(call, error.get());
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

  brillo::ErrorPtr error;
  if (!impl_->EmitArcBooted(&error, account_id))
    return brillo::dbus_utils::GetDBusError(call, error.get());
  return dbus::Response::FromMethodCall(call);
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::GetArcStartTimeTicks(
    dbus::MethodCall* call) {
  brillo::ErrorPtr error;
  int64_t start_time = 0;
  if (!impl_->GetArcStartTimeTicks(&error, &start_time))
    return brillo::dbus_utils::GetDBusError(call, error.get());

  std::unique_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(call));
  dbus::MessageWriter writer(response.get());
  writer.AppendInt64(start_time);
  return response;
}

std::unique_ptr<dbus::Response> SessionManagerDBusAdaptor::RemoveArcData(
    dbus::MethodCall* call) {
  dbus::MessageReader reader(call);
  std::string account_id;
  if (!reader.PopString(&account_id))
    return CreateInvalidArgsError(call, call->GetSignature());

  brillo::ErrorPtr error;
  if (!impl_->RemoveArcData(&error, account_id))
    return brillo::dbus_utils::GetDBusError(call, error.get());
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
