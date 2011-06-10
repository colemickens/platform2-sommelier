// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IMAGE_BURNER_UTILS_INTERFACES_H_
#define IMAGE_BURNER_UTILS_INTERFACES_H_

#include <string>

#include <base/basictypes.h>

namespace imageburn {

class FileSystemWriter {
 public:
  virtual ~FileSystemWriter() {};
  virtual int Write(char* data_block, int data_size) = 0;
  virtual bool Open(const char* path) = 0;
  virtual bool Close() = 0;
};

class FileSystemReader {
 public:
  virtual ~FileSystemReader() {};
  virtual bool Open(const char* path) = 0;
  virtual bool Close() = 0;
  virtual int Read(char* data_block, int data_size) = 0;
  virtual int64 GetSize() = 0;
};

class RootPathGetter {
 public:
  virtual ~RootPathGetter() {};
  virtual bool GetRootPath(std::string* path) = 0;
};

class SignalSender {
 public:
  virtual ~SignalSender() {};
  virtual void SendFinishedSignal(const char* target_path, bool success,
                                  const char* error_message) = 0;
  virtual void SendProgressSignal(int64 amount_burnt, int64 total_size,
                                  const char* target_path) = 0;
};

}  // namespace imageburn.
#endif  // IMAGE_BURNER_UTILS_INTERFACES_H_

