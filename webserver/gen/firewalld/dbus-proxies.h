// Automatic generation of D-Bus interfaces:
//  - org.chromium.Firewalld
#ifndef ____CHROMEOS_DBUS_BINDING____________________BUILD_LUMPY_VAR_CACHE_PORTAGE_CHROMEOS_BASE_PERMISSION_BROKER_OUT_DEFAULT_GEN_INCLUDE_FIREWALLD_DBUS_PROXIES_H
#define ____CHROMEOS_DBUS_BINDING____________________BUILD_LUMPY_VAR_CACHE_PORTAGE_CHROMEOS_BASE_PERMISSION_BROKER_OUT_DEFAULT_GEN_INCLUDE_FIREWALLD_DBUS_PROXIES_H
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
namespace Firewalld {
class ObjectManagerProxy;
}  // namespace Firewalld
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {

// Abstract interface proxy for org::chromium::Firewalld.
class FirewalldProxyInterface {
 public:
  virtual ~FirewalldProxyInterface() = default;

  virtual bool PunchTcpHole(
      uint16_t in_port,
      const std::string& in_interface,
      bool* out_success,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void PunchTcpHoleAsync(
      uint16_t in_port,
      const std::string& in_interface,
      const base::Callback<void(bool /*success*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool PunchUdpHole(
      uint16_t in_port,
      const std::string& in_interface,
      bool* out_success,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void PunchUdpHoleAsync(
      uint16_t in_port,
      const std::string& in_interface,
      const base::Callback<void(bool /*success*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool PlugTcpHole(
      uint16_t in_port,
      const std::string& in_interface,
      bool* out_success,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void PlugTcpHoleAsync(
      uint16_t in_port,
      const std::string& in_interface,
      const base::Callback<void(bool /*success*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool PlugUdpHole(
      uint16_t in_port,
      const std::string& in_interface,
      bool* out_success,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void PlugUdpHoleAsync(
      uint16_t in_port,
      const std::string& in_interface,
      const base::Callback<void(bool /*success*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool RequestVpnSetup(
      const std::vector<std::string>& in_usernames,
      const std::string& in_interface,
      bool* out_success,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RequestVpnSetupAsync(
      const std::vector<std::string>& in_usernames,
      const std::string& in_interface,
      const base::Callback<void(bool /*success*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual bool RemoveVpnSetup(
      const std::vector<std::string>& in_usernames,
      const std::string& in_interface,
      bool* out_success,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void RemoveVpnSetupAsync(
      const std::vector<std::string>& in_usernames,
      const std::string& in_interface,
      const base::Callback<void(bool /*success*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;
};

}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {

// Interface proxy for org::chromium::Firewalld.
class FirewalldProxy final : public FirewalldProxyInterface {
 public:
  class PropertySet : public dbus::PropertySet {
   public:
    PropertySet(dbus::ObjectProxy* object_proxy,
                const PropertyChangedCallback& callback)
        : dbus::PropertySet{object_proxy,
                            "org.chromium.Firewalld",
                            callback} {
    }


   private:
    DISALLOW_COPY_AND_ASSIGN(PropertySet);
  };

  FirewalldProxy(const scoped_refptr<dbus::Bus>& bus) :
      bus_{bus},
      dbus_object_proxy_{
          bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~FirewalldProxy() override {
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

  bool PunchTcpHole(
      uint16_t in_port,
      const std::string& in_interface,
      bool* out_success,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.Firewalld",
        "PunchTcpHole",
        error,
        in_port,
        in_interface);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_success);
  }

  void PunchTcpHoleAsync(
      uint16_t in_port,
      const std::string& in_interface,
      const base::Callback<void(bool /*success*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.Firewalld",
        "PunchTcpHole",
        success_callback,
        error_callback,
        in_port,
        in_interface);
  }

  bool PunchUdpHole(
      uint16_t in_port,
      const std::string& in_interface,
      bool* out_success,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.Firewalld",
        "PunchUdpHole",
        error,
        in_port,
        in_interface);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_success);
  }

  void PunchUdpHoleAsync(
      uint16_t in_port,
      const std::string& in_interface,
      const base::Callback<void(bool /*success*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.Firewalld",
        "PunchUdpHole",
        success_callback,
        error_callback,
        in_port,
        in_interface);
  }

  bool PlugTcpHole(
      uint16_t in_port,
      const std::string& in_interface,
      bool* out_success,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.Firewalld",
        "PlugTcpHole",
        error,
        in_port,
        in_interface);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_success);
  }

  void PlugTcpHoleAsync(
      uint16_t in_port,
      const std::string& in_interface,
      const base::Callback<void(bool /*success*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.Firewalld",
        "PlugTcpHole",
        success_callback,
        error_callback,
        in_port,
        in_interface);
  }

  bool PlugUdpHole(
      uint16_t in_port,
      const std::string& in_interface,
      bool* out_success,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.Firewalld",
        "PlugUdpHole",
        error,
        in_port,
        in_interface);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_success);
  }

  void PlugUdpHoleAsync(
      uint16_t in_port,
      const std::string& in_interface,
      const base::Callback<void(bool /*success*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.Firewalld",
        "PlugUdpHole",
        success_callback,
        error_callback,
        in_port,
        in_interface);
  }

  bool RequestVpnSetup(
      const std::vector<std::string>& in_usernames,
      const std::string& in_interface,
      bool* out_success,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.Firewalld",
        "RequestVpnSetup",
        error,
        in_usernames,
        in_interface);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_success);
  }

  void RequestVpnSetupAsync(
      const std::vector<std::string>& in_usernames,
      const std::string& in_interface,
      const base::Callback<void(bool /*success*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.Firewalld",
        "RequestVpnSetup",
        success_callback,
        error_callback,
        in_usernames,
        in_interface);
  }

  bool RemoveVpnSetup(
      const std::vector<std::string>& in_usernames,
      const std::string& in_interface,
      bool* out_success,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.Firewalld",
        "RemoveVpnSetup",
        error,
        in_usernames,
        in_interface);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_success);
  }

  void RemoveVpnSetupAsync(
      const std::vector<std::string>& in_usernames,
      const std::string& in_interface,
      const base::Callback<void(bool /*success*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.Firewalld",
        "RemoveVpnSetup",
        success_callback,
        error_callback,
        in_usernames,
        in_interface);
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  const std::string service_name_{"org.chromium.Firewalld"};
  const dbus::ObjectPath object_path_{"/org/chromium/Firewalld/Firewall"};
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(FirewalldProxy);
};

}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace Firewalld {

class ObjectManagerProxy : public dbus::ObjectManager::Interface {
 public:
  ObjectManagerProxy(const scoped_refptr<dbus::Bus>& bus)
      : bus_{bus},
        dbus_object_manager_{bus->GetObjectManager(
            "org.chromium.Firewalld",
            dbus::ObjectPath{"/org/chromium/Firewalld"})} {
    dbus_object_manager_->RegisterInterface("org.chromium.Firewalld", this);
  }

  ~ObjectManagerProxy() override {
    dbus_object_manager_->UnregisterInterface("org.chromium.Firewalld");
  }

  dbus::ObjectManager* GetObjectManagerProxy() const {
    return dbus_object_manager_;
  }

  org::chromium::FirewalldProxy* GetFirewalldProxy() {
    if (firewalld_instances_.empty())
      return nullptr;
    return firewalld_instances_.begin()->second.get();
  }
  std::vector<org::chromium::FirewalldProxy*> GetFirewalldInstances() const {
    std::vector<org::chromium::FirewalldProxy*> values;
    values.reserve(firewalld_instances_.size());
    for (const auto& pair : firewalld_instances_)
      values.push_back(pair.second.get());
    return values;
  }
  void SetFirewalldAddedCallback(
      const base::Callback<void(org::chromium::FirewalldProxy*)>& callback) {
    on_firewalld_added_ = callback;
  }
  void SetFirewalldRemovedCallback(
      const base::Callback<void(const dbus::ObjectPath&)>& callback) {
    on_firewalld_removed_ = callback;
  }

 private:
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& interface_name,
                         const std::string& property_name) {
  }

  void ObjectAdded(
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    if (interface_name == "org.chromium.Firewalld") {
      std::unique_ptr<org::chromium::FirewalldProxy> firewalld_proxy{
        new org::chromium::FirewalldProxy{bus_}
      };
      auto p = firewalld_instances_.emplace(object_path, std::move(firewalld_proxy));
      if (!on_firewalld_added_.is_null())
        on_firewalld_added_.Run(p.first->second.get());
      return;
    }
  }

  void ObjectRemoved(
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    if (interface_name == "org.chromium.Firewalld") {
      auto p = firewalld_instances_.find(object_path);
      if (p != firewalld_instances_.end()) {
        if (!on_firewalld_removed_.is_null())
          on_firewalld_removed_.Run(object_path);
        firewalld_instances_.erase(p);
      }
      return;
    }
  }

  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    if (interface_name == "org.chromium.Firewalld") {
      return new org::chromium::FirewalldProxy::PropertySet{
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
           std::unique_ptr<org::chromium::FirewalldProxy>> firewalld_instances_;
  base::Callback<void(org::chromium::FirewalldProxy*)> on_firewalld_added_;
  base::Callback<void(const dbus::ObjectPath&)> on_firewalld_removed_;
  base::WeakPtrFactory<ObjectManagerProxy> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ObjectManagerProxy);
};

}  // namespace Firewalld
}  // namespace chromium
}  // namespace org

#endif  // ____CHROMEOS_DBUS_BINDING____________________BUILD_LUMPY_VAR_CACHE_PORTAGE_CHROMEOS_BASE_PERMISSION_BROKER_OUT_DEFAULT_GEN_INCLUDE_FIREWALLD_DBUS_PROXIES_H
