// Automatic generation of D-Bus interfaces:
//  - org.chromium.WebServer.RequestHandler
#ifndef ____CHROMEOS_DBUS_BINDING_______________________VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_LIBWEBSERV_ORG_CHROMIUM_WEBSERVER_REQUESTHANDLER_H
#define ____CHROMEOS_DBUS_BINDING_______________________VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_LIBWEBSERV_ORG_CHROMIUM_WEBSERVER_REQUESTHANDLER_H
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

// Interface definition for org::chromium::WebServer::RequestHandler.
class RequestHandlerInterface {
 public:
  virtual ~RequestHandlerInterface() = default;

  // Sends a new HTTP request to the handler.
  // Parameters:
  // - request_info - request metadata. Due to limitation of base::Callback
  //                on the number of parameters, we have to collapse a couple
  //                of distinct parameters into a larger struct, containing:
  //                - (s) protocol_handler_id - ID of the protocol handler.
  //                - (s) request_handler_id - ID of the registered request
  //                                           handler.
  //                - (s) request_id - unique ID of this request within the
  //                                   protocol handler.
  //                - (s) url - The request URL (e.g. "/path/object").
  //                - (s) method - Request method (e.g. "GET", "POST", ...).
  // - headers - Request headers (key-value pairs)
  // - params - an array of request parameters which could be either
  //            URL params (specified after "?" in the request URL), or
  //            form fields in a POST request. Elements have the following
  //            structure:
  //            - (b) true = form field, false = URL param
  //            - (s) field_name
  //            - (s) field_value
  // - files - Information about uploaded files.
  //           The data is an array of FileInfo structures containing the
  //           following fields:
  //           - (i) file_id
  //           - (s) field_name
  //           - (s) file_name
  //           - (s) content_type
  //           - (s) transfer_encoding
  //           The actual contents of the file is obtained by calling
  //           GetFileData() on the request object
  // - body - Raw unparsed request data. Could be empty for POST requests
  //          that have form data/uploaded files already parsed into
  //          form_fields/files parameters.
  virtual bool ProcessRequest(
      chromeos::ErrorPtr* error,
      const std::tuple<std::string, std::string, std::string, std::string, std::string>& in_request_info,
      const std::vector<std::tuple<std::string, std::string>>& in_headers,
      const std::vector<std::tuple<bool, std::string, std::string>>& in_params,
      const std::vector<std::tuple<int32_t, std::string, std::string, std::string, std::string>>& in_files,
      const std::vector<uint8_t>& in_body) = 0;
};

// Interface adaptor for org::chromium::WebServer::RequestHandler.
class RequestHandlerAdaptor {
 public:
  RequestHandlerAdaptor(RequestHandlerInterface* interface) : interface_(interface) {}

  void RegisterWithDBusObject(chromeos::dbus_utils::DBusObject* object) {
    chromeos::dbus_utils::DBusInterface* itf =
        object->AddOrGetInterface("org.chromium.WebServer.RequestHandler");

    itf->AddSimpleMethodHandlerWithError(
        "ProcessRequest",
        base::Unretained(interface_),
        &RequestHandlerInterface::ProcessRequest);
  }

  static dbus::ObjectPath GetObjectPath() {
    return dbus::ObjectPath{"/org/chromium/WebServer/RequestHandler"};
  }

 private:
  RequestHandlerInterface* interface_;  // Owned by container of this adapter.

  DISALLOW_COPY_AND_ASSIGN(RequestHandlerAdaptor);
};

}  // namespace WebServer
}  // namespace chromium
}  // namespace org
#endif  // ____CHROMEOS_DBUS_BINDING_______________________VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_LIBWEBSERV_ORG_CHROMIUM_WEBSERVER_REQUESTHANDLER_H
