// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPASSWORDPROVIDER_PASSWORD_H_
#define LIBPASSWORDPROVIDER_PASSWORD_H_

#include <base/macros.h>

#include "libpasswordprovider/libpasswordprovider_export.h"

namespace password_provider {

// Wrapper around a simple char* string. This class is used to handle allocating
// the memory so that it won't be available in a crash dump and won't be paged
// out to disk. Assumption is that this will be used to hold a user-typed
// password, so the max size will be (sizeof(1 page) - 1). The -1 is to reserve
// space for the null terminator that's added to the end when reading back the
// saved password.
//
// Note that the contents size will need to be set once the buffer has been
// written to.
class LIBPASSWORDPROVIDER_EXPORT Password {
 public:
  Password() = default;
  ~Password();

  // Returns the max size of the buffer.
  size_t max_size() const { return max_size_; }

  // Returns the size of the contents.
  size_t size() const { return size_; }

  // Creates an empty buffer. The buffer will have the appropriate protections
  // against page swapping and dumping in core dumps.
  bool Init();

  // Mutable access to the raw memory. Error if the memory has not been
  // initialized.
  char* GetMutableRaw();

  // Access to the raw memory. Error if the memory has not been initialized.
  const char* GetRaw() const;

  // Sets the size of the contents. The size should be the size of the string
  // without the null terminator.
  void SetSize(size_t size);

 private:
  char* password_ = nullptr;
  size_t buffer_alloc_size_ = 0;
  size_t max_size_ = 0;
  size_t size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Password);
};

}  // namespace password_provider

#endif  // LIBPASSWORDPROVIDER_PASSWORD_H_
