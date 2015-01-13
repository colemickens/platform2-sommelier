// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/http/http_utils.h>

#include <algorithm>

#include <base/bind.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/values.h>
#include <chromeos/data_encoding.h>
#include <chromeos/errors/error_codes.h>
#include <chromeos/mime_utils.h>

using chromeos::mime::AppendParameter;
using chromeos::mime::RemoveParameters;

namespace chromeos {
namespace http {

namespace {
void SuccessCallbackStringWrapper(
    const base::Callback<void(const std::string&)>& success_callback,
    scoped_ptr<Response> response) {
  success_callback.Run(response->GetDataAsString());
}
}  // anonymous namespace

std::unique_ptr<Response> GetAndBlock(const std::string& url,
                                      const HeaderList& headers,
                                      std::shared_ptr<Transport> transport,
                                      chromeos::ErrorPtr* error) {
  return SendRequestWithNoDataAndBlock(
      request_type::kGet, url, headers, transport, error);
}

void Get(const std::string& url,
         const HeaderList& headers,
         std::shared_ptr<Transport> transport,
         const SuccessCallback& success_callback,
         const ErrorCallback& error_callback) {
  SendRequestWithNoData(request_type::kGet,
                        url,
                        headers,
                        transport,
                        success_callback,
                        error_callback);
}

std::string GetAsStringAndBlock(const std::string& url,
                                const HeaderList& headers,
                                std::shared_ptr<Transport> transport,
                                chromeos::ErrorPtr* error) {
  auto resp = GetAndBlock(url, headers, transport, error);
  return resp ? resp->GetDataAsString() : std::string();
}

void GetAsString(
    const std::string& url,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    const base::Callback<void(const std::string&)>& success_callback,
    const ErrorCallback& error_callback) {
  Get(url,
      headers,
      transport,
      base::Bind(SuccessCallbackStringWrapper, success_callback),
      error_callback);
}

std::unique_ptr<Response> HeadAndBlock(const std::string& url,
                                       std::shared_ptr<Transport> transport,
                                       chromeos::ErrorPtr* error) {
  return SendRequestWithNoDataAndBlock(
      request_type::kHead, url, {}, transport, error);
}

void Head(const std::string& url,
          std::shared_ptr<Transport> transport,
          const SuccessCallback& success_callback,
          const ErrorCallback& error_callback) {
  SendRequestWithNoData(request_type::kHead,
                        url,
                        {},
                        transport,
                        success_callback,
                        error_callback);
}

std::unique_ptr<Response> PostTextAndBlock(const std::string& url,
                                           const std::string& data,
                                           const std::string& mime_type,
                                           const HeaderList& headers,
                                           std::shared_ptr<Transport> transport,
                                           chromeos::ErrorPtr* error) {
  return PostBinaryAndBlock(
      url, data.data(), data.size(), mime_type, headers, transport, error);
}

void PostText(const std::string& url,
              const std::string& data,
              const std::string& mime_type,
              const HeaderList& headers,
              std::shared_ptr<Transport> transport,
              const SuccessCallback& success_callback,
              const ErrorCallback& error_callback) {
  PostBinary(url,
             data.data(),
             data.size(),
             mime_type,
             headers,
             transport,
             success_callback,
             error_callback);
}

std::unique_ptr<Response> SendRequestAndBlock(
    const std::string& method,
    const std::string& url,
    const void* data,
    size_t data_size,
    const std::string& mime_type,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error) {
  Request request(url, method, transport);
  request.AddHeaders(headers);
  if (data_size > 0) {
    CHECK(!mime_type.empty()) << "MIME type must be specified if request body "
                                 "message is provided";
    request.SetContentType(mime_type);
    if (!request.AddRequestBody(data, data_size, error))
      return std::unique_ptr<Response>();
  }
  return request.GetResponseAndBlock(error);
}

std::unique_ptr<Response> SendRequestWithNoDataAndBlock(
    const std::string& method,
    const std::string& url,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error) {
  return SendRequestAndBlock(
      method, url, nullptr, 0, {}, headers, transport, error);
}

void SendRequest(const std::string& method,
                 const std::string& url,
                 std::unique_ptr<DataReaderInterface> data_reader,
                 const std::string& mime_type,
                 const HeaderList& headers,
                 std::shared_ptr<Transport> transport,
                 const SuccessCallback& success_callback,
                 const ErrorCallback& error_callback) {
  Request request(url, method, transport);
  request.AddHeaders(headers);
  if (data_reader && data_reader->GetDataSize() > 0) {
    CHECK(!mime_type.empty()) << "MIME type must be specified if request body "
                                 "message is provided";
    request.SetContentType(mime_type);
    chromeos::ErrorPtr error;
    if (!request.AddRequestBody(std::move(data_reader), &error)) {
      transport->RunCallbackAsync(
          FROM_HERE, base::Bind(error_callback, base::Owned(error.release())));
      return;
    }
  }
  request.GetResponse(success_callback, error_callback);
}

void SendRequest(const std::string& method,
                 const std::string& url,
                 const void* data,
                 size_t data_size,
                 const std::string& mime_type,
                 const HeaderList& headers,
                 std::shared_ptr<Transport> transport,
                 const SuccessCallback& success_callback,
                 const ErrorCallback& error_callback) {
  std::unique_ptr<DataReaderInterface> data_reader{
      new MemoryDataReader(data, data_size)};
  SendRequest(method,
              url,
              std::move(data_reader),
              mime_type,
              headers,
              transport,
              success_callback,
              error_callback);
}

void SendRequestWithNoData(const std::string& method,
                           const std::string& url,
                           const HeaderList& headers,
                           std::shared_ptr<Transport> transport,
                           const SuccessCallback& success_callback,
                           const ErrorCallback& error_callback) {
  SendRequest(method,
              url,
              {},
              {},
              headers,
              transport,
              success_callback,
              error_callback);
}

std::unique_ptr<Response> PostBinaryAndBlock(
    const std::string& url,
    const void* data,
    size_t data_size,
    const std::string& mime_type,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error) {
  return SendRequestAndBlock(request_type::kPost,
                             url,
                             data,
                             data_size,
                             mime_type,
                             headers,
                             transport,
                             error);
}

void PostBinary(const std::string& url,
                std::unique_ptr<DataReaderInterface> data_reader,
                const std::string& mime_type,
                const HeaderList& headers,
                std::shared_ptr<Transport> transport,
                const SuccessCallback& success_callback,
                const ErrorCallback& error_callback) {
  SendRequest(request_type::kPost,
              url,
              std::move(data_reader),
              mime_type,
              headers,
              transport,
              success_callback,
              error_callback);
}

void PostBinary(const std::string& url,
                const void* data,
                size_t data_size,
                const std::string& mime_type,
                const HeaderList& headers,
                std::shared_ptr<Transport> transport,
                const SuccessCallback& success_callback,
                const ErrorCallback& error_callback) {
  SendRequest(request_type::kPost,
              url,
              data,
              data_size,
              mime_type,
              headers,
              transport,
              success_callback,
              error_callback);
}

std::unique_ptr<Response> PostFormDataAndBlock(
    const std::string& url,
    const FormFieldList& data,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error) {
  std::string encoded_data = chromeos::data_encoding::WebParamsEncode(data);
  return PostBinaryAndBlock(url,
                            encoded_data.c_str(),
                            encoded_data.size(),
                            chromeos::mime::application::kWwwFormUrlEncoded,
                            headers,
                            transport,
                            error);
}

std::unique_ptr<Response> PostFormDataAndBlock(
    const std::string& url,
    std::unique_ptr<FormData> form_data,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error) {
  Request request(url, request_type::kPost, transport);
  request.AddHeaders(headers);
  if (!request.AddRequestBodyAsFormData(std::move(form_data), error))
    return std::unique_ptr<Response>();
  return request.GetResponseAndBlock(error);
}

void PostFormData(const std::string& url,
                  const FormFieldList& data,
                  const HeaderList& headers,
                  std::shared_ptr<Transport> transport,
                  const SuccessCallback& success_callback,
                  const ErrorCallback& error_callback) {
  std::string encoded_data = chromeos::data_encoding::WebParamsEncode(data);
  return PostBinary(url,
                    encoded_data.c_str(),
                    encoded_data.size(),
                    chromeos::mime::application::kWwwFormUrlEncoded,
                    headers,
                    transport,
                    success_callback,
                    error_callback);
}

void PostFormData(const std::string& url,
                  std::unique_ptr<FormData> form_data,
                  const HeaderList& headers,
                  std::shared_ptr<Transport> transport,
                  const SuccessCallback& success_callback,
                  const ErrorCallback& error_callback) {
  Request request(url, request_type::kPost, transport);
  request.AddHeaders(headers);
  chromeos::ErrorPtr error;
  if (!request.AddRequestBodyAsFormData(std::move(form_data), &error)) {
    transport->RunCallbackAsync(
        FROM_HERE, base::Bind(error_callback, base::Owned(error.release())));
    return;
  }
  request.GetResponse(success_callback, error_callback);
}

std::unique_ptr<Response> PostJsonAndBlock(const std::string& url,
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
  return PostBinaryAndBlock(
      url, data.c_str(), data.size(), mime_type, headers, transport, error);
}

void PostJson(const std::string& url,
              std::unique_ptr<base::Value> json,
              const HeaderList& headers,
              std::shared_ptr<Transport> transport,
              const SuccessCallback& success_callback,
              const ErrorCallback& error_callback) {
  std::string data;
  if (json)
    base::JSONWriter::Write(json.get(), &data);
  std::string mime_type = AppendParameter(chromeos::mime::application::kJson,
                                          chromeos::mime::parameters::kCharset,
                                          "utf-8");
  PostBinary(url,
             data.c_str(),
             data.size(),
             mime_type,
             headers,
             transport,
             success_callback,
             error_callback);
}

std::unique_ptr<Response> PatchJsonAndBlock(
    const std::string& url,
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
  return SendRequestAndBlock(request_type::kPatch,
                             url,
                             data.c_str(),
                             data.size(),
                             mime_type,
                             headers,
                             transport,
                             error);
}

void PatchJson(const std::string& url,
               std::unique_ptr<base::Value> json,
               const HeaderList& headers,
               std::shared_ptr<Transport> transport,
               const SuccessCallback& success_callback,
               const ErrorCallback& error_callback) {
  std::string data;
  if (json)
    base::JSONWriter::Write(json.get(), &data);
  std::string mime_type = AppendParameter(chromeos::mime::application::kJson,
                                          chromeos::mime::parameters::kCharset,
                                          "utf-8");
  SendRequest(request_type::kPatch,
              url,
              data.c_str(),
              data.size(),
              mime_type,
              headers,
              transport,
              success_callback,
              error_callback);
}

std::unique_ptr<base::DictionaryValue> ParseJsonResponse(
    const Response* response,
    int* status_code,
    chromeos::ErrorPtr* error) {
  if (!response)
    return std::unique_ptr<base::DictionaryValue>();

  if (status_code)
    *status_code = response->GetStatusCode();

  // Make sure we have a correct content type. Do not try to parse
  // binary files, or HTML output. Limit to application/json and text/plain.
  auto content_type = RemoveParameters(response->GetContentType());
  if (content_type != chromeos::mime::application::kJson &&
      content_type != chromeos::mime::text::kPlain) {
    chromeos::Error::AddTo(error,
                           FROM_HERE,
                           chromeos::errors::json::kDomain,
                           "non_json_content_type",
                           "Unexpected response content type: " + content_type);
    return std::unique_ptr<base::DictionaryValue>();
  }

  std::string json = response->GetDataAsString();
  std::string error_message;
  base::Value* value = base::JSONReader::ReadAndReturnError(
      json, base::JSON_PARSE_RFC, nullptr, &error_message);
  if (!value) {
    chromeos::Error::AddTo(error,
                           FROM_HERE,
                           chromeos::errors::json::kDomain,
                           chromeos::errors::json::kParseError,
                           error_message);
    return std::unique_ptr<base::DictionaryValue>();
  }
  base::DictionaryValue* dict_value = nullptr;
  if (!value->GetAsDictionary(&dict_value)) {
    delete value;
    chromeos::Error::AddTo(error,
                           FROM_HERE,
                           chromeos::errors::json::kDomain,
                           chromeos::errors::json::kObjectExpected,
                           "Response is not a valid JSON object");
    return std::unique_ptr<base::DictionaryValue>();
  }
  return std::unique_ptr<base::DictionaryValue>(dict_value);
}

}  // namespace http
}  // namespace chromeos
