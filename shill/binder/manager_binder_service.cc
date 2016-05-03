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

#include "shill/binder/manager_binder_service.h"

#include "shill/binder/manager_binder_adaptor.h"
#include "shill/logging.h"

using android::binder::Status;
using android::sp;
using android::String8;
using android::system::connectivity::shill::IPropertyChangedCallback;
using android::system::connectivity::shill::IService;
using base::WeakPtr;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kBinder;
static string ObjectID(ManagerBinderService* s) {
  return "Manager binder service (id " + s->rpc_id() + ")";
}
}  // namespace Logging

ManagerBinderService::ManagerBinderService(
    WeakPtr<ManagerBinderAdaptor> adaptor, const string& rpc_id)
    : adaptor_(adaptor), rpc_id_(rpc_id) {}

Status ManagerBinderService::SetupApModeInterface(
    const sp<IBinder>& ap_mode_setter, std::string* _aidl_return) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": manager object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->SetupApModeInterface(ap_mode_setter, _aidl_return);
}

Status ManagerBinderService::SetupStationModeInterface(
    std::string* _aidl_return) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": manager object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->SetupStationModeInterface(_aidl_return);
}

Status ManagerBinderService::ClaimInterface(const sp<IBinder>& claimer,
                                            const std::string& claimer_name,
                                            const std::string& interface_name) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": manager object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->ClaimInterface(claimer, claimer_name, interface_name);
}

Status ManagerBinderService::ReleaseInterface(
    const sp<IBinder>& claimer, const std::string& claimer_name,
    const std::string& interface_name) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": manager object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->ReleaseInterface(claimer, claimer_name, interface_name);
}

Status ManagerBinderService::ConfigureService(
    const android::os::PersistableBundle& properties,
    sp<IService>* _aidl_return) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": manager object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->ConfigureService(properties, _aidl_return);
}

Status ManagerBinderService::RequestScan(int32_t type) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": manager object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->RequestScan(type);
}

Status ManagerBinderService::GetDevices(vector<sp<IBinder>>* _aidl_return) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": manager object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->GetDevices(_aidl_return);
}

Status ManagerBinderService::GetDefaultService(sp<IBinder>* _aidl_return) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": manager object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->GetDefaultService(_aidl_return);
}

Status ManagerBinderService::RegisterPropertyChangedSignalHandler(
    const sp<IPropertyChangedCallback>& callback) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": manager object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->RegisterPropertyChangedSignalHandler(callback);
}

}  // namespace shill
