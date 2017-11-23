// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <libwebserv/request.h>

#include <base/callback.h>
#include <chromeos/http/http_utils.h>

#include <libwebserv/protocol_handler.h>

namespace libwebserv {

FileInfo::FileInfo(ProtocolHandler* handler,
                   int file_id,
                   const std::string& request_id,
                   const std::string& file_name,
                   const std::string& content_type,
                   const std::string& transfer_encoding)
    : handler_{handler},
      file_id_{file_id},
      request_id_{request_id},
      file_name_(file_name),
      content_type_(content_type),
      transfer_encoding_(transfer_encoding) {
}

void FileInfo::GetData(
    const base::Callback<void(const std::vector<uint8_t>&)>& success_callback,
    const base::Callback<void(chromeos::Error*)>& error_callback) {
  handler_->GetFileData(request_id_,
                        file_id_,
                        success_callback,
                        error_callback);
}

Request::Request(ProtocolHandler* handler,
                 const std::string& url,
                 const std::string& method)
    : handler_{handler}, url_{url}, method_{method} {
}

Request::~Request() {
}

const std::vector<uint8_t>& Request::GetData() const {
  return raw_data_;
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
  while (pair.first != pair.second) {
    data.push_back(pair.first->second);
    ++pair.first;
  }
  pair = post_data_.equal_range(name);
  while (pair.first != pair.second) {
    data.push_back(pair.first->second);
    ++pair.first;
  }
  return data;
}

std::vector<std::string> Request::GetFormFieldPost(
    const std::string& name) const {
  std::vector<std::string> data;
  auto pair = post_data_.equal_range(name);
  while (pair.first != pair.second) {
    data.push_back(pair.first->second);
    ++pair.first;
  }
  return data;
}

std::vector<std::string> Request::GetFormFieldGet(
    const std::string& name) const {
  std::vector<std::string> data;
  auto pair = get_data_.equal_range(name);
  while (pair.first != pair.second) {
    data.push_back(pair.first->second);
    ++pair.first;
  }
  return data;
}

std::vector<const FileInfo*> Request::GetFileInfo(
    const std::string& name) const {
  std::vector<const FileInfo*> data;
  auto pair = file_info_.equal_range(name);
  while (pair.first != pair.second) {
    data.push_back(pair.first->second.get());
    ++pair.first;
  }
  return data;
}

std::vector<PairOfStrings> Request::GetHeaders() const {
  return std::vector<PairOfStrings>{headers_.begin(), headers_.end()};
}

std::vector<std::string> Request::GetHeader(const std::string& name) const {
  std::vector<std::string> data;
  auto range =
      headers_.equal_range(chromeos::http::GetCanonicalHeaderName(name));
  while (range.first != range.second) {
    data.push_back(range.first->second);
    ++range.first;
  }
  return data;
}

std::string Request::GetFirstHeader(const std::string& name) const {
  auto p = headers_.find(chromeos::http::GetCanonicalHeaderName(name));
  return (p != headers_.end()) ? p->second : std::string{};
}


}  // namespace libwebserv
