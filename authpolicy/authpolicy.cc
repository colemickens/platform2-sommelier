// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/authpolicy.h"

#include <utility>
#include <vector>

#include <base/memory/ptr_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/dbus/dbus_method_invoker.h>
#include <dbus/authpolicy/dbus-constants.h>
#include <dbus/login_manager/dbus-constants.h>

#include "authpolicy/authpolicy_metrics.h"
#include "authpolicy/path_service.h"
#include "authpolicy/samba_interface.h"
#include "bindings/device_management_backend.pb.h"

namespace ap = authpolicy::protos;
namespace em = enterprise_management;

using brillo::dbus_utils::DBusObject;
using brillo::dbus_utils::ExtractMethodCallResults;

namespace authpolicy {

namespace {

const char kChromeUserPolicyType[] = "google/chromeos/user";
const char kChromeDevicePolicyType[] = "google/chromeos/device";

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

}  // namespace

// static
std::unique_ptr<DBusObject> AuthPolicy::GetDBusObject(
    brillo::dbus_utils::ExportedObjectManager* object_manager) {
  return base::MakeUnique<DBusObject>(
      object_manager,
      object_manager->GetBus(),
      org::chromium::AuthPolicyAdaptor::GetObjectPath());
}

AuthPolicy::AuthPolicy(
    std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object,
    std::unique_ptr<AuthPolicyMetrics> metrics,
    std::unique_ptr<PathService> path_service)
    : org::chromium::AuthPolicyAdaptor(this),
      // Note: We own |metrics|, but SambaInterface owns |path_service|. Also
      // note that |metrics_| has to be initialized before |samba_| or else
      // |samba_|'s metrics pointer isn't valid in |samba_|'s destructor.
      metrics_(std::move(metrics)),
      samba_(metrics_.get(), std::move(path_service)),
      dbus_object_(std::move(dbus_object)),
      weak_ptr_factory_(this) {}

ErrorType AuthPolicy::Initialize(bool expect_config) {
  return samba_.Initialize(expect_config);
}

void AuthPolicy::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(completion_callback);
  session_manager_proxy_ = dbus_object_->GetBus()->GetObjectProxy(
      login_manager::kSessionManagerServiceName,
      dbus::ObjectPath(login_manager::kSessionManagerServicePath));
  DCHECK(session_manager_proxy_);
}

void AuthPolicy::AuthenticateUser(const std::string& user_principal_name,
                                  const dbus::FileDescriptor& password_fd,
                                  int32_t* int_error,
                                  std::string* account_id) {
  LOG(INFO) << "Received 'AuthenticateUser' request";
  ScopedTimerReporter timer(TIMER_AUTHENTICATE_USER);

  ErrorType error = samba_.AuthenticateUser(
      user_principal_name, password_fd.value(), account_id);
  PrintError("AuthenticateUser", error);
  metrics_->ReportDBusResult(DBUS_CALL_AUTHENTICATE_USER, error);
  *int_error = static_cast<int>(error);
}

int32_t AuthPolicy::JoinADDomain(const std::string& machine_name,
                                 const std::string& user_principal_name,
                                 const dbus::FileDescriptor& password_fd) {
  LOG(INFO) << "Received 'JoinADDomain' request";
  ScopedTimerReporter timer(TIMER_JOIN_AD_DOMAIN);

  ErrorType error = samba_.JoinMachine(machine_name, user_principal_name,
                                       password_fd.value());
  PrintError("JoinADDomain", error);
  metrics_->ReportDBusResult(DBUS_CALL_JOIN_AD_DOMAIN, error);
  return error;
}

void AuthPolicy::RefreshUserPolicy(PolicyResponseCallback callback,
                                   const std::string& account_id) {
  LOG(INFO) << "Received 'RefreshUserPolicy' request";
  auto timer = base::MakeUnique<ScopedTimerReporter>(TIMER_REFRESH_USER_POLICY);

  // Fetch GPOs for the current user.
  std::string policy_blob;
  ErrorType error = samba_.FetchUserGpos(account_id, &policy_blob);
  PrintError("User policy fetch and parsing", error);

  // Return immediately on error.
  if (error != ERROR_NONE) {
    metrics_->ReportDBusResult(DBUS_CALL_REFRESH_USER_POLICY, error);
    callback->Return(error);
    return;
  }

  // Send policy to Session Manager.
  StorePolicy(policy_blob, &account_id, std::move(timer), std::move(callback));
}

void AuthPolicy::RefreshDevicePolicy(PolicyResponseCallback callback) {
  LOG(INFO) << "Received 'RefreshDevicePolicy' request";
  auto timer =
      base::MakeUnique<ScopedTimerReporter>(TIMER_REFRESH_DEVICE_POLICY);

  // Fetch GPOs for the device.
  std::string policy_blob;
  ErrorType error = samba_.FetchDeviceGpos(&policy_blob);
  PrintError("Device policy fetch and parsing", error);

  // Return immediately on error.
  if (error != ERROR_NONE) {
    metrics_->ReportDBusResult(DBUS_CALL_REFRESH_DEVICE_POLICY, error);
    callback->Return(error);
    return;
  }

  // Send policy to Session Manager.
  StorePolicy(policy_blob, nullptr, std::move(timer), std::move(callback));
}

void AuthPolicy::StorePolicy(const std::string& policy_blob,
                             const std::string* account_id,
                             std::unique_ptr<ScopedTimerReporter> timer,
                             PolicyResponseCallback callback) {
  // Note: Only policy_value required here, the other data only impacts
  // signature, but since we don't sign, we don't need it.
  const bool is_user_policy = account_id != nullptr;
  const char* const policy_type =
      is_user_policy ? kChromeUserPolicyType : kChromeDevicePolicyType;

  em::PolicyData policy_data;
  policy_data.set_policy_value(policy_blob);
  policy_data.set_policy_type(policy_type);
  // Note: No signature required here, Active Directory policy is unsigned!

  em::PolicyFetchResponse policy_response;
  std::string response_blob;
  if (!policy_data.SerializeToString(policy_response.mutable_policy_data()) ||
      !policy_response.SerializeToString(&response_blob)) {
    LOG(ERROR) << "Failed to serialize policy data";
    const DBusCallType call_type = GetPolicyDBusCallType(is_user_policy);
    metrics_->ReportDBusResult(call_type, ERROR_STORE_POLICY_FAILED);
    callback->Return(ERROR_STORE_POLICY_FAILED);
    return;
  }

  const char* const method = GetSessionManagerStoreMethod(is_user_policy);
  LOG(INFO) << "Calling Session Manager D-Bus method " << method;
  dbus::MethodCall method_call(login_manager::kSessionManagerInterface, method);
  dbus::MessageWriter writer(&method_call);
  if (account_id) writer.AppendString(*account_id);
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(response_blob.data()),
      response_blob.size());
  session_manager_proxy_->CallMethod(&method_call,
                                     dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                     base::Bind(&AuthPolicy::OnPolicyStored,
                                                weak_ptr_factory_.GetWeakPtr(),
                                                is_user_policy,
                                                base::Passed(&timer),
                                                base::Passed(&callback)));
}

void AuthPolicy::OnPolicyStored(bool is_user_policy,
                                std::unique_ptr<ScopedTimerReporter> timer,
                                PolicyResponseCallback callback,
                                dbus::Response* response) {
  const char* const method = GetSessionManagerStoreMethod(is_user_policy);
  brillo::ErrorPtr brillo_error;
  bool done = false;
  std::string msg;
  if (!response) {
    msg = base::StringPrintf("Call to %s failed. No response.", method);
  } else if (!ExtractMethodCallResults(response, &brillo_error, &done)) {
    msg = base::StringPrintf(
        "Call to %s failed. %s",
        method,
        brillo_error ? brillo_error->GetMessage().c_str() : "Unknown error.");
  } else if (!done) {
    msg = base::StringPrintf("Call to %s failed. Call returned false.", method);
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
