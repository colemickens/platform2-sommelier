// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/session_manager_client.h"

#include <memory>

#include <base/callback.h>
#include <brillo/dbus/dbus_method_invoker.h>
#include <brillo/dbus/dbus_object.h>
#include <dbus/login_manager/dbus-constants.h>
#include <dbus/object_proxy.h>

namespace authpolicy {

namespace {

// Prints an error from a D-Bus method call. |method| is the name of the method.
// |response| is the D-Bus response from that call (may be nullptr). |error| is
// the Brillo-Error from parsing return values (can be nullptr).
void PrintError(const char* method,
                dbus::Response* response,
                brillo::Error* error) {
  // In case of a D-Bus error, the proxy prints out the error string and
  // response is empty.
  const char* error_msg =
      !response ? "No response or error."
                : !error ? "Unknown error." : error->GetMessage().c_str();
  LOG(ERROR) << "Call to " << method << " failed. " << error_msg;
}

// Prints an error if connecting to a signal failed.
void LogOnSignalConnected(const std::string& interface_name,
                          const std::string& signal_name,
                          bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to connect to signal " << signal_name
               << " of interface " << interface_name;
  }
}

}  // namespace

SessionManagerClient::SessionManagerClient(
    brillo::dbus_utils::DBusObject* dbus_object)
    : weak_ptr_factory_(this) {
  session_manager_proxy_ = dbus_object->GetBus()->GetObjectProxy(
      login_manager::kSessionManagerServiceName,
      dbus::ObjectPath(login_manager::kSessionManagerServicePath));
}

SessionManagerClient::~SessionManagerClient() = default;

void SessionManagerClient::StoreUnsignedPolicyEx(
    const std::string& descriptor_blob,
    const std::string& policy_blob,
    const base::Callback<void(bool success)>& callback) {
  dbus::MethodCall method_call(
      login_manager::kSessionManagerInterface,
      login_manager::kSessionManagerStoreUnsignedPolicyEx);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(descriptor_blob.data()),
      descriptor_blob.size());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(policy_blob.data()), policy_blob.size());
  session_manager_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&SessionManagerClient::OnPolicyStored,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

bool SessionManagerClient::ListStoredComponentPolicies(
    const std::string& descriptor_blob,
    std::vector<std::string>* component_ids) {
  dbus::MethodCall method_call(
      login_manager::kSessionManagerInterface,
      login_manager::kSessionManagerListStoredComponentPolicies);
  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(descriptor_blob.data()),
      descriptor_blob.size());

  std::unique_ptr<dbus::Response> response =
      session_manager_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);

  brillo::ErrorPtr error;
  if (!response || !brillo::dbus_utils::ExtractMethodCallResults(
                       response.get(), &error, component_ids)) {
    PrintError(login_manager::kSessionManagerListStoredComponentPolicies,
               response.get(), error.get());
    return false;
  }
  return true;
}

void SessionManagerClient::ConnectToSessionStateChangedSignal(
    const base::Callback<void(const std::string& state)>& callback) {
  session_manager_proxy_->ConnectToSignal(
      login_manager::kSessionManagerInterface,
      login_manager::kSessionStateChangedSignal,
      base::Bind(&SessionManagerClient::OnSessionStateChanged,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&LogOnSignalConnected));
}

std::string SessionManagerClient::RetrieveSessionState() {
  dbus::MethodCall method_call(
      login_manager::kSessionManagerInterface,
      login_manager::kSessionManagerRetrieveSessionState);
  dbus::MessageWriter writer(&method_call);
  std::unique_ptr<dbus::Response> response =
      session_manager_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!response)
    return std::string();

  dbus::MessageReader reader(response.get());
  std::string state;
  if (!reader.PopString(&state))
    return std::string();

  return state;
}

void SessionManagerClient::OnPolicyStored(
    const base::Callback<void(bool success)>& callback,
    dbus::Response* response) {
  brillo::ErrorPtr error;
  if (!response ||
      !brillo::dbus_utils::ExtractMethodCallResults(response, &error)) {
    PrintError(login_manager::kSessionManagerStoreUnsignedPolicyEx, response,
               error.get());
    callback.Run(false /* success */);
    return;
  }
  callback.Run(true /* success */);
}

void SessionManagerClient::OnSessionStateChanged(
    const base::Callback<void(const std::string& state)>& callback,
    dbus::Signal* signal) {
  dbus::MessageReader signal_reader(signal);
  std::string state;
  CHECK(signal_reader.PopString(&state));
  callback.Run(state);
}

}  // namespace authpolicy
