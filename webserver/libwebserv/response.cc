// Copyright 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <libwebserv/response.h>

#include <base/json/json_writer.h>
#include <base/values.h>
#include <brillo/http/http_request.h>
#include <brillo/mime_utils.h>
#include <brillo/streams/memory_stream.h>

namespace libwebserv {

void Response::AddHeader(const std::string& header_name,
                         const std::string& value) {
  AddHeaders({std::pair<std::string, std::string>{header_name, value}});
}

void Response::ReplyWithText(int status_code,
                             const std::string& text,
                             const std::string& mime_type) {
  Reply(status_code, brillo::MemoryStream::OpenCopyOf(text, nullptr),
        mime_type);
}

void Response::ReplyWithJson(int status_code, const base::Value* json) {
  std::string text;
  base::JSONWriter::WriteWithOptions(
      *json, base::JSONWriter::OPTIONS_PRETTY_PRINT, &text);
  std::string mime_type = brillo::mime::AppendParameter(
      brillo::mime::application::kJson,
      brillo::mime::parameters::kCharset,
      "utf-8");
  ReplyWithText(status_code, text, mime_type);
}

void Response::ReplyWithJson(
    int status_code, const std::map<std::string, std::string>& json) {
  base::DictionaryValue json_value;
  for (const auto& pair : json) {
    json_value.SetString(pair.first, pair.second);
  }
  ReplyWithJson(status_code, &json_value);
}

void Response::Redirect(int status_code, const std::string& redirect_url) {
  AddHeader(brillo::http::response_header::kLocation, redirect_url);
  ReplyWithError(status_code, "");
}

void Response::ReplyWithError(int status_code,
                              const std::string& error_text) {
  Reply(status_code, brillo::MemoryStream::OpenCopyOf(error_text, nullptr),
        "text/plain");
}

void Response::ReplyWithErrorNotFound() {
  ReplyWithError(brillo::http::status_code::NotFound, "Not Found");
}

}  // namespace libwebserv
