// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webserver/webservd/request.h"

#include <microhttpd.h>

#include <base/bind.h>
#include <base/guid.h>
#include <chromeos/http/http_request.h>
#include <chromeos/http/http_utils.h>
#include <chromeos/mime_utils.h>
#include <chromeos/strings/string_utils.h>
#include "webserver/webservd/protocol_handler.h"
#include "webserver/webservd/request_handler_interface.h"

namespace webservd {

// Helper class to provide static callback methods to microhttpd library,
// with the ability to access private methods of Request class.
class RequestHelper {
 public:
  static int PostDataIterator(void* cls,
                              MHD_ValueKind kind,
                              const char* key,
                              const char* filename,
                              const char* content_type,
                              const char* transfer_encoding,
                              const char* data,
                              uint64_t off,
                              size_t size) {
    auto self = reinterpret_cast<Request*>(cls);
    return self->ProcessPostData(key, filename, content_type, transfer_encoding,
                                 data, off, size) ? MHD_YES : MHD_NO;
  }

  static int ValueCallback(void* cls,
                           MHD_ValueKind kind,
                           const char* key,
                           const char* value) {
    auto self = reinterpret_cast<Request*>(cls);
    std::string data;
    if (value)
      data = value;
    if (kind == MHD_HEADER_KIND) {
      self->headers_.emplace_back(chromeos::http::GetCanonicalHeaderName(key),
                                  data);
    } else if (kind == MHD_COOKIE_KIND) {
      // TODO(avakulenko): add support for cookies...
    } else if (kind == MHD_POSTDATA_KIND) {
      self->post_data_.emplace_back(key, data);
    } else if (kind == MHD_GET_ARGUMENT_KIND) {
      self->get_data_.emplace_back(key, data);
    }
    return MHD_YES;
  }
};

FileInfo::FileInfo(const std::string& in_field_name,
                   const std::string& in_file_name,
                   const std::string& in_content_type,
                   const std::string& in_transfer_encoding)
    : field_name(in_field_name),
      file_name(in_file_name),
      content_type(in_content_type),
      transfer_encoding(in_transfer_encoding) {
}

Request::Request(
    const std::string& request_handler_id,
    const std::string& url,
    const std::string& method,
    MHD_Connection* connection,
    ProtocolHandler* protocol_handler)
    : id_{base::GenerateGUID()},
      request_handler_id_{request_handler_id},
      url_{url},
      method_{method},
      connection_{connection},
      protocol_handler_{protocol_handler} {
  post_processor_ = MHD_create_post_processor(
      connection, 1024, &RequestHelper::PostDataIterator, this);
}

Request::~Request() {
  if (post_processor_)
    MHD_destroy_post_processor(post_processor_);
  protocol_handler_->RemoveRequest(this);
}

bool Request::GetFileData(int file_id, std::vector<uint8_t>* contents) {
  if (file_id < 0 || static_cast<size_t>(file_id) >= file_info_.size())
    return false;
  *contents = file_info_[file_id]->data;
  return true;
}

bool Request::Complete(
    int32_t status_code,
    const std::vector<std::tuple<std::string, std::string>>& headers,
    const std::vector<uint8_t>& data) {
  if (state_ != State::kWaitingForResponse)
    return false;

  response_status_code_ = status_code;
  response_headers_.reserve(headers.size());
  for (const auto& tuple : headers) {
    response_headers_.emplace_back(std::get<0>(tuple), std::get<1>(tuple));
  }
  response_data_ = data;
  state_ = State::kResponseReceived;
  protocol_handler_->OnResponseDataReceived();
  return true;
}

bool Request::Complete(
    int32_t status_code,
    const std::vector<std::tuple<std::string, std::string>>& headers,
    const std::string& mime_type,
    const std::string& data) {
  std::vector<std::tuple<std::string, std::string>> headers_copy;
  headers_copy.emplace_back(chromeos::http::response_header::kContentType,
                            mime_type);
  return Complete(status_code, headers_copy,
                  chromeos::string_utils::GetStringAsBytes(data));
}

const std::string& Request::GetProtocolHandlerID() const {
  return protocol_handler_->GetID();
}

bool Request::BeginRequestData() {
  MHD_get_connection_values(connection_, MHD_HEADER_KIND,
                            &RequestHelper::ValueCallback, this);
  MHD_get_connection_values(connection_, MHD_COOKIE_KIND,
                            &RequestHelper::ValueCallback, this);
  MHD_get_connection_values(connection_, MHD_POSTDATA_KIND,
                            &RequestHelper::ValueCallback, this);
  MHD_get_connection_values(connection_, MHD_GET_ARGUMENT_KIND,
                            &RequestHelper::ValueCallback, this);
  return true;
}

bool Request::AddRequestData(const void* data, size_t size) {
  if (!post_processor_)
    return AddRawRequestData(data, size);
  return MHD_post_process(post_processor_,
                          static_cast<const char*>(data), size) == MHD_YES;
}

void Request::EndRequestData() {
  if (state_ == State::kIdle) {
    state_ = State::kWaitingForResponse;
    if (!request_handler_id_.empty()) {
      protocol_handler_->AddRequest(this);
      auto p = protocol_handler_->request_handlers_.find(request_handler_id_);
      CHECK(p != protocol_handler_->request_handlers_.end());
      // Send the request over D-Bus and await the response...
      p->second.handler->HandleRequest(this);
    } else {
      // There was no handler found when request was made, respond with
      // 404 Page Not Found...
      Complete(chromeos::http::status_code::NotFound, {},
               chromeos::mime::text::kPlain, "Not Found");
    }
  } else if (state_ == State::kResponseReceived) {
    MHD_Response* resp = MHD_create_response_from_buffer(
        response_data_.size(), response_data_.data(), MHD_RESPMEM_PERSISTENT);
    for (const auto& pair : response_headers_) {
      MHD_add_response_header(resp, pair.first.c_str(), pair.second.c_str());
    }
    CHECK_EQ(MHD_YES,
             MHD_queue_response(connection_, response_status_code_, resp))
        << "Failed to queue response";
    MHD_destroy_response(resp);  // |resp| is ref-counted.
    state_ = State::kDone;
  }
}

bool Request::ProcessPostData(const char* key,
                              const char* filename,
                              const char* content_type,
                              const char* transfer_encoding,
                              const char* data,
                              uint64_t off,
                              size_t size) {
  if (off > 0)
    return AppendPostFieldData(key, data, size);

  return  AddPostFieldData(key, filename, content_type, transfer_encoding, data,
                           size);
}


bool Request::AddRawRequestData(const void* data, size_t size) {
  const uint8_t* byte_data_ = static_cast<const uint8_t*>(data);
  raw_data_.insert(raw_data_.end(), byte_data_, byte_data_ + size);
  return true;
}

bool Request::AddPostFieldData(const char* key,
                               const char* filename,
                               const char* content_type,
                               const char* transfer_encoding,
                               const char* data,
                               size_t size) {
  if (filename) {
    std::unique_ptr<FileInfo> file_info{
        new FileInfo{key, filename, content_type ? content_type : "",
                     transfer_encoding ? transfer_encoding : ""}};
    file_info->data.assign(data, data + size);
    file_info_.push_back(std::move(file_info));
    last_posted_data_was_file_ = true;
    return true;
  }
  std::string value{data, size};
  post_data_.emplace_back(key, value);
  last_posted_data_was_file_ = false;
  return true;
}

bool Request::AppendPostFieldData(const char* key,
                                  const char* data,
                                  size_t size) {
  if (last_posted_data_was_file_) {
    CHECK(!file_info_.empty());
    CHECK(file_info_.back()->field_name == key);
    FileInfo* file_info = file_info_.back().get();
    file_info->data.insert(file_info->data.end(), data, data + size);
    return true;
  }

  CHECK(!post_data_.empty());
  CHECK(post_data_.back().first == key);
  post_data_.back().second.append(data, size);
  return true;
}

}  // namespace webservd
