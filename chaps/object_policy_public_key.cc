// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/object_policy_public_key.h"

#include <base/basictypes.h>

namespace chaps {

static const AttributePolicy kPublicKeyPolicies[] = {
  {CKA_TRUSTED, false, {true, true, true}, false},
  {CKA_WRAP_TEMPLATE, false, {false, false, true}, false},
  // RSA-specific attributes.
  {CKA_MODULUS, false, {false, false, true}, true},
  {CKA_PUBLIC_EXPONENT, false, {false, false, true}, true},
};

ObjectPolicyPublicKey::ObjectPolicyPublicKey() {
  AddPolicies(kPublicKeyPolicies, arraysize(kPublicKeyPolicies));
}

ObjectPolicyPublicKey::~ObjectPolicyPublicKey() {}

void ObjectPolicyPublicKey::SetDefaultAttributes() {
  ObjectPolicyKey::SetDefaultAttributes();
  CK_ATTRIBUTE_TYPE false_values[] = {
    CKA_ENCRYPT,
    CKA_VERIFY,
    CKA_VERIFY_RECOVER,
    CKA_WRAP,
    CKA_TRUSTED
  };
  for (size_t i = 0; i < arraysize(false_values); ++i) {
    if (!object_->IsAttributePresent(false_values[i]))
      object_->SetAttributeBool(false_values[i], false);
  }
  if (!object_->IsAttributePresent(CKA_SUBJECT))
    object_->SetAttributeString(CKA_SUBJECT, "");
}

}  // namespace chaps
