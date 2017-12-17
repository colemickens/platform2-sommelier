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

#include <libwebserv/dbus_response.h>

#include <utility>

#include <base/json/json_writer.h>
#include <base/logging.h>
#include <base/values.h>
#include <brillo/http/http_request.h>
#include <brillo/mime_utils.h>
#include <brillo/streams/memory_stream.h>
#include <brillo/strings/string_utils.h>
#include <libwebserv/dbus_protocol_handler.h>

namespace libwebserv {

DBusResponse::DBusResponse(DBusProtocolHandler* handler,
                           const std::string& request_id)
    : handler_{handler}, request_id_{request_id} {
}

DBusResponse::~DBusResponse() {
  if (!reply_sent_) {
    ReplyWithError(brillo::http::status_code::InternalServerError,
                   "Internal server error");
  }
}

void DBusResponse::AddHeaders(
    const std::vector<std::pair<std::string, std::string>>& headers) {
  headers_.insert(headers.begin(), headers.end());
}

void DBusResponse::Reply(int status_code,
                         brillo::StreamPtr data_stream,
                         const std::string& mime_type) {
  CHECK(data_stream);
  status_code_ = status_code;
  data_stream_ = std::move(data_stream);
  AddHeader(brillo::http::response_header::kContentType, mime_type);

  CHECK(!reply_sent_) << "Response already sent";
  reply_sent_ = true;
  handler_->CompleteRequest(request_id_, status_code_, headers_,
                            std::move(data_stream_));
}

}  // namespace libwebserv
