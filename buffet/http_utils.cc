// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/http_utils.h"

#include <algorithm>
#include <string.h>
#include <base/values.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>

#include "mime_utils.h"

namespace chromeos {
namespace http {

std::unique_ptr<Response> Get(char const* url) {
  Request request(url);
  return request.GetResponse();
}

std::string GetAsString(char const* url) {
  auto resp = Get(url);
  return resp ? resp->GetDataAsString() : std::string();
}

std::unique_ptr<Response> Head(char const* url) {
  Request request(url, request_type::kHead);
  return request.GetResponse();
}

std::unique_ptr<Response> PostText(char const* url,
                                   char const* data,
                                   char const* mime_type) {
  if (mime_type == nullptr) {
    mime_type = chromeos::mime::application::kWwwFormUrlEncoded;
  }

  return PostBinary(url, data, strlen(data), mime_type);
}

std::unique_ptr<Response> PostBinary(char const* url, void const* data,
                                     size_t data_size, char const* mime_type) {
  if (mime_type == nullptr) {
    mime_type = chromeos::mime::application::kOctet_stream;
  }

  Request request(url, request_type::kPost);
  request.SetContentType(mime_type);
  request.AddRequestBody(data, data_size);
  return request.GetResponse();
}

std::unique_ptr<Response> PostJson(char const* url, base::Value const* json) {
  std::string data;
  base::JSONWriter::Write(json, &data);
  return PostBinary(url, data.c_str(), data.size(), mime::application::kJson);
}

std::unique_ptr<base::Value> ParseJsonResponse(Response const* response,
                                               std::string* error_message) {
  std::unique_ptr<base::Value> value;
  if (response) {
    if (response->IsSuccessful()) {
      // Make sure we have a correct content type. Do not try to parse
      // binary files, or HTML output. Limit to application/json and text/plain.
      auto content_type = mime::RemoveParameters(response->GetContentType());
      if (content_type == mime::application::kJson ||
          content_type == mime::text::kPlain) {
        std::string json = response->GetDataAsString();
        value.reset(base::JSONReader::ReadAndReturnError(json,
                                                         base::JSON_PARSE_RFC,
                                                         nullptr,
                                                         error_message));
      }
      else if (error_message) {
        *error_message = "Unexpected response content type: " + content_type;
      }
    }
  } else if (error_message) {
    *error_message = "NULL response.";
  }
  return value;
}

} // namespace http
} // namespace chromeos
