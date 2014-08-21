// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/http_utils.h>

#include <algorithm>

#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/values.h>
#include <chromeos/data_encoding.h>
#include <chromeos/error_codes.h>
#include <chromeos/mime_utils.h>

using chromeos::mime::AppendParameter;
using chromeos::mime::RemoveParameters;

namespace chromeos {
namespace http {

std::unique_ptr<Response> Get(const std::string& url,
                              const HeaderList& headers,
                              std::shared_ptr<Transport> transport,
                              chromeos::ErrorPtr* error) {
  return SendRequest(request_type::kGet, url, nullptr, 0, nullptr,
                     headers, transport, error);
}

std::string GetAsString(const std::string& url,
                        const HeaderList& headers,
                        std::shared_ptr<Transport> transport,
                        chromeos::ErrorPtr* error) {
  auto resp = Get(url, headers, transport, error);
  return resp ? resp->GetDataAsString() : std::string();
}

std::unique_ptr<Response> Head(const std::string& url,
                               std::shared_ptr<Transport> transport,
                               chromeos::ErrorPtr* error) {
  Request request(url, request_type::kHead, transport);
  return request.GetResponse(error);
}

std::unique_ptr<Response> PostText(const std::string& url,
                                   const char* data,
                                   const char* mime_type,
                                   const HeaderList& headers,
                                   std::shared_ptr<Transport> transport,
                                   chromeos::ErrorPtr* error) {
  if (mime_type == nullptr) {
    mime_type = chromeos::mime::application::kWwwFormUrlEncoded;
  }

  return PostBinary(url, data, strlen(data), mime_type, headers, transport,
                    error);
}

std::unique_ptr<Response> SendRequest(const char * method,
                                      const std::string& url,
                                      const void* data,
                                      size_t data_size,
                                      const char* mime_type,
                                      const HeaderList& headers,
                                      std::shared_ptr<Transport> transport,
                                      chromeos::ErrorPtr* error) {
  Request request(url, method, transport);
  request.AddHeaders(headers);
  if (data_size > 0) {
    if (mime_type == nullptr) {
      mime_type = chromeos::mime::application::kOctet_stream;
    }
    request.SetContentType(mime_type);
    if (!request.AddRequestBody(data, data_size, error))
      return std::unique_ptr<Response>();
  }
  return request.GetResponse(error);
}

std::unique_ptr<Response> PostBinary(const std::string & url, const void* data,
                                     size_t data_size, const char* mime_type,
                                     const HeaderList& headers,
                                     std::shared_ptr<Transport> transport,
                                     chromeos::ErrorPtr* error) {
  return SendRequest(request_type::kPost, url,
                     data, data_size, mime_type, headers, transport, error);
}

std::unique_ptr<Response> PostFormData(const std::string& url,
                                       const FormFieldList& data,
                                       const HeaderList& headers,
                                       std::shared_ptr<Transport> transport,
                                       chromeos::ErrorPtr* error) {
  std::string encoded_data = chromeos::data_encoding::WebParamsEncode(data);
  return PostBinary(url, encoded_data.c_str(), encoded_data.size(),
                    chromeos::mime::application::kWwwFormUrlEncoded,
                    headers, transport, error);
}


std::unique_ptr<Response> PostJson(const std::string& url,
                                   const base::Value* json,
                                   const HeaderList& headers,
                                   std::shared_ptr<Transport> transport,
                                   chromeos::ErrorPtr* error) {
  std::string data;
  if (json)
    base::JSONWriter::Write(json, &data);
  std::string mime_type = AppendParameter(chromeos::mime::application::kJson,
                                          chromeos::mime::parameters::kCharset,
                                          "utf-8");
  return PostBinary(url, data.c_str(), data.size(),
                    mime_type.c_str(), headers, transport, error);
}

std::unique_ptr<Response> PatchJson(const std::string& url,
                                    const base::Value* json,
                                    const HeaderList& headers,
                                    std::shared_ptr<Transport> transport,
                                    chromeos::ErrorPtr* error) {
  std::string data;
  if (json)
    base::JSONWriter::Write(json, &data);
  std::string mime_type = AppendParameter(chromeos::mime::application::kJson,
                                          chromeos::mime::parameters::kCharset,
                                          "utf-8");
  return SendRequest(request_type::kPatch, url, data.c_str(), data.size(),
                     mime_type.c_str(), headers, transport, error);
}

std::unique_ptr<base::DictionaryValue> ParseJsonResponse(
    const Response* response, int* status_code, chromeos::ErrorPtr* error) {
  if (!response)
    return std::unique_ptr<base::DictionaryValue>();

  if (status_code)
    *status_code = response->GetStatusCode();

  // Make sure we have a correct content type. Do not try to parse
  // binary files, or HTML output. Limit to application/json and text/plain.
  auto content_type = RemoveParameters(response->GetContentType());
  if (content_type != chromeos::mime::application::kJson &&
      content_type != chromeos::mime::text::kPlain) {
    chromeos::Error::AddTo(error, chromeos::errors::json::kDomain,
                           "non_json_content_type",
                           "Unexpected response content type: " + content_type);
    return std::unique_ptr<base::DictionaryValue>();
  }

  std::string json = response->GetDataAsString();
  std::string error_message;
  base::Value* value = base::JSONReader::ReadAndReturnError(
      json, base::JSON_PARSE_RFC, nullptr, &error_message);
  if (!value) {
    chromeos::Error::AddTo(error, chromeos::errors::json::kDomain,
                           chromeos::errors::json::kParseError, error_message);
    return std::unique_ptr<base::DictionaryValue>();
  }
  base::DictionaryValue* dict_value = nullptr;
  if (!value->GetAsDictionary(&dict_value)) {
    delete value;
    chromeos::Error::AddTo(error, chromeos::errors::json::kDomain,
                           chromeos::errors::json::kObjectExpected,
                           "Response is not a valid JSON object");
    return std::unique_ptr<base::DictionaryValue>();
  }
  return std::unique_ptr<base::DictionaryValue>(dict_value);
}

}  // namespace http
}  // namespace chromeos
