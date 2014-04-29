// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/http_connection_fake.h"

#include <base/logging.h>

#include "buffet/http_request.h"
#include "buffet/mime_utils.h"
#include "buffet/string_utils.h"

using namespace chromeos;
using namespace chromeos::http::fake;

Connection::Connection(const std::string& url, const std::string& method,
                       std::shared_ptr<http::Transport> transport) :
    http::Connection(transport), request_(url, method) {
  VLOG(1) << "fake::Connection created: " << method;
}

Connection::~Connection() {
  VLOG(1) << "fake::Connection destroyed";
}

bool Connection::SendHeaders(const HeaderList& headers) {
  request_.AddHeaders(headers);
  return true;
}

bool Connection::WriteRequestData(const void* data, size_t size) {
  request_.AddData(data, size);
  return true;
}

bool Connection::FinishRequest() {
  request_.AddHeaders({{request_header::kContentLength,
                      std::to_string(request_.GetData().size())}});
  fake::Transport* transport = dynamic_cast<fake::Transport*>(transport_.get());
  CHECK(transport) << "Expecting a fake transport";
  auto handler = transport->GetHandler(request_.GetURL(), request_.GetMethod());
  if (handler.is_null()) {
    LOG(ERROR) << "Received unexpected " << request_.GetMethod()
               << " request at " << request_.GetURL();
    response_.ReplyText(status_code::NotFound,
                        "<html><body>Not found</body></html>",
                        mime::text::kHtml);
  } else {
    handler.Run(request_, &response_);
  }
  return true;
}

int Connection::GetResponseStatusCode() const {
  return response_.GetStatusCode();
}

std::string Connection::GetResponseStatusText() const {
  return response_.GetStatusText();
}

std::string Connection::GetProtocolVersion() const {
  return response_.GetProtocolVersion();
}

std::string Connection::GetResponseHeader(
    const std::string& header_name) const {
  return response_.GetHeader(header_name);
}

uint64_t Connection::GetResponseDataSize() const {
  // HEAD requests must not return body.
  return (request_.GetMethod() != request_type::kHead) ?
      response_.GetData().size() : 0;
}

bool Connection::ReadResponseData(void* data, size_t buffer_size,
                                  size_t* size_read) {
  size_t size_to_read = GetResponseDataSize() - response_data_ptr_;
  if (size_to_read > buffer_size)
    size_to_read = buffer_size;
  if (size_to_read > 0)
    memcpy(data, response_.GetData().data() + response_data_ptr_, size_to_read);
  if (size_read)
    *size_read = size_to_read;
  response_data_ptr_ += size_to_read;
  return true;
}

std::string Connection::GetErrorMessage() const {
  return std::string();
}
