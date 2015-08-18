// Automatic generation of D-Bus interface mock proxies for:
//  - org.chromium.WebServer.RequestHandler
#ifndef ____CHROMEOS_DBUS_BINDING___BUILD_LUMPY_VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_LIBWEBSERV_DBUS_MOCKS_H
#define ____CHROMEOS_DBUS_BINDING___BUILD_LUMPY_VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_LIBWEBSERV_DBUS_MOCKS_H
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

// Mock object for RequestHandlerProxyInterface.
class RequestHandlerProxyMock : public RequestHandlerProxyInterface {
 public:
  RequestHandlerProxyMock() = default;

  MOCK_METHOD7(ProcessRequest,
               bool(const std::tuple<std::string, std::string, std::string, std::string, std::string>& /*in_request_info*/,
                    const std::vector<std::tuple<std::string, std::string>>& /*in_headers*/,
                    const std::vector<std::tuple<bool, std::string, std::string>>& /*in_params*/,
                    const std::vector<std::tuple<int32_t, std::string, std::string, std::string, std::string>>& /*in_files*/,
                    const std::vector<uint8_t>& /*in_body*/,
                    chromeos::ErrorPtr* /*error*/,
                    int /*timeout_ms*/));
  MOCK_METHOD8(ProcessRequestAsync,
               void(const std::tuple<std::string, std::string, std::string, std::string, std::string>& /*in_request_info*/,
                    const std::vector<std::tuple<std::string, std::string>>& /*in_headers*/,
                    const std::vector<std::tuple<bool, std::string, std::string>>& /*in_params*/,
                    const std::vector<std::tuple<int32_t, std::string, std::string, std::string, std::string>>& /*in_files*/,
                    const std::vector<uint8_t>& /*in_body*/,
                    const base::Callback<void()>& /*success_callback*/,
                    const base::Callback<void(chromeos::Error*)>& /*error_callback*/,
                    int /*timeout_ms*/));

 private:
  DISALLOW_COPY_AND_ASSIGN(RequestHandlerProxyMock);
};
}  // namespace WebServer
}  // namespace chromium
}  // namespace org

#endif  // ____CHROMEOS_DBUS_BINDING___BUILD_LUMPY_VAR_CACHE_PORTAGE_CHROMEOS_BASE_WEBSERVER_OUT_DEFAULT_GEN_INCLUDE_LIBWEBSERV_DBUS_MOCKS_H
