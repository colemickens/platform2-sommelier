// Automatic generation of D-Bus interfaces:
//  - org.chromium.WebServer.RequestHandler
#ifndef ____CHROMEOS_DBUS_BINDING___BUILD_LUMPY_VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_LIBWEBSERV_DBUS_PROXIES_H
#define ____CHROMEOS_DBUS_BINDING___BUILD_LUMPY_VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_LIBWEBSERV_DBUS_PROXIES_H
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

// Abstract interface proxy for org::chromium::WebServer::RequestHandler.
class RequestHandlerProxyInterface {
 public:
  virtual ~RequestHandlerProxyInterface() = default;

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
      const std::tuple<std::string, std::string, std::string, std::string, std::string>& in_request_info,
      const std::vector<std::tuple<std::string, std::string>>& in_headers,
      const std::vector<std::tuple<bool, std::string, std::string>>& in_params,
      const std::vector<std::tuple<int32_t, std::string, std::string, std::string, std::string>>& in_files,
      const std::vector<uint8_t>& in_body,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;

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
  virtual void ProcessRequestAsync(
      const std::tuple<std::string, std::string, std::string, std::string, std::string>& in_request_info,
      const std::vector<std::tuple<std::string, std::string>>& in_headers,
      const std::vector<std::tuple<bool, std::string, std::string>>& in_params,
      const std::vector<std::tuple<int32_t, std::string, std::string, std::string, std::string>>& in_files,
      const std::vector<uint8_t>& in_body,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) = 0;
};

}  // namespace WebServer
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace WebServer {

// Interface proxy for org::chromium::WebServer::RequestHandler.
class RequestHandlerProxy final : public RequestHandlerProxyInterface {
 public:
  RequestHandlerProxy(
      const scoped_refptr<dbus::Bus>& bus,
      const std::string& service_name) :
          bus_{bus},
          service_name_{service_name},
          dbus_object_proxy_{
              bus_->GetObjectProxy(service_name_, object_path_)} {
  }

  ~RequestHandlerProxy() override {
  }

  void ReleaseObjectProxy(const base::Closure& callback) {
    bus_->RemoveObjectProxy(service_name_, object_path_, callback);
  }

  const dbus::ObjectPath& GetObjectPath() const {
    return object_path_;
  }

  dbus::ObjectProxy* GetObjectProxy() const { return dbus_object_proxy_; }

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
  bool ProcessRequest(
      const std::tuple<std::string, std::string, std::string, std::string, std::string>& in_request_info,
      const std::vector<std::tuple<std::string, std::string>>& in_headers,
      const std::vector<std::tuple<bool, std::string, std::string>>& in_params,
      const std::vector<std::tuple<int32_t, std::string, std::string, std::string, std::string>>& in_files,
      const std::vector<uint8_t>& in_body,
      chromeos::ErrorPtr* error,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    auto response = chromeos::dbus_utils::CallMethodAndBlockWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.WebServer.RequestHandler",
        "ProcessRequest",
        error,
        in_request_info,
        in_headers,
        in_params,
        in_files,
        in_body);
    return response && chromeos::dbus_utils::ExtractMethodCallResults(
        response.get(), error);
  }

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
  void ProcessRequestAsync(
      const std::tuple<std::string, std::string, std::string, std::string, std::string>& in_request_info,
      const std::vector<std::tuple<std::string, std::string>>& in_headers,
      const std::vector<std::tuple<bool, std::string, std::string>>& in_params,
      const std::vector<std::tuple<int32_t, std::string, std::string, std::string, std::string>>& in_files,
      const std::vector<uint8_t>& in_body,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(chromeos::Error*)>& error_callback,
      int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) override {
    chromeos::dbus_utils::CallMethodWithTimeout(
        timeout_ms,
        dbus_object_proxy_,
        "org.chromium.WebServer.RequestHandler",
        "ProcessRequest",
        success_callback,
        error_callback,
        in_request_info,
        in_headers,
        in_params,
        in_files,
        in_body);
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  std::string service_name_;
  const dbus::ObjectPath object_path_{"/org/chromium/WebServer/RequestHandler"};
  dbus::ObjectProxy* dbus_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(RequestHandlerProxy);
};

}  // namespace WebServer
}  // namespace chromium
}  // namespace org

#endif  // ____CHROMEOS_DBUS_BINDING___BUILD_LUMPY_VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_LIBWEBSERV_DBUS_PROXIES_H
