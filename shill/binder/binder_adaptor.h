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

#ifndef SHILL_BINDER_BINDER_ADAPTOR_H_
#define SHILL_BINDER_BINDER_ADAPTOR_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <binder/IBinder.h>
#include <utils/StrongPointer.h>

namespace android {
namespace binder {
class Status;
}  // namespace binder
namespace system {
namespace connectivity {
namespace shill {
class IPropertyChangedCallback;
}  // shill
}  // connectivity
}  // system
}  // android

namespace shill {

class BinderControl;

// Superclass for all Binder Adaptor objects.
//
// The following diagram illustrates the relationship between shill objects
// (e.g. Manager, Service), Binder adaptor objects (e.g. ManagerBinderAdaptor,
// ServiceBinderAdaptor), and Binder service objects (e.g. ManagerBinderService,
// ServiceBinderService):
//
// [Shill Object] <-----> [BinderAdaptor] <-----> [BinderService]
//                  1:1                     1:1
//
// Each shill object exposed on shill's Binder interface will own a single
// BinderAdaptor. This BinderAdaptor contains all the logic and state to
// service the methods exposed on the shill object's Binder interface.
//
// Each BinderAdaptor object, in turn, owns a single binder service object. The
// Binder service object actually inherits from the AIDL-generated Bn* class
// (e.g. ManagerBinderService implements BnManager), and is therefore a Binder
// object. The methods implementations in the Binder service class are thin
// wrappers around the actual method handling logic in the corresponding
// BinderAdaptor.
//
// The Binder service object is ref-counted across process boundaries via
// the Binder driver and Android Strong Pointers (android::sp). By having each
// BinderAdaptor hold a Strong Pointer to its corresponding Binder service,
// we ensure that the Binder service backing the shill object will stay alive
// for at least as long as the shill object does.
class BinderAdaptor {
 public:
  BinderAdaptor(BinderControl* control, const std::string& rpc_id);
  virtual ~BinderAdaptor();

  static android::binder::Status GenerateShillObjectNotAliveErrorStatus();

  const android::sp<android::IBinder>& binder_service() {
    return binder_service_;
  }

 protected:
  // Add a IPropertyChangedCallback binder to |property_changed_callbacks_|.
  // This binder's OnPropertyChanged() method will be invoked when shill
  // properties change.
  void AddPropertyChangedSignalHandler(
      const android::sp<
          android::system::connectivity::shill::IPropertyChangedCallback>&
          property_changed_callback);

  // Signals all registered listeners the shill property |name| has changed by
  // calling the OnPropertyChanged() method of all IPropertyChangedCallback
  // binders in |property_changed_callbacks_|.
  void SendPropertyChangedSignal(const std::string& name);

  BinderControl* control() { return control_; }
  const std::string& rpc_id() { return rpc_id_; }
  void set_binder_service(const android::sp<android::IBinder>& binder_service) {
    binder_service_ = binder_service;
  }

 private:
  // Storing this pointer is safe since the ordering of the members of
  // DaemonTask ensure that the BinderControl will outlive all Binder adaptors.
  BinderControl* control_;

  // Used to uniquely identify this Binder adaptor.
  std::string rpc_id_;

  android::sp<android::IBinder> binder_service_;

  std::vector<android::sp<
      android::system::connectivity::shill::IPropertyChangedCallback>>
      property_changed_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(BinderAdaptor);
};

}  // namespace shill

#endif  // SHILL_BINDER_BINDER_ADAPTOR_H_
