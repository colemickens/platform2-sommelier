// Automatic generation of D-Bus interfaces:
//  - org.chromium.WebServer.Server
#ifndef ____CHROMEOS_DBUS_BINDING_______________________VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_WEBSERVD_ORG_CHROMIUM_WEBSERVER_SERVER_H
#define ____CHROMEOS_DBUS_BINDING_______________________VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_WEBSERVD_ORG_CHROMIUM_WEBSERVER_SERVER_H
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <base/macros.h>
#include <dbus/object_path.h>
#include <chromeos/any.h>
#include <chromeos/dbus/dbus_object.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/variant_dictionary.h>

namespace org {
namespace chromium {
namespace WebServer {

// Interface definition for org::chromium::WebServer::Server.
class ServerInterface {
 public:
  virtual ~ServerInterface() = default;

  virtual std::string Ping() = 0;
};

// Interface adaptor for org::chromium::WebServer::Server.
class ServerAdaptor {
 public:
  ServerAdaptor(ServerInterface* interface) : interface_(interface) {}

  void RegisterWithDBusObject(chromeos::dbus_utils::DBusObject* object) {
    chromeos::dbus_utils::DBusInterface* itf =
        object->AddOrGetInterface("org.chromium.WebServer.Server");

    itf->AddSimpleMethodHandler(
        "Ping",
        base::Unretained(interface_),
        &ServerInterface::Ping);
  }

  static dbus::ObjectPath GetObjectPath() {
    return dbus::ObjectPath{"/org/chromium/WebServer/Server"};
  }

 private:
  ServerInterface* interface_;  // Owned by container of this adapter.

  DISALLOW_COPY_AND_ASSIGN(ServerAdaptor);
};

}  // namespace WebServer
}  // namespace chromium
}  // namespace org
#endif  // ____CHROMEOS_DBUS_BINDING_______________________VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_WEBSERVD_ORG_CHROMIUM_WEBSERVER_SERVER_H
