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

#ifndef SHILL_BINDER_DEVICE_BINDER_ADAPTOR_H_
#define SHILL_BINDER_DEVICE_BINDER_ADAPTOR_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <utils/StrongPointer.h>

#include "android/system/connectivity/shill/BnDevice.h"

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

class Device;

// There is a 1:1 mapping between Device and DeviceBinderAdaptor instances.
// Furthermore, the Device owns the DeviceBinderAdaptor and manages its
// lifetime, so we're OK with DeviceBinderAdaptor having a bare pointer to its
// owner device.
class DeviceBinderAdaptor : public BinderAdaptor,
                            public DeviceAdaptorInterface {
 public:
  DeviceBinderAdaptor(BinderControl* control, Device* device,
                      const std::string& id);
  ~DeviceBinderAdaptor() override {};

  // Implementation of DeviceAdaptorInterface.
  const std::string& GetRpcIdentifier() const override { return rpc_id(); }
  void EmitBoolChanged(const std::string& name, bool value) override;
  void EmitUintChanged(const std::string& name, uint32_t value) override;
  void EmitUint16Changed(const std::string& name, uint16_t value) override;
  void EmitIntChanged(const std::string& name, int value) override;
  void EmitStringChanged(const std::string& name,
                         const std::string& value) override;
  void EmitStringmapChanged(const std::string& name,
                            const Stringmap& value) override;
  void EmitStringmapsChanged(const std::string& name,
                             const Stringmaps& value) override;
  void EmitStringsChanged(const std::string& name,
                          const Strings& value) override;
  void EmitKeyValueStoreChanged(const std::string& name,
                                const KeyValueStore& value) override;
  void EmitRpcIdentifierChanged(const std::string& name,
                                const std::string& value) override;
  void EmitRpcIdentifierArrayChanged(
      const std::string& name, const std::vector<std::string>& value) override;

  // Implementation of BnDevice methods. Called by DeviceBinderService.
  android::binder::Status GetInterface(std::string* _aidl_return);
  android::binder::Status GetSelectedService(
      android::sp<android::IBinder>* _aidl_return);
  android::binder::Status RegisterPropertyChangedSignalHandler(
      const android::sp<
          android::system::connectivity::shill::IPropertyChangedCallback>&
          callback);

  Device* device() const { return device_; }

 private:
  Device* device_;

  // IMPORTANT: this needs to be the last member of the class, so that it is
  // destroyed first, invalidating all weak pointers before the remaining state
  // of the object is destroyed.
  base::WeakPtrFactory<DeviceBinderAdaptor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceBinderAdaptor);
};

}  // namespace shill

#endif  // SHILL_BINDER_DEVICE_BINDER_ADAPTOR_H_
