// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header file defines the wrapper around unsealing functionality of
// SealedStorage for clients who can't use libchrome/libbrillo-specifc headers.

#ifndef SEALED_STORAGE_WRAPPER_H_
#define SEALED_STORAGE_WRAPPER_H_

#include <cstdint>

extern "C" {

namespace sealed_storage {

namespace wrapper {

// Unseals the sealed data blob according to the policy defined by:
//  - |verified_boot_mode|: unsealable only if verified boot mode if true,
//                          unsealable in any mode if false;
//  - |additional_pcr|: optional PCR that must be not extended for unseal to
//                      succeed, or -1 to use no such PCR.
// |sealed_buf| and |sealed_size| define the sealed data blob.
// |plain_buf| is the buffer for returning the plaintext data, and |*plain_size|
// defines the maximum size of data that buffer can hold when calling the
// function.
// Creates MessageLoop required for tpm_managerd interactions internally.
// On success, returns true, fills |plain_buf| with the unsealed data and sets
// |*plain_size| to the actual size of the unsealed data.
// Otherwise (including when the provided buffer is too small for unsealed
// data), returns false and doesn't change |*plain_size|.
__attribute__((__visibility__("default"))) bool Unseal(
    bool verified_boot_mode,
    int additional_pcr,
    const uint8_t* sealed_buf,
    size_t sealed_size,
    uint8_t* plain_buf,
    size_t* plain_size);

}  // namespace wrapper

}  // namespace sealed_storage

}  // extern "C"

#endif  // SEALED_STORAGE_WRAPPER_H_
