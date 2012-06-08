// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "secure_blob.h"

namespace chromeos {

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

SecureBlob::SecureBlob(const unsigned char* from, int from_length)
    : chromeos::Blob(from_length) {
  memcpy(data(), from, from_length);
}

SecureBlob::SecureBlob(const char* from, int from_length)
    : chromeos::Blob(from_length) {
  memcpy(data(), from, from_length);
}

SecureBlob::~SecureBlob() {
  chromeos::SecureMemset(&this->front(), 0, this->capacity());
}

void SecureBlob::resize(size_type sz) {
  if (sz < size()) {
    chromeos::SecureMemset(&this->at(sz), 0, size() - sz);
  }
  chromeos::Blob::resize(sz);
}

void SecureBlob::resize(size_type sz, const SecureBlobElement& x) {
  if (sz < size()) {
    chromeos::SecureMemset(&this->at(sz), 0, size() - sz);
  }
  chromeos::Blob::resize(sz, x);
}

void SecureBlob::clear_contents() {
  chromeos::SecureMemset(this->data(), 0, size());
}

void* SecureBlob::data() {
  return &(this->front());
}

const void* SecureBlob::const_data() const {
  return &(this->front());
}

}  // namespace chromeos
