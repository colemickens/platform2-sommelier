// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_CODEC_TEST_COMMON_H_
#define ARC_CODEC_TEST_COMMON_H_

#include <fstream>
#include <string>
#include <vector>

namespace android {

// Structure to store resolution.
struct Size {
  Size() : width(0), height(0) {}
  Size(int w, int h) : width(w), height(h) {}
  bool IsEmpty() { return width == 0 || height == 0; }

  int width;
  int height;
};

// Wrapper of std::ifstream.
class InputFileStream {
 public:
  explicit InputFileStream(std::string file_path);

  // Check if the file is valid.
  bool IsValid() const;
  // Get the size of the file.
  size_t GetLength();
  // Set position to the beginning of the file.
  void Rewind();
  // Read the given number of bytes to the buffer. Return the number of bytes
  // read or -1 on error.
  size_t Read(char* buffer, size_t size);

 private:
  std::ifstream file_;
};

// Split the string |src| by the delimiter |delim|.
std::vector<std::string> SplitString(const std::string& src, char delim);

// Get monotonic timestamp for now in microseconds.
int64_t GetNowUs();

}  // namespace android
#endif  // ARC_CODEC_TEST_COMMON_H_
