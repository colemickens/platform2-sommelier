// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEBSERV_REQUEST_H_
#define LIBWEBSERV_REQUEST_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>

#include "libwebserv/export.h"

namespace libwebserv {

class Connection;
using PairOfStrings = std::pair<std::string, std::string>;

class LIBWEBSERV_EXPORT FileInfo final {
 public:
  FileInfo(const std::string& file_name,
           const std::string& content_type,
           const std::string& transfer_encoding);

  const std::vector<uint8_t>& GetData() const;
  const std::string& GetFileName() const { return file_name_; }
  const std::string& GetContentType() const { return content_type_; }
  const std::string& GetTransferEncoding() const { return transfer_encoding_; }

 private:
  std::string file_name_;
  std::string content_type_;
  std::string transfer_encoding_;
  std::vector<uint8_t> data_;

  friend class Request;
  DISALLOW_COPY_AND_ASSIGN(FileInfo);
};

// A class that represents the HTTP request data.
class LIBWEBSERV_EXPORT Request final {
 public:
  // Factory constructor method.
  static std::unique_ptr<Request> Create(
      const std::shared_ptr<Connection>& connection,
      const std::string& url,
      const std::string& method);

  // Gets the request body data stream. Note that the stream is available
  // only for requests that provided data and if this data is not already
  // pre-parsed by the server (e.g. "application/x-www-form-urlencoded" and
  // "multipart/form-data"). If there is no request body, or the data has been
  // pre-parsed by the server, the returned stream will be empty.
  const std::vector<uint8_t>& GetData() const;

  // Returns the request path (e.g. "/path/document").
  const std::string& GetPath() const { return url_; }

  // Returns the request method (e.g. "GET", "POST", etc).
  const std::string& GetMethod() const { return method_; }

  // Returns a list of key-value pairs that include values provided on the URL
  // (e.g. "http://server.com/?foo=bar") and the non-file form fields in the
  // POST data.
  std::vector<PairOfStrings> GetFormData() const;

  // Returns a list of key-value pairs for query parameters provided on the URL
  // (e.g. "http://server.com/?foo=bar").
  std::vector<PairOfStrings> GetFormDataGet() const;

  // Returns a list of key-value pairs for the non-file form fields in the
  // POST data.
  std::vector<PairOfStrings> GetFormDataPost() const;

  // Returns a list of file information records for all the file uploads in
  // the POST request.
  std::vector<std::pair<std::string, const FileInfo*>> GetFiles() const;

  // Gets the values of form field with given |name|. This includes both
  // values provided on the URL and as part of form data in POST request.
  std::vector<std::string> GetFormField(const std::string& name) const;

  // Gets the values of form field with given |name| for form data in POST
  // request.
  std::vector<std::string> GetFormFieldPost(const std::string& name) const;

  // Gets the values of URL query parameters with given |name|.
  std::vector<std::string> GetFormFieldGet(const std::string& name) const;

  // Gets the file upload parameters for a file form field of given |name|.
  std::vector<const FileInfo*> GetFileInfo(const std::string& name) const;

  // Returns a list of key-value pairs for all the request headers.
  std::vector<PairOfStrings> GetHeaders() const;

  // Returns the value(s) of a request head of given |name|.
  std::vector<std::string> GetHeader(const std::string& name) const;

 private:
  LIBWEBSERV_PRIVATE Request(const std::shared_ptr<Connection>& connection,
                             const std::string& url,
                             const std::string& method);

  // Helper methods for processing request data coming from the raw HTTP
  // connection.
  // These methods parse the request headers and data so they can be accessed
  // by request handlers later.
  LIBWEBSERV_PRIVATE bool BeginRequestData();
  LIBWEBSERV_PRIVATE bool AddRawRequestData(const void* data, size_t size);
  LIBWEBSERV_PRIVATE bool AddPostFieldData(const char* key,
                                           const char* filename,
                                           const char* content_type,
                                           const char* transfer_encoding,
                                           const char* data,
                                           size_t size);
  LIBWEBSERV_PRIVATE bool AppendPostFieldData(const char* key,
                                              const char* data,
                                              size_t size);
  LIBWEBSERV_PRIVATE void EndRequestData();

  // Converts a request header name to canonical form (lowercase with uppercase
  // first letter and each letter after a hyphen ('-')).
  // "content-TYPE" will be converted to "Content-Type".
  LIBWEBSERV_PRIVATE static std::string GetCanonicalHeaderName(
      const std::string& name);

  std::shared_ptr<Connection> connection_;
  std::string url_;
  std::string method_;
  std::vector<uint8_t> raw_data_;
  bool last_posted_data_was_file_{false};

  std::multimap<std::string, std::string> post_data_;
  std::multimap<std::string, std::string> get_data_;
  std::multimap<std::string, std::unique_ptr<FileInfo>> file_info_;
  std::multimap<std::string, std::string> headers_;

  friend class Connection;
  friend class RequestHelper;
  DISALLOW_COPY_AND_ASSIGN(Request);
};

}  // namespace libwebserv

#endif  // LIBWEBSERV_REQUEST_H_
