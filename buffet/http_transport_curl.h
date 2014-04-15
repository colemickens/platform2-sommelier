// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_HTTP_TRANSPORT_CURL_H_
#define BUFFET_HTTP_TRANSPORT_CURL_H_

#include <map>
#include <curl/curl.h>

#include "buffet/http_request.h"

namespace chromeos {
namespace http {
namespace curl {

///////////////////////////////////////////////////////////////////////////////
// A particular implementation of TransportInterface that uses libcurl for
// HTTP communications. This class (as TransportInterface interface)
// is used by http::Request and http::Response classes to provide HTTP
// functionality to the clients.
///////////////////////////////////////////////////////////////////////////////
class Transport : public TransportInterface {
 public:
  // Standard constructor. |url| is the full request URL with protocol
  // schema, host address, resource path as well as optional query parameters
  // and/or user name/password. |method| is one of HTTP request verbs such as
  // "GET", "POST", etc. If nullptr is specified, "GET" is assumed.
  Transport(std::string const& url, char const* method);
  ~Transport();

  // Returns the current request/response stage.
  virtual Stage GetStage() const override { return stage_; }

  // Implementation of Request::AddRange.
  virtual void AddRange(int64_t bytes) override;
  virtual void AddRange(uint64_t from_byte, uint64_t to_byte) override;

  // Implementation of Request::SetAccept/Request::GetAccept.
  virtual void SetAccept(char const* acceptMimeTypes) override {
    accept_ = acceptMimeTypes;
  }
  virtual std::string GetAccept() const override;

  // Implementation of Request::GetRequestURL.
  virtual std::string GetRequestURL() const override { return request_url_; }

  // Implementation of Request::SetContentType/Request::GetContentType.
  virtual void SetContentType(char const* content_type) override {
    content_type_ = content_type;
  }
  virtual std::string GetContentType() const override { return content_type_; }

  // Implementation of Request::AddHeader.
  virtual void AddHeader(char const* header, char const* value) override;

  // Implementation of Request::RemoveHeader.
  virtual void RemoveHeader(char const* header) override;

  // Implementation of Request::AddRequestBody.
  virtual bool AddRequestBody(void const* data, size_t size) override;

  // Implementation of Request::SetMethod/Request::GetMethod.
  virtual void SetMethod(char const* method) override { method_ = method; }
  virtual std::string GetMethod() const override { return method_; }

  // Implementation of Request::SetReferer/Request::GetReferer.
  virtual void SetReferer(char const* referer) override { referer_ = referer; }
  virtual std::string GetReferer() const override { return referer_; }

  // Implementation of Request::SetUserAgent/Request::GetUserAgent.
  virtual void SetUserAgent(char const* user_agent) override {
    user_agent_ = user_agent;
  }
  virtual std::string GetUserAgent() const override { return user_agent_; }

  // Sends the HTTP request to the server. Used by Request::GetResponse().
  virtual bool Perform() override;

  // Implementation of Response::GetStatusCode.
  virtual int GetResponseStatusCode() const override;

  // Implementation of Response::GetStatusText.
  virtual std::string GetResponseStatusText() const override {
    return status_text_;
  }

  // Implementation of Response::GetHeader.
  virtual std::string GetResponseHeader(char const* header_name) const override;

  // Implementation of Response::GetData.
  virtual std::vector<unsigned char> const& GetResponseData() const override;

  // Implementation of Response::GetErrorMessage.
  virtual std::string GetErrorMessage() const override { return error_; }

  // Closes the connection and frees up internal data
  virtual void Close() override;

 private:
   HeaderList GetHeaders() const;

  // Write data callback. Used by CURL when receiving response data.
  static size_t write_callback(char* ptr, size_t size, size_t num, void* data);
  // Read data callback. Used by CURL when sending request body data.
  static size_t read_callback(char* ptr, size_t size, size_t num, void* data);
  // Write header data callback. Used by CURL when receiving response headers.
  static size_t header_callback(char* ptr, size_t size, size_t num, void* data);

  // Full request URL, such as "http://www.host.com/path/to/object"
  std::string request_url_;
  // HTTP request verb, such as "GET", "POST", "PUT", ...
  std::string method_;

  // Referrer URL, if any. Sent to the server via "Referer: " header.
  std::string referer_;
  // User agent string, if any. Sent to the server via "User-Agent: " header.
  std::string user_agent_;
  // Content type of the request body data.
  // Sent to the server via "Content-Type: " header.
  std::string content_type_;
  // List of acceptable response data types.
  // Sent to the server via "Accept: " header.
  std::string accept_ = "*/*";

  // List of optional request headers provided by the caller.
  // After request has been sent, contains the received response headers.
  std::map<std::string, std::string> headers_;
  // List of optional data ranges to request partial content from the server.
  // Sent to thr server as "Range: " header.
  std::vector<std::pair<uint64_t, uint64_t>> ranges_;
  // Binary data for request body.
  std::vector<unsigned char> request_data_;
  // Read pointer for request data. Used when streaming data to the server.
  size_t request_data_ptr_ = 0;

  // Received response data.
  std::vector<unsigned char> response_data_;

  // Current progress stage.
  Stage stage_ = Stage::failed;
  // CURL error message in case request fails completely.
  std::string error_;
  // Reponse status text, such as "OK" for 200, or "Forbidden" for 403
  std::string status_text_;
  // Flag used when parsing response headers to separate the response status
  // from the rest of response headers.
  bool status_text_set_ = false;

  // range_value_omitted is used in |ranges_| list to indicate omitted value.
  // E.g. range (10,range_value_omitted) represents bytes from 10 to the end
  // of the data stream.
  static const uint64_t range_value_omitted = (uint64_t)-1;

  CURL* curl_handle_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Transport);
};

} // namespace curl
} // namespace http
} // namespace chromeos

#endif // BUFFET_HTTP_TRANSPORT_CURL_H_
