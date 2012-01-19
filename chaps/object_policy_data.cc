// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/object_policy_data.h"

#include <base/basictypes.h>

namespace chaps {

static const AttributePolicy kDataPolicies[] = {
  {CKA_APPLICATION, false, {false, false, true}, false},
  {CKA_OBJECT_ID, false, {false, false, true}, false},
  {CKA_VALUE, false, {false, false, true}, false},
};

ObjectPolicyData::ObjectPolicyData() {
  AddPolicies(kDataPolicies, arraysize(kDataPolicies));
}

ObjectPolicyData::~ObjectPolicyData() {}

void ObjectPolicyData::SetDefaultAttributes() {
  ObjectPolicyCommon::SetDefaultAttributes();
  if (!object_->IsAttributePresent(CKA_APPLICATION))
    object_->SetAttributeBool(CKA_APPLICATION, "");
  if (!object_->IsAttributePresent(CKA_OBJECT_ID))
    object_->SetAttributeBool(CKA_OBJECT_ID, "");
  if (!object_->IsAttributePresent(CKA_VALUE))
    object_->SetAttributeBool(CKA_VALUE, "");
}

}  // namespace chaps
