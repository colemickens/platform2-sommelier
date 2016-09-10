// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "image-burner/image_burner_utils.h"

#include <base/files/file_path.h>
#include <base/logging.h>
#include <rootdev/rootdev.h>

namespace imageburn {

const int kFsyncRatio = 1024;

BurnWriter::BurnWriter() {}

bool BurnWriter::Open(const char* path) {
  if (file_.IsValid())
    return false;

  file_.Initialize(base::FilePath(path),
                   base::File::FLAG_WRITE | base::File::FLAG_OPEN);
  if (!file_.IsValid()) {
    PLOG(ERROR) << "Couldn't open target path " << path;
    return false;
  } else {
    LOG(INFO) << path << " opened";
    return true;
  }
}

bool BurnWriter::Close() {
  if (!file_.IsValid())
    return false;
  file_.Close();
  return true;
}

int BurnWriter::Write(const char* data_block, int data_size) {
  const int written = file_.WriteAtCurrentPos(data_block, data_size);
  if (written != data_size) {
    PLOG(ERROR) << "Error writing to target file";
    return written;
  }

  if (!writes_count_ && !file_.Flush()) {
    PLOG(ERROR) << "Error flushing target file.";
    return -1;
  }

  writes_count_++;
  if (writes_count_ == kFsyncRatio)
    writes_count_ = 0;

  return written;
}

BurnReader::BurnReader() {}

bool BurnReader::Open(const char* path) {
  if (file_.IsValid())
    return false;

  file_.Initialize(base::FilePath(path),
                   base::File::FLAG_READ | base::File::FLAG_OPEN);
  if (!file_.IsValid()) {
    PLOG(ERROR) << "Couldn't open source path " << path;
    return false;
  } else {
    LOG(INFO) << path << " opened";
    return true;
  }
}

bool BurnReader::Close() {
  if (!file_.IsValid())
    return false;
  file_.Close();
  return true;
}

int BurnReader::Read(char* data_block, int data_size) {
  const int read = file_.ReadAtCurrentPos(data_block, data_size);
  if (read < 0)
    PLOG(ERROR) << "Error reading from source file";
  return read;
}

int64_t BurnReader::GetSize() {
  if (!file_.IsValid())
    return -1;
  return file_.GetLength();
}

BurnRootPathGetter::BurnRootPathGetter() {}

bool BurnRootPathGetter::GetRootPath(std::string* path) {
  char root_path[PATH_MAX];
  if (rootdev(root_path, sizeof(root_path), true, true)) {
    // Coult not get root path.
    return false;
  }
  *path = root_path;
  return true;
}

}  // namespace imageburn

