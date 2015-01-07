// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_HTTP_HTTP_UTILS_H_
#define LIBCHROMEOS_CHROMEOS_HTTP_HTTP_UTILS_H_

#include <string>
#include <utility>
#include <vector>

#include <chromeos/chromeos_export.h>
#include <chromeos/errors/error.h>
#include <chromeos/http/http_form_data.h>
#include <chromeos/http/http_request.h>

namespace base {
class Value;
class DictionaryValue;
}  // namespace base

namespace chromeos {
namespace http {

using FormFieldList = std::vector<std::pair<std::string, std::string>>;

////////////////////////////////////////////////////////////////////////////////
// The following are simple utility helper functions for common HTTP operations
// that use http::Request object behind the scenes and set it up accordingly.
// The values for request method, data MIME type, request header names should
// not be directly encoded in most cases, but use predefined constants from
// http_request.h.
// So, instead of calling:
//    SendRequestAndBlock("POST",
//                        "http://url",
//                        "data", 4,
//                        "text/plain",
//                        {{"Authorization", "Bearer TOKEN"}},
//                        transport, error);
// You should do use this instead:
//    SendRequestAndBlock(chromeos::http::request_type::kPost,
//                        "http://url",
//                        "data", 4,
//                        chromeos::mime::text::kPlain,
//                        {{chromeos::http::request_header::kAuthorization,
//                          "Bearer TOKEN"}},
//                        transport, error);
//
// For more advanced functionality you need to use Request/Response objects
// directly.
////////////////////////////////////////////////////////////////////////////////

// Performs a generic HTTP request with binary data. Success status,
// returned data and additional information (such as returned HTTP headers)
// can be obtained from the returned Response object.
CHROMEOS_EXPORT std::unique_ptr<Response> SendRequestAndBlock(
    const std::string& method,
    const std::string& url,
    const void* data,
    size_t data_size,
    const std::string& mime_type,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error);

// Same as above, but without sending the request body.
// This is especially useful for requests like "GET" and "HEAD".
CHROMEOS_EXPORT std::unique_ptr<Response> SendRequestWithNoDataAndBlock(
    const std::string& method,
    const std::string& url,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error);

// Performs a simple GET request and returns the data as a string.
CHROMEOS_EXPORT std::string GetAsStringAndBlock(
    const std::string& url,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error);

// Performs a GET request. Success status, returned data and additional
// information (such as returned HTTP headers) can be obtained from
// the returned Response object.
CHROMEOS_EXPORT std::unique_ptr<Response> GetAndBlock(
    const std::string& url,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error);

// Performs a HEAD request. Success status and additional
// information (such as returned HTTP headers) can be obtained from
// the returned Response object.
CHROMEOS_EXPORT std::unique_ptr<Response> HeadAndBlock(
    const std::string& url,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error);

// Performs a POST request with binary data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object.
CHROMEOS_EXPORT std::unique_ptr<Response> PostBinaryAndBlock(
    const std::string& url,
    const void* data,
    size_t data_size,
    const std::string& mime_type,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error);

// Performs a POST request with text data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object.
CHROMEOS_EXPORT std::unique_ptr<Response> PostTextAndBlock(
    const std::string& url,
    const std::string& data,
    const std::string& mime_type,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error);

// Performs a POST request with form data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object. The form data is a list of key/value
// pairs. The data is posed as "application/x-www-form-urlencoded".
CHROMEOS_EXPORT std::unique_ptr<Response> PostFormDataAndBlock(
    const std::string& url,
    const FormFieldList& data,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error);

// Performs a POST request with form data, including binary file uploads.
// Success status, returned data and additional information (such as returned
// HTTP headers) can be obtained from the returned Response object.
// The data is posed as "multipart/form-data".
CHROMEOS_EXPORT std::unique_ptr<Response> PostFormDataAndBlock(
    const std::string& url,
    std::unique_ptr<FormData> form_data,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error);

// Performs a POST request with JSON data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object. If a JSON response is expected,
// use ParseJsonResponse() method on the returned Response object.
CHROMEOS_EXPORT std::unique_ptr<Response> PostJsonAndBlock(
    const std::string& url,
    const base::Value* json,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error);

// Performs a PATCH request with JSON data. Success status, returned data
// and additional information (such as returned HTTP headers) can be obtained
// from the returned Response object. If a JSON response is expected,
// use ParseJsonResponse() method on the returned Response object.
CHROMEOS_EXPORT std::unique_ptr<Response> PatchJsonAndBlock(
    const std::string& url,
    const base::Value* json,
    const HeaderList& headers,
    std::shared_ptr<Transport> transport,
    chromeos::ErrorPtr* error);

// Given an http::Response object, parse the body data into Json object.
// Returns null if failed. Optional |error| can be passed in to
// get the extended error information as to why the parse failed.
CHROMEOS_EXPORT std::unique_ptr<base::DictionaryValue> ParseJsonResponse(
    const Response* response, int* status_code, chromeos::ErrorPtr* error);

}  // namespace http
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_HTTP_HTTP_UTILS_H_
