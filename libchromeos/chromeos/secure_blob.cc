// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>  // memcpy

#include "chromeos/secure_blob.h"

namespace chromeos {

SecureBlob::SecureBlob()
    : chromeos::Blob() {
}

SecureBlob::SecureBlob(const_iterator begin, const_iterator end)
    : chromeos::Blob(begin, end) {
}

SecureBlob::SecureBlob(size_t size)
    : chromeos::Blob(size) {
}

SecureBlob::SecureBlob(const std::string& from)
    : chromeos::Blob(from.length()) {
  memcpy(data(), from.c_str(), from.length());
}

SecureBlob::SecureBlob(const uint8_t* from, size_t from_length)
    : chromeos::Blob(from_length) {
  memcpy(data(), from, from_length);
}

SecureBlob::SecureBlob(const char* from, size_t from_length)
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

void SecureBlob::resize(size_type sz, const value_type& x) {
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

void* SecureMemset(void* v, int c, size_t n) {
  volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(v);
  while (n--)
    *p++ = c;
  return v;
}

int SecureMemcmp(const void* s1, const void* s2, size_t n) {
  const uint8_t* us1 = reinterpret_cast<const uint8_t*>(s1);
  const uint8_t* us2 = reinterpret_cast<const uint8_t*>(s2);
  int result = 0;

  if (0 == n)
    return 1;

  /* Code snippet without data-dependent branch due to
   * Nate Lawson (nate@root.org) of Root Labs. */
  while (n--)
    result |= *us1++ ^ *us2++;

  return result != 0;
}

}  // namespace chromeos
