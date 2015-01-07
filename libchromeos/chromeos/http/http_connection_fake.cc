// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/http/http_connection_fake.h>

#include <base/logging.h>
#include <chromeos/http/http_request.h>
#include <chromeos/mime_utils.h>
#include <chromeos/strings/string_utils.h>

namespace chromeos {
namespace http {
namespace fake {

Connection::Connection(const std::string& url, const std::string& method,
                       std::shared_ptr<http::Transport> transport) :
    http::Connection(transport), request_(url, method) {
  VLOG(1) << "fake::Connection created: " << method;
}

Connection::~Connection() {
  VLOG(1) << "fake::Connection destroyed";
}

bool Connection::SendHeaders(const HeaderList& headers,
                             chromeos::ErrorPtr* error) {
  request_.AddHeaders(headers);
  return true;
}

bool Connection::SetRequestData(
    std::unique_ptr<DataReaderInterface> data_reader,
    chromeos::ErrorPtr* error) {
  request_.SetData(std::move(data_reader));
  return true;
}

bool Connection::FinishRequest(chromeos::ErrorPtr* error) {
  using chromeos::string_utils::ToString;
  request_.AddHeaders({{request_header::kContentLength,
                      ToString(request_.GetData().size())}});
  fake::Transport* transport = static_cast<fake::Transport*>(transport_.get());
  CHECK(transport) << "Expecting a fake transport";
  auto handler = transport->GetHandler(request_.GetURL(), request_.GetMethod());
  if (handler.is_null()) {
    LOG(ERROR) << "Received unexpected " << request_.GetMethod()
               << " request at " << request_.GetURL();
    response_.ReplyText(status_code::NotFound,
                        "<html><body>Not found</body></html>",
                        chromeos::mime::text::kHtml);
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

bool Connection::ReadResponseData(void* data,
                                  size_t buffer_size,
                                  size_t* size_read,
                                  chromeos::ErrorPtr* error) {
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

}  // namespace fake
}  // namespace http
}  // namespace chromeos
