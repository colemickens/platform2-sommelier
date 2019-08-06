// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/keymaster/context/hook.h"

namespace arc {
namespace keymaster {
namespace context {

Hook::Hook() = default;

keymaster_error_t Hook::SerializeKeyBlob(
    const ::keymaster::KeymasterKeyBlob& key_material,
    const ::keymaster::AuthorizationSet& hidden,
    const ::keymaster::AuthorizationSet& hw_enforced,
    const ::keymaster::AuthorizationSet& sw_enforced,
    ::keymaster::KeymasterKeyBlob* key_blob) const {
  return KM_ERROR_UNKNOWN_ERROR;
}

keymaster_error_t Hook::DeserializeKeyBlob(
    const ::keymaster::KeymasterKeyBlob& key_blob,
    const ::keymaster::AuthorizationSet& hidden,
    ::keymaster::KeymasterKeyBlob* key_material,
    ::keymaster::AuthorizationSet* hw_enforced,
    ::keymaster::AuthorizationSet* sw_enforced) const {
  return KM_ERROR_UNKNOWN_ERROR;
}

keymaster_error_t Hook::DeleteKey(
    const ::keymaster::KeymasterKeyBlob& key_blob) const {
  return KM_ERROR_UNKNOWN_ERROR;
}

keymaster_error_t Hook::DeleteAllKeys() const {
  return KM_ERROR_UNKNOWN_ERROR;
}

}  // namespace context
}  // namespace keymaster
}  // namespace arc
