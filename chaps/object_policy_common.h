// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_POLICY_COMMON_H
#define CHAPS_OBJECT_POLICY_COMMON_H

#include "chaps/object_policy.h"

#include <map>

#include "chaps/object.h"

namespace chaps {

struct AttributePolicy {
  CK_ATTRIBUTE_TYPE type_;
  bool is_sensitive_;
  bool is_readonly_[kNumObjectStages];
  bool is_required_;
};

// Enforces policies that are common to all object types.
class ObjectPolicyCommon : public ObjectPolicy {
 public:
  ObjectPolicyCommon();
  virtual ~ObjectPolicyCommon();
  virtual void Init(Object* object);
  virtual bool IsReadAllowed(CK_ATTRIBUTE_TYPE type);
  virtual bool IsModifyAllowed(CK_ATTRIBUTE_TYPE type,
                               const std::string& value);
  virtual bool IsObjectComplete();
  virtual void SetDefaultAttributes();

 protected:
  Object* object_;  // The object this policy is associated with.
  std::map<CK_ATTRIBUTE_TYPE, AttributePolicy> policies_;
  // Helps sub-classes add more policies.
  void AddPolicies(const AttributePolicy* policies, int size);
  // Determines whether the object is private based on object class.
  bool IsPrivateClass();
};

}  // namespace

#endif  // CHAPS_OBJECT_POLICY_COMMON_H
