// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libwebserv/request.h"

#include <microhttpd.h>

#include "libwebserv/connection.h"

namespace libwebserv {

FileInfo::FileInfo(const std::string& file_name,
                   const std::string& content_type,
                   const std::string& transfer_encoding)
    : file_name_(file_name),
      content_type_(content_type),
      transfer_encoding_(transfer_encoding) {
}

const std::vector<uint8_t>& FileInfo::GetData() const {
  return data_;
}

// Helper class to provide static callback methods to microhttpd library,
// with the ability to access private methods of Request class.
class RequestHelper {
 public:
  static int ValueCallback(void* cls, MHD_ValueKind kind,
                           const char* key, const char* value) {
    auto self = reinterpret_cast<Request*>(cls);
    std::string data;
    if (value)
      data = value;
    if (kind == MHD_HEADER_KIND) {
      self->headers_.emplace(Request::GetCanonicalHeaderName(key), data);
    } else if (kind == MHD_COOKIE_KIND) {
      // TODO(avakulenko): add support for cookies...
    } else if (kind == MHD_POSTDATA_KIND) {
      self->post_data_.emplace(key, data);
    } else if (kind == MHD_GET_ARGUMENT_KIND) {
      self->get_data_.emplace(key, data);
    }
    return MHD_YES;
  }
};

Request::Request(const std::shared_ptr<Connection>& connection,
                 const std::string& url,
                 const std::string& method)
    : connection_{connection}, url_{url}, method_{method} {
}

std::unique_ptr<Request> Request::Create(
    const std::shared_ptr<Connection>& connection,
    const std::string& url,
    const std::string& method) {
  std::unique_ptr<Request> request(new Request(connection, url, method));
  return request;
}

const std::vector<uint8_t>& Request::GetData() const {
  return raw_data_;
}

bool Request::BeginRequestData() {
  MHD_get_connection_values(connection_->raw_connection_, MHD_HEADER_KIND,
                            &RequestHelper::ValueCallback, this);
  MHD_get_connection_values(connection_->raw_connection_, MHD_COOKIE_KIND,
                            &RequestHelper::ValueCallback, this);
  MHD_get_connection_values(connection_->raw_connection_, MHD_POSTDATA_KIND,
                            &RequestHelper::ValueCallback, this);
  MHD_get_connection_values(connection_->raw_connection_, MHD_GET_ARGUMENT_KIND,
                            &RequestHelper::ValueCallback, this);
  return true;
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
        new FileInfo{filename, content_type ? content_type : "",
                     transfer_encoding ? transfer_encoding : ""}};
    file_info->data_.assign(data, data + size);
    file_info_.emplace(key, std::move(file_info));
    last_posted_data_was_file_ = true;
    return true;
  }
  std::string value{data, size};
  post_data_.emplace(key, value);
  last_posted_data_was_file_ = false;
  return true;
}

bool Request::AppendPostFieldData(const char* key,
                                  const char* data,
                                  size_t size) {
  if (last_posted_data_was_file_) {
    auto file_pair = file_info_.equal_range(key);
    if (file_pair.first == file_info_.end())
      return false;
    FileInfo* file_info = file_pair.second->second.get();
    file_info->data_.insert(file_info->data_.end(), data, data + size);
    return true;
  }

  auto pair = post_data_.equal_range(key);
  if (pair.first == post_data_.end())
    return false;
  --pair.second;  // Get the last form field with this name/key.
  pair.second->second.append(data, size);
  return true;
}

void Request::EndRequestData() {
}

std::vector<PairOfStrings> Request::GetFormData() const {
  auto data = GetFormDataGet();
  auto post_data = GetFormDataPost();
  data.insert(data.end(), post_data.begin(), post_data.end());
  return data;
}

std::vector<PairOfStrings> Request::GetFormDataGet() const {
  return std::vector<PairOfStrings>{get_data_.begin(), get_data_.end()};
}

std::vector<PairOfStrings> Request::GetFormDataPost() const {
  return std::vector<PairOfStrings>{post_data_.begin(), post_data_.end()};
}

std::vector<std::pair<std::string, const FileInfo*>> Request::GetFiles() const {
  std::vector<std::pair<std::string, const FileInfo*>> data;
  data.reserve(file_info_.size());
  for (const auto& pair : file_info_) {
    data.emplace_back(pair.first, pair.second.get());
  }
  return data;
}

std::vector<std::string> Request::GetFormField(const std::string& name) const {
  std::vector<std::string> data;
  auto pair = get_data_.equal_range(name);
  while (pair.first != pair.second)
    data.push_back(pair.first->second);
  pair = post_data_.equal_range(name);
  while (pair.first != pair.second)
    data.push_back(pair.first->second);
  return data;
}

std::vector<std::string> Request::GetFormFieldPost(
    const std::string& name) const {
  std::vector<std::string> data;
  auto pair = post_data_.equal_range(name);
  while (pair.first != pair.second)
    data.push_back(pair.first->second);
  return data;
}

std::vector<std::string> Request::GetFormFieldGet(
    const std::string& name) const {
  std::vector<std::string> data;
  auto pair = get_data_.equal_range(name);
  while (pair.first != pair.second)
    data.push_back(pair.first->second);
  return data;
}

std::vector<const FileInfo*> Request::GetFileInfo(
    const std::string& name) const {
  std::vector<const FileInfo*> data;
  auto pair = file_info_.equal_range(name);
  while (pair.first != pair.second)
    data.push_back(pair.first->second.get());
  return data;
}

std::vector<PairOfStrings> Request::GetHeaders() const {
  return std::vector<PairOfStrings>{headers_.begin(), headers_.end()};
}

std::vector<std::string> Request::GetHeader(const std::string& name) const {
  std::vector<std::string> data;
  auto pair = headers_.equal_range(GetCanonicalHeaderName(name));
  while (pair.first != pair.second)
    data.push_back(pair.first->second);
  return data;
}

std::string Request::GetCanonicalHeaderName(const std::string& name) {
  std::string canonical_name = name;
  bool word_begin = true;
  for (char& c : canonical_name) {
    if (c == '-') {
      word_begin = true;
    } else {
      if (word_begin) {
        c = toupper(c);
      } else {
        c = tolower(c);
      }
      word_begin = false;
    }
  }
  return canonical_name;
}

}  // namespace libwebserv
