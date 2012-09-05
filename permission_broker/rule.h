// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERMISSION_BROKER_RULE_H_
#define PERMISSION_BROKER_RULE_H_

#include <string>

#include "base/basictypes.h"

namespace permission_broker {

// A Rule represents a single unit of policy used to decide to which paths
// access is granted. Each time a Rule processes a path it can return one of
// three values: |ALLOW|, |DENY|, or |IGNORE|. If a Rule returns |ALLOW|, it
// means that the policy it represents would allow access to the requested path.
// If |DENY| is returned, then the rule is explicitly denying access to the
// resource. |IGNORE| means that the Rule makes no decision one way or another.
class Rule {
 public:
  enum Result { ALLOW, DENY, IGNORE };

  static const char *ResultToString(const Result &result);

  virtual ~Rule();
  const std::string &name();

  virtual Result Process(const std::string &path) = 0;

 protected:
  Rule(const std::string &name);

 private:
  const std::string name_;
  DISALLOW_COPY_AND_ASSIGN(Rule);
};

}  // namespace permission_broker

#endif  // PERMISSION_BROKER_RULE_H_
