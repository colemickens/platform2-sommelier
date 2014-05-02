// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/storage_impls.h"

#include <base/files/important_file_writer.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>

namespace buffet {

FileStorage::FileStorage(const base::FilePath& file_path)
    : file_path_(file_path) { }

std::unique_ptr<base::Value> FileStorage::Load() {
  std::string json;
  if (!base::ReadFileToString(file_path_, &json))
    return std::unique_ptr<base::Value>();

  return std::unique_ptr<base::Value>(base::JSONReader::Read(json));
}

bool FileStorage::Save(const base::Value* config) {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      config, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return base::ImportantFileWriter::WriteFileAtomically(file_path_, json);
}


std::unique_ptr<base::Value> MemStorage::Load() {
  return std::unique_ptr<base::Value>(cache_->DeepCopy());
}

bool MemStorage::Save(const base::Value* config) {
  cache_.reset(config->DeepCopy());
  ++save_count_;
  return true;
}

}  // namespace buffet
