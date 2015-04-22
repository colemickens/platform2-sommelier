// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_TEST_UTIL_H_
#define GERM_TEST_UTIL_H_

namespace germ {

class ScopedAlarm {
 public:
  explicit ScopedAlarm(unsigned int seconds);
  ~ScopedAlarm();
};

}  // namespace germ

#endif  // GERM_TEST_UTIL_H_
