// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/secure_blob.h"

namespace cryptohome {

SecureBlob::SecureBlob()
    : chromeos::Blob() {
}

SecureBlob::SecureBlob(chromeos::Blob::const_iterator begin,
                       chromeos::Blob::const_iterator end)
    : chromeos::Blob(begin, end) {
}

SecureBlob::SecureBlob(int size)
    : chromeos::Blob(size) {
}

SecureBlob::~SecureBlob() {
  chromeos::SecureMemset(&this->front(), this->capacity(), 0);
}

void SecureBlob::resize(size_type sz) {
  if(sz < size()) {
    chromeos::SecureMemset(&this->at(sz), size() - sz, 0);
  }
  chromeos::Blob::resize(sz);
}

void SecureBlob::resize(size_type sz, const SecureBlobElement& x) {
  if(sz < size()) {
    chromeos::SecureMemset(&this->at(sz), size() - sz, 0);
  }
  chromeos::Blob::resize(sz, x);
}

}  // cryptohome
