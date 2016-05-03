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

#ifndef SHILL_BINDER_MANAGER_BINDER_SERVICE_H_
#define SHILL_BINDER_MANAGER_BINDER_SERVICE_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <utils/StrongPointer.h>

#include "android/system/connectivity/shill/BnManager.h"

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

class ManagerBinderAdaptor;

// Subclass of the AIDL-generated BnManager class. Objects of this class are
// Binder objects, and are ref-counted across process boundaries via the Binder
// Driver and Android Strong Pointers (android::sp). Consequently, this object
// might outlive its |adaptor_|. Therefore, |adaptor_| should always be
// null-tested before using it.
class ManagerBinderService
    : public android::system::connectivity::shill::BnManager {
 public:
  ManagerBinderService(const base::WeakPtr<ManagerBinderAdaptor> adaptor,
                       const std::string& rpc_id);
  ~ManagerBinderService() override {}

  const std::string& rpc_id() { return rpc_id_; }

  // Implementation of BnManager. Calls the actual implementation of these
  // methods in ManagerBinderAdaptor.
  android::binder::Status SetupApModeInterface(
      const android::sp<IBinder>& ap_mode_setter,
      std::string* _aidl_return) override;
  android::binder::Status SetupStationModeInterface(
      std::string* _aidl_return) override;
  android::binder::Status ClaimInterface(
      const android::sp<IBinder>& claimer, const std::string& claimer_name,
      const std::string& interface_name) override;
  android::binder::Status ReleaseInterface(
      const android::sp<IBinder>& claimer, const std::string& claimer_name,
      const std::string& interface_name) override;
  android::binder::Status ConfigureService(
      const android::os::PersistableBundle& properties,
      android::sp<android::system::connectivity::shill::IService>* _aidl_return)
      override;
  android::binder::Status RequestScan(int32_t type) override;
  android::binder::Status GetDevices(
      std::vector<android::sp<android::IBinder>>* _aidl_return);
  android::binder::Status GetDefaultService(
      android::sp<android::IBinder>* _aidl_return) override;
  android::binder::Status RegisterPropertyChangedSignalHandler(
      const android::sp<
          android::system::connectivity::shill::IPropertyChangedCallback>&
          callback) override;

 private:
  base::WeakPtr<ManagerBinderAdaptor> adaptor_;

  // Stored for logging.
  std::string rpc_id_;

  DISALLOW_COPY_AND_ASSIGN(ManagerBinderService);
};

}  // namespace shill

#endif  // SHILL_BINDER_MANAGER_BINDER_SERVICE_H_
