// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_POLICY_SECRET_KEY_H
#define CHAPS_OBJECT_POLICY_SECRET_KEY_H

#include "chaps/object_policy_key.h"

namespace chaps {

// Enforces common policies for private key objects (CKO_SECRET_KEY).
class ObjectPolicySecretKey : public ObjectPolicyKey {
 public:
  ObjectPolicySecretKey();
  virtual ~ObjectPolicySecretKey();
  virtual void SetDefaultAttributes();
};

}  // namespace

#endif  // CHAPS_OBJECT_POLICY_SECRET_KEY_H
