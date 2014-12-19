// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-dbus-bindings/proxy_generator.h"

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "chromeos-dbus-bindings/interface.h"

using std::string;
using std::vector;
using testing::Test;

namespace chromeos_dbus_bindings {

namespace {

const char kInterfaceName[] = "org.chromium.TestInterface";
const char kInterfaceName2[] = "org.chromium.TestInterface2";
const char kMethod1Name[] = "Elements";
const char kMethod1Return[] = "s";
const char kMethod1Argument1[] = "s";
const char kMethod1ArgumentName1[] = "space_walk";
const char kMethod1Argument2[] = "ao";
const char kMethod1ArgumentName2[] = "ramblin_man";
const char kMethod2Name[] = "ReturnToPatagonia";
const char kMethod2Return[] = "x";
const char kMethod3Name[] = "NiceWeatherForDucks";
const char kMethod3Argument1[] = "b";
const char kMethod4Name[] = "ExperimentNumberSix";
const char kMethod5Name[] = "GetPersonInfo";
const char kMethod5Argument1[] = "s";
const char kMethod5ArgumentName1[] = "name";
const char kMethod5Argument2[] = "i";
const char kMethod5ArgumentName2[] = "age";
const char kSignal1Name[] = "Closer";
const char kSignal2Name[] = "TheCurseOfKaZar";
const char kSignal2Argument1[] = "as";
const char kSignal2Argument2[] = "y";
const char kExpectedContent[] = R"literal_string(
#include <memory>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <chromeos/any.h>
#include <chromeos/dbus/dbus_method_invoker.h>
#include <chromeos/dbus/dbus_property.h>
#include <chromeos/dbus/dbus_signal_handler.h>
#include <chromeos/errors/error.h>
#include <chromeos/variant_dictionary.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_manager.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>

namespace org {
namespace chromium {

// Interface proxy for org::chromium::TestInterface.
class TestInterfaceProxy final {
 public:
  TestInterfaceProxy(
      const scoped_refptr<dbus::Bus>& bus,
      const std::string& service_name) :
          bus_{bus},
          service_name_{service_name},
          dbus_object_proxy_{
              bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~TestInterfaceProxy() {
  }

  void RegisterCloserSignalHandler(
      const base::Closure& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
    chromeos::dbus_utils::ConnectToSignal(
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "Closer",
        signal_callback,
        on_connected_callback);
  }

  void RegisterTheCurseOfKaZarSignalHandler(
      const base::Callback<void(const std::vector<std::string>&,
                                uint8_t)>& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
    chromeos::dbus_utils::ConnectToSignal(
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "TheCurseOfKaZar",
        signal_callback,
        on_connected_callback);
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

  bool Elements(
      const std::string& in_space_walk,
      const std::vector<dbus::ObjectPath>& in_ramblin_man,
      std::string* out_3,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "Elements",
        error,
        in_space_walk,
        in_ramblin_man);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_3);
  }

  void ElementsAsync(
      const std::string& in_space_walk,
      const std::vector<dbus::ObjectPath>& in_ramblin_man,
      const base::Callback<void(const std::string&)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "Elements",
        success_callback,
        error_callback,
        in_space_walk,
        in_ramblin_man);
  }

  bool ReturnToPatagonia(
      int64_t* out_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "ReturnToPatagonia",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_1);
  }

  void ReturnToPatagoniaAsync(
      const base::Callback<void(int64_t)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "ReturnToPatagonia",
        success_callback,
        error_callback);
  }

  bool NiceWeatherForDucks(
      bool in_1,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "NiceWeatherForDucks",
        error,
        in_1);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  void NiceWeatherForDucksAsync(
      bool in_1,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "NiceWeatherForDucks",
        success_callback,
        error_callback,
        in_1);
  }

  // Comment line1
  // line2
  bool ExperimentNumberSix(
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "ExperimentNumberSix",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  // Comment line1
  // line2
  void ExperimentNumberSixAsync(
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "ExperimentNumberSix",
        success_callback,
        error_callback);
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  std::string service_name_;
  const dbus::ObjectPath object_path_{"/org/chromium/Test"};
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(TestInterfaceProxy);
};

}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {

// Interface proxy for org::chromium::TestInterface2.
class TestInterface2Proxy final {
 public:
  TestInterface2Proxy(
      const scoped_refptr<dbus::Bus>& bus,
      const std::string& service_name,
      const dbus::ObjectPath& object_path) :
          bus_{bus},
          service_name_{service_name},
          object_path_{object_path},
          dbus_object_proxy_{
              bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~TestInterface2Proxy() {
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

  bool GetPersonInfo(
      std::string* out_name,
      int32_t* out_age,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.TestInterface2",
        "GetPersonInfo",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_name, out_age);
  }

  void GetPersonInfoAsync(
      const base::Callback<void(const std::string& /*name*/, int32_t /*age*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.TestInterface2",
        "GetPersonInfo",
        success_callback,
        error_callback);
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  std::string service_name_;
  dbus::ObjectPath object_path_;
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(TestInterface2Proxy);
};

}  // namespace chromium
}  // namespace org
)literal_string";

const char kExpectedContentWithService[] = R"literal_string(
#include <memory>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <chromeos/any.h>
#include <chromeos/dbus/dbus_method_invoker.h>
#include <chromeos/dbus/dbus_property.h>
#include <chromeos/dbus/dbus_signal_handler.h>
#include <chromeos/errors/error.h>
#include <chromeos/variant_dictionary.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_manager.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>

namespace org {
namespace chromium {

// Interface proxy for org::chromium::TestInterface.
class TestInterfaceProxy final {
 public:
  TestInterfaceProxy(const scoped_refptr<dbus::Bus>& bus) :
      bus_{bus},
      dbus_object_proxy_{
          bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~TestInterfaceProxy() {
  }

  void RegisterCloserSignalHandler(
      const base::Closure& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
    chromeos::dbus_utils::ConnectToSignal(
        dbus_object_proxy_,
        "org.chromium.TestInterface",
        "Closer",
        signal_callback,
        on_connected_callback);
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

 private:
  scoped_refptr<dbus::Bus> bus_;
  const std::string service_name_{"org.chromium.Test"};
  const dbus::ObjectPath object_path_{"/org/chromium/Test"};
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(TestInterfaceProxy);
};

}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {

// Interface proxy for org::chromium::TestInterface2.
class TestInterface2Proxy final {
 public:
  TestInterface2Proxy(
      const scoped_refptr<dbus::Bus>& bus,
      const dbus::ObjectPath& object_path) :
          bus_{bus},
          object_path_{object_path},
          dbus_object_proxy_{
              bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~TestInterface2Proxy() {
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

 private:
  scoped_refptr<dbus::Bus> bus_;
  const std::string service_name_{"org.chromium.Test"};
  dbus::ObjectPath object_path_;
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(TestInterface2Proxy);
};

}  // namespace chromium
}  // namespace org
)literal_string";

const char kExpectedContentWithObjectManager[] = R"literal_string(
#include <memory>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <chromeos/any.h>
#include <chromeos/dbus/dbus_method_invoker.h>
#include <chromeos/dbus/dbus_property.h>
#include <chromeos/dbus/dbus_signal_handler.h>
#include <chromeos/errors/error.h>
#include <chromeos/variant_dictionary.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_manager.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>

namespace org {
namespace chromium {
class ObjectManagerProxy;
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {

// Interface proxy for org::chromium::Itf1.
class Itf1Proxy final {
 public:
  class PropertySet : public dbus::PropertySet {
   public:
    PropertySet(dbus::ObjectProxy* object_proxy,
                const PropertyChangedCallback& callback)
        : dbus::PropertySet{object_proxy,
                            "org.chromium.Itf1",
                            callback} {
      RegisterProperty("data", &data);
    }

    chromeos::dbus_utils::Property<std::string> data;

   private:
    DISALLOW_COPY_AND_ASSIGN(PropertySet);
  };

  Itf1Proxy(
      const scoped_refptr<dbus::Bus>& bus,
      const std::string& service_name,
      PropertySet* property_set) :
          bus_{bus},
          service_name_{service_name},
          property_set_{property_set},
          dbus_object_proxy_{
              bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~Itf1Proxy() {
  }

  void RegisterCloserSignalHandler(
      const base::Closure& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
    chromeos::dbus_utils::ConnectToSignal(
        dbus_object_proxy_,
        "org.chromium.Itf1",
        "Closer",
        signal_callback,
        on_connected_callback);
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

  void SetPropertyChangedCallback(
      const base::Callback<void(Itf1Proxy*, const std::string&)>& callback) {
    on_property_changed_ = callback;
  }

  const PropertySet* GetProperties() const { return property_set_; }
  PropertySet* GetProperties() { return property_set_; }

  const std::string& data() const {
    return property_set_->data.value();
  }

 private:
  void OnPropertyChanged(const std::string& property_name) {
    if (!on_property_changed_.is_null())
      on_property_changed_.Run(this, property_name);
  }

  scoped_refptr<dbus::Bus> bus_;
  std::string service_name_;
  const dbus::ObjectPath object_path_{"/org/chromium/Test/Object"};
  PropertySet* property_set_;
  base::Callback<void(Itf1Proxy*, const std::string&)> on_property_changed_;
  dbus::ObjectProxy* dbus_object_proxy_;

  friend class org::chromium::ObjectManagerProxy;
  DISALLOW_COPY_AND_ASSIGN(Itf1Proxy);
};

}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {

// Interface proxy for org::chromium::Itf2.
class Itf2Proxy final {
 public:
  class PropertySet : public dbus::PropertySet {
   public:
    PropertySet(dbus::ObjectProxy* object_proxy,
                const PropertyChangedCallback& callback)
        : dbus::PropertySet{object_proxy,
                            "org.chromium.Itf2",
                            callback} {
    }


   private:
    DISALLOW_COPY_AND_ASSIGN(PropertySet);
  };

  Itf2Proxy(
      const scoped_refptr<dbus::Bus>& bus,
      const std::string& service_name,
      const dbus::ObjectPath& object_path) :
          bus_{bus},
          service_name_{service_name},
          object_path_{object_path},
          dbus_object_proxy_{
              bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~Itf2Proxy() {
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

 private:
  scoped_refptr<dbus::Bus> bus_;
  std::string service_name_;
  dbus::ObjectPath object_path_;
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(Itf2Proxy);
};

}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {

class ObjectManagerProxy : public dbus::ObjectManager::Interface {
 public:
  ObjectManagerProxy(const scoped_refptr<dbus::Bus>& bus,
                     const std::string& service_name)
      : bus_{bus},
        dbus_object_manager_{bus->GetObjectManager(
            service_name,
            dbus::ObjectPath{"/org/chromium/Test"})} {
    dbus_object_manager_->RegisterInterface("org.chromium.Itf1", this);
    dbus_object_manager_->RegisterInterface("org.chromium.Itf2", this);
  }

  ~ObjectManagerProxy() override {
    dbus_object_manager_->UnregisterInterface("org.chromium.Itf1");
    dbus_object_manager_->UnregisterInterface("org.chromium.Itf2");
  }

  dbus::ObjectManager* GetObjectManagerProxy() const {
    return dbus_object_manager_;
  }

  org::chromium::Itf1Proxy* GetItf1Proxy(
      const dbus::ObjectPath& object_path) {
    auto p = itf1_instances_.find(object_path);
    if (p != itf1_instances_.end())
      return p->second.get();
    return nullptr;
  }
  std::vector<org::chromium::Itf1Proxy*> GetItf1Instances() const {
    std::vector<org::chromium::Itf1Proxy*> values;
    values.reserve(itf1_instances_.size());
    for (const auto& pair : itf1_instances_)
      values.push_back(pair.second.get());
    return values;
  }
  void SetItf1AddedCallback(
      const base::Callback<void(org::chromium::Itf1Proxy*)>& callback) {
    on_itf1_added_ = callback;
  }
  void SetItf1RemovedCallback(
      const base::Callback<void(const dbus::ObjectPath&)>& callback) {
    on_itf1_removed_ = callback;
  }

  org::chromium::Itf2Proxy* GetItf2Proxy(
      const dbus::ObjectPath& object_path) {
    auto p = itf2_instances_.find(object_path);
    if (p != itf2_instances_.end())
      return p->second.get();
    return nullptr;
  }
  std::vector<org::chromium::Itf2Proxy*> GetItf2Instances() const {
    std::vector<org::chromium::Itf2Proxy*> values;
    values.reserve(itf2_instances_.size());
    for (const auto& pair : itf2_instances_)
      values.push_back(pair.second.get());
    return values;
  }
  void SetItf2AddedCallback(
      const base::Callback<void(org::chromium::Itf2Proxy*)>& callback) {
    on_itf2_added_ = callback;
  }
  void SetItf2RemovedCallback(
      const base::Callback<void(const dbus::ObjectPath&)>& callback) {
    on_itf2_removed_ = callback;
  }

 private:
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& interface_name,
                         const std::string& property_name) {
    if (interface_name == "org.chromium.Itf1") {
      auto p = itf1_instances_.find(object_path);
      if (p == itf1_instances_.end())
        return;
      p->second->OnPropertyChanged(property_name);
      return;
    }
  }

  void ObjectAdded(
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    if (interface_name == "org.chromium.Itf1") {
      auto property_set =
          static_cast<org::chromium::Itf1Proxy::PropertySet*>(
              dbus_object_manager_->GetProperties(object_path, interface_name));
      std::unique_ptr<org::chromium::Itf1Proxy> itf1_proxy{
        new org::chromium::Itf1Proxy{bus_, property_set}
      };
      auto p = itf1_instances_.emplace(object_path, std::move(itf1_proxy));
      if (!on_itf1_added_.is_null())
        on_itf1_added_.Run(p.first->second.get());
      return;
    }
    if (interface_name == "org.chromium.Itf2") {
      std::unique_ptr<org::chromium::Itf2Proxy> itf2_proxy{
        new org::chromium::Itf2Proxy{bus_, object_path}
      };
      auto p = itf2_instances_.emplace(object_path, std::move(itf2_proxy));
      if (!on_itf2_added_.is_null())
        on_itf2_added_.Run(p.first->second.get());
      return;
    }
  }

  void ObjectRemoved(
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    if (interface_name == "org.chromium.Itf1") {
      auto p = itf1_instances_.find(object_path);
      if (p != itf1_instances_.end()) {
        if (!on_itf1_removed_.is_null())
          on_itf1_removed_.Run(object_path);
        itf1_instances_.erase(p);
      }
      return;
    }
    if (interface_name == "org.chromium.Itf2") {
      auto p = itf2_instances_.find(object_path);
      if (p != itf2_instances_.end()) {
        if (!on_itf2_removed_.is_null())
          on_itf2_removed_.Run(object_path);
        itf2_instances_.erase(p);
      }
      return;
    }
  }

  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    if (interface_name == "org.chromium.Itf1") {
      return new org::chromium::Itf1Proxy::PropertySet{
          object_proxy,
          base::Bind(&ObjectManagerProxy::OnPropertyChanged,
                     weak_ptr_factory_.GetWeakPtr(),
                     object_path,
                     interface_name)
      };
    }
    if (interface_name == "org.chromium.Itf2") {
      return new org::chromium::Itf2Proxy::PropertySet{
          object_proxy,
          base::Bind(&ObjectManagerProxy::OnPropertyChanged,
                     weak_ptr_factory_.GetWeakPtr(),
                     object_path,
                     interface_name)
      };
    }
    LOG(FATAL) << "Creating properties for unsupported interface "
               << interface_name;
    return nullptr;
  }

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectManager* dbus_object_manager_;
  std::map<dbus::ObjectPath,
           std::unique_ptr<org::chromium::Itf1Proxy>> itf1_instances_;
  base::Callback<void(org::chromium::Itf1Proxy*)> on_itf1_added_;
  base::Callback<void(const dbus::ObjectPath&)> on_itf1_removed_;
  std::map<dbus::ObjectPath,
           std::unique_ptr<org::chromium::Itf2Proxy>> itf2_instances_;
  base::Callback<void(org::chromium::Itf2Proxy*)> on_itf2_added_;
  base::Callback<void(const dbus::ObjectPath&)> on_itf2_removed_;
  base::WeakPtrFactory<ObjectManagerProxy> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ObjectManagerProxy);
};

}  // namespace chromium
}  // namespace org
)literal_string";

const char kExpectedContentWithObjectManagerAndServiceName[] = R"literal_string(
#include <memory>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <chromeos/any.h>
#include <chromeos/dbus/dbus_method_invoker.h>
#include <chromeos/dbus/dbus_property.h>
#include <chromeos/dbus/dbus_signal_handler.h>
#include <chromeos/errors/error.h>
#include <chromeos/variant_dictionary.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_manager.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>

namespace org {
namespace chromium {
class ObjectManagerProxy;
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {

// Interface proxy for org::chromium::Itf1.
class Itf1Proxy final {
 public:
  class PropertySet : public dbus::PropertySet {
   public:
    PropertySet(dbus::ObjectProxy* object_proxy,
                const PropertyChangedCallback& callback)
        : dbus::PropertySet{object_proxy,
                            "org.chromium.Itf1",
                            callback} {
    }


   private:
    DISALLOW_COPY_AND_ASSIGN(PropertySet);
  };

  Itf1Proxy(const scoped_refptr<dbus::Bus>& bus) :
      bus_{bus},
      dbus_object_proxy_{
          bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~Itf1Proxy() {
  }

  void RegisterCloserSignalHandler(
      const base::Closure& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
    chromeos::dbus_utils::ConnectToSignal(
        dbus_object_proxy_,
        "org.chromium.Itf1",
        "Closer",
        signal_callback,
        on_connected_callback);
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

 private:
  scoped_refptr<dbus::Bus> bus_;
  const std::string service_name_{"org.chromium.Test"};
  const dbus::ObjectPath object_path_{"/org/chromium/Test/Object"};
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(Itf1Proxy);
};

}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {

// Interface proxy for org::chromium::Itf2.
class Itf2Proxy final {
 public:
  class PropertySet : public dbus::PropertySet {
   public:
    PropertySet(dbus::ObjectProxy* object_proxy,
                const PropertyChangedCallback& callback)
        : dbus::PropertySet{object_proxy,
                            "org.chromium.Itf2",
                            callback} {
    }


   private:
    DISALLOW_COPY_AND_ASSIGN(PropertySet);
  };

  Itf2Proxy(
      const scoped_refptr<dbus::Bus>& bus,
      const dbus::ObjectPath& object_path) :
          bus_{bus},
          object_path_{object_path},
          dbus_object_proxy_{
              bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~Itf2Proxy() {
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

 private:
  scoped_refptr<dbus::Bus> bus_;
  const std::string service_name_{"org.chromium.Test"};
  dbus::ObjectPath object_path_;
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(Itf2Proxy);
};

}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {

class ObjectManagerProxy : public dbus::ObjectManager::Interface {
 public:
  ObjectManagerProxy(const scoped_refptr<dbus::Bus>& bus)
      : bus_{bus},
        dbus_object_manager_{bus->GetObjectManager(
            "org.chromium.Test",
            dbus::ObjectPath{"/org/chromium/Test"})} {
    dbus_object_manager_->RegisterInterface("org.chromium.Itf1", this);
    dbus_object_manager_->RegisterInterface("org.chromium.Itf2", this);
  }

  ~ObjectManagerProxy() override {
    dbus_object_manager_->UnregisterInterface("org.chromium.Itf1");
    dbus_object_manager_->UnregisterInterface("org.chromium.Itf2");
  }

  dbus::ObjectManager* GetObjectManagerProxy() const {
    return dbus_object_manager_;
  }

  org::chromium::Itf1Proxy* GetItf1Proxy(
      const dbus::ObjectPath& object_path) {
    auto p = itf1_instances_.find(object_path);
    if (p != itf1_instances_.end())
      return p->second.get();
    return nullptr;
  }
  std::vector<org::chromium::Itf1Proxy*> GetItf1Instances() const {
    std::vector<org::chromium::Itf1Proxy*> values;
    values.reserve(itf1_instances_.size());
    for (const auto& pair : itf1_instances_)
      values.push_back(pair.second.get());
    return values;
  }
  void SetItf1AddedCallback(
      const base::Callback<void(org::chromium::Itf1Proxy*)>& callback) {
    on_itf1_added_ = callback;
  }
  void SetItf1RemovedCallback(
      const base::Callback<void(const dbus::ObjectPath&)>& callback) {
    on_itf1_removed_ = callback;
  }

  org::chromium::Itf2Proxy* GetItf2Proxy(
      const dbus::ObjectPath& object_path) {
    auto p = itf2_instances_.find(object_path);
    if (p != itf2_instances_.end())
      return p->second.get();
    return nullptr;
  }
  std::vector<org::chromium::Itf2Proxy*> GetItf2Instances() const {
    std::vector<org::chromium::Itf2Proxy*> values;
    values.reserve(itf2_instances_.size());
    for (const auto& pair : itf2_instances_)
      values.push_back(pair.second.get());
    return values;
  }
  void SetItf2AddedCallback(
      const base::Callback<void(org::chromium::Itf2Proxy*)>& callback) {
    on_itf2_added_ = callback;
  }
  void SetItf2RemovedCallback(
      const base::Callback<void(const dbus::ObjectPath&)>& callback) {
    on_itf2_removed_ = callback;
  }

 private:
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& interface_name,
                         const std::string& property_name) {
  }

  void ObjectAdded(
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    if (interface_name == "org.chromium.Itf1") {
      std::unique_ptr<org::chromium::Itf1Proxy> itf1_proxy{
        new org::chromium::Itf1Proxy{bus_}
      };
      auto p = itf1_instances_.emplace(object_path, std::move(itf1_proxy));
      if (!on_itf1_added_.is_null())
        on_itf1_added_.Run(p.first->second.get());
      return;
    }
    if (interface_name == "org.chromium.Itf2") {
      std::unique_ptr<org::chromium::Itf2Proxy> itf2_proxy{
        new org::chromium::Itf2Proxy{bus_, object_path}
      };
      auto p = itf2_instances_.emplace(object_path, std::move(itf2_proxy));
      if (!on_itf2_added_.is_null())
        on_itf2_added_.Run(p.first->second.get());
      return;
    }
  }

  void ObjectRemoved(
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    if (interface_name == "org.chromium.Itf1") {
      auto p = itf1_instances_.find(object_path);
      if (p != itf1_instances_.end()) {
        if (!on_itf1_removed_.is_null())
          on_itf1_removed_.Run(object_path);
        itf1_instances_.erase(p);
      }
      return;
    }
    if (interface_name == "org.chromium.Itf2") {
      auto p = itf2_instances_.find(object_path);
      if (p != itf2_instances_.end()) {
        if (!on_itf2_removed_.is_null())
          on_itf2_removed_.Run(object_path);
        itf2_instances_.erase(p);
      }
      return;
    }
  }

  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    if (interface_name == "org.chromium.Itf1") {
      return new org::chromium::Itf1Proxy::PropertySet{
          object_proxy,
          base::Bind(&ObjectManagerProxy::OnPropertyChanged,
                     weak_ptr_factory_.GetWeakPtr(),
                     object_path,
                     interface_name)
      };
    }
    if (interface_name == "org.chromium.Itf2") {
      return new org::chromium::Itf2Proxy::PropertySet{
          object_proxy,
          base::Bind(&ObjectManagerProxy::OnPropertyChanged,
                     weak_ptr_factory_.GetWeakPtr(),
                     object_path,
                     interface_name)
      };
    }
    LOG(FATAL) << "Creating properties for unsupported interface "
               << interface_name;
    return nullptr;
  }

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectManager* dbus_object_manager_;
  std::map<dbus::ObjectPath,
           std::unique_ptr<org::chromium::Itf1Proxy>> itf1_instances_;
  base::Callback<void(org::chromium::Itf1Proxy*)> on_itf1_added_;
  base::Callback<void(const dbus::ObjectPath&)> on_itf1_removed_;
  std::map<dbus::ObjectPath,
           std::unique_ptr<org::chromium::Itf2Proxy>> itf2_instances_;
  base::Callback<void(org::chromium::Itf2Proxy*)> on_itf2_added_;
  base::Callback<void(const dbus::ObjectPath&)> on_itf2_removed_;
  base::WeakPtrFactory<ObjectManagerProxy> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ObjectManagerProxy);
};

}  // namespace chromium
}  // namespace org
)literal_string";
}  // namespace

class ProxyGeneratorTest : public Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

 protected:
  base::FilePath CreateInputFile(const string& contents) {
    base::FilePath path;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(), &path));
    EXPECT_EQ(contents.size(),
              base::WriteFile(path, contents.c_str(), contents.size()));
    return path;
  }

  base::ScopedTempDir temp_dir_;
};

TEST_F(ProxyGeneratorTest, GenerateAdaptors) {
  Interface interface;
  interface.name = kInterfaceName;
  interface.path = "/org/chromium/Test";
  interface.methods.emplace_back(
      kMethod1Name,
      vector<Interface::Argument>{
          {kMethod1ArgumentName1, kMethod1Argument1},
          {kMethod1ArgumentName2, kMethod1Argument2}},
      vector<Interface::Argument>{{"", kMethod1Return}});
  interface.methods.emplace_back(
      kMethod2Name,
      vector<Interface::Argument>{},
      vector<Interface::Argument>{{"", kMethod2Return}});
  interface.methods.emplace_back(
      kMethod3Name,
      vector<Interface::Argument>{{"", kMethod3Argument1}},
      vector<Interface::Argument>{});
  interface.methods.emplace_back(kMethod4Name);
  interface.signals.emplace_back(kSignal1Name);
  interface.signals.emplace_back(
      kSignal2Name,
      vector<Interface::Argument>{
          {"", kSignal2Argument1},
          {"", kSignal2Argument2}});
  interface.methods.back().doc_string = "Comment line1\nline2";
  Interface interface2;
  interface2.name = kInterfaceName2;
  interface2.methods.emplace_back(
      kMethod5Name,
      vector<Interface::Argument>{},
      vector<Interface::Argument>{
          {kMethod5ArgumentName1, kMethod5Argument1},
          {kMethod5ArgumentName2, kMethod5Argument2}});
  vector<Interface> interfaces{interface, interface2};
  base::FilePath output_path = temp_dir_.path().Append("output.h");
  ServiceConfig config;
  EXPECT_TRUE(ProxyGenerator::GenerateProxies(config, interfaces, output_path));
  string contents;
  EXPECT_TRUE(base::ReadFileToString(output_path, &contents));
  // The header guards contain the (temporary) filename, so we search for
  // the content we need within the string.
  EXPECT_NE(string::npos, contents.find(kExpectedContent))
      << "Expected to find the following content...\n"
      << kExpectedContent << "...within content...\n" << contents;
}

TEST_F(ProxyGeneratorTest, GenerateAdaptorsWithServiceName) {
  Interface interface;
  interface.name = kInterfaceName;
  interface.path = "/org/chromium/Test";
  interface.signals.emplace_back(kSignal1Name);
  Interface interface2;
  interface2.name = kInterfaceName2;
  vector<Interface> interfaces{interface, interface2};
  base::FilePath output_path = temp_dir_.path().Append("output2.h");
  ServiceConfig config;
  config.service_name = "org.chromium.Test";
  EXPECT_TRUE(ProxyGenerator::GenerateProxies(config, interfaces, output_path));
  string contents;
  EXPECT_TRUE(base::ReadFileToString(output_path, &contents));
  // The header guards contain the (temporary) filename, so we search for
  // the content we need within the string.
  EXPECT_NE(string::npos, contents.find(kExpectedContentWithService))
      << "Expected to find the following content...\n"
      << kExpectedContentWithService << "...within content...\n" << contents;
}

TEST_F(ProxyGeneratorTest, GenerateAdaptorsWithObjectManager) {
  Interface interface;
  interface.name = "org.chromium.Itf1";
  interface.path = "/org/chromium/Test/Object";
  interface.signals.emplace_back(kSignal1Name);
  interface.properties.emplace_back("data", "s", "read");
  Interface interface2;
  interface2.name = "org.chromium.Itf2";
  vector<Interface> interfaces{interface, interface2};
  base::FilePath output_path = temp_dir_.path().Append("output3.h");
  ServiceConfig config;
  config.object_manager.name = "org.chromium.ObjectManager";
  config.object_manager.object_path = "/org/chromium/Test";
  EXPECT_TRUE(ProxyGenerator::GenerateProxies(config, interfaces, output_path));
  string contents;
  EXPECT_TRUE(base::ReadFileToString(output_path, &contents));
  // The header guards contain the (temporary) filename, so we search for
  // the content we need within the string.
  EXPECT_NE(string::npos, contents.find(kExpectedContentWithObjectManager))
      << "Expected to find the following content...\n"
      << kExpectedContentWithObjectManager << "...within content...\n"
      << contents;
}

TEST_F(ProxyGeneratorTest, GenerateAdaptorsWithObjectManagerAndServiceName) {
  Interface interface;
  interface.name = "org.chromium.Itf1";
  interface.path = "/org/chromium/Test/Object";
  interface.signals.emplace_back(kSignal1Name);
  Interface interface2;
  interface2.name = "org.chromium.Itf2";
  vector<Interface> interfaces{interface, interface2};
  base::FilePath output_path = temp_dir_.path().Append("output4.h");
  ServiceConfig config;
  config.service_name = "org.chromium.Test";
  config.object_manager.name = "org.chromium.ObjectManager";
  config.object_manager.object_path = "/org/chromium/Test";
  EXPECT_TRUE(ProxyGenerator::GenerateProxies(config, interfaces, output_path));
  string contents;
  EXPECT_TRUE(base::ReadFileToString(output_path, &contents));
  // The header guards contain the (temporary) filename, so we search for
  // the content we need within the string.
  EXPECT_NE(string::npos,
            contents.find(kExpectedContentWithObjectManagerAndServiceName))
      << "Expected to find the following content...\n"
      << kExpectedContentWithObjectManagerAndServiceName
      << "...within content...\n" << contents;
}

}  // namespace chromeos_dbus_bindings
