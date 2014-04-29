// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/http_utils.h"

#include <algorithm>
#include <string.h>
#include <base/values.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>

#include "buffet/mime_utils.h"
#include "buffet/data_encoding.h"

namespace chromeos {
namespace http {

std::unique_ptr<Response> Get(const std::string& url,
                              const HeaderList& headers,
                              std::shared_ptr<Transport> transport) {
  return SendRequest(request_type::kGet, url, nullptr, 0, nullptr,
                     headers, transport);
}

std::string GetAsString(const std::string& url,
                        const HeaderList& headers,
                        std::shared_ptr<Transport> transport) {
  auto resp = Get(url, headers, transport);
  return resp ? resp->GetDataAsString() : std::string();
}

std::unique_ptr<Response> Head(const std::string& url,
                               std::shared_ptr<Transport> transport) {
  Request request(url, request_type::kHead, transport);
  return request.GetResponse();
}

std::unique_ptr<Response> PostText(const std::string& url,
                                   const char* data,
                                   const char* mime_type,
                                   const HeaderList& headers,
                                   std::shared_ptr<Transport> transport) {
  if (mime_type == nullptr) {
    mime_type = chromeos::mime::application::kWwwFormUrlEncoded;
  }

  return PostBinary(url, data, strlen(data), mime_type, headers, transport);
}

std::unique_ptr<Response> SendRequest(const char * method,
                                      const std::string& url,
                                      const void* data,
                                      size_t data_size,
                                      const char* mime_type,
                                      const HeaderList& headers,
                                      std::shared_ptr<Transport> transport) {
  Request request(url, method, transport);
  request.AddHeaders(headers);
  if (data_size > 0) {
    if (mime_type == nullptr) {
      mime_type = chromeos::mime::application::kOctet_stream;
    }
    request.SetContentType(mime_type);
    request.AddRequestBody(data, data_size);
  }
  return request.GetResponse();
}

std::unique_ptr<Response> PostBinary(const std::string & url, const void* data,
                                     size_t data_size, const char* mime_type,
                                     const HeaderList& headers,
                                     std::shared_ptr<Transport> transport) {
  return SendRequest(request_type::kPost, url,
                     data, data_size, mime_type, headers, transport);
}

std::unique_ptr<Response> PostFormData(const std::string& url,
                                       const FormFieldList& data,
                                       const HeaderList& headers,
                                       std::shared_ptr<Transport> transport) {
  std::string encoded_data = chromeos::data_encoding::WebParamsEncode(data);
  return PostBinary(url, encoded_data.c_str(), encoded_data.size(),
                    chromeos::mime::application::kWwwFormUrlEncoded,
                    headers, transport);
}


std::unique_ptr<Response> PostJson(const std::string& url,
                                   const base::Value* json,
                                   const HeaderList& headers,
                                   std::shared_ptr<Transport> transport) {
  std::string data;
  if (json)
    base::JSONWriter::Write(json, &data);
  std::string mime_type = mime::AppendParameter(mime::application::kJson,
                                                mime::parameters::kCharset,
                                                "utf-8");
  return PostBinary(url, data.c_str(), data.size(),
                    mime_type.c_str(), headers, transport);
}

std::unique_ptr<Response> PatchJson(const std::string& url,
                                    const base::Value* json,
                                    const HeaderList& headers,
                                    std::shared_ptr<Transport> transport) {
  std::string data;
  if (json)
    base::JSONWriter::Write(json, &data);
  std::string mime_type = mime::AppendParameter(mime::application::kJson,
                                                mime::parameters::kCharset,
                                                "utf-8");
  return SendRequest(request_type::kPatch, url, data.c_str(), data.size(),
                     mime_type.c_str(), headers, transport);
}

std::unique_ptr<base::DictionaryValue> ParseJsonResponse(
    const Response* response, int* status_code, std::string* error_message) {
  std::unique_ptr<base::DictionaryValue> dict;
  if (response) {
    if (status_code)
      *status_code = response->GetStatusCode();

    // Make sure we have a correct content type. Do not try to parse
    // binary files, or HTML output. Limit to application/json and text/plain.
    auto content_type = mime::RemoveParameters(response->GetContentType());
    if (content_type == mime::application::kJson ||
        content_type == mime::text::kPlain) {
      std::string json = response->GetDataAsString();
      auto value = base::JSONReader::ReadAndReturnError(json,
                                                        base::JSON_PARSE_RFC,
                                                        nullptr,
                                                        error_message);
      if (value) {
        base::DictionaryValue* dict_value = nullptr;
        if (value->GetAsDictionary(&dict_value)) {
          dict.reset(dict_value);
        } else {
          delete value;
          if (error_message) {
            *error_message = "Reponse is not a valid JSON object";
          }
        }
      }
    }
    else if (error_message) {
      *error_message = "Unexpected response content type: " + content_type;
    }
  } else if (error_message) {
    *error_message = "NULL response.";
  }
  return dict;
}

} // namespace http
} // namespace chromeos
