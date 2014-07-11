// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/rule.h"

namespace permission_broker {

const char *Rule::ResultToString(const Result &result) {
  switch (result) {
    case ALLOW:
      return "ALLOW";
    case DENY:
      return "DENY";
    case IGNORE:
      return "IGNORE";
    default:
      return "INVALID";
  }
}

Rule::Rule(const std::string &name) : name_(name) {}

Rule::~Rule() {}

const std::string &Rule::name() {
  return name_;
}

}  // namespace permission_broker
