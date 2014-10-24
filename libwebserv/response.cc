// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libwebserv/response.h"

#include <algorithm>

#include <base/json/json_writer.h>
#include <base/logging.h>
#include <base/values.h>
#include <chromeos/http/http_request.h>
#include <chromeos/mime_utils.h>
#include <chromeos/strings/string_utils.h>
#include <microhttpd.h>

#include "libwebserv/connection.h"

namespace libwebserv {

// A helper wrapper to keep a reference to shared copy of Response inside
// libmicrohttpd connection. It must be a raw pointer here.
class ResponseHolder {
 public:
  explicit ResponseHolder(const std::shared_ptr<Response>& resp)
      : response(resp) {}
  std::shared_ptr<Response> response;
};

// Helper class to provide static callback methods to microhttpd library,
// with the ability to access private methods of Response class.
class ResponseHelper {
 public:
  static ssize_t ContentReaderCallback(void* cls,
                                       uint64_t pos,
                                       char* buf,
                                       size_t max) {
    Response* response = reinterpret_cast<ResponseHolder*>(cls)->response.get();
    uint64_t data_size = response->data_.size();
    if (pos > data_size)
      return MHD_CONTENT_READER_END_WITH_ERROR;

    if (pos == data_size)
      return MHD_CONTENT_READER_END_OF_STREAM;

    size_t max_data_size = static_cast<size_t>(data_size - pos);
    size_t size_read = std::min(max, max_data_size);
    memcpy(buf, response->data_.data() + pos, size_read);
    return size_read;
  }

  static void ContentReaderFreeCallback(void* cls) {
    delete reinterpret_cast<ResponseHolder*>(cls);
  }
};

Response::Response(const std::shared_ptr<Connection>& connection)
    : connection_(connection) {
}

Response::~Response() {
  if (!reply_sent_) {
    ReplyWithError(chromeos::http::status_code::InternalServerError,
                   "Internal server error");
  }
}

std::unique_ptr<Response> Response::Create(
    const std::shared_ptr<Connection>& connection) {
  std::unique_ptr<Response> response(new Response(connection));
  return response;
}

void Response::AddHeader(const std::string& header_name,
                         const std::string& value) {
  headers_.emplace(header_name, value);
}

void Response::AddHeaders(
    const std::vector<std::pair<std::string, std::string>>& headers) {
  headers_.insert(headers.begin(), headers.end());
}

void Response::Reply(int status_code,
                     const void* data,
                     size_t data_size,
                     const char* mime_type) {
  status_code_ = status_code;
  const uint8_t* byte_ptr = static_cast<const uint8_t*>(data);
  data_.assign(byte_ptr, byte_ptr + data_size);
  std::string data_mime_type =
      mime_type ? mime_type : chromeos::mime::application::kOctet_stream;
  AddHeader(chromeos::http::response_header::kContentType, data_mime_type);
  SendResponse();
}

void Response::ReplyWithText(int status_code,
                             const std::string& text,
                             const char* mime_type) {
  Reply(status_code, text.data(), text.size(),
        mime_type ? mime_type : chromeos::mime::text::kPlain);
}

void Response::ReplyWithJson(int status_code, const base::Value* json) {
  std::string text;
  base::JSONWriter::WriteWithOptions(json,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &text);
  std::string mime_type = chromeos::mime::AppendParameter(
      chromeos::mime::application::kJson,
      chromeos::mime::parameters::kCharset,
      "utf-8");
  ReplyWithText(status_code, text, mime_type.c_str());
}

void Response::ReplyWithJson(int status_code,
                             const std::map<std::string, std::string>& json) {
  base::DictionaryValue json_value;
  for (const auto& pair : json) {
    json_value.SetString(pair.first, pair.second);
  }
  ReplyWithJson(status_code, &json_value);
}

void Response::Redirect(int status_code, const std::string& redirect_url) {
  AddHeader(chromeos::http::response_header::kLocation, redirect_url);
  ReplyWithError(status_code, "");
}

void Response::ReplyWithError(int status_code, const std::string& error_text) {
  status_code_ = status_code;
  const char* ptr = error_text.data();
  data_.assign(ptr, ptr + error_text.size());
  SendResponse();
}

void Response::ReplyWithErrorNotFound() {
  ReplyWithError(chromeos::http::status_code::NotFound, "Not Found");
}

void Response::SendResponse() {
  CHECK(!reply_sent_) << "Response already sent";
  VLOG(1) << "Sending HTTP response for connection (" << connection_.get()
          << "): " << status_code_ << ", data size = " << data_.size();
  uint64_t size = data_.size();
  MHD_Response* resp = MHD_create_response_from_callback(
      size, 1024, &ResponseHelper::ContentReaderCallback,
      new ResponseHolder(shared_from_this()),
      &ResponseHelper::ContentReaderFreeCallback);
  for (const auto& pair : headers_) {
    MHD_add_response_header(resp, pair.first.c_str(), pair.second.c_str());
  }
  MHD_queue_response(connection_->raw_connection_, status_code_, resp);
  MHD_destroy_response(resp);  // |resp| is ref-counted.
  reply_sent_ = true;
}

}  // namespace libwebserv
