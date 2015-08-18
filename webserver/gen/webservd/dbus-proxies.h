// Automatic generation of D-Bus interfaces:
//  - org.chromium.WebServer.ProtocolHandler
//  - org.chromium.WebServer.Server
#ifndef ____CHROMEOS_DBUS_BINDING___BUILD_LUMPY_VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_WEBSERVD_DBUS_PROXIES_H
#define ____CHROMEOS_DBUS_BINDING___BUILD_LUMPY_VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_WEBSERVD_DBUS_PROXIES_H
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
namespace WebServer {
class ObjectManagerProxy;
}  // namespace WebServer
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace WebServer {

// Abstract interface proxy for org::chromium::WebServer::ProtocolHandler.
class ProtocolHandlerProxyInterface {
 public:
  virtual ~ProtocolHandlerProxyInterface() = default;

  // Adds a handler for the given |url|, and optionally request |method|.
  // On success returns a handler ID.
  virtual bool AddRequestHandler(
      const std::string& in_url,
      const std::string& in_method,
      const std::string& in_service_name,
      std::string* out_request_handler_id,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  // Adds a handler for the given |url|, and optionally request |method|.
  // On success returns a handler ID.
  virtual void AddRequestHandlerAsync(
      const std::string& in_url,
      const std::string& in_method,
      const std::string& in_service_name,
      const base::Callback<void(const std::string& /*request_handler_id*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  // Removes a previously registered request handler.
  // The |handler_id| is the ID returned from AddHanlder() method.
  virtual bool RemoveRequestHandler(
      const std::string& in_request_handler_id,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  // Removes a previously registered request handler.
  // The |handler_id| is the ID returned from AddHanlder() method.
  virtual void RemoveRequestHandlerAsync(
      const std::string& in_request_handler_id,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  // Returns the contents of the given uploaded file. The |file_id| parameter
  // must correspond to the file_id member of FileInfo structure returned
  // by |Files| property for the given |request_id|.
  virtual bool GetRequestFileData(
      const std::string& in_request_id,
      int32_t in_file_id,
      std::vector<uint8_t>* out_contents,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  // Returns the contents of the given uploaded file. The |file_id| parameter
  // must correspond to the file_id member of FileInfo structure returned
  // by |Files| property for the given |request_id|.
  virtual void GetRequestFileDataAsync(
      const std::string& in_request_id,
      int32_t in_file_id,
      const base::Callback<void(const std::vector<uint8_t>& /*contents*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  // Fulfills the request with specified |request_id| and provides response.
  virtual bool CompleteRequest(
      const std::string& in_request_id,
      int32_t in_status_code,
      const std::vector<std::tuple<std::string, std::string>>& in_headers,
      const std::vector<uint8_t>& in_data,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  // Fulfills the request with specified |request_id| and provides response.
  virtual void CompleteRequestAsync(
      const std::string& in_request_id,
      int32_t in_status_code,
      const std::vector<std::tuple<std::string, std::string>>& in_headers,
      const std::vector<uint8_t>& in_data,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  static const char* IdName() { return "Id"; }
  virtual const std::string& id() const = 0;
  static const char* NameName() { return "Name"; }
  virtual const std::string& name() const = 0;
  static const char* PortName() { return "Port"; }
  virtual uint16_t port() const = 0;
  static const char* ProtocolName() { return "Protocol"; }
  virtual const std::string& protocol() const = 0;
  static const char* CertificateFingerprintName() { return "CertificateFingerprint"; }
  virtual const std::vector<uint8_t>& certificate_fingerprint() const = 0;
};

}  // namespace WebServer
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace WebServer {

// Interface proxy for org::chromium::WebServer::ProtocolHandler.
class ProtocolHandlerProxy final : public ProtocolHandlerProxyInterface {
 public:
  class PropertySet : public dbus::PropertySet {
   public:
    PropertySet(dbus::ObjectProxy* object_proxy,
                const PropertyChangedCallback& callback)
        : dbus::PropertySet{object_proxy,
                            "org.chromium.WebServer.ProtocolHandler",
                            callback} {
      RegisterProperty(IdName(), &id);
      RegisterProperty(NameName(), &name);
      RegisterProperty(PortName(), &port);
      RegisterProperty(ProtocolName(), &protocol);
      RegisterProperty(CertificateFingerprintName(), &certificate_fingerprint);
    }

    chromeos::dbus_utils::Property<std::string> id;
    chromeos::dbus_utils::Property<std::string> name;
    chromeos::dbus_utils::Property<uint16_t> port;
    chromeos::dbus_utils::Property<std::string> protocol;
    chromeos::dbus_utils::Property<std::vector<uint8_t>> certificate_fingerprint;

   private:
    DISALLOW_COPY_AND_ASSIGN(PropertySet);
  };

  ProtocolHandlerProxy(
      const scoped_refptr<dbus::Bus>& bus,
      const dbus::ObjectPath& object_path,
      PropertySet* property_set) :
          bus_{bus},
          object_path_{object_path},
          property_set_{property_set},
          dbus_object_proxy_{
              bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~ProtocolHandlerProxy() override {
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

  void SetPropertyChangedCallback(
      const base::Callback<void(ProtocolHandlerProxy*, const std::string&)>& callback) {
    on_property_changed_ = callback;
  }

  const PropertySet* GetProperties() const { return property_set_; }
  PropertySet* GetProperties() { return property_set_; }

  // Adds a handler for the given |url|, and optionally request |method|.
  // On success returns a handler ID.
  bool AddRequestHandler(
      const std::string& in_url,
      const std::string& in_method,
      const std::string& in_service_name,
      std::string* out_request_handler_id,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.WebServer.ProtocolHandler",
        "AddRequestHandler",
        error,
        in_url,
        in_method,
        in_service_name);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_request_handler_id);
  }

  // Adds a handler for the given |url|, and optionally request |method|.
  // On success returns a handler ID.
  void AddRequestHandlerAsync(
      const std::string& in_url,
      const std::string& in_method,
      const std::string& in_service_name,
      const base::Callback<void(const std::string& /*request_handler_id*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.WebServer.ProtocolHandler",
        "AddRequestHandler",
        success_callback,
        error_callback,
        in_url,
        in_method,
        in_service_name);
  }

  // Removes a previously registered request handler.
  // The |handler_id| is the ID returned from AddHanlder() method.
  bool RemoveRequestHandler(
      const std::string& in_request_handler_id,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.WebServer.ProtocolHandler",
        "RemoveRequestHandler",
        error,
        in_request_handler_id);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  // Removes a previously registered request handler.
  // The |handler_id| is the ID returned from AddHanlder() method.
  void RemoveRequestHandlerAsync(
      const std::string& in_request_handler_id,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.WebServer.ProtocolHandler",
        "RemoveRequestHandler",
        success_callback,
        error_callback,
        in_request_handler_id);
  }

  // Returns the contents of the given uploaded file. The |file_id| parameter
  // must correspond to the file_id member of FileInfo structure returned
  // by |Files| property for the given |request_id|.
  bool GetRequestFileData(
      const std::string& in_request_id,
      int32_t in_file_id,
      std::vector<uint8_t>* out_contents,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.WebServer.ProtocolHandler",
        "GetRequestFileData",
        error,
        in_request_id,
        in_file_id);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_contents);
  }

  // Returns the contents of the given uploaded file. The |file_id| parameter
  // must correspond to the file_id member of FileInfo structure returned
  // by |Files| property for the given |request_id|.
  void GetRequestFileDataAsync(
      const std::string& in_request_id,
      int32_t in_file_id,
      const base::Callback<void(const std::vector<uint8_t>& /*contents*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.WebServer.ProtocolHandler",
        "GetRequestFileData",
        success_callback,
        error_callback,
        in_request_id,
        in_file_id);
  }

  // Fulfills the request with specified |request_id| and provides response.
  bool CompleteRequest(
      const std::string& in_request_id,
      int32_t in_status_code,
      const std::vector<std::tuple<std::string, std::string>>& in_headers,
      const std::vector<uint8_t>& in_data,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.WebServer.ProtocolHandler",
        "CompleteRequest",
        error,
        in_request_id,
        in_status_code,
        in_headers,
        in_data);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

  // Fulfills the request with specified |request_id| and provides response.
  void CompleteRequestAsync(
      const std::string& in_request_id,
      int32_t in_status_code,
      const std::vector<std::tuple<std::string, std::string>>& in_headers,
      const std::vector<uint8_t>& in_data,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.WebServer.ProtocolHandler",
        "CompleteRequest",
        success_callback,
        error_callback,
        in_request_id,
        in_status_code,
        in_headers,
        in_data);
  }

  const std::string& id() const override {
    return property_set_->id.value();
  }

  const std::string& name() const override {
    return property_set_->name.value();
  }

  uint16_t port() const override {
    return property_set_->port.value();
  }

  const std::string& protocol() const override {
    return property_set_->protocol.value();
  }

  const std::vector<uint8_t>& certificate_fingerprint() const override {
    return property_set_->certificate_fingerprint.value();
  }

 private:
  void OnPropertyChanged(const std::string& property_name) {
    if (!on_property_changed_.is_null())
      on_property_changed_.Run(this, property_name);
  }

  scoped_refptr<dbus::Bus> bus_;
  const std::string service_name_{"org.chromium.WebServer"};
  dbus::ObjectPath object_path_;
  PropertySet* property_set_;
  base::Callback<void(ProtocolHandlerProxy*, const std::string&)> on_property_changed_;
  dbus::ObjectProxy* dbus_object_proxy_;

  friend class org::chromium::WebServer::ObjectManagerProxy;
  DISALLOW_COPY_AND_ASSIGN(ProtocolHandlerProxy);
};

}  // namespace WebServer
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace WebServer {

// Abstract interface proxy for org::chromium::WebServer::Server.
class ServerProxyInterface {
 public:
  virtual ~ServerProxyInterface() = default;

  virtual bool Ping(
      std::string* out_message,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

  virtual void PingAsync(
      const base::Callback<void(const std::string& /*message*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;
};

}  // namespace WebServer
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace WebServer {

// Interface proxy for org::chromium::WebServer::Server.
class ServerProxy final : public ServerProxyInterface {
 public:
  class PropertySet : public dbus::PropertySet {
   public:
    PropertySet(dbus::ObjectProxy* object_proxy,
                const PropertyChangedCallback& callback)
        : dbus::PropertySet{object_proxy,
                            "org.chromium.WebServer.Server",
                            callback} {
    }


   private:
    DISALLOW_COPY_AND_ASSIGN(PropertySet);
  };

  ServerProxy(const scoped_refptr<dbus::Bus>& bus) :
      bus_{bus},
      dbus_object_proxy_{
          bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~ServerProxy() override {
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

  bool Ping(
      std::string* out_message,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.WebServer.Server",
        "Ping",
        error);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error, out_message);
  }

  void PingAsync(
      const base::Callback<void(const std::string& /*message*/)>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.WebServer.Server",
        "Ping",
        success_callback,
        error_callback);
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  const std::string service_name_{"org.chromium.WebServer"};
  const dbus::ObjectPath object_path_{"/org/chromium/WebServer/Server"};
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ServerProxy);
};

}  // namespace WebServer
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace WebServer {

class ObjectManagerProxy : public dbus::ObjectManager::Interface {
 public:
  ObjectManagerProxy(const scoped_refptr<dbus::Bus>& bus)
      : bus_{bus},
        dbus_object_manager_{bus->GetObjectManager(
            "org.chromium.WebServer",
            dbus::ObjectPath{"/org/chromium/WebServer"})} {
    dbus_object_manager_->RegisterInterface("org.chromium.WebServer.ProtocolHandler", this);
    dbus_object_manager_->RegisterInterface("org.chromium.WebServer.Server", this);
  }

  ~ObjectManagerProxy() override {
    dbus_object_manager_->UnregisterInterface("org.chromium.WebServer.ProtocolHandler");
    dbus_object_manager_->UnregisterInterface("org.chromium.WebServer.Server");
  }

  dbus::ObjectManager* GetObjectManagerProxy() const {
    return dbus_object_manager_;
  }

  org::chromium::WebServer::ProtocolHandlerProxy* GetProtocolHandlerProxy(
      const dbus::ObjectPath& object_path) {
    auto p = protocol_handler_instances_.find(object_path);
    if (p != protocol_handler_instances_.end())
      return p->second.get();
    return nullptr;
  }
  std::vector<org::chromium::WebServer::ProtocolHandlerProxy*> GetProtocolHandlerInstances() const {
    std::vector<org::chromium::WebServer::ProtocolHandlerProxy*> values;
    values.reserve(protocol_handler_instances_.size());
    for (const auto& pair : protocol_handler_instances_)
      values.push_back(pair.second.get());
    return values;
  }
  void SetProtocolHandlerAddedCallback(
      const base::Callback<void(org::chromium::WebServer::ProtocolHandlerProxy*)>& callback) {
    on_protocol_handler_added_ = callback;
  }
  void SetProtocolHandlerRemovedCallback(
      const base::Callback<void(const dbus::ObjectPath&)>& callback) {
    on_protocol_handler_removed_ = callback;
  }

  org::chromium::WebServer::ServerProxy* GetServerProxy() {
    if (server_instances_.empty())
      return nullptr;
    return server_instances_.begin()->second.get();
  }
  std::vector<org::chromium::WebServer::ServerProxy*> GetServerInstances() const {
    std::vector<org::chromium::WebServer::ServerProxy*> values;
    values.reserve(server_instances_.size());
    for (const auto& pair : server_instances_)
      values.push_back(pair.second.get());
    return values;
  }
  void SetServerAddedCallback(
      const base::Callback<void(org::chromium::WebServer::ServerProxy*)>& callback) {
    on_server_added_ = callback;
  }
  void SetServerRemovedCallback(
      const base::Callback<void(const dbus::ObjectPath&)>& callback) {
    on_server_removed_ = callback;
  }

 private:
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& interface_name,
                         const std::string& property_name) {
    if (interface_name == "org.chromium.WebServer.ProtocolHandler") {
      auto p = protocol_handler_instances_.find(object_path);
      if (p == protocol_handler_instances_.end())
        return;
      p->second->OnPropertyChanged(property_name);
      return;
    }
  }

  void ObjectAdded(
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    if (interface_name == "org.chromium.WebServer.ProtocolHandler") {
      auto property_set =
          static_cast<org::chromium::WebServer::ProtocolHandlerProxy::PropertySet*>(
              dbus_object_manager_->GetProperties(object_path, interface_name));
      std::unique_ptr<org::chromium::WebServer::ProtocolHandlerProxy> protocol_handler_proxy{
        new org::chromium::WebServer::ProtocolHandlerProxy{bus_, object_path, property_set}
      };
      auto p = protocol_handler_instances_.emplace(object_path, std::move(protocol_handler_proxy));
      if (!on_protocol_handler_added_.is_null())
        on_protocol_handler_added_.Run(p.first->second.get());
      return;
    }
    if (interface_name == "org.chromium.WebServer.Server") {
      std::unique_ptr<org::chromium::WebServer::ServerProxy> server_proxy{
        new org::chromium::WebServer::ServerProxy{bus_}
      };
      auto p = server_instances_.emplace(object_path, std::move(server_proxy));
      if (!on_server_added_.is_null())
        on_server_added_.Run(p.first->second.get());
      return;
    }
  }

  void ObjectRemoved(
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    if (interface_name == "org.chromium.WebServer.ProtocolHandler") {
      auto p = protocol_handler_instances_.find(object_path);
      if (p != protocol_handler_instances_.end()) {
        if (!on_protocol_handler_removed_.is_null())
          on_protocol_handler_removed_.Run(object_path);
        protocol_handler_instances_.erase(p);
      }
      return;
    }
    if (interface_name == "org.chromium.WebServer.Server") {
      auto p = server_instances_.find(object_path);
      if (p != server_instances_.end()) {
        if (!on_server_removed_.is_null())
          on_server_removed_.Run(object_path);
        server_instances_.erase(p);
      }
      return;
    }
  }

  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override {
    if (interface_name == "org.chromium.WebServer.ProtocolHandler") {
      return new org::chromium::WebServer::ProtocolHandlerProxy::PropertySet{
          object_proxy,
          base::Bind(&ObjectManagerProxy::OnPropertyChanged,
                     weak_ptr_factory_.GetWeakPtr(),
                     object_path,
                     interface_name)
      };
    }
    if (interface_name == "org.chromium.WebServer.Server") {
      return new org::chromium::WebServer::ServerProxy::PropertySet{
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
           std::unique_ptr<org::chromium::WebServer::ProtocolHandlerProxy>> protocol_handler_instances_;
  base::Callback<void(org::chromium::WebServer::ProtocolHandlerProxy*)> on_protocol_handler_added_;
  base::Callback<void(const dbus::ObjectPath&)> on_protocol_handler_removed_;
  std::map<dbus::ObjectPath,
           std::unique_ptr<org::chromium::WebServer::ServerProxy>> server_instances_;
  base::Callback<void(org::chromium::WebServer::ServerProxy*)> on_server_added_;
  base::Callback<void(const dbus::ObjectPath&)> on_server_removed_;
  base::WeakPtrFactory<ObjectManagerProxy> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ObjectManagerProxy);
};

}  // namespace WebServer
}  // namespace chromium
}  // namespace org

#endif  // ____CHROMEOS_DBUS_BINDING___BUILD_LUMPY_VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_WEBSERVD_DBUS_PROXIES_H
