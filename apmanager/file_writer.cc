// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/file_writer.h"

#include <base/files/file_util.h>

namespace apmanager {

namespace {

base::LazyInstance<FileWriter>::DestructorAtExit g_file_writer
    = LAZY_INSTANCE_INITIALIZER;

}  // namespace

FileWriter::FileWriter() {}
FileWriter::~FileWriter() {}

FileWriter* FileWriter::GetInstance() {
  return g_file_writer.Pointer();
}

bool FileWriter::Write(const std::string& file_name,
                       const std::string& content) {
  if (base::WriteFile(base::FilePath(file_name),
                      content.c_str(),
                      content.size()) == -1) {
    return false;
  }
  return true;
}

}  // namespace apmanager
