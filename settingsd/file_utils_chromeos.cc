// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/file_utils.h"

#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/important_file_writer.h>

namespace settingsd {

namespace utils {

namespace {

std::vector<std::string> ListEntries(const std::string& path, int file_type) {
  std::vector<std::string> entries;
  base::FileEnumerator fe(base::FilePath(FILE_PATH_LITERAL(path)), false,
                          file_type);
  for (base::FilePath name = fe.Next(); !name.empty(); name = fe.Next())
    entries.push_back(name.BaseName().value());
  return entries;
}

}  // namespace

std::vector<std::string> ListFiles(const std::string& path) {
  return ListEntries(path, base::FileEnumerator::FILES);
}

std::vector<std::string> ListDirectories(const std::string& path) {
  return ListEntries(path, base::FileEnumerator::DIRECTORIES);
}

bool CreateDirectory(const std::string& path) {
  return CreateDirectory(base::FilePath(FILE_PATH_LITERAL(path)));
}

bool PathExists(const std::string& path) {
  return base::PathExists(base::FilePath(FILE_PATH_LITERAL(path)));
}

bool DeleteFile(const std::string& path) {
  return (unlink(path.c_str()) == 0 || errno == ENOENT);
}

bool ReadFile(const std::string& path,
              std::vector<uint8_t>* contents,
              size_t max_size) {
  std::string data;
  if (!base::ReadFileToString(base::FilePath(FILE_PATH_LITERAL(path)), &data,
                              max_size)) {
    return false;
  }
  if (contents)
    *contents = std::vector<uint8_t>(data.begin(), data.end());
  return true;
}

bool WriteFileAtomically(const std::string& path,
                         const uint8_t* data,
                         size_t size) {
  DCHECK_EQ(sizeof(char), sizeof(uint8_t));
  std::string out_data(reinterpret_cast<const char*>(data), size);
  return base::ImportantFileWriter::WriteFileAtomically(
      base::FilePath(FILE_PATH_LITERAL(path)), out_data);
}

}  // namespace utils

}  // namespace settingsd
