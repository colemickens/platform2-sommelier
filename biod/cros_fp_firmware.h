// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_CROS_FP_FIRMWARE_H_
#define BIOD_CROS_FP_FIRMWARE_H_

#include <string>

#include <base/files/file_path.h>
#include <base/macros.h>

namespace biod {

class CrosFpFirmware {
 public:
  enum class Status {
    kUninitialized,
    kOk,
    kNotFound,
    kOpenError,
    kBadFmap,
  };

  struct ImageVersion {
    std::string ro_version;
    std::string rw_version;
  };

  explicit CrosFpFirmware(const base::FilePath& image_path);
  base::FilePath GetPath() const;
  bool IsValid() const;
  Status GetStatus() const;
  std::string GetStatusString() const;
  const ImageVersion& GetVersion() const;

 private:
  void DecodeVersionFromFile();

  static std::string StatusToString(Status status);

  base::FilePath path_;
  ImageVersion version_;
  Status status_;

  DISALLOW_COPY_AND_ASSIGN(CrosFpFirmware);
};

}  // namespace biod

#endif  // BIOD_CROS_FP_FIRMWARE_H_
