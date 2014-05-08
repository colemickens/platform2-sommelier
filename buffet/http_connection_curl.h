// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_HTTP_CONNECTION_CURL_H_
#define BUFFET_HTTP_CONNECTION_CURL_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <curl/curl.h>

#include "buffet/http_connection.h"

namespace chromeos {
namespace http {
namespace curl {

// This is a libcurl-based implementation of http::Connection.
class Connection : public chromeos::http::Connection {
 public:
  Connection(CURL* curl_handle, const std::string& method,
             std::shared_ptr<http::Transport> transport);
  virtual ~Connection();

  // Overrides from http::Connection.
  // See http_connection.h for description of these methods.
  virtual bool SendHeaders(const HeaderList& headers, ErrorPtr* error) override;
  virtual bool WriteRequestData(const void* data, size_t size,
                                ErrorPtr* error) override;
  virtual bool FinishRequest(ErrorPtr* error) override;

  virtual int GetResponseStatusCode() const override;
  virtual std::string GetResponseStatusText() const override;
  virtual std::string GetProtocolVersion() const override;
  virtual std::string GetResponseHeader(
     const std::string& header_name) const override;
  virtual uint64_t GetResponseDataSize() const override;
  virtual bool ReadResponseData(void* data, size_t buffer_size,
                                size_t* size_read, ErrorPtr* error) override;

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

#endif  // BUFFET_HTTP_CONNECTION_CURL_H_
