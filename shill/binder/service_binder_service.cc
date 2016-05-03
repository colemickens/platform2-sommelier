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

#include "shill/binder/service_binder_service.h"

#include "shill/binder/service_binder_adaptor.h"
#include "shill/logging.h"

using android::binder::Status;
using android::sp;
using android::system::connectivity::shill::IPropertyChangedCallback;
using base::WeakPtr;
using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kBinder;
static string ObjectID(ServiceBinderService* s) {
  return "Service binder service (id " + s->rpc_id() + ")";
}
}  // namespace Logging

ServiceBinderService::ServiceBinderService(
    WeakPtr<ServiceBinderAdaptor> adaptor, const string& rpc_id)
    : adaptor_(adaptor), rpc_id_(rpc_id) {}

Status ServiceBinderService::Connect() {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": service object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->Connect();
}

Status ServiceBinderService::GetState(int32_t* _aidl_return) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": service object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->GetState(_aidl_return);
}

Status ServiceBinderService::GetStrength(int8_t* _aidl_return) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": service object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->GetStrength(_aidl_return);
}

Status ServiceBinderService::GetError(int32_t* _aidl_return) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": service object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->GetError(_aidl_return);
}

Status ServiceBinderService::GetTethering(int32_t* _aidl_return) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": service object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->GetTethering(_aidl_return);
}

Status ServiceBinderService::GetType(int32_t* _aidl_return) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": service object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->GetType(_aidl_return);
}

Status ServiceBinderService::GetPhysicalTechnology(int32_t* _aidl_return) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": service object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->GetPhysicalTechnology(_aidl_return);
}

Status ServiceBinderService::RegisterPropertyChangedSignalHandler(
    const sp<IPropertyChangedCallback>& callback) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": service object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->RegisterPropertyChangedSignalHandler(callback);
}

}  // namespace shill
