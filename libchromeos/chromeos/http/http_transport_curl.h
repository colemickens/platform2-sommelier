// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_HTTP_HTTP_TRANSPORT_CURL_H_
#define LIBCHROMEOS_CHROMEOS_HTTP_HTTP_TRANSPORT_CURL_H_

#include <string>

#include <base/memory/ref_counted.h>
#include <base/task_runner.h>
#include <chromeos/chromeos_export.h>
#include <chromeos/http/curl_api.h>
#include <chromeos/http/http_transport.h>

namespace chromeos {
namespace http {
namespace curl {

///////////////////////////////////////////////////////////////////////////////
// An implementation of http::Transport that uses libcurl for
// HTTP communications. This class (as http::Transport base)
// is used by http::Request and http::Response classes to provide HTTP
// functionality to the clients.
// See http_transport.h for more details.
///////////////////////////////////////////////////////////////////////////////
class CHROMEOS_EXPORT Transport : public http::Transport {
 public:
  // Constructs the transport using the current message loop for async
  // operations.
  explicit Transport(const std::shared_ptr<CurlInterface>& curl_interface);
  // Constructs the transport with a custom task runner for async operations.
  Transport(const std::shared_ptr<CurlInterface>& curl_interface,
            scoped_refptr<base::TaskRunner> task_runner);
  // Creates a transport object using a proxy.
  // |proxy| is of the form [protocol://][user:password@]host[:port].
  // If not defined, protocol is assumed to be http://.
  Transport(const std::shared_ptr<CurlInterface>& curl_interface,
            const std::string& proxy);
  virtual ~Transport();

  // Overrides from http::Transport.
  std::shared_ptr<http::Connection> CreateConnection(
      const std::string& url,
      const std::string& method,
      const HeaderList& headers,
      const std::string& user_agent,
      const std::string& referer,
      chromeos::ErrorPtr* error) override;

  void RunCallbackAsync(const tracked_objects::Location& from_here,
                        const base::Closure& callback) override;

  void StartAsyncTransfer(http::Connection* connection,
                          const SuccessCallback& success_callback,
                          const ErrorCallback& error_callback) override;

  static void AddCurlError(chromeos::ErrorPtr* error,
                           const tracked_objects::Location& location,
                           CURLcode code,
                           CurlInterface* curl_interface);

 private:
  std::shared_ptr<CurlInterface> curl_interface_;
  std::string proxy_;
  scoped_refptr<base::TaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Transport);
};

}  // namespace curl
}  // namespace http
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_HTTP_HTTP_TRANSPORT_CURL_H_
