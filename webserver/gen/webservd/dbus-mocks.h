// Automatic generation of D-Bus interface mock proxies for:
//  - org.chromium.WebServer.ProtocolHandler
//  - org.chromium.WebServer.Server
#ifndef ____CHROMEOS_DBUS_BINDING___BUILD_LUMPY_VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_WEBSERVD_DBUS_MOCKS_H
#define ____CHROMEOS_DBUS_BINDING___BUILD_LUMPY_VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_WEBSERVD_DBUS_MOCKS_H
#include <string>
#include <vector>

#include <base/callback_forward.h>
#include <base/logging.h>
#include <base/macros.h>
#include <chromeos/any.h>
#include <chromeos/errors/error.h>
#include <chromeos/variant_dictionary.h>
#include <gmock/gmock.h>

#include "dbus-proxies.h"

namespace org {
namespace chromium {
namespace WebServer {

// Mock object for ProtocolHandlerProxyInterface.
class ProtocolHandlerProxyMock : public ProtocolHandlerProxyInterface {
 public:
  ProtocolHandlerProxyMock() = default;

  MOCK_METHOD6(AddRequestHandler,
               bool(const std::string& /*in_url*/,
                    const std::string& /*in_method*/,
                    const std::string& /*in_service_name*/,
                    std::string* /*out_request_handler_id*/,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD6(AddRequestHandlerAsync,
               void(const std::string& /*in_url*/,
                    const std::string& /*in_method*/,
                    const std::string& /*in_service_name*/,
                    const base::Callback<void(const std::string& /*request_handler_id*/)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(RemoveRequestHandler,
               bool(const std::string& /*in_request_handler_id*/,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD4(RemoveRequestHandlerAsync,
               void(const std::string& /*in_request_handler_id*/,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(GetRequestFileData,
               bool(const std::string& /*in_request_id*/,
                    int32_t /*in_file_id*/,
                    std::vector<uint8_t>* /*out_contents*/,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD5(GetRequestFileDataAsync,
               void(const std::string& /*in_request_id*/,
                    int32_t /*in_file_id*/,
                    const base::Callback<void(const std::vector<uint8_t>& /*contents*/)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_METHOD6(CompleteRequest,
               bool(const std::string& /*in_request_id*/,
                    int32_t /*in_status_code*/,
                    const std::vector<std::tuple<std::string, std::string>>& /*in_headers*/,
                    const std::vector<uint8_t>& /*in_data*/,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD7(CompleteRequestAsync,
               void(const std::string& /*in_request_id*/,
                    int32_t /*in_status_code*/,
                    const std::vector<std::tuple<std::string, std::string>>& /*in_headers*/,
                    const std::vector<uint8_t>& /*in_data*/,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));
  MOCK_CONST_METHOD0(id, const std::string&());
  MOCK_CONST_METHOD0(name, const std::string&());
  MOCK_CONST_METHOD0(port, uint16_t());
  MOCK_CONST_METHOD0(protocol, const std::string&());
  MOCK_CONST_METHOD0(certificate_fingerprint, const std::vector<uint8_t>&());

 private:
  DISALLOW_COPY_AND_ASSIGN(ProtocolHandlerProxyMock);
};
}  // namespace WebServer
}  // namespace chromium
}  // namespace org

namespace org {
namespace chromium {
namespace WebServer {

// Mock object for ServerProxyInterface.
class ServerProxyMock : public ServerProxyInterface {
 public:
  ServerProxyMock() = default;

  MOCK_METHOD3(Ping,
               bool(std::string* /*out_message*/,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD3(PingAsync,
               void(const base::Callback<void(const std::string& /*message*/)>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));

 private:
  DISALLOW_COPY_AND_ASSIGN(ServerProxyMock);
};
}  // namespace WebServer
}  // namespace chromium
}  // namespace org

#endif  // ____CHROMEOS_DBUS_BINDING___BUILD_LUMPY_VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_WEBSERVD_DBUS_MOCKS_H
