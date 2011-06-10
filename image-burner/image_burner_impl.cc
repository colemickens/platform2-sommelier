// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "image_burner_impl.h"

#include <cstring>
#include <ctype.h>
#include <base/basictypes.h>
#include <base/logging.h>

namespace imageburn {
const int kBurningBlockSize = 4 * 1024;  // 4 KiB

const char* kFilePathPrefix = "/dev/sd";

BurnerImpl::BurnerImpl(FileSystemWriter* writer,
                       FileSystemReader* reader,
                       SignalSender* signal_sender,
                       RootPathGetter* root_path_getter)
    : writer_(writer),
      reader_(reader),
      root_path_getter_(root_path_getter),
      signal_sender_(signal_sender),
      data_block_size_(kBurningBlockSize) {
}

ErrorCode BurnerImpl::BurnImage(const char* from_path, const char* to_path) {
  ErrorCode err = IMAGEBURN_OK;

  if (!writer_ || !reader_ || !signal_sender_ || !root_path_getter_)
    err = IMAGEBURN_ERROR_BURNER_NOT_INITIALIZED;

  if (!err)
    ValidateTargetPath(to_path, &err);

  if (!err)
    ValidateSourcePath(from_path, &err);

  if (!err)
    DoBurn(from_path, to_path, &err);

  // TODO(tbarzic) send error specific error message.
  signal_sender_->SendFinishedSignal(to_path, !err,
                                     (!err ? "" : "Error during burn"));
  return err;
}

void BurnerImpl::InitSignalSender(SignalSender* signal_sender) {
  signal_sender_ = signal_sender;
}

bool BurnerImpl::ValidateTargetPath(const char* path, ErrorCode* error) {
  if (!path) {
    *error = IMAGEBURN_ERROR_NULL_TARGET_PATH;
    LOG(ERROR) << "Target path set to NULL.";
    return false;
  }

  int file_prefix_len = strlen(kFilePathPrefix);
  if (strncmp(path, kFilePathPrefix, file_prefix_len) != 0 ||
      strlen(path) == file_prefix_len) {
    *error = IMAGEBURN_ERROR_INVALID_TARGET_PATH;
    LOG(ERROR) << "Target path is not valid file path.";
    return false;
  }

  int i = file_prefix_len;
  while (path[i]) {
    if (!islower(path[i])) {
      *error = IMAGEBURN_ERROR_INVALID_TARGET_PATH;
      LOG(ERROR) << "Target path is not valid file path.";
      return false;
    }
    i++;
  }

  std::string root_path;
  // This will return roots file path, so we can compare target path, which
  // should also be devices file path, to it.
  bool got_root_path = root_path_getter_->GetRootPath(&root_path);
  if (!got_root_path ||
      strncmp(root_path.c_str(), path, root_path.length()) == 0) {
    *error = IMAGEBURN_ERROR_TARGET_PATH_ON_ROOT;
    LOG(ERROR) << "Target path is on root device.";
    return false;
  }

  return true;
}

bool BurnerImpl::ValidateSourcePath(const char* path, ErrorCode* error) {
  if (!path) {
    *error = IMAGEBURN_ERROR_NULL_SOURCE_PATH;
    LOG(ERROR) << "Source path set to NULL.";
    return false;
  }
  return true;
}

bool BurnerImpl::DoBurn(const char* from_path, const char* to_path,
                    ErrorCode* error) {
  LOG(INFO) << "Burning " << from_path << " to " << to_path;
  *error = IMAGEBURN_OK;

  bool success = reader_->Open(from_path);
  if (success) {
    if (!(success = writer_->Open(to_path)))
      *error = IMAGEBURN_ERROR_CANNOT_OPEN_TARGET;
  } else {
    *error = IMAGEBURN_ERROR_CANNOT_OPEN_SOURCE;
  }

  if (success) {
    scoped_ptr<char> buffer(new char[data_block_size_]);

    int64 total_burnt = 0;
    int64 image_size = reader_->GetSize();

    int len = 0;
    while ((len = reader_->Read(buffer.get(), data_block_size_)) > 0) {
      if (writer_->Write(buffer.get(), len) == len) {
        total_burnt += static_cast<int64>(len);
        signal_sender_->SendProgressSignal(total_burnt, image_size, to_path);
      } else {
        success = false;
        *error = IMAGEBURN_ERROR_FAILED_WRITING_TO_TARGET;
        break;
      }
    }
    if (len < 0) {
      success = false;
      *error = IMAGEBURN_ERROR_FAILED_READING_SOURCE;
    }
  }

  if (!writer_->Close() && success) {
    // We set this only when there is no previous error.
    success = false;
    *error = IMAGEBURN_ERROR_CANNOT_CLOSE_TARGET;
  }

  if (!reader_->Close() && success) {
    // We set this only when there is no previous error.
    success = false;
    *error = IMAGEBURN_ERROR_CANNOT_CLOSE_SOURCE;
  }
  return success;
}

}  // namespace imageburn.
