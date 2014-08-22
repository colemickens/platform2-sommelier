// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_SECURE_BLOB_H_
#define LIBCHROMEOS_CHROMEOS_SECURE_BLOB_H_

#include <string>
#include <vector>

namespace chromeos {

using Blob = std::vector<uint8_t>;

// SecureBlob erases the contents on destruction.  It does not guarantee erasure
// on resize, assign, etc.
class SecureBlob : public chromeos::Blob {
 public:
  SecureBlob();
  SecureBlob(const_iterator begin, const_iterator end);
  explicit SecureBlob(size_t size);
  explicit SecureBlob(const std::string& str);
  SecureBlob(const uint8_t* from, size_t from_length);
  SecureBlob(const char* from, size_t from_length);
  virtual ~SecureBlob();

  void resize(size_type sz);
  void resize(size_type sz, const value_type& x);

  void clear_contents();

  void* data();
  const void* const_data() const;
};

// Secure memset(). This function is guaranteed to fill int the whole buffer
// and is not subject to compiler optimization as allowed by Sub-clause 5.1.2.3
// of C Standard [ISO/IEC 9899:2011] which states:
// In the abstract machine, all expressions are evaluated as specified by the
// semantics. An actual implementation need not evaluate part of an expression
// if it can deduce that its value is not used and that no needed side effects
// are produced (including any caused by calling a function or accessing
// a volatile object).
// While memset() can be optimized out in certain situations (since most
// compilers implement this function as intrinsic and know of its side effects),
// this function will not be optimized out.
void* SecureMemset(void* v, int c, size_t n);

// Compare [n] bytes starting at [s1] with [s2] and return 0 if they match,
// 1 if they don't. Time taken to perform the comparison is only dependent on
// [n] and not on the relationship of the match between [s1] and [s2].
int SecureMemcmp(const void* s1, const void* s2, size_t n);

}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_SECURE_BLOB_H_
