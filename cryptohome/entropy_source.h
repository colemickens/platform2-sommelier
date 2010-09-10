// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// EntropySource is the interface for objects that provide random bytes.

#ifndef CRYPTOHOME_ENTROPY_SOURCE_H_
#define CRYPTOHOME_ENTROPY_SOURCE_H_

namespace cryptohome {

class EntropySource {
 public:
  EntropySource() {}
  virtual ~EntropySource() {}

  virtual void GetSecureRandom(unsigned char *rand,
                               unsigned int length) const = 0;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_ENTROPY_SOURCE_H_
