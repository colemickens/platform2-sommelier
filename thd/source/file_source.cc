// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "thd/source/file_source.h"

#include <string>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>

namespace thd {

FileSource::FileSource(const base::FilePath& file_path)
    : file_path_(file_path) {}

FileSource::~FileSource() {}

bool FileSource::ReadValue(int64_t* value_out) {
  DCHECK(value_out);
  if (!base::PathExists(file_path_)) {
    LOG(ERROR) << "Path " << file_path_.value() << " not found.";
    return false;
  }
  std::string contents;
  if (!base::ReadFileToString(file_path_, &contents)) {
    PLOG(ERROR) << "Failed to read path " << file_path_.value() << ".";
    return false;
  }
  return base::StringToInt64(contents, value_out);
}

}  // namespace thd
