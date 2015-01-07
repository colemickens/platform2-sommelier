// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/http/http_transport_fake.h>

#include <utility>

#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/logging.h>
#include <chromeos/bind_lambda.h>
#include <chromeos/http/http_connection_fake.h>
#include <chromeos/http/http_request.h>
#include <chromeos/mime_utils.h>
#include <chromeos/strings/string_utils.h>
#include <chromeos/url_utils.h>

namespace chromeos {

using http::fake::Transport;
using http::fake::ServerRequestResponseBase;
using http::fake::ServerRequest;
using http::fake::ServerResponse;

Transport::Transport() {
  VLOG(1) << "fake::Transport created";
}

Transport::~Transport() {
  VLOG(1) << "fake::Transport destroyed";
}

std::unique_ptr<http::Connection> Transport::CreateConnection(
    std::shared_ptr<http::Transport> transport,
    const std::string& url,
    const std::string& method,
    const HeaderList& headers,
    const std::string& user_agent,
    const std::string& referer,
    chromeos::ErrorPtr* error) {
  HeaderList headers_copy = headers;
  if (!user_agent.empty()) {
    headers_copy.push_back(std::make_pair(http::request_header::kUserAgent,
                                          user_agent));
  }
  if (!referer.empty()) {
    headers_copy.push_back(std::make_pair(http::request_header::kReferer,
                                          referer));
  }
  std::unique_ptr<http::Connection> connection(
      new http::fake::Connection(url, method, transport));
  CHECK(connection) << "Unable to create Connection object";
  if (!connection->SendHeaders(headers_copy, error))
    connection.reset();
  request_count_++;
  return connection;
}

static inline std::string GetHandlerMapKey(const std::string& url,
                                           const std::string& method) {
  return method + ":" + url;
}

void Transport::AddHandler(const std::string& url, const std::string& method,
                           const HandlerCallback& handler) {
  handlers_.insert(std::make_pair(GetHandlerMapKey(url, method), handler));
}

void Transport::AddSimpleReplyHandler(const std::string& url,
                                      const std::string& method,
                                      int status_code,
                                      const std::string& reply_text,
                                      const std::string& mime_type) {
  auto handler = [status_code, reply_text, mime_type](
      const ServerRequest& request, ServerResponse* response) {
    response->ReplyText(status_code, reply_text, mime_type);
  };
  AddHandler(url, method, base::Bind(handler));
}

Transport::HandlerCallback Transport::GetHandler(
    const std::string& url, const std::string& method) const {
  // First try the exact combination of URL/Method
  auto p = handlers_.find(GetHandlerMapKey(url, method));
  if (p != handlers_.end())
    return p->second;
  // If not found, try URL/*
  p = handlers_.find(GetHandlerMapKey(url, "*"));
  if (p != handlers_.end())
    return p->second;
  // If still not found, try */method
  p = handlers_.find(GetHandlerMapKey("*", method));
  if (p != handlers_.end())
    return p->second;
  // Finally, try */*
  p = handlers_.find(GetHandlerMapKey("*", "*"));
  return (p != handlers_.end()) ? p->second : HandlerCallback();
}

void ServerRequestResponseBase::SetData(
    std::unique_ptr<DataReaderInterface> data_reader) {
  data_.clear();
  if (data_reader) {
    uint8_t buffer[1024];
    size_t size = 0;
    data_.reserve(data_reader->GetDataSize());
    do {
      CHECK(data_reader->ReadData(buffer, sizeof(buffer), &size, nullptr));
      data_.insert(data_.end(), buffer, buffer + size);
    } while (size > 0);
  }
}

std::string ServerRequestResponseBase::GetDataAsString() const {
  if (data_.empty())
    return std::string();
  auto chars = reinterpret_cast<const char*>(data_.data());
  return std::string(chars, data_.size());
}

std::unique_ptr<base::DictionaryValue>
    ServerRequestResponseBase::GetDataAsJson() const {
  if (chromeos::mime::RemoveParameters(
          GetHeader(request_header::kContentType)) ==
      chromeos::mime::application::kJson) {
    auto value = base::JSONReader::Read(GetDataAsString());
    if (value) {
      base::DictionaryValue* dict = nullptr;
      if (value->GetAsDictionary(&dict)) {
        return std::unique_ptr<base::DictionaryValue>(dict);
      } else {
        delete value;
      }
    }
  }
  return std::unique_ptr<base::DictionaryValue>();
}

void ServerRequestResponseBase::AddHeaders(const HeaderList& headers) {
  for (const auto& pair : headers) {
    if (pair.second.empty())
      headers_.erase(pair.first);
    else
      headers_.insert(pair);
  }
}

std::string ServerRequestResponseBase::GetHeader(
    const std::string& header_name) const {
  auto p = headers_.find(header_name);
  return p != headers_.end() ? p->second : std::string();
}

ServerRequest::ServerRequest(const std::string& url,
                             const std::string& method) : method_(method) {
  auto params = chromeos::url::GetQueryStringParameters(url);
  url_ = chromeos::url::RemoveQueryString(url, true);
  form_fields_.insert(params.begin(), params.end());
}

std::string ServerRequest::GetFormField(const std::string& field_name) const {
  if (!form_fields_parsed_) {
    std::string mime_type = chromeos::mime::RemoveParameters(
        GetHeader(request_header::kContentType));
    if (mime_type == chromeos::mime::application::kWwwFormUrlEncoded &&
        !GetData().empty()) {
      auto fields = chromeos::data_encoding::WebParamsDecode(GetDataAsString());
      form_fields_.insert(fields.begin(), fields.end());
    }
    form_fields_parsed_ = true;
  }
  auto p = form_fields_.find(field_name);
  return p != form_fields_.end() ? p->second : std::string();
}

void ServerResponse::Reply(int status_code,
                           const void* data,
                           size_t data_size,
                           const std::string& mime_type) {
  data_.clear();
  status_code_ = status_code;
  SetData(std::unique_ptr<DataReaderInterface>{
      new MemoryDataReader{data, data_size}});
  AddHeaders({
    {response_header::kContentLength,
        chromeos::string_utils::ToString(data_size)},
    {response_header::kContentType, mime_type}
  });
}

void ServerResponse::ReplyText(int status_code,
                               const std::string& text,
                               const std::string& mime_type) {
  Reply(status_code, text.data(), text.size(), mime_type);
}

void ServerResponse::ReplyJson(int status_code, const base::Value* json) {
  std::string text;
  base::JSONWriter::WriteWithOptions(json,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &text);
  std::string mime_type = chromeos::mime::AppendParameter(
      chromeos::mime::application::kJson,
      chromeos::mime::parameters::kCharset,
      "utf-8");
  ReplyText(status_code, text, mime_type);
}

void ServerResponse::ReplyJson(int status_code,
                               const http::FormFieldList& fields) {
  base::DictionaryValue json;
  for (const auto& pair : fields) {
    json.SetString(pair.first, pair.second);
  }
  ReplyJson(status_code, &json);
}

std::string ServerResponse::GetStatusText() const {
  static std::vector<std::pair<int, const char*>> status_text_map = {
    {100, "Continue"},
    {101, "Switching Protocols"},
    {102, "Processing"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {207, "Multi-Status"},
    {208, "Already Reported"},
    {226, "IM Used"},
    {300, "Multiple Choices"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {306, "Switch Proxy"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Request Entity Too Large"},
    {414, "Request - URI Too Long"},
    {415, "Unsupported Media Type"},
    {429, "Too Many Requests"},
    {431, "Request Header Fields Too Large"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
  };

  for (const auto& pair : status_text_map) {
    if (pair.first == status_code_)
      return pair.second;
  }
  return std::string();
}

}  // namespace chromeos
