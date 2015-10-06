// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIDES_BLOB_REF_H_
#define FIDES_BLOB_REF_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace fides {

// A simple wrapper to refer to a blob of binary data. Note that BlobRef doesn't
// grab ownership of the memory that backs the BlobRef, so the memory a BlobRef
// refers to must remain valid for the lifetime of the BlobRef. In particular,
// BlobRefs constructed from std::string and std::vector<uint8_t> require that
// the underlying container not be mutated during the lifetime of the BlobRef,
// as that may cause re-allocation of the memory buffer and thus break the
// BlobRef.
class BlobRef {
 public:
  BlobRef();
  BlobRef(const uint8_t* data, size_t size);
  explicit BlobRef(const std::vector<uint8_t>* data);
  explicit BlobRef(const std::string* data);
  explicit BlobRef(const char* data);

  const uint8_t* data() const { return data_; }
  size_t size() const { return size_; }


  // Returns true if the BlobRef has been initialized with a value at
  // construction rather than having been created by the default constructor.
  // Otherwise returns false.
  bool valid() const { return data_ != kInvalidData; }

  // Returns true if the contents of the |that| BlobRef is byte-wise equal to
  // the contents of this BlobRef. Note: this may only be called on a valid
  // BlobRef.
  bool Equals(const BlobRef& that) const;

  // Returns a string containing a copy of the data. Note: this may only be
  // called on a valid BlobRef.
  std::string ToString() const;

  // Returns a vector containing a copy of the data. Note: this may only be
  // called on a valid BlobRef.
  std::vector<uint8_t> ToVector() const;

 private:
  static const uint8_t* kInvalidData;
  const uint8_t* const data_;
  const size_t size_;
};

}  // namespace fides

#endif  // FIDES_BLOB_REF_H_
