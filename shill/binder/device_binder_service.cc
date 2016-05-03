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

#include "shill/binder/device_binder_service.h"

#include "shill/binder/device_binder_adaptor.h"
#include "shill/logging.h"

using android::binder::Status;
using android::sp;
using android::system::connectivity::shill::IPropertyChangedCallback;
using base::WeakPtr;
using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kBinder;
static string ObjectID(DeviceBinderService* s) {
  return "Device binder service (id " + s->rpc_id() + ")";
}
}  // namespace Logging

DeviceBinderService::DeviceBinderService(WeakPtr<DeviceBinderAdaptor> adaptor,
                                         const string& rpc_id)
    : adaptor_(adaptor), rpc_id_(rpc_id) {}

Status DeviceBinderService::GetInterface(string* _aidl_return) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": device object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->GetInterface(_aidl_return);
}

Status DeviceBinderService::GetSelectedService(sp<IBinder>* _aidl_return) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": device object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->GetSelectedService(_aidl_return);
}

Status DeviceBinderService::RegisterPropertyChangedSignalHandler(
    const sp<IPropertyChangedCallback>& callback) {
  if (!adaptor_) {
    SLOG(this, 2) << __func__ << ": device object is no longer alive.";
    return BinderAdaptor::GenerateShillObjectNotAliveErrorStatus();
  }
  return adaptor_->RegisterPropertyChangedSignalHandler(callback);
}

}  // namespace shill
