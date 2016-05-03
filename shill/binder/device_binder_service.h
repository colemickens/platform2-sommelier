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

#ifndef SHILL_BINDER_DEVICE_BINDER_SERVICE_H_
#define SHILL_BINDER_DEVICE_BINDER_SERVICE_H_

#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <utils/StrongPointer.h>

#include "android/system/connectivity/shill/BnDevice.h"

namespace android {
namespace binder {
class Status;
}  // namespace binder
namespace system {
namespace connectivity {
namespace shill {
class IPropertyChangedCallback;
}  // namespace shill
}  // namespace connectivity
}  // namespace system
}  // namespace android

namespace shill {

class DeviceBinderAdaptor;

// Subclass of the AIDL-generated BnDevice class. Objects of this class are
// Binder objects, and are ref-counted across process boundaries via the Binder
// Driver and Android Strong Pointers (android::sp). Consequently, this object
// might outlive its |adaptor_|. Therefore, |adaptor_| should always be
// null-tested before using it.
class DeviceBinderService
    : public android::system::connectivity::shill::BnDevice {
 public:
  DeviceBinderService(base::WeakPtr<DeviceBinderAdaptor> adaptor,
                      const std::string& rpc_id);
  ~DeviceBinderService() override {}

  const std::string& rpc_id() { return rpc_id_; }

  // Implementation of BnDevice. Calls the actual implementation of these
  // methods in DeviceBinderAdaptor.
  android::binder::Status GetInterface(std::string* _aidl_return) override;
  android::binder::Status GetSelectedService(
      android::sp<IBinder>* _aidl_return) override;
  android::binder::Status RegisterPropertyChangedSignalHandler(
      const android::sp<
          android::system::connectivity::shill::IPropertyChangedCallback>&
          callback) override;

 private:
  base::WeakPtr<DeviceBinderAdaptor> adaptor_;

  // Stored for logging.
  std::string rpc_id_;

  DISALLOW_COPY_AND_ASSIGN(DeviceBinderService);
};

}  // namespace shill

#endif  // SHILL_BINDER_DEVICE_BINDER_SERVICE_H_
