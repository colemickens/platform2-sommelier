// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_KEYMASTER_CONTEXT_HOOK_H_
#define ARC_KEYMASTER_CONTEXT_HOOK_H_

#include <base/macros.h>
#include <hardware/keymaster_defs.h>
#include <keymaster/keymaster_context.h>

namespace arc {
namespace keymaster {
namespace context {

// Defines ARC specific hooks into the Keymaster context. It is intended to be
// used by the PureSoftKeymasterContext to intercept and customize some of the
// context behavior.
class Hook {
 public:
  Hook();

  // Called when the context is given a new key for serialization.
  keymaster_error_t SerializeKeyBlob(
      const ::keymaster::KeymasterKeyBlob& key_material,
      const ::keymaster::AuthorizationSet& hidden,
      const ::keymaster::AuthorizationSet& hw_enforced,
      const ::keymaster::AuthorizationSet& sw_enforced,
      ::keymaster::KeymasterKeyBlob* key_blob) const;

  // Called when the context is asked to deserialize a key.
  keymaster_error_t DeserializeKeyBlob(
      const ::keymaster::KeymasterKeyBlob& key_blob,
      const ::keymaster::AuthorizationSet& hidden,
      ::keymaster::KeymasterKeyBlob* key_material,
      ::keymaster::AuthorizationSet* hw_enforced,
      ::keymaster::AuthorizationSet* sw_enforced) const;

  // Called when the context is asked to delete a key.
  keymaster_error_t DeleteKey(
      const ::keymaster::KeymasterKeyBlob& key_blob) const;

  // Called when the context is asked to delete all known keys.
  keymaster_error_t DeleteAllKeys() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(Hook);
};

}  // namespace context
}  // namespace keymaster
}  // namespace arc

#endif  // ARC_KEYMASTER_CONTEXT_HOOK_H_
