// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/http/data_reader.h>

namespace chromeos {
namespace http {

MemoryDataReader::MemoryDataReader(const std::string& data)
    : MemoryDataReader{data.data(), data.size()} {}

MemoryDataReader::MemoryDataReader(const void* data, size_t data_size)
    : data_{reinterpret_cast<const uint8_t*>(data),
            reinterpret_cast<const uint8_t*>(data) + data_size} {}

void MemoryDataReader::SetData(const std::string& data) {
  SetData(data.data(), data.size());
}

void MemoryDataReader::SetData(const void* data, size_t data_size) {
  data_.assign(reinterpret_cast<const uint8_t*>(data),
               reinterpret_cast<const uint8_t*>(data) + data_size);
  read_pointer_ = 0;
}

bool MemoryDataReader::ReadData(void* buffer,
                                size_t max_size_to_read,
                                size_t* size_read,
                                chromeos::ErrorPtr* error) {
  if (read_pointer_ >= data_.size()) {
    *size_read = 0;
    return true;
  }

  if (read_pointer_ + max_size_to_read > data_.size())
    max_size_to_read = data_.size() - read_pointer_;

  memcpy(buffer, data_.data() + read_pointer_, max_size_to_read);
  read_pointer_ += max_size_to_read;
  *size_read = max_size_to_read;
  return true;
}

}  // namespace http
}  // namespace chromeos
