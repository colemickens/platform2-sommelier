// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_HTTP_UTILS_H_
#define BUFFET_HTTP_UTILS_H_

#include "buffet/http_request.h"

namespace base { class Value; }

namespace chromeos {
namespace http {

////////////////////////////////////////////////////////////////////////////////
// The following are simple utility helper functions for common HTTP operations
// that use http::Request object behind the scenes and set it up accordingly.
//
// For more advanced functionality you need to use Request/Response objects
// directly.
////////////////////////////////////////////////////////////////////////////////

// Performs a simple GET request and returns the data as a string.
std::string GetAsString(char const* url);

// Performs a GET request. Success status, returned data and additional
// information (such as returned HTTP headers) can be obtained from
// the returned Response object.
std::unique_ptr<Response> Get(char const* url);

// Performs a HEAD request. Success status and additional
// information (such as returned HTTP headers) can be obtained from
// the returned Response object.
std::unique_ptr<Response> Head(char const* url);

// Performs a POST request with binary data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object.
// If data MIME type is not specified, "application/octet-stream" is assumed
std::unique_ptr<Response> PostBinary(char const* url,
                                     void const* data,
                                     size_t data_size,
                                     char const* mime_type);

inline std::unique_ptr<Response> PostBinary(char const* url,
                                            void const* data,
                                            size_t data_size) {
  return PostBinary(url, data, data_size, nullptr);
}

// Performs a POST request with text data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object.
// If data MIME type is not specified, "application/x-www-form-urlencoded"
// is assumed
std::unique_ptr<Response> PostText(char const* url,
                                   char const* data,
                                   char const* mime_type);

inline std::unique_ptr<Response> PostText(char const* url, char const* data) {
  return PostText(url, data, nullptr);
}

// Performs a POST request with JSON data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object. If a JSON response is expected,
// use ParseJsonResponse() method on the returned Response object.
std::unique_ptr<Response> PostJson(char const* url, base::Value const* json);

// Given an http::Response object, parse the body data into Json object.
// Returns null if failed. Optional |error_message| can be passed in to
// get the extended error information as to why the parse failed.
std::unique_ptr<base::Value> ParseJsonResponse(Response const* response,
                                               std::string* error_message);

} // namespace http
} // namespace chromeos

#endif // BUFFET_HTTP_UTILS_H_
