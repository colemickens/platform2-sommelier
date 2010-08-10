// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Creates credential stores for testing.  This class is only used in prepping
// the test data for unit tests.

#ifndef CRYPTOHOME_MAKE_TESTS_H_
#define CRYPTOHOME_MAKE_TESTS_H_

#include <base/basictypes.h>
#include <string>

namespace cryptohome {

struct TestUserInfo {
  const char* username;
  const char* password;
  bool create;
  bool use_old_format;
};

extern const TestUserInfo kDefaultUsers[];
extern const unsigned int kDefaultUserCount;

class MakeTests {
 public:
  MakeTests();

  virtual ~MakeTests() { }

  void InitTestData(const std::string& image_dir);

 private:
  DISALLOW_COPY_AND_ASSIGN(MakeTests);
};

}  // namespace cryptohome

#endif // CRYPTOHOME_MAKE_TESTS_H_
