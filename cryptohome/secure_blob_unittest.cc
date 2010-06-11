// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for SecureBlob.

#include "secure_blob.h"

#include <base/logging.h>
#include <chromeos/utility.h>
#include <gtest/gtest.h>

namespace cryptohome {
using std::string;

class SecureBlobTest : public ::testing::Test {
 public:
  SecureBlobTest() { }
  virtual ~SecureBlobTest() { }

  static bool FindBlobInBlob(const chromeos::Blob& haystack,
                             const chromeos::Blob& needle) {
    if (needle.size() > haystack.size()) {
      return false;
    }
    for (unsigned int start = 0; start <= (haystack.size() - needle.size());
         start++) {
      if (memcmp(&haystack[start], &needle[0], needle.size()) == 0) {
        return true;
      }
    }
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SecureBlobTest);
};

TEST_F(SecureBlobTest, AllocationSizeTest) {
  // Check that allocating a SecureBlob of a specified size works
  SecureBlob blob(32);

  EXPECT_EQ(32, blob.size());
}

TEST_F(SecureBlobTest, AllocationCopyTest) {
  // Check that allocating a SecureBlob with an iterator works
  unsigned char from_data[32];
  for (unsigned int i = 0; i < sizeof(from_data); i++) {
    from_data[i] = i;
  }

  SecureBlob blob(from_data, sizeof(from_data));

  EXPECT_EQ(sizeof(from_data), blob.size());

  for (unsigned int i = 0; i < sizeof(from_data); i++) {
    EXPECT_EQ(from_data[i], blob[i]);
  }
}

TEST_F(SecureBlobTest, IteratorConstructorTest) {
  // Check that allocating a SecureBlob with an iterator works
  chromeos::Blob from_blob(32);
  for (unsigned int i = 0; i < from_blob.size(); i++) {
    from_blob[i] = i;
  }

  SecureBlob blob(from_blob.begin(), from_blob.end());

  EXPECT_EQ(from_blob.size(), blob.size());
  EXPECT_EQ(true, SecureBlobTest::FindBlobInBlob(from_blob, blob));
}

TEST_F(SecureBlobTest, ResizeTest) {
  // Check that resizing a SecureBlob wipes the excess memory.  The test assumes
  // that resize() down by one will not re-allocate the memory, so the last byte
  // will still be part of the SecureBlob's allocation
  unsigned int length = 1024;
  SecureBlob blob(length);
  void* original_data = blob.data();
  for (unsigned int i = 0; i < length; i++) {
    blob[i] = i;
  }

  blob.resize(length - 1);

  EXPECT_EQ(original_data, blob.data());
  EXPECT_EQ(length - 1, blob.size());
  EXPECT_EQ(0, static_cast<unsigned char*>(blob.data())[length - 1]);
}

TEST_F(SecureBlobTest, DestructorTest) {
  // Check that resizing a SecureBlob wipes memory on destruction.  The test
  // assumes that the unallocated memory will not be reused in the meantime,
  // which means that this test needs to be done carefully.  It intentionally
  // accesses freed memory to check of it has been zeroed.
  unsigned int length = 1024;
  SecureBlob* blob = new SecureBlob(length);
  unsigned char* data = static_cast<unsigned char*>(blob->data());
  for (unsigned int i = 0; i < length; i++) {
    data[i] = i % 256;
  }

  delete(blob);

  for (unsigned int i = 0; i < length; i++) {
    EXPECT_EQ(0, data[i]);
  }
}

} // namespace cryptohome
