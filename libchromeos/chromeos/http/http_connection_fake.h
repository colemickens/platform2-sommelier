// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_HTTP_HTTP_CONNECTION_FAKE_H_
#define LIBCHROMEOS_CHROMEOS_HTTP_HTTP_CONNECTION_FAKE_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>
#include <chromeos/http/http_connection.h>
#include <chromeos/http/http_transport_fake.h>

namespace chromeos {
namespace http {
namespace fake {

// This is a fake implementation of http::Connection for unit testing.
class Connection : public http::Connection {
 public:
  Connection(const std::string& url, const std::string& method,
             const std::shared_ptr<http::Transport>& transport);
  virtual ~Connection();

  // Overrides from http::Connection.
  // See http_connection.h for description of these methods.
  bool SendHeaders(const HeaderList& headers,
                   chromeos::ErrorPtr* error) override;
  bool SetRequestData(std::unique_ptr<DataReaderInterface> data_reader,
                      chromeos::ErrorPtr* error) override;
  bool FinishRequest(chromeos::ErrorPtr* error) override;
  void FinishRequestAsync(const SuccessCallback& success_callback,
                          const ErrorCallback& error_callback) override;

  int GetResponseStatusCode() const override;
  std::string GetResponseStatusText() const override;
  std::string GetProtocolVersion() const override;
  std::string GetResponseHeader(const std::string& header_name) const override;
  uint64_t GetResponseDataSize() const override;
  bool ReadResponseData(void* data, size_t buffer_size,
                        size_t* size_read, chromeos::ErrorPtr* error) override;

 private:
  // A helper method for FinishRequestAsync() implementation.
  void FinishRequestAsyncHelper(const SuccessCallback& success_callback,
                                const ErrorCallback& error_callback);

  // Request and response objects passed to the user-provided request handler
  // callback. The request object contains all the request information.
  // The response object is the server response that is created by
  // the handler in response to the request.
  ServerRequest request_;
  ServerResponse response_;

  // Internal read data pointer needed for ReadResponseData() implementation.
  size_t response_data_ptr_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Connection);
};

}  // namespace fake
}  // namespace http
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_HTTP_HTTP_CONNECTION_FAKE_H_
