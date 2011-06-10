// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IMAGE_BURNER_UTILS_H_
#define IMAGE_BURNER_UTILS_H_

#include <fstream>
#include <string>

#include "image_burner_utils_interfaces.h"

namespace imageburn {

class BurnWriter : public FileSystemWriter {
 public:
  BurnWriter();
  virtual ~BurnWriter();

  virtual bool Open(const char* path);
  virtual bool Close();
  virtual int Write(char* data_block, int data_size);

 private:
   FILE* file_;
   int writes_count_;
};

class BurnReader : public FileSystemReader {
 public:
  BurnReader();
  virtual ~BurnReader();

  virtual bool Open(const char* path);
  virtual bool Close();
  virtual int Read(char* data_block, int data_size);
  virtual int64 GetSize();

 private:
  FILE* file_;
};

class BurnRootPathGetter : public RootPathGetter {
 public:
  virtual bool GetRootPath(std::string* path);
};

}  // namespace imageburn.

#endif  // IMAGE_BURNER_UTLS_H_

