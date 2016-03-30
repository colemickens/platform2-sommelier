//
// Copyright (C) 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/binder/manager_binder_adaptor.h"

#include <base/bind.h>
#include <binder/Status.h>
#include <binderwrapper/binder_wrapper.h>
#include <utils/String8.h>
// TODO(samueltan): remove these includes once b/27270173 is resolved,
// and Manager is no longer reliant on D-Bus service constants.
#if defined(__ANDROID__)
#include <dbus/service_constants.h>
#else
#include <chromeos/dbus/service_constants.h>
#endif  // __ANDROID__

#include "shill/binder/binder_control.h"
#include "shill/binder/device_binder_adaptor.h"
#include "shill/binder/service_binder_adaptor.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/manager.h"

using android::binder::Status;
using android::BinderWrapper;
using android::IBinder;
using android::interface_cast;
using android::sp;
using android::String8;
using android::system::connectivity::shill::IPropertyChangedCallback;
using android::system::connectivity::shill::IService;
using base::Bind;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kBinder;
static string ObjectID(ManagerBinderAdaptor* m) {
  return "Manager binder adaptor (id " + m->GetRpcIdentifier() + ")";
}
}  // namespace Logging

ManagerBinderAdaptor::ManagerBinderAdaptor(BinderControl* control,
                                           Manager* manager,
                                           const std::string& id)
    : BinderAdaptor(control, id),
      manager_(manager),
      ap_mode_setter_(nullptr),
      device_claimer_(nullptr) {}

ManagerBinderAdaptor::~ManagerBinderAdaptor() {
  if (ap_mode_setter_ != nullptr) {
    BinderWrapper::Get()->UnregisterForDeathNotifications(ap_mode_setter_);
  }
  if (device_claimer_ != nullptr) {
    BinderWrapper::Get()->UnregisterForDeathNotifications(device_claimer_);
  }
}

void ManagerBinderAdaptor::RegisterAsync(
    const base::Callback<void(bool)>& /*completion_callback*/) {
  // Registration is performed synchronously in Binder.
  BinderWrapper::Get()->RegisterService(
      String8(getInterfaceDescriptor()).string(), this);
}

void ManagerBinderAdaptor::EmitBoolChanged(const string& name, bool /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

void ManagerBinderAdaptor::EmitUintChanged(const string& name,
                                           uint32_t /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

void ManagerBinderAdaptor::EmitIntChanged(const string& name, int /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

void ManagerBinderAdaptor::EmitStringChanged(const string& name,
                                             const string& /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

void ManagerBinderAdaptor::EmitStringsChanged(const string& name,
                                              const vector<string>& /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

void ManagerBinderAdaptor::EmitRpcIdentifierChanged(const string& name,
                                                    const string& /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

void ManagerBinderAdaptor::EmitRpcIdentifierArrayChanged(
    const string& name, const vector<string>& /*value*/) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name);
}

Status ManagerBinderAdaptor::SetupApModeInterface(
    const sp<IBinder>& ap_mode_setter, std::string* _aidl_return) {
  SLOG(this, 2) << __func__;
  Error e;
#if !defined(DISABLE_WIFI) && defined(__BRILLO__)
  manager_->SetupApModeInterface(_aidl_return, &e);
  if (e.IsFailure()) {
    return e.ToBinderStatus();
  }
  // Register for death notifications from the caller. This will restore
  // interface mode to station mode if the caller vanishes.
  ap_mode_setter_ = ap_mode_setter;
  BinderWrapper::Get()->RegisterForDeathNotifications(
      ap_mode_setter, Bind(&ManagerBinderAdaptor::OnApModeSetterVanished,
                           base::Unretained(this)));
  return Status::ok();
#else
  return Status::fromExceptionCode(Status::EX_UNSUPPORTED_OPERATION);
#endif  // !DISABLE_WIFI && __BRILLO__
}

Status ManagerBinderAdaptor::SetupStationModeInterface(
    std::string* _aidl_return) {
  SLOG(this, 2) << __func__;
#if !defined(DISABLE_WIFI) && defined(__BRILLO__)
  Error e;
  manager_->SetupStationModeInterface(_aidl_return, &e);
  if (e.IsFailure()) {
    return e.ToBinderStatus();
  }
  if (ap_mode_setter_ != nullptr) {
    // Unregister for death notifications from the AP mode setter, if case
    // SetupApModeInterface() was previously called.
    BinderWrapper::Get()->UnregisterForDeathNotifications(ap_mode_setter_);
    ap_mode_setter_ = nullptr;
  }
  return Status::ok();
#else
  return Status::fromExceptionCode(Status::EX_UNSUPPORTED_OPERATION);
#endif  // !DISABLE_WIFI && __BRILLO__
}

Status ManagerBinderAdaptor::ClaimInterface(const sp<IBinder>& claimer,
                                            const std::string& claimer_name,
                                            const std::string& interface_name) {
  SLOG(this, 2) << __func__;
  Error e;
  // Empty claimer name is used to indicate default claimer.
  // TODO(samueltan): update this API or make a new API to use a flag to
  // indicate default claimer instead (b/27924738).
  manager_->ClaimDevice(claimer_name, interface_name, &e);
  if (e.IsFailure()) {
    return e.ToBinderStatus();
  }
  if (!claimer_name.empty()) {
    device_claimer_ = claimer;
    BinderWrapper::Get()->RegisterForDeathNotifications(
        claimer, Bind(&ManagerBinderAdaptor::OnDeviceClaimerVanished,
                      base::Unretained(this)));
  }
  return Status::ok();
}

Status ManagerBinderAdaptor::ReleaseInterface(
    const sp<IBinder>& claimer, const std::string& claimer_name,
    const std::string& interface_name) {
  SLOG(this, 2) << __func__;
  Error e;
  bool claimer_removed;
  // Empty claimer name is used to indicate default claimer.
  // TODO(samueltan): update this API or make a new API to use a flag to
  // indicate default claimer instead (b/27924738).
  manager_->ReleaseDevice(claimer_name, interface_name, &claimer_removed, &e);
  if (e.IsFailure()) {
    return e.ToBinderStatus();
  }
  if (claimer_removed) {
    BinderWrapper::Get()->UnregisterForDeathNotifications(claimer);
  }
  return Status::ok();
}

Status ManagerBinderAdaptor::ConfigureService(
    const android::os::PersistableBundle& properties,
    sp<IService>* _aidl_return) {
  SLOG(this, 2) << __func__;
  ServiceRefPtr service;
  KeyValueStore args_store;
  KeyValueStore::ConvertFromPersistableBundle(properties, &args_store);

  Error e;
  service = manager_->ConfigureService(args_store, &e);
  if (e.IsFailure()) {
    return e.ToBinderStatus();
  }
  *_aidl_return = interface_cast<IService>(static_cast<ServiceBinderAdaptor*>(
      control()->GetBinderAdaptorForRpcIdentifier(
          service->GetRpcIdentifier())));
  return Status::ok();
}

Status ManagerBinderAdaptor::RequestScan(int32_t type) {
  string technology;
  switch (type) {
    // TODO(samueltan): remove the use of these D-Bus service constants once
    // b/27270173 is resolved, and Manager is no longer reliant on  them.
    case IManager::TECHNOLOGY_ANY:
      technology = "";
      break;
    case IManager::TECHNOLOGY_WIFI:
      technology = kTypeWifi;
      break;
    default:
      return Status::fromExceptionCode(
          Status::EX_ILLEGAL_ARGUMENT,
          String8::format("%s: invalid technology type %d", __func__, type));
  }

  SLOG(this, 2) << __func__ << ": " << technology;
  Error e;
  manager_->RequestScan(Device::kFullScan, technology, &e);
  return e.ToBinderStatus();
}

Status ManagerBinderAdaptor::GetDevices(vector<sp<IBinder>>* _aidl_return) {
  SLOG(this, 2) << __func__;
  Error e;
  RpcIdentifiers device_rpc_ids = manager_->EnumerateDevices(&e);
  if (e.IsFailure()) {
    return e.ToBinderStatus();
  }
  for (const auto& device_rpc_id : device_rpc_ids) {
    _aidl_return->emplace_back(static_cast<DeviceBinderAdaptor*>(
        control()->GetBinderAdaptorForRpcIdentifier(device_rpc_id)));
  }
  return Status::ok();
}

Status ManagerBinderAdaptor::GetDefaultService(sp<IBinder>* _aidl_return) {
  SLOG(this, 2) << __func__;
  Error e;
  RpcIdentifier default_service_rpc_id =
      manager_->GetDefaultServiceRpcIdentifier(&e);
  if (e.IsFailure()) {
    return e.ToBinderStatus();
  }
  *_aidl_return = static_cast<ServiceBinderAdaptor*>(
      control()->GetBinderAdaptorForRpcIdentifier(default_service_rpc_id));
  return Status::ok();
}

Status ManagerBinderAdaptor::RegisterPropertyChangedSignalHandler(
    const sp<IPropertyChangedCallback>& callback) {
  AddPropertyChangedSignalHandler(callback);
  return Status::ok();
}

void ManagerBinderAdaptor::OnApModeSetterVanished() {
  SLOG(this, 3) << __func__;
#if !defined(DISABLE_WIFI) && defined(__BRILLO__)
  manager_->OnApModeSetterVanished();
#endif  // !DISABLE_WIFI && __BRILLO__
  BinderWrapper::Get()->UnregisterForDeathNotifications(ap_mode_setter_);
  ap_mode_setter_ = nullptr;
}

void ManagerBinderAdaptor::OnDeviceClaimerVanished() {
  SLOG(this, 3) << __func__;
  manager_->OnDeviceClaimerVanished();
  BinderWrapper::Get()->UnregisterForDeathNotifications(device_claimer_);
  device_claimer_ = nullptr;
}

}  // namespace shill
