// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/authpolicy.h"

#include <utility>
#include <vector>

#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/threading/thread_task_runner_handle.h>
#include <brillo/dbus/dbus_method_invoker.h>
#include <dbus/authpolicy/dbus-constants.h>
#include <dbus/login_manager/dbus-constants.h>

#include "authpolicy/authpolicy_metrics.h"
#include "authpolicy/path_service.h"
#include "authpolicy/proto_bindings/active_directory_info.pb.h"
#include "authpolicy/samba_helper.h"
#include "authpolicy/samba_interface.h"
#include "bindings/device_management_backend.pb.h"

namespace em = enterprise_management;

using brillo::dbus_utils::DBusObject;
using brillo::dbus_utils::ExtractMethodCallResults;

namespace authpolicy {

const char kChromeUserPolicyType[] = "google/chromeos/user";
const char kChromeDevicePolicyType[] = "google/chromeos/device";

namespace {

void PrintError(const char* msg, ErrorType error) {
  if (error == ERROR_NONE)
    LOG(INFO) << msg << " succeeded";
  else
    LOG(INFO) << msg << " failed with code " << error;
}

const char* GetSessionManagerStoreMethod(bool is_user_policy) {
  return is_user_policy
             ? login_manager::kSessionManagerStoreUnsignedPolicyForUser
             : login_manager::kSessionManagerStoreUnsignedPolicy;
}

DBusCallType GetPolicyDBusCallType(bool is_user_policy) {
  return is_user_policy ? DBUS_CALL_REFRESH_USER_POLICY
                        : DBUS_CALL_REFRESH_DEVICE_POLICY;
}

// Serializes |proto| to the byte array |proto_blob|. Returns ERROR_NONE on
// success and ERROR_PARSE_FAILED otherwise.
ErrorType SerializeProto(const google::protobuf::MessageLite& proto,
                         std::vector<uint8_t>* proto_blob) {
  proto_blob->resize(proto.ByteSizeLong());
  if (!proto.SerializeToArray(proto_blob->data(), proto_blob->size())) {
    LOG(ERROR) << "Failed to serialize proto";
    return ERROR_PARSE_FAILED;
  }
  return ERROR_NONE;
}

ErrorType ParseProto(google::protobuf::MessageLite* proto,
                     const std::vector<uint8_t>& proto_blob) {
  if (!proto->ParseFromArray(proto_blob.data(), proto_blob.size())) {
    LOG(ERROR) << "Failed to parse proto";
    return ERROR_PARSE_FAILED;
  }
  return ERROR_NONE;
}

}  // namespace

// static
std::unique_ptr<DBusObject> AuthPolicy::GetDBusObject(
    brillo::dbus_utils::ExportedObjectManager* object_manager) {
  return std::make_unique<DBusObject>(
      object_manager, object_manager->GetBus(),
      org::chromium::AuthPolicyAdaptor::GetObjectPath());
}

AuthPolicy::AuthPolicy(AuthPolicyMetrics* metrics,
                       const PathService* path_service)
    : org::chromium::AuthPolicyAdaptor(this),
      metrics_(metrics),
      samba_(base::ThreadTaskRunnerHandle::Get(),
             metrics,
             path_service,
             base::Bind(&AuthPolicy::OnUserKerberosFilesChanged,
                        base::Unretained(this))),
      weak_ptr_factory_(this) {}

ErrorType AuthPolicy::Initialize(bool device_is_locked) {
  device_is_locked_ = device_is_locked;
  return samba_.Initialize(/* expect_config */ device_is_locked_);
}

void AuthPolicy::RegisterAsync(
    std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object,
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  DCHECK(!dbus_object_);
  dbus_object_ = std::move(dbus_object);
  // Make sure the task runner passed to |samba_| in the constructor is actually
  // the D-Bus task runner. This guarantees that automatic TGT renewal won't
  // interfere with D-Bus calls. Note that |GetDBusTaskRunner()| returns a
  // TaskRunner, which is a base class of SingleThreadTaskRunner accepted by
  // |samba_|.
  CHECK_EQ(base::ThreadTaskRunnerHandle::Get(),
           dbus_object_->GetBus()->GetDBusTaskRunner());
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(completion_callback);
  session_manager_proxy_ = dbus_object_->GetBus()->GetObjectProxy(
      login_manager::kSessionManagerServiceName,
      dbus::ObjectPath(login_manager::kSessionManagerServicePath));
  DCHECK(session_manager_proxy_);
}

void AuthPolicy::AuthenticateUser(
    const std::vector<uint8_t>& auth_user_request_blob,
    const dbus::FileDescriptor& password_fd,
    int32_t* int_error,
    std::vector<uint8_t>* account_info_blob) {
  LOG(INFO) << "Received 'AuthenticateUser' request";
  ScopedTimerReporter timer(TIMER_AUTHENTICATE_USER);
  authpolicy::AuthenticateUserRequest request;
  ErrorType error = ParseProto(&request, auth_user_request_blob);

  ActiveDirectoryAccountInfo account_info;
  if (error == ERROR_NONE) {
    error = samba_.AuthenticateUser(request.user_principal_name(),
                                    request.account_id(), password_fd.value(),
                                    &account_info);
  }
  if (error == ERROR_NONE)
    error = SerializeProto(account_info, account_info_blob);

  PrintError("AuthenticateUser", error);
  metrics_->ReportDBusResult(DBUS_CALL_AUTHENTICATE_USER, error);
  *int_error = static_cast<int>(error);
}

void AuthPolicy::GetUserStatus(dbus::MethodCall* method_call,
                               brillo::dbus_utils::ResponseSender sender) {
  // Read input arguments.
  dbus::MessageReader reader(method_call);
  GetUserStatusRequest request;
  bool success = reader.PopArrayOfBytesAsProto(&request) ||
                 reader.PopString(request.mutable_account_id());

  int32_t error = ERROR_NONE;
  std::vector<uint8_t> user_status_blob;
  if (success) {
    GetUserStatus(request, &error, &user_status_blob);
  } else {
    error = ERROR_DBUS_FAILURE;
  }

  // Send response.
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendInt32(error);
  writer.AppendArrayOfBytes(user_status_blob.data(), user_status_blob.size());
  sender.Run(std::move(response));
}

void AuthPolicy::GetUserStatus(const GetUserStatusRequest& request,
                               int32_t* int_error,
                               std::vector<uint8_t>* user_status_blob) {
  LOG(INFO) << "Received 'GetUserStatus' request";
  ScopedTimerReporter timer(TIMER_GET_USER_STATUS);

  ActiveDirectoryUserStatus user_status;
  ErrorType error = samba_.GetUserStatus(request.user_principal_name(),
                                         request.account_id(), &user_status);
  if (error == ERROR_NONE)
    error = SerializeProto(user_status, user_status_blob);
  PrintError("GetUserStatus", error);
  metrics_->ReportDBusResult(DBUS_CALL_GET_USER_STATUS, error);
  *int_error = static_cast<int>(error);
}

void AuthPolicy::GetUserKerberosFiles(
    const std::string& account_id,
    int32_t* int_error,
    std::vector<uint8_t>* kerberos_files_blob) {
  LOG(INFO) << "Received 'GetUserKerberosFiles' request";
  ScopedTimerReporter timer(TIMER_GET_USER_KERBEROS_FILES);

  KerberosFiles kerberos_files;
  ErrorType error = samba_.GetUserKerberosFiles(account_id, &kerberos_files);
  if (error == ERROR_NONE)
    error = SerializeProto(kerberos_files, kerberos_files_blob);
  PrintError("GetUserKerberosFiles", error);
  metrics_->ReportDBusResult(DBUS_CALL_GET_USER_KERBEROS_FILES, error);
  *int_error = static_cast<int>(error);
}

void AuthPolicy::JoinADDomain(
    const std::vector<uint8_t>& join_domain_request_blob,
    const dbus::FileDescriptor& password_fd,
    int32_t* int_error,
    std::string* joined_domain) {
  LOG(INFO) << "Received 'JoinADDomain' request";
  ScopedTimerReporter timer(TIMER_JOIN_AD_DOMAIN);

  JoinDomainRequest request;
  ErrorType error = ParseProto(&request, join_domain_request_blob);

  if (error == ERROR_NONE) {
    std::vector<std::string> machine_ou(request.machine_ou().begin(),
                                        request.machine_ou().end());

    error = samba_.JoinMachine(request.machine_name(), request.machine_domain(),
                               machine_ou, request.user_principal_name(),
                               password_fd.value(), joined_domain);
  }

  PrintError("JoinADDomain", error);
  metrics_->ReportDBusResult(DBUS_CALL_JOIN_AD_DOMAIN, error);
  *int_error = static_cast<int>(error);
}

void AuthPolicy::RefreshUserPolicy(PolicyResponseCallback callback,
                                   const std::string& account_id) {
  LOG(INFO) << "Received 'RefreshUserPolicy' request";
  auto timer = std::make_unique<ScopedTimerReporter>(TIMER_REFRESH_USER_POLICY);

  // Fetch GPOs for the current user.
  std::unique_ptr<protos::GpoPolicyData> gpo_policy_data =
      std::make_unique<protos::GpoPolicyData>();
  ErrorType error = samba_.FetchUserGpos(account_id, gpo_policy_data.get());
  PrintError("User policy fetch and parsing", error);

  // Return immediately on error.
  if (error != ERROR_NONE) {
    metrics_->ReportDBusResult(DBUS_CALL_REFRESH_USER_POLICY, error);
    callback->Return(error);
    return;
  }

  // Send policy to Session Manager.
  const std::string account_id_key = GetAccountIdKey(account_id);
  StorePolicy(std::move(gpo_policy_data), &account_id_key, std::move(timer),
              std::move(callback));
}

void AuthPolicy::RefreshDevicePolicy(PolicyResponseCallback callback) {
  LOG(INFO) << "Received 'RefreshDevicePolicy' request";
  auto timer =
      std::make_unique<ScopedTimerReporter>(TIMER_REFRESH_DEVICE_POLICY);

  if (cached_device_policy_data_) {
    // Send policy to Session Manager.
    LOG(INFO) << "Using cached policy";
    StorePolicy(std::move(cached_device_policy_data_), nullptr,
                std::move(timer), std::move(callback));
    return;
  }

  // Fetch GPOs for the device.
  std::unique_ptr<protos::GpoPolicyData> gpo_policy_data =
      std::make_unique<protos::GpoPolicyData>();
  ErrorType error = samba_.FetchDeviceGpos(gpo_policy_data.get());
  PrintError("Device policy fetch and parsing", error);

  device_is_locked_ = device_is_locked_ || InstallAttributesReader().IsLocked();
  if (!device_is_locked_ && error == ERROR_NONE) {
    LOG(INFO) << "Device is not locked yet. Caching device policy.";
    cached_device_policy_data_ = std::move(gpo_policy_data);
    error = ERROR_DEVICE_POLICY_CACHED_BUT_NOT_SENT;
  }

  // Return immediately on error.
  if (error != ERROR_NONE) {
    metrics_->ReportDBusResult(DBUS_CALL_REFRESH_DEVICE_POLICY, error);
    callback->Return(error);
    return;
  }

  // Send policy to Session Manager.
  StorePolicy(std::move(gpo_policy_data), nullptr, std::move(timer),
              std::move(callback));
}

std::string AuthPolicy::SetDefaultLogLevel(int32_t level) {
  LOG(INFO) << "Received 'SetDefaultLogLevel' request";
  if (level < AuthPolicyFlags::kMinLevel ||
      level > AuthPolicyFlags::kMaxLevel) {
    std::string message = base::StringPrintf("Level must be between %i and %i.",
                                             AuthPolicyFlags::kMinLevel,
                                             AuthPolicyFlags::kMaxLevel);
    LOG(ERROR) << message;
    return message;
  }
  samba_.SetDefaultLogLevel(static_cast<AuthPolicyFlags::DefaultLevel>(level));
  return std::string();
}

void AuthPolicy::OnUserKerberosFilesChanged() {
  LOG(INFO) << "Firing signal UserKerberosFilesChanged";
  SendUserKerberosFilesChangedSignal();
}

void AuthPolicy::StorePolicy(
    std::unique_ptr<protos::GpoPolicyData> gpo_policy_data,
    const std::string* account_id_key,
    std::unique_ptr<ScopedTimerReporter> timer,
    PolicyResponseCallback callback) {
  // Note: Only policy_value required here, the other data only impacts
  // signature, but since we don't sign, we don't need it.
  const bool is_user_policy = account_id_key != nullptr;
  const char* const policy_type =
      is_user_policy ? kChromeUserPolicyType : kChromeDevicePolicyType;

  em::PolicyData em_policy_data;
  em_policy_data.set_policy_value(gpo_policy_data->user_or_device_policy());
  em_policy_data.set_policy_type(policy_type);
  if (is_user_policy) {
    em_policy_data.set_username(samba_.user_sam_account_name());
    // Device id in the proto also could be used as an account/client id.
    em_policy_data.set_device_id(samba_.user_account_id());
  } else {
    em_policy_data.set_device_id(samba_.machine_name());
  }
  em_policy_data.set_timestamp(base::Time::Now().ToJavaTime());
  em_policy_data.set_management_mode(em::PolicyData::ENTERPRISE_MANAGED);
  // Note: No signature required here, Active Directory policy is unsigned!

  em::PolicyFetchResponse policy_response;
  std::string response_blob;
  if (!em_policy_data.SerializeToString(
          policy_response.mutable_policy_data()) ||
      !policy_response.SerializeToString(&response_blob)) {
    LOG(ERROR) << "Failed to serialize policy data";
    const DBusCallType call_type = GetPolicyDBusCallType(is_user_policy);
    metrics_->ReportDBusResult(call_type, ERROR_STORE_POLICY_FAILED);
    callback->Return(ERROR_STORE_POLICY_FAILED);
    return;
  }

  const char* const method = GetSessionManagerStoreMethod(is_user_policy);
  dbus::MethodCall method_call(login_manager::kSessionManagerInterface, method);
  dbus::MessageWriter writer(&method_call);
  if (account_id_key)
    writer.AppendString(*account_id_key);
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(response_blob.data()),
      response_blob.size());
  session_manager_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&AuthPolicy::OnPolicyStored, weak_ptr_factory_.GetWeakPtr(),
                 is_user_policy, base::Passed(&timer),
                 base::Passed(&callback)));
}

void AuthPolicy::OnPolicyStored(
    bool is_user_policy,
    std::unique_ptr<ScopedTimerReporter> /* timer */,
    PolicyResponseCallback callback,
    dbus::Response* response) {
  const char* const method = GetSessionManagerStoreMethod(is_user_policy);
  brillo::ErrorPtr brillo_error;
  std::string msg;
  if (!response) {
    // In case of error, session_manager_proxy_ prints out the error string and
    // response is empty.
    msg =
        base::StringPrintf("Call to %s failed. No response or error.", method);
  } else if (!ExtractMethodCallResults(response, &brillo_error)) {
    // Response is expected have no call results.
    msg = base::StringPrintf(
        "Call to %s failed. %s", method,
        brillo_error ? brillo_error->GetMessage().c_str() : "Unknown error.");
  }

  ErrorType error;
  if (!msg.empty()) {
    LOG(ERROR) << msg;
    error = ERROR_STORE_POLICY_FAILED;
  } else {
    LOG(INFO) << "Call to " << method << " succeeded.";
    error = ERROR_NONE;
  }
  const DBusCallType call_type = GetPolicyDBusCallType(is_user_policy);
  metrics_->ReportDBusResult(call_type, error);
  callback->Return(error);
}

}  // namespace authpolicy
