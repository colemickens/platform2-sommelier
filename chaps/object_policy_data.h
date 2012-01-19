// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_POLICY_DATA_H
#define CHAPS_OBJECT_POLICY_DATA_H

#include "chaps/object_policy_common.h"

namespace chaps {

// Enforces policies for data objects (CKO_DATA).
class ObjectPolicyData : public ObjectPolicyCommon {
 public:
  ObjectPolicyData();
  virtual ~ObjectPolicyData();
  virtual void SetDefaultAttributes();
};

}  // namespace

#endif  // CHAPS_OBJECT_POLICY_DATA_H
