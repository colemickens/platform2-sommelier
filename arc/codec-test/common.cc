// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/codec-test/common.h"

#include <time.h>

#include <algorithm>
#include <sstream>

namespace android {

InputFileStream::InputFileStream(std::string file_path) {
  file_ = std::ifstream(file_path, std::ifstream::binary);
}

bool InputFileStream::IsValid() const {
  return file_.is_open();
}

size_t InputFileStream::GetLength() {
  int current_pos = file_.tellg();

  file_.seekg(0, file_.end);
  size_t ret = file_.tellg();

  file_.seekg(current_pos, file_.beg);
  return ret;
}

void InputFileStream::Rewind() {
  file_.clear();
  file_.seekg(0);
}

size_t InputFileStream::Read(char* buffer, size_t size) {
  file_.read(buffer, size);
  if (file_.fail())
    return -1;

  return file_.gcount();
}

std::vector<std::string> SplitString(const std::string& src, char delim) {
  std::stringstream ss(src);
  std::string item;
  std::vector<std::string> ret;
  while (std::getline(ss, item, delim)) {
    ret.push_back(item);
  }
  return ret;
}

int64_t GetNowUs() {
  struct timespec t;
  t.tv_sec = t.tv_nsec = 0;
  clock_gettime(CLOCK_MONOTONIC, &t);
  int64_t nsecs = static_cast<int64_t>(t.tv_sec) * 1000000000LL + t.tv_nsec;
  return nsecs / 1000ll;
}

}  // namespace android
