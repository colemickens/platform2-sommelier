// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef METRICS_PERSISTENT_INTEGER_H_
#define METRICS_PERSISTENT_INTEGER_H_

#include <stdint.h>

#include <string>

#include <base/macros.h>

namespace chromeos_metrics {

// PersistentIntegers is a named 64-bit integer value backed by a file.
// The in-memory value acts as a write-through cache of the file value.
// If the backing file doesn't exist or has bad content, the value is 0.

class PersistentInteger {
 public:
  explicit PersistentInteger(const std::string& name);

  // Virtual only because of mock.
  virtual ~PersistentInteger();

  // Sets the value.  This writes through to the backing file.
  void Set(int64_t v);

  // Gets the value.  May sync from backing file first.
  int64_t Get();

  // Returns the name of the object.
  std::string Name() { return name_; }

  // Convenience function for Get() followed by Set(0).
  int64_t GetAndClear();

  // Convenience function for v = Get, Set(v + x).
  // Virtual only because of mock.
  virtual void Add(int64_t x);

  // Changes some behavior for the purpose of testing, including using
  // |backing_file_dir_path| as the directory where the persistent integer
  // backing files are stored.
  static void SetTestingMode(const std::string& backing_files_dir);

 private:
  static const int kVersion = 1001;

  // Writes |value_| to the backing file, creating it if necessary.
  void Write();

  // Reads the value from the backing file, stores it in |value_|, and returns
  // true if the backing file is valid.  Returns false otherwise, and creates
  // a valid backing file as a side effect.
  bool Read();

  // Returns the name of the backing files directory.
  static std::string GetBackingFilesDirectory();
  // If |dir_name| is not empty, sets the backing file directory name to
  // |dir_name|.  In all cases returns the backing files directory name.  It is
  // an error to attempt to set the name when not testing.
  static std::string GetAndMaybeSetBackingFilesDirectory(
      const std::string& dir_name);

  int64_t value_;
  int32_t version_;
  std::string name_;
  std::string backing_file_name_;
  bool synced_;

  static bool testing_;

  DISALLOW_COPY_AND_ASSIGN(PersistentInteger);
};

}  // namespace chromeos_metrics

#endif  // METRICS_PERSISTENT_INTEGER_H_
