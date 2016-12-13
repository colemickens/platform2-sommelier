// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/authpolicy.h"

#include <utility>
#include <vector>

#include <base/strings/stringprintf.h>
#include <brillo/dbus/dbus_method_invoker.h>
#include <chromeos/dbus/service_constants.h>

#include "authpolicy/errors.h"
#include "authpolicy/samba_interface.h"
#include "bindings/device_management_backend.pb.h"

namespace ap = authpolicy::protos;
namespace em = enterprise_management;

using brillo::dbus_utils::ExtractMethodCallResults;

namespace {

const char kChromeUserPolicyType[] = "google/chromeos/user";
const char kChromeDevicePolicyType[] = "google/chromeos/device";

}  // namespace

namespace authpolicy {

AuthPolicy::AuthPolicy(
    brillo::dbus_utils::ExportedObjectManager* object_manager)
    : org::chromium::AuthPolicyAdaptor(this),
      dbus_object_(object_manager, object_manager->GetBus(),
                   org::chromium::AuthPolicyAdaptor::GetObjectPath()),
      session_manager_proxy_(nullptr),
      weak_ptr_factory_(this) {}

void AuthPolicy::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(completion_callback);
  session_manager_proxy_ = dbus_object_.GetBus()->GetObjectProxy(
      login_manager::kSessionManagerServiceName,
      dbus::ObjectPath(login_manager::kSessionManagerServicePath));
  DCHECK(session_manager_proxy_);
}

bool AuthPolicy::AuthenticateUser(brillo::ErrorPtr* error,
                                  const std::string& in_user_principal_name,
                                  const dbus::FileDescriptor& in_password_fd,
                                  int32_t* out_error_code,
                                  std::string* out_account_id) {
  LOG(INFO) << "Received 'AuthenticateUser' request";
  *out_error_code = 0;

  // Call samba to authenticate user and get account id.
  const char* error_code = nullptr;
  if (!samba_.AuthenticateUser(in_user_principal_name, in_password_fd.value(),
                               out_account_id, &error_code)) {
    brillo::Error::AddToPrintf(error, FROM_HERE, errors::kDomain,
                               error_code, "Failed to authenticate user.");
    return false;
  }

  LOG(INFO) << "AuthenticateUser succeeded";
  return true;
}

bool AuthPolicy::JoinADDomain(brillo::ErrorPtr* error,
                              const std::string& in_machine_name,
                              const std::string& in_user_principal_name,
                              const dbus::FileDescriptor& in_password_fd,
                              int32_t* out_error_code) {
  LOG(INFO) << "Received 'JoinADDomain' request";
  *out_error_code = 0;

  // Call samba to join machine to the Active Directory domain.
  const char* error_code = nullptr;
  if (!samba_.JoinMachine(in_machine_name, in_user_principal_name,
                          in_password_fd.value(), &error_code)) {
    brillo::Error::AddToPrintf(error, FROM_HERE, errors::kDomain,
                               error_code,
                               "Failed to join Active Directory domain.");
    return false;
  }

  LOG(INFO) << "JoinADDomain succeeded";
  return true;
}

void AuthPolicy::RefreshUserPolicy(PolicyResponseCallback callback,
                                   const std::string& in_account_id) {
  LOG(INFO) << "Received 'RefreshUserPolicy' request";

  // Fetch GPOs for the current user.
  const char* error_code = nullptr;
  std::string policy_blob;
  if (!samba_.FetchUserGpos(in_account_id, &policy_blob, &error_code)) {
    callback->ReplyWithError(FROM_HERE, errors::kDomain, error_code,
                             "Failed to fetch user policy files.");
    return;
  }

  LOG(INFO) << "User policy fetch and parsing succeeded";

  // Send policy to Session Manager.
  StorePolicy(policy_blob, &in_account_id, std::move(callback));
}

void AuthPolicy::RefreshDevicePolicy(PolicyResponseCallback callback) {
  LOG(INFO) << "Received 'RefreshDevicePolicy' request";

  // Fetch GPOs for the device.
  const char* error_code = nullptr;
  std::string policy_blob;
  if (!samba_.FetchDeviceGpos(&policy_blob, &error_code)) {
    callback->ReplyWithError(FROM_HERE, errors::kDomain, error_code,
                             "Failed to fetch device policy files.");
    return;
  }

  LOG(INFO) << "Device policy fetch and parsing succeeded";

  // Send policy to Session Manager.
  StorePolicy(policy_blob, nullptr, std::move(callback));
}

void AuthPolicy::StorePolicy(const std::string& policy_blob,
                             const std::string* account_id,
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
  if (!policy_data.SerializeToString(policy_response.mutable_policy_data())) {
    callback->ReplyWithError(FROM_HERE, errors::kDomain,
                             errors::kStorePolicyFailed,
                             "Failed to serialize policy data.");
    return;
  }

  // Finally, write response to a string blob.
  std::string response_blob;
  if (!policy_response.SerializeToString(&response_blob)) {
    callback->ReplyWithError(FROM_HERE, errors::kDomain,
                             errors::kStorePolicyFailed,
                             "Failed to serialize policy response.");
    return;
  }

  const char* const method =
      is_user_policy ? login_manager::kSessionManagerStoreUnsignedPolicyForUser
                     : login_manager::kSessionManagerStoreUnsignedPolicy;

  LOG(INFO) << "Calling Session Manager D-Bus method " << method;
  dbus::MethodCall method_call(login_manager::kSessionManagerInterface, method);
  dbus::MessageWriter writer(&method_call);
  if (account_id) writer.AppendString(*account_id);
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(response_blob.data()),
      response_blob.size());
  session_manager_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&AuthPolicy::OnPolicyStored, weak_ptr_factory_.GetWeakPtr(),
                 method, base::Passed(&callback)));
}

void AuthPolicy::OnPolicyStored(const char* method,
                                PolicyResponseCallback callback,
                                dbus::Response* response) {
  brillo::ErrorPtr error;
  bool done = false;
  std::string msg;
  if (!response) {
    msg = base::StringPrintf("Call to %s failed. No response.", method);
  } else if (!ExtractMethodCallResults(response, &error, &done)) {
    msg = base::StringPrintf("Call to %s failed. %s",
        method, error ? error->GetMessage().c_str() : "Unknown error.");
  } else if (!done) {
    msg = base::StringPrintf("Call to %s failed. Call returned false.", method);
  }

  if (!msg.empty()) {
    LOG(ERROR) << msg;
    callback->ReplyWithError(FROM_HERE, errors::kDomain,
        errors::kStorePolicyFailed, msg);
  } else {
    LOG(INFO) << "Call to " << method << " succeeded.";
    callback->Return(0);
  }
}

}  // namespace authpolicy
