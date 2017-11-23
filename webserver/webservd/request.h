// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBSERVER_WEBSERVD_REQUEST_H_
#define WEBSERVER_WEBSERVD_REQUEST_H_

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <base/macros.h>

struct MHD_Connection;
struct MHD_PostProcessor;

namespace webservd {

class ProtocolHandler;

using PairOfStrings = std::pair<std::string, std::string>;

// This class represents the file information about a file uploaded via
// POST request using multipart/form-data request.
class FileInfo final {
 public:
  FileInfo(const std::string& in_field_name,
           const std::string& in_file_name,
           const std::string& in_content_type,
           const std::string& in_transfer_encoding);

  // The name of the form field for the file upload.
  std::string field_name;
  // The name of the file name specified in the form field.
  std::string file_name;
  // The content type of the file data.
  std::string content_type;
  // Data transfer encoding specified. Could be empty if no transfer encoding
  // was specified.
  std::string transfer_encoding;
  // The file content data.
  std::vector<uint8_t> data;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileInfo);
};

// A class that represents the HTTP request data.
class Request final {
 public:
  Request(const std::string& request_handler_id,
          const std::string& url,
          const std::string& method,
          const std::string& version,
          MHD_Connection* connection,
          ProtocolHandler* protocol_handler);
  ~Request();

  // Obtains the content data of uploaded file identified by |file_id|.
  bool GetFileData(int file_id, std::vector<uint8_t>* contents);

  // Finishes the request and provides the reply data.
  bool Complete(
      int32_t status_code,
      const std::vector<std::tuple<std::string, std::string>>& headers,
      const std::vector<uint8_t>& data);

  // Helper function to provide the string data and mime type.
  bool Complete(
      int32_t status_code,
      const std::vector<std::tuple<std::string, std::string>>& headers,
      const std::string& mime_type,
      const std::string& data);

  // Returns the unique ID of this request (GUID).
  const std::string& GetID() const { return id_; }

  // Returns the unique ID of the request handler this request is processed by
  // (GUID).
  const std::string& GetRequestHandlerID() const { return request_handler_id_; }

  // Returns the unique ID of the protocol handler this request is received
  // from (GUID or "http"/"https" for the two default handlers).
  const std::string& GetProtocolHandlerID() const;

  // Returns the object path of the HTTP request (e.g. "/privet/info").
  const std::string& GetURL() const { return url_; }

  // Returns the request method (e.g. "GET", "POST", ...).
  const std::string& GetMethod() const { return method_; }

  // Returns the raw body of the request, or empty byte array of the request
  // had no body or a POST request has been parsed into form data.
  const std::vector<uint8_t>& GetBody() const { return raw_data_; }

  // Returns the POST form field data.
  const std::vector<PairOfStrings>& GetDataPost() const { return post_data_; }

  // Returns query parameters specified on the URL (as in "?param=value").
  const std::vector<PairOfStrings>& GetDataGet() const { return get_data_; }

  // Returns the information about any files uploaded as part of POST request.
  const std::vector<std::unique_ptr<FileInfo>>& GetFileInfo() const {
    return file_info_;
  }

  // Returns the HTTP request headers.
  const std::vector<PairOfStrings>& GetHeaders() const { return headers_; }

 private:
  friend class RequestHelper;
  friend class ServerHelper;

  enum class State { kIdle, kWaitingForResponse, kResponseReceived, kDone };

  // Helper methods for processing request data coming from the raw HTTP
  // connection.
  // Helper callback methods used by ProtocolHandler's ConnectionHandler to
  // transfer request headers and data to the Request object.
  bool BeginRequestData();
  bool AddRequestData(const void* data, size_t size);
  void EndRequestData();

  // Callback for libmicrohttpd's PostProcessor.
  bool ProcessPostData(const char* key,
                       const char* filename,
                       const char* content_type,
                       const char* transfer_encoding,
                       const char* data,
                       uint64_t off,
                       size_t size);

  // These methods parse the request headers and data so they can be accessed
  // by request handlers later.
  bool AddRawRequestData(const void* data, size_t size);
  bool AddPostFieldData(const char* key,
                        const char* filename,
                        const char* content_type,
                        const char* transfer_encoding,
                        const char* data,
                        size_t size);
  bool AppendPostFieldData(const char* key, const char* data, size_t size);

  std::string id_;
  std::string request_handler_id_;
  std::string url_;
  std::string method_;
  std::string version_;
  MHD_Connection* connection_{nullptr};
  MHD_PostProcessor* post_processor_{nullptr};
  std::vector<uint8_t> raw_data_;
  bool last_posted_data_was_file_{false};
  State state_{State::kIdle};

  std::vector<PairOfStrings> post_data_;
  std::vector<PairOfStrings> get_data_;
  std::vector<std::unique_ptr<FileInfo>> file_info_;
  std::vector<PairOfStrings> headers_;

  int response_status_code_{0};
  std::vector<uint8_t> response_data_;
  std::vector<PairOfStrings> response_headers_;
  ProtocolHandler* protocol_handler_;

  DISALLOW_COPY_AND_ASSIGN(Request);
};

}  // namespace webservd

#endif  // WEBSERVER_WEBSERVD_REQUEST_H_
