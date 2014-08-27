// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_HTTP_HTTP_UTILS_H_
#define LIBCHROMEOS_CHROMEOS_HTTP_HTTP_UTILS_H_

#include <string>
#include <utility>
#include <vector>

#include <chromeos/errors/error.h>
#include <chromeos/http/http_request.h>

namespace base {
class Value;
class DictionaryValue;
}  // namespace base

namespace chromeos {
namespace http {

typedef std::vector<std::pair<std::string, std::string>> FormFieldList;

////////////////////////////////////////////////////////////////////////////////
// The following are simple utility helper functions for common HTTP operations
// that use http::Request object behind the scenes and set it up accordingly.
//
// For more advanced functionality you need to use Request/Response objects
// directly.
////////////////////////////////////////////////////////////////////////////////

// Performs a generic HTTP request with binary data. Success status,
// returned data and additional information (such as returned HTTP headers)
// can be obtained from the returned Response object.
// If data MIME type is not specified, "application/octet-stream" is assumed.
std::unique_ptr<Response> SendRequest(
    const char* method, const std::string& url,
    const void* data, size_t data_size, const char* mime_type,
    const HeaderList& headers, std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error);

// Performs a simple GET request and returns the data as a string.
std::string GetAsString(const std::string& url, const HeaderList& headers,
                        std::shared_ptr<Transport> transport,
                        chromeos::ErrorPtr* error);
inline std::string GetAsString(const std::string& url,
                               std::shared_ptr<Transport> transport,
                               chromeos::ErrorPtr* error) {
  return GetAsString(url, HeaderList(), transport, error);
}

// Performs a GET request. Success status, returned data and additional
// information (such as returned HTTP headers) can be obtained from
// the returned Response object.
std::unique_ptr<Response> Get(const std::string& url,
                              const HeaderList& headers,
                              std::shared_ptr<Transport> transport,
                              chromeos::ErrorPtr* error);
inline std::unique_ptr<Response> Get(
    const std::string& url, std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error) {
  return Get(url, HeaderList(), transport, error);
}

// Performs a HEAD request. Success status and additional
// information (such as returned HTTP headers) can be obtained from
// the returned Response object.
std::unique_ptr<Response> Head(const std::string& url,
                               std::shared_ptr<Transport> transport,
                               chromeos::ErrorPtr* error);

// Performs a POST request with binary data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object.
// If data MIME type is not specified, "application/octet-stream" is assumed
std::unique_ptr<Response> PostBinary(const std::string& url,
                                     const void* data,
                                     size_t data_size,
                                     const char* mime_type,
                                     const HeaderList& headers,
                                     std::shared_ptr<Transport> transport,
                                     chromeos::ErrorPtr* error);

inline std::unique_ptr<Response> PostBinary(
    const std::string& url, const void* data, size_t data_size,
    const char* mime_type, std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error) {
  return PostBinary(url, data, data_size, mime_type, HeaderList(), transport,
                    error);
}

inline std::unique_ptr<Response> PostBinary(
    const std::string& url, const void* data, size_t data_size,
    std::shared_ptr<Transport> transport, chromeos::ErrorPtr* error) {
  return PostBinary(url, data, data_size, nullptr, transport, error);
}

// Performs a POST request with text data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object.
// If data MIME type is not specified, "application/x-www-form-urlencoded"
// is assumed.
std::unique_ptr<Response> PostText(const std::string& url,
                                   const char* data,
                                   const char* mime_type,
                                   const HeaderList& headers,
                                   std::shared_ptr<Transport> transport,
                                   chromeos::ErrorPtr* error);

inline std::unique_ptr<Response> PostText(
    const std::string& url, const char* data, const char* mime_type,
    std::shared_ptr<Transport> transport, chromeos::ErrorPtr* error) {
  return PostText(url, data, mime_type, HeaderList(), transport, error);
}

inline std::unique_ptr<Response> PostText(
    const std::string& url, const char* data,
    std::shared_ptr<Transport> transport, chromeos::ErrorPtr* error) {
  return PostText(url, data, nullptr, transport, error);
}

// Performs a POST request with form data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object. The form data is a list of key/value
// pairs. The data is posed as "application/x-www-form-urlencoded".
std::unique_ptr<Response> PostFormData(
    const std::string& url, const FormFieldList& data,
    const HeaderList& headers, std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error);

inline std::unique_ptr<Response> PostFormData(
    const std::string& url, const FormFieldList& data,
    std::shared_ptr<Transport> transport, chromeos::ErrorPtr* error) {
  return PostFormData(url, data, HeaderList(), transport, error);
}

// Performs a POST request with JSON data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object. If a JSON response is expected,
// use ParseJsonResponse() method on the returned Response object.
std::unique_ptr<Response> PostJson(const std::string& url,
                                   const base::Value* json,
                                   const HeaderList& headers,
                                   std::shared_ptr<Transport> transport,
                                   chromeos::ErrorPtr* error);

inline std::unique_ptr<Response> PostJson(
    const std::string& url, const base::Value* json,
    std::shared_ptr<Transport> transport, chromeos::ErrorPtr* error) {
  return PostJson(url, json, HeaderList(), transport, error);
}

// Performs a PATCH request with JSON data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object. If a JSON response is expected,
// use ParseJsonResponse() method on the returned Response object.
std::unique_ptr<Response> PatchJson(const std::string& url,
                                    const base::Value* json,
                                    const HeaderList& headers,
                                    std::shared_ptr<Transport> transport,
                                    chromeos::ErrorPtr* error);

inline std::unique_ptr<Response> PatchJson(
    const std::string& url, const base::Value* json,
    std::shared_ptr<Transport> transport, chromeos::ErrorPtr* error) {
  return PatchJson(url, json, HeaderList(), transport, error);
}

// Given an http::Response object, parse the body data into Json object.
// Returns null if failed. Optional |error| can be passed in to
// get the extended error information as to why the parse failed.
std::unique_ptr<base::DictionaryValue> ParseJsonResponse(
    const Response* response, int* status_code, chromeos::ErrorPtr* error);

}  // namespace http
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_HTTP_HTTP_UTILS_H_
