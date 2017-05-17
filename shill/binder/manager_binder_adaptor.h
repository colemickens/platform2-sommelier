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

#ifndef SHILL_BINDER_MANAGER_BINDER_ADAPTOR_H_
#define SHILL_BINDER_MANAGER_BINDER_ADAPTOR_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <utils/StrongPointer.h>

#include "android/system/connectivity/shill/BnManager.h"
#include "shill/adaptor_interfaces.h"
#include "shill/binder/binder_adaptor.h"

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

class Manager;

// Subclass of BinderAdaptor for Manager objects
// There is a 1:1 mapping between Manager and ManagerBinderAdaptor
// instances.  Furthermore, the Manager owns the ManagerBinderAdaptor
// and manages its lifetime, so we're OK with ManagerBinderAdaptor
// having a bare pointer to its owner manager.
class ManagerBinderAdaptor : public BinderAdaptor,
                             public ManagerAdaptorInterface {
 public:
  ManagerBinderAdaptor(BinderControl* control, Manager* manager,
                       const std::string& id);
  ~ManagerBinderAdaptor() override;

  // Implementation of ManagerAdaptorInterface.
  void RegisterAsync(
      const base::Callback<void(bool)>& completion_callback) override;
  const std::string& GetRpcIdentifier() const override { return rpc_id(); }
  void EmitBoolChanged(const std::string& name, bool value) override;
  void EmitUintChanged(const std::string& name, uint32_t value) override;
  void EmitIntChanged(const std::string& name, int value) override;
  void EmitStringChanged(const std::string& name,
                         const std::string& value) override;
  void EmitStringsChanged(const std::string& name,
                          const std::vector<std::string>& value) override;
  void EmitRpcIdentifierChanged(const std::string& name,
                                const std::string& value) override;
  void EmitRpcIdentifierArrayChanged(
      const std::string& name, const std::vector<std::string>& value) override;

  // Implementation of BnManager methods. Called by ManagerBinderService.
  android::binder::Status SetupApModeInterface(
      const android::sp<android::IBinder>& ap_mode_setter,
      std::string* _aidl_return);
  android::binder::Status SetupStationModeInterface(std::string* _aidl_return);
  android::binder::Status ClaimInterface(
      const android::sp<android::IBinder>& claimer,
      const std::string& claimer_name, const std::string& interface_name);
  android::binder::Status ReleaseInterface(
      const android::sp<android::IBinder>& claimer,
      const std::string& claimer_name, const std::string& interface_name);
  android::binder::Status ConfigureService(
      const android::os::PersistableBundle& properties,
      android::sp<android::system::connectivity::shill::IService>*
          _aidl_return);
  android::binder::Status RequestScan(int32_t type);
  android::binder::Status GetDevices(
      std::vector<android::sp<android::IBinder>>* _aidl_return);
  android::binder::Status GetDefaultService(
      android::sp<android::IBinder>* _aidl_return);
  android::binder::Status RegisterPropertyChangedSignalHandler(
      const android::sp<
          android::system::connectivity::shill::IPropertyChangedCallback>&
          callback);

 private:
  void OnApModeSetterVanished();
  void OnDeviceClaimerVanished();

  Manager* manager_;
  android::sp<android::IBinder> ap_mode_setter_;
  android::sp<android::IBinder> device_claimer_;

  // IMPORTANT: this needs to be the last member of the class, so that it is
  // destroyed first, invalidating all weak pointers before the remaining state
  // of the object is destroyed.
  base::WeakPtrFactory<ManagerBinderAdaptor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagerBinderAdaptor);
};

}  // namespace shill

#endif  // SHILL_BINDER_MANAGER_BINDER_ADAPTOR_H_
