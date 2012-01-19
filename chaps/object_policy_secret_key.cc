// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/object_policy_secret_key.h"

#include <base/basictypes.h>
#include <base/logging.h>

#include "chaps/chaps_utility.h"

namespace chaps {

static const AttributePolicy kSecretKeyPolicies[] = {
  {CKA_ALWAYS_SENSITIVE, false, {true, true, true}, false},
  {CKA_NEVER_EXTRACTABLE, false, {true, true, true}, false},
  {CKA_WRAP_TEMPLATE, false, {false, false, true}, false},
  {CKA_UNWRAP_TEMPLATE, false, {false, false, true}, false},
  {CKA_CHECK_VALUE, false, {false, false, true}, false},
  {CKA_TRUSTED, false, {true, true, true}, false},
  {CKA_VALUE, true, {false, false, true}, true}
};

ObjectPolicySecretKey::ObjectPolicySecretKey() {
  AddPolicies(kSecretKeyPolicies, arraysize(kSecretKeyPolicies));
}

ObjectPolicySecretKey::~ObjectPolicySecretKey() {}

void ObjectPolicySecretKey::SetDefaultAttributes() {
  ObjectPolicyKey::SetDefaultAttributes();
  if (!object_->IsAttributePresent(CKA_SENSITIVE))
    object_->SetAttributeBool(CKA_SENSITIVE, true);
  if (!object_->IsAttributePresent(CKA_ENCRYPT))
    object_->SetAttributeBool(CKA_ENCRYPT, false);
  if (!object_->IsAttributePresent(CKA_DECRYPT))
    object_->SetAttributeBool(CKA_DECRYPT, false);
  if (!object_->IsAttributePresent(CKA_SIGN))
    object_->SetAttributeBool(CKA_SIGN, false);
  if (!object_->IsAttributePresent(CKA_VERIFY))
    object_->SetAttributeBool(CKA_VERIFY, false);
  if (!object_->IsAttributePresent(CKA_WRAP))
    object_->SetAttributeBool(CKA_WRAP, false);
  if (!object_->IsAttributePresent(CKA_UNWRAP))
    object_->SetAttributeBool(CKA_UNWRAP, false);
  if (!object_->IsAttributePresent(CKA_EXTRACTABLE))
    object_->SetAttributeBool(CKA_EXTRACTABLE, false);
  if (!object_->IsAttributePresent(CKA_WRAP_WITH_TRUSTED))
    object_->SetAttributeBool(CKA_WRAP_WITH_TRUSTED, false);
  if (object_->GetStage() == kCreate) {
    if (object_->GetAttributeBool(CKA_SENSITIVE, false))
      object_->SetAttributeBool(CKA_ALWAYS_SENSITIVE, true);
    if (!object_->GetAttributeBool(CKA_EXTRACTABLE, true))
      object_->SetAttributeBool(CKA_NEVER_EXTRACTABLE, true);
  }
}

}  // namespace chaps
