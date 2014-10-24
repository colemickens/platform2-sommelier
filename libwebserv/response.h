// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEBSERV_RESPONSE_H_
#define LIBWEBSERV_RESPONSE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>

#include "libwebserv/export.h"

namespace base {
class Value;
}  // namespace base

struct MHD_Connection;

namespace libwebserv {

class Connection;

// Response class is a proxy for HTTP response used by the request handler
// to provide response HTTP headers and data.
class LIBWEBSERV_EXPORT Response
    : public std::enable_shared_from_this<Response> {
 public:
  ~Response();

  // Factory constructor method.
  static std::unique_ptr<Response> Create(
      const std::shared_ptr<Connection>& connection);

  // Adds a single HTTP response header to the response.
  void AddHeader(const std::string& header_name, const std::string& value);

  // Adds number of HTTP response headers to the response.
  void AddHeaders(
      const std::vector<std::pair<std::string, std::string>>& headers);

  // Generic reply method for sending arbitrary binary data response.
  void Reply(int status_code,
             const void* data,
             size_t data_size,
             const char* mime_type);

  // Reply with text body.
  void ReplyWithText(int status_code,
                     const std::string& text,
                     const char* mime_type);

  // Reply with JSON object. The content type will be "application/json".
  void ReplyWithJson(int status_code, const base::Value* json);

  // Special form for JSON response for simple objects that have a flat
  // list of key-value pairs of string type.
  void ReplyWithJson(int status_code,
                     const std::map<std::string, std::string>& json);

  // Issue a redirect response, so the client browser loads a page at
  // the URL specified in |redirect_url|. If this is not an external URL,
  // it must be an absolute path starting at the root "/...".
  void Redirect(int status_code, const std::string& redirect_url);

  // Send a plain text response (with no Content-Type header).
  // Usually used with error responses. |error_text| must be plain text.
  void ReplyWithError(int status_code, const std::string& error_text);

  // Send "404 Not Found" response.
  void ReplyWithErrorNotFound();

 private:
  LIBWEBSERV_PRIVATE explicit Response(
      const std::shared_ptr<Connection>& connection);

  LIBWEBSERV_PRIVATE void SendResponse();

  const std::shared_ptr<Connection>& connection_;
  int status_code_{0};
  std::vector<uint8_t> data_;
  std::multimap<std::string, std::string> headers_;
  bool reply_sent_{false};

  friend class Connection;
  friend class ResponseHelper;
  DISALLOW_COPY_AND_ASSIGN(Response);
};

}  // namespace libwebserv

#endif  // LIBWEBSERV_RESPONSE_H_
