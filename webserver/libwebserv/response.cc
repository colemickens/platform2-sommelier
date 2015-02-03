// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <libwebserv/response.h>

#include <algorithm>

#include <base/json/json_writer.h>
#include <base/logging.h>
#include <base/values.h>
#include <chromeos/http/http_request.h>
#include <chromeos/mime_utils.h>
#include <chromeos/strings/string_utils.h>
#include <libwebserv/protocol_handler.h>

namespace libwebserv {

Response::Response(ProtocolHandler* handler, const std::string& request_id)
    : handler_{handler}, request_id_{request_id} {
}

Response::~Response() {
  if (!reply_sent_) {
    ReplyWithError(chromeos::http::status_code::InternalServerError,
                   "Internal server error");
  }
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
                     const std::string& mime_type) {
  status_code_ = status_code;
  const uint8_t* byte_ptr = static_cast<const uint8_t*>(data);
  data_.assign(byte_ptr, byte_ptr + data_size);
  AddHeader(chromeos::http::response_header::kContentType, mime_type);
  SendResponse();
}

void Response::ReplyWithText(int status_code,
                             const std::string& text,
                             const std::string& mime_type) {
  Reply(status_code, text.data(), text.size(), mime_type);
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
  ReplyWithText(status_code, text, mime_type);
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
  data_.assign(error_text.begin(), error_text.end());
  SendResponse();
}

void Response::ReplyWithErrorNotFound() {
  ReplyWithError(chromeos::http::status_code::NotFound, "Not Found");
}

void Response::SendResponse() {
  CHECK(!reply_sent_) << "Response already sent";
  reply_sent_ = true;
  handler_->CompleteRequest(request_id_, status_code_, headers_, data_);
}

}  // namespace libwebserv
