// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SECURE_BLOB_H_
#define SECURE_BLOB_H_

#include <chromeos/utility.h>

namespace cryptohome {

typedef unsigned char SecureBlobElement;

// SecureBlob erases the contents on destruction.  It does not guarantee erasure
// on resize, assign, etc.
class SecureBlob : public chromeos::Blob {
 public:
  SecureBlob();
  SecureBlob(chromeos::Blob::const_iterator begin,
             chromeos::Blob::const_iterator end);
  explicit SecureBlob(int size);
  SecureBlob(const unsigned char* from, int from_length);
  SecureBlob(const char* from, int from_length);
  virtual ~SecureBlob();

  void resize(size_type sz);
  void resize(size_type sz, const SecureBlobElement& x);

  void clear_contents();

  void* data();
  const void* const_data() const;
};

}  // namespace cryptohome

#endif  // SECURE_BLOB_H_
