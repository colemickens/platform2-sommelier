// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_HTTP_CONNECTION_CURL_H_
#define LIBCHROMEOS_CHROMEOS_HTTP_CONNECTION_CURL_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <chromeos/http_connection.h>
#include <curl/curl.h>

namespace chromeos {
namespace http {
namespace curl {

// This is a libcurl-based implementation of http::Connection.
class Connection : public http::Connection {
 public:
  Connection(CURL* curl_handle, const std::string& method,
             std::shared_ptr<http::Transport> transport);
  virtual ~Connection();

  // Overrides from http::Connection.
  // See http_connection.h for description of these methods.
  bool SendHeaders(const HeaderList& headers,
                   chromeos::ErrorPtr* error) override;
  bool WriteRequestData(const void* data, size_t size,
                        chromeos::ErrorPtr* error) override;
  bool FinishRequest(chromeos::ErrorPtr* error) override;

  int GetResponseStatusCode() const override;
  std::string GetResponseStatusText() const override;
  std::string GetProtocolVersion() const override;
  std::string GetResponseHeader(const std::string& header_name) const override;
  uint64_t GetResponseDataSize() const override;
  bool ReadResponseData(void* data, size_t buffer_size,
                        size_t* size_read, chromeos::ErrorPtr* error) override;

 protected:
  // Write data callback. Used by CURL when receiving response data.
  static size_t write_callback(char* ptr, size_t size, size_t num, void* data);
  // Read data callback. Used by CURL when sending request body data.
  static size_t read_callback(char* ptr, size_t size, size_t num, void* data);
  // Write header data callback. Used by CURL when receiving response headers.
  static size_t header_callback(char* ptr, size_t size, size_t num, void* data);

  // HTTP request verb, such as "GET", "POST", "PUT", ...
  std::string method_;

  // Binary data for request body.
  std::vector<unsigned char> request_data_;
  // Read pointer for request data. Used when streaming data to the server.
  size_t request_data_ptr_ = 0;

  // Received response data.
  std::vector<unsigned char> response_data_;
  size_t response_data_ptr_ = 0;

  // List of optional request headers provided by the caller.
  // After request has been sent, contains the received response headers.
  std::map<std::string, std::string> headers_;

  // HTTP protocol version, such as HTTP/1.1
  std::string protocol_version_;
  // Response status text, such as "OK" for 200, or "Forbidden" for 403
  std::string status_text_;
  // Flag used when parsing response headers to separate the response status
  // from the rest of response headers.
  bool status_text_set_ = false;

  CURL* curl_handle_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(Connection);
};

}  // namespace curl
}  // namespace http
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_HTTP_CONNECTION_CURL_H_
