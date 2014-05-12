// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_STORAGE_IMPLS_H_
#define BUFFET_STORAGE_IMPLS_H_

#include <base/basictypes.h>
#include <base/file_util.h>
#include <base/values.h>

#include "buffet/storage_interface.h"

namespace buffet {

// Persists the given Value to an atomically written file.
class FileStorage : public StorageInterface {
 public:
  explicit FileStorage(const base::FilePath& file_path);
  virtual ~FileStorage() = default;
  virtual std::unique_ptr<base::Value> Load() override;
  virtual bool Save(const base::Value* config) override;

 private:
  base::FilePath file_path_;
  DISALLOW_COPY_AND_ASSIGN(FileStorage);
};

// StorageInterface for testing. Just stores the values in memory.
class MemStorage : public StorageInterface {
 public:
  MemStorage() = default;
  virtual ~MemStorage() = default;
  virtual std::unique_ptr<base::Value> Load() override;
  virtual bool Save(const base::Value* config) override;
  int save_count() { return save_count_; }
  void reset_save_count() { save_count_ = 0; }

 private:
  int save_count_ = 0;
  std::unique_ptr<base::Value> cache_;
  DISALLOW_COPY_AND_ASSIGN(MemStorage);
};

}  // namespace buffet

#endif  // BUFFET_STORAGE_IMPLS_H_
