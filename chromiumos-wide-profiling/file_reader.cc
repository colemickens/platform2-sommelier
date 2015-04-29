// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromiumos-wide-profiling/file_reader.h"

#include <string.h>

#include <memory>

namespace quipper {

FileReader::FileReader(const string& filename) {
  infile_ = fopen(filename.c_str(), "rb");
  if (!IsOpen()) {
    size_ = 0;
    return;
  }
  // Determine the size of the file.
  fseek(infile_, 0, SEEK_END);
  size_ = ftell(infile_);

  // Reset to the beginning of the file.
  fseek(infile_, 0, SEEK_SET);
}

FileReader::~FileReader() {
  fclose(infile_);
}

bool FileReader::ReadData(const size_t size, void* dest) {
  if (Tell() + size > size_ || fread(dest, 1, size, infile_) < size)
    return false;
  return true;
}

bool FileReader::ReadString(const size_t size, string* str) {
  std::unique_ptr<char[]> buffer(new char[size]);
  if (!ReadData(size, buffer.get()))
    return false;

  size_t actual_length = strnlen(buffer.get(), size);
  *str = string(buffer.get(), actual_length);
  return true;
}

}  // namespace quipper
