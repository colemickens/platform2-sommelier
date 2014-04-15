// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_HTTP_UTILS_H_
#define BUFFET_HTTP_UTILS_H_

#include "buffet/http_request.h"

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
std::unique_ptr<Response> SendRequest(char const* method,
                                      std::string const& url,
                                      void const* data,
                                      size_t data_size,
                                      char const* mime_type,
                                      HeaderList const& headers);

// Performs a simple GET request and returns the data as a string.
std::string GetAsString(std::string const& url);

// Performs a GET request. Success status, returned data and additional
// information (such as returned HTTP headers) can be obtained from
// the returned Response object.
std::unique_ptr<Response> Get(std::string const& url);

// Performs a HEAD request. Success status and additional
// information (such as returned HTTP headers) can be obtained from
// the returned Response object.
std::unique_ptr<Response> Head(std::string const& url);

// Performs a POST request with binary data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object.
// If data MIME type is not specified, "application/octet-stream" is assumed
std::unique_ptr<Response> PostBinary(std::string const& url,
                                     void const* data,
                                     size_t data_size,
                                     char const* mime_type,
                                     HeaderList const& headers);

inline std::unique_ptr<Response> PostBinary(std::string const& url,
                                            void const* data,
                                            size_t data_size,
                                            char const* mime_type) {
  return PostBinary(url, data, data_size, mime_type, HeaderList());
}

inline std::unique_ptr<Response> PostBinary(std::string const& url,
                                            void const* data,
                                            size_t data_size) {
  return PostBinary(url, data, data_size, nullptr);
}

// Performs a POST request with text data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object.
// If data MIME type is not specified, "application/x-www-form-urlencoded"
// is assumed.
std::unique_ptr<Response> PostText(std::string const& url,
                                   char const* data,
                                   char const* mime_type,
                                   HeaderList const& headers);

inline std::unique_ptr<Response> PostText(std::string const& url,
                                          char const* data,
                                          char const* mime_type) {
  return PostText(url, data, mime_type, HeaderList());
}

inline std::unique_ptr<Response> PostText(std::string const& url,
                                          char const* data) {
  return PostText(url, data, nullptr);
}

// Performs a POST request with form data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object. The form data is a list of key/value
// pairs. The data is posed as "application/x-www-form-urlencoded".
std::unique_ptr<Response> PostFormData(std::string const& url,
                                       FormFieldList const& data,
                                       HeaderList const& headers);

inline std::unique_ptr<Response> PostFormData(std::string const& url,
                                              FormFieldList const& data) {
  return PostFormData(url, data, HeaderList());
}

// Performs a POST request with JSON data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object. If a JSON response is expected,
// use ParseJsonResponse() method on the returned Response object.
std::unique_ptr<Response> PostJson(std::string const& url,
                                   base::Value const* json,
                                   HeaderList const& headers);

inline std::unique_ptr<Response> PostJson(std::string const& url,
                                          base::Value const* json) {
  return PostJson(url, json, HeaderList());
}

// Performs a PATCH request with JSON data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object. If a JSON response is expected,
// use ParseJsonResponse() method on the returned Response object.
std::unique_ptr<Response> PatchJson(std::string const& url,
                                    base::Value const* json,
                                    HeaderList const& headers);

inline std::unique_ptr<Response> PatchJson(std::string const& url,
                                           base::Value const* json) {
  return PatchJson(url, json, HeaderList());
}

// Given an http::Response object, parse the body data into Json object.
// Returns null if failed. Optional |error_message| can be passed in to
// get the extended error information as to why the parse failed.
std::unique_ptr<base::DictionaryValue> ParseJsonResponse(
    Response const* response, int* status_code, std::string* error_message);

} // namespace http
} // namespace chromeos

#endif // BUFFET_HTTP_UTILS_H_
