// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/http/http_connection_curl.h>

#include <base/logging.h>
#include <chromeos/http/http_request.h>
#include <chromeos/http/http_transport_curl.h>
#include <chromeos/strings/string_utils.h>

namespace chromeos {
namespace http {
namespace curl {

static int curl_trace(CURL* handle,
                      curl_infotype type,
                      char* data,
                      size_t size,
                      void* userp) {
  std::string msg(data, size);

  switch (type) {
    case CURLINFO_TEXT:
      VLOG(3) << "== Info: " << msg;
      break;
    case CURLINFO_HEADER_OUT:
      VLOG(3) << "=> Send headers:\n" << msg;
      break;
    case CURLINFO_DATA_OUT:
      VLOG(3) << "=> Send data:\n" << msg;
      break;
    case CURLINFO_SSL_DATA_OUT:
      VLOG(3) << "=> Send SSL data" << msg;
      break;
    case CURLINFO_HEADER_IN:
      VLOG(3) << "<= Recv header: " << msg;
      break;
    case CURLINFO_DATA_IN:
      VLOG(3) << "<= Recv data:\n" << msg;
      break;
    case CURLINFO_SSL_DATA_IN:
      VLOG(3) << "<= Recv SSL data" << msg;
      break;
    default:
      break;
  }
  return 0;
}

Connection::Connection(CURL* curl_handle,
                       const std::string& method,
                       const std::shared_ptr<CurlInterface>& curl_interface,
                       const std::shared_ptr<http::Transport>& transport)
    : http::Connection(transport),
      method_(method),
      curl_handle_(curl_handle),
      curl_interface_(curl_interface) {
  // Store the connection pointer inside the CURL handle so we can easily
  // retrieve it when doing asynchronous I/O.
  curl_interface_->EasySetOptPtr(curl_handle_, CURLOPT_PRIVATE, this);
  VLOG(1) << "curl::Connection created: " << method_;
}

Connection::~Connection() {
  if (header_list_)
    curl_slist_free_all(header_list_);
  curl_interface_->EasyCleanup(curl_handle_);
  VLOG(1) << "curl::Connection destroyed";
}

bool Connection::SendHeaders(const HeaderList& headers,
                             chromeos::ErrorPtr* error) {
  headers_.insert(headers.begin(), headers.end());
  return true;
}

bool Connection::SetRequestData(
    std::unique_ptr<DataReaderInterface> data_reader,
    chromeos::ErrorPtr* error) {
  request_data_reader_ = std::move(data_reader);
  return true;
}

void Connection::PrepareRequest() {
  if (VLOG_IS_ON(3)) {
    curl_interface_->EasySetOptCallback(
        curl_handle_, CURLOPT_DEBUGFUNCTION, &curl_trace);
    curl_interface_->EasySetOptInt(curl_handle_, CURLOPT_VERBOSE, 1);
  }

  // Set up HTTP request data.
  uint64_t data_size =
      request_data_reader_ ? request_data_reader_->GetDataSize() : 0;
  if (method_ == request_type::kPut) {
    curl_interface_->EasySetOptOffT(
        curl_handle_, CURLOPT_INFILESIZE_LARGE, data_size);
  } else {
    curl_interface_->EasySetOptOffT(
        curl_handle_, CURLOPT_POSTFIELDSIZE_LARGE, data_size);
  }
  if (request_data_reader_) {
    curl_interface_->EasySetOptCallback(
        curl_handle_, CURLOPT_READFUNCTION, &Connection::read_callback);
    curl_interface_->EasySetOptPtr(curl_handle_, CURLOPT_READDATA, this);
  }

  if (!headers_.empty()) {
    CHECK(header_list_ == nullptr);
    for (auto pair : headers_) {
      std::string header =
          chromeos::string_utils::Join(": ", pair.first, pair.second);
      VLOG(2) << "Request header: " << header;
      header_list_ = curl_slist_append(header_list_, header.c_str());
    }
    curl_interface_->EasySetOptPtr(
        curl_handle_, CURLOPT_HTTPHEADER, header_list_);
  }

  headers_.clear();

  // Set up HTTP response data.
  if (method_ != request_type::kHead) {
    curl_interface_->EasySetOptCallback(
        curl_handle_, CURLOPT_WRITEFUNCTION, &Connection::write_callback);
    curl_interface_->EasySetOptPtr(curl_handle_, CURLOPT_WRITEDATA, this);
  }

  // HTTP response headers
  curl_interface_->EasySetOptCallback(
      curl_handle_, CURLOPT_HEADERFUNCTION, &Connection::header_callback);
  curl_interface_->EasySetOptPtr(curl_handle_, CURLOPT_HEADERDATA, this);
}

bool Connection::FinishRequest(chromeos::ErrorPtr* error) {
  PrepareRequest();
  CURLcode ret = curl_interface_->EasyPerform(curl_handle_);
  if (ret != CURLE_OK) {
    Transport::AddEasyCurlError(error, FROM_HERE, ret, curl_interface_.get());
  } else {
    LOG(INFO) << "Response: " << GetResponseStatusCode() << " ("
              << GetResponseStatusText() << ")";
    VLOG(2) << "Response data (" << response_data_.size() << "): "
            << std::string(reinterpret_cast<const char*>(response_data_.data()),
                           response_data_.size());
  }
  return (ret == CURLE_OK);
}

int Connection::FinishRequestAsync(const SuccessCallback& success_callback,
                                   const ErrorCallback& error_callback) {
  PrepareRequest();
  return transport_->StartAsyncTransfer(this, success_callback, error_callback);
}

int Connection::GetResponseStatusCode() const {
  int status_code = 0;
  curl_interface_->EasyGetInfoInt(
      curl_handle_, CURLINFO_RESPONSE_CODE, &status_code);
  return status_code;
}

std::string Connection::GetResponseStatusText() const {
  return status_text_;
}

std::string Connection::GetProtocolVersion() const {
  return protocol_version_;
}

std::string Connection::GetResponseHeader(
    const std::string& header_name) const {
  auto p = headers_.find(header_name);
  return p != headers_.end() ? p->second : std::string();
}

uint64_t Connection::GetResponseDataSize() const {
  return response_data_.size();
}

bool Connection::ReadResponseData(void* data,
                                  size_t buffer_size,
                                  size_t* size_read,
                                  chromeos::ErrorPtr* error) {
  size_t size_to_read = response_data_.size() - response_data_ptr_;
  if (size_to_read > buffer_size)
    size_to_read = buffer_size;
  memcpy(data, response_data_.data() + response_data_ptr_, size_to_read);
  if (size_read)
    *size_read = size_to_read;
  response_data_ptr_ += size_to_read;
  return true;
}

size_t Connection::write_callback(char* ptr,
                                  size_t size,
                                  size_t num,
                                  void* data) {
  Connection* me = reinterpret_cast<Connection*>(data);
  size_t data_len = size * num;
  me->response_data_.insert(me->response_data_.end(), ptr, ptr + data_len);
  return data_len;
}

size_t Connection::read_callback(char* ptr,
                                 size_t size,
                                 size_t num,
                                 void* data) {
  Connection* me = reinterpret_cast<Connection*>(data);
  size_t data_len = size * num;

  size_t read_size = 0;
  bool success =
      me->request_data_reader_->ReadData(ptr, data_len, &read_size, nullptr);
  VLOG_IF(3, success) << "Sending data: " << std::string{ptr, read_size};
  return success ? read_size : CURL_READFUNC_ABORT;
}

size_t Connection::header_callback(char* ptr,
                                   size_t size,
                                   size_t num,
                                   void* data) {
  using chromeos::string_utils::SplitAtFirst;
  Connection* me = reinterpret_cast<Connection*>(data);
  size_t hdr_len = size * num;
  std::string header(ptr, hdr_len);
  // Remove newlines at the end of header line.
  while (!header.empty() && (header.back() == '\r' || header.back() == '\n')) {
    header.pop_back();
  }

  VLOG(2) << "Response header: " << header;

  if (!me->status_text_set_) {
    // First header - response code as "HTTP/1.1 200 OK".
    // Need to extract the OK part
    auto pair = SplitAtFirst(header, " ");
    me->protocol_version_ = pair.first;
    me->status_text_ = SplitAtFirst(pair.second, " ").second;
    me->status_text_set_ = true;
  } else {
    auto pair = SplitAtFirst(header, ":");
    if (!pair.second.empty())
      me->headers_.insert(pair);
  }
  return hdr_len;
}

}  // namespace curl
}  // namespace http
}  // namespace chromeos
