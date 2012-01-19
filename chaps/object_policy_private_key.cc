// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/object_policy_private_key.h"

#include <base/basictypes.h>
#include <base/logging.h>

#include "chaps/chaps_utility.h"

namespace chaps {

static const AttributePolicy kPrivateKeyPolicies[] = {
  {CKA_ALWAYS_SENSITIVE, false, {true, true, true}, false},
  {CKA_NEVER_EXTRACTABLE, false, {true, true, true}, false},
  {CKA_UNWRAP_TEMPLATE, false, {false, false, true}, false},
  {CKA_ALWAYS_AUTHENTICATE, false, {false, false, true}, false},
  // RSA-specific attributes.
  {CKA_MODULUS, false, {false, false, true}, true},
  {CKA_PUBLIC_EXPONENT, false, {false, false, true}, true},
  {CKA_PRIVATE_EXPONENT, true, {false, false, true}, true},
  {CKA_PRIME_1, true, {false, false, true}, true},
  {CKA_PRIME_2, true, {false, false, true}, true},
  {CKA_EXPONENT_1, true, {false, false, true}, true},
  {CKA_EXPONENT_2, true, {false, false, true}, true},
  {CKA_COEFFICIENT, true, {false, false, true}, true},
};

ObjectPolicyPrivateKey::ObjectPolicyPrivateKey() {
  AddPolicies(kPrivateKeyPolicies, arraysize(kPrivateKeyPolicies));
}

ObjectPolicyPrivateKey::~ObjectPolicyPrivateKey() {}

void ObjectPolicyPrivateKey::SetDefaultAttributes() {
  ObjectPolicyKey::SetDefaultAttributes();
  if (!object_->IsAttributePresent(CKA_SUBJECT))
    object_->SetAttributeString(CKA_SUBJECT, "");
  if (!object_->IsAttributePresent(CKA_SENSITIVE))
    object_->SetAttributeBool(CKA_SENSITIVE, true);
  if (!object_->IsAttributePresent(CKA_DECRYPT))
    object_->SetAttributeBool(CKA_DECRYPT, false);
  if (!object_->IsAttributePresent(CKA_SIGN))
    object_->SetAttributeBool(CKA_SIGN, false);
  if (!object_->IsAttributePresent(CKA_SIGN_RECOVER))
    object_->SetAttributeBool(CKA_SIGN_RECOVER, false);
  if (!object_->IsAttributePresent(CKA_UNWRAP))
    object_->SetAttributeBool(CKA_UNWRAP, false);
  if (!object_->IsAttributePresent(CKA_EXTRACTABLE))
    object_->SetAttributeBool(CKA_EXTRACTABLE, false);
  if (!object_->IsAttributePresent(CKA_WRAP_WITH_TRUSTED))
    object_->SetAttributeBool(CKA_WRAP_WITH_TRUSTED, false);
  if (!object_->IsAttributePresent(CKA_ALWAYS_AUTHENTICATE))
    object_->SetAttributeBool(CKA_ALWAYS_AUTHENTICATE, false);
  if (object_->GetStage() == kCreate) {
    if (object_->GetAttributeBool(CKA_SENSITIVE, false))
      object_->SetAttributeBool(CKA_ALWAYS_SENSITIVE, true);
    if (!object_->GetAttributeBool(CKA_EXTRACTABLE, true))
      object_->SetAttributeBool(CKA_NEVER_EXTRACTABLE, true);
  }
}

}  // namespace chaps
