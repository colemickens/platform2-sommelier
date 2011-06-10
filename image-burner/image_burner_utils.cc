// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "image_burner_utils.h"

#include <cstring>
#include <errno.h>
#include <rootdev/rootdev.h>

#include <base/logging.h>

namespace imageburn {

// After every kSentSignalRatio IO operations, update signal will be emited.
const int kSentSignalRatio = 256;
const int kFsyncRatio = 1024;

BurnWriter::BurnWriter() : file_(NULL),
                           writes_count_(0) {
}

BurnWriter::~BurnWriter() {
  if (file_)
    fclose(file_);
}

bool BurnWriter::Open(const char* path) {
  file_ = fopen(path, "wb");
  if (!file_) {
    LOG(ERROR) << "Couldn't open target path " << path << " : "
               << strerror(errno);
    return false;
  } else {
    LOG(INFO) << path << " opened";
    return true;
  }
}

bool BurnWriter::Close() {
  if (file_) {
    if (fclose(file_) != 0) {
      LOG(ERROR) << "Couldn't close target file: " << strerror(errno);
      return false;
   } else {
      LOG(INFO) << "Target file closed";
    }
  }
  return true;
}

int BurnWriter::Write(char* data_block, int data_size) {
  size_t written = fwrite(data_block, sizeof(char), data_size, file_);
  if (written != static_cast<size_t>(data_size)) {
    LOG(ERROR) << "Error writing to target file: " << strerror(errno);
    return written;
  }

  if (!writes_count_ && fsync(fileno(file_))) {
    LOG(ERROR) << "Error syncing target file: " << strerror(errno);
    return -1;
  }
  writes_count_++;
  if (writes_count_ == kFsyncRatio)
    writes_count_ = 0;

  return written;
}

BurnReader::BurnReader() : file_(NULL) {
}

BurnReader::~BurnReader() {
  if (file_)
    fclose(file_);
}

bool BurnReader::Open(const char* path) {
  file_ = fopen(path, "rb");
  if (!file_) {
    LOG(ERROR) << "Couldn't open source path " << path << " : "
               << strerror(errno);
    return false;
  } else {
    LOG(INFO) << path << " opened";
    return true;
  }
}

bool BurnReader::Close() {
  if (file_) {
    if (fclose(file_) != 0) {
      LOG(ERROR) << "Couldn't close source file: " << strerror(errno);
      return false;
   } else {
      LOG(INFO) << "Source file closed";
    }
  }
  return true;
}

int BurnReader::Read(char* data_block, int data_size) {
  int read = fread(data_block, sizeof(char), data_size, file_);
  if (read < 0)
    LOG(ERROR) << "Error reading from source file: " << strerror(errno);
  return read;
}

int64 BurnReader::GetSize() {
  int current = ftell(file_);
  fseek(file_, 0, SEEK_END);
  int64 result = static_cast<int64>(ftell(file_));
  fseek(file_, current, SEEK_SET);
  return result;
}

bool BurnRootPathGetter::GetRootPath(std::string* path) {
  char root_path[PATH_MAX];
  if (rootdev(root_path, sizeof(root_path), true, true)) {
    // Coult not get root path.
    return false;
  }
  *path = root_path;
  return true;
}

}  // namespace imageburn.

