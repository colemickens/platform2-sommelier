// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/object_policy_key.h"

#include <base/basictypes.h>

namespace chaps {

static const AttributePolicy kKeyPolicies[] = {
  {CKA_KEY_TYPE, false, {false, false, true}, true},
  {CKA_LOCAL, false, {true, true, true}, false},
  {CKA_KEY_GEN_MECHANISM, false, {true, true, true}, false},
  {CKA_ALLOWED_MECHANISMS, false, {false, false, true}, false},
};

ObjectPolicyKey::ObjectPolicyKey() {
  AddPolicies(kKeyPolicies, arraysize(kKeyPolicies));
}

ObjectPolicyKey::~ObjectPolicyKey() {}

void ObjectPolicyKey::SetDefaultAttributes() {
  ObjectPolicyCommon::SetDefaultAttributes();
  CK_ATTRIBUTE_TYPE empty[] = {
    CKA_ID,
    CKA_START_DATE,
    CKA_END_DATE
  };
  for (size_t i = 0; i < arraysize(empty); ++i) {
    if (!object_->IsAttributePresent(empty[i]))
      object_->SetAttributeString(empty[i], "");
  }
  if (!object_->IsAttributePresent(CKA_DERIVE))
    object_->SetAttributeBool(CKA_DERIVE, false);
  if (!object_->IsAttributePresent(CKA_LOCAL))
    object_->SetAttributeBool(CKA_LOCAL, false);
  if (!object_->IsAttributePresent(CKA_KEY_GEN_MECHANISM))
    object_->SetAttributeBool(CKA_KEY_GEN_MECHANISM,
                              CK_UNAVAILABLE_INFORMATION);
}

}  // namespace chaps
