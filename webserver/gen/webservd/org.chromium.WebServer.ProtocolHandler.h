// Automatic generation of D-Bus interfaces:
//  - org.chromium.WebServer.ProtocolHandler
#ifndef ____CHROMEOS_DBUS_BINDING_______________________VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_WEBSERVD_ORG_CHROMIUM_WEBSERVER_PROTOCOLHANDLER_H
#define ____CHROMEOS_DBUS_BINDING_______________________VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_WEBSERVD_ORG_CHROMIUM_WEBSERVER_PROTOCOLHANDLER_H
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

// Interface definition for org::chromium::WebServer::ProtocolHandler.
class ProtocolHandlerInterface {
 public:
  virtual ~ProtocolHandlerInterface() = default;

  // Adds a handler for the given |url|, and optionally request |method|.
  // On success returns a handler ID.
  virtual std::string AddRequestHandler(
      const std::string& in_url,
      const std::string& in_method,
      const std::string& in_service_name) = 0;
  // Removes a previously registered request handler.
  // The |handler_id| is the ID returned from AddHanlder() method.
  virtual bool RemoveRequestHandler(
      chromeos::ErrorPtr* error,
      const std::string& in_request_handler_id) = 0;
  // Returns the contents of the given uploaded file. The |file_id| parameter
  // must correspond to the file_id member of FileInfo structure returned
  // by |Files| property for the given |request_id|.
  virtual bool GetRequestFileData(
      chromeos::ErrorPtr* error,
      const std::string& in_request_id,
      int32_t in_file_id,
      std::vector<uint8_t>* out_contents) = 0;
  // Fulfills the request with specified |request_id| and provides response.
  virtual bool CompleteRequest(
      chromeos::ErrorPtr* error,
      const std::string& in_request_id,
      int32_t in_status_code,
      const std::vector<std::tuple<std::string, std::string>>& in_headers,
      const std::vector<uint8_t>& in_data) = 0;
};

// Interface adaptor for org::chromium::WebServer::ProtocolHandler.
class ProtocolHandlerAdaptor {
 public:
  ProtocolHandlerAdaptor(ProtocolHandlerInterface* interface) : interface_(interface) {}

  void RegisterWithDBusObject(chromeos::dbus_utils::DBusObject* object) {
    chromeos::dbus_utils::DBusInterface* itf =
        object->AddOrGetInterface("org.chromium.WebServer.ProtocolHandler");

    itf->AddSimpleMethodHandler(
        "AddRequestHandler",
        base::Unretained(interface_),
        &ProtocolHandlerInterface::AddRequestHandler);
    itf->AddSimpleMethodHandlerWithError(
        "RemoveRequestHandler",
        base::Unretained(interface_),
        &ProtocolHandlerInterface::RemoveRequestHandler);
    itf->AddSimpleMethodHandlerWithError(
        "GetRequestFileData",
        base::Unretained(interface_),
        &ProtocolHandlerInterface::GetRequestFileData);
    itf->AddSimpleMethodHandlerWithError(
        "CompleteRequest",
        base::Unretained(interface_),
        &ProtocolHandlerInterface::CompleteRequest);

    itf->AddProperty(IdName(), &id_);
    itf->AddProperty(NameName(), &name_);
    itf->AddProperty(PortName(), &port_);
    itf->AddProperty(ProtocolName(), &protocol_);
    itf->AddProperty(CertificateFingerprintName(), &certificate_fingerprint_);
  }

  // Returns a unique ID of this instance.
  static const char* IdName() { return "Id"; }
  std::string GetId() const {
    return id_.GetValue().Get<std::string>();
  }
  void SetId(const std::string& id) {
    id_.SetValue(id);
  }

  // Returns the name of the handler. Multiple related protocol handler
  // could share the same name so that clients don't have to register
  // request handlers for each of them separately.
  static const char* NameName() { return "Name"; }
  std::string GetName() const {
    return name_.GetValue().Get<std::string>();
  }
  void SetName(const std::string& name) {
    name_.SetValue(name);
  }

  // Returns the port number this instance is serving requests on.
  static const char* PortName() { return "Port"; }
  uint16_t GetPort() const {
    return port_.GetValue().Get<uint16_t>();
  }
  void SetPort(uint16_t port) {
    port_.SetValue(port);
  }

  // Returns the protocol name of this instance ("http" or "https").
  static const char* ProtocolName() { return "Protocol"; }
  std::string GetProtocol() const {
    return protocol_.GetValue().Get<std::string>();
  }
  void SetProtocol(const std::string& protocol) {
    protocol_.SetValue(protocol);
  }

  // Returns the TLS certificate fingerprint used for HTTPS instance or
  // empty array if this is an unsecured HTTP instance.
  static const char* CertificateFingerprintName() { return "CertificateFingerprint"; }
  std::vector<uint8_t> GetCertificateFingerprint() const {
    return certificate_fingerprint_.GetValue().Get<std::vector<uint8_t>>();
  }
  void SetCertificateFingerprint(const std::vector<uint8_t>& certificate_fingerprint) {
    certificate_fingerprint_.SetValue(certificate_fingerprint);
  }

 private:
  chromeos::dbus_utils::ExportedProperty<std::string> id_;
  chromeos::dbus_utils::ExportedProperty<std::string> name_;
  chromeos::dbus_utils::ExportedProperty<uint16_t> port_;
  chromeos::dbus_utils::ExportedProperty<std::string> protocol_;
  chromeos::dbus_utils::ExportedProperty<std::vector<uint8_t>> certificate_fingerprint_;

  ProtocolHandlerInterface* interface_;  // Owned by container of this adapter.

  DISALLOW_COPY_AND_ASSIGN(ProtocolHandlerAdaptor);
};

}  // namespace WebServer
}  // namespace chromium
}  // namespace org
#endif  // ____CHROMEOS_DBUS_BINDING_______________________VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_WEBSERVD_ORG_CHROMIUM_WEBSERVER_PROTOCOLHANDLER_H
