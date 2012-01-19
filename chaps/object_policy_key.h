// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_POLICY_KEY_H
#define CHAPS_OBJECT_POLICY_KEY_H

#include "chaps/object_policy_common.h"

namespace chaps {

// Enforces common policies for key objects.
class ObjectPolicyKey : public ObjectPolicyCommon {
 public:
  ObjectPolicyKey();
  virtual ~ObjectPolicyKey();
  virtual void SetDefaultAttributes();
};

}  // namespace

#endif  // CHAPS_OBJECT_POLICY_KEY_H
