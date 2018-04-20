// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CRYPTOHOME_LOCKBOX_CACHE_TPM_H_
#define CRYPTOHOME_LOCKBOX_CACHE_TPM_H_

#include <stdio.h>
#include <unistd.h>

#include <string>

#include <base/files/file_path.h>
#include <base/logging.h>

#include "cryptohome/platform.h"
#include "cryptohome/stub_tpm.h"

namespace cryptohome {
// Implement just enough to make Lockbox able to use a file for the nvram
// contents.
class LockboxCacheTpm : public StubTpm {
 public:
  explicit LockboxCacheTpm(const uint32_t index,
                           const base::FilePath& nvram_path)
    : index_(index), nvram_path_(nvram_path) { }
  virtual ~LockboxCacheTpm();

  // Initialized the faked out NVRAM state internally using
  // |platform| system helpers.  |open_key| is not used.
  virtual bool Init(Platform* platform, bool open_key) {
    if (open_key || !platform)
      return false;
    if (!platform->ReadFile(nvram_path_, &nvram_data_))
      nvram_data_.clear();
    return true;
  }

  // Pretend the TPM is enabled.
  virtual bool IsEnabled() { return true; }

  // Indicate if the TPM is owned based on if there is an NVRAM area or not.
  // If there is no NVRAM data, we assume an unowned TPM, not a failure to
  // verify.
  virtual bool IsOwned() { return !!nvram_data_.size(); }

  // Populates |blob| with the prepared contents if the |index|
  // matches.  If |blob| is unpopulated, it returns false.  On
  // success, it returns true.
  virtual bool ReadNvram(uint32_t index, brillo::SecureBlob* blob) {
    if (index == index_) {
      blob->clear();
      blob->resize(nvram_data_.size());
      memcpy(blob->data(), nvram_data_.data(), nvram_data_.size());
      return true;
    }
    return false;
  }

  // If the |index| matches the prepared index and data was read, this
  // returns true.  Otherwise false.
  virtual bool IsNvramDefined(uint32_t index) {
    if (index == index_ && nvram_data_.size() > 0)
      return true;
    return false;
  }

  // Pretend that the NVRAM is locked if the |index| matches the prepared
  // index and there is NVRAM data available.
  virtual bool IsNvramLocked(uint32_t index) {
    if (index == index_ && nvram_data_.size())
      return true;
    return false;
  }

  // Returns the size of the NVRAM data we've read if it matches the
  // prepared |index|.
  virtual unsigned int GetNvramSize(uint32_t index) {
    if (index == index_)
      return nvram_data_.size();
    return 0;
  }

 private:
  uint32_t index_;
  const base::FilePath nvram_path_;
  brillo::Blob nvram_data_;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_LOCKBOX_CACHE_TPM_H_
