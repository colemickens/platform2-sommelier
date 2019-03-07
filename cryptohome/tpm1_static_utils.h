// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains helper functions for dealing with TPM 1.2 data.
//
// The functions here do not talk to the TPM, allowing to use these helpers in
// unit tests and fuzzers.
//
// NOTE: Please do NOT include here functions that talk to the TPM. Also please
// don't include functions that operate with Trousers handles (TSS_HCONTEXT,
// TSS_HTPM, TSS_HOBJECT, etc.), since Trousers provides incomplete support for
// working with such objects without the TPM.

#ifndef CRYPTOHOME_TPM1_STATIC_UTILS_H_
#define CRYPTOHOME_TPM1_STATIC_UTILS_H_

#include <cstdint>
#include <string>

#include <trousers/tss.h>

#include <brillo/secure_blob.h>
#include <crypto/scoped_openssl_types.h>

namespace cryptohome {

// Returns human-readable representation of the Trousers return code, for
// logging purposes.
std::string FormatTrousersErrorCode(TSS_RESULT result);

// Returns the OpenSSL RSA structure parsed from the given TPM_PUBKEY blob. Only
// fields containing the public key information (i.e., |e| and |n| are set).
//
// Returns null on failure.
crypto::ScopedRSA ParseRsaFromTpmPubkeyBlob(const brillo::Blob& pubkey);

}  // namespace cryptohome

#endif  // CRYPTOHOME_TPM1_STATIC_UTILS_H_
