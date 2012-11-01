// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMMON_H_
#define COMMON_H_

#include <iostream>
#include <sstream>
#include <stdint.h>

typedef uint64_t uint64;

// Simulate Chrome-like logging:

enum LOG_LEVEL {
  ERROR,
  INFO,
  WARNING,
};

class LOG {
 public:
  LOG(int level) {
    level_ = level;
  }

  ~LOG() {
    std::cerr << ss_.str() << std::endl;
  }

  template <class T> LOG & operator <<(const T &x) {
    ss_ << x;
    return *this;
  }

 private:
  LOG() {}
  std::ostringstream ss_;
  int level_;
};

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&); \
    void operator=(const TypeName&)

#endif /*COMMON_H_*/
