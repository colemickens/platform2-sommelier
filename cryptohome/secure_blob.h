// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SECURE_BLOB_H_
#define SECURE_BLOB_H_

#include "chromeos/utility.h"

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
  virtual ~SecureBlob();

  void resize(size_type sz);
  void resize(size_type sz, const SecureBlobElement& x);
};

}  // cryptohome

#endif  // SECURE_BLOB_H_
