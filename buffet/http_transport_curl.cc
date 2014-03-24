// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/http_transport_curl.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <string.h>
#include <base/logging.h>

#include "buffet/mime_utils.h"
#include "buffet/string_utils.h"
#include "buffet/map_utils.h"

using namespace chromeos;
using namespace chromeos::http::curl;

Transport::Transport(std::string const& url, char const* method) :
    request_url_(url),
    method_(method ? method : request_type::kGet) {
  stage_ = Stage::initialized;
}

Transport::~Transport() {
  Close();
}

void Transport::AddRange(int64_t bytes) {
  if (bytes < 0) {
    ranges_.emplace_back(Transport::range_value_omitted, -bytes);
  } else {
    ranges_.emplace_back(bytes, Transport::range_value_omitted);
  }
}

void Transport::AddRange(uint64_t fromByte, uint64_t toByte) {
  ranges_.emplace_back(fromByte, toByte);
}

std::string Transport::GetAccept() const {
  return accept_;
}

std::vector<std::pair<std::string, std::string>> Transport::GetHeaders() const {
  auto headers = MapToVector(headers_);
  std::vector<std::string> ranges;
  if (method_ != request_type::kHead) {
    ranges.reserve(ranges_.size());
    for (auto p : ranges_) {
      if (p.first != range_value_omitted || p.second != range_value_omitted) {
        std::string range;
        if (p.first != range_value_omitted) {
          range = std::to_string(p.first);
        }
        range += '-';
        if (p.second != range_value_omitted) {
          range += std::to_string(p.second);
        }
        ranges.push_back(range);
      }
    }
  }
  if (!ranges.empty())
    headers.emplace_back(request_header::kRange,
                         "bytes=" + string_utils::Join(',', ranges));

  headers.emplace_back(request_header::kAccept, GetAccept());

  return headers;
}

void Transport::AddHeader(char const* header, char const* value) {
  headers_[header] = value;
}

void Transport::RemoveHeader(char const* header) {
  AddHeader(header, "");
}

bool Transport::AddRequestBody(void const* data, size_t size) {
  if (size == 0)
    return true;

  if (data == nullptr) {
    LOG(ERROR) << "Invalid request body data pointer";
    return false;
  }

  unsigned char const* data_ptr = reinterpret_cast<unsigned char const*>(data);
  request_data_.insert(request_data_.end(), data_ptr, data_ptr + size);
  return true;
}

bool Transport::Perform() {
  if (stage_ != Stage::initialized) {
    LOG(ERROR) << "Cannot call Perform() on unintialized transport object";
    return false;
  }

  curl_handle_ = curl_easy_init();
  if (!curl_handle_) {
    LOG(ERROR) << "Failed to initialize CURL";
    return false;
  }

  curl_easy_setopt(curl_handle_, CURLOPT_URL, request_url_.c_str());

  if (!user_agent_.empty()) {
    curl_easy_setopt(curl_handle_,
                     CURLOPT_USERAGENT, user_agent_.c_str());
  }

  if (!referer_.empty()) {
    curl_easy_setopt(curl_handle_,
                     CURLOPT_REFERER, referer_.c_str());
  }

  // Setup HTTP request method and optional request body.
  if (method_ == request_type::kGet) {
    curl_easy_setopt(curl_handle_, CURLOPT_HTTPGET, 1L);
  } else if (method_ == request_type::kHead) {
    curl_easy_setopt(curl_handle_, CURLOPT_NOBODY, 1L);
  } else if (method_ == request_type::kPost) {
    curl_easy_setopt(curl_handle_, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, nullptr);
    if (!request_data_.empty()) {
      curl_easy_setopt(curl_handle_,
                       CURLOPT_READFUNCTION, &Transport::read_callback);
      curl_easy_setopt(curl_handle_, CURLOPT_READDATA, this);
    }
    curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDSIZE_LARGE,
                     curl_off_t(request_data_.size()));
  } else if (method_ == request_type::kPut) {
    curl_easy_setopt(curl_handle_, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl_handle_, CURLOPT_INFILESIZE_LARGE,
                     curl_off_t(request_data_.size()));
    curl_easy_setopt(curl_handle_,
                     CURLOPT_READFUNCTION, &Transport::read_callback);
    curl_easy_setopt(curl_handle_, CURLOPT_READDATA, this);
  } else {
    curl_easy_setopt(curl_handle_, CURLOPT_CUSTOMREQUEST, method_.c_str());
    if (!request_data_.empty()) {
      curl_easy_setopt(curl_handle_,
                       CURLOPT_READFUNCTION, &Transport::read_callback);
      curl_easy_setopt(curl_handle_, CURLOPT_READDATA, this);
    }
  }

  // Setup HTTP response data.
  if (method_ != request_type::kHead) {
    curl_easy_setopt(curl_handle_,
                     CURLOPT_WRITEFUNCTION, &Transport::write_callback);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, this);
  }

  // HTTP request headers
  auto headers = GetHeaders();
  if (method_ != request_type::kGet && method_ != request_type::kHead) {
    if (!content_type_.empty())
      headers.emplace_back(request_header::kContentType, content_type_);
  }

  curl_slist* header_list = nullptr;
  if (!headers.empty()) {
    for (auto pair : headers) {
      std::string header = string_utils::Join(": ", pair.first, pair.second);
      header_list = curl_slist_append(header_list, header.c_str());
    }
    curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, header_list);
  }

  headers.clear();

  // HTTP response headers
  curl_easy_setopt(curl_handle_,
                   CURLOPT_HEADERFUNCTION, &Transport::header_callback);
  curl_easy_setopt(curl_handle_, CURLOPT_HEADERDATA, this);

  CURLcode ret = curl_easy_perform(curl_handle_);
  if (ret != CURLE_OK) {
    error_ = curl_easy_strerror(ret);
    stage_ = Stage::failed;
    LOG(ERROR) << "CURL request failed: " << error_;
  } else {
    stage_ = Stage::response_received;
  }
  curl_slist_free_all(header_list);
  return (ret == CURLE_OK);
}

int Transport::GetResponseStatusCode() const {
  if (stage_ != Stage::response_received)
    return 0;
  long status_code = 0;
  curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &status_code);
  return status_code;
}

std::string Transport::GetResponseHeader(char const* headerName) const {
  auto p = headers_.find(headerName);
  return p != headers_.end() ? p->second : std::string();
}


void Transport::Close() {
  if (curl_handle_) {
    curl_easy_cleanup(curl_handle_);
    curl_handle_ = nullptr;
  }
  stage_ = Stage::closed;
}

size_t Transport::write_callback(char* ptr, size_t size,
                                 size_t num, void* data) {
  Transport* me = reinterpret_cast<Transport*>(data);
  size_t data_len = size * num;
  me->response_data_.insert(me->response_data_.end(), ptr, ptr + data_len);
  return data_len;
}

size_t Transport::read_callback(char* ptr, size_t size,
                                size_t num, void* data) {
  Transport* me = reinterpret_cast<Transport*>(data);
  size_t data_len = size * num;

  if (me->request_data_ptr_ >= me->request_data_.size())
    return 0;

  if (me->request_data_ptr_ + data_len > me->request_data_.size())
    data_len = me->request_data_.size() - me->request_data_ptr_;

  memcpy(ptr, me->request_data_.data() + me->request_data_ptr_, data_len);
  me->request_data_ptr_ += data_len;

  return data_len;
}

size_t Transport::header_callback(char* ptr, size_t size,
                                  size_t num, void* data) {
  Transport* me = reinterpret_cast<Transport*>(data);
  size_t hdr_len = size * num;
  std::string header(ptr, int(hdr_len));
  if (!me->status_text_set_) {
    // First header - response code as "HTTP/1.1 200 OK".
    // Need to extract the OK part
    size_t pos = header.find(' ');
    if(pos != std::string::npos)
      pos = header.find(' ', pos + 1);
    if (pos != std::string::npos)
      me->status_text_ = header.substr(pos + 1);
    me->status_text_set_ = true;
  } else {
    auto pair = string_utils::SplitAtFirst(header, ':');
    if (!pair.second.empty())
      me->headers_.insert(pair);
  }
  return hdr_len;
}
